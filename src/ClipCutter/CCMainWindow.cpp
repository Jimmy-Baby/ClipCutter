#include <QAudioOutput>
#include <QFileDialog>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSlider>
#include <QTreeWidgetItem>
#include <QVideoWidget>
#include <qt_windows.h>

#include <limits>

#include "CCMainWindow.h"
#include "ClipLogic.h"
#include "FFmpeg.h"
#include "ui_CCMainWindow.h"
#include "Utility.h"

namespace
{
    constexpr int ClipSkipColumn = 0;
    constexpr int ClipNameColumn = 1;
    constexpr int KeywordUseColumn = 0;
    constexpr int KeywordTextColumn = 1;
}

CClipCutterWindow::CClipCutterWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::ClipCutterWindow),
      playIcon(QStringLiteral(":/icons/icons/play-solid.svg")),
      pauseIcon(QStringLiteral(":/icons/icons/pause-solid.svg")),
      currentVideo(nullptr)
{
    ui->setupUi(this);

    // Connect qt actions
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

    connect(ui->clipsTree, &QTreeWidget::currentItemChanged, this, &CClipCutterWindow::OnVideoListItemChanged);
    connect(ui->clipNameEdit, &QLineEdit::textChanged, this, &CClipCutterWindow::OnVideoNameChanged);
    connect(ui->processButton, &QPushButton::pressed, this, &CClipCutterWindow::ProcessClips);
    connect(ui->skipAllButton, &QPushButton::pressed, this, &CClipCutterWindow::MarkAllAsSkipped);
    ui->checkboxCopyMetadata->setCheckState(Qt::CheckState::Checked);
    connect(ui->checkboxCopyMetadata, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state)
    {
        userSettings.copyDateTime = state == Qt::CheckState::Checked;
    });

    connect(ui->buttonAddKeyword, &QPushButton::pressed, this, &CClipCutterWindow::AddKeyword);
    connect(ui->buttonRemoveSelected, &QPushButton::pressed, this, &CClipCutterWindow::RemoveKeyword);
    connect(ui->buttonUseSelected, &QPushButton::pressed, this, &CClipCutterWindow::UseKeyword);
    connect(ui->keywordsTree, &QTreeWidget::currentItemChanged, this, &CClipCutterWindow::OnKeywordChanged);

    // Setup media player
    videoWidget = new QVideoWidget(ui->playerBox);
    ui->playerLayout->addWidget(videoWidget);

    player = new QMediaPlayer(this);
    player->setVideoOutput(videoWidget);

    audioOutput = new QAudioOutput(this);
    player->setAudioOutput(audioOutput);

    // Adjust slider properties when player changes
    connect(player, &QMediaPlayer::durationChanged, this, &CClipCutterWindow::OnPlayerDurationChanged);
    connect(player, &QMediaPlayer::positionChanged, this, &CClipCutterWindow::OnPlayerPositionChanged);

    // Adjust player video position when slider is dragged by user
    connect(ui->timelineSlider, &QSlider::sliderMoved, player, &QMediaPlayer::setPosition);

    // Adjust player volume
    audioOutput->setVolume(1.0f);
    connect(ui->volumeSlider, &QSlider::sliderMoved, this, &CClipCutterWindow::OnVolumeChanged);

    // Setup quality dropdown
    QStringList qualityItems;
    qualityItems << "Copy";
    qualityItems << "Lowest";
    qualityItems << "Low";
    qualityItems << "Medium";
    qualityItems << "High";
    qualityItems << "Best";
    ui->qualityCombo->addItems(qualityItems);

    ClearCurrentClipUI();
}


CClipCutterWindow::~CClipCutterWindow()
{
    delete ui;
}


void CClipCutterWindow::ActionOpenFolderTriggered()
{
    const QFileDialog::Options dialogFlags = QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks;
    const QString folderString = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "/home", dialogFlags);

    if (folderString.isEmpty())
    {
        return;
    }

    const QDir selectedDirectory(folderString);
    const QStringList videoNames = selectedDirectory.entryList(
        { QStringLiteral("*.mp4"), QStringLiteral("*.mkv"), QStringLiteral("*.avi"), QStringLiteral("*.mov") },
        QDir::Files,
        QDir::Name);

    if (videoNames.isEmpty())
    {
        QMessageBox::information(
            this,
            QStringLiteral("ClipCutter"),
            QStringLiteral("No supported video files were found in:\n%1").arg(selectedDirectory.absolutePath()));
        return;
    }

    QString preparedOutputDirectory;
    QString outputError;
    if (!Utility::PrepareOutputDirectory(selectedDirectory, preparedOutputDirectory, outputError))
    {
        QMessageBox::critical(this, QStringLiteral("ClipCutter"), outputError);
        return;
    }

    QStringList filePaths;
    filePaths.reserve(videoNames.size());
    for (const QString& videoName : videoNames)
    {
        filePaths.append(selectedDirectory.absoluteFilePath(videoName));
    }

    LoadVideoFiles(filePaths, selectedDirectory, preparedOutputDirectory);
}


