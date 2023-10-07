#include <QMessageBox>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QFileDialog>
#include <QSlider>
#include <QRandomGenerator>
#include <QTreeWidgetItem>

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
    connect(ui->actionOpen, &QAction::triggered, this, &CClipCutterWindow::ActionOpenTriggered);
    connect(ui->actionPlayPause, &QAction::triggered, this, &CClipCutterWindow::ActionPlayPauseTriggered);
    connect(ui->actionNext, &QAction::triggered, this, &CClipCutterWindow::ActionNextTriggered);
    connect(ui->actionSkip, &QAction::triggered, this, &CClipCutterWindow::ActionSkipTriggered);
    connect(ui->actionPrev, &QAction::triggered, this, &CClipCutterWindow::ActionPrevTriggered);
    connect(ui->actionStop, &QAction::triggered, this, &CClipCutterWindow::ActionStopTriggered);
    connect(ui->actionSetStart, &QAction::triggered, this, &CClipCutterWindow::ActionSetStartTriggered);
    connect(ui->actionSetEnd, &QAction::triggered, this, &CClipCutterWindow::ActionSetEndTriggered);

    connect(ui->clipsTree, &QTreeWidget::currentItemChanged, this, &CClipCutterWindow::OnVideoListItemChanged);
    connect(ui->clipNameEdit, &QLineEdit::textChanged, this, &CClipCutterWindow::OnVideoNameChanged);
    connect(ui->processButton, &QPushButton::pressed, this, &CClipCutterWindow::ProcessClips);

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
}


CClipCutterWindow::~CClipCutterWindow()
{
    // Clear queue memory
    for (const QueueItem* ptr : videoList)
    {
        delete ptr;
    }

    delete ui;
}


void CClipCutterWindow::ActionOpenTriggered()
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
    for (const QueueItem* ptr : videoList)
    {
        delete ptr;
    }

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
        videoList.push_back(newItem);

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
    currentVideo->TreeItem->setText(1, newNameWithExtension);
}


int CClipCutterWindow::GetVideoIndexFromTreeItem(QTreeWidgetItem* treeItem)
{
    for (QueueItem* item : videoList)
    {
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
    currentVideo = videoList[videoIndex];

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
    ui->actionOpen->setDisabled(true);
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
    ui->actionOpen->setDisabled(false);
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

    DisableActions();

    for (int index = 0; index < videoList.length(); ++index)
    {
        const QueueItem* queueItem = videoList[index];

        // Set progress bar
        const int progress = static_cast<float>(index + 1) * (100.0f / static_cast<float>(videoList.length()));
        ui->progressBar->setValue(progress);

        if (queueItem->Skip)
        {
            continue;
        }

        EReEncodeQuality quality = static_cast<EReEncodeQuality>(ui->qualityCombo->currentIndex());
        FFmpeg::ProcessQueueItem(queueItem, outputDirectory, quality);
    }

    EnableActions();
}
