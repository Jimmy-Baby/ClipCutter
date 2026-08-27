#include "Core/Export/ExportQueueController.h"
#include "Core/Export/FfmpegCommandBuilder.h"
#include "Core/Export/FfmpegProgressParser.h"

#include <QFile>
#include <QPointer>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <chrono>
#include <memory>

using namespace std::chrono_literals;

namespace
{
struct RunnerRecord
{
    ClipCutter::FfmpegCommand Command;
    int StartCount = 0;
    int TerminateCount = 0;
    int KillCount = 0;
};

class FakeProcessRunner final : public ClipCutter::ProcessRunner
{
public:
    explicit FakeProcessRunner(std::shared_ptr<RunnerRecord> record, QObject* parent = nullptr)
        : ProcessRunner(parent), Record_(std::move(record))
    {
    }

    void Start(const ClipCutter::FfmpegCommand& command) override
    {
        Record_->Command = command;
        ++Record_->StartCount;
    }

    void Terminate() override
    {
        ++Record_->TerminateCount;
    }

    void Kill() override
    {
        ++Record_->KillCount;
    }

    void EmitStarted()
    {
        emit Started();
    }

    void EmitStandardOutput(const QByteArray& data)
    {
        emit StandardOutputReady(data);
    }

    void EmitStandardError(const QByteArray& data)
    {
        emit StandardErrorReady(data);
    }

    void EmitError(const QProcess::ProcessError error)
    {
        emit ErrorOccurred(error);
    }

    void EmitFinished(const int exitCode, const QProcess::ExitStatus exitStatus = QProcess::NormalExit)
    {
        emit Finished(exitCode, exitStatus);
    }

private:
    std::shared_ptr<RunnerRecord> Record_;
};

class SuccessfulMetadataService final : public ClipCutter::MetadataService
{
public:
    bool CopyFileTimestamps(const QString&, const QString&, QString& error) override
    {
        error.clear();

        return true;
    }
};

struct RunnerHarness
{
    QVector<QPointer<FakeProcessRunner>> Runners;
    QVector<std::shared_ptr<RunnerRecord>> Records;

    ClipCutter::ProcessRunner* Create(QObject* parent)
    {
        auto record = std::make_shared<RunnerRecord>();
        auto* runner = new FakeProcessRunner(record, parent);
        Records.append(record);
        Runners.append(runner);

        return runner;
    }
};

ClipCutter::ExportJob MakeJob(const QTemporaryDir& directory, const QString& name,
                              const std::optional<std::chrono::milliseconds> duration = 1s)
{
    ClipCutter::ExportJob job;
    job.ClipId = QUuid::createUuid();
    job.SegmentId = QUuid::createUuid();
    job.SourcePath = QDir(directory.path()).absoluteFilePath(QStringLiteral("source %1.mp4").arg(name));
    job.FinalOutputPath = QDir(directory.path()).absoluteFilePath(name + QStringLiteral(".mp4"));
    job.TemporaryOutputPath = QDir(directory.path()).absoluteFilePath(name + QStringLiteral(".part.mp4"));
    job.StartTime = 250ms;
    job.Duration = duration;
    job.CopyMetadata = false;
    job.DisplayName = name;

    return job;
}

void CreateTemporaryOutput(const ClipCutter::ExportJob& job)
{
    QFile output(job.TemporaryOutputPath);
    QVERIFY(output.open(QIODevice::WriteOnly));
    QCOMPARE(output.write("encoded"), qint64{7});
    output.close();
}

std::unique_ptr<ClipCutter::ExportQueueController> MakeController(RunnerHarness& harness)
{
    return std::make_unique<ClipCutter::ExportQueueController>(
        ClipCutter::FfmpegCommandBuilder(QStringLiteral("test-ffmpeg")),
        [&harness](QObject* parent) { return harness.Create(parent); }, std::make_unique<SuccessfulMetadataService>());
}

double LastProgress(const QSignalSpy& spy)
{
    return spy.constLast().at(0).toDouble();
}
} // namespace

