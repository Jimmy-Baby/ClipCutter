#include "Core/Media/MediaProbe.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace ClipCutter
{
namespace
{
std::optional<std::chrono::milliseconds> ParseDuration(const QJsonValue& value)
{
    bool ok = false;
    const double seconds = value.toVariant().toString().toDouble(&ok);
    if (!ok || !std::isfinite(seconds) || seconds < 0.0)
    {
        return std::nullopt;
    }
    return std::chrono::milliseconds{qRound64(seconds * 1000.0)};
}

void ParseFrameRate(const QString& text, MediaInfo& info)
{
    const QStringList parts = text.split(QLatin1Char('/'));
    bool numeratorOk = false;
    bool denominatorOk = false;
    const int numerator = parts.value(0).toInt(&numeratorOk);
    const int denominator = parts.size() == 2 ? parts.at(1).toInt(&denominatorOk) : 1;
    denominatorOk = parts.size() == 1 || denominatorOk;
    if (numeratorOk && denominatorOk && numerator > 0 && denominator > 0)
    {
        info.FrameRateNumerator = numerator;
        info.FrameRateDenominator = denominator;
    }
}
} // namespace

MediaProbe::MediaProbe(QObject* parent, QString ffprobePath, const int maximumConcurrency)
    : QObject(parent), ProgramPath_(std::move(ffprobePath)), MaximumConcurrency_(std::max(1, maximumConcurrency))
{
    if (ProgramPath_.isEmpty())
    {
        ProgramPath_ = DefaultProgramPath();
    }
    qRegisterMetaType<MediaProbeResult>();
}

void MediaProbe::Probe(const QUuid& clipId, const QString& sourcePath)
{
    const quint64 generation = Generations_.value(clipId) + 1;
    Generations_.insert(clipId, generation);
    Request request{clipId, QDir::cleanPath(sourcePath), BuildCacheKey(sourcePath), generation};
    emit ProbeStarted(clipId);

    const auto cached = Cache_.constFind(request.CacheKey);
    if (!request.CacheKey.isEmpty() && cached != Cache_.cend())
    {
        QTimer::singleShot(0, this,
                           [this, request, info = *cached]()
                           {
                               if (Generations_.value(request.ClipId) == request.Generation)
                               {
                                   emit ProbeCompleted({request.ClipId, request.SourcePath, info,
                                                        QStringLiteral("Cached ffprobe result.")});
                               }
                           });
        return;
    }

    Pending_.enqueue(request);
    StartPending();
}

void MediaProbe::Cancel(const QUuid& clipId)
{
    Generations_.insert(clipId, Generations_.value(clipId) + 1);
    for (auto iterator = Active_.begin(); iterator != Active_.end(); ++iterator)
    {
        if (iterator->RequestValue.ClipId == clipId)
        {
            iterator.key()->kill();
        }
    }
}

void MediaProbe::ClearCache()
{
    Cache_.clear();
}

int MediaProbe::MaximumConcurrency() const noexcept
{
    return MaximumConcurrency_;
}

void MediaProbe::SetMaximumConcurrency(const int count)
{
    MaximumConcurrency_ = std::max(1, count);
    StartPending();
}

MediaInfo MediaProbe::ParseJson(const QByteArray& json, QString* error)
{
    if (error != nullptr)
    {
        error->clear();
    }
    MediaInfo info;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        const QString message = QStringLiteral("Invalid ffprobe JSON: %1").arg(parseError.errorString());
        info.ProbeStatus = EProbeStatus::Failed;
        info.ProbeError = message;
        if (error != nullptr)
        {
            *error = message;
        }
        return info;
    }

    const QJsonObject root = document.object();
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();
    const QString formatName = format.value(QStringLiteral("format_name")).toString();
    if (!formatName.isEmpty())
    {
        info.ContainerName = formatName;
    }
    info.Duration = ParseDuration(format.value(QStringLiteral("duration")));
    const QString creationTime = format.value(QStringLiteral("tags")).toObject().value(QStringLiteral("creation_time")).toString();
    if (!creationTime.isEmpty())
    {
        info.CreationTime = creationTime;
    }

    bool hasVideo = false;
    bool hasAudio = false;
    for (const QJsonValue& value : root.value(QStringLiteral("streams")).toArray())
    {
        const QJsonObject stream = value.toObject();
        if (!info.Duration.has_value())
        {
            const auto streamDuration = ParseDuration(stream.value(QStringLiteral("duration")));
            if (streamDuration.has_value()) info.Duration = streamDuration;
        }
        const QString type = stream.value(QStringLiteral("codec_type")).toString();
        const int index = stream.value(QStringLiteral("index")).toInt(-1);
        if (type == QStringLiteral("video"))
        {
            hasVideo = true;
            if (index >= 0)
            {
                info.VideoStreamIndices.append(index);
            }
            if (!info.VideoCodec.has_value())
            {
                const QString codec = stream.value(QStringLiteral("codec_name")).toString();
                if (!codec.isEmpty()) info.VideoCodec = codec;
                const int width = stream.value(QStringLiteral("width")).toInt();
                const int height = stream.value(QStringLiteral("height")).toInt();
                if (width > 0) info.Width = width;
                if (height > 0) info.Height = height;
                ParseFrameRate(stream.value(QStringLiteral("avg_frame_rate")).toString(), info);
                if (!info.FrameRateNumerator.has_value())
                    ParseFrameRate(stream.value(QStringLiteral("r_frame_rate")).toString(), info);
            }
            if (!info.CreationTime.has_value())
            {
                const QString valueCreation = stream.value(QStringLiteral("tags")).toObject().value(QStringLiteral("creation_time")).toString();
                if (!valueCreation.isEmpty()) info.CreationTime = valueCreation;
            }
        }
        else if (type == QStringLiteral("audio"))
        {
            hasAudio = true;
            if (index >= 0) info.AudioStreamIndices.append(index);
            if (!info.AudioCodec.has_value())
            {
                const QString codec = stream.value(QStringLiteral("codec_name")).toString();
                if (!codec.isEmpty()) info.AudioCodec = codec;
            }
        }
    }
    info.HasVideo = hasVideo;
    info.HasAudio = hasAudio;
    if (!hasVideo)
    {
        const QString message = QStringLiteral("Unsupported file: ffprobe found no video stream.");
        info.ProbeStatus = EProbeStatus::Failed;
        info.ProbeError = message;
        if (error != nullptr) *error = message;
        return info;
    }
    info.ProbeStatus = EProbeStatus::Ready;
    return info;
}

