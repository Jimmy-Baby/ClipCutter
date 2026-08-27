#ifndef CLIPCUTTER_CORE_EXPORT_OUTPUTPROFILE_H
#define CLIPCUTTER_CORE_EXPORT_OUTPUTPROFILE_H

#include <QString>
#include <QVector>

#include <optional>

namespace ClipCutter
{
enum class ETrimMode
{
    FastCopy,
    AccurateEncode
};

enum class EContainerMode
{
    PreserveSource,
    Mp4
};

enum class EVideoCodecMode
{
    Copy,
    H264
};

enum class EAudioCodecMode
{
    Copy,
    Aac
};

struct OutputProfile
{
    QString Id;
    QString DisplayName;
    QString Description;
    ETrimMode TrimMode = ETrimMode::FastCopy;
    EContainerMode Container = EContainerMode::PreserveSource;
    QString OutputExtension;
    EVideoCodecMode VideoCodec = EVideoCodecMode::Copy;
    EAudioCodecMode AudioCodec = EAudioCodecMode::Copy;
    std::optional<int> Crf;
    QString EncoderPreset;
    bool PreserveMetadataByDefault = true;
    QStringList CompatibilityConstraints;
};

class OutputProfiles final
{
public:
    static const QVector<OutputProfile>& BuiltIns();
    static const OutputProfile* Find(const QString& id);
    static QString ExtensionFor(const OutputProfile& profile, const QString& sourcePath);
};
} // namespace ClipCutter

#endif
