#include <QMessageBox>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QFileDialog>
#include <QSlider>
#include <QRandomGenerator>
#include <QTreeWidgetItem>
#include <QVector>
#include <qt_windows.h>

#include "ccmainwindow.h"
#include "ui_ccmainwindow.h"
#include "utility.h"
#include "ffmpeg.h"

CClipCutterWindow::CClipCutterWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::ClipCutterWindow), currentVideo(nullptr)
{
    ui->setupUi(this);

    playIcon = new QIcon(":/icons/icons/play-solid.svg");
    pauseIcon = new QIcon(":/icons/icons/pause-solid.svg");

    // Connect qt actions
    connect(ui->actionOpenFolder, &QAction::triggered, this, &CClipCutterWindow::ActionOpenFolderTriggered);
    connect(ui->actionOpenFile, &QAction::triggered, this, &CClipCutterWindow::ActionOpenFileTriggered);
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
    connect(ui->checkboxCopyMetadata, &QCheckBox::stateChanged, this, &CClipCutterWindow::OnCopyMetadataChanged);
    connect(ui->checkboxShowFfmpeg, &QCheckBox::stateChanged, this, &CClipCutterWindow::OnShowFfmpegChanged);

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
    qualityItems << "Low";
    qualityItems << "Medium";
    qualityItems << "High";
    ui->qualityCombo->addItems(qualityItems);

    // Setup encoding speed dropdown
    QStringList encodingSpeedItems;
    encodingSpeedItems << "Fast";
    encodingSpeedItems << "Medium";
    encodingSpeedItems << "Slow";
    ui->presetCombo->addItems(encodingSpeedItems);
}


CClipCutterWindow::~CClipCutterWindow()
{
    delete ui;
}


void CClipCutterWindow::ActionOpenFolderTriggered()
{
    // Make sure the player is paused
    ui->actionPlayPause->setIcon(*playIcon);
    player->pause();

    // Folder Selection
    QFlags dialogFlags = QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks;
    const QString folderString = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "/home", dialogFlags);

    // Check if get directory failed
    if (folderString.isEmpty())
    {
        return;
    }

    // Make sure player is stopped and cleared first
    player->stop();

    // Assign new directory
    videoDirectory = QDir(folderString);

    // Clear lists incase previous items exist
    videoList.clear();
    ui->clipsTree->clear();

    // Get list of compatible videos
    QStringList openedVideoList = videoDirectory.entryList({ "*.mp4", "*.mkv", "*.avi", "*.mov" }, QDir::Files);

    if (openedVideoList.isEmpty())
    {
        // DisableUI();

        QMessageBox::information(this, "ClipCutter", "No compatible video types found", QMessageBox::Ok);
        return;
    }

    // Create video list from file list
    for (int index = 0; index < openedVideoList.length(); ++index)
    {
        auto& item = openedVideoList[index];

        // Setup tree list item
        QTreeWidgetItem* treeItem = new QTreeWidgetItem(ui->clipsTree);
        treeItem->setCheckState(0, Qt::CheckState::Unchecked);
        treeItem->setText(1, item);

        // Create queue item
        QueueItem* newItem = new QueueItem;
        newItem->ListIndex = index;
        newItem->TreeItem = treeItem;
        newItem->VideoName = item;
        newItem->OriginalPath = videoDirectory.absoluteFilePath(item);
        newItem->EndTimeMs = Utility::GetVideoLength(newItem->OriginalPath);

        // Add queue item to list
        videoList.push_back(std::unique_ptr<QueueItem>(newItem));

        // Connect signal for 'Skip?' checkbox, do this with item that was added to list
        connect(ui->clipsTree, &QTreeWidget::itemChanged, newItem, &QueueItem::UpdateSkip);
    }

    // Create output folder
    const QDir outDir(folderString);
    if (!outDir.exists("ClipCutterOutput"))
    {
        [[maybe_unused]] bool directoryMade = outDir.mkdir("ClipCutterOutput");
    }

    // Store output directory
    outputDirectory = outDir.path() + QDir::separator() + "ClipCutterOutput";

    // Open first video in list
    OpenVideo(0);
}