void CClipCutterWindow::ActionOpenFilesTriggered()
{
    const QString fileFilter = tr("Video Files (*.mp4 *.mkv *.avi *.mov)");
    const QStringList filePaths = QFileDialog::getOpenFileNames(this, tr("Open File"), "/home", fileFilter);

    if (filePaths.isEmpty())
    {
        return;
    }

    const QFileInfo firstFileInfo(filePaths.constFirst());

    for (const QString& filePath : filePaths)
    {
        const QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isFile())
        {
            QMessageBox::warning(
                this,
                QStringLiteral("ClipCutter"),
                QStringLiteral("A selected video file is unavailable:\n%1").arg(fileInfo.absoluteFilePath()));
            return;
        }
    }

    const QDir selectedDirectory = firstFileInfo.dir();
    QString preparedOutputDirectory;
    QString outputError;
    if (!Utility::PrepareOutputDirectory(selectedDirectory, preparedOutputDirectory, outputError))
    {
        QMessageBox::critical(this, QStringLiteral("ClipCutter"), outputError);
        return;
    }

    LoadVideoFiles(filePaths, selectedDirectory, preparedOutputDirectory);
}


void CClipCutterWindow::ActionPlayPauseTriggered()
{
    if (currentVideo == nullptr)
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
    if (currentVideo == nullptr)
    {
        return;
    }

    const int currentIndex = currentVideo->ListIndex;
    const int nextIndex = currentIndex + 1;

    // If we are at end of list, do nothing
    if (nextIndex < 0 || nextIndex >= static_cast<int>(videoList.size()))
    {
        return;
    }

    OpenVideo(nextIndex);
}


void CClipCutterWindow::ActionSkipTriggered()
{
    if (currentVideo == nullptr)
    {
        return;
    }

    currentVideo->Skip = true;
    currentVideo->TreeItem->setCheckState(ClipSkipColumn, Qt::CheckState::Checked);

    ActionNextTriggered();
    UpdateActionStates();
}

void CClipCutterWindow::ActionPrevTriggered()
{
    if (currentVideo == nullptr)
    {
        return;
    }

    const int currentIndex = currentVideo->ListIndex;

    // If we are at start of list, do nothing
    if (currentIndex == 0)
    {
        return;
    }

    OpenVideo(currentIndex - 1);
}


void CClipCutterWindow::ActionStopTriggered()
{
    if (currentVideo == nullptr)
    {
        return;
    }

    ui->actionPlayPause->setIcon(playIcon);
    player->stop();
}


void CClipCutterWindow::ActionSetStartTriggered()
{
    if (currentVideo == nullptr)
    {
        return;
    }

    const qint64 videoTimeMs = player->position();

    // Make sure the start time we are going to set is not after the currently set EndTimeMs
    if (videoTimeMs >= currentVideo->EndTimeMs)
    {
        QMessageBox::warning(this, "ClipCutter", "Start point must be before end point", QMessageBox::Ok);
        return;
    }

    // Set the time
    currentVideo->StartTimeMs = videoTimeMs;

    // Update start/end display
    UpdateStartEndUI();
}


void CClipCutterWindow::ActionSetEndTriggered()
{
    // First check if a video is loaded
    if (currentVideo == nullptr)
    {
        return;
    }

    const qint64 videoTimeMs = player->position();

    // Make sure the end time we are going to set is after the currently set StartTimeMs
    if (videoTimeMs <= currentVideo->StartTimeMs)
    {
        QMessageBox::warning(this, "ClipCutter", "End point must be after start point", QMessageBox::Ok);
        return;
    }

    // Set the time
    currentVideo->EndTimeMs = videoTimeMs;

    // Update start/end display
    UpdateStartEndUI();
}


