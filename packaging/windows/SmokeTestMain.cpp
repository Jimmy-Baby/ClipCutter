#include "ClipCutterVersion.h"
#include "Core/Export/ExportQueueController.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include <chrono>

namespace
{
using namespace std::chrono_literals;

struct ProcessResult
{
    bool Success = false;
    QByteArray StandardOutput;
    QByteArray StandardError;
};

QString ProgramPath(const QString& baseName)
{
#ifdef Q_OS_WIN
    const QString fileName = baseName + QStringLiteral(".exe");
#else
    const QString fileName = baseName;
#endif
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(fileName);
}

ProcessResult Run(const QString& program, const QStringList& arguments, const int timeoutMs = 30000)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    const bool started = process.waitForStarted(5000);
    const bool finished = started && process.waitForFinished(timeoutMs);
    if (!finished) process.kill();
    return {finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
            process.readAllStandardOutput(), process.readAllStandardError()};
}

int Fail(const QString& message, const QByteArray& diagnostics = {})
{
    QTextStream error(stderr);
    error << "package smoke test failed: " << message << Qt::endl;
    if (!diagnostics.isEmpty()) error << QString::fromUtf8(diagnostics) << Qt::endl;
    return 1;
}
} // namespace

int main(int argumentCount, char* arguments[])
{
    QCoreApplication application(argumentCount, arguments);
    QCoreApplication::setApplicationName(QStringLiteral("ClipCutterPackageSmoke"));
    QCoreApplication::setApplicationVersion(QStringLiteral(CLIPCUTTER_VERSION_STRING));

    if (argumentCount != 2) return Fail(QStringLiteral("expected one package-directory argument"));
    const QString expectedPackageDirectory = QDir::cleanPath(QFileInfo(QString::fromLocal8Bit(arguments[1])).absoluteFilePath());
    const QString actualPackageDirectory = QDir::cleanPath(QCoreApplication::applicationDirPath());
    if (QDir(expectedPackageDirectory) != QDir(actualPackageDirectory))
        return Fail(QStringLiteral("smoke executable must run from the package root"));

    const QString ffmpeg = ProgramPath(QStringLiteral("ffmpeg"));
    const QString ffprobe = ProgramPath(QStringLiteral("ffprobe"));
    if (!QFileInfo::exists(ffmpeg) || !QFileInfo::exists(ffprobe))
        return Fail(QStringLiteral("bundled ffmpeg.exe or ffprobe.exe is missing"));

    const ProcessResult ffmpegVersion = Run(ffmpeg, {QStringLiteral("-version")});
    const ProcessResult ffprobeVersion = Run(ffprobe, {QStringLiteral("-version")});
    if (!ffmpegVersion.Success) return Fail(QStringLiteral("bundled ffmpeg version check failed"), ffmpegVersion.StandardError);
    if (!ffprobeVersion.Success) return Fail(QStringLiteral("bundled ffprobe version check failed"), ffprobeVersion.StandardError);

    QTemporaryDir temporary(QStringLiteral("ClipCutter-package-smoke-XXXXXX"));
    if (!temporary.isValid()) return Fail(QStringLiteral("could not create the smoke-test directory"));
    const QString source = QDir(temporary.path()).absoluteFilePath(QStringLiteral("source.mp4"));
    const QString output = QDir(temporary.path()).absoluteFilePath(QStringLiteral("export.mp4"));
    const QString partial = QDir(temporary.path()).absoluteFilePath(QStringLiteral("export.part.mp4"));

    const ProcessResult generated = Run(ffmpeg,
        {QStringLiteral("-nostdin"), QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
         QStringLiteral("error"), QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"),
         QStringLiteral("-i"), QStringLiteral("testsrc2=size=64x64:rate=10:duration=1"),
         QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
         QStringLiteral("sine=frequency=1000:sample_rate=48000:duration=1"), QStringLiteral("-c:v"),
         QStringLiteral("libx264"), QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"),
         QStringLiteral("-c:a"), QStringLiteral("aac"), QStringLiteral("-metadata"),
         QStringLiteral("creation_time=2000-01-01T00:00:00Z"), source});
    if (!generated.Success) return Fail(QStringLiteral("deterministic media generation failed"), generated.StandardError);

    const ProcessResult sourceProbe = Run(ffprobe,
        {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-show_entries"),
         QStringLiteral("format=duration:stream=codec_type"), QStringLiteral("-of"), QStringLiteral("json"), source});
    if (!sourceProbe.Success || !sourceProbe.StandardOutput.contains("video"))
        return Fail(QStringLiteral("generated sample could not be probed"), sourceProbe.StandardError + sourceProbe.StandardOutput);

    ClipCutter::ExportJob job;
    job.SourcePath = source;
    job.FinalOutputPath = output;
    job.TemporaryOutputPath = partial;
    job.StartTime = 200ms;
    job.Duration = 400ms;
    job.OutputProfileId = QStringLiteral("accurate-balanced");
    job.CopyMetadata = false;
    job.CollisionPolicy = ClipCutter::ECollisionPolicy::Overwrite;
    job.SourceMediaInfo.Duration = 1000ms;
    job.SourceMediaInfo.HasVideo = true;
    job.SourceMediaInfo.HasAudio = true;
    job.SourceMediaInfo.VideoStreamIndices = {0};
    job.SourceMediaInfo.AudioStreamIndices = {1};
    job.SourceMediaInfo.ProbeStatus = ClipCutter::EProbeStatus::Ready;

    ClipCutter::ExportQueueController controller;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool completed = false;
    ClipCutter::ExportSummary summary;
    QObject::connect(&controller, &ClipCutter::ExportQueueController::QueueCompleted,
                     &loop, [&](const ClipCutter::ExportSummary& value) { completed = true; summary = value; loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    if (!controller.SetJobs({job}) || !controller.Start()) return Fail(QStringLiteral("core export queue did not start"));
    timeout.start(30000);
    loop.exec();
    if (!completed || summary.SucceededCount != 1 || summary.FailedCount != 0 || !QFileInfo::exists(output))
        return Fail(QStringLiteral("core export path failed"), controller.LogForJob(job.JobId).toUtf8());

    const ProcessResult outputProbe = Run(ffprobe,
        {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-show_entries"),
         QStringLiteral("format=duration:stream=codec_type"), QStringLiteral("-of"), QStringLiteral("json"), output});
    if (!outputProbe.Success || !outputProbe.StandardOutput.contains("video"))
        return Fail(QStringLiteral("exported sample failed ffprobe verification"), outputProbe.StandardError + outputProbe.StandardOutput);

    const QJsonObject result{
        {QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion()},
        {QStringLiteral("packageDirectory"), actualPackageDirectory},
        {QStringLiteral("ffmpegPath"), ffmpeg},
        {QStringLiteral("ffprobePath"), ffprobe},
        {QStringLiteral("sourceProbed"), true},
        {QStringLiteral("coreExportSucceeded"), true},
        {QStringLiteral("exportProbed"), true}
    };
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << Qt::endl;
    return 0;
}
