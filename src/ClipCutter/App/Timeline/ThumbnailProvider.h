#ifndef CLIPCUTTER_APP_TIMELINE_THUMBNAILPROVIDER_H
#define CLIPCUTTER_APP_TIMELINE_THUMBNAILPROVIDER_H

#include "Core/Clip/TimeRange.h"

#include <QHash>
#include <QImage>
#include <QObject>
#include <QQueue>
#include <QSize>

class QProcess;

namespace ClipCutter
{
class ThumbnailProvider : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~ThumbnailProvider() override = default;

    virtual quint64 RequestVisible(const QString& sourcePath, const TimeRange& visibleRange,
                                   const QSize& thumbnailSize, int thumbnailCount) = 0;
    virtual void Cancel() = 0;
    virtual bool ClearCache(QString* error = nullptr) = 0;

signals:
    void ThumbnailReady(quint64 generation, qint64 timestampMs, const QImage& image);
    void RequestFailed(quint64 generation, qint64 timestampMs, const QString& error);
};

class FfmpegThumbnailProvider final : public ThumbnailProvider
{
    Q_OBJECT

public:
    explicit FfmpegThumbnailProvider(QObject* parent = nullptr, QString ffmpegPath = {},
                                     int maximumConcurrency = 2, qint64 maximumCacheBytes = 256LL * 1024 * 1024);
    ~FfmpegThumbnailProvider() override;

    quint64 RequestVisible(const QString& sourcePath, const TimeRange& visibleRange,
                           const QSize& thumbnailSize, int thumbnailCount) override;
    void Cancel() override;
    bool ClearCache(QString* error = nullptr) override;

    quint64 Generation() const noexcept;
    bool IsCurrentGeneration(quint64 generation) const noexcept;
    QString CacheDirectory() const;
    static QString BuildSourceFingerprint(const QString& sourcePath);
    static QString BuildCacheKey(const QString& sourceFingerprint, qint64 timestampMs, const QSize& size);

private:
    struct Task
    {
        quint64 Generation = 0;
        QString SourcePath;
        QString SourceFingerprint;
        qint64 TimestampMs = 0;
        QSize Size;
        QString CachePath;
    };

    void StartPending();
    void StartTask(Task task);
    void FinishTask(QProcess* process, int exitCode);
    void PruneCache();
    QString DefaultProgramPath() const;

    QString FfmpegPath_;
    int MaximumConcurrency_ = 2;
    qint64 MaximumCacheBytes_ = 0;
    quint64 Generation_ = 0;
    QQueue<Task> Pending_;
    QHash<QProcess*, Task> Active_;
};
} // namespace ClipCutter

#endif
