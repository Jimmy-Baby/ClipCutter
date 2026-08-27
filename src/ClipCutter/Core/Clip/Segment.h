#ifndef CLIPCUTTER_CORE_CLIP_SEGMENT_H
#define CLIPCUTTER_CORE_CLIP_SEGMENT_H

#include "Core/Clip/TimeRange.h"
#include "Core/Export/ExportState.h"

#include <QString>
#include <QUuid>

#include <optional>

namespace ClipCutter
{
struct Segment
{
    QUuid Id = QUuid::createUuid();
    TimeRange Range;
    QString OutputBaseName;
    QString OutputExtension;
    std::optional<QString> Prefix;
    bool Skipped = false;
    QString ExportProfileId;
    EExportState ExportState = EExportState::Pending;
    std::optional<double> ExportProgress;
    QString ExportLog;
    bool ExportLocked = false;
    bool TrimRangeUserEdited = false;

    QString OutputFileName() const;
};
} // namespace ClipCutter

#endif