class ExportEngineTests : public QObject
{
    Q_OBJECT

private slots:
    void CommandArgumentGeneration();
    void PathsWithSpacesAndUnicodeRemainSingleArguments();
    void PartialProgressLines();
    void MalformedProgressValues();
    void StateTransitionsRejectInvalidChanges();
    void SuccessfulStateSequence();
    void FailureToStart();
    void NonzeroExitCode();
    void ProcessCrash();
    void CancellationBeforeStart();
    void CancellationWhileRunning();
    void ForcedKillAfterTimeout();
    void QueueContinuesAfterFailure();
    void RetryFailedAndCancelledJob();
    void EmptyQueue();
    void QueueContainingOnlySkippedJobs();
    void DuplicateFinishedAndErrorSignals();
    void TotalProgressUsesDurationWeights();
    void OptionalFfmpegIntegration();
};

void ExportEngineTests::CommandArgumentGeneration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("output"), 2500ms);
    job.SourcePath = QStringLiteral("C:/Media/source.mp4");
    job.TemporaryOutputPath = QStringLiteral("C:/Output/output.part.mp4");
    job.StartTime = 1250ms;
    job.EncodingQuality = ClipCutter::EEncodingQuality::Medium;

    const ClipCutter::FfmpegCommand command =
        ClipCutter::FfmpegCommandBuilder(QStringLiteral("C:/Tools/ffmpeg.exe")).Build(job);
    QCOMPARE(command.Program, QStringLiteral("C:/Tools/ffmpeg.exe"));
    QCOMPARE(command.Arguments, QStringList({QStringLiteral("-nostdin"),
                                             QStringLiteral("-hide_banner"),
                                             QStringLiteral("-loglevel"),
                                             QStringLiteral("error"),
                                             QStringLiteral("-progress"),
                                             QStringLiteral("pipe:1"),
                                             QStringLiteral("-y"),
                                             QStringLiteral("-i"),
                                             QStringLiteral("C:/Media/source.mp4"),
                                             QStringLiteral("-ss"),
                                             QStringLiteral("1250ms"),
                                             QStringLiteral("-t"),
                                             QStringLiteral("2500ms"),
                                             QStringLiteral("-c:v"),
                                             QStringLiteral("libx264"),
                                             QStringLiteral("-crf"),
                                             QStringLiteral("25"),
                                             QStringLiteral("-preset"),
                                             QStringLiteral("fast"),
                                             QStringLiteral("-c:a"),
                                             QStringLiteral("copy"),
                                             QStringLiteral("C:/Output/output.part.mp4")}));
}

void ExportEngineTests::PathsWithSpacesAndUnicodeRemainSingleArguments()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("完成 output"));
    job.SourcePath = QStringLiteral("C:/Clips/日本語 source clip.mp4");
    job.TemporaryOutputPath = QStringLiteral("C:/Exports/完成 output.part.mp4");

    const ClipCutter::FfmpegCommand command = ClipCutter::FfmpegCommandBuilder(QStringLiteral("ffmpeg")).Build(job);
    const int inputIndex = command.Arguments.indexOf(QStringLiteral("-i"));
    QCOMPARE(command.Arguments.at(inputIndex + 1), job.SourcePath);
    QCOMPARE(command.Arguments.constLast(), job.TemporaryOutputPath);
    QVERIFY(!command.Arguments.at(inputIndex + 1).startsWith(QLatin1Char('"')));
    QVERIFY(!command.Arguments.constLast().startsWith(QLatin1Char('"')));
}

