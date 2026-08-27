#include "Core/Import/ClipImporter.h"

#include <QFileInfo>
#include <QDirIterator>

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

        const QDirIterator::IteratorFlag iteratorFlag = options.RecursiveDirectories ? QDirIterator::Subdirectories
                                                                                      : QDirIterator::NoIteratorFlags;
        QDirIterator iterator(directory->absolutePath(), QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                              iteratorFlag);
        while (iterator.hasNext()) candidates.append(iterator.next());
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

ImportResult ClipImporter::ImportPaths(const QStringList& paths, const ImportOptions& options) const
{
    QStringList files;
    QStringList directories;
    for (const QString& path : paths)
    {
        const QFileInfo info(path);
        if (info.isDir()) directories.append(info.absoluteFilePath());
        else files.append(path);
    }
    ImportResult combined = ImportFiles(files, options);
    ImportOptions next = options;
    for (const Clip& clip : combined.Imported) next.ExistingCanonicalPaths.insert(clip.SourcePath);
    for (const QString& directory : directories)
    {
        ImportResult part = ImportDirectory(QDir(directory), next);
        for (const Clip& clip : part.Imported) next.ExistingCanonicalPaths.insert(clip.SourcePath);
        combined.Imported += std::move(part.Imported);
        combined.Skipped += part.Skipped;
        combined.Errors += part.Errors;
    }
    return combined;
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
    clip.MediaInformation.ProbeStatus = EProbeStatus::Probing;

    Segment segment;
    segment.OutputBaseName = fileInfo.completeBaseName();
    segment.OutputExtension = fileInfo.suffix().isEmpty() ? QString() : QStringLiteral(".") + fileInfo.suffix();
    clip.Segments.append(segment);

    return clip;
}
} // namespace ClipCutter
