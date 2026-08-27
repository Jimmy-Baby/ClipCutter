#ifndef CLIPCUTTER_CORE_MEDIA_MEDIAINFO_H
#define CLIPCUTTER_CORE_MEDIA_MEDIAINFO_H

#include <QString>

#include <chrono>
#include <optional>

namespace clipcutter
{
enum class ProbeStatus
{
    NotProbed,
    Probing,
    Ready,
    Failed
};

struct MediaInfo
{
    std::optional<std::chrono::milliseconds> duration;
    std::optional<QString> containerName;
    std::optional<QString> videoCodec;
    std::optional<QString> audioCodec;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<double> frameRate;
    ProbeStatus probeStatus = ProbeStatus::NotProbed;
    std::optional<QString> probeError;
};
}

#endif
