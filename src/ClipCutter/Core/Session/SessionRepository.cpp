#include "Core/Session/SessionRepository.h"

#include "Core/Export/OutputProfile.h"
#include "Core/Naming/NamingTemplate.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

using namespace std::chrono_literals;

namespace ClipCutter
{
namespace
{
QString StoredSourcePath(const QString& sourcePath, const QDir& sessionDirectory)
{
    const QString relative = sessionDirectory.relativeFilePath(sourcePath);
    if (!relative.isEmpty() && !QDir::isAbsolutePath(relative) && relative != QStringLiteral("..") &&
        !relative.startsWith(QStringLiteral("../")) && !relative.startsWith(QStringLiteral("..\\")))
        return QDir::fromNativeSeparators(relative);
    return QDir::cleanPath(QFileInfo(sourcePath).absoluteFilePath());
}

QString ResolvedSourcePath(const QString& storedPath, const QDir& sessionDirectory)
{
    const QString path = QDir::isAbsolutePath(storedPath) ? storedPath : sessionDirectory.absoluteFilePath(storedPath);
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

QJsonObject SegmentToJson(const Segment& segment)
{
    QJsonObject json;
    json.insert(QStringLiteral("id"), segment.Id.toString(QUuid::WithoutBraces));
    json.insert(QStringLiteral("startMs"), static_cast<qint64>(segment.Range.Start().count()));
    json.insert(QStringLiteral("endMs"), static_cast<qint64>(segment.Range.End().count()));
    json.insert(QStringLiteral("trimRangeUserEdited"), segment.TrimRangeUserEdited);
    json.insert(QStringLiteral("outputBaseName"), segment.OutputBaseName);
    json.insert(QStringLiteral("outputExtension"), segment.OutputExtension);
    json.insert(QStringLiteral("prefix"), segment.Prefix.value_or(QString()));
    json.insert(QStringLiteral("namingTemplate"), segment.NamingTemplatePattern.value_or(QString()));
    json.insert(QStringLiteral("skipped"), segment.Skipped);
    json.insert(QStringLiteral("outputProfile"), segment.ExportProfileId);
    return json;
}

QJsonObject ClipToJson(const Clip& clip, const QDir& sessionDirectory)
{
    QJsonObject json;
    json.insert(QStringLiteral("id"), clip.Id.toString(QUuid::WithoutBraces));
    json.insert(QStringLiteral("sourcePath"), StoredSourcePath(clip.SourcePath, sessionDirectory));
    json.insert(QStringLiteral("sourcePathKind"), QDir::isAbsolutePath(StoredSourcePath(clip.SourcePath, sessionDirectory))
                                                     ? QStringLiteral("absolute") : QStringLiteral("relative"));
    json.insert(QStringLiteral("originalFileName"), clip.OriginalFileName);
    json.insert(QStringLiteral("thumbnailSourceFingerprint"), clip.ThumbnailSourceFingerprint);
    QJsonArray segments;
    for (const Segment& segment : clip.Segments) segments.append(SegmentToJson(segment));
    json.insert(QStringLiteral("segments"), segments);
    return json;
}

bool ReadUuid(const QJsonObject& json, const QString& key, QUuid& value)
{
    value = QUuid(json.value(key).toString());
    return !value.isNull();
}

std::optional<Segment> SegmentFromJson(const QJsonObject& json, QString& error)
{
    Segment segment;
    if (!ReadUuid(json, QStringLiteral("id"), segment.Id))
    {
        error = QStringLiteral("Session contains an invalid segment ID.");
        return std::nullopt;
    }
    const qint64 start = json.value(QStringLiteral("startMs")).toVariant().toLongLong();
    const qint64 end = json.value(QStringLiteral("endMs")).toVariant().toLongLong();
    const auto range = TimeRange::Create(std::chrono::milliseconds{start}, std::chrono::milliseconds{end});
    if (!range.has_value())
    {
        error = QStringLiteral("Session contains an invalid trim range.");
        return std::nullopt;
    }
    segment.Range = *range;
    segment.TrimRangeUserEdited = json.value(QStringLiteral("trimRangeUserEdited")).toBool();
    segment.OutputBaseName = json.value(QStringLiteral("outputBaseName")).toString();
    segment.OutputExtension = json.value(QStringLiteral("outputExtension")).toString();
    const QString prefix = json.value(QStringLiteral("prefix")).toString();
    if (!prefix.isEmpty()) segment.Prefix = prefix;
    const QString namingTemplate = json.value(QStringLiteral("namingTemplate")).toString();
    if (!namingTemplate.isEmpty()) segment.NamingTemplatePattern = namingTemplate;
    segment.Skipped = json.value(QStringLiteral("skipped")).toBool();
    segment.ExportProfileId = json.value(QStringLiteral("outputProfile")).toString(QStringLiteral("fast-copy"));
    if (OutputProfiles::Find(segment.ExportProfileId) == nullptr) segment.ExportProfileId = QStringLiteral("fast-copy");
    return segment;
}

QJsonObject MigratedSingleSegment(const QJsonObject& clipJson)
{
    QJsonObject segment = clipJson.value(QStringLiteral("segment")).toObject();
    QJsonObject range = segment.value(QStringLiteral("range")).toObject();
    if (range.isEmpty()) range = clipJson.value(QStringLiteral("range")).toObject();
    if (!segment.contains(QStringLiteral("id")))
        segment.insert(QStringLiteral("id"), clipJson.value(QStringLiteral("segmentId")));
    if (!segment.contains(QStringLiteral("startMs")))
        segment.insert(QStringLiteral("startMs"), range.contains(QStringLiteral("startMs"))
            ? range.value(QStringLiteral("startMs"))
            : range.contains(QStringLiteral("start")) ? range.value(QStringLiteral("start"))
            : clipJson.contains(QStringLiteral("trimStartMs")) ? clipJson.value(QStringLiteral("trimStartMs"))
            : clipJson.value(QStringLiteral("startMs")));
    if (!segment.contains(QStringLiteral("endMs")))
        segment.insert(QStringLiteral("endMs"), range.contains(QStringLiteral("endMs"))
            ? range.value(QStringLiteral("endMs"))
            : range.contains(QStringLiteral("end")) ? range.value(QStringLiteral("end"))
            : clipJson.contains(QStringLiteral("trimEndMs")) ? clipJson.value(QStringLiteral("trimEndMs"))
            : clipJson.value(QStringLiteral("endMs")));
    if (!segment.contains(QStringLiteral("trimRangeUserEdited")))
        segment.insert(QStringLiteral("trimRangeUserEdited"), true);
    for (const QString& key : {QStringLiteral("outputBaseName"), QStringLiteral("outputExtension"),
                               QStringLiteral("prefix"), QStringLiteral("namingTemplate"),
                               QStringLiteral("skipped"), QStringLiteral("outputProfile")})
        if (!segment.contains(key)) segment.insert(key, clipJson.value(key));
    if (segment.value(QStringLiteral("outputBaseName")).toString().isEmpty())
    {
        const QFileInfo outputName(clipJson.value(QStringLiteral("outputName")).toString());
        if (!outputName.fileName().isEmpty())
        {
            segment.insert(QStringLiteral("outputBaseName"), outputName.completeBaseName());
            if (segment.value(QStringLiteral("outputExtension")).toString().isEmpty() && !outputName.suffix().isEmpty())
                segment.insert(QStringLiteral("outputExtension"), QStringLiteral(".") + outputName.suffix());
        }
    }
    return segment;
}
} // namespace

QString SessionRepository::DefaultRecoveryPath()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(root).absoluteFilePath(QStringLiteral("recovery.clipcutter.json"));
}

bool SessionRepository::Save(const QString& filePath, const SessionData& session, QString* error,
                             const bool recoveryFile)
{
    const QFileInfo target(filePath);
    if (!QDir().mkpath(target.absolutePath()))
    {
        if (error != nullptr) *error = QStringLiteral("Unable to create the session directory.");
        return false;
    }
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), SessionData::CurrentSchemaVersion);
    root.insert(QStringLiteral("savedAtUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("recovery"), recoveryFile);
    root.insert(QStringLiteral("explicitSessionPath"), session.ExplicitSessionPath);
    root.insert(QStringLiteral("explicitSaveTimeUtc"), session.ExplicitSaveTimeUtc.toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("activeClipId"), session.ActiveClipId.toString(QUuid::WithoutBraces));
    root.insert(QStringLiteral("activeSegmentId"), session.ActiveSegmentId.toString(QUuid::WithoutBraces));
    QJsonObject workflow;
    workflow.insert(QStringLiteral("destinationMode"), session.Destination.Mode == EOutputDestinationMode::FixedDirectory
                                                           ? QStringLiteral("fixed") : QStringLiteral("source-relative"));
    workflow.insert(QStringLiteral("sourceRelativeSubdirectory"), session.Destination.SourceRelativeSubdirectory);
    workflow.insert(QStringLiteral("fixedDirectory"), session.Destination.FixedDirectory);
    workflow.insert(QStringLiteral("outputProfile"), session.SelectedOutputProfile);
    workflow.insert(QStringLiteral("preserveMetadata"), session.PreserveMetadata);
    workflow.insert(QStringLiteral("recursiveFolderImport"), session.RecursiveFolderImport);
    workflow.insert(QStringLiteral("savedPrefixes"), QJsonArray::fromStringList(session.SavedPrefixes));
    workflow.insert(QStringLiteral("namingTemplate"), session.SelectedNamingTemplate);
    root.insert(QStringLiteral("workflow"), workflow);
    QJsonArray clips;
    const QDir sessionDirectory = target.dir();
    for (const Clip& clip : session.Clips) clips.append(ClipToJson(clip, sessionDirectory));
    root.insert(QStringLiteral("clips"), clips);

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit())
    {
        if (error != nullptr) *error = QStringLiteral("Unable to atomically save session: %1").arg(filePath);
        return false;
    }
    return true;
}

