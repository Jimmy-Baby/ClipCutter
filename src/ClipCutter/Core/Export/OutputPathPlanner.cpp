#include "Core/Export/OutputPathPlanner.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <filesystem>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace ClipCutter
{
namespace
{
QString CollisionKey(const QString& path)
{
    return QDir::cleanPath(path).toCaseFolded();
}

QString AutoRenamedPath(const QString& desiredPath, QSet<QString>& used)
{
    const QFileInfo info(desiredPath);
    const QString suffix = info.suffix();
    const QString extension = suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;
    const QString stem = info.completeBaseName();
    for (int number = 2;; ++number)
    {
        const QString candidate = info.dir().absoluteFilePath(
            QStringLiteral("%1 (%2)%3").arg(stem).arg(number).arg(extension));
        const QString key = CollisionKey(candidate);
        if (!used.contains(key) && !QFileInfo::exists(candidate))
        {
            used.insert(key);
            return candidate;
        }
    }
}
} // namespace

QString OutputPathPlanner::ValidateBaseName(const QString& baseName)
{
    if (baseName.isEmpty() || baseName.trimmed().isEmpty())
        return QStringLiteral("Output name cannot be empty.");
    if (baseName.endsWith(QLatin1Char('.')) || baseName.endsWith(QLatin1Char(' ')))
        return QStringLiteral("Output name cannot end in a period or space.");
    static const QRegularExpression invalid(QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])"));
    if (invalid.match(baseName).hasMatch())
        return QStringLiteral("Output name contains a character that is invalid on Windows.");
    const QString device = baseName.section(QLatin1Char('.'), 0, 0).toUpper();
    static const QRegularExpression reserved(QStringLiteral(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$)"));
    if (reserved.match(device).hasMatch())
        return QStringLiteral("Output name is reserved by Windows.");
    return {};
}

QString OutputPathPlanner::CreateTemporaryPath(const QString& finalPath, const QUuid& token)
{
    const QFileInfo info(finalPath);
    const QString suffix = info.suffix();
    const QString name = suffix.isEmpty()
                             ? QStringLiteral(".clipcutter-%1.part").arg(token.toString(QUuid::WithoutBraces))
                             : QStringLiteral(".clipcutter-%1.part.%2")
                                   .arg(token.toString(QUuid::WithoutBraces), suffix);
    return info.dir().absoluteFilePath(name);
}

OutputPreflightResult OutputPathPlanner::Preflight(const QVector<OutputRequest>& requests,
                                                   const QDir& outputDirectory, const ECollisionPolicy policy)
{
    OutputPreflightResult result;
    const QFileInfo directoryInfo(outputDirectory.absolutePath());
    if (!directoryInfo.exists() || !directoryInfo.isDir())
    {
        result.Errors.append(QStringLiteral("Output directory does not exist: %1").arg(directoryInfo.absoluteFilePath()));
        return result;
    }
    if (!directoryInfo.isWritable())
    {
        result.Errors.append(QStringLiteral("Output directory is not writable: %1").arg(directoryInfo.absoluteFilePath()));
        return result;
    }

    QSet<QString> used;
    for (const OutputRequest& request : requests)
    {
        const QString validationError = ValidateBaseName(request.BaseName);
        if (!validationError.isEmpty())
        {
            result.Errors.append(QStringLiteral("%1: %2").arg(request.BaseName, validationError));
            continue;
        }
        PlannedOutput output{request.SegmentId, request.BaseName, request.Extension};
        QString desiredPath = outputDirectory.absoluteFilePath(request.BaseName + request.Extension);
        const QString desiredKey = CollisionKey(desiredPath);
        const bool duplicate = used.contains(desiredKey);
        const bool exists = QFileInfo::exists(desiredPath);
        if (duplicate || exists)
        {
            if (policy == ECollisionPolicy::Ask)
            {
                result.Collisions.append(desiredPath);
                continue;
            }
            if (policy == ECollisionPolicy::Skip)
            {
                output.Skipped = true;
            }
            else if (policy == ECollisionPolicy::AutoRename)
            {
                desiredPath = AutoRenamedPath(desiredPath, used);
                output.BaseName = QFileInfo(desiredPath).completeBaseName();
            }
            else if (duplicate)
            {
                result.Errors.append(QStringLiteral("Duplicate output in this batch: %1").arg(desiredPath));
                continue;
            }
        }
        used.insert(CollisionKey(desiredPath));
        output.FinalPath = desiredPath;
        output.TemporaryPath = CreateTemporaryPath(desiredPath);
        result.Outputs.append(std::move(output));
    }
    return result;
}

FileOperationResult OutputPathPlanner::Finalise(const QString& temporaryPath, const QString& finalPath,
                                                const ECollisionPolicy policy)
{
    const QFileInfo temporary(temporaryPath);
    if (!temporary.exists() || !temporary.isFile() || temporary.size() <= 0)
        return {false, QStringLiteral("Verified temporary output is missing or empty: %1").arg(temporaryPath)};
    const bool targetExists = QFileInfo::exists(finalPath);
    if (targetExists && policy != ECollisionPolicy::Overwrite)
        return {false, QStringLiteral("Output appeared after preflight and was not overwritten: %1").arg(finalPath)};

#ifdef Q_OS_WIN
    const BOOL succeeded = targetExists
        ? ReplaceFileW(reinterpret_cast<LPCWSTR>(finalPath.utf16()), reinterpret_cast<LPCWSTR>(temporaryPath.utf16()),
                       nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)
        : MoveFileExW(reinterpret_cast<LPCWSTR>(temporaryPath.utf16()), reinterpret_cast<LPCWSTR>(finalPath.utf16()),
                      MOVEFILE_WRITE_THROUGH);
    if (!succeeded)
        return {false, QStringLiteral("Unable to finalise output %1 (Windows error %2).").arg(finalPath).arg(GetLastError())};
#else
    std::error_code error;
    std::filesystem::rename(std::filesystem::path(QFile::encodeName(temporaryPath).constData()),
                            std::filesystem::path(QFile::encodeName(finalPath).constData()), error);
    if (error)
        return {false, QStringLiteral("Unable to atomically finalise output %1: %2")
                           .arg(finalPath, QString::fromStdString(error.message()))};
#endif
    return {true, {}};
}

int OutputPathPlanner::CleanupStaleTemporaryFiles(const QDir& directory, const int minimumAgeDays)
{
    int removed = 0;
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-qMax(1, minimumAgeDays));
    const QFileInfoList candidates = directory.entryInfoList({QStringLiteral(".clipcutter-*.part.*")}, QDir::Files);
    for (const QFileInfo& candidate : candidates)
    {
        if (candidate.lastModified().toUTC() < cutoff && QFile::remove(candidate.absoluteFilePath())) ++removed;
    }
    return removed;
}
} // namespace ClipCutter
