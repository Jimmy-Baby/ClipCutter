#ifndef CLIPCUTTER_CORE_EXPORT_METADATASERVICE_H
#define CLIPCUTTER_CORE_EXPORT_METADATASERVICE_H

#include <QString>

namespace ClipCutter
{
enum class EMetadataError
{
    None,
    SourceOpenFailed,
    SourceReadFailed,
    OutputOpenFailed,
    OutputWriteFailed
};

struct MetadataResult
{
    bool Success = false;
    EMetadataError Error = EMetadataError::None;
    QString Message;
    unsigned long NativeError = 0;
};

class MetadataService
{
public:
    virtual ~MetadataService() = default;

    virtual MetadataResult CopyFileTimestamps(const QString& sourcePath, const QString& outputPath) = 0;
};

class PlatformMetadataService final : public MetadataService
{
public:
    MetadataResult CopyFileTimestamps(const QString& sourcePath, const QString& outputPath) override;
};
} // namespace ClipCutter

#endif
