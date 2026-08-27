#ifndef CLIPCUTTER_CORE_CLIP_CLIP_H
#define CLIPCUTTER_CORE_CLIP_CLIP_H

#include "core/clip/Segment.h"
#include "core/media/MediaInfo.h"

#include <QString>
#include <QUuid>
#include <QVector>

namespace clipcutter
{
struct Clip
{
    QUuid id = QUuid::createUuid();
    QString sourcePath;
    QString originalFileName;
    MediaInfo mediaInfo;
    QVector<Segment> segments;
};
}

#endif
