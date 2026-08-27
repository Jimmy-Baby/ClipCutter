#include "App/Models/ClipQueueModel.h"
#include "Core/Export/FfmpegCommandBuilder.h"
#include "Core/Export/MetadataService.h"
#include "Core/Export/OutputPathPlanner.h"
#include "Core/Export/OutputProfile.h"
#include "Core/Export/OutputVerifier.h"
#include "Core/Media/MediaProbe.h"
#include "Core/Import/ClipImporter.h"

#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include <chrono>

using namespace std::chrono_literals;

namespace
{
QByteArray ProbeJson(bool audio = true, bool multiple = false, bool duration = true)
{
    const QByteArray durationField = duration ? QByteArray(R"("duration":"12.345",)") : QByteArray();
    const QByteArray audioStreams = audio
        ? (multiple
               ? QByteArray(R"(,{"index":2,"codec_type":"audio","codec_name":"aac"},{"index":3,"codec_type":"audio","codec_name":"opus"})")
               : QByteArray(R"(,{"index":1,"codec_type":"audio","codec_name":"aac"})"))
        : QByteArray();
    return QByteArray(R"({"format":{)") + durationField +
           R"("format_name":"mov,mp4","tags":{"creation_time":"2026-01-02T03:04:05Z"}},"streams":[{"index":0,"codec_type":"video","codec_name":"h264","width":1920,"height":1080,"avg_frame_rate":"30000/1001"})" +
           audioStreams + R"(]})";
}

void WriteFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
    QCOMPARE(file.write(contents), contents.size());
}

ClipCutter::ExportJob MakeVerificationJob(std::chrono::milliseconds duration)
{
    ClipCutter::ExportJob job;
    job.Duration = duration;
    job.SourceMediaInfo.HasVideo = true;
    job.SourceMediaInfo.HasAudio = false;
    return job;
}
} // namespace

class MediaCorrectnessTests : public QObject
{
    Q_OBJECT

private slots:
    void ProbeJsonParsing();
    void ProbeAbsentAudio();
    void ProbeMultipleStreams();
    void ProbeMalformedJson();
    void ProbeUnknownDuration();
    void LateProbeDoesNotOverwriteTrim();
    void OutputProfileLookupAndExtensions();
    void BuiltInCommandGeneration();
    void FastCopyAndAccurateOrdering();
    void UnicodePathsRemainArguments();
    void InvalidWindowsNames();
    void CollisionPoliciesAndAutoRename();
    void TemporaryFilenameGeneration();
    void FinalisationSuccessAndFailure();
    void MetadataErrorsAreStructured();
    void VerificationTolerance();
    void OptionalFfmpegIntegration();
};

