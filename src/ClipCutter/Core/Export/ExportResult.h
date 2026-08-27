#ifndef CLIPCUTTER_CORE_EXPORT_EXPORTRESULT_H
#define CLIPCUTTER_CORE_EXPORT_EXPORTRESULT_H

#include "Core/Export/ExportState.h"

#include <QString>
#include <QUuid>

namespace ClipCutter
{
struct ExportResult
{
    QUuid JobId;
    EExportState State = EExportState::Failed;
    int ExitCode = -1;
    bool ProcessCrashed = false;
    QString ErrorMessage;
    QString StandardOutput;
    QString StandardError;
};

struct ExportSummary
{
    int SucceededCount = 0;
    int FailedCount = 0;
    int SkippedCount = 0;
    int CancelledCount = 0;
};
} // namespace ClipCutter

#endif
