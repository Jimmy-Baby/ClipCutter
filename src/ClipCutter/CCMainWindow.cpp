#include "CCMainWindow.h"

#include "ClipLogic.h"
#include "FFmpeg.h"
#include "Utility.h"
#include "ui_CCMainWindow.h"

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
#include <Windows.h>

#include <chrono>
#include <limits>
#include <utility>

namespace
{
constexpr int KeywordUseColumn = 0;
constexpr int KeywordTextColumn = 1;
}

CClipCutterWindow::CClipCutterWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::ClipCutterWindow),
      player(nullptr),
      videoWidget(nullptr),
      audioOutput(nullptr),
      playIcon(QStringLiteral(":/icons/icons/play-solid.svg")),
      pauseIcon(QStringLiteral(":/icons/icons/pause-solid.svg")),
      queueModel(new clipcutter::ClipQueueModel(this))
{
    ui->setupUi(this);
    ui->clipsTable->setModel(queueModel);
    ui->clipsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->clipsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->clipsTable->setAlternatingRowColors(true);
    ui->clipsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->clipsTable->horizontalHeader()->setSectionResizeMode(
        clipcutter::ClipQueueModel::OutputNameColumn, QHeaderView::Stretch);

    connect(ui->actionOpenFolder, &QAction::triggered, this, &CClipCutterWindow::ActionOpenFolderTriggered);
    connect(ui->actionOpenFiles, &QAction::triggered, this, &CClipCutterWindow::ActionOpenFilesTriggered);
    connect(ui->actionPlayPause, &QAction::triggered, this, &CClipCutterWindow::ActionPlayPauseTriggered);
    connect(ui->actionNext, &QAction::triggered, this, &CClipCutterWindow::ActionNextTriggered);
    connect(ui->actionSkip, &QAction::triggered, this, &CClipCutterWindow::ActionSkipTriggered);
    connect(ui->actionPrev, &QAction::triggered, this, &CClipCutterWindow::ActionPrevTriggered);
    connect(ui->actionStop, &QAction::triggered, this, &CClipCutterWindow::ActionStopTriggered);
    connect(ui->actionSetStart, &QAction::triggered, this, &CClipCutterWindow::ActionSetStartTriggered);
    connect(ui->actionSetEnd, &QAction::triggered, this, &CClipCutterWindow::ActionSetEndTriggered);
    connect(ui->actionProcessClips, &QAction::triggered, this, &CClipCutterWindow::ProcessClips);
    connect(ui->clipsTable->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &CClipCutterWindow::OnClipSelectionChanged);
    connect(ui->clipNameEdit, &QLineEdit::textEdited, this, &CClipCutterWindow::OnVideoNameChanged);
    connect(ui->processButton, &QPushButton::pressed, this, &CClipCutterWindow::ProcessClips);
    connect(ui->skipAllButton, &QPushButton::pressed, this, &CClipCutterWindow::MarkAllAsSkipped);
    ui->checkboxCopyMetadata->setCheckState(Qt::Checked);
    connect(ui->checkboxCopyMetadata, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state)
    {
        userSettings.copyDateTime = state == Qt::Checked;
    });
    connect(ui->buttonAddKeyword, &QPushButton::pressed, this, &CClipCutterWindow::AddKeyword);
    connect(ui->buttonRemoveSelected, &QPushButton::pressed, this, &CClipCutterWindow::RemoveKeyword);
    connect(ui->buttonUseSelected, &QPushButton::pressed, this, &CClipCutterWindow::UseKeyword);
    connect(ui->keywordsTree, &QTreeWidget::currentItemChanged, this, &CClipCutterWindow::OnKeywordChanged);

    videoWidget = new QVideoWidget(ui->playerBox);
    ui->playerLayout->addWidget(videoWidget);
    player = new QMediaPlayer(this);
    player->setVideoOutput(videoWidget);
    audioOutput = new QAudioOutput(this);
    player->setAudioOutput(audioOutput);
    connect(player, &QMediaPlayer::durationChanged, this, &CClipCutterWindow::OnPlayerDurationChanged);
    connect(player, &QMediaPlayer::positionChanged, this, &CClipCutterWindow::OnPlayerPositionChanged);
    connect(ui->timelineSlider, &QSlider::sliderMoved, player, &QMediaPlayer::setPosition);
    audioOutput->setVolume(1.0f);
    connect(ui->volumeSlider, &QSlider::sliderMoved, this, &CClipCutterWindow::OnVolumeChanged);
    ui->qualityCombo->addItems({ QStringLiteral("Copy"), QStringLiteral("Lowest"), QStringLiteral("Low"),
                                 QStringLiteral("Medium"), QStringLiteral("High"), QStringLiteral("Best") });
    connect(ui->qualityCombo, &QComboBox::currentTextChanged, queueModel,
            [this](const QString& text) { queueModel->updateAllExportProfiles(text.toLower()); });
    ClearCurrentClipUI();
}

