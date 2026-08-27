#include "Core/Import/ClipImporter.h"

#include <QFileInfo>

#include <algorithm>

namespace ClipCutter
{
QStringList ClipImporter::SupportedExtensions()
{
    return {QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"), QStringLiteral("mov")};
}

QString ClipImporter::FileDialogFilter()
{
    QStringList patterns;
    for (const QString& extension : SupportedExtensions())
    {
        patterns.append(QStringLiteral("*.%1").arg(extension));
    }

    return QStringLiteral("Video Files (%1)").arg(patterns.join(QLatin1Char(' ')));
}

bool ClipImporter::IsSupported(const QString& path)
{
    return SupportedExtensions().contains(QFileInfo(path).suffix(), Qt::CaseInsensitive);
}

ImportResult ClipImporter::Import(const QStringList& files, const std::optional<QDir>& directory,
                                  const ImportOptions& options) const
{
    QStringList candidates = files;
    if (directory.has_value())
    {
        if (!directory->exists())
        {
            ImportResult result;
            result.Errors.append({directory->absolutePath(), QStringLiteral("Directory does not exist.")});

            return result;
        }

        const QFileInfoList entries = directory->entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo& entry : entries)
        {
            candidates.append(entry.absoluteFilePath());
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const QString& left, const QString& right)
              {
                  const int folded = QString::compare(left, right, Qt::CaseInsensitive);
                  return folded == 0 ? left < right : folded < 0;
              });

    ImportResult result;
    QSet<QString> seen;
    for (const QString& existingPath : options.ExistingCanonicalPaths)
    {
        seen.insert(QDir::cleanPath(existingPath).toCaseFolded());
    }
    for (const QString& path : candidates)
    {
        const QFileInfo fileInfo(path);
        const QString displayPath = fileInfo.absoluteFilePath();

        if (!fileInfo.exists() || !fileInfo.isFile())
        {
            result.Errors.append({displayPath, QStringLiteral("File does not exist or is not a regular file.")});

            continue;
        }

        if (!IsSupported(fileInfo.fileName()))
        {
            result.Skipped.append(displayPath);

            continue;
        }

        const QString sourcePath = CanonicalPath(fileInfo);
        const QString duplicateKey = sourcePath.toCaseFolded();

        if (options.DuplicatePolicy == EDuplicatePolicy::Skip && seen.contains(duplicateKey))
        {
            result.Skipped.append(sourcePath);

            continue;
        }

        seen.insert(duplicateKey);
        result.Imported.append(MakeClip(fileInfo, sourcePath));
    }

    return result;
}

ImportResult ClipImporter::ImportFiles(const QStringList& files, const ImportOptions& options) const
{
    return Import(files, std::nullopt, options);
}

ImportResult ClipImporter::ImportDirectory(const QDir& directory, const ImportOptions& options) const
{
    return Import({}, directory, options);
}

QString ClipImporter::CanonicalPath(const QFileInfo& fileInfo)
{
    const QString canonical = fileInfo.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? fileInfo.absoluteFilePath() : canonical);
}

Clip ClipImporter::MakeClip(const QFileInfo& fileInfo, const QString& sourcePath)
{
    Clip clip;
    clip.SourcePath = sourcePath;
    clip.OriginalFileName = fileInfo.fileName();

    Segment segment;
    segment.OutputBaseName = fileInfo.completeBaseName();
    segment.OutputExtension = fileInfo.suffix().isEmpty() ? QString() : QStringLiteral(".") + fileInfo.suffix();
    clip.Segments.append(segment);

    return clip;
}
} // namespace ClipCutter
