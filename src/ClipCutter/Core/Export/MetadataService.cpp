#include "Core/Export/MetadataService.h"

#include <QFile>
#include <QFileDevice>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace ClipCutter
{
bool PlatformMetadataService::CopyFileTimestamps(const QString& sourcePath, const QString& outputPath, QString& error)
{
    error.clear();

#ifdef Q_OS_WIN
    FILETIME creationTime;
    FILETIME accessedTime;
    FILETIME modifiedTime;
    const HANDLE source = CreateFileW(reinterpret_cast<LPCWSTR>(sourcePath.utf16()), FILE_READ_ATTRIBUTES,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);

    if (source == INVALID_HANDLE_VALUE)
    {
        error = QStringLiteral("Unable to open the source file for metadata. Windows error: %1").arg(GetLastError());

        return false;
    }

    const bool readSucceeded = GetFileTime(source, &creationTime, &accessedTime, &modifiedTime) != FALSE;
    CloseHandle(source);

    if (!readSucceeded)
    {
        error = QStringLiteral("Unable to read the source file timestamps. Windows error: %1").arg(GetLastError());

        return false;
    }

    const HANDLE output = CreateFileW(reinterpret_cast<LPCWSTR>(outputPath.utf16()), FILE_WRITE_ATTRIBUTES,
                                      FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (output == INVALID_HANDLE_VALUE)
    {
        error = QStringLiteral("Unable to open the output file for metadata. Windows error: %1").arg(GetLastError());

        return false;
    }

    const bool writeSucceeded = SetFileTime(output, &creationTime, &accessedTime, &modifiedTime) != FALSE;
    CloseHandle(output);

    if (!writeSucceeded)
    {
        error = QStringLiteral("Unable to set the output file timestamps. Windows error: %1").arg(GetLastError());

        return false;
    }

    return true;
#else
    QFile source(sourcePath);
    QFile output(outputPath);
    const QDateTime modifiedTime = source.fileTime(QFileDevice::FileModificationTime);

    if (!modifiedTime.isValid() || !output.setFileTime(modifiedTime, QFileDevice::FileModificationTime))
    {
        error = QStringLiteral("Unable to copy the output file modification time.");

        return false;
    }

    return true;
#endif
}
} // namespace ClipCutter
