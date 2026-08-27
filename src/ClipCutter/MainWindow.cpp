#include "MainWindow.h"

#include "ClipLogic.h"
#include "Utility.h"
#include "Core/Export/OutputProfile.h"
#include "Core/Naming/NamingTemplate.h"
#include "ui_MainWindow.h"

#include <QAudioOutput>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMenu>
#include <QMediaPlayer>
#include <QMimeData>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QSlider>
#include <QTableView>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QSplitter>

#include <chrono>
#include <limits>
#include <utility>

namespace
{
constexpr int KKeywordUseColumn = 0;
constexpr int KKeywordTextColumn = 1;
} // namespace

ClipCutter::MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), Ui_(new Ui::ClipCutterWindow), Player_(nullptr), VideoWidget_(nullptr),
      AudioOutput_(nullptr), PlayIcon_(QStringLiteral(":/icons/play-solid.svg")),
      PauseIcon_(QStringLiteral(":/icons/pause-solid.svg")), QueueModel_(new ClipQueueModel(this)),
      QueueProxy_(new ClipQueueFilterModel(this)),
      ExportController_(new ExportQueueController(this)), MediaProbe_(new MediaProbe(this)),
      StartupDiagnostics_(new StartupDiagnostics(this)), AutosaveTimer_(new QTimer(this))
{
    Ui_->setupUi(this);
    SetupWorkflowUi();
    QueueProxy_->setSourceModel(QueueModel_);
    Ui_->clipsTable->setModel(QueueProxy_);
    Ui_->clipsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    Ui_->clipsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    Ui_->clipsTable->setAlternatingRowColors(true);
    Ui_->clipsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    Ui_->clipsTable->horizontalHeader()->setSectionResizeMode(ClipQueueModel::OutputNameColumn, QHeaderView::Stretch);

    connect(Ui_->actionOpenFolder, &QAction::triggered, this, &ClipCutter::MainWindow::ActionOpenFolderTriggered);
    connect(Ui_->actionOpenFiles, &QAction::triggered, this, &ClipCutter::MainWindow::ActionOpenFilesTriggered);
    connect(Ui_->actionPlayPause, &QAction::triggered, this, &ClipCutter::MainWindow::ActionPlayPauseTriggered);
    connect(Ui_->actionNext, &QAction::triggered, this, &ClipCutter::MainWindow::ActionNextTriggered);
    connect(Ui_->actionSkip, &QAction::triggered, this, &ClipCutter::MainWindow::ActionSkipTriggered);
    connect(Ui_->actionPrev, &QAction::triggered, this, &ClipCutter::MainWindow::ActionPrevTriggered);
    connect(Ui_->actionStop, &QAction::triggered, this, &ClipCutter::MainWindow::ActionStopTriggered);
    connect(Ui_->actionSetStart, &QAction::triggered, this, &ClipCutter::MainWindow::ActionSetStartTriggered);
    connect(Ui_->actionSetEnd, &QAction::triggered, this, &ClipCutter::MainWindow::ActionSetEndTriggered);
    connect(Ui_->actionProcessClips, &QAction::triggered, this, &ClipCutter::MainWindow::ProcessClips);
    connect(Ui_->clipsTable->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &ClipCutter::MainWindow::OnClipSelectionChanged);
    connect(Ui_->clipNameEdit, &QLineEdit::textEdited, this, &ClipCutter::MainWindow::OnVideoNameChanged);
    connect(Ui_->processButton, &QPushButton::pressed, this, &ClipCutter::MainWindow::ProcessClips);
    connect(Ui_->cancelButton, &QPushButton::pressed, this, &ClipCutter::MainWindow::CancelExport);
    connect(Ui_->retryButton, &QPushButton::pressed, this, &ClipCutter::MainWindow::RetryFailedJobs);
    connect(Ui_->viewLogButton, &QPushButton::pressed, this, &ClipCutter::MainWindow::ShowSelectedExportLog);
    connect(Ui_->skipAllButton, &QPushButton::pressed, this, &ClipCutter::MainWindow::MarkAllAsSkipped);
    Ui_->checkboxCopyMetadata->setCheckState(Qt::Checked);
    connect(Ui_->checkboxCopyMetadata, &QCheckBox::checkStateChanged, this,
            [this](Qt::CheckState state) { UserSettings_.CopyDateTime = state == Qt::Checked; });
    connect(Ui_->buttonAddKeyword, &QPushButton::pressed, this, &ClipCutter::MainWindow::AddKeyword);
    connect(Ui_->buttonRemoveSelected, &QPushButton::pressed, this, &ClipCutter::MainWindow::RemoveKeyword);
    connect(Ui_->buttonUseSelected, &QPushButton::pressed, this, &ClipCutter::MainWindow::UseKeyword);
    connect(Ui_->keywordsTree, &QTreeWidget::currentItemChanged, this, &ClipCutter::MainWindow::OnKeywordChanged);

    VideoWidget_ = new QVideoWidget(Ui_->playerBox);
    Ui_->playerLayout->addWidget(VideoWidget_);
    Player_ = new QMediaPlayer(this);
    Player_->setVideoOutput(VideoWidget_);
    AudioOutput_ = new QAudioOutput(this);
    Player_->setAudioOutput(AudioOutput_);
    connect(Player_, &QMediaPlayer::durationChanged, this, &ClipCutter::MainWindow::OnPlayerDurationChanged);
    connect(Player_, &QMediaPlayer::positionChanged, this, &ClipCutter::MainWindow::OnPlayerPositionChanged);
    connect(Ui_->timelineSlider, &QSlider::sliderMoved, Player_, &QMediaPlayer::setPosition);
    AudioOutput_->setVolume(1.0f);
    connect(Ui_->volumeSlider, &QSlider::sliderMoved, this, &ClipCutter::MainWindow::OnVolumeChanged);
    for (const OutputProfile& profile : OutputProfiles::BuiltIns())
    {
        Ui_->qualityCombo->addItem(profile.DisplayName, profile.Id);
        Ui_->qualityCombo->setItemData(Ui_->qualityCombo->count() - 1, profile.Description, Qt::ToolTipRole);
    }
    Ui_->qualityCombo->setCurrentIndex(0);
    connect(Ui_->qualityCombo, &QComboBox::currentIndexChanged, QueueModel_,
            [this](const int)
            {
                if (LoadingSession_) return;
                if (QueueModel_->rowCount() > 0)
                    QueueModel_->ApplyExportProfile(SelectedSegmentIds(), SelectedProfileId());
                MarkSessionDirty();
                UpdateOutputPreview();
                UpdateActionStates();
            });
    Ui_->collisionCombo->addItem(QStringLiteral("Ask"), static_cast<int>(ECollisionPolicy::Ask));
    Ui_->collisionCombo->addItem(QStringLiteral("Auto Rename"), static_cast<int>(ECollisionPolicy::AutoRename));
    Ui_->collisionCombo->addItem(QStringLiteral("Skip"), static_cast<int>(ECollisionPolicy::Skip));
    Ui_->collisionCombo->addItem(QStringLiteral("Overwrite"), static_cast<int>(ECollisionPolicy::Overwrite));
    connect(MediaProbe_, &MediaProbe::ProbeCompleted, this, &MainWindow::OnProbeCompleted);
    connect(StartupDiagnostics_, &StartupDiagnostics::Completed, this, &MainWindow::OnDiagnosticsCompleted);
    connect(ExportController_, &ExportQueueController::JobStateChanged, this,
            &ClipCutter::MainWindow::OnJobStateChanged);
    connect(ExportController_, &ExportQueueController::JobProgressChanged, this,
            &ClipCutter::MainWindow::OnJobProgressChanged);
    connect(ExportController_, &ExportQueueController::JobLogUpdated, this, &ClipCutter::MainWindow::OnJobLogUpdated);
    connect(ExportController_, &ExportQueueController::TotalProgressChanged, this, [this](const double progress)
            { Ui_->progressBar->setValue(qRound(std::clamp(progress, 0.0, 1.0) * 100.0)); });
    connect(ExportController_, &ExportQueueController::QueueCompleted, this, &ClipCutter::MainWindow::OnQueueCompleted);
    AutosaveTimer_->setSingleShot(true);
    AutosaveTimer_->setInterval(1500);
    connect(AutosaveTimer_, &QTimer::timeout, this, &MainWindow::AutosaveSession);
    connect(QueueModel_, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles)
            {
                const bool transient = roles.contains(ClipQueueModel::ExportStateRole) ||
                                       roles.contains(ClipQueueModel::ExportProgressRole) ||
                                       roles.contains(ClipQueueModel::ExportLogRole) ||
                                       roles.contains(ClipQueueModel::MediaProbeRole);
                if (!transient) MarkSessionDirty();
                UpdateOutputPreview();
            });
    connect(QueueModel_, &QAbstractItemModel::rowsInserted, this, [this] { MarkSessionDirty(); UpdateFilterStatus(); });
    connect(QueueModel_, &QAbstractItemModel::rowsRemoved, this, [this] { MarkSessionDirty(); UpdateFilterStatus(); });
    connect(QueueModel_, &QAbstractItemModel::modelReset, this, [this] { MarkSessionDirty(); UpdateFilterStatus(); });
    LoadApplicationSettings();
    ClearCurrentClipUi();
    StartupDiagnostics_->Start();
    QTimer::singleShot(0, this,
        [this]
        {
            const QString recovery = SessionRepository::DefaultRecoveryPath();
            if (SessionRepository::ShouldOfferRecovery(recovery) &&
                QMessageBox::question(this, QStringLiteral("Recover session"),
                                      QStringLiteral("A newer autosaved session was found. Recover it?")) == QMessageBox::Yes)
                LoadSessionFile(recovery, true);
        });
}

