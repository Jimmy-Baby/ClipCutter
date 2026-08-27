#ifndef CLIPCUTTER_CORE_EXPORT_OUTPUTDESTINATION_H
#define CLIPCUTTER_CORE_EXPORT_OUTPUTDESTINATION_H

#include "Core/Export/OutputPathPlanner.h"

#include <QString>
#include <QVector>

namespace ClipCutter
{
enum class EOutputDestinationMode
{
    SourceRelative = 0,
    FixedDirectory = 1
};

struct OutputDestinationSettings
{
    EOutputDestinationMode Mode = EOutputDestinationMode::SourceRelative;
    QString SourceRelativeSubdirectory = QStringLiteral("ClipCutterOutput");
    QString FixedDirectory;
};

struct DestinationRequest : OutputRequest
{
    QString SourcePath;
};

class OutputDestination final
{
public:
    static QString Validate(const OutputDestinationSettings& settings, const QStringList& sourcePaths);
    static QString DirectoryForSource(const QString& sourcePath, const OutputDestinationSettings& settings);
    static bool PrepareDirectories(const OutputDestinationSettings& settings, const QStringList& sourcePaths,
                                   QString* error = nullptr);
    static OutputPreflightResult Preflight(const QVector<DestinationRequest>& requests,
                                           const OutputDestinationSettings& settings, ECollisionPolicy policy);
};
} // namespace ClipCutter

#endif