void CClipCutterWindow::ActionOpenFileTriggered()
{
    // Make sure the player is paused
    ui->actionPlayPause->setIcon(*playIcon);
    player->pause();

    // Folder Selection
    const QString fileFilter = tr("Video Files (*.mp4 *.mkv *.avi *.mov)");
    const QString filePathString = QFileDialog::getOpenFileName(this, tr("Open File"), "/home", fileFilter);
    const QFileInfo fileInfo(filePathString);

    // Check if get directory failed
    if (filePathString.isEmpty() || !fileInfo.exists())
    {
        return;
    }

    // Make sure player is stopped and cleared first
    player->stop();

    // Set video directory
    videoDirectory = fileInfo.dir();

    // Clear lists incase previous items exist
    videoList.clear();
    ui->clipsTree->clear();

    // Setup tree list item
    QTreeWidgetItem* treeItem = new QTreeWidgetItem(ui->clipsTree);
    treeItem->setCheckState(0, Qt::CheckState::Unchecked);
    treeItem->setText(1, fileInfo.fileName());

    // Create queue item
    QueueItem* newItem = new QueueItem;
    newItem->ListIndex = 0;
    newItem->TreeItem = treeItem;
    newItem->VideoName = fileInfo.fileName();
    newItem->OriginalPath = fileInfo.absoluteFilePath();
    newItem->EndTimeMs = Utility::GetVideoLength(newItem->OriginalPath);

    // Add queue item to list
    videoList.push_back(std::unique_ptr<QueueItem>(newItem));

    // Connect signal for 'Skip?' checkbox, do this with item that was added to list
    connect(ui->clipsTree, &QTreeWidget::itemChanged, newItem, &QueueItem::UpdateSkip);

    // Create output folder
    if (!videoDirectory.exists("ClipCutterOutput"))
    {
        [[maybe_unused]] bool directoryMade = videoDirectory.mkdir("ClipCutterOutput");
    }

    // Store output directory
    outputDirectory = videoDirectory.path() + QDir::separator() + "ClipCutterOutput";

    // Open first video in list
    OpenVideo(0);
}

void CClipCutterWindow::ActionOpenFilesTriggered()
{
    // Make sure the player is paused
    ui->actionPlayPause->setIcon(*playIcon);
    player->pause();

    // Folder Selection
    const QString fileFilter = tr("Video Files (*.mp4 *.mkv *.avi *.mov)");
    const QStringList filePaths = QFileDialog::getOpenFileNames(this, tr("Open File"), "/home", fileFilter);
    const QFileInfo firstFileInfo(filePaths[0]);

    // Check if get directory failed
    if (filePaths.isEmpty() || !firstFileInfo.exists())
    {
        return;
    }

    // Make sure player is stopped and cleared first
    player->stop();

    // Set video directory
    videoDirectory = firstFileInfo.dir();

    // Clear lists incase previous items exist
    videoList.clear();
    ui->clipsTree->clear();

    // Create video list from file list
    for (int index = 0; index < filePaths.length(); ++index)
    {
        const QFileInfo fileInfo(filePaths[index]);

        // Setup tree list item
        QTreeWidgetItem* treeItem = new QTreeWidgetItem(ui->clipsTree);
        treeItem->setCheckState(0, Qt::CheckState::Unchecked);
        treeItem->setText(1, fileInfo.fileName());

        // Create queue item
        QueueItem* newItem = new QueueItem;
        newItem->ListIndex = index;
        newItem->TreeItem = treeItem;
        newItem->VideoName = fileInfo.fileName();
        newItem->OriginalPath = fileInfo.absoluteFilePath();
        newItem->EndTimeMs = Utility::GetVideoLength(newItem->OriginalPath);

        // Add queue item to list
        videoList.push_back(std::unique_ptr<QueueItem>(newItem));

        // Connect signal for 'Skip?' checkbox, do this with item that was added to list
        connect(ui->clipsTree, &QTreeWidget::itemChanged, newItem, &QueueItem::UpdateSkip);
    }

    // Create output folder
    if (!videoDirectory.exists("ClipCutterOutput"))
    {
        [[maybe_unused]] bool directoryMade = videoDirectory.mkdir("ClipCutterOutput");
    }

    // Store output directory
    outputDirectory = videoDirectory.path() + QDir::separator() + "ClipCutterOutput";

    // Open first video in list
    OpenVideo(0);
}


