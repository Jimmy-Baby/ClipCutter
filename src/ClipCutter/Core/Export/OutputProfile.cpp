#include "Core/Export/OutputProfile.h"

#include <QFileInfo>

namespace ClipCutter
{
const QVector<OutputProfile>& OutputProfiles::BuiltIns()
{
    static const QVector<OutputProfile> profiles{
        {QStringLiteral("fast-copy"), QStringLiteral("Fast Copy"),
         QStringLiteral("Fast stream copy. Trim boundaries can align to nearby seek/keyframe boundaries."),
         ETrimMode::FastCopy, EContainerMode::PreserveSource, {}, EVideoCodecMode::Copy, EAudioCodecMode::Copy,
         std::nullopt, {}, true, {QStringLiteral("Preserves the source container and compatible streams.")}},
        {QStringLiteral("accurate-balanced"), QStringLiteral("Accurate Balanced"),
         QStringLiteral("Accurate MP4 trim using H.264 video and AAC audio; balanced size and speed."),
         ETrimMode::AccurateEncode, EContainerMode::Mp4, QStringLiteral(".mp4"), EVideoCodecMode::H264,
         EAudioCodecMode::Aac, 23, QStringLiteral("medium"), true,
         {QStringLiteral("Requires libx264, AAC, and the MP4 muxer.")}},
        {QStringLiteral("accurate-high-quality"), QStringLiteral("Accurate High Quality"),
         QStringLiteral("Accurate MP4 trim with higher H.264 quality and a slower encoder preset."),
         ETrimMode::AccurateEncode, EContainerMode::Mp4, QStringLiteral(".mp4"), EVideoCodecMode::H264,
         EAudioCodecMode::Aac, 18, QStringLiteral("slow"), true,
         {QStringLiteral("Requires libx264, AAC, and the MP4 muxer.")}},
        {QStringLiteral("compact"), QStringLiteral("Compact"),
         QStringLiteral("Accurate MP4 trim tuned for smaller H.264/AAC files."), ETrimMode::AccurateEncode,
         EContainerMode::Mp4, QStringLiteral(".mp4"), EVideoCodecMode::H264, EAudioCodecMode::Aac, 28,
         QStringLiteral("fast"), true, {QStringLiteral("Requires libx264, AAC, and the MP4 muxer.")}}
    };
    return profiles;
}

const OutputProfile* OutputProfiles::Find(const QString& id)
{
    for (const OutputProfile& profile : BuiltIns())
    {
        if (profile.Id == id)
        {
            return &profile;
        }
    }
    return nullptr;
}

QString OutputProfiles::ExtensionFor(const OutputProfile& profile, const QString& sourcePath)
{
    if (profile.Container != EContainerMode::PreserveSource)
    {
        return profile.OutputExtension;
    }
    const QString suffix = QFileInfo(sourcePath).suffix();
    return suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;
}
} // namespace ClipCutter
