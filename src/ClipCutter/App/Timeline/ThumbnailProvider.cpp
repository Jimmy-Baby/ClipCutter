#include "App/Timeline/ThumbnailProvider.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QPointer>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>

namespace ClipCutter
{
FfmpegThumbnailProvider::FfmpegThumbnailProvider(QObject* parent, QString ffmpegPath,
                                                 const int maximumConcurrency,
                                                 const qint64 maximumCacheBytes)
    : ThumbnailProvider(parent), FfmpegPath_(std::move(ffmpegPath)),
      MaximumConcurrency_(std::max(1, maximumConcurrency)), MaximumCacheBytes_(std::max<qint64>(0, maximumCacheBytes))
{
    if (FfmpegPath_.isEmpty()) FfmpegPath_ = DefaultProgramPath();
    QDir().mkpath(CacheDirectory());
}

FfmpegThumbnailProvider::~FfmpegThumbnailProvider()
{
    Cancel();
}

quint64 FfmpegThumbnailProvider::RequestVisible(const QString& sourcePath, const TimeRange& visibleRange,
                                                const QSize& thumbnailSize, int thumbnailCount)
{
    Cancel();
    const quint64 requestGeneration = Generation_;
    if (!visibleRange.IsValid(std::nullopt, true) || !thumbnailSize.isValid() || thumbnailCount <= 0)
        return requestGeneration;
    const QString fingerprint = BuildSourceFingerprint(sourcePath);
    if (fingerprint.isEmpty()) return requestGeneration;
    thumbnailCount = std::clamp(thumbnailCount, 1, 256);
    QDir().mkpath(CacheDirectory());
    for (int index = 0; index < thumbnailCount; ++index)
    {
        const long double fraction = (static_cast<long double>(index) + 0.5L) /
                                     static_cast<long double>(thumbnailCount);
        const qint64 timestamp = visibleRange.Start().count() +
                                 static_cast<qint64>(fraction * visibleRange.Duration().count());
        const QString key = BuildCacheKey(fingerprint, timestamp, thumbnailSize);
        const QString cachePath = QDir(CacheDirectory()).absoluteFilePath(key + QStringLiteral(".jpg"));
        QImageReader reader(cachePath);
        if (QFileInfo::exists(cachePath) && reader.canRead())
        {
            const QPointer<FfmpegThumbnailProvider> guard(this);
            QThreadPool::globalInstance()->start([guard, requestGeneration, timestamp, cachePath]
            {
                QImageReader backgroundReader(cachePath);
                const QImage image = backgroundReader.read();
                if (guard.isNull()) return;
                QMetaObject::invokeMethod(guard.data(), [guard, requestGeneration, timestamp, image]
                {
                    if (!guard.isNull() && guard->IsCurrentGeneration(requestGeneration) && !image.isNull())
                        emit guard->ThumbnailReady(requestGeneration, timestamp, image);
                }, Qt::QueuedConnection);
            });
            continue;
        }
        if (QFileInfo::exists(cachePath)) QFile::remove(cachePath);
        Pending_.enqueue({requestGeneration, sourcePath, fingerprint, timestamp, thumbnailSize, cachePath});
    }
    StartPending();
    return requestGeneration;
}

void FfmpegThumbnailProvider::Cancel()
{
    ++Generation_;
    Pending_.clear();
    const QList<QProcess*> processes = Active_.keys();
    for (QProcess* process : processes) process->kill();
}

bool FfmpegThumbnailProvider::ClearCache(QString* error)
{
    Cancel();
    QDir directory(CacheDirectory());
    bool ok = true;
    for (const QFileInfo& file : directory.entryInfoList({QStringLiteral("*.jpg")}, QDir::Files))
        ok = QFile::remove(file.absoluteFilePath()) && ok;
    if (!ok && error != nullptr) *error = QStringLiteral("Unable to remove one or more thumbnail cache files.");
    return ok;
}

quint64 FfmpegThumbnailProvider::Generation() const noexcept { return Generation_; }
bool FfmpegThumbnailProvider::IsCurrentGeneration(const quint64 generation) const noexcept
{
    return generation == Generation_;
}

QString FfmpegThumbnailProvider::CacheDirectory() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .absoluteFilePath(QStringLiteral("timeline-thumbnails"));
}