void CClipCutterWindow::ActionPlayPauseTriggered()
{
    if (player->isPlaying())
    {
        ui->actionPlayPause->setIcon(*playIcon);
        player->pause();
    }
    else
    {
        ui->actionPlayPause->setIcon(*pauseIcon);
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
    if (nextIndex == videoList.size())
    {
        return;
    }

    OpenVideo(nextIndex);
}


void CClipCutterWindow::ActionSkipTriggered()
{
    currentVideo->Skip = true;
    currentVideo->TreeItem->setCheckState(0, Qt::CheckState::Checked);

    ActionNextTriggered();
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

    ui->actionPlayPause->setIcon(*playIcon);
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
    float volumeAsFloat = static_cast<float>(volume) / 100.0f;
    audioOutput->setVolume(volumeAsFloat);
    userSettings.preferredVolume = volumeAsFloat;
}


void CClipCutterWindow::OnPlayerDurationChanged(qint64 duration)
{
    ui->timelineSlider->setMaximum(duration);
}


void CClipCutterWindow::OnPlayerPositionChanged(qint64 position)
{
    ui->timelineSlider->setValue(position);
    ui->timelineLabel->setText(Utility::GetTimeStringFromMilli(position));
}


void CClipCutterWindow::OnVideoListItemChanged(QTreeWidgetItem* curr, QTreeWidgetItem* prev)
{
    if (curr == nullptr) // probably means we just opened an empty folder
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


void CClipCutterWindow::OnVideoNameChanged(QString newName)
{
    if (currentVideo == nullptr)
    {
        return;
    }

    QString newNameWithExtension = newName + ".mp4";
    currentVideo->VideoName = newNameWithExtension;
    currentVideo->TreeItem->setText(1, currentVideo->GetOutputName());
}

void CClipCutterWindow::OnCopyMetadataChanged(int value)
{
    userSettings.copyDateTime = value;
}

void CClipCutterWindow::OnShowFfmpegChanged(int value)
{
    userSettings.showFfmpeg = value;
}

void CClipCutterWindow::OnKeywordChanged(QTreeWidgetItem *curr, QTreeWidgetItem *prev)
{
	if (currentVideo == nullptr || curr == nullptr)
    {
        return;
    }

    currentVideo->keyword = curr->text(0);
}


int CClipCutterWindow::GetVideoIndexFromTreeItem(QTreeWidgetItem* treeItem)
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
    const QString label = QString("Start: %1 / End: %2")
                              .arg(Utility::GetTimeStringFromMilli(currentVideo->StartTimeMs),
                                   Utility::GetTimeStringFromMilli(currentVideo->EndTimeMs));

    ui->startEndLabel->setText(label);
}


void CClipCutterWindow::OpenVideo(qint64 videoIndex)
{
    currentVideo = videoList.at(videoIndex).get();

    ActionStopTriggered();
    player->setSource(QUrl::fromLocalFile(currentVideo->OriginalPath));
    player->pause();

    if (!currentVideo->HasBeenOpened)
    {
        currentVideo->EndTimeMs = player->duration();
        currentVideo->HasBeenOpened = true;
    }

    // Set increment for timeline slider to match length of video
    ui->timelineSlider->setMaximum(player->duration());

    // Set list selected item
    ui->clipsTree->setCurrentItem(currentVideo->TreeItem);

    // Set the clip name text, while removing the extension
    ui->clipNameEdit->setText(QFileInfo(currentVideo->VideoName).completeBaseName());

    // Update start/end display
    UpdateStartEndUI();

    // Play the player
    ui->actionPlayPause->setIcon(*pauseIcon);
    player->play();
}


void CClipCutterWindow::DisableActions()
{
    ui->actionOpenFolder->setDisabled(true);
    ui->actionPlayPause->setDisabled(true);
    ui->actionNext->setDisabled(true);
    ui->actionSkip->setDisabled(true);
    ui->actionPrev->setDisabled(true);
    ui->actionStop->setDisabled(true);
    ui->actionSetStart->setDisabled(true);
    ui->actionSetEnd->setDisabled(true);
}


void CClipCutterWindow::EnableActions()
{
    ui->actionOpenFolder->setDisabled(false);
    ui->actionPlayPause->setDisabled(false);
    ui->actionNext->setDisabled(false);
    ui->actionSkip->setDisabled(false);
    ui->actionPrev->setDisabled(false);
    ui->actionStop->setDisabled(false);
    ui->actionSetStart->setDisabled(false);
    ui->actionSetEnd->setDisabled(false);
}


void CClipCutterWindow::ProcessClips()
{
    if (ui->qualityCombo->currentIndex() == -1)
    {
        QMessageBox::information(nullptr, "ClipCutter", "Set a output quality level before processing your clips");
        return;
    }

    if (videoList.empty())
    {
        QMessageBox::information(nullptr, "ClipCutter", "No clips to process!");
        return;
    }

    DisableActions();
    player->stop();
    player->setSource(QUrl());

    for (int index = 0; index < videoList.size(); ++index)
    {
        const QueueItem* queueItem = videoList.at(index).get();

        // Set progress bar
        const int progress = static_cast<float>(index) * (100.0f / static_cast<float>(videoList.size()));
        ui->progressBar->setValue(progress);

        if (queueItem->Skip)
        {
            continue;
        }

        EReEncodeQuality quality = static_cast<EReEncodeQuality>(ui->qualityCombo->currentIndex());
        EReEncodeSpeed preset = static_cast<EReEncodeSpeed>(ui->presetCombo->currentIndex());
        FFmpeg::ProcessQueueItem(queueItem, outputDirectory, quality, preset, userSettings.showFfmpeg);

        if (userSettings.copyDateTime)
        {
            FILETIME creationTime, accessedTime, modifiedTime;

            // Get original times
            HANDLE hOriginalFile;
            QByteArray originalFilePathArr = queueItem->OriginalPath.toLatin1();
            LPCSTR originalFilePath = originalFilePathArr.data();
            hOriginalFile = CreateFileA(originalFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

            if (hOriginalFile == INVALID_HANDLE_VALUE)
            {
                QMessageBox::information(this, "ClipCutter", "Unable to open original file for metadata");
                QString umm = QString("%1").arg(GetLastError());
                QMessageBox::information(this, "ClipCutter", umm);
            }
            else
            {
                if(!GetFileTime(hOriginalFile, &creationTime, &accessedTime, &modifiedTime))
                {
                    QMessageBox::information(this, "ClipCutter", "Unable to get date and time");
                }
            }

            CloseHandle(hOriginalFile);

            // Set new times
            HANDLE hNewFile;
            QByteArray newFilePathArr = (outputDirectory + "\\" + queueItem->GetOutputName()).toLatin1();
            LPCSTR newFilePath = newFilePathArr.data();
            hNewFile = CreateFileA(newFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

            if (hNewFile == INVALID_HANDLE_VALUE)
            {
                QMessageBox::information(this, "ClipCutter", "Unable to open new file for metadata");
            }
            else
            {
                if(!SetFileTime(hNewFile, &creationTime, &accessedTime, &modifiedTime))
                {
                    QMessageBox::information(this, "ClipCutter", "Unable to set date and time");
                }
            }

            CloseHandle(hNewFile);
        }
    }

    ui->progressBar->setValue(100);

    QMessageBox::information(nullptr, "ClipCutter", "Done!");
    EnableActions();
}

void CClipCutterWindow::MarkAllAsSkipped()
{
    for (auto it = videoList.begin(); it != videoList.end(); ++it)
    {
        QueueItem* item = it->get();
        item->TreeItem->setCheckState(0, Qt::CheckState::Checked);
        item->Skip = true;
    }
}

void CClipCutterWindow::AddKeyword()
{
    QString keyword = ui->keywordEdit->text();

    // Setup tree list item
    QTreeWidgetItem* treeItem = new QTreeWidgetItem(ui->keywordsTree);
    treeItem->setText(1, keyword);
}

void CClipCutterWindow::RemoveKeyword()
{
    QList<QTreeWidgetItem*> treeItemsToRemove = ui->keywordsTree->selectedItems();

    for (auto item : treeItemsToRemove)
    {
        for (auto it = videoList.begin(); it != videoList.end(); ++it)
        {
            QueueItem* video = it->get();

            if (video->keyword == item->text(1))
            {
                video->keyword = "";
                video->TreeItem->setText(1, currentVideo->GetOutputName());
            }
        }

        delete item;
    }
}

void CClipCutterWindow::UseKeyword()
{
    if (currentVideo == nullptr)
    {
        return;
    }

    QList<QTreeWidgetItem*> selectedItems = ui->keywordsTree->selectedItems();

    if (selectedItems.empty())
    {
        return;
    }

    QTreeWidgetItem* keywordItem = selectedItems[0];
    QString keywordText = keywordItem->text(1);

    // If they match then we remove keyword entirely
    if (keywordText == currentVideo->keyword)
    {
        keywordItem->setText(0, "");
        currentVideo->keyword = "";
        currentVideo->TreeItem->setText(1, currentVideo->GetOutputName());
        return;
    }

    keywordItem->setText(0, "Yes");
    currentVideo->keyword = keywordText;
    currentVideo->TreeItem->setText(1, currentVideo->GetOutputName());
}
