SessionLoadResult SessionRepository::Load(const QString& filePath)
{
    SessionLoadResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        result.Error = QStringLiteral("Unable to open session: %1").arg(filePath);
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        result.Error = QStringLiteral("Invalid session JSON: %1").arg(parseError.errorString());
        return result;
    }
    const QJsonObject root = document.object();
    const int version = root.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (version > SessionData::CurrentSchemaVersion)
    {
        result.Error = QStringLiteral("Session schema version %1 is newer than supported version %2.")
                           .arg(version).arg(SessionData::CurrentSchemaVersion);
        return result;
    }
    if (version < 1)
    {
        result.Error = QStringLiteral("Unsupported or missing session schema version.");
        return result;
    }
    result.Session.SchemaVersion = SessionData::CurrentSchemaVersion;
    result.Session.SavedAtUtc = QDateTime::fromString(root.value(QStringLiteral("savedAtUtc")).toString(), Qt::ISODateWithMs);
    result.Session.ExplicitSessionPath = root.value(QStringLiteral("explicitSessionPath")).toString();
    result.Session.ExplicitSaveTimeUtc = QDateTime::fromString(root.value(QStringLiteral("explicitSaveTimeUtc")).toString(), Qt::ISODateWithMs);
    result.Session.ActiveClipId = QUuid(root.value(QStringLiteral("activeClipId")).toString());
    result.Session.ActiveSegmentId = QUuid(root.value(QStringLiteral("activeSegmentId")).toString());
    const QJsonObject workflow = root.value(QStringLiteral("workflow")).toObject();
    result.Session.Destination.Mode = workflow.value(QStringLiteral("destinationMode")).toString() == QStringLiteral("fixed")
                                              ? EOutputDestinationMode::FixedDirectory : EOutputDestinationMode::SourceRelative;
    result.Session.Destination.SourceRelativeSubdirectory = workflow.value(QStringLiteral("sourceRelativeSubdirectory"))
                                                               .toString(QStringLiteral("ClipCutterOutput"));
    result.Session.Destination.FixedDirectory = workflow.value(QStringLiteral("fixedDirectory")).toString();
    result.Session.SelectedOutputProfile = workflow.value(QStringLiteral("outputProfile")).toString(QStringLiteral("fast-copy"));
    if (OutputProfiles::Find(result.Session.SelectedOutputProfile) == nullptr) result.Session.SelectedOutputProfile = QStringLiteral("fast-copy");
    result.Session.PreserveMetadata = workflow.value(QStringLiteral("preserveMetadata")).toBool(true);
    result.Session.RecursiveFolderImport = workflow.value(QStringLiteral("recursiveFolderImport")).toBool(false);
    for (const QJsonValue& value : workflow.value(QStringLiteral("savedPrefixes")).toArray())
        if (value.isString() && !value.toString().trimmed().isEmpty()) result.Session.SavedPrefixes.append(value.toString().trimmed());
    result.Session.SelectedNamingTemplate = workflow.value(QStringLiteral("namingTemplate")).toString(NamingTemplate::DefaultPattern());
    if (!NamingTemplate::Validate(result.Session.SelectedNamingTemplate).isEmpty())
        result.Session.SelectedNamingTemplate = NamingTemplate::DefaultPattern();

    const QDir sessionDirectory = QFileInfo(filePath).dir();
    QSet<QUuid> seenClipIds;
    QSet<QUuid> seenSegmentIds;
    for (const QJsonValue& clipValue : root.value(QStringLiteral("clips")).toArray())
    {
        if (!clipValue.isObject()) continue;
        const QJsonObject clipJson = clipValue.toObject();
        Clip clip;
        if (!ReadUuid(clipJson, QStringLiteral("id"), clip.Id))
        {
            result.Error = QStringLiteral("Session contains an invalid clip ID.");
            return result;
        }
        if (seenClipIds.contains(clip.Id))
        {
            result.Error = QStringLiteral("Session contains a duplicate clip ID.");
            return result;
        }
        seenClipIds.insert(clip.Id);
        clip.SourcePath = ResolvedSourcePath(clipJson.value(QStringLiteral("sourcePath")).toString(), sessionDirectory);
        clip.OriginalFileName = clipJson.value(QStringLiteral("originalFileName")).toString(QFileInfo(clip.SourcePath).fileName());
        clip.ThumbnailSourceFingerprint = clipJson.value(QStringLiteral("thumbnailSourceFingerprint")).toString();
        clip.MediaInformation.ProbeStatus = QFileInfo::exists(clip.SourcePath) ? EProbeStatus::Probing : EProbeStatus::Failed;
        if (!QFileInfo::exists(clip.SourcePath))
        {
            clip.MediaInformation.ProbeError = QStringLiteral("Source file is missing.");
            result.Session.MissingSourcePaths.append(clip.SourcePath);
            result.Warnings.append(QStringLiteral("Missing source: %1").arg(clip.SourcePath));
        }
        QJsonArray segmentValues = clipJson.value(QStringLiteral("segments")).toArray();
        if (version == 1 && segmentValues.isEmpty() &&
            (clipJson.value(QStringLiteral("segment")).isObject() || clipJson.contains(QStringLiteral("range")) ||
             clipJson.contains(QStringLiteral("startMs")) || clipJson.contains(QStringLiteral("trimStartMs"))))
        {
            QJsonObject migrated = MigratedSingleSegment(clipJson);
            if (QUuid(migrated.value(QStringLiteral("id")).toString()).isNull())
                migrated.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
            if (migrated.value(QStringLiteral("outputBaseName")).toString().isEmpty())
                migrated.insert(QStringLiteral("outputBaseName"), QFileInfo(clip.OriginalFileName).completeBaseName());
            if (migrated.value(QStringLiteral("outputExtension")).toString().isEmpty())
            {
                const QString suffix = QFileInfo(clip.OriginalFileName).suffix();
                migrated.insert(QStringLiteral("outputExtension"), suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix);
            }
            if (migrated.value(QStringLiteral("outputProfile")).toString().isEmpty())
                migrated.insert(QStringLiteral("outputProfile"), QStringLiteral("fast-copy"));
            segmentValues.append(migrated);
            result.Warnings.append(QStringLiteral("Migrated a version 1 single-segment clip to the version 2 segment collection."));
        }
        for (const QJsonValue& segmentValue : segmentValues)
        {
            if (!segmentValue.isObject()) continue;
            QString segmentError;
            const auto segment = SegmentFromJson(segmentValue.toObject(), segmentError);
            if (!segment.has_value())
            {
                result.Error = segmentError;
                return result;
            }
            if (seenSegmentIds.contains(segment->Id))
            {
                result.Error = QStringLiteral("Session contains a duplicate segment ID.");
                return result;
            }
            seenSegmentIds.insert(segment->Id);
            clip.Segments.append(*segment);
        }
        if (!clip.Segments.isEmpty()) result.Session.Clips.push_back(std::move(clip));
    }
    return result;
}

