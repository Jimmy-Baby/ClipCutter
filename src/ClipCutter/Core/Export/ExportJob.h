#ifndef CLIPCUTTER_CORE_EXPORT_EXPORTJOB_H
#define CLIPCUTTER_CORE_EXPORT_EXPORTJOB_H

#include <QString>
#include <QUuid>

#include <chrono>
#include <optional>

namespace ClipCutter
{
enum class EEncodingQuality
{
    Copy,
    Lowest,
    Low,
    Medium,
    High,
    Highest
};

struct ExportJob
{
    QUuid JobId = QUuid::createUuid();
    QUuid ClipId;
    QUuid SegmentId;
    QString SourcePath;
    QString FinalOutputPath;
    QString TemporaryOutputPath;
    std::chrono::milliseconds StartTime{0};
    std::optional<std::chrono::milliseconds> Duration;
    EEncodingQuality EncodingQuality = EEncodingQuality::Copy;
    bool CopyMetadata = true;
    bool SkipRequested = false;
    QString DisplayName;
};
} // namespace ClipCutter

#endif
