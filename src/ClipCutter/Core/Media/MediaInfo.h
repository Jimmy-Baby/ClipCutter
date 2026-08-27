#ifndef CLIPCUTTER_CORE_MEDIA_MEDIAINFO_H
#define CLIPCUTTER_CORE_MEDIA_MEDIAINFO_H

#include <QString>

#include <chrono>
#include <optional>

namespace ClipCutter
{
enum class EProbeStatus
{
    NotProbed,
    Probing,
    Ready,
    Failed
};

struct MediaInfo
{
    std::optional<std::chrono::milliseconds> Duration;
    std::optional<QString> ContainerName;
    std::optional<QString> VideoCodec;
    std::optional<QString> AudioCodec;
    std::optional<int> Width;
    std::optional<int> Height;
    std::optional<double> FrameRate;
    EProbeStatus ProbeStatus = EProbeStatus::NotProbed;
    std::optional<QString> ProbeError;
};
} // namespace ClipCutter

#endif
