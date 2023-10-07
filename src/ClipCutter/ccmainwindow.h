#ifndef CCMAINWINDOW_H
#define CCMAINWINDOW_H

#include <QMainWindow>
#include <QDir>

#include "queueitem.h"

class QTreeWidgetItem;
class QMediaPlayer;
class QVideoWidget;
class QAudioOutput;
class QSlider;

QT_BEGIN_NAMESPACE
namespace Ui { class ClipCutterWindow; }
QT_END_NAMESPACE

class CClipCutterWindow : public QMainWindow
{
    Q_OBJECT

public:
    CClipCutterWindow(QWidget *parent = nullptr);
    ~CClipCutterWindow();

private:
    void ActionOpenTriggered();
    void ActionPlayPauseTriggered();
    void ActionNextTriggered();
    void ActionSkipTriggered();
    void ActionPrevTriggered();
    void ActionStopTriggered();
    void ActionSetStartTriggered();
    void ActionSetEndTriggered();

    void OnVolumeChanged(int volume);
    void OnPlayerDurationChanged(qint64 duration);
    void OnPlayerPositionChanged(qint64 position);
    void OnVideoListItemChanged(QTreeWidgetItem* curr, QTreeWidgetItem* prev);
    void OnVideoNameChanged(QString newName);

    int GetVideoIndexFromTreeItem(QTreeWidgetItem* treeItem);
    void UpdateStartEndUI();
    void OpenVideo(qint64 videoIndex);
    void DisableActions();
    void EnableActions();
    void ProcessClips();

    // UI
    Ui::ClipCutterWindow *ui;
    QMediaPlayer* player;
    QVideoWidget* videoWidget;
    QAudioOutput* audioOutput;
    QIcon* playIcon;
    QIcon* pauseIcon;

    // Videos
    QueueItem* currentVideo;
    QVector<QueueItem*> videoList;
    QDir videoDirectory;
    QString outputDirectory;
};

#endif