ClipCutter::MainWindow::~MainWindow()
{
    SaveApplicationSettings();
    delete Ui_;
}

void ClipCutter::MainWindow::ActionOpenFolderTriggered()
{
    const QString folder =
        QFileDialog::getExistingDirectory(this, tr("Open Directory"), ApplicationSettings_.LastImportDirectory,
                                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (folder.isEmpty())
    {
        return;
    }
    ApplicationSettings_.LastImportDirectory = QDir(folder).absolutePath();
    ImportPaths({folder});
}

void ClipCutter::MainWindow::ActionOpenFilesTriggered()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Open File"), ApplicationSettings_.LastImportDirectory,
                                                            ClipCutter::ClipImporter::FileDialogFilter());
    if (paths.isEmpty())
    {
        return;
    }
    ApplicationSettings_.LastImportDirectory = QFileInfo(paths.constFirst()).dir().absolutePath();
    ImportPaths(paths);
}

void ClipCutter::MainWindow::ActionPlayPauseTriggered()
{
    if (CurrentClip() == nullptr)
    {
        return;
    }
    if (Player_->isPlaying())
    {
        Ui_->actionPlayPause->setIcon(PlayIcon_);
        Player_->pause();
    }
    else
    {
        Ui_->actionPlayPause->setIcon(PauseIcon_);
        Player_->play();
    }
}

void ClipCutter::MainWindow::ActionNextTriggered()
{
    const int row = QueueModel_->RowForSegment(CurrentSegmentId_);
    if (row >= 0 && row + 1 < QueueModel_->rowCount())
    {
        OpenVideo(row + 1);
    }
}

void ClipCutter::MainWindow::ActionSkipTriggered()
{
    if (!CurrentSegmentId_.isNull())
    {
        QueueModel_->UpdateSkipState(CurrentSegmentId_, true);
        ActionNextTriggered();
        UpdateActionStates();
    }
}

void ClipCutter::MainWindow::ActionPrevTriggered()
{
    const int row = QueueModel_->RowForSegment(CurrentSegmentId_);
    if (row > 0)
    {
        OpenVideo(row - 1);
    }
}

void ClipCutter::MainWindow::ActionStopTriggered()
{
    if (CurrentClip() != nullptr)
    {
        Ui_->actionPlayPause->setIcon(PlayIcon_);
        Player_->stop();
    }
}

void ClipCutter::MainWindow::ActionSetStartTriggered()
{
    const ClipCutter::Clip* clip = CurrentClip();
    const ClipCutter::Segment* segment = CurrentSegment();
    if (clip == nullptr || segment == nullptr)
    {
        return;
    }
    QString error;
    const auto range =
        ClipCutter::TimeRange::Create(std::chrono::milliseconds{Player_->position()}, segment->Range.End(),
                                      clip->MediaInformation.Duration, true, &error);
    if (!range.has_value() || !QueueModel_->UpdateTrimRange(CurrentSegmentId_, *range, &error))
    {
        QMessageBox::warning(this, QStringLiteral("ClipCutter"), error);
        return;
    }
    UpdateStartEndUi();
}

void ClipCutter::MainWindow::ActionSetEndTriggered()
{
    const ClipCutter::Clip* clip = CurrentClip();
    const ClipCutter::Segment* segment = CurrentSegment();
    if (clip == nullptr || segment == nullptr)
    {
        return;
    }
    QString error;
    const auto range =
        ClipCutter::TimeRange::Create(segment->Range.Start(), std::chrono::milliseconds{Player_->position()},
                                      clip->MediaInformation.Duration, true, &error);
    if (!range.has_value() || !QueueModel_->UpdateTrimRange(CurrentSegmentId_, *range, &error))
    {
        QMessageBox::warning(this, QStringLiteral("ClipCutter"), error);
        return;
    }
    UpdateStartEndUi();
}

void ClipCutter::MainWindow::OnVolumeChanged(int volume)
{
    AudioOutput_->setVolume(static_cast<float>(volume) / 100.0f);
}

void ClipCutter::MainWindow::OnPlayerDurationChanged(qint64 duration)
{
    Ui_->timelineSlider->setMaximum(
        static_cast<int>(qBound<qint64>(qint64{0}, duration, qint64{std::numeric_limits<int>::max()})));
}

void ClipCutter::MainWindow::OnPlayerPositionChanged(qint64 position)
{
    Ui_->timelineSlider->setValue(
        static_cast<int>(qBound<qint64>(qint64{0}, position, qint64{std::numeric_limits<int>::max()})));
    Ui_->timelineLabel->setText(Utility::GetTimeStringFromMilliseconds(position));
}

void ClipCutter::MainWindow::OnClipSelectionChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    if (current.isValid())
    {
        const QUuid segmentId = current.data(ClipCutter::ClipQueueModel::SegmentIdRole).toUuid();
        OpenVideo(QueueModel_->RowForSegment(segmentId));
    }
}

void ClipCutter::MainWindow::OnVideoNameChanged(const QString& newName)
{
    if (!CurrentSegmentId_.isNull())
    {
        QueueModel_->UpdateOutputBaseName(CurrentSegmentId_, newName);
        if (Segment* segment = QueueModel_->FindSegment(CurrentSegmentId_)) segment->NamingTemplatePattern.reset();
        UpdateOutputPreview();
    }
}

void ClipCutter::MainWindow::OnKeywordChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
    UpdateKeywordUi();
    UpdateActionStates();
    UpdateOutputPreview();
}

void ClipCutter::MainWindow::OnJobStateChanged(const QUuid& jobId, const EExportState state)
{
    const QUuid segmentId = SegmentIdForJob(jobId);

    if (!segmentId.isNull())
    {
        QueueModel_->UpdateExportState(segmentId, state);
    }

    UpdateActionStates();
}

void ClipCutter::MainWindow::OnJobProgressChanged(const QUuid& jobId, const double progress, const bool determinate)
{
    const QUuid segmentId = SegmentIdForJob(jobId);

    if (!segmentId.isNull())
    {
        QueueModel_->UpdateExportProgress(segmentId, determinate ? std::optional<double>{progress} : std::nullopt);
    }
}

void ClipCutter::MainWindow::OnJobLogUpdated(const QUuid& jobId, const QString& text)
{
    const QUuid segmentId = SegmentIdForJob(jobId);

    if (!segmentId.isNull())
    {
        QueueModel_->AppendExportLog(segmentId, text);
    }
}

void ClipCutter::MainWindow::OnQueueCompleted(const ExportSummary& summary)
{
    QueueModel_->SetExportRuntimeLocked(false);
    UpdateActionStates();

    const QString title = summary.FailedCount > 0 ? QStringLiteral("Export completed with failures")
                                                  : QStringLiteral("Export queue completed");
    const QString message = QStringLiteral("Succeeded: %1\nMetadata warnings: %2\nFailed: %3\nSkipped: %4\nCancelled: %5")
                                .arg(summary.SucceededCount)
                                .arg(summary.WarningCount)
                                .arg(summary.FailedCount)
                                .arg(summary.SkippedCount)
                                .arg(summary.CancelledCount);
    QMessageBox::information(this, title, message);
}

void ClipCutter::MainWindow::UpdateStartEndUi()
{
    const ClipCutter::Segment* segment = CurrentSegment();
    if (segment == nullptr)
    {
        Ui_->startEndLabel->setText(QStringLiteral("Start: 00:00:00.000 / End: 00:00:00.000"));
        return;
    }
    Ui_->startEndLabel->setText(QStringLiteral("Start: %1 / End: %2")
                                    .arg(Utility::GetTimeStringFromMilliseconds(segment->Range.Start().count()),
                                         Utility::GetTimeStringFromMilliseconds(segment->Range.End().count())));
}

void ClipCutter::MainWindow::UpdateKeywordUi()
{
    const ClipCutter::Segment* segment = CurrentSegment();
    for (int index = 0; index < Ui_->keywordsTree->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* item = Ui_->keywordsTree->topLevelItem(index);
        const bool used = segment != nullptr && segment->Prefix.has_value() &&
                          ClipLogic::KeywordsEqual(item->text(KKeywordTextColumn), *segment->Prefix);
        item->setText(KKeywordUseColumn, used ? QStringLiteral("Yes") : QString());
    }
}

void ClipCutter::MainWindow::UpdateCurrentVideoName()
{
    const ClipCutter::Segment* segment = CurrentSegment();
    const QSignalBlocker blocker(Ui_->clipNameEdit);
    segment == nullptr ? Ui_->clipNameEdit->clear() : Ui_->clipNameEdit->setText(segment->OutputBaseName);
}