CClipCutterWindow::~CClipCutterWindow()
{
    delete ui;
}

void CClipCutterWindow::ActionOpenFolderTriggered()
{
    const QString folder = QFileDialog::getExistingDirectory(
        this, tr("Open Directory"), QStringLiteral("/home"),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (folder.isEmpty())
    {
        return;
    }
    const QDir directory(folder);
    clipcutter::ImportResult result = clipImporter.importDirectory(directory);
    if (result.imported.isEmpty())
    {
        PresentImportMessages(result);
        QMessageBox::information(this, QStringLiteral("ClipCutter"),
                                 QStringLiteral("No supported video files were found in:\n%1")
                                     .arg(directory.absolutePath()));
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

void CClipCutterWindow::ActionOpenFilesTriggered()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Open File"), QStringLiteral("/home"), clipcutter::ClipImporter::fileDialogFilter());
    if (paths.isEmpty())
    {
        return;
    }
    clipcutter::ImportResult result = clipImporter.importFiles(paths);
    if (result.imported.isEmpty())
    {
        PresentImportMessages(result);
        return;
    }
    const QDir directory = QFileInfo(result.imported.constFirst().sourcePath).dir();
    QString preparedOutputDirectory;
    QString outputError;
    if (!Utility::PrepareOutputDirectory(directory, preparedOutputDirectory, outputError))
    {
        QMessageBox::critical(this, QStringLiteral("ClipCutter"), outputError);
        return;
    }
    LoadImportedClips(std::move(result), directory, preparedOutputDirectory);
}

void CClipCutterWindow::ActionPlayPauseTriggered()
{
    if (currentClip() == nullptr)
    {
        return;
    }
    if (player->isPlaying())
    {
        ui->actionPlayPause->setIcon(playIcon);
        player->pause();
    }
    else
    {
        ui->actionPlayPause->setIcon(pauseIcon);
        player->play();
    }
}

void CClipCutterWindow::ActionNextTriggered()
{
    const int row = queueModel->rowForSegment(currentSegmentId);
    if (row >= 0 && row + 1 < queueModel->rowCount())
    {
        OpenVideo(row + 1);
    }
}

void CClipCutterWindow::ActionSkipTriggered()
{
    if (!currentSegmentId.isNull())
    {
        queueModel->updateSkipState(currentSegmentId, true);
        ActionNextTriggered();
        UpdateActionStates();
    }
}

void CClipCutterWindow::ActionPrevTriggered()
{
    const int row = queueModel->rowForSegment(currentSegmentId);
    if (row > 0)
    {
        OpenVideo(row - 1);
    }
}

void CClipCutterWindow::ActionStopTriggered()
{
    if (currentClip() != nullptr)
    {
        ui->actionPlayPause->setIcon(playIcon);
        player->stop();
    }
}

void CClipCutterWindow::ActionSetStartTriggered()
{
    const clipcutter::Clip* clip = currentClip();
    const clipcutter::Segment* segment = currentSegment();
    if (clip == nullptr || segment == nullptr)
    {
        return;
    }
    QString error;
    const auto range = clipcutter::TimeRange::create(
        std::chrono::milliseconds{player->position()}, segment->range.end(), clip->mediaInfo.duration, true, &error);
    if (!range.has_value() || !queueModel->updateTrimRange(currentSegmentId, *range, &error))
    {
        QMessageBox::warning(this, QStringLiteral("ClipCutter"), error);
        return;
    }
    UpdateStartEndUI();
}

void CClipCutterWindow::ActionSetEndTriggered()
{
    const clipcutter::Clip* clip = currentClip();
    const clipcutter::Segment* segment = currentSegment();
    if (clip == nullptr || segment == nullptr)
    {
        return;
    }
    QString error;
    const auto range = clipcutter::TimeRange::create(
        segment->range.start(), std::chrono::milliseconds{player->position()}, clip->mediaInfo.duration, true, &error);
    if (!range.has_value() || !queueModel->updateTrimRange(currentSegmentId, *range, &error))
    {
        QMessageBox::warning(this, QStringLiteral("ClipCutter"), error);
        return;
    }
    UpdateStartEndUI();
}

void CClipCutterWindow::OnVolumeChanged(int volume)
{
    audioOutput->setVolume(static_cast<float>(volume) / 100.0f);
}

void CClipCutterWindow::OnPlayerDurationChanged(qint64 duration)
{
    ui->timelineSlider->setMaximum(static_cast<int>(qBound<qint64>(
        qint64{0}, duration, qint64{std::numeric_limits<int>::max()})));
    if (!currentClipId.isNull() && duration > 0)
    {
        queueModel->updateMediaDuration(currentClipId, std::chrono::milliseconds{duration});
        UpdateStartEndUI();
    }
}

void CClipCutterWindow::OnPlayerPositionChanged(qint64 position)
{
    ui->timelineSlider->setValue(static_cast<int>(qBound<qint64>(
        qint64{0}, position, qint64{std::numeric_limits<int>::max()})));
    ui->timelineLabel->setText(Utility::GetTimeStringFromMilli(position));
}

void CClipCutterWindow::OnClipSelectionChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    if (current.isValid())
    {
        const QUuid segmentId = current.data(clipcutter::ClipQueueModel::SegmentIdRole).toUuid();
        OpenVideo(queueModel->rowForSegment(segmentId));
    }
}

void CClipCutterWindow::OnVideoNameChanged(const QString& newName)
{
    if (!currentSegmentId.isNull())
    {
        queueModel->updateOutputName(currentSegmentId, newName + QStringLiteral(".mp4"));
    }
}

void CClipCutterWindow::OnKeywordChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
    UpdateKeywordUI();
    UpdateActionStates();
}