bool SessionRepository::ShouldOfferRecovery(const QString& recoveryPath, const QString& explicitSessionPath)
{
    const QFileInfo recovery(recoveryPath);
    if (!recovery.exists() || !recovery.isFile()) return false;
    const SessionLoadResult loaded = Load(recoveryPath);
    if (!loaded.IsValid()) return false;
    const QString explicitPath = explicitSessionPath.isEmpty() ? loaded.Session.ExplicitSessionPath : explicitSessionPath;
    if (explicitPath.isEmpty()) return true;
    const QFileInfo explicitFile(explicitPath);
    if (!explicitFile.exists()) return true;
    const QDateTime reference = loaded.Session.ExplicitSaveTimeUtc.isValid()
                                    ? loaded.Session.ExplicitSaveTimeUtc : explicitFile.lastModified().toUTC();
    const QDateTime recoveryTime = loaded.Session.SavedAtUtc.isValid() ? loaded.Session.SavedAtUtc
                                                                       : recovery.lastModified().toUTC();
    return recoveryTime > reference;
}

bool SessionRepository::RemoveRecovery(const QString& recoveryPath)
{
    return !QFileInfo::exists(recoveryPath) || QFile::remove(recoveryPath);
}

QString SessionRepository::RelinkSource(const QString& missingSourcePath, const QString& replacementPath,
                                        SessionData& session)
{
    const QFileInfo replacement(replacementPath);
    if (!replacement.exists() || !replacement.isFile()) return QStringLiteral("Replacement source does not exist.");
    for (Clip& clip : session.Clips)
    {
        if (QDir::cleanPath(clip.SourcePath).compare(QDir::cleanPath(missingSourcePath), Qt::CaseInsensitive) != 0) continue;
        clip.SourcePath = ResolvedSourcePath(replacementPath, QDir::current());
        clip.OriginalFileName = replacement.fileName();
        clip.MediaInformation = {};
        clip.MediaInformation.ProbeStatus = EProbeStatus::Probing;
        session.MissingSourcePaths.removeAll(missingSourcePath);
        return {};
    }
    return QStringLiteral("Missing source is not part of this session.");
}
} // namespace ClipCutter