QString FfmpegThumbnailProvider::BuildSourceFingerprint(const QString& sourcePath)
{
    const QFileInfo info(sourcePath);
    if (!info.exists() || !info.isFile()) return {};
    const QString canonical = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    return QStringLiteral("%1|%2|%3").arg(QDir::cleanPath(canonical)).arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch());
}

QString FfmpegThumbnailProvider::BuildCacheKey(const QString& sourceFingerprint, const qint64 timestampMs,
                                               const QSize& size)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(sourceFingerprint.toUtf8());
    hash.addData(QByteArrayView("\0", 1));
    hash.addData(QByteArray::number(timestampMs));
    hash.addData(QByteArrayView("\0", 1));
    hash.addData(QByteArray::number(size.width()));
    hash.addData(QByteArrayView("x", 1));
    hash.addData(QByteArray::number(size.height()));
    return QString::fromLatin1(hash.result().toHex());
}

void FfmpegThumbnailProvider::StartPending()
{
    while (Active_.size() < MaximumConcurrency_ && !Pending_.isEmpty())
    {
        Task task = Pending_.dequeue();
        if (!IsCurrentGeneration(task.Generation)) continue;
        StartTask(std::move(task));
    }
}

void FfmpegThumbnailProvider::StartTask(Task task)
{
    auto* process = new QProcess(this);
    Active_.insert(process, task);
    connect(process, &QProcess::finished, this,
            [this, process](const int exitCode, QProcess::ExitStatus) { FinishTask(process, exitCode); });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error)
            {
                if (error == QProcess::FailedToStart) FinishTask(process, -1);
            });
    const QString timestamp = QString::number(static_cast<double>(task.TimestampMs) / 1000.0, 'f', 3);
    const QString scale = QStringLiteral("scale=%1:%2:force_original_aspect_ratio=decrease,pad=%1:%2:(ow-iw)/2:(oh-ih)/2")
                              .arg(task.Size.width()).arg(task.Size.height());
    process->setProgram(FfmpegPath_);
    process->setArguments({QStringLiteral("-nostdin"), QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
                           QStringLiteral("error"), QStringLiteral("-ss"), timestamp, QStringLiteral("-i"),
                           task.SourcePath, QStringLiteral("-frames:v"), QStringLiteral("1"),
                           QStringLiteral("-vf"), scale, QStringLiteral("-q:v"), QStringLiteral("4"),
                           QStringLiteral("-y"), task.CachePath});
    process->start();
}

void FfmpegThumbnailProvider::FinishTask(QProcess* process, const int exitCode)
{
    const auto found = Active_.find(process);
    if (found == Active_.end()) return;
    const Task task = found.value();
    Active_.erase(found);
    const QString processError = QString::fromUtf8(process->readAllStandardError()).trimmed();
    process->deleteLater();
    if (IsCurrentGeneration(task.Generation))
    {
        QImageReader reader(task.CachePath);
        const QImage image = exitCode == 0 && reader.canRead() ? reader.read() : QImage();
        if (!image.isNull()) emit ThumbnailReady(task.Generation, task.TimestampMs, image);
        else
        {
            QFile::remove(task.CachePath);
            emit RequestFailed(task.Generation, task.TimestampMs,
                               processError.isEmpty() ? QStringLiteral("FFmpeg could not extract this thumbnail.")
                                                      : processError);
        }
    }
    StartPending();
    if (Pending_.isEmpty() && Active_.isEmpty()) PruneCache();
}

void FfmpegThumbnailProvider::PruneCache()
{
    if (MaximumCacheBytes_ <= 0) return;
    const QString cacheDirectory = CacheDirectory();
    const qint64 maximumBytes = MaximumCacheBytes_;
    QThreadPool::globalInstance()->start([cacheDirectory, maximumBytes]
    {
        QDir directory(cacheDirectory);
        const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.jpg")}, QDir::Files,
                                                             QDir::Time | QDir::Reversed);
        qint64 total = 0;
        for (const QFileInfo& file : files) total += file.size();
        for (const QFileInfo& file : files)
        {
            if (total <= maximumBytes) break;
            const qint64 size = file.size();
            if (QFile::remove(file.absoluteFilePath())) total -= size;
        }
    });
}

QString FfmpegThumbnailProvider::DefaultProgramPath() const
{
#ifdef Q_OS_WIN
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("ffmpeg.exe"));
#else
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("ffmpeg"));
#endif
    return QFileInfo::exists(bundled) ? bundled : QStringLiteral("ffmpeg");
}
} // namespace ClipCutter