void ExportEngineTests::PartialProgressLines()
{
    ClipCutter::FfmpegProgressParser parser;
    QVERIFY(parser.Feed("out_time_us=500").isEmpty());
    QVERIFY(parser.Feed("000\nprogr").isEmpty());
    const QVector<ClipCutter::FfmpegProgress> updates = parser.Feed("ess=continue\n");
    QCOMPARE(updates.size(), 1);
    QVERIFY(updates.constFirst().OutputTime.has_value());
    QCOMPARE(*updates.constFirst().OutputTime, 500ms);
    QVERIFY(!updates.constFirst().ProgressEnded);

    const QVector<ClipCutter::FfmpegProgress> clockUpdates = parser.Feed("out_time=01:02:03.250000\nprogress=end\n");
    QCOMPARE(clockUpdates.size(), 1);
    QCOMPARE(*clockUpdates.constFirst().OutputTime, std::chrono::microseconds{3'723'250'000});
    QVERIFY(clockUpdates.constFirst().ProgressEnded);
}

void ExportEngineTests::MalformedProgressValues()
{
    ClipCutter::FfmpegProgressParser parser;
    const QVector<ClipCutter::FfmpegProgress> updates =
        parser.Feed("missing-separator\nout_time_us=invalid\nout_time=-1:70:90\nprogress=continue\n");
    QCOMPARE(updates.size(), 1);
    QVERIFY(!updates.constFirst().OutputTime.has_value());
    QVERIFY(!updates.constFirst().ProgressEnded);

    parser.Feed("out_time_us=250000\nprogress=continue\n");
    const QVector<ClipCutter::FfmpegProgress> resetUpdates =
        parser.Feed("out_time_us=invalid\nprogress=continue\n");
    QCOMPARE(resetUpdates.size(), 1);
    QVERIFY(!resetUpdates.constFirst().OutputTime.has_value());
}

void ExportEngineTests::StateTransitionsRejectInvalidChanges()
{
    QVERIFY(ClipCutter::IsValidTransition(ClipCutter::EExportState::Pending, ClipCutter::EExportState::Preparing));
    QVERIFY(!ClipCutter::IsValidTransition(ClipCutter::EExportState::Pending, ClipCutter::EExportState::Succeeded));
    QVERIFY(!ClipCutter::IsValidTransition(ClipCutter::EExportState::Succeeded, ClipCutter::EExportState::Running));
    QVERIFY(ClipCutter::IsTerminalState(ClipCutter::EExportState::Cancelled));
}

void ExportEngineTests::SuccessfulStateSequence()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RunnerHarness harness;
    auto controller = MakeController(harness);
    const ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("success"));
    QVector<ClipCutter::EExportState> states;
    connect(controller.get(), &ClipCutter::ExportQueueController::JobStateChanged, controller.get(),
            [&states](const QUuid&, const ClipCutter::EExportState state) { states.append(state); });
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    QVERIFY(controller->SetJobs({job}));
    QVERIFY(controller->Start());
    QTRY_COMPARE(harness.Runners.size(), 1);
    harness.Runners.at(0)->EmitStarted();
    harness.Runners.at(0)->EmitStandardOutput("out_time_us=1000000\nprogress=end\n");
    CreateTemporaryOutput(job);
    harness.Runners.at(0)->EmitFinished(0);
    QTRY_COMPARE(completionSpy.count(), 1);
    QCOMPARE(states, QVector<ClipCutter::EExportState>(
                         {ClipCutter::EExportState::Preparing, ClipCutter::EExportState::Running,
                          ClipCutter::EExportState::Finalising, ClipCutter::EExportState::Succeeded}));
    QVERIFY(QFile::exists(job.FinalOutputPath));
    QVERIFY(!QFile::exists(job.TemporaryOutputPath));
}

void ExportEngineTests::FailureToStart()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    const ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("launch-failure"));
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({job});
    controller->Start();
    QTRY_COMPARE(harness.Runners.size(), 1);
    harness.Runners.at(0)->EmitError(QProcess::FailedToStart);
    QTRY_COMPARE(completionSpy.count(), 1);
    QCOMPARE(*controller->StateForJob(job.JobId), ClipCutter::EExportState::Failed);
    QVERIFY(controller->ResultForJob(job.JobId)->ErrorMessage.contains(QStringLiteral("failed to start")));
}

