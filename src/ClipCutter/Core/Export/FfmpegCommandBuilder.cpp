#include "Core/Export/FfmpegCommandBuilder.h"

#include <QCoreApplication>
#include <QDir>

namespace ClipCutter
{
FfmpegCommandBuilder::FfmpegCommandBuilder(QString programPath) : ProgramPath_(std::move(programPath))
{
    if (ProgramPath_.isEmpty())
    {
#ifdef Q_OS_WIN
        ProgramPath_ = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("ffmpeg.exe"));
#else
        ProgramPath_ = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("ffmpeg"));
#endif
    }
}

FfmpegCommand FfmpegCommandBuilder::Build(const ExportJob& job) const
{
    FfmpegCommand command;
    command.Program = ProgramPath_;
    command.Arguments =
    {
        QStringLiteral("-nostdin"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"),
        QStringLiteral("error"),
        QStringLiteral("-progress"),
        QStringLiteral("pipe:1"),
        QStringLiteral("-y"),
        QStringLiteral("-i"),
        job.SourcePath,
        QStringLiteral("-ss"),
        MillisecondsArgument(job.StartTime)
    };

    if (job.Duration.has_value())
    {
        command.Arguments.append(QStringLiteral("-t"));
        command.Arguments.append(MillisecondsArgument(*job.Duration));
    }

    QStringList encodingArguments;

    switch (job.EncodingQuality)
    {
    case EEncodingQuality::Copy:
        encodingArguments =
        {
            QStringLiteral("-c:v"), QStringLiteral("copy"), QStringLiteral("-c:a"), QStringLiteral("copy")
        };
        break;
    case EEncodingQuality::Lowest:
        encodingArguments =
        {
            QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-crf"), QStringLiteral("35"),
            QStringLiteral("-preset"), QStringLiteral("faster"), QStringLiteral("-c:a"), QStringLiteral("copy")
        };
        break;
    case EEncodingQuality::Low:
        encodingArguments =
        {
            QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-crf"), QStringLiteral("30"),
            QStringLiteral("-preset"), QStringLiteral("fast"), QStringLiteral("-c:a"), QStringLiteral("copy")
        };
        break;
    case EEncodingQuality::Medium:
        encodingArguments =
        {
            QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-crf"), QStringLiteral("25"),
            QStringLiteral("-preset"), QStringLiteral("fast"), QStringLiteral("-c:a"), QStringLiteral("copy")
        };
        break;
    case EEncodingQuality::High:
        encodingArguments =
        {
            QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-crf"), QStringLiteral("20"),
            QStringLiteral("-preset"), QStringLiteral("medium"), QStringLiteral("-c:a"), QStringLiteral("copy")
        };
        break;
    case EEncodingQuality::Highest:
        encodingArguments =
        {
            QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-crf"), QStringLiteral("15"),
            QStringLiteral("-preset"), QStringLiteral("slow"), QStringLiteral("-c:a"), QStringLiteral("copy")
        };
        break;
    }

    command.Arguments.append(encodingArguments);
    command.Arguments.append(job.TemporaryOutputPath);

    return command;
}

QString FfmpegCommandBuilder::MillisecondsArgument(const std::chrono::milliseconds value)
{
    return QStringLiteral("%1ms").arg(value.count());
}
} // namespace ClipCutter
