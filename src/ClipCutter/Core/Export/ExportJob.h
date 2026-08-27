#ifndef CLIPCUTTER_CORE_EXPORT_EXPORTJOB_H
#define CLIPCUTTER_CORE_EXPORT_EXPORTJOB_H

#include "Core/Media/MediaInfo.h"
#include "Core/Export/OutputPathPlanner.h"

#include <QString>
#include <QUuid>

#include <chrono>
#include <optional>

namespace ClipCutter
{
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
    QString OutputProfileId = QStringLiteral("fast-copy");
    MediaInfo SourceMediaInfo;
    bool CopyMetadata = true;
    ECollisionPolicy CollisionPolicy = ECollisionPolicy::Ask;
    bool SkipRequested = false;
    QString DisplayName;
};
} // namespace ClipCutter

#endif
