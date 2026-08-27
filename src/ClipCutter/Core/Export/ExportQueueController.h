#ifndef CLIPCUTTER_CORE_EXPORT_EXPORTQUEUECONTROLLER_H
#define CLIPCUTTER_CORE_EXPORT_EXPORTQUEUECONTROLLER_H

#include "Core/Export/ExportJob.h"
#include "Core/Export/ExportResult.h"
#include "Core/Export/FfmpegCommandBuilder.h"
#include "Core/Export/FfmpegProgressParser.h"
#include "Core/Export/MetadataService.h"
#include "Core/Export/ProcessRunner.h"

#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

namespace ClipCutter
{
using ProcessRunnerFactory = std::function<ProcessRunner*(QObject* parent)>;

class ExportQueueController final : public QObject
{
    Q_OBJECT

public:
    explicit ExportQueueController(QObject* parent = nullptr);
    ExportQueueController(FfmpegCommandBuilder commandBuilder, ProcessRunnerFactory processRunnerFactory,
                          std::unique_ptr<MetadataService> metadataService, QObject* parent = nullptr);

    bool SetJobs(QVector<ExportJob> jobs);
    bool Start();
    bool RetryJobs(const QSet<QUuid>& jobIds);
    void CancelActiveJob();
    void CancelPendingJobs();
    void CancelQueue();
    void SetTerminationTimeout(int milliseconds);

    bool IsActive() const noexcept;
    QVector<ExportJob> Jobs() const;
    std::optional<EExportState> StateForJob(const QUuid& jobId) const;
    std::optional<ExportResult> ResultForJob(const QUuid& jobId) const;
    QString LogForJob(const QUuid& jobId) const;

signals:
    void JobStateChanged(const QUuid& jobId, ClipCutter::EExportState state);
    void JobProgressChanged(const QUuid& jobId, double progress, bool determinate);
    void TotalProgressChanged(double progress);
    void JobLogUpdated(const QUuid& jobId, const QString& appendedText);
    void JobCompleted(const ClipCutter::ExportResult& result);
    void QueueCompleted(const ClipCutter::ExportSummary& summary);
    void CancellationRequested();

private:
    struct JobEntry
    {
        ExportJob Job;
        EExportState State = EExportState::Pending;
        std::optional<double> Progress;
        FfmpegProgressParser ProgressParser;
        QByteArray StandardOutput;
        QByteArray StandardError;
        std::optional<ExportResult> Result;
    };

    static constexpr qsizetype KMaximumRetainedLogBytes = 1024 * 1024;
    static constexpr int KDefaultTerminationTimeoutMilliseconds = 3000;

    void StartNextJob();
    void HandleProcessStarted(ProcessRunner* runner);
    void HandleStandardOutput(ProcessRunner* runner, const QByteArray& data);
    void HandleStandardError(ProcessRunner* runner, const QByteArray& data);
    void HandleProcessError(ProcessRunner* runner, QProcess::ProcessError error);
    void HandleProcessFinished(ProcessRunner* runner, int exitCode, QProcess::ExitStatus exitStatus);
    void HandleProgress(const FfmpegProgress& progress);
    void FinishActiveJob(EExportState state, int exitCode, bool processCrashed, const QString& errorMessage);
    void FinaliseSuccessfulJob(int exitCode);
    void CompleteQueueIfIdle();
    void UpdateTotalProgress();
    void CancelPendingEntry(JobEntry& entry);
    bool SetState(JobEntry& entry, EExportState state);
    bool PromoteTemporaryOutput(const ExportJob& job, QString& error) const;
    int IndexForJob(const QUuid& jobId) const;
    double EntryWeight(const JobEntry& entry, double fallbackWeight) const;
    static void AppendRetained(QByteArray& destination, const QByteArray& data);
    static QString ProcessErrorText(QProcess::ProcessError error);

    QVector<JobEntry> Entries_;
    QQueue<int> PendingIndexes_;
    FfmpegCommandBuilder CommandBuilder_;
    ProcessRunnerFactory ProcessRunnerFactory_;
    std::unique_ptr<MetadataService> MetadataService_;
    QTimer TerminationTimer_;
    ProcessRunner* ActiveRunner_ = nullptr;
    int ActiveIndex_ = -1;
    bool QueueActive_ = false;
    bool QueueCancellationRequested_ = false;
};
} // namespace ClipCutter

Q_DECLARE_METATYPE(ClipCutter::EExportState)
Q_DECLARE_METATYPE(ClipCutter::ExportResult)
Q_DECLARE_METATYPE(ClipCutter::ExportSummary)

#endif