void CClipCutterWindow::OnVolumeChanged(int volume)
{
    const float volumeAsFloat = static_cast<float>(volume) / 100.0f;
    audioOutput->setVolume(volumeAsFloat);
}


void CClipCutterWindow::OnPlayerDurationChanged(qint64 duration)
{
    const int sliderMaximum = static_cast<int>(qBound<qint64>(
        qint64(0), duration, qint64(std::numeric_limits<int>::max())));
    ui->timelineSlider->setMaximum(sliderMaximum);

    if (currentVideo != nullptr && !currentVideo->HasBeenOpened && duration > 0)
    {
        currentVideo->EndTimeMs = duration;
        currentVideo->HasBeenOpened = true;
        UpdateStartEndUI();
    }
}


void CClipCutterWindow::OnPlayerPositionChanged(qint64 position)
{
    const int sliderPosition = static_cast<int>(qBound<qint64>(
        qint64(0), position, qint64(std::numeric_limits<int>::max())));
    ui->timelineSlider->setValue(sliderPosition);
    ui->timelineLabel->setText(Utility::GetTimeStringFromMilli(position));
}


void CClipCutterWindow::OnVideoListItemChanged(QTreeWidgetItem* curr, QTreeWidgetItem* prev)
{
    (void)prev;

    if (curr == nullptr)
    {
        return;
    }

    int index = GetVideoIndexFromTreeItem(curr);

    if (index == -1)
    {
        QMessageBox::warning(nullptr, "ClipCutter", "Failed to find video from tree item");
        return;
    }

    OpenVideo(index);
}


void CClipCutterWindow::OnVideoNameChanged(const QString& newName)
{
    if (currentVideo == nullptr)
    {
        return;
    }

    currentVideo->VideoName = newName + QStringLiteral(".mp4");
    UpdateCurrentVideoName();
}

void CClipCutterWindow::OnKeywordChanged(QTreeWidgetItem *curr, QTreeWidgetItem *prev)
{
    (void)prev;

    (void)curr;
    UpdateKeywordUI();
    UpdateActionStates();
}


int CClipCutterWindow::GetVideoIndexFromTreeItem(const QTreeWidgetItem* treeItem) const
{
    for (auto it = videoList.begin(); it != videoList.end(); ++it)
    {
        const QueueItem* item = it->get();
        if (item->TreeItem == treeItem)
        {
            return item->ListIndex;
        }
    }

    return -1;
}


void CClipCutterWindow::UpdateStartEndUI()
{
    if (currentVideo == nullptr)
    {
        ui->startEndLabel->setText(QStringLiteral("Start: 00:00:00.000 / End: 00:00:00.000"));
        return;
    }

    const QString label = QString("Start: %1 / End: %2")
                              .arg(Utility::GetTimeStringFromMilli(currentVideo->StartTimeMs),
                                   Utility::GetTimeStringFromMilli(currentVideo->EndTimeMs));

    ui->startEndLabel->setText(label);
}

void CClipCutterWindow::UpdateKeywordUI()
{
    for (int index = 0; index < ui->keywordsTree->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* keywordItem = ui->keywordsTree->topLevelItem(index);
        const bool isUsed = currentVideo != nullptr
            && !currentVideo->keyword.isEmpty()
            && ClipLogic::KeywordsEqual(keywordItem->text(KeywordTextColumn), currentVideo->keyword);
        keywordItem->setText(KeywordUseColumn, isUsed ? QStringLiteral("Yes") : QString());
    }
}

void CClipCutterWindow::UpdateCurrentVideoName()
{
    if (currentVideo == nullptr || currentVideo->TreeItem == nullptr)
    {
        return;
    }

    currentVideo->TreeItem->setText(ClipNameColumn, currentVideo->GetOutputName());
}

