#include "Core/Export/FfmpegCommandBuilder.h"

#include "Core/Export/OutputProfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace ClipCutter
{
FfmpegCommandBuilder::FfmpegCommandBuilder(QString programPath) : ProgramPath_(std::move(programPath))
{
    if (ProgramPath_.isEmpty())
    {
#ifdef Q_OS_WIN
        const QString bundled = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("ffmpeg.exe"));
#else
        const QString bundled = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("ffmpeg"));
#endif
        ProgramPath_ = QFileInfo::exists(bundled) ? bundled : QStringLiteral("ffmpeg");
    }
}

FfmpegCommand FfmpegCommandBuilder::Build(const ExportJob& job) const
{
    FfmpegCommand command;
    command.Program = ProgramPath_;
    command.Arguments = {
        QStringLiteral("-nostdin"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"),
        QStringLiteral("error"),
        QStringLiteral("-progress"),
        QStringLiteral("pipe:1"),
        QStringLiteral("-n")
    };

    const OutputProfile* profile = OutputProfiles::Find(job.OutputProfileId);
    if (profile == nullptr)
    {
        return {};
    }

    if (profile->TrimMode == ETrimMode::FastCopy)
    {
        command.Arguments.append({QStringLiteral("-ss"), MillisecondsArgument(job.StartTime)});
    }
    command.Arguments.append({QStringLiteral("-i"), job.SourcePath});
    if (profile->TrimMode == ETrimMode::AccurateEncode)
        command.Arguments.append({QStringLiteral("-ss"), MillisecondsArgument(job.StartTime)});
    if (job.Duration.has_value())
        command.Arguments.append({QStringLiteral("-t"), MillisecondsArgument(*job.Duration)});

    if (profile->TrimMode == ETrimMode::FastCopy)
        command.Arguments.append({QStringLiteral("-map"), QStringLiteral("0"),
                                  QStringLiteral("-map_metadata"), QStringLiteral("0"),
                                  QStringLiteral("-map_chapters"), QStringLiteral("0"),
                                  QStringLiteral("-c"), QStringLiteral("copy")});
    else
    {
        command.Arguments.append({QStringLiteral("-map"), QStringLiteral("0:v:0"),
                                  QStringLiteral("-map"), QStringLiteral("0:a:0?"),
                                  QStringLiteral("-map_metadata"), QStringLiteral("0"),
                                  QStringLiteral("-map_chapters"), QStringLiteral("0"),
                                  QStringLiteral("-c:v"), QStringLiteral("libx264"),
                                  QStringLiteral("-crf"), QString::number(profile->Crf.value()),
                                  QStringLiteral("-preset"), profile->EncoderPreset,
                                  QStringLiteral("-c:a"), QStringLiteral("aac"),
                                  QStringLiteral("-movflags"), QStringLiteral("+faststart"),
                                  QStringLiteral("-f"), QStringLiteral("mp4")});
    }

    command.Arguments.append(job.TemporaryOutputPath);

    return command;
}

QString FfmpegCommandBuilder::MillisecondsArgument(const std::chrono::milliseconds value)
{
    return QStringLiteral("%1ms").arg(value.count());
}
} // namespace ClipCutter
