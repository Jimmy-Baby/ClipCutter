#include "MainWindow.h"

#include "ClipLogic.h"
#include "Utility.h"
#include "ui_MainWindow.h"

#include <QAudioOutput>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTableView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVideoWidget>

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
      ExportController_(new ExportQueueController(this))
{
    Ui_->setupUi(this);
    Ui_->clipsTable->setModel(QueueModel_);
    Ui_->clipsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    Ui_->clipsTable->setSelectionMode(QAbstractItemView::SingleSelection);
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
    Ui_->qualityCombo->addItems({QStringLiteral("Copy"), QStringLiteral("Lowest"), QStringLiteral("Low"),
                                 QStringLiteral("Medium"), QStringLiteral("High"), QStringLiteral("Best")});
    connect(Ui_->qualityCombo, &QComboBox::currentTextChanged, QueueModel_,
            [this](const QString& text) { QueueModel_->UpdateAllExportProfiles(text.toLower()); });
    connect(ExportController_, &ExportQueueController::JobStateChanged, this,
            &ClipCutter::MainWindow::OnJobStateChanged);
    connect(ExportController_, &ExportQueueController::JobProgressChanged, this,
            &ClipCutter::MainWindow::OnJobProgressChanged);
    connect(ExportController_, &ExportQueueController::JobLogUpdated, this, &ClipCutter::MainWindow::OnJobLogUpdated);
    connect(ExportController_, &ExportQueueController::TotalProgressChanged, this, [this](const double progress)
            { Ui_->progressBar->setValue(qRound(std::clamp(progress, 0.0, 1.0) * 100.0)); });
    connect(ExportController_, &ExportQueueController::QueueCompleted, this, &ClipCutter::MainWindow::OnQueueCompleted);
    ClearCurrentClipUi();
}

ClipCutter::MainWindow::~MainWindow()
{
    delete Ui_;
}

void ClipCutter::MainWindow::ActionOpenFolderTriggered()
{
    const QString folder =
        QFileDialog::getExistingDirectory(this, tr("Open Directory"), QStringLiteral("/home"),
                                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (folder.isEmpty())
    {
        return;
    }
    const QDir directory(folder);
    ClipCutter::ImportResult result = ClipImporter_.ImportDirectory(directory);
    if (result.Imported.isEmpty())
    {
        PresentImportMessages(result);
        QMessageBox::information(
            this, QStringLiteral("ClipCutter"),
            QStringLiteral("No supported video files were found in:\n%1").arg(directory.absolutePath()));
        return;
    }
    QString preparedOutputDirectory;
    QString outputError;
    if (!Utility::PrepareOutputDirectory(directory, preparedOutputDirectory, outputError))
    {
        QMessageBox::critical(this, QStringLiteral("ClipCutter"), outputError);
        return;
    }
    LoadImportedClips(std::move(result), directory, preparedOutputDirectory);
}

void ClipCutter::MainWindow::ActionOpenFilesTriggered()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Open File"), QStringLiteral("/home"),
                                                            ClipCutter::ClipImporter::FileDialogFilter());
    if (paths.isEmpty())
    {
        return;
    }
    ClipCutter::ImportResult result = ClipImporter_.ImportFiles(paths);
    if (result.Imported.isEmpty())
    {
        PresentImportMessages(result);
        return;
    }
    const QDir directory = QFileInfo(result.Imported.constFirst().SourcePath).dir();
    QString preparedOutputDirectory;
    QString outputError;
    if (!Utility::PrepareOutputDirectory(directory, preparedOutputDirectory, outputError))
    {
        QMessageBox::critical(this, QStringLiteral("ClipCutter"), outputError);
        return;
    }
    LoadImportedClips(std::move(result), directory, preparedOutputDirectory);
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
    if (!CurrentClipId_.isNull() && duration > 0)
    {
        QueueModel_->UpdateMediaDuration(CurrentClipId_, std::chrono::milliseconds{duration});
        UpdateStartEndUi();
    }
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
        QueueModel_->UpdateOutputName(CurrentSegmentId_, newName + QStringLiteral(".mp4"));
    }
}

