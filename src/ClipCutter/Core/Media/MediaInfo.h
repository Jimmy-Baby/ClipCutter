#ifndef CLIPCUTTER_CORE_MEDIA_MEDIAINFO_H
#define CLIPCUTTER_CORE_MEDIA_MEDIAINFO_H

#include <QString>
#include <QVector>

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
    std::optional<bool> HasVideo;
    std::optional<QString> VideoCodec;
    std::optional<QString> AudioCodec;
    std::optional<int> Width;
    std::optional<int> Height;
    std::optional<int> FrameRateNumerator;
    std::optional<int> FrameRateDenominator;
    std::optional<bool> HasAudio;
    QVector<int> VideoStreamIndices;
    QVector<int> AudioStreamIndices;
    std::optional<QString> CreationTime;
    EProbeStatus ProbeStatus = EProbeStatus::NotProbed;
    std::optional<QString> ProbeError;

    bool IsReliableForExport() const noexcept
    {
        return ProbeStatus == EProbeStatus::Ready && Duration.has_value() && Duration->count() > 0 &&
               HasVideo.value_or(false) && !VideoStreamIndices.isEmpty();
    }
};
} // namespace ClipCutter

#endif