void ClipCutter::MainWindow::UpdateActionStates()
{
    const Segment* segment = CurrentSegment();
    const bool hasVideo = segment != nullptr;
    const bool hasQueue = QueueModel_->rowCount() > 0;
    bool hasExportable = true;
    bool hasUnskipped = false;
    for (const Clip& clip : QueueModel_->Clips())
    {
        for (const Segment& candidate : clip.Segments)
        {
            if (!candidate.Skipped)
            {
                hasUnskipped = true;
                hasExportable = hasExportable && clip.MediaInformation.IsReliableForExport();
            }
        }
    }
    hasExportable = hasExportable && hasUnskipped;
    bool profileSupported = false;
    if (const auto* profileModel = qobject_cast<const QStandardItemModel*>(Ui_->qualityCombo->model()))
    {
        const QStandardItem* item = profileModel->item(Ui_->qualityCombo->currentIndex());
        profileSupported = item != nullptr && item->isEnabled();
    }
    const bool exportActive = ExportController_->IsActive();
    const bool editingAllowed = hasVideo && !exportActive;
    const int row = QueueModel_->RowForSegment(CurrentSegmentId_);
    bool canRetry = false;
    const Clip* currentClip = CurrentClip();
    bool hasDiagnostics = segment != nullptr && !segment->ExportLog.isEmpty();
    hasDiagnostics = hasDiagnostics || (currentClip != nullptr && currentClip->MediaInformation.ProbeError.has_value());

    for (const ExportJob& job : ExportController_->Jobs())
    {
        const auto state = ExportController_->StateForJob(job.JobId);
        canRetry =
            canRetry || (state.has_value() && (*state == EExportState::Failed || *state == EExportState::Cancelled));

        if (segment != nullptr && job.SegmentId == segment->Id)
        {
            const auto result = ExportController_->ResultForJob(job.JobId);
            hasDiagnostics = hasDiagnostics || (result.has_value() && !result->ErrorMessage.isEmpty());
        }
    }

    Ui_->actionPlayPause->setEnabled(hasVideo);
    Ui_->actionNext->setEnabled(hasVideo && row + 1 < QueueModel_->rowCount());
    Ui_->actionSkip->setEnabled(editingAllowed);
    Ui_->actionPrev->setEnabled(hasVideo && row > 0);
    Ui_->actionStop->setEnabled(hasVideo);
    Ui_->actionSetStart->setEnabled(editingAllowed);
    Ui_->actionSetEnd->setEnabled(editingAllowed);
    Ui_->actionProcessClips->setEnabled(hasExportable && DiagnosticsComplete_ && profileSupported && !exportActive);
    Ui_->actionOpenFolder->setEnabled(!exportActive);
    Ui_->actionOpenFiles->setEnabled(!exportActive);
    Ui_->clipNameEdit->setEnabled(editingAllowed);
    Ui_->buttonUseSelected->setEnabled(editingAllowed && !Ui_->keywordsTree->selectedItems().isEmpty());
    Ui_->buttonRemoveSelected->setEnabled(!exportActive && !Ui_->keywordsTree->selectedItems().isEmpty());
    Ui_->processButton->setEnabled(hasExportable && DiagnosticsComplete_ && profileSupported && !exportActive);
    Ui_->cancelButton->setEnabled(exportActive);
    Ui_->retryButton->setEnabled(!exportActive && canRetry);
    Ui_->viewLogButton->setEnabled(hasDiagnostics);
    Ui_->skipAllButton->setEnabled(hasQueue && !exportActive);
    Ui_->qualityCombo->setEnabled(!exportActive);
    Ui_->collisionCombo->setEnabled(!exportActive);
    Ui_->checkboxCopyMetadata->setEnabled(!exportActive);
}

void ClipCutter::MainWindow::ClearCurrentClipUi()
{
    CurrentClipId_ = {};
    CurrentSegmentId_ = {};
    Player_->stop();
    Player_->setSource(QUrl());
    Ui_->actionPlayPause->setIcon(PlayIcon_);
    Ui_->timelineSlider->setRange(0, 0);
    Ui_->timelineLabel->setText(QStringLiteral("00:00:00.000"));
    UpdateCurrentVideoName();
    UpdateStartEndUi();
    UpdateKeywordUi();
    UpdateActionStates();
    UpdateOutputPreview();
}

void ClipCutter::MainWindow::LoadImportedClips(ClipCutter::ImportResult result, const QDir& directory,
                                               const QString& preparedOutputDirectory)
{
    Q_UNUSED(preparedOutputDirectory);
    if (QueueModel_->rowCount() == 0) ClearCurrentClipUi();
    if (directory.exists()) VideoDirectory_ = directory;
    const int firstNewRow = QueueModel_->rowCount();
    QueueModel_->AddClips(std::move(result.Imported));
    Ui_->progressBar->setValue(0);
    if (Ui_->qualityCombo->currentIndex() >= 0)
    {
        QSet<QUuid> importedIds;
        for (int row = firstNewRow; row < QueueModel_->rowCount(); ++row)
            importedIds.insert(QueueModel_->SegmentIdAtRow(row));
        QueueModel_->ApplyExportProfile(importedIds, SelectedProfileId());
    }
    PresentImportMessages(result);
    for (int row = firstNewRow; row < QueueModel_->rowCount(); ++row)
    {
        const Clip* clip = QueueModel_->FindClip(QueueModel_->ClipIdAtRow(row));
        if (clip != nullptr) MediaProbe_->Probe(clip->Id, clip->SourcePath);
    }
    if (QueueModel_->rowCount() > 0)
    {
        OpenVideo(firstNewRow);
    }
    UpdateOutputPreview();
}

void ClipCutter::MainWindow::ImportPaths(const QStringList& paths)
{
    ImportOptions options;
    options.ExistingCanonicalPaths = QueueModel_->CanonicalSourcePaths();
    options.DuplicatePolicy = EDuplicatePolicy::Skip;
    options.RecursiveDirectories = RecursiveImportCheckBox_ != nullptr && RecursiveImportCheckBox_->isChecked();
    ImportResult result = ClipImporter_.ImportPaths(paths, options);
    if (result.Imported.isEmpty())
    {
        PresentImportMessages(result);
        statusBar()->showMessage(QStringLiteral("No supported new videos found; %1 item(s) ignored.").arg(result.Skipped.size()), 6000);
        return;
    }
    const QDir directory = QFileInfo(result.Imported.constFirst().SourcePath).dir();
    const int imported = result.Imported.size();
    const int ignored = result.Skipped.size() + result.Errors.size();
    LoadImportedClips(std::move(result), directory);
    statusBar()->showMessage(QStringLiteral("Imported %1 video(s); ignored %2 unsupported or duplicate item(s).")
                                 .arg(imported).arg(ignored), 6000);
}

void ClipCutter::MainWindow::PresentImportMessages(const ClipCutter::ImportResult& result)
{
    if (!result.Errors.isEmpty())
    {
        QStringList lines;
        for (const ClipCutter::ImportError& error : result.Errors)
        {
            lines.append(QStringLiteral("%1: %2").arg(error.Path, error.Message));
        }
        QMessageBox::warning(this, QStringLiteral("ClipCutter"), lines.join(QLatin1Char('\n')));
    }
}

