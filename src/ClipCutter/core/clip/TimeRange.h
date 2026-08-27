#ifndef CLIPCUTTER_CORE_CLIP_TIMERANGE_H
#define CLIPCUTTER_CORE_CLIP_TIMERANGE_H

#include <QString>

#include <chrono>
#include <optional>

namespace clipcutter
{
class TimeRange
{
public:
    using Milliseconds = std::chrono::milliseconds;

    TimeRange() = default;

    static std::optional<TimeRange> create(
        Milliseconds start,
        Milliseconds end,
        std::optional<Milliseconds> mediaDuration = std::nullopt,
        bool requireNonEmpty = false,
        QString* error = nullptr);
    static TimeRange clamped(Milliseconds start, Milliseconds end, Milliseconds mediaDuration);

    Milliseconds start() const noexcept;
    Milliseconds end() const noexcept;
    Milliseconds duration() const noexcept;
    bool isValid(
        std::optional<Milliseconds> mediaDuration = std::nullopt,
        bool requireNonEmpty = false,
        QString* error = nullptr) const;

    friend bool operator==(const TimeRange& left, const TimeRange& right) noexcept;
    friend bool operator!=(const TimeRange& left, const TimeRange& right) noexcept;

private:
    TimeRange(Milliseconds start, Milliseconds end) noexcept;

    Milliseconds m_start{0};
    Milliseconds m_end{0};
};
}

#endif
