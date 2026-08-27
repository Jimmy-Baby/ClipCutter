#ifndef CLIPCUTTER_CORE_EXPORT_EXPORTSTATE_H
#define CLIPCUTTER_CORE_EXPORT_EXPORTSTATE_H

#include <QString>

namespace ClipCutter
{
enum class EExportState
{
    Pending,
    Preparing,
    Running,
    Finalising,
    Succeeded,
    Failed,
    Cancelling,
    Cancelled,
    Skipped
};

bool IsTerminalState(EExportState state) noexcept;
bool IsValidTransition(EExportState from, EExportState to) noexcept;
QString ExportStateText(EExportState state);
} // namespace ClipCutter

#endif