QString MediaProbe::DefaultProgramPath()
{
#ifdef Q_OS_WIN
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("ffprobe.exe"));
#else
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("ffprobe"));
#endif
    return QFileInfo::exists(bundled) ? bundled : QStringLiteral("ffprobe");
}

QString MediaProbe::BuildCacheKey(const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) return {};
    const QString canonical = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    return QStringLiteral("%1|%2|%3").arg(QDir::cleanPath(canonical)).arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
}

QString MediaProbe::ProcessErrorText(const QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart)
        return QStringLiteral("ffprobe failed to start. Install ffprobe or place it beside ClipCutter.");
    if (error == QProcess::Crashed) return QStringLiteral("ffprobe crashed while inspecting the source.");
    return QStringLiteral("ffprobe process error (%1).").arg(static_cast<int>(error));
}

void MediaProbe::StartPending()
{
    while (Active_.size() < MaximumConcurrency_ && !Pending_.isEmpty())
    {
        const Request request = Pending_.dequeue();
        if (Generations_.value(request.ClipId) == request.Generation)
            StartRequest(request);
    }
}

void MediaProbe::StartRequest(const Request& request)
{
    auto* process = new QProcess(this);
    Active_.insert(process, {request});
    process->setProgram(ProgramPath_);
    process->setArguments({QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-show_format"),
                           QStringLiteral("-show_streams"), QStringLiteral("-of"), QStringLiteral("json"), request.SourcePath});
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]()
            { if (Active_.contains(process)) Active_[process].StandardOutput += process->readAllStandardOutput(); });
    connect(process, &QProcess::readyReadStandardError, this, [this, process]()
            { if (Active_.contains(process)) Active_[process].StandardError += process->readAllStandardError(); });
    connect(process, &QProcess::errorOccurred, this, [this, process](const QProcess::ProcessError error)
            {
                if (!Active_.contains(process) || Active_[process].Completed) return;
                MediaInfo info;
                info.ProbeStatus = EProbeStatus::Failed;
                info.ProbeError = ProcessErrorText(error);
                Complete(process, info, QString::fromUtf8(Active_[process].StandardError));
            });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process](const int exitCode, const QProcess::ExitStatus exitStatus)
            {
                if (!Active_.contains(process) || Active_[process].Completed) return;
                Active_[process].StandardOutput += process->readAllStandardOutput();
                Active_[process].StandardError += process->readAllStandardError();
                if (exitStatus != QProcess::NormalExit || exitCode != 0)
                {
                    MediaInfo info;
                    info.ProbeStatus = EProbeStatus::Failed;
                    info.ProbeError = QStringLiteral("ffprobe exited with code %1: %2")
                                          .arg(exitCode).arg(QString::fromUtf8(Active_[process].StandardError).trimmed());
                    Complete(process, info, QString::fromUtf8(Active_[process].StandardError));
                    return;
                }
                QString parseError;
                MediaInfo info = ParseJson(Active_[process].StandardOutput, &parseError);
                Complete(process, info, parseError.isEmpty() ? QString::fromUtf8(Active_[process].StandardError) : parseError);
            });
    process->start();
}

void MediaProbe::Complete(QProcess* process, MediaInfo info, const QString& diagnostics)
{
    auto iterator = Active_.find(process);
    if (iterator == Active_.end() || iterator->Completed) return;
    iterator->Completed = true;
    const Request request = iterator->RequestValue;
    if (info.ProbeStatus == EProbeStatus::Ready && !request.CacheKey.isEmpty()) Cache_.insert(request.CacheKey, info);
    Active_.erase(iterator);
    process->disconnect(this);
    process->deleteLater();
    if (Generations_.value(request.ClipId) == request.Generation)
        emit ProbeCompleted({request.ClipId, request.SourcePath, info, diagnostics});
    StartPending();
}
} // namespace ClipCutter