void CClipCutterWindow::UpdateActionStates()
{
    const bool hasVideo = currentVideo != nullptr;
    const bool hasQueue = !videoList.empty();
    const int currentIndex = hasVideo ? currentVideo->ListIndex : -1;

    ui->actionPlayPause->setEnabled(hasVideo);
    ui->actionNext->setEnabled(hasVideo && currentIndex + 1 < static_cast<int>(videoList.size()));
    ui->actionSkip->setEnabled(hasVideo);
    ui->actionPrev->setEnabled(hasVideo && currentIndex > 0);
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
    currentVideo = nullptr;
    player->stop();
    player->setSource(QUrl());
    ui->actionPlayPause->setIcon(playIcon);
    ui->timelineSlider->setRange(0, 0);
    ui->timelineLabel->setText(QStringLiteral("00:00:00.000"));

    const QSignalBlocker nameBlocker(ui->clipNameEdit);
    ui->clipNameEdit->clear();

    UpdateStartEndUI();
    UpdateKeywordUI();
    UpdateActionStates();
}
void CClipCutterWindow::LoadVideoFiles(
    const QStringList& filePaths,
    const QDir& directory,
    const QString& preparedOutputDirectory)
{
    ClearCurrentClipUI();

    {
        const QSignalBlocker clipsBlocker(ui->clipsTree);
        videoList.clear();
        ui->clipsTree->clear();

        videoDirectory = directory;
        outputDirectory = preparedOutputDirectory;

        videoList.reserve(static_cast<std::size_t>(filePaths.size()));
        for (int index = 0; index < filePaths.size(); ++index)
        {
            const QFileInfo fileInfo(filePaths.at(index));
            auto queueItem = std::make_unique<QueueItem>();
            queueItem->ListIndex = index;
            queueItem->VideoName = fileInfo.fileName();
            queueItem->OriginalPath = fileInfo.absoluteFilePath();

            QTreeWidgetItem* treeItem = new QTreeWidgetItem(ui->clipsTree);
            treeItem->setCheckState(ClipSkipColumn, Qt::CheckState::Unchecked);
            treeItem->setText(ClipNameColumn, queueItem->VideoName);
            queueItem->TreeItem = treeItem;

            QueueItem* queueItemPointer = queueItem.get();
            videoList.push_back(std::move(queueItem));
            connect(ui->clipsTree, &QTreeWidget::itemChanged, queueItemPointer, &QueueItem::UpdateSkip);
        }
    }

    OpenVideo(0);
}


