#ifndef FFMPEG_H
#define FFMPEG_H

#include <QString>

#include "queueitem.h"

namespace FFmpeg
{
    void ProcessQueueItem(const QueueItem* item, const QString& outputDirectory, EReEncodeQuality quality, bool showFfmpeg);
    bool FFmpegTest();
}

#endif // FFMPEG_H