void ClipCutter::MainWindow::OpenVideo(int row)
{
    const QUuid clipId = QueueModel_->ClipIdAtRow(row);
    const QUuid segmentId = QueueModel_->SegmentIdAtRow(row);
    ClipCutter::Clip* clip = QueueModel_->FindClip(clipId);
    if (clip == nullptr || segmentId.isNull())
    {
        return;
    }
    CurrentClipId_ = clipId;
    CurrentSegmentId_ = segmentId;
    Player_->stop();
    Ui_->actionPlayPause->setIcon(PlayIcon_);
    Player_->setSource(QUrl::fromLocalFile(clip->SourcePath));
    Player_->pause();
    {
        const QSignalBlocker blocker(Ui_->clipsTable->selectionModel());
        const QModelIndex proxyIndex = QueueProxy_->mapFromSource(
            QueueModel_->index(row, ClipCutter::ClipQueueModel::SourceNameColumn));
        Ui_->clipsTable->selectionModel()->setCurrentIndex(
            proxyIndex,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    UpdateCurrentVideoName();
    UpdateStartEndUi();
    UpdateKeywordUi();
    UpdateActionStates();
    UpdateOutputPreview();
    Ui_->actionPlayPause->setIcon(PauseIcon_);
    Player_->play();
}

void ClipCutter::MainWindow::ProcessClips()
{
    if (Ui_->qualityCombo->currentIndex() == -1)
    {
        QMessageBox::information(this, QStringLiteral("ClipCutter"),
                                 QStringLiteral("Select an output quality before processing clips."));
        return;
    }
    if (QueueModel_->rowCount() == 0 || ExportController_->IsActive())
    {
        QMessageBox::information(this, QStringLiteral("ClipCutter"), QStringLiteral("There are no clips to process."));
        return;
    }
    QStringList rangeErrors;
    const QVector<ExportSegment> segments = QueueModel_->ExportSegments(&rangeErrors);
    if (!rangeErrors.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("ClipCutter"), rangeErrors.join(QLatin1Char('\n')));
        return;
    }
    OutputDestinationSettings destination;
    destination.Mode = DestinationModeCombo_->currentData().toInt() == 1
                           ? EOutputDestinationMode::FixedDirectory : EOutputDestinationMode::SourceRelative;
    destination.SourceRelativeSubdirectory = destination.Mode == EOutputDestinationMode::SourceRelative
                                                   ? DestinationEdit_->text().trimmed() : ApplicationSettings_.SourceRelativeSubdirectory;
    destination.FixedDirectory = destination.Mode == EOutputDestinationMode::FixedDirectory
                                      ? DestinationEdit_->text().trimmed() : ApplicationSettings_.LastFixedOutputDirectory;
    QVector<DestinationRequest> requests;
    QStringList sourcePaths;
    for (const ExportSegment& segment : segments)
    {
        if (!segment.Skipped)
        {
            DestinationRequest request;
            request.SegmentId = segment.SegmentId;
            request.BaseName = segment.OutputBaseName;
            request.Extension = segment.OutputExtension;
            request.SourcePath = segment.SourcePath;
            requests.append(request);
            sourcePaths.append(segment.SourcePath);
        }
    }
    QString outputError = OutputDestination::Validate(destination, sourcePaths);
    if (!outputError.isEmpty())
    {
        QMessageBox::critical(this, QStringLiteral("Output destination"), outputError);
        return;
    }
    if (!OutputDestination::PrepareDirectories(destination, sourcePaths, &outputError))
    {
        QMessageBox::critical(this, QStringLiteral("Output destination"), outputError);
        return;
    }
    ECollisionPolicy collisionPolicy = SelectedCollisionPolicy();
    OutputPreflightResult preflight = OutputDestination::Preflight(requests, destination, collisionPolicy);
    if (collisionPolicy == ECollisionPolicy::Ask && !preflight.Collisions.isEmpty())
    {
        QMessageBox choice(QMessageBox::Question, QStringLiteral("Output conflicts"),
                           QStringLiteral("%1 output path(s) conflict. Resolve them before export.")
                               .arg(preflight.Collisions.size()), QMessageBox::Cancel, this);
        QPushButton* rename = choice.addButton(QStringLiteral("Auto Rename"), QMessageBox::AcceptRole);
        QPushButton* skip = choice.addButton(QStringLiteral("Skip Existing"), QMessageBox::DestructiveRole);
        QPushButton* overwrite = choice.addButton(QStringLiteral("Overwrite"), QMessageBox::YesRole);
        choice.setDetailedText(preflight.Collisions.join(QLatin1Char('\n')));
        choice.exec();
        if (choice.clickedButton() == rename) collisionPolicy = ECollisionPolicy::AutoRename;
        else if (choice.clickedButton() == skip) collisionPolicy = ECollisionPolicy::Skip;
        else if (choice.clickedButton() == overwrite) collisionPolicy = ECollisionPolicy::Overwrite;
        else return;
        preflight = OutputDestination::Preflight(requests, destination, collisionPolicy);
    }
    if (!preflight.Errors.isEmpty() || !preflight.Collisions.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("Output preflight"),
                             (preflight.Errors + preflight.Collisions).join(QLatin1Char('\n')));
        return;
    }
    QStringList batchPaths;
    for (const PlannedOutput& output : preflight.Outputs)
        if (!output.Skipped) batchPaths.append(output.FinalPath);
    QMessageBox batchPreview(QMessageBox::Question, QStringLiteral("Batch output preview"),
                             QStringLiteral("Export %1 output(s) to the paths shown?").arg(batchPaths.size()),
                             QMessageBox::Yes | QMessageBox::Cancel, this);
    batchPreview.setDefaultButton(QMessageBox::Yes);
    batchPreview.setDetailedText(batchPaths.join(QLatin1Char('\n')));
    if (batchPreview.exec() != QMessageBox::Yes) return;
    QSet<QString> cleanedDirectories;
    for (const DestinationRequest& request : requests)
    {
        const QString directory = OutputDestination::DirectoryForSource(request.SourcePath, destination);
        if (!cleanedDirectories.contains(directory)) OutputPathPlanner::CleanupStaleTemporaryFiles(QDir(directory));
        cleanedDirectories.insert(directory);
    }
    QueueModel_->ResetExportRuntime();
    JobToSegmentId_.clear();

    QVector<ExportJob> jobs = BuildExportJobs(segments, preflight.Outputs, collisionPolicy);

    for (const ExportJob& job : jobs)
    {
        JobToSegmentId_.insert(job.JobId, job.SegmentId);
    }

    if (!ExportController_->SetJobs(std::move(jobs)))
    {
        QMessageBox::warning(this, QStringLiteral("ClipCutter"), QStringLiteral("The export queue is already active."));

        return;
    }

    QueueModel_->SetExportRuntimeLocked(true);
    Ui_->progressBar->setValue(0);
    ExportController_->Start();
    UpdateActionStates();
}

void ClipCutter::MainWindow::CancelExport()
{
    ExportController_->CancelQueue();
    UpdateActionStates();
}

void ClipCutter::MainWindow::RetryFailedJobs()
{
    QSet<QUuid> retryJobIds;

    for (const ExportJob& job : ExportController_->Jobs())
    {
        const auto state = ExportController_->StateForJob(job.JobId);

        if (state.has_value() && (*state == EExportState::Failed || *state == EExportState::Cancelled))
        {
            retryJobIds.insert(job.JobId);
            QueueModel_->ResetExportJobRuntime(job.SegmentId);
        }
    }

    if (retryJobIds.isEmpty())
    {
        return;
    }

    QueueModel_->SetExportRuntimeLocked(true);
    ExportController_->RetryJobs(retryJobIds);
    UpdateActionStates();
}

void ClipCutter::MainWindow::ShowSelectedExportLog()
{
    const Segment* segment = CurrentSegment();

    if (segment == nullptr)
    {
        return;
    }

    QString log = segment->ExportLog;
    const Clip* clip = CurrentClip();
    if (clip != nullptr && clip->MediaInformation.ProbeError.has_value())
        log.prepend(QStringLiteral("Probe error: %1\n").arg(*clip->MediaInformation.ProbeError));

    for (const ExportJob& job : ExportController_->Jobs())
    {
        if (job.SegmentId != segment->Id)
        {
            continue;
        }

        const auto result = ExportController_->ResultForJob(job.JobId);

        if (result.has_value() && !result->ErrorMessage.isEmpty())
        {
            log.prepend(result->ErrorMessage + QLatin1Char('\n'));
        }
        if (result.has_value() && result->HasMetadataWarning)
            log.prepend(QStringLiteral("Metadata warning: %1\n").arg(result->MetadataWarning));
        if (result.has_value() && !result->VerificationDiagnostics.isEmpty())
            log.append(QStringLiteral("\nVerification diagnostics:\n") + result->VerificationDiagnostics);

        break;
    }

    QMessageBox messageBox(QMessageBox::Critical, QStringLiteral("FFmpeg export log"),
                           QStringLiteral("Export diagnostics for %1").arg(segment->OutputFileName()), QMessageBox::Ok,
                           this);
    messageBox.setDetailedText(log.isEmpty() ? QStringLiteral("No diagnostic output was captured.") : log);
    messageBox.exec();
}

void ClipCutter::MainWindow::MarkAllAsSkipped()
{
    QueueModel_->UpdateAllSkipStates(true);
}

void ClipCutter::MainWindow::AddKeyword()
{
    const QString keyword = ClipLogic::NormalizeKeyword(Ui_->keywordEdit->text());
    if (keyword.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("ClipCutter"), QStringLiteral("Enter a non-empty keyword."));
        return;
    }
    QStringList existing;
    for (int index = 0; index < Ui_->keywordsTree->topLevelItemCount(); ++index)
    {
        existing.append(Ui_->keywordsTree->topLevelItem(index)->text(KKeywordTextColumn));
    }
    if (ClipLogic::ContainsKeyword(existing, keyword))
    {
        QMessageBox::information(this, QStringLiteral("ClipCutter"),
                                 QStringLiteral("The keyword \"%1\" already exists.").arg(keyword));
        return;
    }
    auto* item = new QTreeWidgetItem(Ui_->keywordsTree);
    item->setText(KKeywordTextColumn, keyword);
    Ui_->keywordEdit->clear();
    Ui_->keywordsTree->setCurrentItem(item);
    UpdateKeywordUi();
    UpdateActionStates();
}

void ClipCutter::MainWindow::RemoveKeyword()
{
    const QList<QTreeWidgetItem*> selected = Ui_->keywordsTree->selectedItems();
    for (QTreeWidgetItem* item : selected)
    {
        QueueModel_->ClearPrefixFromAll(item->text(KKeywordTextColumn));
        delete item;
    }
    UpdateKeywordUi();
    UpdateActionStates();
}

