#ifndef CLIPCUTTER_CORE_IMPORT_CLIPIMPORTER_H
#define CLIPCUTTER_CORE_IMPORT_CLIPIMPORTER_H

#include "core/clip/Clip.h"

#include <QDir>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <optional>

namespace clipcutter
{
enum class DuplicatePolicy
{
    Skip,
    Keep
};

struct ImportError
{
    QString path;
    QString message;
};

struct ImportOptions
{
    DuplicatePolicy duplicatePolicy = DuplicatePolicy::Skip;
    QSet<QString> existingCanonicalPaths;
};

struct ImportResult
{
    QVector<Clip> imported;
    QStringList skipped;
    QVector<ImportError> errors;
};

class ClipImporter
{
public:
    static QStringList supportedExtensions();
    static QString fileDialogFilter();
    static bool isSupported(const QString& path);

    ImportResult import(
        const QStringList& files,
        const std::optional<QDir>& directory = std::nullopt,
        const ImportOptions& options = {}) const;
    ImportResult importFiles(const QStringList& files, const ImportOptions& options = {}) const;
    ImportResult importDirectory(const QDir& directory, const ImportOptions& options = {}) const;

private:
    static QString canonicalPath(const QFileInfo& fileInfo);
    static Clip makeClip(const QFileInfo& fileInfo, const QString& sourcePath);
};
}

#endif
