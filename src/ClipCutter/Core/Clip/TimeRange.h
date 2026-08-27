#ifndef CLIPCUTTER_CORE_CLIP_TIMERANGE_H
#define CLIPCUTTER_CORE_CLIP_TIMERANGE_H

#include <QString>

#include <chrono>
#include <optional>

namespace ClipCutter
{
class TimeRange
{
public:
    using Milliseconds = std::chrono::milliseconds;

    TimeRange() = default;

    static std::optional<TimeRange> Create(Milliseconds start, Milliseconds end,
                                           std::optional<Milliseconds> mediaDuration = std::nullopt,
                                           bool requireNonEmpty = false, QString* error = nullptr);
    static TimeRange Clamped(Milliseconds start, Milliseconds end, Milliseconds mediaDuration);

    Milliseconds Start() const noexcept;
    Milliseconds End() const noexcept;
    Milliseconds Duration() const noexcept;
    bool IsValid(std::optional<Milliseconds> mediaDuration = std::nullopt, bool requireNonEmpty = false,
                 QString* error = nullptr) const;

    friend bool operator==(const TimeRange& left, const TimeRange& right) noexcept;
    friend bool operator!=(const TimeRange& left, const TimeRange& right) noexcept;

private:
    TimeRange(Milliseconds start, Milliseconds end) noexcept;

    Milliseconds Start_{0};
    Milliseconds End_{0};
};
} // namespace ClipCutter

#endif