void MediaCorrectnessTests::ProbeJsonParsing()
{
    QString error;
    const ClipCutter::MediaInfo info = ClipCutter::MediaProbe::ParseJson(ProbeJson(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(info.ProbeStatus, ClipCutter::EProbeStatus::Ready);
    QCOMPARE(info.Duration, std::optional<std::chrono::milliseconds>{12345ms});
    QCOMPARE(info.ContainerName, std::optional<QString>{QStringLiteral("mov,mp4")});
    QCOMPARE(info.VideoCodec, std::optional<QString>{QStringLiteral("h264")});
    QCOMPARE(info.Width, std::optional<int>{1920});
    QCOMPARE(info.Height, std::optional<int>{1080});
    QCOMPARE(info.FrameRateNumerator, std::optional<int>{30000});
    QCOMPARE(info.FrameRateDenominator, std::optional<int>{1001});
    QCOMPARE(info.CreationTime, std::optional<QString>{QStringLiteral("2026-01-02T03:04:05Z")});
}

void MediaCorrectnessTests::ProbeAbsentAudio()
{
    const ClipCutter::MediaInfo info = ClipCutter::MediaProbe::ParseJson(ProbeJson(false));
    QCOMPARE(info.HasVideo, std::optional<bool>{true});
    QCOMPARE(info.HasAudio, std::optional<bool>{false});
    QVERIFY(!info.AudioCodec.has_value());
    QVERIFY(info.AudioStreamIndices.isEmpty());
}

void MediaCorrectnessTests::ProbeMultipleStreams()
{
    const ClipCutter::MediaInfo info = ClipCutter::MediaProbe::ParseJson(ProbeJson(true, true));
    QCOMPARE(info.VideoStreamIndices, QVector<int>{0});
    QCOMPARE(info.AudioStreamIndices, QVector<int>({2, 3}));
    QCOMPARE(info.AudioCodec, std::optional<QString>{QStringLiteral("aac")});
}

void MediaCorrectnessTests::ProbeMalformedJson()
{
    QString error;
    const ClipCutter::MediaInfo info = ClipCutter::MediaProbe::ParseJson("{broken", &error);
    QCOMPARE(info.ProbeStatus, ClipCutter::EProbeStatus::Failed);
    QVERIFY(!error.isEmpty());
    QVERIFY(info.ProbeError.has_value());
}

void MediaCorrectnessTests::ProbeUnknownDuration()
{
    const ClipCutter::MediaInfo info = ClipCutter::MediaProbe::ParseJson(ProbeJson(true, false, false));
    QCOMPARE(info.ProbeStatus, ClipCutter::EProbeStatus::Ready);
    QVERIFY(!info.Duration.has_value());
    QVERIFY(!info.IsReliableForExport());
}

void MediaCorrectnessTests::LateProbeDoesNotOverwriteTrim()
{
    ClipCutter::Clip clip;
    clip.SourcePath = QStringLiteral("C:/source.mp4");
    clip.OriginalFileName = QStringLiteral("source.mp4");
    ClipCutter::Segment segment;
    segment.OutputBaseName = QStringLiteral("source");
    segment.OutputExtension = QStringLiteral(".mp4");
    clip.Segments.append(segment);
    const QUuid clipId = clip.Id;
    const QUuid segmentId = segment.Id;
    ClipCutter::ClipQueueModel model;
    model.AddClip(clip);
    QVERIFY(model.UpdateTrimRange(segmentId, *ClipCutter::TimeRange::Create(1s, 2s)));
    ClipCutter::MediaInfo info = ClipCutter::MediaProbe::ParseJson(ProbeJson());
    QVERIFY(model.UpdateMediaInfo(clipId, clip.SourcePath, info));
    QCOMPARE(model.FindSegment(segmentId)->Range.Start(), 1s);
    QCOMPARE(model.FindSegment(segmentId)->Range.End(), 2s);
    QVERIFY(!model.UpdateMediaInfo(clipId, QStringLiteral("C:/replacement.mp4"), info));
}

void MediaCorrectnessTests::OutputProfileLookupAndExtensions()
{
    QCOMPARE(ClipCutter::OutputProfiles::BuiltIns().size(), 4);
    QVERIFY(ClipCutter::OutputProfiles::Find(QStringLiteral("missing")) == nullptr);
    const auto* copy = ClipCutter::OutputProfiles::Find(QStringLiteral("fast-copy"));
    const auto* balanced = ClipCutter::OutputProfiles::Find(QStringLiteral("accurate-balanced"));
    QVERIFY(copy != nullptr && balanced != nullptr);
    QCOMPARE(ClipCutter::OutputProfiles::ExtensionFor(*copy, QStringLiteral("C:/x/film.MKV")), QStringLiteral(".MKV"));
    QCOMPARE(ClipCutter::OutputProfiles::ExtensionFor(*balanced, QStringLiteral("C:/x/film.MKV")), QStringLiteral(".mp4"));
}

void MediaCorrectnessTests::BuiltInCommandGeneration()
{
    for (const ClipCutter::OutputProfile& profile : ClipCutter::OutputProfiles::BuiltIns())
    {
        ClipCutter::ExportJob job;
        job.SourcePath = QStringLiteral("C:/input.mkv");
        job.TemporaryOutputPath = QStringLiteral("C:/out/.clipcutter.part.mp4");
        job.StartTime = 1s;
        job.Duration = 2s;
        job.OutputProfileId = profile.Id;
        const QStringList arguments = ClipCutter::FfmpegCommandBuilder(QStringLiteral("ffmpeg")).Build(job).Arguments;
        QCOMPARE(arguments.constLast(), job.TemporaryOutputPath);
        QVERIFY(arguments.contains(QStringLiteral("-t")));
        if (profile.TrimMode == ClipCutter::ETrimMode::FastCopy)
        {
            QVERIFY(arguments.contains(QStringLiteral("copy")));
            QVERIFY(arguments.contains(QStringLiteral("0")));
        }
        else
        {
            QVERIFY(arguments.contains(QStringLiteral("0:a:0?")));
            QVERIFY(arguments.contains(QStringLiteral("libx264")));
            QVERIFY(arguments.contains(QStringLiteral("aac")));
            QVERIFY(arguments.contains(QString::number(profile.Crf.value())));
            QVERIFY(arguments.contains(profile.EncoderPreset));
            QVERIFY(arguments.contains(QStringLiteral("mp4")));
        }
    }
}

void MediaCorrectnessTests::FastCopyAndAccurateOrdering()
{
    ClipCutter::ExportJob job;
    job.SourcePath = QStringLiteral("input.mkv");
    job.TemporaryOutputPath = QStringLiteral("out.mkv");
    job.StartTime = 1500ms;
    job.Duration = 2s;
    job.OutputProfileId = QStringLiteral("fast-copy");
    QStringList arguments = ClipCutter::FfmpegCommandBuilder(QStringLiteral("ffmpeg")).Build(job).Arguments;
    QVERIFY(arguments.indexOf(QStringLiteral("-ss")) < arguments.indexOf(QStringLiteral("-i")));
    job.OutputProfileId = QStringLiteral("accurate-balanced");
    arguments = ClipCutter::FfmpegCommandBuilder(QStringLiteral("ffmpeg")).Build(job).Arguments;
    QVERIFY(arguments.indexOf(QStringLiteral("-ss")) > arguments.indexOf(QStringLiteral("-i")));
    QVERIFY(arguments.indexOf(QStringLiteral("-t")) > arguments.indexOf(QStringLiteral("-ss")));
}

void MediaCorrectnessTests::UnicodePathsRemainArguments()
{
    ClipCutter::ExportJob job;
    job.SourcePath = QStringLiteral("C:/映像/日本語 clip.mkv");
    job.TemporaryOutputPath = QStringLiteral("C:/出力/.clipcutter-完成.part.mkv");
    job.OutputProfileId = QStringLiteral("fast-copy");
    const QStringList arguments = ClipCutter::FfmpegCommandBuilder(QStringLiteral("ffmpeg")).Build(job).Arguments;
    QCOMPARE(arguments.at(arguments.indexOf(QStringLiteral("-i")) + 1), job.SourcePath);
    QCOMPARE(arguments.constLast(), job.TemporaryOutputPath);
}

void MediaCorrectnessTests::InvalidWindowsNames()
{
    QVERIFY(!ClipCutter::OutputPathPlanner::ValidateBaseName(QString()).isEmpty());
    QVERIFY(!ClipCutter::OutputPathPlanner::ValidateBaseName(QStringLiteral("bad:name")).isEmpty());
    QVERIFY(!ClipCutter::OutputPathPlanner::ValidateBaseName(QStringLiteral("trail.")).isEmpty());
    QVERIFY(!ClipCutter::OutputPathPlanner::ValidateBaseName(QStringLiteral("name ")).isEmpty());
    for (const QString& name : {QStringLiteral("CON"), QStringLiteral("nul.txt"), QStringLiteral("COM9"), QStringLiteral("LPT1")})
        QVERIFY2(!ClipCutter::OutputPathPlanner::ValidateBaseName(name).isEmpty(), qPrintable(name));
    QVERIFY(ClipCutter::OutputPathPlanner::ValidateBaseName(QStringLiteral("日本語 clip")).isEmpty());
}

void MediaCorrectnessTests::CollisionPoliciesAndAutoRename()
{
    QTemporaryDir temporary;
    const QDir directory(temporary.path());
    WriteFile(directory.absoluteFilePath(QStringLiteral("clip.mp4")), "old");
    const QVector<ClipCutter::OutputRequest> request{{QUuid::createUuid(), QStringLiteral("clip"), QStringLiteral(".mp4")}};
    const auto ask = ClipCutter::OutputPathPlanner::Preflight(request, directory, ClipCutter::ECollisionPolicy::Ask);
    QCOMPARE(ask.Collisions.size(), 1);
    const auto skip = ClipCutter::OutputPathPlanner::Preflight(request, directory, ClipCutter::ECollisionPolicy::Skip);
    QVERIFY(skip.IsReady());
    QVERIFY(skip.Outputs.constFirst().Skipped);
    const auto overwrite = ClipCutter::OutputPathPlanner::Preflight(request, directory, ClipCutter::ECollisionPolicy::Overwrite);
    QVERIFY(overwrite.IsReady());
    QCOMPARE(overwrite.Outputs.constFirst().FinalPath, directory.absoluteFilePath(QStringLiteral("clip.mp4")));
    const auto rename1 = ClipCutter::OutputPathPlanner::Preflight(request, directory, ClipCutter::ECollisionPolicy::AutoRename);
    const auto rename2 = ClipCutter::OutputPathPlanner::Preflight(request, directory, ClipCutter::ECollisionPolicy::AutoRename);
    QCOMPARE(rename1.Outputs.constFirst().FinalPath, directory.absoluteFilePath(QStringLiteral("clip (2).mp4")));
    QCOMPARE(rename1.Outputs.constFirst().FinalPath, rename2.Outputs.constFirst().FinalPath);
}

void MediaCorrectnessTests::TemporaryFilenameGeneration()
{
    const QUuid token(QStringLiteral("{12345678-1234-1234-1234-123456789abc}"));
    const QString path = ClipCutter::OutputPathPlanner::CreateTemporaryPath(QStringLiteral("C:/out/movie.mp4"), token);
    QVERIFY(path.endsWith(QStringLiteral(".part.mp4")));
    QVERIFY(path.contains(QStringLiteral("12345678-1234-1234-1234-123456789abc")));
}

void MediaCorrectnessTests::FinalisationSuccessAndFailure()
{
    QTemporaryDir temporary;
    const QDir directory(temporary.path());
    const QString finalPath = directory.absoluteFilePath(QStringLiteral("final.mp4"));
    const QString tempPath = directory.absoluteFilePath(QStringLiteral(".clipcutter-a.part.mp4"));
    WriteFile(tempPath, "new");
    QVERIFY(ClipCutter::OutputPathPlanner::Finalise(tempPath, finalPath, ClipCutter::ECollisionPolicy::Ask).Success);
    QCOMPARE(QFileInfo(finalPath).size(), qint64{3});
    WriteFile(finalPath, "original");
    const QString replacement = directory.absoluteFilePath(QStringLiteral(".clipcutter-b.part.mp4"));
    WriteFile(replacement, "replacement");
    QVERIFY(ClipCutter::OutputPathPlanner::Finalise(
                replacement, finalPath, ClipCutter::ECollisionPolicy::Overwrite).Success);
    QFile replaced(finalPath);
    QVERIFY(replaced.open(QIODevice::ReadOnly));
    QCOMPARE(replaced.readAll(), QByteArray("replacement"));
    replaced.close();
    WriteFile(finalPath, "original");
    const auto failed = ClipCutter::OutputPathPlanner::Finalise(
        directory.absoluteFilePath(QStringLiteral("missing.part.mp4")), finalPath,
        ClipCutter::ECollisionPolicy::Overwrite);
    QVERIFY(!failed.Success);
    QFile existing(finalPath);
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("original"));
}

void MediaCorrectnessTests::MetadataErrorsAreStructured()
{
    ClipCutter::PlatformMetadataService service;
    const ClipCutter::MetadataResult result = service.CopyFileTimestamps(
        QStringLiteral("Z:/definitely/missing/source.mp4"), QStringLiteral("Z:/missing/output.mp4"));
    QVERIFY(!result.Success);
    QCOMPARE(result.Error, ClipCutter::EMetadataError::SourceOpenFailed);
    QVERIFY(!result.Message.isEmpty());
}

void MediaCorrectnessTests::VerificationTolerance()
{
    const auto* accurate = ClipCutter::OutputProfiles::Find(QStringLiteral("accurate-balanced"));
    const auto* copy = ClipCutter::OutputProfiles::Find(QStringLiteral("fast-copy"));
    QVERIFY(accurate != nullptr && copy != nullptr);
    ClipCutter::ExportJob job = MakeVerificationJob(10s);
    ClipCutter::MediaInfo output;
    output.ProbeStatus = ClipCutter::EProbeStatus::Ready;
    output.HasVideo = true;
    output.HasAudio = false;
    output.Duration = 10600ms;
    QVERIFY(!ClipCutter::OutputVerifier::Validate(job, *accurate, output).Success);
    QVERIFY(ClipCutter::OutputVerifier::Validate(job, *copy, output).Success);
    output.Duration = 13000ms;
    QVERIFY(!ClipCutter::OutputVerifier::Validate(job, *copy, output).Success);
}

void MediaCorrectnessTests::OptionalFfmpegIntegration()
{
    const QString ffmpeg = qEnvironmentVariable("CLIPCUTTER_TEST_FFMPEG");
    const QString ffprobe = qEnvironmentVariable("CLIPCUTTER_TEST_FFPROBE");
    if (ffmpeg.isEmpty() || ffprobe.isEmpty())
        QSKIP("Set CLIPCUTTER_TEST_FFMPEG and CLIPCUTTER_TEST_FFPROBE to enable integration tests.");
    QTemporaryDir temporary;
    const QString source = QDir(temporary.path()).absoluteFilePath(QStringLiteral("sourcé no audio.mkv"));
    QProcess generator;
    generator.start(ffmpeg, {QStringLiteral("-nostdin"), QStringLiteral("-y"), QStringLiteral("-f"),
                              QStringLiteral("lavfi"), QStringLiteral("-i"), QStringLiteral("testsrc=size=64x64:rate=10:duration=1"),
                              QStringLiteral("-c:v"), QStringLiteral("libx264"), source});
    QVERIFY(generator.waitForFinished(15000));
    QCOMPARE(generator.exitCode(), 0);
    QProcess probe;
    probe.start(ffprobe, {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-show_format"),
                           QStringLiteral("-show_streams"), QStringLiteral("-of"), QStringLiteral("json"), source});
    QVERIFY(probe.waitForFinished(10000));
    QCOMPARE(probe.exitCode(), 0);
    const ClipCutter::MediaInfo info = ClipCutter::MediaProbe::ParseJson(probe.readAllStandardOutput());
    QCOMPARE(info.ProbeStatus, ClipCutter::EProbeStatus::Ready);
    QCOMPARE(info.HasAudio, std::optional<bool>{false});

    for (const QString& extension : {QStringLiteral("mp4"), QStringLiteral("mkv"),
                                     QStringLiteral("avi"), QStringLiteral("mov")})
    {
        const QString sample = QDir(temporary.path()).absoluteFilePath(QStringLiteral("sample %1.%1").arg(extension));
        QProcess sampleGenerator;
        sampleGenerator.start(ffmpeg, {QStringLiteral("-nostdin"), QStringLiteral("-y"), QStringLiteral("-f"),
                                        QStringLiteral("lavfi"), QStringLiteral("-i"),
                                        QStringLiteral("color=size=32x32:duration=0.3"), QStringLiteral("-c:v"),
                                        QStringLiteral("mpeg4"), QStringLiteral("-an"), sample});
        QVERIFY(sampleGenerator.waitForFinished(15000));
        QCOMPARE(sampleGenerator.exitCode(), 0);
        QVERIFY(ClipCutter::ClipImporter::IsSupported(sample));

        QProcess sampleProbe;
        sampleProbe.start(ffprobe, {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-show_format"),
                                     QStringLiteral("-show_streams"), QStringLiteral("-of"), QStringLiteral("json"), sample});
        QVERIFY(sampleProbe.waitForFinished(10000));
        QCOMPARE(sampleProbe.exitCode(), 0);
        QCOMPARE(ClipCutter::MediaProbe::ParseJson(sampleProbe.readAllStandardOutput()).ProbeStatus,
                 ClipCutter::EProbeStatus::Ready);
    }

    for (const ClipCutter::OutputProfile& profile : ClipCutter::OutputProfiles::BuiltIns())
    {
        ClipCutter::ExportJob job;
        job.SourcePath = source;
        job.StartTime = 0ms;
        job.Duration = 500ms;
        job.OutputProfileId = profile.Id;
        job.SourceMediaInfo = info;
        const QString extension = ClipCutter::OutputProfiles::ExtensionFor(profile, source);
        job.TemporaryOutputPath = QDir(temporary.path()).absoluteFilePath(profile.Id + QStringLiteral(".part") + extension);

        QProcess exportProcess;
        const ClipCutter::FfmpegCommand command = ClipCutter::FfmpegCommandBuilder(ffmpeg).Build(job);
        exportProcess.start(command.Program, command.Arguments);
        QVERIFY2(exportProcess.waitForFinished(20000), qPrintable(exportProcess.errorString()));
        QVERIFY2(exportProcess.exitCode() == 0, exportProcess.readAllStandardError().constData());

        QProcess outputProbe;
        outputProbe.start(ffprobe, {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-show_format"),
                                    QStringLiteral("-show_streams"), QStringLiteral("-of"), QStringLiteral("json"),
                                    job.TemporaryOutputPath});
        QVERIFY(outputProbe.waitForFinished(10000));
        QCOMPARE(outputProbe.exitCode(), 0);
        const ClipCutter::MediaInfo outputInfo =
            ClipCutter::MediaProbe::ParseJson(outputProbe.readAllStandardOutput());
        const ClipCutter::VerificationResult verification =
            ClipCutter::OutputVerifier::Validate(job, profile, outputInfo);
        QVERIFY2(verification.Success, qPrintable(verification.ErrorMessage));
    }
}

QTEST_APPLESS_MAIN(MediaCorrectnessTests)

#include "MediaCorrectnessTests.moc"
