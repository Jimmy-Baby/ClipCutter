#include "Core/Clip/TimeRange.h"

#include <algorithm>

namespace ClipCutter
{
namespace
{
bool Fail(QString* error, const QString& message)
{
    if (error != nullptr)
    {
        *error = message;
    }

    return false;
}
} // namespace

TimeRange::TimeRange(Milliseconds start, Milliseconds end) noexcept : Start_(start), End_(end) {}

std::optional<TimeRange> TimeRange::Create(Milliseconds start, Milliseconds end,
                                           std::optional<Milliseconds> mediaDuration, bool requireNonEmpty,
                                           QString* error)
{
    const TimeRange range(start, end);

    if (!range.IsValid(mediaDuration, requireNonEmpty, error))
    {
        return std::nullopt;
    }

    return range;
}

TimeRange TimeRange::Clamped(Milliseconds start, Milliseconds end, Milliseconds mediaDuration)
{
    const Milliseconds nonNegativeDuration = std::max(Milliseconds{0}, mediaDuration);
    const Milliseconds clampedStart = std::clamp(start, Milliseconds{0}, nonNegativeDuration);
    const Milliseconds clampedEnd = std::clamp(end, clampedStart, nonNegativeDuration);

    return TimeRange(clampedStart, clampedEnd);
}

TimeRange::Milliseconds TimeRange::Start() const noexcept
{
    return Start_;
}

TimeRange::Milliseconds TimeRange::End() const noexcept
{
    return End_;
}

TimeRange::Milliseconds TimeRange::Duration() const noexcept
{
    return End_ - Start_;
}

bool TimeRange::IsValid(std::optional<Milliseconds> mediaDuration, bool requireNonEmpty, QString* error) const
{
    if (error != nullptr)
    {
        error->clear();
    }

    if (Start_.count() < 0 || End_.count() < 0)
    {
        return Fail(error, QStringLiteral("Time values cannot be negative."));
    }

    if (End_ < Start_)
    {
        return Fail(error, QStringLiteral("End time cannot be before start time."));
    }

    if (requireNonEmpty && End_ == Start_)
    {
        return Fail(error, QStringLiteral("The selected range must not be empty."));
    }

    if (mediaDuration.has_value())
    {
        if (mediaDuration->count() < 0)
        {
            return Fail(error, QStringLiteral("Media duration cannot be negative."));
        }

        if (End_ > *mediaDuration)
        {
            return Fail(error, QStringLiteral("The selected range exceeds the source duration."));
        }
    }

    return true;
}

bool operator==(const TimeRange& left, const TimeRange& right) noexcept
{
    return left.Start_ == right.Start_ && left.End_ == right.End_;
}

bool operator!=(const TimeRange& left, const TimeRange& right) noexcept
{
    return !(left == right);
}
} // namespace ClipCutter
