#ifndef CLIPCUTTER_CORE_EXPORT_FFMPEGCOMMAND_H
#define CLIPCUTTER_CORE_EXPORT_FFMPEGCOMMAND_H

#include <QString>
#include <QStringList>

namespace ClipCutter
{
struct FfmpegCommand
{
    QString Program;
    QStringList Arguments;
};
} // namespace ClipCutter

#endif
