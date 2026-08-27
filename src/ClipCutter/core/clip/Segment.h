#ifndef CLIPCUTTER_CORE_CLIP_SEGMENT_H
#define CLIPCUTTER_CORE_CLIP_SEGMENT_H

#include "core/clip/TimeRange.h"

#include <QString>
#include <QUuid>

#include <optional>

namespace clipcutter
{
enum class ExportStatus
{
    Pending,
    Processing,
    Complete,
    Failed
};

struct Segment
{
    QUuid id = QUuid::createUuid();
    TimeRange range;
    QString outputBaseName;
    QString outputExtension;
    std::optional<QString> prefix;
    bool skipped = false;
    QString exportProfileId;
    ExportStatus exportStatus = ExportStatus::Pending;

    QString outputFileName() const;
};
}

#endif
