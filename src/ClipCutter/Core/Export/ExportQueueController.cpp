#include "Core/Export/ExportQueueController.h"

#include "Core/Export/OutputPathPlanner.h"
#include "Core/Export/OutputProfile.h"

#include <QFile>

#include <algorithm>

namespace ClipCutter
{
ExportQueueController::ExportQueueController(QObject* parent)
    : ExportQueueController(FfmpegCommandBuilder{}, {}, {}, {}, parent)
{
}

ExportQueueController::ExportQueueController(FfmpegCommandBuilder commandBuilder,
                                             ProcessRunnerFactory processRunnerFactory,
                                             std::unique_ptr<MetadataService> metadataService,
                                             std::unique_ptr<OutputVerifier> outputVerifier, QObject* parent)
    : QObject(parent), CommandBuilder_(std::move(commandBuilder)),
      ProcessRunnerFactory_(std::move(processRunnerFactory)), MetadataService_(std::move(metadataService)),
      OutputVerifier_(std::move(outputVerifier))
{
    if (!ProcessRunnerFactory_)
    {
        ProcessRunnerFactory_ = [](QObject* runnerParent) { return new QProcessRunner(runnerParent); };
    }

    if (!MetadataService_)
    {
        MetadataService_ = std::make_unique<PlatformMetadataService>();
    }
    if (!OutputVerifier_)
    {
        OutputVerifier_ = std::make_unique<FfprobeOutputVerifier>(this);
    }

    TerminationTimer_.setSingleShot(true);
    TerminationTimer_.setInterval(KDefaultTerminationTimeoutMilliseconds);
    connect(&TerminationTimer_, &QTimer::timeout, this,
            [this]()
            {
                if (ActiveRunner_ != nullptr && ActiveIndex_ >= 0 &&
                    Entries_.at(ActiveIndex_).State == EExportState::Cancelling)
                {
                    ActiveRunner_->Kill();
                }
            });
}

bool ExportQueueController::SetJobs(QVector<ExportJob> jobs)
{
    if (QueueActive_)
    {
        return false;
    }

    Entries_.clear();
    PendingIndexes_.clear();
    ActiveIndex_ = -1;
    QueueCancellationRequested_ = false;

    Entries_.reserve(jobs.size());

    for (ExportJob& job : jobs)
    {
        JobEntry entry;
        entry.Job = std::move(job);
        entry.State = entry.Job.SkipRequested ? EExportState::Skipped : EExportState::Pending;
        entry.Progress = entry.Job.SkipRequested ? std::optional<double>{1.0} : std::nullopt;
        Entries_.append(std::move(entry));
    }

    UpdateTotalProgress();

    return true;
}

bool ExportQueueController::Start()
{
    if (QueueActive_)
    {
        return false;
    }

    PendingIndexes_.clear();

    for (int index = 0; index < Entries_.size(); ++index)
    {
        if (Entries_.at(index).State == EExportState::Pending)
        {
            PendingIndexes_.enqueue(index);
        }
        else if (Entries_.at(index).State == EExportState::Skipped)
        {
            emit JobStateChanged(Entries_.at(index).Job.JobId, EExportState::Skipped);
            emit JobProgressChanged(Entries_.at(index).Job.JobId, 1.0, true);
        }
    }

    QueueActive_ = true;
    QueueCancellationRequested_ = false;
    UpdateTotalProgress();
    QTimer::singleShot(0, this, &ExportQueueController::StartNextJob);

    return true;
}

bool ExportQueueController::RetryJobs(const QSet<QUuid>& jobIds)
{
    if (QueueActive_ || jobIds.isEmpty())
    {
        return false;
    }

    PendingIndexes_.clear();

    for (int index = 0; index < Entries_.size(); ++index)
    {
        JobEntry& entry = Entries_[index];

        if (!jobIds.contains(entry.Job.JobId) ||
            (entry.State != EExportState::Failed && entry.State != EExportState::Cancelled))
        {
            continue;
        }

        SetState(entry, EExportState::Pending);
        entry.Progress.reset();
        entry.ProgressParser.Reset();
        entry.StandardOutput.clear();
        entry.StandardError.clear();
        entry.Result.reset();
        entry.MetadataWarning.clear();
        entry.VerificationDiagnostics.clear();
        PendingIndexes_.enqueue(index);
        emit JobProgressChanged(entry.Job.JobId, 0.0, entry.Job.Duration.has_value());
    }

    if (PendingIndexes_.isEmpty())
    {
        return false;
    }

    QueueActive_ = true;
    QueueCancellationRequested_ = false;
    UpdateTotalProgress();
    QTimer::singleShot(0, this, &ExportQueueController::StartNextJob);

    return true;
}

void ExportQueueController::CancelActiveJob()
{
    if (!QueueActive_ || ActiveRunner_ == nullptr || ActiveIndex_ < 0)
    {
        return;
    }

    JobEntry& entry = Entries_[ActiveIndex_];

    if (entry.State == EExportState::Cancelling || IsTerminalState(entry.State))
    {
        return;
    }

    if (!SetState(entry, EExportState::Cancelling))
    {
        return;
    }

    ActiveRunner_->Terminate();
    TerminationTimer_.start();
}

void ExportQueueController::CancelPendingJobs()
{
    if (!QueueActive_)
    {
        return;
    }

    for (JobEntry& entry : Entries_)
    {
        if (entry.State == EExportState::Pending)
        {
            CancelPendingEntry(entry);
        }
    }

    PendingIndexes_.clear();
    UpdateTotalProgress();
    CompleteQueueIfIdle();
}

void ExportQueueController::CancelQueue()
{
    if (!QueueActive_)
    {
        return;
    }

    if (!QueueCancellationRequested_)
    {
        QueueCancellationRequested_ = true;
        emit CancellationRequested();
    }

    CancelPendingJobs();
    CancelActiveJob();
}

void ExportQueueController::SetTerminationTimeout(const int milliseconds)
{
    TerminationTimer_.setInterval(std::max(0, milliseconds));
}

bool ExportQueueController::IsActive() const noexcept
{
    return QueueActive_;
}

QVector<ExportJob> ExportQueueController::Jobs() const
{
    QVector<ExportJob> jobs;
    jobs.reserve(Entries_.size());

    for (const JobEntry& entry : Entries_)
    {
        jobs.append(entry.Job);
    }

    return jobs;
}

std::optional<EExportState> ExportQueueController::StateForJob(const QUuid& jobId) const
{
    const int index = IndexForJob(jobId);

    if (index < 0)
    {
        return std::nullopt;
    }

    return Entries_.at(index).State;
}

std::optional<ExportResult> ExportQueueController::ResultForJob(const QUuid& jobId) const
{
    const int index = IndexForJob(jobId);

    if (index < 0)
    {
        return std::nullopt;
    }

    return Entries_.at(index).Result;
}

QString ExportQueueController::LogForJob(const QUuid& jobId) const
{
    const int index = IndexForJob(jobId);

    if (index < 0)
    {
        return {};
    }

    const JobEntry& entry = Entries_.at(index);
    return QString::fromUtf8(entry.StandardError.isEmpty() ? entry.StandardOutput : entry.StandardError);
}

void ExportQueueController::StartNextJob()
{
    if (!QueueActive_ || ActiveRunner_ != nullptr)
    {
        return;
    }

    while (!PendingIndexes_.isEmpty())
    {
        const int index = PendingIndexes_.dequeue();

        if (index < 0 || index >= Entries_.size() || Entries_.at(index).State != EExportState::Pending)
        {
            continue;
        }

        ActiveIndex_ = index;
        JobEntry& entry = Entries_[index];

        if (!SetState(entry, EExportState::Preparing))
        {
            ActiveIndex_ = -1;

            continue;
        }

        entry.Progress = 0.0;
        entry.ProgressParser.Reset();
        emit JobProgressChanged(entry.Job.JobId, 0.0, entry.Job.Duration.has_value());

        ActiveRunner_ = ProcessRunnerFactory_(this);

        if (ActiveRunner_ == nullptr)
        {
            FinishActiveJob(EExportState::Failed, -1, false,
                            QStringLiteral("The process runner could not be created."));

            return;
        }

        ProcessRunner* runner = ActiveRunner_;
        connect(runner, &ProcessRunner::Started, this, [this, runner]() { HandleProcessStarted(runner); });
        connect(runner, &ProcessRunner::StandardOutputReady, this,
                [this, runner](const QByteArray& data) { HandleStandardOutput(runner, data); });
        connect(runner, &ProcessRunner::StandardErrorReady, this,
                [this, runner](const QByteArray& data) { HandleStandardError(runner, data); });
        connect(runner, &ProcessRunner::ErrorOccurred, this,
                [this, runner](const QProcess::ProcessError error) { HandleProcessError(runner, error); });
        connect(runner, &ProcessRunner::Finished, this,
                [this, runner](const int exitCode, const QProcess::ExitStatus exitStatus)
                { HandleProcessFinished(runner, exitCode, exitStatus); });

        runner->Start(CommandBuilder_.Build(entry.Job));

        return;
    }

    CompleteQueueIfIdle();
}

void ExportQueueController::HandleProcessStarted(ProcessRunner* runner)
{
    if (runner != ActiveRunner_ || ActiveIndex_ < 0)
    {
        return;
    }

    JobEntry& entry = Entries_[ActiveIndex_];

    if (entry.State == EExportState::Cancelling)
    {
        runner->Terminate();

        return;
    }

    SetState(entry, EExportState::Running);
}

void ExportQueueController::HandleStandardOutput(ProcessRunner* runner, const QByteArray& data)
{
    if (runner != ActiveRunner_ || ActiveIndex_ < 0)
    {
        return;
    }

    JobEntry& entry = Entries_[ActiveIndex_];
    AppendRetained(entry.StandardOutput, data);
    const QVector<FfmpegProgress> progressUpdates = entry.ProgressParser.Feed(data);

    for (const FfmpegProgress& progress : progressUpdates)
    {
        HandleProgress(progress);
    }
}

void ExportQueueController::HandleStandardError(ProcessRunner* runner, const QByteArray& data)
{
    if (runner != ActiveRunner_ || ActiveIndex_ < 0)
    {
        return;
    }

    JobEntry& entry = Entries_[ActiveIndex_];
    AppendRetained(entry.StandardError, data);
    emit JobLogUpdated(entry.Job.JobId, QString::fromUtf8(data));
}

void ExportQueueController::HandleProcessError(ProcessRunner* runner, const QProcess::ProcessError error)
{
    if (runner != ActiveRunner_ || ActiveIndex_ < 0)
    {
        return;
    }

    const JobEntry& entry = Entries_.at(ActiveIndex_);

    if (entry.State == EExportState::Cancelling)
    {
        FinishActiveJob(EExportState::Cancelled, -1, error == QProcess::Crashed, QString());

        return;
    }

    FinishActiveJob(EExportState::Failed, -1, error == QProcess::Crashed, ProcessErrorText(error));
}

void ExportQueueController::HandleProcessFinished(ProcessRunner* runner, const int exitCode,
                                                  const QProcess::ExitStatus exitStatus)
{
    if (runner != ActiveRunner_ || ActiveIndex_ < 0)
    {
        return;
    }

    const EExportState state = Entries_.at(ActiveIndex_).State;

    if (state == EExportState::Cancelling)
    {
        FinishActiveJob(EExportState::Cancelled, exitCode, exitStatus == QProcess::CrashExit, QString());

        return;
    }

    if (exitStatus == QProcess::CrashExit)
    {
        FinishActiveJob(EExportState::Failed, exitCode, true, QStringLiteral("FFmpeg crashed."));

        return;
    }

    if (exitCode != 0)
    {
        FinishActiveJob(EExportState::Failed, exitCode, false,
                        QStringLiteral("FFmpeg exited with code %1.").arg(exitCode));

        return;
    }

    FinaliseSuccessfulJob(exitCode);
}

void ExportQueueController::HandleProgress(const FfmpegProgress& progress)
{
    if (ActiveIndex_ < 0)
    {
        return;
    }

    JobEntry& entry = Entries_[ActiveIndex_];
    const bool determinate = entry.Job.Duration.has_value() && entry.Job.Duration->count() > 0;
    double normalizedProgress = entry.Progress.value_or(0.0);

    if (progress.ProgressEnded)
    {
        normalizedProgress = 1.0;
    }
    else if (determinate && progress.OutputTime.has_value())
    {
        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(*entry.Job.Duration);
        normalizedProgress = static_cast<double>(progress.OutputTime->count()) / static_cast<double>(duration.count());
        normalizedProgress = std::clamp(normalizedProgress, 0.0, 1.0);
    }

    entry.Progress = normalizedProgress;
    emit JobProgressChanged(entry.Job.JobId, normalizedProgress, determinate);
    UpdateTotalProgress();
}

void ExportQueueController::FinishActiveJob(const EExportState state, const int exitCode, const bool processCrashed,
                                            const QString& errorMessage)
{
    if (ActiveIndex_ < 0)
    {
        return;
    }

    TerminationTimer_.stop();
    JobEntry& entry = Entries_[ActiveIndex_];

    if (!SetState(entry, state))
    {
        return;
    }

    if (state == EExportState::Cancelled || state == EExportState::Failed)
    {
        QFile::remove(entry.Job.TemporaryOutputPath);
    }

    ExportResult result;
    result.JobId = entry.Job.JobId;
    result.State = state;
    result.ExitCode = exitCode;
    result.ProcessCrashed = processCrashed;
    result.MediaExportSucceeded = state == EExportState::Succeeded;
    result.HasMetadataWarning = !entry.MetadataWarning.isEmpty();
    result.ErrorMessage = errorMessage;
    result.MetadataWarning = entry.MetadataWarning;
    result.VerificationDiagnostics = entry.VerificationDiagnostics;
    result.StandardOutput = QString::fromUtf8(entry.StandardOutput);
    result.StandardError = QString::fromUtf8(entry.StandardError);
    entry.Result = result;

    if (state == EExportState::Succeeded)
    {
        entry.Progress = 1.0;
        emit JobProgressChanged(entry.Job.JobId, 1.0, true);
    }

    emit JobCompleted(result);

    ProcessRunner* completedRunner = ActiveRunner_;
    ActiveRunner_ = nullptr;
    ActiveIndex_ = -1;

    if (completedRunner != nullptr)
    {
        // QProcess may report both error and finished for one exit. Disconnect before advancing the queue.
        completedRunner->disconnect(this);
        completedRunner->deleteLater();
    }

    UpdateTotalProgress();
    QTimer::singleShot(0, this, &ExportQueueController::StartNextJob);
}

void ExportQueueController::FinaliseSuccessfulJob(const int exitCode)
{
    if (ActiveIndex_ < 0)
    {
        return;
    }

    JobEntry& entry = Entries_[ActiveIndex_];

    if (!SetState(entry, EExportState::Finalising))
    {
        return;
    }

    const OutputProfile* profile = OutputProfiles::Find(entry.Job.OutputProfileId);
    if (profile == nullptr)
    {
        FinishActiveJob(EExportState::Failed, exitCode, false, QStringLiteral("Unknown output profile."));
        return;
    }
    const QUuid jobId = entry.Job.JobId;
    OutputVerifier_->Verify(entry.Job, *profile,
                            [this, jobId, exitCode](VerificationResult verification)
                            {
                                if (ActiveIndex_ < 0 || Entries_.at(ActiveIndex_).Job.JobId != jobId) return;
                                JobEntry& active = Entries_[ActiveIndex_];
                                active.VerificationDiagnostics = verification.Diagnostics;
                                if (!verification.Diagnostics.isEmpty())
                                {
                                    AppendRetained(active.StandardError,
                                                   QByteArray("\nOutput verification:\n") + verification.Diagnostics.toUtf8());
                                    emit JobLogUpdated(jobId, QStringLiteral("\nOutput verification:\n") + verification.Diagnostics);
                                }
                                if (!verification.Success)
                                {
                                    FinishActiveJob(EExportState::Failed, exitCode, false, verification.ErrorMessage);
                                    return;
                                }
                                const FileOperationResult promoted = OutputPathPlanner::Finalise(
                                    active.Job.TemporaryOutputPath, active.Job.FinalOutputPath,
                                    active.Job.CollisionPolicy);
                                if (!promoted.Success)
                                {
                                    FinishActiveJob(EExportState::Failed, exitCode, false, promoted.ErrorMessage);
                                    return;
                                }
                                if (active.Job.CopyMetadata)
                                {
                                    const MetadataResult metadata = MetadataService_->CopyFileTimestamps(
                                        active.Job.SourcePath, active.Job.FinalOutputPath);
                                    if (!metadata.Success)
                                    {
                                        active.MetadataWarning = metadata.Message;
                                        emit JobLogUpdated(jobId, QStringLiteral("Metadata warning: %1\n").arg(metadata.Message));
                                    }
                                }
                                FinishActiveJob(EExportState::Succeeded, exitCode, false, QString());
                            });
}

void ExportQueueController::CompleteQueueIfIdle()
{
    if (!QueueActive_ || ActiveRunner_ != nullptr || !PendingIndexes_.isEmpty())
    {
        return;
    }

    QueueActive_ = false;
    QueueCancellationRequested_ = false;
    ExportSummary summary;

    for (const JobEntry& entry : Entries_)
    {
        switch (entry.State)
        {
        case EExportState::Succeeded:
            ++summary.SucceededCount;
            if (!entry.MetadataWarning.isEmpty()) ++summary.WarningCount;
            break;
        case EExportState::Failed:
            ++summary.FailedCount;
            break;
        case EExportState::Skipped:
            ++summary.SkippedCount;
            break;
        case EExportState::Cancelled:
            ++summary.CancelledCount;
            break;
        default:
            break;
        }
    }

    UpdateTotalProgress();
    emit QueueCompleted(summary);
}

void ExportQueueController::UpdateTotalProgress()
{
    if (Entries_.isEmpty())
    {
        emit TotalProgressChanged(1.0);

        return;
    }

    double knownWeightTotal = 0.0;
    int knownWeightCount = 0;

    for (const JobEntry& entry : Entries_)
    {
        if (entry.Job.Duration.has_value() && entry.Job.Duration->count() > 0)
        {
            knownWeightTotal += static_cast<double>(entry.Job.Duration->count());
            ++knownWeightCount;
        }
    }

    // Unknown durations receive the average known duration, or one job unit when every duration is unknown.
    const double fallbackWeight = knownWeightCount > 0 ? knownWeightTotal / static_cast<double>(knownWeightCount) : 1.0;
    double weightedProgress = 0.0;
    double totalWeight = 0.0;

    for (const JobEntry& entry : Entries_)
    {
        const double weight = EntryWeight(entry, fallbackWeight);
        const double progress = IsTerminalState(entry.State) ? 1.0 : entry.Progress.value_or(0.0);
        weightedProgress += weight * std::clamp(progress, 0.0, 1.0);
        totalWeight += weight;
    }

    emit TotalProgressChanged(totalWeight > 0.0 ? weightedProgress / totalWeight : 1.0);
}

void ExportQueueController::CancelPendingEntry(JobEntry& entry)
{
    if (!SetState(entry, EExportState::Cancelled))
    {
        return;
    }

    entry.Progress = 0.0;
    ExportResult result;
    result.JobId = entry.Job.JobId;
    result.State = EExportState::Cancelled;
    entry.Result = result;
    emit JobProgressChanged(entry.Job.JobId, 0.0, entry.Job.Duration.has_value());
    emit JobCompleted(result);
}

bool ExportQueueController::SetState(JobEntry& entry, const EExportState state)
{
    if (entry.State == state || !IsValidTransition(entry.State, state))
    {
        return false;
    }

    entry.State = state;
    emit JobStateChanged(entry.Job.JobId, state);

    return true;
}

int ExportQueueController::IndexForJob(const QUuid& jobId) const
{
    for (int index = 0; index < Entries_.size(); ++index)
    {
        if (Entries_.at(index).Job.JobId == jobId)
        {
            return index;
        }
    }

    return -1;
}

double ExportQueueController::EntryWeight(const JobEntry& entry, const double fallbackWeight) const
{
    if (entry.Job.Duration.has_value() && entry.Job.Duration->count() > 0)
    {
        return static_cast<double>(entry.Job.Duration->count());
    }

    return fallbackWeight;
}

void ExportQueueController::AppendRetained(QByteArray& destination, const QByteArray& data)
{
    destination.append(data);

    if (destination.size() > KMaximumRetainedLogBytes)
    {
        destination.remove(0, destination.size() - KMaximumRetainedLogBytes);
    }
}

QString ExportQueueController::ProcessErrorText(const QProcess::ProcessError error)
{
    switch (error)
    {
    case QProcess::FailedToStart:
        return QStringLiteral("FFmpeg failed to start. Verify that ffmpeg is installed beside ClipCutter.");
    case QProcess::Crashed:
        return QStringLiteral("FFmpeg crashed.");
    case QProcess::Timedout:
        return QStringLiteral("The FFmpeg process operation timed out.");
    case QProcess::WriteError:
        return QStringLiteral("ClipCutter could not write to the FFmpeg process.");
    case QProcess::ReadError:
        return QStringLiteral("ClipCutter could not read from the FFmpeg process.");
    case QProcess::UnknownError:
        return QStringLiteral("An unknown FFmpeg process error occurred.");
    }

    return QStringLiteral("An FFmpeg process error occurred.");
}
} // namespace ClipCutter
