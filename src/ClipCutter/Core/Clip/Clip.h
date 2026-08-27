#ifndef CLIPCUTTER_CORE_CLIP_CLIP_H
#define CLIPCUTTER_CORE_CLIP_CLIP_H

#include "Core/Clip/Segment.h"
#include "Core/Media/MediaInfo.h"

#include <QString>
#include <QUuid>
#include <QVector>

namespace ClipCutter
{
struct Clip
{
    QUuid Id = QUuid::createUuid();
    QString SourcePath;
    QString OriginalFileName;
    MediaInfo MediaInformation;
    QVector<Segment> Segments;
};
} // namespace ClipCutter

#endif
