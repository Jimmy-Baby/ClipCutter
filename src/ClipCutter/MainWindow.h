#ifndef CLIPCUTTER_MAINWINDOW_H
#define CLIPCUTTER_MAINWINDOW_H

#include "App/Models/ClipQueueModel.h"
#include "App/Models/ClipQueueFilterModel.h"
#include "App/Settings/SettingsRepository.h"
#include "Core/Export/ExportQueueController.h"
#include "Core/Import/ClipImporter.h"
#include "Core/Media/MediaProbe.h"
#include "Core/Diagnostics/StartupDiagnostics.h"
#include "Core/Export/OutputPathPlanner.h"
#include "Core/Export/OutputDestination.h"
#include "Core/Session/SessionRepository.h"

#include <QDir>
#include <QHash>
#include <QIcon>
#include <QMainWindow>
#include <QUuid>

class QAudioOutput;
class QCloseEvent;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QLineEdit;
class QMediaPlayer;
class QModelIndex;
class QPushButton;
class QTreeWidgetItem;
class QVideoWidget;
class QCheckBox;
class QSplitter;
class QTimer;

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

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void ActionOpenFolderTriggered();
    void ActionOpenFilesTriggered();
    void NewSession();
    void OpenSession();
    bool SaveSession();
    bool SaveSessionAs();
    void ResetSettings();
    void RelinkMissingSources();
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
    void LoadImportedClips(ImportResult result, const QDir& directory = {}, const QString& preparedOutputDirectory = {});
    void ImportPaths(const QStringList& paths);
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
    void SetupWorkflowUi();
    void LoadApplicationSettings();
    void SaveApplicationSettings();
    void UpdateOutputPreview();
    void UpdateFilterStatus();
    void MarkSessionDirty();
    void AutosaveSession();
    bool MaybeSaveChanges();
    bool LoadSessionFile(const QString& path, bool recovery = false);
    SessionData CurrentSessionData() const;
    QSet<QUuid> SelectedSegmentIds() const;
    void ApplyNamingTemplateTo(const QSet<QUuid>& ids);
    QVector<ExportJob> BuildExportJobs(const QVector<ExportSegment>& segments,
                                       const QVector<PlannedOutput>& outputs, ECollisionPolicy policy) const;
    QString SelectedProfileId() const;
    ECollisionPolicy SelectedCollisionPolicy() const;
    void OnProbeCompleted(const MediaProbeResult& result);
    void OnDiagnosticsCompleted(const StartupDiagnosticsResult& result);
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
    ClipQueueFilterModel* QueueProxy_;
    ClipImporter ClipImporter_;
    ExportQueueController* ExportController_;
    MediaProbe* MediaProbe_;
    StartupDiagnostics* StartupDiagnostics_;
    QHash<QUuid, QUuid> JobToSegmentId_;
    QUuid CurrentClipId_;
    QUuid CurrentSegmentId_;
    QDir VideoDirectory_;
    SettingsRepository SettingsRepository_;
    ApplicationSettings ApplicationSettings_;
    SessionDirtyState SessionState_;
    QTimer* AutosaveTimer_;
    QSplitter* MainSplitter_ = nullptr;
    QComboBox* DestinationModeCombo_ = nullptr;
    QLineEdit* DestinationEdit_ = nullptr;
    QPushButton* DestinationBrowseButton_ = nullptr;
    QLabel* OutputPreviewLabel_ = nullptr;
    QLineEdit* NamingTemplateEdit_ = nullptr;
    QLabel* NamingPreviewLabel_ = nullptr;
    QLineEdit* QueueFilterEdit_ = nullptr;
    QLabel* FilterStatusLabel_ = nullptr;
    QCheckBox* RecursiveImportCheckBox_ = nullptr;
    bool LoadingSession_ = false;
    bool DiagnosticsComplete_ = false;

    struct UserSettings
    {
        bool CopyDateTime = true;
    } UserSettings_;
};
} // namespace ClipCutter

#endif
