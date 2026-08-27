#ifndef CLIPCUTTER_CORE_EXPORT_FFMPEGCOMMANDBUILDER_H
#define CLIPCUTTER_CORE_EXPORT_FFMPEGCOMMANDBUILDER_H

#include "Core/Export/ExportJob.h"
#include "Core/Export/FfmpegCommand.h"

namespace ClipCutter
{
class FfmpegCommandBuilder
{
public:
    explicit FfmpegCommandBuilder(QString programPath = {});

    FfmpegCommand Build(const ExportJob& job) const;

private:
    static QString MillisecondsArgument(std::chrono::milliseconds value);

    QString ProgramPath_;
};
} // namespace ClipCutter

#endif