void ExportEngineTests::NonzeroExitCode()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    const ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("nonzero"));
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({job});
    controller->Start();
    QTRY_COMPARE(harness.Runners.size(), 1);
    harness.Runners.at(0)->EmitStarted();
    harness.Runners.at(0)->EmitStandardError("invalid input\n");
    harness.Runners.at(0)->EmitFinished(7);
    QTRY_COMPARE(completionSpy.count(), 1);
    const auto result = controller->ResultForJob(job.JobId);
    QCOMPARE(result->State, ClipCutter::EExportState::Failed);
    QCOMPARE(result->ExitCode, 7);
    QCOMPARE(result->StandardError, QStringLiteral("invalid input\n"));
}

void ExportEngineTests::ProcessCrash()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    const ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("crash"));
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({job});
    controller->Start();
    QTRY_COMPARE(harness.Runners.size(), 1);
    harness.Runners.at(0)->EmitStarted();
    harness.Runners.at(0)->EmitFinished(-1, QProcess::CrashExit);
    QTRY_COMPARE(completionSpy.count(), 1);
    QVERIFY(controller->ResultForJob(job.JobId)->ProcessCrashed);
    QCOMPARE(*controller->StateForJob(job.JobId), ClipCutter::EExportState::Failed);
}

void ExportEngineTests::CancellationBeforeStart()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    const ClipCutter::ExportJob first = MakeJob(directory, QStringLiteral("first"));
    const ClipCutter::ExportJob second = MakeJob(directory, QStringLiteral("second"));
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({first, second});
    controller->Start();
    controller->CancelQueue();
    QTRY_COMPARE(completionSpy.count(), 1);
    QCOMPARE(harness.Runners.size(), 0);
    QCOMPARE(*controller->StateForJob(first.JobId), ClipCutter::EExportState::Cancelled);
    QCOMPARE(*controller->StateForJob(second.JobId), ClipCutter::EExportState::Cancelled);
}

void ExportEngineTests::CancellationWhileRunning()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    const ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("cancel-active"));
    const ClipCutter::ExportJob laterJob = MakeJob(directory, QStringLiteral("after-cancel"));
    QFile partial(job.TemporaryOutputPath);
    QVERIFY(partial.open(QIODevice::WriteOnly));
    partial.close();
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({job, laterJob});
    controller->Start();
    QTRY_COMPARE(harness.Runners.size(), 1);
    harness.Runners.at(0)->EmitStarted();
    controller->CancelActiveJob();
    QCOMPARE(*controller->StateForJob(job.JobId), ClipCutter::EExportState::Cancelling);
    QCOMPARE(harness.Records.at(0)->TerminateCount, 1);
    controller->CancelActiveJob();
    QCOMPARE(harness.Records.at(0)->TerminateCount, 1);
    harness.Runners.at(0)->EmitFinished(-15, QProcess::CrashExit);
    QTRY_COMPARE(harness.Runners.size(), 2);
    harness.Runners.at(1)->EmitStarted();
    CreateTemporaryOutput(laterJob);
    harness.Runners.at(1)->EmitFinished(0);
    QTRY_COMPARE(completionSpy.count(), 1);
    QCOMPARE(*controller->StateForJob(job.JobId), ClipCutter::EExportState::Cancelled);
    QCOMPARE(*controller->StateForJob(laterJob.JobId), ClipCutter::EExportState::Succeeded);
    QVERIFY(!QFile::exists(job.TemporaryOutputPath));
}

void ExportEngineTests::ForcedKillAfterTimeout()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    controller->SetTerminationTimeout(0);
    const ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("forced-kill"));
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({job});
    controller->Start();
    QTRY_COMPARE(harness.Runners.size(), 1);
    harness.Runners.at(0)->EmitStarted();
    controller->CancelQueue();
    QTRY_COMPARE(harness.Records.at(0)->KillCount, 1);
    harness.Runners.at(0)->EmitFinished(-1, QProcess::CrashExit);
    QTRY_COMPARE(completionSpy.count(), 1);
    QCOMPARE(*controller->StateForJob(job.JobId), ClipCutter::EExportState::Cancelled);
}

