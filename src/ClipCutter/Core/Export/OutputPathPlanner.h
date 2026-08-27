#ifndef CLIPCUTTER_CORE_EXPORT_OUTPUTPATHPLANNER_H
#define CLIPCUTTER_CORE_EXPORT_OUTPUTPATHPLANNER_H

#include <QDir>
#include <QString>
#include <QUuid>
#include <QVector>

namespace ClipCutter
{
enum class ECollisionPolicy
{
    Ask,
    AutoRename,
    Skip,
    Overwrite
};

struct OutputRequest
{
    QUuid SegmentId;
    QString BaseName;
    QString Extension;
};

struct PlannedOutput
{
    QUuid SegmentId;
    QString BaseName;
    QString Extension;
    QString FinalPath;
    QString TemporaryPath;
    bool Skipped = false;
};

struct OutputPreflightResult
{
    QVector<PlannedOutput> Outputs;
    QStringList Errors;
    QStringList Collisions;

    bool IsReady() const noexcept { return Errors.isEmpty() && Collisions.isEmpty(); }
};

struct FileOperationResult
{
    bool Success = false;
    QString ErrorMessage;
};

class OutputPathPlanner final
{
public:
    static QString ValidateBaseName(const QString& baseName);
    static QString CreateTemporaryPath(const QString& finalPath, const QUuid& token = QUuid::createUuid());
    static OutputPreflightResult Preflight(const QVector<OutputRequest>& requests, const QDir& outputDirectory,
                                           ECollisionPolicy policy);
    static FileOperationResult Finalise(const QString& temporaryPath, const QString& finalPath,
                                        ECollisionPolicy policy);
    static int CleanupStaleTemporaryFiles(const QDir& directory, int minimumAgeDays = 7);
};
} // namespace ClipCutter

#endif