void CClipCutterWindow::OpenVideo(int videoIndex)
{
    if (videoIndex < 0 || videoIndex >= static_cast<int>(videoList.size()))
    {
        return;
    }

    currentVideo = videoList[static_cast<std::size_t>(videoIndex)].get();

    ActionStopTriggered();
    player->setSource(QUrl::fromLocalFile(currentVideo->OriginalPath));
    player->pause();

    if (!currentVideo->HasBeenOpened && player->duration() > 0)
    {
        currentVideo->EndTimeMs = player->duration();
        currentVideo->HasBeenOpened = true;
    }

    const int sliderMaximum = static_cast<int>(qBound<qint64>(
        qint64(0), player->duration(), qint64(std::numeric_limits<int>::max())));
    ui->timelineSlider->setMaximum(sliderMaximum);

    {
        const QSignalBlocker clipsBlocker(ui->clipsTree);
        ui->clipsTree->setCurrentItem(currentVideo->TreeItem);
    }

    {
        const QSignalBlocker nameBlocker(ui->clipNameEdit);
        ui->clipNameEdit->setText(QFileInfo(currentVideo->VideoName).completeBaseName());
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
        QMessageBox::information(this, "ClipCutter", "Select an output quality before processing clips.");
        return;
    }

    if (videoList.empty())
    {
        QMessageBox::information(this, "ClipCutter", "There are no clips to process.");
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

    DisableActions();
    player->stop();
    player->setSource(QUrl());

    for (std::size_t index = 0; index < videoList.size(); ++index)
    {
        const QueueItem* queueItem = videoList[index].get();

        const int progress = static_cast<int>(index * 100 / videoList.size());
        ui->progressBar->setValue(progress);

        if (queueItem->Skip)
        {
            continue;
        }

        const EReEncodeQuality quality = static_cast<EReEncodeQuality>(ui->qualityCombo->currentIndex());
        FFmpeg::ProcessQueueItem(queueItem, outputDirectory, quality, false);

        if (userSettings.copyDateTime)
        {
            FILETIME creationTime, accessedTime, modifiedTime;
            const QString originalPath = QDir::toNativeSeparators(queueItem->OriginalPath);
            const HANDLE hOriginalFile = CreateFileW(
                reinterpret_cast<LPCWSTR>(originalPath.utf16()),
                FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (hOriginalFile == INVALID_HANDLE_VALUE)
            {
                QMessageBox::information(
                    this,
                    "ClipCutter",
                    QString("Unable to open the original file for metadata. Error: %1").arg(GetLastError()));
                continue;
            }

            const bool timestampsRead = GetFileTime(hOriginalFile, &creationTime, &accessedTime, &modifiedTime);
            CloseHandle(hOriginalFile);

            if (!timestampsRead)
            {
                QMessageBox::information(this, "ClipCutter", "Unable to read the original file's timestamps.");
                continue;
            }

            const QString newFilePath = QDir::toNativeSeparators(
                QDir(outputDirectory).absoluteFilePath(queueItem->GetOutputName()));
            const HANDLE hNewFile = CreateFileW(
                reinterpret_cast<LPCWSTR>(newFilePath.utf16()),
                FILE_WRITE_ATTRIBUTES,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (hNewFile == INVALID_HANDLE_VALUE)
            {
                QMessageBox::information(this, "ClipCutter", "Unable to open the output file for metadata.");
            }
            else
            {
                if (!SetFileTime(hNewFile, &creationTime, &accessedTime, &modifiedTime))
                {
                    QMessageBox::information(this, "ClipCutter", "Unable to set the output file's timestamps.");
                }

                CloseHandle(hNewFile);
            }
        }
    }

    ui->progressBar->setValue(100);

    if (currentVideo != nullptr)
    {
        const int currentIndex = currentVideo->ListIndex;
        OpenVideo(currentIndex);
        player->pause();
        ui->actionPlayPause->setIcon(playIcon);
    }

    QMessageBox::information(this, "ClipCutter", "Processing complete.");
    EnableActions();
}

void CClipCutterWindow::MarkAllAsSkipped()
{
    for (auto it = videoList.begin(); it != videoList.end(); ++it)
    {
        QueueItem* item = it->get();
        item->TreeItem->setCheckState(ClipSkipColumn, Qt::CheckState::Checked);
        item->Skip = true;
    }
}

void CClipCutterWindow::AddKeyword()
{
    const QString keyword = ClipLogic::NormalizeKeyword(ui->keywordEdit->text());

    if (keyword.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("ClipCutter"), QStringLiteral("Enter a non-empty keyword."));
        return;
    }

    QStringList existingKeywords;
    existingKeywords.reserve(ui->keywordsTree->topLevelItemCount());
    for (int index = 0; index < ui->keywordsTree->topLevelItemCount(); ++index)
    {
        existingKeywords.append(ui->keywordsTree->topLevelItem(index)->text(KeywordTextColumn));
    }

    if (ClipLogic::ContainsKeyword(existingKeywords, keyword))
    {
        QMessageBox::information(
            this,
            QStringLiteral("ClipCutter"),
            QStringLiteral("The keyword \"%1\" already exists.").arg(keyword));
        return;
    }

    QTreeWidgetItem* treeItem = new QTreeWidgetItem(ui->keywordsTree);
    treeItem->setText(KeywordTextColumn, keyword);
    ui->keywordEdit->clear();
    ui->keywordsTree->setCurrentItem(treeItem);
    UpdateKeywordUI();
    UpdateActionStates();
}

void CClipCutterWindow::RemoveKeyword()
{
    const QList<QTreeWidgetItem*> treeItemsToRemove = ui->keywordsTree->selectedItems();

    for (QTreeWidgetItem* item : treeItemsToRemove)
    {
        const QString removedKeyword = item->text(KeywordTextColumn);

        for (const std::unique_ptr<QueueItem>& videoItem : videoList)
        {
            QueueItem* video = videoItem.get();

            if (ClipLogic::KeywordsEqual(video->keyword, removedKeyword))
            {
                video->keyword.clear();
                video->TreeItem->setText(ClipNameColumn, video->GetOutputName());
            }
        }

        delete item;
    }

    UpdateKeywordUI();
    UpdateActionStates();
}

void CClipCutterWindow::UseKeyword()
{
    if (currentVideo == nullptr)
    {
        return;
    }

    const QList<QTreeWidgetItem*> selectedItems = ui->keywordsTree->selectedItems();

    if (selectedItems.isEmpty())
    {
        return;
    }

    const QTreeWidgetItem* keywordItem = selectedItems.constFirst();
    const QString keywordText = keywordItem->text(KeywordTextColumn);

    if (ClipLogic::KeywordsEqual(keywordText, currentVideo->keyword))
    {
        currentVideo->keyword.clear();
    }
    else
    {
        currentVideo->keyword = keywordText;
    }

    UpdateCurrentVideoName();
    UpdateKeywordUI();
    UpdateActionStates();
}
