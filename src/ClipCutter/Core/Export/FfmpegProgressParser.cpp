#include "Core/Export/FfmpegProgressParser.h"

#include <QStringList>

#include <cmath>

namespace ClipCutter
{
QVector<FfmpegProgress> FfmpegProgressParser::Feed(const QByteArray& data)
{
    Buffer_.append(data);

    QVector<FfmpegProgress> updates;
    qsizetype newlineIndex = Buffer_.indexOf('\n');

    while (newlineIndex >= 0)
    {
        QByteArray line = Buffer_.left(newlineIndex);
        Buffer_.remove(0, newlineIndex + 1);

        if (line.endsWith('\r'))
        {
            line.chop(1);
        }

        ParseLine(line, updates);
        newlineIndex = Buffer_.indexOf('\n');
    }

    return updates;
}

void FfmpegProgressParser::Reset()
{
    Buffer_.clear();
    Current_ = {};
}

void FfmpegProgressParser::ParseLine(const QByteArray& line, QVector<FfmpegProgress>& updates)
{
    const qsizetype separatorIndex = line.indexOf('=');

    if (separatorIndex <= 0)
    {
        return;
    }

    const QByteArray key = line.left(separatorIndex).trimmed();
    const QByteArray value = line.mid(separatorIndex + 1).trimmed();

    if (key == "out_time_us" || key == "out_time_ms")
    {
        // FFmpeg's historical out_time_ms field is also expressed in microseconds.
        bool valid = false;
        const qint64 microseconds = value.toLongLong(&valid);
        Current_.OutputTime.reset();

        if (valid && microseconds >= 0)
        {
            Current_.OutputTime = std::chrono::microseconds{microseconds};
        }

        return;
    }

    if (key == "out_time")
    {
        const auto outputTime = ParseClockTime(value);
        Current_.OutputTime.reset();

        if (outputTime.has_value())
        {
            Current_.OutputTime = outputTime;
        }

        return;
    }

    if (key == "progress")
    {
        Current_.ProgressEnded = value == "end";
        updates.append(Current_);
        Current_.ProgressEnded = false;
    }
}

std::optional<std::chrono::microseconds> FfmpegProgressParser::ParseClockTime(const QByteArray& value)
{
    const QStringList parts = QString::fromLatin1(value).split(QLatin1Char(':'));

    if (parts.size() != 3)
    {
        return std::nullopt;
    }

    bool hoursValid = false;
    bool minutesValid = false;
    bool secondsValid = false;
    const qint64 hours = parts.at(0).toLongLong(&hoursValid);
    const qint64 minutes = parts.at(1).toLongLong(&minutesValid);
    const double seconds = parts.at(2).toDouble(&secondsValid);

    if (!hoursValid || !minutesValid || !secondsValid || hours < 0 || minutes < 0 || minutes >= 60 ||
        !std::isfinite(seconds) || seconds < 0.0 || seconds >= 60.0)
    {
        return std::nullopt;
    }

    const double totalSeconds = static_cast<double>(hours * 3600 + minutes * 60) + seconds;
    const qint64 microseconds = static_cast<qint64>(std::llround(totalSeconds * 1'000'000.0));

    return std::chrono::microseconds{microseconds};
}
} // namespace ClipCutter