void CClipCutterWindow::UpdateStartEndUI()
{
    const clipcutter::Segment* segment = currentSegment();
    if (segment == nullptr)
    {
        ui->startEndLabel->setText(QStringLiteral("Start: 00:00:00.000 / End: 00:00:00.000"));
        return;
    }
    ui->startEndLabel->setText(QStringLiteral("Start: %1 / End: %2").arg(
        Utility::GetTimeStringFromMilli(segment->range.start().count()),
        Utility::GetTimeStringFromMilli(segment->range.end().count())));
}

void CClipCutterWindow::UpdateKeywordUI()
{
    const clipcutter::Segment* segment = currentSegment();
    for (int index = 0; index < ui->keywordsTree->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* item = ui->keywordsTree->topLevelItem(index);
        const bool used = segment != nullptr && segment->prefix.has_value()
            && ClipLogic::KeywordsEqual(item->text(KeywordTextColumn), *segment->prefix);
        item->setText(KeywordUseColumn, used ? QStringLiteral("Yes") : QString());
    }
}

void CClipCutterWindow::UpdateCurrentVideoName()
{
    const clipcutter::Segment* segment = currentSegment();
    const QSignalBlocker blocker(ui->clipNameEdit);
    segment == nullptr ? ui->clipNameEdit->clear() : ui->clipNameEdit->setText(segment->outputBaseName);
}

void CClipCutterWindow::UpdateActionStates()
{
    const bool hasVideo = currentSegment() != nullptr;
    const bool hasQueue = queueModel->rowCount() > 0;
    const int row = queueModel->rowForSegment(currentSegmentId);
    ui->actionPlayPause->setEnabled(hasVideo);
    ui->actionNext->setEnabled(hasVideo && row + 1 < queueModel->rowCount());
    ui->actionSkip->setEnabled(hasVideo);
    ui->actionPrev->setEnabled(hasVideo && row > 0);
    ui->actionStop->setEnabled(hasVideo);
    ui->actionSetStart->setEnabled(hasVideo);
    ui->actionSetEnd->setEnabled(hasVideo);
    ui->actionProcessClips->setEnabled(hasQueue);
    ui->clipNameEdit->setEnabled(hasVideo);
    ui->buttonUseSelected->setEnabled(hasVideo && !ui->keywordsTree->selectedItems().isEmpty());
    ui->buttonRemoveSelected->setEnabled(!ui->keywordsTree->selectedItems().isEmpty());
    ui->processButton->setEnabled(hasQueue);
    ui->skipAllButton->setEnabled(hasQueue);
}

void CClipCutterWindow::ClearCurrentClipUI()
{
    currentClipId = {};
    currentSegmentId = {};
    player->stop();
    player->setSource(QUrl());
    ui->actionPlayPause->setIcon(playIcon);
    ui->timelineSlider->setRange(0, 0);
    ui->timelineLabel->setText(QStringLiteral("00:00:00.000"));
    UpdateCurrentVideoName();
    UpdateStartEndUI();
    UpdateKeywordUI();
    UpdateActionStates();
}

