#include "Core/Export/OutputVerifier.h"

#include "Core/Media/MediaProbe.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include <cmath>
#include <memory>

namespace ClipCutter
{
std::chrono::milliseconds OutputVerifier::DurationTolerance(const ETrimMode mode) noexcept
{
    return mode == ETrimMode::FastCopy ? std::chrono::milliseconds{2500} : std::chrono::milliseconds{350};
}

VerificationResult OutputVerifier::Validate(const ExportJob& job, const OutputProfile& profile,
                                            const MediaInfo& outputInfo, const QString& diagnostics)
{
    VerificationResult result;
    result.Diagnostics = diagnostics;
    result.OutputInfo = outputInfo;
    if (outputInfo.ProbeStatus != EProbeStatus::Ready)
    {
        result.ErrorMessage = outputInfo.ProbeError.value_or(QStringLiteral("Output could not be probed."));
        return result;
    }
    if (job.SourceMediaInfo.HasVideo.value_or(true) && !outputInfo.HasVideo.value_or(false))
    {
        result.ErrorMessage = QStringLiteral("Output verification found no video stream.");
        return result;
    }
    if (job.SourceMediaInfo.HasAudio.value_or(false) && !outputInfo.HasAudio.value_or(false))
    {
        result.ErrorMessage = QStringLiteral("Output verification found no audio stream expected from the source.");
        return result;
    }
    if (!job.Duration.has_value() || !outputInfo.Duration.has_value())
    {
        result.ErrorMessage = QStringLiteral("Output duration is unknown and cannot be verified.");
        return result;
    }
    const auto difference = std::chrono::milliseconds{std::llabs((outputInfo.Duration.value() - *job.Duration).count())};
    if (difference > DurationTolerance(profile.TrimMode))
    {
        result.ErrorMessage = QStringLiteral("Output duration differs from the selected duration by %1 ms (allowed %2 ms).")
                                  .arg(difference.count()).arg(DurationTolerance(profile.TrimMode).count());
        return result;
    }
    result.Success = true;
    return result;
}

FfprobeOutputVerifier::FfprobeOutputVerifier(QObject* parent, QString ffprobePath)
    : OutputVerifier(parent), ProgramPath_(std::move(ffprobePath))
{
    if (ProgramPath_.isEmpty())
    {
#ifdef Q_OS_WIN
        const QString bundled = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("ffprobe.exe"));
#else
        const QString bundled = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("ffprobe"));
#endif
        ProgramPath_ = QFileInfo::exists(bundled) ? bundled : QStringLiteral("ffprobe");
    }
}

void FfprobeOutputVerifier::Verify(const ExportJob& job, const OutputProfile& profile, Callback callback)
{
    const QFileInfo output(job.TemporaryOutputPath);
    if (!output.exists() || !output.isFile() || output.size() <= 0)
    {
        callback({false, QStringLiteral("FFmpeg reported success but the temporary output is missing or empty."), {}});
        return;
    }

    auto* process = new QProcess(this);
    auto standardOutput = std::make_shared<QByteArray>();
    auto standardError = std::make_shared<QByteArray>();
    auto completed = std::make_shared<bool>(false);
    process->setProgram(ProgramPath_);
    process->setArguments({QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-show_format"),
                           QStringLiteral("-show_streams"), QStringLiteral("-of"), QStringLiteral("json"),
                           job.TemporaryOutputPath});
    connect(process, &QProcess::readyReadStandardOutput, this,
            [process, standardOutput]() { *standardOutput += process->readAllStandardOutput(); });
    connect(process, &QProcess::readyReadStandardError, this,
            [process, standardError]() { *standardError += process->readAllStandardError(); });
    connect(process, &QProcess::errorOccurred, this,
            [process, callback, completed](const QProcess::ProcessError error)
            {
                if (*completed) return;
                *completed = true;
                callback({false, QStringLiteral("ffprobe could not verify the output (process error %1).").arg(static_cast<int>(error)), {}});
                process->deleteLater();
            });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [process, standardOutput, standardError, completed, job, profile, callback]
            (const int exitCode, const QProcess::ExitStatus exitStatus)
            {
                if (*completed) return;
                *completed = true;
                *standardOutput += process->readAllStandardOutput();
                *standardError += process->readAllStandardError();
                const QString diagnostics = QStringLiteral("ffprobe stderr:\n%1\nffprobe JSON:\n%2")
                                                .arg(QString::fromUtf8(*standardError), QString::fromUtf8(*standardOutput));
                if (exitStatus != QProcess::NormalExit || exitCode != 0)
                {
                    callback({false, QStringLiteral("ffprobe verification exited with code %1.").arg(exitCode), diagnostics});
                }
                else
                {
                    QString parseError;
                    const MediaInfo info = MediaProbe::ParseJson(*standardOutput, &parseError);
                    callback(OutputVerifier::Validate(job, profile, info, diagnostics));
                }
                process->deleteLater();
            });
    process->start();
}
} // namespace ClipCutter
