#include "Core/Diagnostics/StartupDiagnostics.h"

#include "Core/Export/OutputProfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace ClipCutter
{
StartupDiagnostics::StartupDiagnostics(QObject* parent, QString ffmpegPath, QString ffprobePath)
    : QObject(parent), FfmpegPath_(std::move(ffmpegPath)), FfprobePath_(std::move(ffprobePath))
{
    if (FfmpegPath_.isEmpty()) FfmpegPath_ = DefaultPath(QStringLiteral("ffmpeg"));
    if (FfprobePath_.isEmpty()) FfprobePath_ = DefaultPath(QStringLiteral("ffprobe"));
    qRegisterMetaType<StartupDiagnosticsResult>();
    connect(&Process_, &QProcess::readyReadStandardOutput, this, [this]() { Output_ += Process_.readAllStandardOutput(); });
    connect(&Process_, &QProcess::readyReadStandardError, this, [this]() { Error_ += Process_.readAllStandardError(); });
    connect(&Process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](const int code, const QProcess::ExitStatus status) { FinishStep(code, status); });
    connect(&Process_, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError error)
            { if (error == QProcess::FailedToStart) FinishStep(-1, QProcess::CrashExit); });
}

void StartupDiagnostics::Start()
{
    if (Process_.state() != QProcess::NotRunning) return;
    Result_ = {};
    Step_ = EStep::FfmpegVersion;
    RunStep();
}

void StartupDiagnostics::RunStep()
{
    Output_.clear();
    Error_.clear();
    switch (Step_)
    {
    case EStep::FfmpegVersion: Process_.start(FfmpegPath_, {QStringLiteral("-version")}); break;
    case EStep::FfprobeVersion: Process_.start(FfprobePath_, {QStringLiteral("-version")}); break;
    case EStep::Encoders: Process_.start(FfmpegPath_, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")}); break;
    case EStep::Muxers: Process_.start(FfmpegPath_, {QStringLiteral("-hide_banner"), QStringLiteral("-muxers")}); break;
    case EStep::Done: Complete(); break;
    }
}

void StartupDiagnostics::FinishStep(const int exitCode, const QProcess::ExitStatus status)
{
    Output_ += Process_.readAllStandardOutput();
    Error_ += Process_.readAllStandardError();
    const bool success = status == QProcess::NormalExit && exitCode == 0;
    Result_.Diagnostics += QStringLiteral("%1\n%2\n").arg(QString::fromUtf8(Output_), QString::fromUtf8(Error_));
    switch (Step_)
    {
    case EStep::FfmpegVersion:
        Result_.FfmpegAvailable = success;
        Result_.FfmpegVersion = QString::fromUtf8(Output_).section(QLatin1Char('\n'), 0, 0).trimmed();
        Step_ = EStep::FfprobeVersion;
        break;
    case EStep::FfprobeVersion:
        Result_.FfprobeAvailable = success;
        Result_.FfprobeVersion = QString::fromUtf8(Output_).section(QLatin1Char('\n'), 0, 0).trimmed();
        Step_ = Result_.FfmpegAvailable ? EStep::Encoders : EStep::Done;
        break;
    case EStep::Encoders:
        Encoders_ = QString::fromUtf8(Output_) + QString::fromUtf8(Error_);
        Step_ = EStep::Muxers;
        break;
    case EStep::Muxers:
        Muxers_ = QString::fromUtf8(Output_) + QString::fromUtf8(Error_);
        Step_ = EStep::Done;
        break;
    case EStep::Done: return;
    }
    RunStep();
}

void StartupDiagnostics::Complete()
{
    for (const OutputProfile& profile : OutputProfiles::BuiltIns())
    {
        ProfileSupport support;
        if (!Result_.FfmpegAvailable || !Result_.FfprobeAvailable)
            support.Reason = QStringLiteral("Both ffmpeg and ffprobe are required.");
        else if (profile.TrimMode == ETrimMode::FastCopy)
            support.Supported = true;
        else
        {
            QStringList missing;
            if (!Encoders_.contains(QStringLiteral("libx264"))) missing.append(QStringLiteral("libx264 encoder"));
            if (!Encoders_.contains(QStringLiteral(" aac"))) missing.append(QStringLiteral("AAC encoder"));
            if (!Muxers_.contains(QStringLiteral(" mp4"))) missing.append(QStringLiteral("MP4 muxer"));
            support.Supported = missing.isEmpty();
            if (!support.Supported) support.Reason = QStringLiteral("Unavailable: %1.").arg(missing.join(QStringLiteral(", ")));
        }
        Result_.Profiles.insert(profile.Id, support);
    }
    emit Completed(Result_);
}

QString StartupDiagnostics::DefaultPath(const QString& executable)
{
#ifdef Q_OS_WIN
    const QString fileName = executable + QStringLiteral(".exe");
#else
    const QString fileName = executable;
#endif
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(fileName);
    return QFileInfo::exists(bundled) ? bundled : executable;
}
} // namespace ClipCutter
