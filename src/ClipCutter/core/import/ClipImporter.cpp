#include "core/import/ClipImporter.h"

#include <QFileInfo>

#include <algorithm>

namespace clipcutter
{
QStringList ClipImporter::supportedExtensions()
{
    return { QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"), QStringLiteral("mov") };
}

QString ClipImporter::fileDialogFilter()
{
    QStringList patterns;
    for (const QString& extension : supportedExtensions())
    {
        patterns.append(QStringLiteral("*.%1").arg(extension));
    }
    return QStringLiteral("Video Files (%1)").arg(patterns.join(QLatin1Char(' ')));
}

bool ClipImporter::isSupported(const QString& path)
{
    return supportedExtensions().contains(QFileInfo(path).suffix(), Qt::CaseInsensitive);
}

ImportResult ClipImporter::import(
    const QStringList& files,
    const std::optional<QDir>& directory,
    const ImportOptions& options) const
{
    QStringList candidates = files;
    if (directory.has_value())
    {
        if (!directory->exists())
        {
            ImportResult result;
            result.errors.append({ directory->absolutePath(), QStringLiteral("Directory does not exist.") });
            return result;
        }

        const QFileInfoList entries = directory->entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo& entry : entries)
        {
            candidates.append(entry.absoluteFilePath());
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const QString& left, const QString& right)
    {
        const int folded = QString::compare(left, right, Qt::CaseInsensitive);
        return folded == 0 ? left < right : folded < 0;
    });

    ImportResult result;
    QSet<QString> seen;
    for (const QString& existingPath : options.existingCanonicalPaths)
    {
        seen.insert(QDir::cleanPath(existingPath).toCaseFolded());
    }
    for (const QString& path : candidates)
    {
        const QFileInfo fileInfo(path);
        const QString displayPath = fileInfo.absoluteFilePath();
        if (!fileInfo.exists() || !fileInfo.isFile())
        {
            result.errors.append({ displayPath, QStringLiteral("File does not exist or is not a regular file.") });
            continue;
        }
        if (!isSupported(fileInfo.fileName()))
        {
            result.skipped.append(displayPath);
            continue;
        }

        const QString sourcePath = canonicalPath(fileInfo);
        const QString duplicateKey = sourcePath.toCaseFolded();
        if (options.duplicatePolicy == DuplicatePolicy::Skip && seen.contains(duplicateKey))
        {
            result.skipped.append(sourcePath);
            continue;
        }
        seen.insert(duplicateKey);
        result.imported.append(makeClip(fileInfo, sourcePath));
    }
    return result;
}

ImportResult ClipImporter::importFiles(const QStringList& files, const ImportOptions& options) const
{
    return import(files, std::nullopt, options);
}

ImportResult ClipImporter::importDirectory(const QDir& directory, const ImportOptions& options) const
{
    return import({}, directory, options);
}

QString ClipImporter::canonicalPath(const QFileInfo& fileInfo)
{
    const QString canonical = fileInfo.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? fileInfo.absoluteFilePath() : canonical);
}

Clip ClipImporter::makeClip(const QFileInfo& fileInfo, const QString& sourcePath)
{
    Clip clip;
    clip.sourcePath = sourcePath;
    clip.originalFileName = fileInfo.fileName();

    Segment segment;
    segment.outputBaseName = fileInfo.completeBaseName();
    segment.outputExtension = fileInfo.suffix().isEmpty()
        ? QString()
        : QStringLiteral(".") + fileInfo.suffix();
    clip.segments.append(segment);
    return clip;
}
}
