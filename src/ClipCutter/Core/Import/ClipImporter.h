#ifndef CLIPCUTTER_CORE_IMPORT_CLIPIMPORTER_H
#define CLIPCUTTER_CORE_IMPORT_CLIPIMPORTER_H

#include "Core/Clip/Clip.h"

#include <QDir>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <optional>

namespace ClipCutter
{
enum class EDuplicatePolicy
{
    Skip,
    Keep
};

struct ImportError
{
    QString Path;
    QString Message;
};

struct ImportOptions
{
    EDuplicatePolicy DuplicatePolicy = EDuplicatePolicy::Skip;
    QSet<QString> ExistingCanonicalPaths;
    bool RecursiveDirectories = false;
};

struct ImportResult
{
    QVector<Clip> Imported;
    QStringList Skipped;
    QVector<ImportError> Errors;
};

class ClipImporter
{
public:
    static QStringList SupportedExtensions();
    static QString FileDialogFilter();
    static bool IsSupported(const QString& path);

    ImportResult Import(const QStringList& files, const std::optional<QDir>& directory = std::nullopt,
                        const ImportOptions& options = {}) const;
    ImportResult ImportFiles(const QStringList& files, const ImportOptions& options = {}) const;
    ImportResult ImportDirectory(const QDir& directory, const ImportOptions& options = {}) const;
    ImportResult ImportPaths(const QStringList& paths, const ImportOptions& options = {}) const;

private:
    static QString CanonicalPath(const QFileInfo& fileInfo);
    static Clip MakeClip(const QFileInfo& fileInfo, const QString& sourcePath);
};
} // namespace ClipCutter

#endif
