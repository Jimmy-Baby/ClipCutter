#include "core/clip/TimeRange.h"

#include <algorithm>

namespace clipcutter
{
namespace
{
bool fail(QString* error, const QString& message)
{
    if (error != nullptr)
    {
        *error = message;
    }
    return false;
}
}

TimeRange::TimeRange(Milliseconds start, Milliseconds end) noexcept
    : m_start(start), m_end(end)
{
}

std::optional<TimeRange> TimeRange::create(
    Milliseconds start,
    Milliseconds end,
    std::optional<Milliseconds> mediaDuration,
    bool requireNonEmpty,
    QString* error)
{
    const TimeRange range(start, end);
    if (!range.isValid(mediaDuration, requireNonEmpty, error))
    {
        return std::nullopt;
    }
    return range;
}

TimeRange TimeRange::clamped(Milliseconds start, Milliseconds end, Milliseconds mediaDuration)
{
    const Milliseconds nonNegativeDuration = std::max(Milliseconds{0}, mediaDuration);
    const Milliseconds clampedStart = std::clamp(start, Milliseconds{0}, nonNegativeDuration);
    const Milliseconds clampedEnd = std::clamp(end, clampedStart, nonNegativeDuration);
    return TimeRange(clampedStart, clampedEnd);
}

TimeRange::Milliseconds TimeRange::start() const noexcept
{
    return m_start;
}

TimeRange::Milliseconds TimeRange::end() const noexcept
{
    return m_end;
}

TimeRange::Milliseconds TimeRange::duration() const noexcept
{
    return m_end - m_start;
}

bool TimeRange::isValid(
    std::optional<Milliseconds> mediaDuration,
    bool requireNonEmpty,
    QString* error) const
{
    if (error != nullptr)
    {
        error->clear();
    }
    if (m_start.count() < 0 || m_end.count() < 0)
    {
        return fail(error, QStringLiteral("Time values cannot be negative."));
    }
    if (m_end < m_start)
    {
        return fail(error, QStringLiteral("End time cannot be before start time."));
    }
    if (requireNonEmpty && m_end == m_start)
    {
        return fail(error, QStringLiteral("The selected range must not be empty."));
    }
    if (mediaDuration.has_value())
    {
        if (mediaDuration->count() < 0)
        {
            return fail(error, QStringLiteral("Media duration cannot be negative."));
        }
        if (m_end > *mediaDuration)
        {
            return fail(error, QStringLiteral("The selected range exceeds the source duration."));
        }
    }
    return true;
}

bool operator==(const TimeRange& left, const TimeRange& right) noexcept
{
    return left.m_start == right.m_start && left.m_end == right.m_end;
}

bool operator!=(const TimeRange& left, const TimeRange& right) noexcept
{
    return !(left == right);
}
}
