#ifndef CLIPCUTTER_MAINWINDOW_H
#define CLIPCUTTER_MAINWINDOW_H

#include "App/Models/ClipQueueModel.h"
#include "Core/Export/ExportQueueController.h"
#include "Core/Import/ClipImporter.h"

#include <QDir>
#include <QHash>
#include <QIcon>
#include <QMainWindow>
#include <QUuid>

class QAudioOutput;
class QMediaPlayer;
class QModelIndex;
class QTreeWidgetItem;
class QVideoWidget;

QT_BEGIN_NAMESPACE
namespace Ui
{
class ClipCutterWindow;
}
QT_END_NAMESPACE

namespace ClipCutter
{
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

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
    void OnJobStateChanged(const QUuid& jobId, EExportState state);
    void OnJobProgressChanged(const QUuid& jobId, double progress, bool determinate);
    void OnJobLogUpdated(const QUuid& jobId, const QString& text);
    void OnQueueCompleted(const ExportSummary& summary);
    void UpdateStartEndUi();
    void UpdateKeywordUi();
    void UpdateCurrentVideoName();
    void UpdateActionStates();
    void ClearCurrentClipUi();
    void LoadImportedClips(ImportResult result, const QDir& directory, const QString& preparedOutputDirectory);
    void PresentImportMessages(const ImportResult& result);
    void OpenVideo(int row);
    void ProcessClips();
    void CancelExport();
    void RetryFailedJobs();
    void ShowSelectedExportLog();
    void MarkAllAsSkipped();
    void AddKeyword();
    void RemoveKeyword();
    void UseKeyword();
    QVector<ExportJob> BuildExportJobs(const QVector<ExportSegment>& segments) const;
    EEncodingQuality SelectedEncodingQuality() const;
    QUuid SegmentIdForJob(const QUuid& jobId) const;
    Clip* CurrentClip();
    const Clip* CurrentClip() const;
    Segment* CurrentSegment();
    const Segment* CurrentSegment() const;

    Ui::ClipCutterWindow* Ui_;
    QMediaPlayer* Player_;
    QVideoWidget* VideoWidget_;
    QAudioOutput* AudioOutput_;
    QIcon PlayIcon_;
    QIcon PauseIcon_;
    ClipQueueModel* QueueModel_;
    ClipImporter ClipImporter_;
    ExportQueueController* ExportController_;
    QHash<QUuid, QUuid> JobToSegmentId_;
    QUuid CurrentClipId_;
    QUuid CurrentSegmentId_;
    QDir VideoDirectory_;
    QString OutputDirectory_;

    struct UserSettings
    {
        bool CopyDateTime = true;
    } UserSettings_;
};
} // namespace ClipCutter

#endif
