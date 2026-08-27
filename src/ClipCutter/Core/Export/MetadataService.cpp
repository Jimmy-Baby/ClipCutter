#include "Core/Export/MetadataService.h"

#include <QFile>
#include <QFileDevice>

#ifdef Q_OS_WIN
#include <Windows.h>

#include <utility>
#endif

namespace ClipCutter
{
MetadataResult PlatformMetadataService::CopyFileTimestamps(const QString& sourcePath, const QString& outputPath)
{
#ifdef Q_OS_WIN
    class Handle final
    {
    public:
        explicit Handle(HANDLE value) noexcept : Value_(value) {}
        ~Handle() { if (Value_ != INVALID_HANDLE_VALUE && Value_ != nullptr) CloseHandle(Value_); }
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        HANDLE Get() const noexcept { return Value_; }
        bool IsValid() const noexcept { return Value_ != INVALID_HANDLE_VALUE && Value_ != nullptr; }
    private:
        HANDLE Value_ = INVALID_HANDLE_VALUE;
    };

    FILETIME creationTime{};
    FILETIME accessedTime{};
    FILETIME modifiedTime{};
    const Handle source(CreateFileW(reinterpret_cast<LPCWSTR>(sourcePath.utf16()), FILE_READ_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr));

    if (!source.IsValid())
    {
        const DWORD code = GetLastError();
        return {false, EMetadataError::SourceOpenFailed,
                QStringLiteral("Unable to open the source file for metadata (Windows error %1).").arg(code), code};
    }

    if (GetFileTime(source.Get(), &creationTime, &accessedTime, &modifiedTime) == FALSE)
    {
        const DWORD code = GetLastError();
        return {false, EMetadataError::SourceReadFailed,
                QStringLiteral("Unable to read source timestamps (Windows error %1).").arg(code), code};
    }

    const Handle output(CreateFileW(reinterpret_cast<LPCWSTR>(outputPath.utf16()), FILE_WRITE_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr));

    if (!output.IsValid())
    {
        const DWORD code = GetLastError();
        return {false, EMetadataError::OutputOpenFailed,
                QStringLiteral("Unable to open the output for metadata (Windows error %1).").arg(code), code};
    }

    if (SetFileTime(output.Get(), &creationTime, &accessedTime, &modifiedTime) == FALSE)
    {
        const DWORD code = GetLastError();
        return {false, EMetadataError::OutputWriteFailed,
                QStringLiteral("Unable to set output timestamps (Windows error %1).").arg(code), code};
    }

    return {true, EMetadataError::None, {}, 0};
#else
    QFile source(sourcePath);
    QFile output(outputPath);
    const QDateTime modifiedTime = source.fileTime(QFileDevice::FileModificationTime);

    if (!modifiedTime.isValid())
    {
        return {false, EMetadataError::SourceReadFailed,
                QStringLiteral("Unable to read the source file modification time."), 0};
    }
    if (!output.setFileTime(modifiedTime, QFileDevice::FileModificationTime))
        return {false, EMetadataError::OutputWriteFailed,
                QStringLiteral("Unable to set the output file modification time."), 0};
    return {true, EMetadataError::None, {}, 0};
#endif
}
} // namespace ClipCutter