void ClipCutter::MainWindow::UseKeyword()
{
    ClipCutter::Segment* segment = CurrentSegment();
    const QList<QTreeWidgetItem*> selected = Ui_->keywordsTree->selectedItems();
    if (segment == nullptr || selected.isEmpty())
    {
        return;
    }
    const QString keyword = selected.constFirst()->text(KKeywordTextColumn);
    if (segment->Prefix.has_value() && ClipLogic::KeywordsEqual(*segment->Prefix, keyword))
    {
        QueueModel_->ClearPrefix(segment->Id);
    }
    else
    {
        QueueModel_->ApplyPrefix(segment->Id, keyword);
    }
    UpdateKeywordUi();
    UpdateActionStates();
}

QVector<ClipCutter::ExportJob> ClipCutter::MainWindow::BuildExportJobs(
    const QVector<ExportSegment>& segments, const QVector<PlannedOutput>& outputs,
    const ECollisionPolicy collisionPolicy) const
{
    QVector<ExportJob> jobs;
    jobs.reserve(segments.size());

    for (const ExportSegment& segment : segments)
    {
        ExportJob job;
        job.ClipId = segment.ClipId;
        job.SegmentId = segment.SegmentId;
        job.SourcePath = segment.SourcePath;
        job.StartTime = segment.Range.Start();

        if (segment.Range.Duration().count() > 0)
        {
            job.Duration = segment.Range.Duration();
        }

        job.OutputProfileId = segment.ExportProfileId;
        job.SourceMediaInfo = segment.SourceMediaInfo;
        job.CopyMetadata = UserSettings_.CopyDateTime;
        job.SkipRequested = segment.Skipped;
        job.CollisionPolicy = collisionPolicy;
        job.DisplayName = segment.OutputFileName;
        for (const PlannedOutput& output : outputs)
        {
            if (output.SegmentId != segment.SegmentId) continue;
            job.FinalOutputPath = output.FinalPath;
            job.TemporaryOutputPath = output.TemporaryPath;
            job.SkipRequested = job.SkipRequested || output.Skipped;
            job.DisplayName = QFileInfo(output.FinalPath).fileName();
            break;
        }
        if (job.FinalOutputPath.isEmpty())
        {
            OutputDestinationSettings destination;
            destination.Mode = DestinationModeCombo_->currentData().toInt() == 1
                                   ? EOutputDestinationMode::FixedDirectory : EOutputDestinationMode::SourceRelative;
            destination.SourceRelativeSubdirectory = ApplicationSettings_.SourceRelativeSubdirectory;
            destination.FixedDirectory = ApplicationSettings_.LastFixedOutputDirectory;
            job.FinalOutputPath = QDir(OutputDestination::DirectoryForSource(segment.SourcePath, destination))
                                      .absoluteFilePath(segment.OutputFileName);
            job.TemporaryOutputPath = OutputPathPlanner::CreateTemporaryPath(job.FinalOutputPath, job.JobId);
        }
        jobs.append(std::move(job));
    }

    return jobs;
}

QString ClipCutter::MainWindow::SelectedProfileId() const
{
    return Ui_->qualityCombo->currentData().toString();
}

ClipCutter::ECollisionPolicy ClipCutter::MainWindow::SelectedCollisionPolicy() const
{
    return static_cast<ECollisionPolicy>(Ui_->collisionCombo->currentData().toInt());
}

void ClipCutter::MainWindow::OnProbeCompleted(const MediaProbeResult& result)
{
    if (!QueueModel_->UpdateMediaInfo(result.ClipId, result.SourcePath, result.Info)) return;
    if (result.ClipId == CurrentClipId_)
    {
        UpdateStartEndUi();
        if (result.Info.Duration.has_value())
            Ui_->timelineSlider->setMaximum(static_cast<int>(qMin<qint64>(result.Info.Duration->count(),
                                                                         std::numeric_limits<int>::max())));
    }
    UpdateActionStates();
}

void ClipCutter::MainWindow::OnDiagnosticsCompleted(const StartupDiagnosticsResult& result)
{
    DiagnosticsComplete_ = true;
    auto* model = qobject_cast<QStandardItemModel*>(Ui_->qualityCombo->model());
    int firstSupported = -1;
    for (int index = 0; index < Ui_->qualityCombo->count(); ++index)
    {
        const ProfileSupport support = result.Profiles.value(Ui_->qualityCombo->itemData(index).toString());
        if (model != nullptr && model->item(index) != nullptr)
        {
            model->item(index)->setEnabled(support.Supported);
            if (!support.Supported) Ui_->qualityCombo->setItemData(index, support.Reason, Qt::ToolTipRole);
        }
        if (support.Supported && firstSupported < 0) firstSupported = index;
    }
    if (firstSupported >= 0 &&
        !result.Profiles.value(SelectedProfileId()).Supported)
        Ui_->qualityCombo->setCurrentIndex(firstSupported);
    if (!result.FfmpegAvailable || !result.FfprobeAvailable)
        statusBar()->showMessage(QStringLiteral("Export unavailable: ffmpeg and ffprobe are required."));
    UpdateActionStates();
}

QUuid ClipCutter::MainWindow::SegmentIdForJob(const QUuid& jobId) const
{
    return JobToSegmentId_.value(jobId);
}

ClipCutter::Clip* ClipCutter::MainWindow::CurrentClip()
{
    return QueueModel_->FindClip(CurrentClipId_);
}

const ClipCutter::Clip* ClipCutter::MainWindow::CurrentClip() const
{
    return QueueModel_->FindClip(CurrentClipId_);
}

ClipCutter::Segment* ClipCutter::MainWindow::CurrentSegment()
{
    return QueueModel_->FindSegment(CurrentSegmentId_);
}

const ClipCutter::Segment* ClipCutter::MainWindow::CurrentSegment() const
{
    return QueueModel_->FindSegment(CurrentSegmentId_);
}