void ExportEngineTests::QueueContinuesAfterFailure()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    const ClipCutter::ExportJob failedJob = MakeJob(directory, QStringLiteral("failed"));
    const ClipCutter::ExportJob successfulJob = MakeJob(directory, QStringLiteral("later-success"));
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({failedJob, successfulJob});
    controller->Start();
    QTRY_COMPARE(harness.Runners.size(), 1);
    harness.Runners.at(0)->EmitStarted();
    harness.Runners.at(0)->EmitFinished(1);
    QTRY_COMPARE(harness.Runners.size(), 2);
    harness.Runners.at(1)->EmitStarted();
    CreateTemporaryOutput(successfulJob);
    harness.Runners.at(1)->EmitFinished(0);
    QTRY_COMPARE(completionSpy.count(), 1);
    QCOMPARE(*controller->StateForJob(failedJob.JobId), ClipCutter::EExportState::Failed);
    QCOMPARE(*controller->StateForJob(successfulJob.JobId), ClipCutter::EExportState::Succeeded);
}

void ExportEngineTests::RetryFailedAndCancelledJob()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    const ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("retry"));
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({job});
    controller->Start();
    QTRY_COMPARE(harness.Runners.size(), 1);
    harness.Runners.at(0)->EmitStarted();
    harness.Runners.at(0)->EmitFinished(2);
    QTRY_COMPARE(completionSpy.count(), 1);
    QVERIFY(controller->RetryJobs({job.JobId}));
    QTRY_COMPARE(harness.Runners.size(), 2);
    QCOMPARE(harness.Records.at(1)->Command.Arguments, harness.Records.at(0)->Command.Arguments);
    harness.Runners.at(1)->EmitStarted();
    CreateTemporaryOutput(job);
    harness.Runners.at(1)->EmitFinished(0);
    QTRY_COMPARE(completionSpy.count(), 2);
    QCOMPARE(*controller->StateForJob(job.JobId), ClipCutter::EExportState::Succeeded);
}

void ExportEngineTests::EmptyQueue()
{
    RunnerHarness harness;
    auto controller = MakeController(harness);
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({});
    controller->Start();
    QTRY_COMPARE(completionSpy.count(), 1);
    QCOMPARE(harness.Runners.size(), 0);
    const ClipCutter::ExportSummary summary = qvariant_cast<ClipCutter::ExportSummary>(completionSpy.at(0).at(0));
    QCOMPARE(summary.SucceededCount, 0);
    QCOMPARE(summary.FailedCount, 0);
}

void ExportEngineTests::QueueContainingOnlySkippedJobs()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    ClipCutter::ExportJob first = MakeJob(directory, QStringLiteral("skip-one"));
    ClipCutter::ExportJob second = MakeJob(directory, QStringLiteral("skip-two"));
    first.SkipRequested = true;
    second.SkipRequested = true;
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({first, second});
    controller->Start();
    QTRY_COMPARE(completionSpy.count(), 1);
    QCOMPARE(harness.Runners.size(), 0);
    const ClipCutter::ExportSummary summary = qvariant_cast<ClipCutter::ExportSummary>(completionSpy.at(0).at(0));
    QCOMPARE(summary.SkippedCount, 2);
}

void ExportEngineTests::DuplicateFinishedAndErrorSignals()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    const ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("duplicates"));
    QSignalSpy jobCompletionSpy(controller.get(), &ClipCutter::ExportQueueController::JobCompleted);
    QSignalSpy queueCompletionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({job});
    controller->Start();
    QTRY_COMPARE(harness.Runners.size(), 1);
    FakeProcessRunner* runner = harness.Runners.at(0);
    runner->EmitStarted();
    runner->EmitError(QProcess::Crashed);
    runner->EmitFinished(-1, QProcess::CrashExit);
    runner->EmitError(QProcess::UnknownError);
    QTRY_COMPARE(queueCompletionSpy.count(), 1);
    QCOMPARE(jobCompletionSpy.count(), 1);
}

