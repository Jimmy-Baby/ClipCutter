#include "Core/Export/OutputDestination.h"

#include <QFileInfo>
#include <QSet>

namespace ClipCutter
{
namespace
{
QString Key(const QString& path) { return QDir::cleanPath(path).toCaseFolded(); }
}

QString OutputDestination::DirectoryForSource(const QString& sourcePath,
                                              const OutputDestinationSettings& settings)
{
    if (settings.Mode == EOutputDestinationMode::FixedDirectory)
        return settings.FixedDirectory.trimmed().isEmpty()
                   ? QString() : QDir::cleanPath(QFileInfo(settings.FixedDirectory).absoluteFilePath());
    return QDir::cleanPath(QFileInfo(sourcePath).dir().absoluteFilePath(settings.SourceRelativeSubdirectory));
}

QString OutputDestination::Validate(const OutputDestinationSettings& settings, const QStringList& sourcePaths)
{
    if (settings.Mode == EOutputDestinationMode::FixedDirectory)
    {
        if (settings.FixedDirectory.trimmed().isEmpty()) return QStringLiteral("Choose a fixed output directory.");
        const QFileInfo info(settings.FixedDirectory);
        if (info.exists() && !info.isDir()) return QStringLiteral("Output destination is not a directory: %1").arg(info.absoluteFilePath());
        const QDir parent = info.exists() ? info.dir() : QDir(info.absolutePath());
        if (!info.exists() && (!parent.exists() || !QFileInfo(parent.absolutePath()).isWritable()))
            return QStringLiteral("Output destination cannot be created: %1").arg(info.absoluteFilePath());
        if (info.exists() && !info.isWritable()) return QStringLiteral("Output directory is not writable: %1").arg(info.absoluteFilePath());
        return {};
    }

    const QString subdirectory = settings.SourceRelativeSubdirectory.trimmed();
    if (subdirectory.isEmpty()) return QStringLiteral("Source-relative subdirectory cannot be empty.");
    if (QDir::isAbsolutePath(subdirectory) || subdirectory.contains(QLatin1Char('/')) || subdirectory.contains(QLatin1Char('\\')) ||
        subdirectory == QStringLiteral(".") || subdirectory == QStringLiteral(".."))
        return QStringLiteral("Source-relative destination must be one subdirectory name.");
    const QString nameError = OutputPathPlanner::ValidateBaseName(subdirectory);
    if (!nameError.isEmpty()) return QStringLiteral("Invalid source-relative subdirectory: %1").arg(nameError);

    for (const QString& source : sourcePaths)
    {
        const QFileInfo sourceInfo(source);
        if (!sourceInfo.exists() || !sourceInfo.isFile()) continue;
        const QFileInfo destination(DirectoryForSource(source, settings));
        if (destination.exists() && !destination.isDir())
            return QStringLiteral("Output destination conflicts with a file: %1").arg(destination.absoluteFilePath());
        if (!destination.exists() && !QFileInfo(sourceInfo.dir().absolutePath()).isWritable())
            return QStringLiteral("Source directory is not writable: %1").arg(sourceInfo.dir().absolutePath());
        if (destination.exists() && !destination.isWritable())
            return QStringLiteral("Output directory is not writable: %1").arg(destination.absoluteFilePath());
    }
    return {};
}

bool OutputDestination::PrepareDirectories(const OutputDestinationSettings& settings, const QStringList& sourcePaths,
                                           QString* error)
{
    const QString validation = Validate(settings, sourcePaths);
    if (!validation.isEmpty())
    {
        if (error != nullptr) *error = validation;
        return false;
    }
    QSet<QString> directories;
    if (settings.Mode == EOutputDestinationMode::FixedDirectory)
        directories.insert(DirectoryForSource(sourcePaths.value(0), settings));
    else
        for (const QString& source : sourcePaths) directories.insert(DirectoryForSource(source, settings));
    for (const QString& directory : directories)
    {
        if (!QDir().mkpath(directory))
        {
            if (error != nullptr) *error = QStringLiteral("Unable to create output directory: %1").arg(directory);
            return false;
        }
    }
    return true;
}

OutputPreflightResult OutputDestination::Preflight(const QVector<DestinationRequest>& requests,
                                                   const OutputDestinationSettings& settings,
                                                   const ECollisionPolicy policy)
{
    OutputPreflightResult combined;
    QStringList sources;
    for (const DestinationRequest& request : requests) sources.append(request.SourcePath);
    const QString validation = Validate(settings, sources);
    if (!validation.isEmpty())
    {
        combined.Errors.append(validation);
        return combined;
    }

    QHash<QString, QVector<OutputRequest>> groups;
    QHash<QString, QString> originalDirectories;
    for (const DestinationRequest& request : requests)
    {
        const QString directory = DirectoryForSource(request.SourcePath, settings);
        const QString key = Key(directory);
        groups[key].append({request.SegmentId, request.BaseName, request.Extension});
        originalDirectories.insert(key, directory);
    }
    for (auto iterator = groups.cbegin(); iterator != groups.cend(); ++iterator)
    {
        const OutputPreflightResult part = OutputPathPlanner::Preflight(iterator.value(), QDir(originalDirectories.value(iterator.key())), policy);
        combined.Outputs += part.Outputs;
        combined.Errors += part.Errors;
        combined.Collisions += part.Collisions;
    }
    return combined;
}
} // namespace ClipCutter