void ClipCutter::MainWindow::SetupWorkflowUi()
{
    setMinimumSize(760, 520);
    Ui_->clipsBox->setMinimumSize(250, 0);
    Ui_->clipsBox->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    Ui_->playerBox->setMinimumSize(300, 180);
    Ui_->qualityCombo->setMinimumWidth(120);
    Ui_->qualityCombo->setMaximumWidth(QWIDGETSIZE_MAX);

    Ui_->gridLayout->removeWidget(Ui_->playerBox);
    Ui_->gridLayout->removeWidget(Ui_->timelineBox);
    Ui_->gridLayout->removeWidget(Ui_->clipsBox);
    auto* previewPanel = new QWidget(Ui_->centralWidget);
    auto* previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->addWidget(Ui_->playerBox, 1);
    previewLayout->addWidget(Ui_->timelineBox);
    MainSplitter_ = new QSplitter(Qt::Horizontal, Ui_->centralWidget);
    MainSplitter_->setObjectName(QStringLiteral("mainSplitter"));
    MainSplitter_->addWidget(previewPanel);
    MainSplitter_->addWidget(Ui_->clipsBox);
    MainSplitter_->setStretchFactor(0, 3);
    MainSplitter_->setStretchFactor(1, 2);
    MainSplitter_->setChildrenCollapsible(false);
    Ui_->gridLayout->addWidget(MainSplitter_, 4, 2, 2, 6);

    auto* outputDestinationBox = new QGroupBox(QStringLiteral("Destination and naming"), Ui_->centralWidget);
    auto* destinationLayout = new QHBoxLayout(outputDestinationBox);
    DestinationModeCombo_ = new QComboBox(outputDestinationBox);
    DestinationModeCombo_->addItem(QStringLiteral("Beside each source"), 0);
    DestinationModeCombo_->addItem(QStringLiteral("Fixed directory"), 1);
    DestinationEdit_ = new QLineEdit(outputDestinationBox);
    DestinationBrowseButton_ = new QPushButton(QStringLiteral("Browse…"), outputDestinationBox);
    OutputPreviewLabel_ = new QLabel(outputDestinationBox);
    OutputPreviewLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    OutputPreviewLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    destinationLayout->addWidget(DestinationModeCombo_);
    destinationLayout->addWidget(DestinationEdit_, 1);
    destinationLayout->addWidget(DestinationBrowseButton_);
    destinationLayout->addWidget(OutputPreviewLabel_, 2);
    Ui_->gridLayout->addWidget(outputDestinationBox, 3, 2, 1, 6);

    QueueFilterEdit_ = new QLineEdit(Ui_->clipsGroupBox);
    QueueFilterEdit_->setPlaceholderText(QStringLiteral("Filter source, output, prefix, status, keep/skip"));
    auto* clearFilter = new QPushButton(QStringLiteral("Clear"), Ui_->clipsGroupBox);
    FilterStatusLabel_ = new QLabel(Ui_->clipsGroupBox);
    Ui_->gridLayout_3->addWidget(QueueFilterEdit_, 0, 0);
    Ui_->gridLayout_3->addWidget(clearFilter, 0, 1);
    Ui_->gridLayout_3->addWidget(FilterStatusLabel_, 1, 0, 1, 2);

    NamingTemplateEdit_ = new QLineEdit(Ui_->clipNameBox);
    NamingTemplateEdit_->setPlaceholderText(NamingTemplate::DefaultPattern());
    NamingPreviewLabel_ = new QLabel(Ui_->clipNameBox);
    NamingPreviewLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* applyTemplate = new QPushButton(QStringLiteral("Apply template"), Ui_->clipNameBox);
    Ui_->gridLayout_5->addWidget(new QLabel(QStringLiteral("Template:"), Ui_->clipNameBox), 1, 0);
    Ui_->gridLayout_5->addWidget(NamingTemplateEdit_, 1, 1);
    Ui_->gridLayout_5->addWidget(applyTemplate, 1, 2);
    Ui_->gridLayout_5->addWidget(NamingPreviewLabel_, 2, 0, 1, 3);

    RecursiveImportCheckBox_ = new QCheckBox(QStringLiteral("Import dropped/opened folders recursively"), Ui_->clipsBox);
    Ui_->gridLayout_2->addWidget(RecursiveImportCheckBox_, 0, 0);

    connect(QueueFilterEdit_, &QLineEdit::textChanged, this,
            [this](const QString& text) { QueueProxy_->SetSearchText(text); UpdateFilterStatus(); });
    connect(clearFilter, &QPushButton::clicked, this,
            [this] { QueueFilterEdit_->clear(); QueueProxy_->ClearFilters(); UpdateFilterStatus(); });
    connect(NamingTemplateEdit_, &QLineEdit::textChanged, this,
            [this] { UpdateOutputPreview(); MarkSessionDirty(); });
    connect(applyTemplate, &QPushButton::clicked, this,
            [this] { ApplyNamingTemplateTo(SelectedSegmentIds()); });
    connect(DestinationModeCombo_, &QComboBox::currentIndexChanged, this,
            [this]
            {
                const bool fixed = DestinationModeCombo_->currentData().toInt() == 1;
                DestinationBrowseButton_->setEnabled(fixed);
                DestinationEdit_->setText(fixed ? ApplicationSettings_.LastFixedOutputDirectory
                                                : ApplicationSettings_.SourceRelativeSubdirectory);
                MarkSessionDirty();
                UpdateOutputPreview();
            });
    connect(DestinationEdit_, &QLineEdit::textChanged, this,
            [this](const QString& value)
            {
                if (DestinationModeCombo_->currentData().toInt() == 1) ApplicationSettings_.LastFixedOutputDirectory = value;
                else ApplicationSettings_.SourceRelativeSubdirectory = value;
                MarkSessionDirty();
                UpdateOutputPreview();
            });
    connect(DestinationBrowseButton_, &QPushButton::clicked, this,
            [this]
            {
                const QString directory = QFileDialog::getExistingDirectory(
                    this, QStringLiteral("Choose output directory"), ApplicationSettings_.LastFixedOutputDirectory);
                if (!directory.isEmpty()) DestinationEdit_->setText(QDir::cleanPath(directory));
            });
    connect(RecursiveImportCheckBox_, &QCheckBox::toggled, this,
            [this](const bool checked) { ApplicationSettings_.RecursiveFolderImport = checked; MarkSessionDirty(); });

    QAction* newAction = new QAction(QStringLiteral("New Session"), this);
    Ui_->menuFile->insertAction(Ui_->actionOpenFolder, newAction);
    newAction->setShortcut(QKeySequence::New);
    QAction* openAction = new QAction(QStringLiteral("Open Session…"), this);
    Ui_->menuFile->insertAction(Ui_->actionOpenFolder, openAction);
    openAction->setShortcut(QKeySequence::Open);
    QAction* saveAction = new QAction(QStringLiteral("Save Session"), this);
    Ui_->menuFile->insertAction(Ui_->actionOpenFolder, saveAction);
    saveAction->setShortcut(QKeySequence::Save);
    QAction* saveAsAction = new QAction(QStringLiteral("Save Session As…"), this);
    Ui_->menuFile->insertAction(Ui_->actionOpenFolder, saveAsAction);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    Ui_->menuFile->insertSeparator(Ui_->actionOpenFolder);
    QAction* resetSettingsAction = Ui_->menuFile->addAction(QStringLiteral("Reset Settings"));
    QAction* relinkAction = new QAction(QStringLiteral("Relink Missing Sources…"), this);
    Ui_->menuFile->insertAction(Ui_->actionOpenFolder, relinkAction);
    connect(newAction, &QAction::triggered, this, &MainWindow::NewSession);
    connect(openAction, &QAction::triggered, this, &MainWindow::OpenSession);
    connect(saveAction, &QAction::triggered, this, [this] { SaveSession(); });
    connect(saveAsAction, &QAction::triggered, this, [this] { SaveSessionAs(); });
    connect(resetSettingsAction, &QAction::triggered, this, &MainWindow::ResetSettings);
    connect(relinkAction, &QAction::triggered, this, &MainWindow::RelinkMissingSources);

    QMenu* batchMenu = menuBar()->addMenu(QStringLiteral("Batch"));
    batchMenu->addAction(QStringLiteral("Keep selected"), this,
        [this] { QueueModel_->UpdateSkipStates(SelectedSegmentIds(), false); });
    batchMenu->addAction(QStringLiteral("Skip selected"), this,
        [this] { QueueModel_->UpdateSkipStates(SelectedSegmentIds(), true); });
    batchMenu->addAction(QStringLiteral("Invert selected keep/skip"), this,
        [this] { QueueModel_->InvertSkipStates(SelectedSegmentIds()); });
    batchMenu->addAction(QStringLiteral("Keep all"), this, [this] { QueueModel_->UpdateAllSkipStates(false); });
    batchMenu->addAction(QStringLiteral("Skip all"), this, [this] { QueueModel_->UpdateAllSkipStates(true); });
    batchMenu->addAction(QStringLiteral("Invert keep/skip"), this, [this] { QueueModel_->InvertSkipStates(); });
    batchMenu->addSeparator();
    batchMenu->addAction(QStringLiteral("Apply prefix to selected"), this,
        [this] { QueueModel_->ApplyPrefixTo(SelectedSegmentIds(), Ui_->keywordEdit->text()); });
    batchMenu->addAction(QStringLiteral("Apply prefix to all"), this,
        [this] { QueueModel_->ApplyPrefixTo({}, Ui_->keywordEdit->text()); });
    batchMenu->addAction(QStringLiteral("Clear prefix from selected"), this,
        [this] { QueueModel_->ClearPrefixes(SelectedSegmentIds()); });
    batchMenu->addAction(QStringLiteral("Clear prefix from all"), this,
        [this] { QueueModel_->ClearPrefixes(); });
    batchMenu->addAction(QStringLiteral("Apply naming template to selected"), this,
        [this] { ApplyNamingTemplateTo(SelectedSegmentIds()); });
    batchMenu->addAction(QStringLiteral("Apply naming template to all"), this,
        [this] { ApplyNamingTemplateTo({}); });
    batchMenu->addAction(QStringLiteral("Apply output profile to selected"), this,
        [this] { QueueModel_->ApplyExportProfile(SelectedSegmentIds(), SelectedProfileId()); });
    batchMenu->addAction(QStringLiteral("Apply output profile to all"), this,
        [this] { QueueModel_->ApplyExportProfile({}, SelectedProfileId()); });
    batchMenu->addAction(QStringLiteral("Reset selected trim range"), this,
        [this] { QueueModel_->ResetTrimRanges(SelectedSegmentIds()); });
    batchMenu->addAction(QStringLiteral("Reset all trim ranges"), this,
        [this] { QueueModel_->ResetTrimRanges(); });
    batchMenu->addAction(QStringLiteral("Remove selected entries"), this,
        [this] { QueueModel_->RemoveEntries(SelectedSegmentIds()); });
    batchMenu->addSeparator();
    batchMenu->addAction(QStringLiteral("Clear queue"), this,
        [this]
        {
            if (QueueModel_->rowCount() > 0 && QMessageBox::question(this, QStringLiteral("Clear queue"),
                    QStringLiteral("Remove all entries from the queue?")) == QMessageBox::Yes)
            {
                ClearCurrentClipUi();
                QueueModel_->ClearClips();
            }
        });
}

void ClipCutter::MainWindow::LoadApplicationSettings()
{
    ApplicationSettings_ = SettingsRepository_.Load();
    if (!ApplicationSettings_.WindowGeometry.isEmpty()) restoreGeometry(ApplicationSettings_.WindowGeometry);
    if (!ApplicationSettings_.WindowState.isEmpty()) restoreState(ApplicationSettings_.WindowState);
    if (MainSplitter_ != nullptr && !ApplicationSettings_.MainSplitterState.isEmpty())
        MainSplitter_->restoreState(ApplicationSettings_.MainSplitterState);
    Ui_->volumeSlider->setValue(ApplicationSettings_.PreviewVolume);
    OnVolumeChanged(ApplicationSettings_.PreviewVolume);
    Ui_->checkboxCopyMetadata->setChecked(ApplicationSettings_.PreserveMetadata);
    UserSettings_.CopyDateTime = ApplicationSettings_.PreserveMetadata;
    RecursiveImportCheckBox_->setChecked(ApplicationSettings_.RecursiveFolderImport);
    NamingTemplateEdit_->setText(ApplicationSettings_.SelectedNamingTemplate);
    for (const QString& prefix : ApplicationSettings_.SavedPrefixes)
    {
        auto* item = new QTreeWidgetItem(Ui_->keywordsTree);
        item->setText(KKeywordTextColumn, prefix);
    }
    const int profileIndex = Ui_->qualityCombo->findData(ApplicationSettings_.SelectedOutputProfile);
    if (profileIndex >= 0) Ui_->qualityCombo->setCurrentIndex(profileIndex);
    DestinationModeCombo_->setCurrentIndex(ApplicationSettings_.DestinationMode == EOutputDestinationMode::FixedDirectory ? 1 : 0);
    DestinationEdit_->setText(ApplicationSettings_.DestinationMode == EOutputDestinationMode::FixedDirectory
                                  ? ApplicationSettings_.LastFixedOutputDirectory
                                  : ApplicationSettings_.SourceRelativeSubdirectory);
    SessionState_.Reset();
    setWindowTitle(QStringLiteral("ClipCutter"));
}

