#ifndef FFMPEG_H
#define FFMPEG_H

#include <QString>

#include "app/models/ClipQueueModel.h"

namespace clipcutter
{
enum class ReEncodeQuality
{
    Copy,
    Lowest,
    Low,
    Medium,
    High,
    Highest
};

namespace FFmpeg
{
    void ProcessSegment(
        const ExportSegment& segment,
        const QString& outputDirectory,
        ReEncodeQuality quality,
        bool showFfmpeg);
    bool FFmpegTest();
}
}

#endif // FFMPEG_H
