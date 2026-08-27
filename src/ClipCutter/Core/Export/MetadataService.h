#ifndef CLIPCUTTER_CORE_EXPORT_METADATASERVICE_H
#define CLIPCUTTER_CORE_EXPORT_METADATASERVICE_H

#include <QString>

namespace ClipCutter
{
class MetadataService
{
public:
    virtual ~MetadataService() = default;

    virtual bool CopyFileTimestamps(const QString& sourcePath, const QString& outputPath, QString& error) = 0;
};

class PlatformMetadataService final : public MetadataService
{
public:
    bool CopyFileTimestamps(const QString& sourcePath, const QString& outputPath, QString& error) override;
};
} // namespace ClipCutter

#endif
