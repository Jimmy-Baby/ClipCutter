#ifndef CLIPCUTTER_CORE_EXPORT_FFMPEGPROGRESS_H
#define CLIPCUTTER_CORE_EXPORT_FFMPEGPROGRESS_H

#include <chrono>
#include <optional>

namespace ClipCutter
{
struct FfmpegProgress
{
    std::optional<std::chrono::microseconds> OutputTime;
    bool ProgressEnded = false;
};
} // namespace ClipCutter

#endif
