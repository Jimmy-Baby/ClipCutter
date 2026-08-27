#ifndef CLIPCUTTER_CORE_EXPORT_FFMPEGPROGRESSPARSER_H
#define CLIPCUTTER_CORE_EXPORT_FFMPEGPROGRESSPARSER_H

#include "Core/Export/FfmpegProgress.h"

#include <QByteArray>
#include <QVector>

namespace ClipCutter
{
class FfmpegProgressParser
{
public:
    QVector<FfmpegProgress> Feed(const QByteArray& data);
    void Reset();

private:
    void ParseLine(const QByteArray& line, QVector<FfmpegProgress>& updates);
    static std::optional<std::chrono::microseconds> ParseClockTime(const QByteArray& value);

    QByteArray Buffer_;
    FfmpegProgress Current_;
};
} // namespace ClipCutter

#endif
