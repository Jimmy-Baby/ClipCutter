#include "Core/Timeline/TimelineMath.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ClipCutter
{
namespace
{
constexpr double KMaximumZoom = 1'000'000.0;

qint64 RoundedMilliseconds(const long double value) noexcept
{
    if (!std::isfinite(value)) return 0;
    const long double low = static_cast<long double>(std::numeric_limits<qint64>::min());
    const long double high = static_cast<long double>(std::numeric_limits<qint64>::max());
    return static_cast<qint64>(std::llround(std::clamp(value, low, high)));
}
} // namespace

void TimelineCoordinateMapper::SetDuration(const std::optional<Milliseconds> duration)
{
    Duration_ = duration.has_value() && duration->count() > 0 ? duration : std::nullopt;
    ClampViewport();
}

void TimelineCoordinateMapper::SetViewportWidth(const double width)
{
    ViewportWidth_ = std::isfinite(width) && width > 0.0 ? width : 1.0;
}

void TimelineCoordinateMapper::SetZoomFactor(const double factor, const std::optional<Milliseconds> anchor)
{
    if (!IsUsable())
    {
        ZoomFactor_ = 1.0;
        VisibleStart_ = Milliseconds{0};
        return;
    }
    const Milliseconds oldVisible = VisibleDuration();
    const Milliseconds anchorTime = anchor.value_or(VisibleStart_ + oldVisible / 2);
    const long double relative = oldVisible.count() > 0
                                     ? static_cast<long double>((anchorTime - VisibleStart_).count()) /
                                           static_cast<long double>(oldVisible.count())
                                     : 0.5L;
    ZoomFactor_ = std::clamp(std::isfinite(factor) ? factor : 1.0, MinimumZoomFactor(), MaximumZoomFactor());
    const Milliseconds nextVisible = VisibleDuration();
    VisibleStart_ = anchorTime - Milliseconds{RoundedMilliseconds(relative * nextVisible.count())};
    ClampViewport();
}

void TimelineCoordinateMapper::ZoomBy(const double multiplier, const std::optional<Milliseconds> anchor)
{
    if (!std::isfinite(multiplier) || multiplier <= 0.0) return;
    SetZoomFactor(ZoomFactor_ * multiplier, anchor);
}

bool TimelineCoordinateMapper::ZoomToSelection(const TimeRange& selection)
{
    if (!IsUsable() || !selection.IsValid(Duration_, true)) return false;
    const qint64 selected = selection.Duration().count();
    if (selected <= 0) return false;
    ZoomFactor_ = std::clamp(static_cast<double>(Duration_->count()) / static_cast<double>(selected),
                             MinimumZoomFactor(), MaximumZoomFactor());
    VisibleStart_ = selection.Start();
    ClampViewport();
    return true;
}

void TimelineCoordinateMapper::ZoomToFull()
{
    ZoomFactor_ = 1.0;
    VisibleStart_ = Milliseconds{0};
}

void TimelineCoordinateMapper::ScrollTo(const Milliseconds start)
{
    VisibleStart_ = start;
    ClampViewport();
}

void TimelineCoordinateMapper::ScrollBy(const Milliseconds delta)
{
    const qint64 current = VisibleStart_.count();
    const qint64 amount = delta.count();
    qint64 target = 0;
    if (amount > 0 && current > std::numeric_limits<qint64>::max() - amount)
        target = std::numeric_limits<qint64>::max();
    else if (amount < 0 && current < std::numeric_limits<qint64>::min() - amount)
        target = std::numeric_limits<qint64>::min();
    else
        target = current + amount;
    ScrollTo(Milliseconds{target});
}

std::optional<TimelineCoordinateMapper::Milliseconds> TimelineCoordinateMapper::Duration() const noexcept { return Duration_; }
TimelineCoordinateMapper::Milliseconds TimelineCoordinateMapper::VisibleStart() const noexcept { return VisibleStart_; }
TimelineCoordinateMapper::Milliseconds TimelineCoordinateMapper::VisibleEnd() const noexcept
{
    return VisibleStart_ + VisibleDuration();
}

TimelineCoordinateMapper::Milliseconds TimelineCoordinateMapper::VisibleDuration() const noexcept
{
    if (!IsUsable()) return Milliseconds{0};
    const long double value = static_cast<long double>(Duration_->count()) / static_cast<long double>(ZoomFactor_);
    return Milliseconds{std::clamp<qint64>(RoundedMilliseconds(value), 1, Duration_->count())};
}

double TimelineCoordinateMapper::ViewportWidth() const noexcept { return ViewportWidth_; }
double TimelineCoordinateMapper::ZoomFactor() const noexcept { return ZoomFactor_; }
double TimelineCoordinateMapper::MinimumZoomFactor() const noexcept { return 1.0; }
double TimelineCoordinateMapper::MaximumZoomFactor() const noexcept
{
    return IsUsable() ? std::min(KMaximumZoom, static_cast<double>(std::max<qint64>(1, Duration_->count()))) : 1.0;
}

bool TimelineCoordinateMapper::IsUsable() const noexcept
{
    return Duration_.has_value() && Duration_->count() > 0 && ViewportWidth_ > 0.0;
}

double TimelineCoordinateMapper::TimeToPixel(const Milliseconds time) const noexcept
{
    if (!IsUsable()) return 0.0;
    const long double offset = static_cast<long double>((time - VisibleStart_).count());
    const long double visible = static_cast<long double>(VisibleDuration().count());
    return static_cast<double>(offset * static_cast<long double>(ViewportWidth_) / visible);
}

TimelineCoordinateMapper::Milliseconds TimelineCoordinateMapper::PixelToTime(const double pixel) const noexcept
{
    if (!IsUsable() || !std::isfinite(pixel)) return Milliseconds{0};
    const long double ratio = static_cast<long double>(pixel) / static_cast<long double>(ViewportWidth_);
    const long double value = static_cast<long double>(VisibleStart_.count()) +
                              ratio * static_cast<long double>(VisibleDuration().count());
    return Milliseconds{std::clamp<qint64>(RoundedMilliseconds(value), 0, Duration_->count())};
}

void TimelineCoordinateMapper::ClampViewport()
{
    if (!IsUsable())
    {
        VisibleStart_ = Milliseconds{0};
        ZoomFactor_ = 1.0;
        return;
    }
    ZoomFactor_ = std::clamp(ZoomFactor_, MinimumZoomFactor(), MaximumZoomFactor());
    const qint64 maximumStart = std::max<qint64>(0, Duration_->count() - VisibleDuration().count());
    VisibleStart_ = Milliseconds{std::clamp<qint64>(VisibleStart_.count(), 0, maximumStart)};
}

std::optional<long double> FrameStepper::FrameDurationMilliseconds(const MediaInfo& mediaInfo) noexcept
{
    if (!mediaInfo.FrameRateNumerator.has_value() || !mediaInfo.FrameRateDenominator.has_value() ||
        *mediaInfo.FrameRateNumerator <= 0 || *mediaInfo.FrameRateDenominator <= 0)
        return std::nullopt;
    return 1000.0L * static_cast<long double>(*mediaInfo.FrameRateDenominator) /
           static_cast<long double>(*mediaInfo.FrameRateNumerator);
}

FrameStepResult FrameStepper::Step(const MediaInfo& mediaInfo, const std::chrono::milliseconds position,
                                   const qint64 frameCount,
                                   const std::optional<std::chrono::milliseconds> duration)
{
    FrameStepResult result;
    const auto frameDuration = FrameDurationMilliseconds(mediaInfo);
    const long double delta = static_cast<long double>(frameCount) * frameDuration.value_or(40.0L);
    qint64 target = RoundedMilliseconds(static_cast<long double>(position.count()) + delta);
    target = std::max<qint64>(0, target);
    if (duration.has_value()) target = std::min(target, std::max<qint64>(0, duration->count()));
    result.Position = std::chrono::milliseconds{target};
    result.Approximate = mediaInfo.VariableFrameRate.value_or(!frameDuration.has_value());
    result.Description = result.Approximate
                             ? QStringLiteral("Approximate frame step; preview seeking is not frame accurate.")
                             : QStringLiteral("Frame interval uses the source rational frame rate; preview seeking remains approximate.");
    return result;
}

void LoopRangeController::SetEnabled(const bool enabled) noexcept
{
    Enabled_ = enabled;
    SeekGuard_ = false;
}

bool LoopRangeController::IsEnabled() const noexcept { return Enabled_; }

void LoopRangeController::SetRange(std::optional<TimeRange> range)
{
    if (range.has_value() && !range->IsValid(std::nullopt, true)) range.reset();
    Range_ = std::move(range);
    SeekGuard_ = false;
}

bool LoopRangeController::IsLoopable() const noexcept
{
    return Enabled_ && Range_.has_value() && Range_->Duration().count() > 0;
}

std::optional<std::chrono::milliseconds> LoopRangeController::Evaluate(const std::chrono::milliseconds position)
{
    if (!IsLoopable()) return std::nullopt;
    if (position < Range_->End()) SeekGuard_ = false;
    if (position < Range_->End() || SeekGuard_) return std::nullopt;
    SeekGuard_ = true;
    return Range_->Start();
}

void LoopRangeController::ResetSeekGuard() noexcept { SeekGuard_ = false; }
} // namespace ClipCutter