void CClipCutterWindow::LoadImportedClips(
    clipcutter::ImportResult result,
    const QDir& directory,
    const QString& preparedOutputDirectory)
{
    ClearCurrentClipUI();
    queueModel->clearClips();
    videoDirectory = directory;
    outputDirectory = preparedOutputDirectory;
    queueModel->addClips(std::move(result.imported));
    if (ui->qualityCombo->currentIndex() >= 0)
    {
        queueModel->updateAllExportProfiles(ui->qualityCombo->currentText().toLower());
    }
    PresentImportMessages(result);
    if (queueModel->rowCount() > 0)
    {
        OpenVideo(0);
    }
}

void CClipCutterWindow::PresentImportMessages(const clipcutter::ImportResult& result)
{
    if (!result.errors.isEmpty())
    {
        QStringList lines;
        for (const clipcutter::ImportError& error : result.errors)
        {
            lines.append(QStringLiteral("%1: %2").arg(error.path, error.message));
        }
        QMessageBox::warning(this, QStringLiteral("ClipCutter"), lines.join(QLatin1Char('\n')));
    }
}

void CClipCutterWindow::OpenVideo(int row)
{
    const QUuid clipId = queueModel->clipIdAtRow(row);
    const QUuid segmentId = queueModel->segmentIdAtRow(row);
    clipcutter::Clip* clip = queueModel->findClip(clipId);
    if (clip == nullptr || segmentId.isNull())
    {
        return;
    }
    currentClipId = clipId;
    currentSegmentId = segmentId;
    player->stop();
    ui->actionPlayPause->setIcon(playIcon);
    player->setSource(QUrl::fromLocalFile(clip->sourcePath));
    player->pause();
    {
        const QSignalBlocker blocker(ui->clipsTable->selectionModel());
        ui->clipsTable->selectionModel()->setCurrentIndex(
            queueModel->index(row, clipcutter::ClipQueueModel::SourceNameColumn),
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    UpdateCurrentVideoName();
    UpdateStartEndUI();
    UpdateKeywordUI();
    UpdateActionStates();
    ui->actionPlayPause->setIcon(pauseIcon);
    player->play();
}

void CClipCutterWindow::DisableActions()
{
    ui->actionOpenFolder->setEnabled(false);
    ui->actionOpenFiles->setEnabled(false);
    ui->actionPlayPause->setEnabled(false);
    ui->actionNext->setEnabled(false);
    ui->actionSkip->setEnabled(false);
    ui->actionPrev->setEnabled(false);
    ui->actionStop->setEnabled(false);
    ui->actionSetStart->setEnabled(false);
    ui->actionSetEnd->setEnabled(false);
    ui->actionProcessClips->setEnabled(false);
    ui->processButton->setEnabled(false);
    ui->skipAllButton->setEnabled(false);
}

void CClipCutterWindow::EnableActions()
{
    ui->actionOpenFolder->setEnabled(true);
    ui->actionOpenFiles->setEnabled(true);
    UpdateActionStates();
}

void CClipCutterWindow::ProcessClips()
{
    if (ui->qualityCombo->currentIndex() == -1)
    {
        QMessageBox::information(this, QStringLiteral("ClipCutter"),
                                 QStringLiteral("Select an output quality before processing clips."));
        return;
    }
    if (queueModel->rowCount() == 0)
    {
        QMessageBox::information(this, QStringLiteral("ClipCutter"), QStringLiteral("There are no clips to process."));
        return;
    }
    QStringList rangeErrors;
    const QVector<clipcutter::ExportSegment> exports = queueModel->exportableSegments(&rangeErrors);
    if (!rangeErrors.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("ClipCutter"), rangeErrors.join(QLatin1Char('\n')));
        return;
    }
    QString preparedOutputDirectory;
    QString outputError;
    if (!Utility::PrepareOutputDirectory(videoDirectory, preparedOutputDirectory, outputError))
    {
        QMessageBox::critical(this, QStringLiteral("ClipCutter"), outputError);
        return;
    }
    outputDirectory = preparedOutputDirectory;
    const QUuid selectedSegmentId = currentSegmentId;
    DisableActions();
    player->stop();
    player->setSource(QUrl());

    for (int index = 0; index < exports.size(); ++index)
    {
        const clipcutter::ExportSegment& segment = exports.at(index);
        ui->progressBar->setValue(exports.isEmpty() ? 100 : index * 100 / exports.size());
        const auto quality = static_cast<clipcutter::ReEncodeQuality>(ui->qualityCombo->currentIndex());
        clipcutter::FFmpeg::ProcessSegment(segment, outputDirectory, quality, false);
        if (!userSettings.copyDateTime)
        {
            continue;
        }

        FILETIME creationTime, accessedTime, modifiedTime;
        const QString originalPath = QDir::toNativeSeparators(segment.sourcePath);
        const HANDLE original = CreateFileW(
            reinterpret_cast<LPCWSTR>(originalPath.utf16()), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (original == INVALID_HANDLE_VALUE)
        {
            QMessageBox::information(this, QStringLiteral("ClipCutter"),
                                     QStringLiteral("Unable to open the original file for metadata. Error: %1")
                                         .arg(GetLastError()));
            continue;
        }
        const bool read = GetFileTime(original, &creationTime, &accessedTime, &modifiedTime);
        CloseHandle(original);
        if (!read)
        {
            QMessageBox::information(this, QStringLiteral("ClipCutter"),
                                     QStringLiteral("Unable to read the original file's timestamps."));
            continue;
        }

        const QString newPath = QDir::toNativeSeparators(
            QDir(outputDirectory).absoluteFilePath(segment.outputFileName));
        const HANDLE output = CreateFileW(
            reinterpret_cast<LPCWSTR>(newPath.utf16()), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (output == INVALID_HANDLE_VALUE)
        {
            QMessageBox::information(this, QStringLiteral("ClipCutter"),
                                     QStringLiteral("Unable to open the output file for metadata."));
        }
        else
        {
            if (!SetFileTime(output, &creationTime, &accessedTime, &modifiedTime))
            {
                QMessageBox::information(this, QStringLiteral("ClipCutter"),
                                         QStringLiteral("Unable to set the output file's timestamps."));
            }
            CloseHandle(output);
        }
    }

    ui->progressBar->setValue(100);
    const int selectedRow = queueModel->rowForSegment(selectedSegmentId);
    if (selectedRow >= 0)
    {
        OpenVideo(selectedRow);
        player->pause();
        ui->actionPlayPause->setIcon(playIcon);
    }
    QMessageBox::information(this, QStringLiteral("ClipCutter"), QStringLiteral("Processing complete."));
    EnableActions();
}

void CClipCutterWindow::MarkAllAsSkipped()
{
    queueModel->updateAllSkipStates(true);
}

void CClipCutterWindow::AddKeyword()
{
    const QString keyword = ClipLogic::NormalizeKeyword(ui->keywordEdit->text());
    if (keyword.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("ClipCutter"), QStringLiteral("Enter a non-empty keyword."));
        return;
    }
    QStringList existing;
    for (int index = 0; index < ui->keywordsTree->topLevelItemCount(); ++index)
    {
        existing.append(ui->keywordsTree->topLevelItem(index)->text(KeywordTextColumn));
    }
    if (ClipLogic::ContainsKeyword(existing, keyword))
    {
        QMessageBox::information(this, QStringLiteral("ClipCutter"),
                                 QStringLiteral("The keyword \"%1\" already exists.").arg(keyword));
        return;
    }
    auto* item = new QTreeWidgetItem(ui->keywordsTree);
    item->setText(KeywordTextColumn, keyword);
    ui->keywordEdit->clear();
    ui->keywordsTree->setCurrentItem(item);
    UpdateKeywordUI();
    UpdateActionStates();
}

void CClipCutterWindow::RemoveKeyword()
{
    const QList<QTreeWidgetItem*> selected = ui->keywordsTree->selectedItems();
    for (QTreeWidgetItem* item : selected)
    {
        queueModel->clearPrefixFromAll(item->text(KeywordTextColumn));
        delete item;
    }
    UpdateKeywordUI();
    UpdateActionStates();
}

void CClipCutterWindow::UseKeyword()
{
    clipcutter::Segment* segment = currentSegment();
    const QList<QTreeWidgetItem*> selected = ui->keywordsTree->selectedItems();
    if (segment == nullptr || selected.isEmpty())
    {
        return;
    }
    const QString keyword = selected.constFirst()->text(KeywordTextColumn);
    if (segment->prefix.has_value() && ClipLogic::KeywordsEqual(*segment->prefix, keyword))
    {
        queueModel->clearPrefix(segment->id);
    }
    else
    {
        queueModel->applyPrefix(segment->id, keyword);
    }
    UpdateKeywordUI();
    UpdateActionStates();
}

clipcutter::Clip* CClipCutterWindow::currentClip()
{
    return queueModel->findClip(currentClipId);
}

const clipcutter::Clip* CClipCutterWindow::currentClip() const
{
    return queueModel->findClip(currentClipId);
}

clipcutter::Segment* CClipCutterWindow::currentSegment()
{
    return queueModel->findSegment(currentSegmentId);
}

const clipcutter::Segment* CClipCutterWindow::currentSegment() const
{
    return queueModel->findSegment(currentSegmentId);
}