void ExportEngineTests::TotalProgressUsesDurationWeights()
{
    QTemporaryDir directory;
    RunnerHarness harness;
    auto controller = MakeController(harness);
    const ClipCutter::ExportJob shortJob = MakeJob(directory, QStringLiteral("short"), 1s);
    const ClipCutter::ExportJob longJob = MakeJob(directory, QStringLiteral("long"), 3s);
    QSignalSpy progressSpy(controller.get(), &ClipCutter::ExportQueueController::TotalProgressChanged);
    QSignalSpy completionSpy(controller.get(), &ClipCutter::ExportQueueController::QueueCompleted);
    controller->SetJobs({shortJob, longJob});
    controller->Start();
    QTRY_COMPARE(harness.Runners.size(), 1);
    harness.Runners.at(0)->EmitStarted();
    harness.Runners.at(0)->EmitStandardOutput("out_time_us=500000\nprogress=continue\n");
    QVERIFY(qAbs(LastProgress(progressSpy) - 0.125) < 0.0001);
    CreateTemporaryOutput(shortJob);
    harness.Runners.at(0)->EmitFinished(0);
    QTRY_COMPARE(harness.Runners.size(), 2);
    harness.Runners.at(1)->EmitStarted();
    harness.Runners.at(1)->EmitStandardOutput("out_time_ms=1500000\nprogress=continue\n");
    QVERIFY(qAbs(LastProgress(progressSpy) - 0.625) < 0.0001);
    CreateTemporaryOutput(longJob);
    harness.Runners.at(1)->EmitFinished(0);
    QTRY_COMPARE(completionSpy.count(), 1);
    QVERIFY(qAbs(LastProgress(progressSpy) - 1.0) < 0.0001);
}

void ExportEngineTests::OptionalFfmpegIntegration()
{
    const QString ffmpegPath = qEnvironmentVariable("CLIPCUTTER_TEST_FFMPEG");

    if (ffmpegPath.isEmpty())
    {
        QSKIP("Set CLIPCUTTER_TEST_FFMPEG to enable the FFmpeg integration test.");
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = QDir(directory.path()).absoluteFilePath(QStringLiteral("integration-source.mp4"));
    QProcess generator;
    QSignalSpy generatorStartedSpy(&generator, &QProcess::started);
    QSignalSpy generatorFinishedSpy(&generator, qOverload<int, QProcess::ExitStatus>(&QProcess::finished));
    generator.setProgram(ffmpegPath);
    generator.setArguments({QStringLiteral("-nostdin"), QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
                            QStringLiteral("error"), QStringLiteral("-y"), QStringLiteral("-f"),
                            QStringLiteral("lavfi"), QStringLiteral("-i"), QStringLiteral("color=c=black:s=64x64:d=1"),
                            QStringLiteral("-c:v"), QStringLiteral("libx264"), sourcePath});
    generator.start();
    QTRY_COMPARE_WITH_TIMEOUT(generatorStartedSpy.count(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(generatorFinishedSpy.count(), 1, 15000);
    QCOMPARE(generator.exitCode(), 0);

    ClipCutter::ExportJob job = MakeJob(directory, QStringLiteral("integration-output"), 500ms);
    job.SourcePath = sourcePath;
    job.StartTime = 0ms;
    ClipCutter::ExportQueueController controller(ClipCutter::FfmpegCommandBuilder(ffmpegPath), {},
                                                 std::make_unique<SuccessfulMetadataService>());
    QSignalSpy completionSpy(&controller, &ClipCutter::ExportQueueController::QueueCompleted);
    controller.SetJobs({job});
    controller.Start();
    QTRY_COMPARE_WITH_TIMEOUT(completionSpy.count(), 1, 15000);
    QCOMPARE(*controller.StateForJob(job.JobId), ClipCutter::EExportState::Succeeded);
    QVERIFY(QFile::exists(job.FinalOutputPath));
}

QTEST_GUILESS_MAIN(ExportEngineTests)

#include "ExportEngineTests.moc"
