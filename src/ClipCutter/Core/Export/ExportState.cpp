#include "Core/Export/ExportState.h"

namespace ClipCutter
{
bool IsTerminalState(const EExportState state) noexcept
{
    return state == EExportState::Succeeded || state == EExportState::Failed || state == EExportState::Cancelled ||
           state == EExportState::Skipped;
}

bool IsValidTransition(const EExportState from, const EExportState to) noexcept
{
    switch (from)
    {
    case EExportState::Pending:
        return to == EExportState::Preparing || to == EExportState::Cancelled || to == EExportState::Skipped;
    case EExportState::Preparing:
        return to == EExportState::Running || to == EExportState::Failed || to == EExportState::Cancelling ||
               to == EExportState::Cancelled;
    case EExportState::Running:
        return to == EExportState::Finalising || to == EExportState::Failed || to == EExportState::Cancelling;
    case EExportState::Finalising:
        return to == EExportState::Succeeded || to == EExportState::Failed;
    case EExportState::Failed:
    case EExportState::Cancelled:
        return to == EExportState::Pending;
    case EExportState::Cancelling:
        return to == EExportState::Cancelled;
    case EExportState::Succeeded:
    case EExportState::Skipped:
        return false;
    }

    return false;
}

QString ExportStateText(const EExportState state)
{
    switch (state)
    {
    case EExportState::Pending:
        return QStringLiteral("Pending");
    case EExportState::Preparing:
        return QStringLiteral("Preparing");
    case EExportState::Running:
        return QStringLiteral("Running");
    case EExportState::Finalising:
        return QStringLiteral("Finalising");
    case EExportState::Succeeded:
        return QStringLiteral("Succeeded");
    case EExportState::Failed:
        return QStringLiteral("Failed");
    case EExportState::Cancelling:
        return QStringLiteral("Cancelling");
    case EExportState::Cancelled:
        return QStringLiteral("Cancelled");
    case EExportState::Skipped:
        return QStringLiteral("Skipped");
    }

    return QStringLiteral("Unknown");
}
} // namespace ClipCutter
