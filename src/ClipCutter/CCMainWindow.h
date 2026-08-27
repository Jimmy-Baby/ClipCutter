#ifndef CCMAINWINDOW_H
#define CCMAINWINDOW_H

#include "app/models/ClipQueueModel.h"
#include "core/import/ClipImporter.h"

#include <QDir>
#include <QIcon>
#include <QMainWindow>
#include <QUuid>

class QAudioOutput;
class QMediaPlayer;
class QModelIndex;
class QTreeWidgetItem;
class QVideoWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class ClipCutterWindow; }
QT_END_NAMESPACE

class CClipCutterWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CClipCutterWindow(QWidget* parent = nullptr);
    ~CClipCutterWindow() override;

private:
    void ActionOpenFolderTriggered();
    void ActionOpenFilesTriggered();
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
    void OnClipSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    void OnVideoNameChanged(const QString& newName);
    void OnKeywordChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void UpdateStartEndUI();
    void UpdateKeywordUI();
    void UpdateCurrentVideoName();
    void UpdateActionStates();
    void ClearCurrentClipUI();
    void LoadImportedClips(clipcutter::ImportResult result, const QDir& directory,
                           const QString& preparedOutputDirectory);
    void PresentImportMessages(const clipcutter::ImportResult& result);
    void OpenVideo(int row);
    void DisableActions();
    void EnableActions();
    void ProcessClips();
    void MarkAllAsSkipped();
    void AddKeyword();
    void RemoveKeyword();
    void UseKeyword();
    clipcutter::Clip* currentClip();
    const clipcutter::Clip* currentClip() const;
    clipcutter::Segment* currentSegment();
    const clipcutter::Segment* currentSegment() const;

    Ui::ClipCutterWindow* ui;
    QMediaPlayer* player;
    QVideoWidget* videoWidget;
    QAudioOutput* audioOutput;
    QIcon playIcon;
    QIcon pauseIcon;
    clipcutter::ClipQueueModel* queueModel;
    clipcutter::ClipImporter clipImporter;
    QUuid currentClipId;
    QUuid currentSegmentId;
    QDir videoDirectory;
    QString outputDirectory;

    struct UserSettings
    {
        bool copyDateTime = true;
    } userSettings;
};

#endif
