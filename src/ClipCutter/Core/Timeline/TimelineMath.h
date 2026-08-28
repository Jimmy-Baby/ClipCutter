#ifndef CLIPCUTTER_CORE_TIMELINE_TIMELINEMATH_H
#define CLIPCUTTER_CORE_TIMELINE_TIMELINEMATH_H

#include "Core/Clip/TimeRange.h"
#include "Core/Media/MediaInfo.h"

#include <QString>

#include <chrono>
#include <optional>

namespace ClipCutter
{
class TimelineCoordinateMapper final
{
public:
    using Milliseconds = std::chrono::milliseconds;

    void SetDuration(std::optional<Milliseconds> duration);
    void SetViewportWidth(double width);
    void SetZoomFactor(double factor, std::optional<Milliseconds> anchor = std::nullopt);
    void ZoomBy(double multiplier, std::optional<Milliseconds> anchor = std::nullopt);
    bool ZoomToSelection(const TimeRange& selection);
    void ZoomToFull();
    void ScrollTo(Milliseconds start);
    void ScrollBy(Milliseconds delta);

    std::optional<Milliseconds> Duration() const noexcept;
    Milliseconds VisibleStart() const noexcept;
    Milliseconds VisibleEnd() const noexcept;
    Milliseconds VisibleDuration() const noexcept;
    double ViewportWidth() const noexcept;
    double ZoomFactor() const noexcept;
    double MinimumZoomFactor() const noexcept;
    double MaximumZoomFactor() const noexcept;
    bool IsUsable() const noexcept;

    double TimeToPixel(Milliseconds time) const noexcept;
    Milliseconds PixelToTime(double pixel) const noexcept;

private:
    void ClampViewport();

    std::optional<Milliseconds> Duration_;
    Milliseconds VisibleStart_{0};
    double ViewportWidth_ = 1.0;
    double ZoomFactor_ = 1.0;
};

struct FrameStepResult
{
    std::chrono::milliseconds Position{0};
    bool Approximate = true;
    QString Description;
};

class FrameStepper final
{
public:
    static FrameStepResult Step(const MediaInfo& mediaInfo, std::chrono::milliseconds position,
                                qint64 frameCount,
                                std::optional<std::chrono::milliseconds> duration = std::nullopt);
    static std::optional<long double> FrameDurationMilliseconds(const MediaInfo& mediaInfo) noexcept;
};

class LoopRangeController final
{
public:
    void SetEnabled(bool enabled) noexcept;
    bool IsEnabled() const noexcept;
    void SetRange(std::optional<TimeRange> range);
    bool IsLoopable() const noexcept;
    std::optional<std::chrono::milliseconds> Evaluate(std::chrono::milliseconds position);
    void ResetSeekGuard() noexcept;

private:
    bool Enabled_ = false;
    bool SeekGuard_ = false;
    std::optional<TimeRange> Range_;
};
} // namespace ClipCutter

#endif