void ClipCutter::MainWindow::SaveApplicationSettings()
{
    ApplicationSettings_.WindowGeometry = saveGeometry();
    ApplicationSettings_.WindowState = saveState();
    ApplicationSettings_.MainSplitterState = MainSplitter_ == nullptr ? QByteArray() : MainSplitter_->saveState();
    ApplicationSettings_.PreviewVolume = Ui_->volumeSlider->value();
    ApplicationSettings_.DestinationMode = DestinationModeCombo_->currentData().toInt() == 1
                                                ? EOutputDestinationMode::FixedDirectory
                                                : EOutputDestinationMode::SourceRelative;
    ApplicationSettings_.SelectedOutputProfile = SelectedProfileId();
    ApplicationSettings_.PreserveMetadata = Ui_->checkboxCopyMetadata->isChecked();
    ApplicationSettings_.RecursiveFolderImport = RecursiveImportCheckBox_->isChecked();
    ApplicationSettings_.SelectedNamingTemplate = NamingTemplateEdit_->text();
    ApplicationSettings_.SavedPrefixes.clear();
    for (int index = 0; index < Ui_->keywordsTree->topLevelItemCount(); ++index)
        ApplicationSettings_.SavedPrefixes.append(Ui_->keywordsTree->topLevelItem(index)->text(KKeywordTextColumn));
    SettingsRepository_.Save(ApplicationSettings_);
}

void ClipCutter::MainWindow::UpdateOutputPreview()
{
    const Clip* clip = CurrentClip();
    const Segment* segment = CurrentSegment();
    if (clip == nullptr || segment == nullptr || DestinationEdit_ == nullptr)
    {
        if (OutputPreviewLabel_ != nullptr) OutputPreviewLabel_->setText(QStringLiteral("Output: —"));
        if (NamingPreviewLabel_ != nullptr) NamingPreviewLabel_->clear();
        return;
    }
    NamingTemplateContext context;
    context.Original = QFileInfo(clip->OriginalFileName).completeBaseName();
    context.Prefix = segment->Prefix.value_or(QString());
    context.Index = QueueModel_->RowForSegment(segment->Id) + 1;
    context.Profile = SelectedProfileId();
    const NamingTemplateResult naming = NamingTemplate::Render(NamingTemplateEdit_->text(), context);
    NamingPreviewLabel_->setText(naming.IsValid() ? QStringLiteral("Preview: %1%2").arg(naming.Value, segment->OutputExtension)
                                                  : naming.Error);
    NamingPreviewLabel_->setStyleSheet(naming.IsValid() ? QString() : QStringLiteral("color: palette(bright-text);"));
    OutputDestinationSettings destination;
    destination.Mode = DestinationModeCombo_->currentData().toInt() == 1
                           ? EOutputDestinationMode::FixedDirectory : EOutputDestinationMode::SourceRelative;
    destination.SourceRelativeSubdirectory = destination.Mode == EOutputDestinationMode::SourceRelative
                                                   ? DestinationEdit_->text() : ApplicationSettings_.SourceRelativeSubdirectory;
    destination.FixedDirectory = destination.Mode == EOutputDestinationMode::FixedDirectory
                                      ? DestinationEdit_->text() : ApplicationSettings_.LastFixedOutputDirectory;
    const QString directory = OutputDestination::DirectoryForSource(clip->SourcePath, destination);
    const QString base = naming.IsValid() ? naming.Value : segment->OutputBaseName;
    QString preview = directory.isEmpty()
                          ? QStringLiteral("Output: choose a fixed directory")
                          : QStringLiteral("Output: %1").arg(QDir(directory).absoluteFilePath(base + segment->OutputExtension));
    QVector<DestinationRequest> requests;
    QStringList sources;
    bool destinationsExist = true;
    for (const ExportSegment& item : QueueModel_->ExportSegments())
    {
        if (item.Skipped) continue;
        DestinationRequest request;
        request.SegmentId = item.SegmentId;
        request.BaseName = item.OutputBaseName;
        request.Extension = item.OutputExtension;
        request.SourcePath = item.SourcePath;
        requests.append(request);
        sources.append(item.SourcePath);
        destinationsExist = destinationsExist && QFileInfo::exists(OutputDestination::DirectoryForSource(item.SourcePath, destination));
    }
    if (destinationsExist && OutputDestination::Validate(destination, sources).isEmpty())
    {
        const OutputPreflightResult preflight = OutputDestination::Preflight(requests, destination, ECollisionPolicy::Ask);
        if (!preflight.Collisions.isEmpty()) preview += QStringLiteral("  — %1 collision(s)").arg(preflight.Collisions.size());
    }
    OutputPreviewLabel_->setText(preview);
    OutputPreviewLabel_->setToolTip(OutputPreviewLabel_->text());
}

void ClipCutter::MainWindow::UpdateFilterStatus()
{
    const int visible = QueueProxy_->rowCount();
    const int total = QueueModel_->rowCount();
    FilterStatusLabel_->setText(QueueProxy_->HasActiveFilter()
                                    ? QStringLiteral("Showing %1 of %2 — filter active").arg(visible).arg(total)
                                    : QStringLiteral("%1 item(s)").arg(total));
}

QSet<QUuid> ClipCutter::MainWindow::SelectedSegmentIds() const
{
    QSet<QUuid> result;
    if (Ui_->clipsTable->selectionModel() != nullptr)
        for (const QModelIndex& index : Ui_->clipsTable->selectionModel()->selectedRows())
            result.insert(index.data(ClipQueueModel::SegmentIdRole).toUuid());
    if (result.isEmpty() && !CurrentSegmentId_.isNull()) result.insert(CurrentSegmentId_);
    return result;
}

void ClipCutter::MainWindow::ApplyNamingTemplateTo(const QSet<QUuid>& ids)
{
    QString error;
    if (!QueueModel_->ApplyNamingTemplate(ids, NamingTemplateEdit_->text(), QDate::currentDate(), &error))
    {
        QMessageBox::warning(this, QStringLiteral("Naming template"), error);
        return;
    }
    UpdateCurrentVideoName();
    UpdateOutputPreview();
}

void ClipCutter::MainWindow::MarkSessionDirty()
{
    if (LoadingSession_) return;
    SessionState_.MarkModified();
    setWindowTitle(QStringLiteral("ClipCutter — %1%2")
                       .arg(SessionState_.FilePath().isEmpty() ? QStringLiteral("Untitled")
                                                              : QFileInfo(SessionState_.FilePath()).fileName(),
                            QStringLiteral(" *")));
    AutosaveTimer_->start();
}

ClipCutter::SessionData ClipCutter::MainWindow::CurrentSessionData() const
{
    SessionData session;
    session.Clips = QueueModel_->Clips();
    session.Destination.Mode = DestinationModeCombo_->currentData().toInt() == 1
                                ? EOutputDestinationMode::FixedDirectory : EOutputDestinationMode::SourceRelative;
    session.Destination.SourceRelativeSubdirectory = ApplicationSettings_.SourceRelativeSubdirectory;
    session.Destination.FixedDirectory = ApplicationSettings_.LastFixedOutputDirectory;
    session.SelectedOutputProfile = SelectedProfileId();
    session.PreserveMetadata = Ui_->checkboxCopyMetadata->isChecked();
    session.RecursiveFolderImport = RecursiveImportCheckBox_->isChecked();
    session.SelectedNamingTemplate = NamingTemplateEdit_->text();
    for (int index = 0; index < Ui_->keywordsTree->topLevelItemCount(); ++index)
        session.SavedPrefixes.append(Ui_->keywordsTree->topLevelItem(index)->text(KKeywordTextColumn));
    session.ExplicitSessionPath = SessionState_.FilePath();
    if (!session.ExplicitSessionPath.isEmpty()) session.ExplicitSaveTimeUtc = QFileInfo(session.ExplicitSessionPath).lastModified().toUTC();
    return session;
}

void ClipCutter::MainWindow::AutosaveSession()
{
    if (!SessionState_.IsDirty()) return;
    QString error;
    if (!SessionRepository::Save(SessionRepository::DefaultRecoveryPath(), CurrentSessionData(), &error, true))
        statusBar()->showMessage(error, 8000);
}

