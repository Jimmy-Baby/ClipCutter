#ifndef CLIPCUTTER_CORE_MEDIA_MEDIAPROBE_H
#define CLIPCUTTER_CORE_MEDIA_MEDIAPROBE_H

#include "Core/Media/MediaInfo.h"

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QUuid>

namespace ClipCutter
{
struct MediaProbeResult
{
    QUuid ClipId;
    QString SourcePath;
    MediaInfo Info;
    QString Diagnostics;
};

class MediaProbe final : public QObject
{
    Q_OBJECT

public:
    explicit MediaProbe(QObject* parent = nullptr, QString ffprobePath = {}, int maximumConcurrency = 2);

    void Probe(const QUuid& clipId, const QString& sourcePath);
    void Cancel(const QUuid& clipId);
    void ClearCache();
    int MaximumConcurrency() const noexcept;
    void SetMaximumConcurrency(int count);

    static MediaInfo ParseJson(const QByteArray& json, QString* error = nullptr);

signals:
    void ProbeStarted(const QUuid& clipId);
    void ProbeCompleted(const ClipCutter::MediaProbeResult& result);

private:
    struct Request
    {
        QUuid ClipId;
        QString SourcePath;
        QString CacheKey;
        quint64 Generation = 0;
    };

    struct ActiveProbe
    {
        Request RequestValue;
        QByteArray StandardOutput;
        QByteArray StandardError;
        bool Completed = false;
    };

    static QString DefaultProgramPath();
    static QString BuildCacheKey(const QString& path);
    static QString ProcessErrorText(QProcess::ProcessError error);
    void StartPending();
    void StartRequest(const Request& request);
    void Complete(QProcess* process, MediaInfo info, const QString& diagnostics);

    QString ProgramPath_;
    int MaximumConcurrency_ = 2;
    QQueue<Request> Pending_;
    QHash<QProcess*, ActiveProbe> Active_;
    QHash<QUuid, quint64> Generations_;
    QHash<QString, MediaInfo> Cache_;
};
} // namespace ClipCutter

Q_DECLARE_METATYPE(ClipCutter::MediaProbeResult)

#endif