void ClipCutter::MainWindow::OnKeywordChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
    UpdateKeywordUi();
    UpdateActionStates();
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
    const QString message = QStringLiteral("Succeeded: %1\nFailed: %2\nSkipped: %3\nCancelled: %4")
                                .arg(summary.SucceededCount)
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
    const bool exportActive = ExportController_->IsActive();
    const bool editingAllowed = hasVideo && !exportActive;
    const int row = QueueModel_->RowForSegment(CurrentSegmentId_);
    bool canRetry = false;
    bool hasDiagnostics = segment != nullptr && !segment->ExportLog.isEmpty();

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
    Ui_->actionProcessClips->setEnabled(hasQueue && !exportActive);
    Ui_->actionOpenFolder->setEnabled(!exportActive);
    Ui_->actionOpenFiles->setEnabled(!exportActive);
    Ui_->clipNameEdit->setEnabled(editingAllowed);
    Ui_->buttonUseSelected->setEnabled(editingAllowed && !Ui_->keywordsTree->selectedItems().isEmpty());
    Ui_->buttonRemoveSelected->setEnabled(!exportActive && !Ui_->keywordsTree->selectedItems().isEmpty());
    Ui_->processButton->setEnabled(hasQueue && !exportActive);
    Ui_->cancelButton->setEnabled(exportActive);
    Ui_->retryButton->setEnabled(!exportActive && canRetry);
    Ui_->viewLogButton->setEnabled(hasDiagnostics);
    Ui_->skipAllButton->setEnabled(hasQueue && !exportActive);
    Ui_->qualityCombo->setEnabled(!exportActive);
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
}

void ClipCutter::MainWindow::LoadImportedClips(ClipCutter::ImportResult result, const QDir& directory,
                                               const QString& preparedOutputDirectory)
{
    ClearCurrentClipUi();
    ExportController_->SetJobs({});
    JobToSegmentId_.clear();
    QueueModel_->ClearClips();
    VideoDirectory_ = directory;
    OutputDirectory_ = preparedOutputDirectory;
    QueueModel_->AddClips(std::move(result.Imported));
    Ui_->progressBar->setValue(0);
    if (Ui_->qualityCombo->currentIndex() >= 0)
    {
        QueueModel_->UpdateAllExportProfiles(Ui_->qualityCombo->currentText().toLower());
    }
    PresentImportMessages(result);
    if (QueueModel_->rowCount() > 0)
    {
        OpenVideo(0);
    }
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
        Ui_->clipsTable->selectionModel()->setCurrentIndex(
            QueueModel_->index(row, ClipCutter::ClipQueueModel::SourceNameColumn),
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    UpdateCurrentVideoName();
    UpdateStartEndUi();
    UpdateKeywordUi();
    UpdateActionStates();
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
    QString preparedOutputDirectory;
    QString outputError;
    if (!Utility::PrepareOutputDirectory(VideoDirectory_, preparedOutputDirectory, outputError))
    {
        QMessageBox::critical(this, QStringLiteral("ClipCutter"), outputError);
        return;
    }
    OutputDirectory_ = preparedOutputDirectory;
    QueueModel_->ResetExportRuntime();
    JobToSegmentId_.clear();

    QVector<ExportJob> jobs = BuildExportJobs(segments);

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

QVector<ClipCutter::ExportJob> ClipCutter::MainWindow::BuildExportJobs(const QVector<ExportSegment>& segments) const
{
    QVector<ExportJob> jobs;
    jobs.reserve(segments.size());

    for (const ExportSegment& segment : segments)
    {
        ExportJob job;
        job.ClipId = segment.ClipId;
        job.SegmentId = segment.SegmentId;
        job.SourcePath = segment.SourcePath;
        job.FinalOutputPath = QDir(OutputDirectory_).absoluteFilePath(segment.OutputFileName);
        job.StartTime = segment.Range.Start();

        if (segment.Range.Duration().count() > 0)
        {
            job.Duration = segment.Range.Duration();
        }

        job.EncodingQuality = SelectedEncodingQuality();
        job.CopyMetadata = UserSettings_.CopyDateTime;
        job.SkipRequested = segment.Skipped;
        job.DisplayName = segment.OutputFileName;

        const QFileInfo outputFile(job.FinalOutputPath);
        const QString suffix = outputFile.suffix();
        const QString temporaryName =
            suffix.isEmpty()
                ? QStringLiteral(".clipcutter-%1-part").arg(job.JobId.toString(QUuid::WithoutBraces))
                : QStringLiteral(".clipcutter-%1-part.%2").arg(job.JobId.toString(QUuid::WithoutBraces), suffix);
        job.TemporaryOutputPath = outputFile.dir().absoluteFilePath(temporaryName);
        jobs.append(std::move(job));
    }

    return jobs;
}

ClipCutter::EEncodingQuality ClipCutter::MainWindow::SelectedEncodingQuality() const
{
    const int selectedIndex =
        std::clamp(Ui_->qualityCombo->currentIndex(), 0, static_cast<int>(EEncodingQuality::Highest));
    return static_cast<EEncodingQuality>(selectedIndex);
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