bool ClipCutter::MainWindow::MaybeSaveChanges()
{
    if (!SessionState_.IsDirty()) return true;
    QMessageBox box(QMessageBox::Warning, QStringLiteral("Unsaved changes"),
                    QStringLiteral("Save changes to the current session?"), QMessageBox::NoButton, this);
    box.addButton(QMessageBox::Save);
    box.addButton(QMessageBox::Discard);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.standardButton(box.clickedButton()) == QMessageBox::Save) return SaveSession();
    if (box.standardButton(box.clickedButton()) == QMessageBox::Discard) return true;
    return false;
}

void ClipCutter::MainWindow::NewSession()
{
    if (!MaybeSaveChanges()) return;
    LoadingSession_ = true;
    ClearCurrentClipUi();
    QueueModel_->ClearClips();
    ExportController_->SetJobs({});
    JobToSegmentId_.clear();
    SessionState_.Reset();
    LoadingSession_ = false;
    SessionRepository::RemoveRecovery();
    setWindowTitle(QStringLiteral("ClipCutter"));
    UpdateFilterStatus();
}

void ClipCutter::MainWindow::OpenSession()
{
    if (!MaybeSaveChanges()) return;
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open ClipCutter session"),
                                                      ApplicationSettings_.LastImportDirectory,
                                                      QStringLiteral("ClipCutter Session (*.clipcutter.json *.json)"));
    if (!path.isEmpty()) LoadSessionFile(path);
}

bool ClipCutter::MainWindow::LoadSessionFile(const QString& path, const bool recovery)
{
    SessionLoadResult loaded = SessionRepository::Load(path);
    if (!loaded.IsValid())
    {
        QMessageBox::critical(this, QStringLiteral("Open session"), loaded.Error);
        return false;
    }
    LoadingSession_ = true;
    ClearCurrentClipUi();
    QueueModel_->ReplaceClips(std::move(loaded.Session.Clips));
    const int destinationIndex = loaded.Session.Destination.Mode == EOutputDestinationMode::FixedDirectory ? 1 : 0;
    ApplicationSettings_.SourceRelativeSubdirectory = loaded.Session.Destination.SourceRelativeSubdirectory;
    ApplicationSettings_.LastFixedOutputDirectory = loaded.Session.Destination.FixedDirectory;
    DestinationModeCombo_->setCurrentIndex(destinationIndex);
    DestinationEdit_->setText(destinationIndex == 1 ? loaded.Session.Destination.FixedDirectory
                                                     : loaded.Session.Destination.SourceRelativeSubdirectory);
    NamingTemplateEdit_->setText(loaded.Session.SelectedNamingTemplate);
    RecursiveImportCheckBox_->setChecked(loaded.Session.RecursiveFolderImport);
    Ui_->checkboxCopyMetadata->setChecked(loaded.Session.PreserveMetadata);
    UserSettings_.CopyDateTime = loaded.Session.PreserveMetadata;
    const int profileIndex = Ui_->qualityCombo->findData(loaded.Session.SelectedOutputProfile);
    if (profileIndex >= 0) Ui_->qualityCombo->setCurrentIndex(profileIndex);
    Ui_->keywordsTree->clear();
    for (const QString& prefix : loaded.Session.SavedPrefixes)
    {
        auto* item = new QTreeWidgetItem(Ui_->keywordsTree);
        item->setText(KKeywordTextColumn, prefix);
    }
    SessionState_.MarkSaved(recovery ? loaded.Session.ExplicitSessionPath : path);
    LoadingSession_ = false;
    for (const Clip& clip : QueueModel_->Clips())
        if (QFileInfo::exists(clip.SourcePath)) MediaProbe_->Probe(clip.Id, clip.SourcePath);
    if (QueueModel_->rowCount() > 0) OpenVideo(0);
    if (recovery) SessionState_.MarkModified();
    setWindowTitle(QStringLiteral("ClipCutter — %1%2")
                       .arg(SessionState_.FilePath().isEmpty() ? QStringLiteral("Recovered Untitled")
                                                              : QFileInfo(SessionState_.FilePath()).fileName(),
                            SessionState_.IsDirty() ? QStringLiteral(" *") : QString()));
    if (!loaded.Warnings.isEmpty())
        QMessageBox::warning(this, QStringLiteral("Session sources"),
                             loaded.Warnings.join(QLatin1Char('\n')) +
                                 QStringLiteral("\n\nMissing entries remain in the queue and can be relinked by importing their replacement files."));
    if (!recovery)
    {
        ApplicationSettings_.RecentSessionFiles.removeAll(path);
        ApplicationSettings_.RecentSessionFiles.prepend(path);
        while (ApplicationSettings_.RecentSessionFiles.size() > 10) ApplicationSettings_.RecentSessionFiles.removeLast();
    }
    UpdateFilterStatus();
    UpdateOutputPreview();
    return true;
}

bool ClipCutter::MainWindow::SaveSession()
{
    if (SessionState_.FilePath().isEmpty()) return SaveSessionAs();
    SessionData session = CurrentSessionData();
    session.ExplicitSessionPath = SessionState_.FilePath();
    session.ExplicitSaveTimeUtc = QDateTime::currentDateTimeUtc();
    QString error;
    if (!SessionRepository::Save(SessionState_.FilePath(), session, &error))
    {
        QMessageBox::critical(this, QStringLiteral("Save session"), error);
        return false;
    }
    SessionState_.MarkSaved(SessionState_.FilePath());
    SessionRepository::RemoveRecovery();
    setWindowTitle(QStringLiteral("ClipCutter — %1").arg(QFileInfo(SessionState_.FilePath()).fileName()));
    ApplicationSettings_.RecentSessionFiles.removeAll(SessionState_.FilePath());
    ApplicationSettings_.RecentSessionFiles.prepend(SessionState_.FilePath());
    return true;
}

bool ClipCutter::MainWindow::SaveSessionAs()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save ClipCutter session"),
                                                SessionState_.FilePath(),
                                                QStringLiteral("ClipCutter Session (*.clipcutter.json)"));
    if (path.isEmpty()) return false;
    if (!path.endsWith(QStringLiteral(".clipcutter.json"), Qt::CaseInsensitive)) path += QStringLiteral(".clipcutter.json");
    const QString previous = SessionState_.FilePath();
    SessionState_.MarkSaved(path);
    if (SaveSession()) return true;
    if (previous.isEmpty()) SessionState_.Reset(); else SessionState_.MarkSaved(previous);
    SessionState_.MarkModified();
    return false;
}

void ClipCutter::MainWindow::ResetSettings()
{
    if (QMessageBox::question(this, QStringLiteral("Reset settings"),
                              QStringLiteral("Reset all saved application settings to defaults?")) != QMessageBox::Yes)
        return;
    QString error;
    if (!SettingsRepository_.Reset(&error)) QMessageBox::critical(this, QStringLiteral("Reset settings"), error);
    else statusBar()->showMessage(QStringLiteral("Settings reset. Defaults apply after restart."), 6000);
}

void ClipCutter::MainWindow::RelinkMissingSources()
{
    for (const Clip& snapshot : QueueModel_->Clips())
    {
        if (QFileInfo::exists(snapshot.SourcePath)) continue;
        const QString replacement = QFileDialog::getOpenFileName(
            this, QStringLiteral("Relink %1").arg(snapshot.OriginalFileName),
            QFileInfo(snapshot.SourcePath).absolutePath(), ClipImporter::FileDialogFilter());
        if (replacement.isEmpty()) continue;
        QString error;
        if (!QueueModel_->RelinkClipSource(snapshot.Id, replacement, &error))
        {
            QMessageBox::warning(this, QStringLiteral("Relink source"), error);
            continue;
        }
        if (const Clip* clip = QueueModel_->FindClip(snapshot.Id)) MediaProbe_->Probe(clip->Id, clip->SourcePath);
    }
}

void ClipCutter::MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()->hasUrls()) return;
    bool valid = false;
    for (const QUrl& url : event->mimeData()->urls())
    {
        if (!url.isLocalFile()) continue;
        const QFileInfo info(url.toLocalFile());
        valid = valid || info.isDir() || (info.isFile() && ClipImporter::IsSupported(info.fileName()));
    }
    if (valid)
    {
        event->acceptProposedAction();
        statusBar()->showMessage(QStringLiteral("Drop videos or folders to add them to the queue"));
    }
}

void ClipCutter::MainWindow::dragLeaveEvent(QDragLeaveEvent* event)
{
    statusBar()->clearMessage();
    QMainWindow::dragLeaveEvent(event);
}

void ClipCutter::MainWindow::dropEvent(QDropEvent* event)
{
    QStringList paths;
    int remoteUrls = 0;
    for (const QUrl& url : event->mimeData()->urls())
    {
        if (!url.isLocalFile()) { ++remoteUrls; continue; }
        paths.append(QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath()));
    }
    if (!paths.isEmpty())
    {
        event->acceptProposedAction();
        ImportPaths(paths);
    }
    if (remoteUrls > 0)
        statusBar()->showMessage(QStringLiteral("Ignored %1 non-local URL(s); only local files and folders are accepted.").arg(remoteUrls), 7000);
}

void ClipCutter::MainWindow::closeEvent(QCloseEvent* event)
{
    if (ExportController_->IsActive() || !MaybeSaveChanges())
    {
        event->ignore();
        return;
    }
    SaveApplicationSettings();
    SessionRepository::RemoveRecovery();
    event->accept();
}
