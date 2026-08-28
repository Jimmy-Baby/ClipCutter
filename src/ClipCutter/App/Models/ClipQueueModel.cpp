#include "App/Models/ClipQueueModel.h"

#include "Utility.h"
#include "Core/Export/OutputProfile.h"
#include "Core/Naming/NamingTemplate.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace ClipCutter
{
ClipQueueModel::ClipQueueModel(QObject* parent) : QAbstractTableModel(parent) {}

int ClipQueueModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    int count = 0;

    for (const Clip& clip : Clips_)
    {
        count += clip.Segments.size();
    }

    return count;
}

int ClipQueueModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ClipQueueModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.column() < 0 || index.column() >= ColumnCount)
    {
        return {};
    }

    const ConstRowRef rowRef = GetRowRef(index.row());

    if (rowRef.ClipValue == nullptr || rowRef.SegmentValue == nullptr)
    {
        return {};
    }

    const Clip& clip = *rowRef.ClipValue;
    const Segment& segment = *rowRef.SegmentValue;

    switch (role)
    {
    case ClipIdRole:
        return clip.Id;
    case SegmentIdRole:
        return segment.Id;
    case SourcePathRole:
        return clip.SourcePath;
    case StartMillisecondsRole:
        return QVariant::fromValue<qint64>(segment.Range.Start().count());
    case EndMillisecondsRole:
        return QVariant::fromValue<qint64>(segment.Range.End().count());
    case DurationMillisecondsRole:
        return QVariant::fromValue<qint64>(segment.Range.Duration().count());
    case SkippedRole:
        return segment.Skipped;
    case OutputFileNameRole:
        return segment.OutputFileName();
    case ExportProfileRole:
        return segment.ExportProfileId;
    case ExportStateRole:
        return QVariant::fromValue(segment.ExportState);
    case ExportProgressRole:
        return segment.ExportProgress.has_value() ? QVariant(*segment.ExportProgress) : QVariant();
    case ExportLogRole:
        return segment.ExportLog;
    case PrefixRole:
        return segment.Prefix.value_or(QString());
    case NamingTemplateRole:
        return segment.NamingTemplatePattern.value_or(QString());
    case Qt::ToolTipRole:
        if (clip.MediaInformation.ProbeStatus == EProbeStatus::Failed)
            return clip.MediaInformation.ProbeError.value_or(QStringLiteral("Media probing failed."));
        return segment.ExportLog.isEmpty() ? QVariant() : QVariant(segment.ExportLog);
    case Qt::CheckStateRole:
        return index.column() == SkipColumn ? QVariant::fromValue(segment.Skipped ? Qt::Checked : Qt::Unchecked)
                                            : QVariant();
    case Qt::EditRole:
        return index.column() == OutputNameColumn ? QVariant(segment.OutputBaseName) : QVariant();
    case Qt::DisplayRole:
        switch (index.column())
        {
        case SkipColumn:
            return {};
        case SourceNameColumn:
            return clip.OriginalFileName;
        case OutputNameColumn:
            return segment.OutputFileName();
        case StartColumn:
            return TimeText(segment.Range.Start());
        case EndColumn:
            return TimeText(segment.Range.End());
        case DurationColumn:
            return TimeText(segment.Range.Duration());
        case StatusColumn:
            return StatusText(clip, segment);
        default:
            return {};
        }
    default:
        return {};
    }
}

QVariant ClipQueueModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    switch (section)
    {
    case SkipColumn:
        return QStringLiteral("Skip?");
    case SourceNameColumn:
        return QStringLiteral("Source");
    case OutputNameColumn:
        return QStringLiteral("Output");
    case StartColumn:
        return QStringLiteral("Start");
    case EndColumn:
        return QStringLiteral("End");
    case DurationColumn:
        return QStringLiteral("Selected");
    case StatusColumn:
        return QStringLiteral("Status");
    default:
        return {};
    }
}

Qt::ItemFlags ClipQueueModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    const ConstRowRef rowRef = GetRowRef(index.row());
    const bool editable = rowRef.SegmentValue != nullptr && !rowRef.SegmentValue->ExportLocked &&
                          IsRuntimeEditable(rowRef.SegmentValue->ExportState);

    if (editable && index.column() == SkipColumn)
    {
        flags |= Qt::ItemIsUserCheckable;
    }

    if (editable && index.column() == OutputNameColumn)
    {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

bool ClipQueueModel::setData(const QModelIndex& index, const QVariant& value, const int role)
{
    if (!index.isValid())
    {
        return false;
    }

    const QUuid segmentId = SegmentIdAtRow(index.row());
    const Segment* segment = FindSegment(segmentId);

    if (segment == nullptr || segment->ExportLocked || !IsRuntimeEditable(segment->ExportState))
    {
        return false;
    }

    if (EditInterceptor_) return EditInterceptor_(index, value, role);

    if (index.column() == SkipColumn && role == Qt::CheckStateRole)
    {
        return UpdateSkipState(segmentId, value.toInt() == Qt::Checked);
    }

    if (index.column() == OutputNameColumn && role == Qt::EditRole)
    {
        return UpdateOutputName(segmentId, value.toString());
    }

    return false;
}

void ClipQueueModel::SetEditInterceptor(EditInterceptor interceptor)
{
    EditInterceptor_ = std::move(interceptor);
}

QHash<int, QByteArray> ClipQueueModel::roleNames() const
{
    QHash<int, QByteArray> names = QAbstractTableModel::roleNames();
    names.insert(ClipIdRole, "clipId");
    names.insert(SegmentIdRole, "segmentId");
    names.insert(SourcePathRole, "sourcePath");
    names.insert(StartMillisecondsRole, "startMilliseconds");
    names.insert(EndMillisecondsRole, "endMilliseconds");
    names.insert(DurationMillisecondsRole, "durationMilliseconds");
    names.insert(SkippedRole, "skipped");
    names.insert(OutputFileNameRole, "outputFileName");
    names.insert(ExportProfileRole, "exportProfile");
    names.insert(ExportStateRole, "exportState");
    names.insert(ExportProgressRole, "exportProgress");
    names.insert(ExportLogRole, "exportLog");
    names.insert(PrefixRole, "prefix");
    names.insert(NamingTemplateRole, "namingTemplate");
    names.insert(MediaProbeRole, "mediaProbe");

    return names;
}

void ClipQueueModel::AddClip(Clip clip)
{
    QVector<Clip> clips;
    clips.append(std::move(clip));
    AddClips(std::move(clips));
}

void ClipQueueModel::AddClips(QVector<Clip> clips)
{
    if (clips.isEmpty())
    {
        return;
    }

    int addedRows = 0;

    for (const Clip& clip : clips)
    {
        addedRows += clip.Segments.size();
    }

    const int firstRow = rowCount();

    if (addedRows > 0)
    {
        beginInsertRows({}, firstRow, firstRow + addedRows - 1);
    }

    Clips_.reserve(Clips_.size() + static_cast<std::size_t>(clips.size()));

    for (Clip& clip : clips)
    {
        Clips_.push_back(std::move(clip));
    }

    if (addedRows > 0)
    {
        endInsertRows();
    }
}

bool ClipQueueModel::InsertClip(const int clipIndex, Clip clip)
{
    if (clipIndex < 0 || clipIndex > static_cast<int>(Clips_.size()) || clip.Id.isNull() || FindClip(clip.Id) != nullptr)
        return false;
    for (const Segment& segment : clip.Segments)
        if (segment.Id.isNull() || FindSegment(segment.Id) != nullptr) return false;
    const int firstRow = FirstRowForClipIndex(clipIndex);
    if (!clip.Segments.isEmpty())
        beginInsertRows({}, firstRow, firstRow + clip.Segments.size() - 1);
    Clips_.insert(Clips_.begin() + clipIndex, std::move(clip));
    if (!Clips_.at(static_cast<std::size_t>(clipIndex)).Segments.isEmpty()) endInsertRows();
    return true;
}

void ClipQueueModel::ReplaceClips(std::vector<Clip> clips)
{
    beginResetModel();
    Clips_ = std::move(clips);
    endResetModel();
}

void ClipQueueModel::ClearClips()
{
    if (Clips_.empty())
    {
        return;
    }

    beginResetModel();
    Clips_.clear();
    endResetModel();
}

bool ClipQueueModel::RemoveEntries(const QSet<QUuid>& segmentIds)
{
    bool removed = false;

    for (int row = rowCount() - 1; row >= 0; --row)
    {
        const QUuid segmentId = SegmentIdAtRow(row);

        if (!segmentIds.contains(segmentId))
        {
            continue;
        }

        beginRemoveRows({}, row, row);

        for (auto clipIterator = Clips_.begin(); clipIterator != Clips_.end(); ++clipIterator)
        {
            QVector<Segment>& segments = clipIterator->Segments;
            const auto segmentIterator =
                std::find_if(segments.begin(), segments.end(),
                             [&segmentId](const Segment& segment) { return segment.Id == segmentId; });

            if (segmentIterator == segments.end())
            {
                continue;
            }

            segments.erase(segmentIterator);

            if (segments.isEmpty())
            {
                Clips_.erase(clipIterator);
            }

            break;
        }

        endRemoveRows();
        removed = true;
    }

    return removed;
}

bool ClipQueueModel::InsertSegment(const QUuid& clipId, Segment segment, int segmentIndex)
{
    Clip* clip = FindClip(clipId);
    if (clip == nullptr || segment.Id.isNull() || FindSegment(segment.Id) != nullptr) return false;
    if (segmentIndex < 0) segmentIndex = clip->Segments.size();
    if (segmentIndex < 0 || segmentIndex > clip->Segments.size()) return false;
    const int clipIndex = ClipIndex(clipId);
    const int row = FirstRowForClipIndex(clipIndex) + segmentIndex;
    beginInsertRows({}, row, row);
    clip->Segments.insert(segmentIndex, std::move(segment));
    endInsertRows();
    return true;
}

bool ClipQueueModel::RemoveSegment(const QUuid& segmentId)
{
    return RemoveEntries({segmentId});
}

bool ClipQueueModel::MoveSegment(const QUuid& segmentId, int destinationIndex)
{
    const QUuid clipId = ClipIdForSegment(segmentId);
    Clip* clip = FindClip(clipId);
    const int sourceIndex = SegmentIndex(segmentId);
    if (clip == nullptr || sourceIndex < 0 || destinationIndex < 0 || destinationIndex >= clip->Segments.size())
        return false;
    if (sourceIndex == destinationIndex) return true;
    const int firstRow = FirstRowForClipIndex(ClipIndex(clipId));
    const int destinationChild = destinationIndex > sourceIndex ? firstRow + destinationIndex + 1
                                                                 : firstRow + destinationIndex;
    if (!beginMoveRows({}, firstRow + sourceIndex, firstRow + sourceIndex, {}, destinationChild)) return false;
    clip->Segments.move(sourceIndex, destinationIndex);
    endMoveRows();
    return true;
}

bool ClipQueueModel::AssignDeterministicName(Clip& clip, Segment& segment, const int segmentIndex,
                                             const QString& namingPattern, const QDate& date, QString* error) const
{
    const QString pattern = namingPattern.isEmpty() ? NamingTemplate::DefaultPattern() : namingPattern;
    NamingTemplateContext context;
    context.Original = QFileInfo(clip.OriginalFileName).completeBaseName();
    context.Prefix = segment.Prefix.value_or(QString());
    context.Index = FirstRowForClipIndex(ClipIndex(clip.Id)) + segmentIndex + 1;
    context.Segment = segmentIndex + 1;
    context.Date = date;
    context.Profile = segment.ExportProfileId;
    const NamingTemplateResult rendered = NamingTemplate::Render(pattern, context);
    if (!rendered.IsValid())
    {
        if (error != nullptr) *error = rendered.Error;
        return false;
    }
    QString candidate = rendered.Value;
    QSet<QString> existing;
    for (const Segment& value : clip.Segments) existing.insert(value.OutputBaseName.toCaseFolded());
    if (existing.contains(candidate.toCaseFolded()))
    {
        const QString base = candidate;
        int suffix = 2;
        do candidate = QStringLiteral("%1_%2").arg(base).arg(suffix++, 2, 10, QLatin1Char('0'));
        while (existing.contains(candidate.toCaseFolded()));
    }
    segment.OutputBaseName = candidate;
    segment.NamingTemplatePattern = pattern;
    return true;
}

std::optional<QUuid> ClipQueueModel::CreateSegment(const QUuid& clipId, const TimeRange& range,
                                                    const QString& namingPattern, const QDate& date, QString* error)
{
    Clip* clip = FindClip(clipId);
    if (clip == nullptr)
    {
        if (error != nullptr) *error = QStringLiteral("Clip was not found.");
        return std::nullopt;
    }
    if (!range.IsValid(clip->MediaInformation.Duration, true, error)) return std::nullopt;
    Segment segment;
    segment.Range = range;
    segment.TrimRangeUserEdited = true;
    segment.ExportProfileId = clip->Segments.isEmpty() ? QStringLiteral("fast-copy") : clip->Segments.constFirst().ExportProfileId;
    if (const OutputProfile* profile = OutputProfiles::Find(segment.ExportProfileId))
        segment.OutputExtension = OutputProfiles::ExtensionFor(*profile, clip->SourcePath);
    if (!AssignDeterministicName(*clip, segment, clip->Segments.size(), namingPattern, date, error)) return std::nullopt;
    const QUuid id = segment.Id;
    return InsertSegment(clipId, std::move(segment)) ? std::optional<QUuid>{id} : std::nullopt;
}

std::optional<QUuid> ClipQueueModel::CreateSegmentAtPlayhead(const QUuid& clipId,
                                                              const std::chrono::milliseconds playhead,
                                                              const QString& namingPattern,
                                                              const QDate& date, QString* error)
{
    const Clip* clip = FindClip(clipId);
    if (clip == nullptr || !clip->MediaInformation.Duration.has_value())
    {
        if (error != nullptr) *error = QStringLiteral("A known source duration is required.");
        return std::nullopt;
    }
    const auto range = TimeRange::Create(playhead, *clip->MediaInformation.Duration,
                                         clip->MediaInformation.Duration, true, error);
    return range.has_value() ? CreateSegment(clipId, *range, namingPattern, date, error) : std::nullopt;
}

std::optional<QUuid> ClipQueueModel::DuplicateSegment(const QUuid& segmentId, const QString& namingPattern,
                                                       const QDate& date, QString* error)
{
    const QUuid clipId = ClipIdForSegment(segmentId);
    Clip* clip = FindClip(clipId);
    const int sourceIndex = SegmentIndex(segmentId);
    if (clip == nullptr || sourceIndex < 0)
    {
        if (error != nullptr) *error = QStringLiteral("Segment was not found.");
        return std::nullopt;
    }
    Segment duplicate = clip->Segments.at(sourceIndex);
    duplicate.Id = QUuid::createUuid();
    duplicate.ExportState = EExportState::Pending;
    duplicate.ExportProgress.reset();
    duplicate.ExportLog.clear();
    duplicate.ExportLocked = false;
    const QString pattern = namingPattern.isEmpty() ? duplicate.NamingTemplatePattern.value_or(NamingTemplate::DefaultPattern())
                                                     : namingPattern;
    if (!AssignDeterministicName(*clip, duplicate, sourceIndex + 1, pattern, date, error)) return std::nullopt;
    const QUuid id = duplicate.Id;
    return InsertSegment(clipId, std::move(duplicate), sourceIndex + 1) ? std::optional<QUuid>{id} : std::nullopt;
}

Clip* ClipQueueModel::FindClip(const QUuid& clipId)
{
    const auto found =
        std::find_if(Clips_.begin(), Clips_.end(), [&clipId](const Clip& clip) { return clip.Id == clipId; });

    return found == Clips_.end() ? nullptr : &*found;
}

const Clip* ClipQueueModel::FindClip(const QUuid& clipId) const
{
    const auto found =
        std::find_if(Clips_.cbegin(), Clips_.cend(), [&clipId](const Clip& clip) { return clip.Id == clipId; });

    return found == Clips_.cend() ? nullptr : &*found;
}

Segment* ClipQueueModel::FindSegment(const QUuid& segmentId)
{
    for (Clip& clip : Clips_)
    {
        const auto found = std::find_if(clip.Segments.begin(), clip.Segments.end(),
                                        [&segmentId](const Segment& segment) { return segment.Id == segmentId; });

        if (found != clip.Segments.end())
        {
            return &*found;
        }
    }

    return nullptr;
}

const Segment* ClipQueueModel::FindSegment(const QUuid& segmentId) const
{
    for (const Clip& clip : Clips_)
    {
        const auto found = std::find_if(clip.Segments.cbegin(), clip.Segments.cend(),
                                        [&segmentId](const Segment& segment) { return segment.Id == segmentId; });

        if (found != clip.Segments.cend())
        {
            return &*found;
        }
    }

    return nullptr;
}

int ClipQueueModel::RowForClip(const QUuid& clipId) const
{
    int row = 0;

    for (const Clip& clip : Clips_)
    {
        if (clip.Id == clipId)
        {
            return clip.Segments.isEmpty() ? -1 : row;
        }

        row += clip.Segments.size();
    }

    return -1;
}

int ClipQueueModel::RowForSegment(const QUuid& segmentId) const
{
    int row = 0;

    for (const Clip& clip : Clips_)
    {
        for (const Segment& segment : clip.Segments)
        {
            if (segment.Id == segmentId)
            {
                return row;
            }

            ++row;
        }
    }

    return -1;
}

int ClipQueueModel::ClipIndex(const QUuid& clipId) const
{
    for (int index = 0; index < static_cast<int>(Clips_.size()); ++index)
        if (Clips_.at(static_cast<std::size_t>(index)).Id == clipId) return index;
    return -1;
}

int ClipQueueModel::SegmentIndex(const QUuid& segmentId) const
{
    for (const Clip& clip : Clips_)
        for (int index = 0; index < clip.Segments.size(); ++index)
            if (clip.Segments.at(index).Id == segmentId) return index;
    return -1;
}

QUuid ClipQueueModel::ClipIdForSegment(const QUuid& segmentId) const
{
    for (const Clip& clip : Clips_)
        for (const Segment& segment : clip.Segments)
            if (segment.Id == segmentId) return clip.Id;
    return {};
}

QUuid ClipQueueModel::ClipIdAtRow(const int row) const
{
    const ConstRowRef rowRef = GetRowRef(row);
    return rowRef.ClipValue == nullptr ? QUuid() : rowRef.ClipValue->Id;
}

QUuid ClipQueueModel::SegmentIdAtRow(const int row) const
{
    const ConstRowRef rowRef = GetRowRef(row);
    return rowRef.SegmentValue == nullptr ? QUuid() : rowRef.SegmentValue->Id;
}

bool ClipQueueModel::UpdateSkipState(const QUuid& segmentId, const bool skipped)
{
    Segment* segment = FindSegment(segmentId);

    if (segment == nullptr || segment->Skipped == skipped)
    {
        return segment != nullptr;
    }

    segment->Skipped = skipped;
    const int row = RowForSegment(segmentId);
    emit dataChanged(index(row, SkipColumn), index(row, SkipColumn), {Qt::CheckStateRole, SkippedRole});

    return true;
}

void ClipQueueModel::UpdateAllSkipStates(const bool skipped)
{
    bool changed = false;

    for (Clip& clip : Clips_)
    {
        for (Segment& segment : clip.Segments)
        {
            changed = changed || segment.Skipped != skipped;
            segment.Skipped = skipped;
        }
    }

    if (changed && rowCount() > 0)
    {
        emit dataChanged(index(0, SkipColumn), index(rowCount() - 1, SkipColumn), {Qt::CheckStateRole, SkippedRole});
    }
}

bool ClipQueueModel::UpdateSkipStates(const QSet<QUuid>& segmentIds, const bool skipped)
{
    bool found = segmentIds.isEmpty();
    for (int row = 0; row < rowCount(); ++row)
    {
        const QUuid id = SegmentIdAtRow(row);
        if (!segmentIds.isEmpty() && !segmentIds.contains(id)) continue;
        found = true;
        UpdateSkipState(id, skipped);
    }
    return found;
}

bool ClipQueueModel::InvertSkipStates(const QSet<QUuid>& segmentIds)
{
    bool found = segmentIds.isEmpty();
    for (int row = 0; row < rowCount(); ++row)
    {
        const QUuid id = SegmentIdAtRow(row);
        if (!segmentIds.isEmpty() && !segmentIds.contains(id)) continue;
        Segment* segment = FindSegment(id);
        if (segment != nullptr) { found = true; UpdateSkipState(id, !segment->Skipped); }
    }
    return found;
}

bool ClipQueueModel::UpdateOutputBaseName(const QUuid& segmentId, const QString& outputBaseName)
{
    Segment* segment = FindSegment(segmentId);
    const QString normalized = outputBaseName;

    if (segment == nullptr)
    {
        return false;
    }

    if (segment->OutputBaseName == normalized)
    {
        return true;
    }

    segment->OutputBaseName = normalized;
    const int row = RowForSegment(segmentId);
    emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn),
                     {Qt::DisplayRole, Qt::EditRole, OutputFileNameRole});

    return true;
}

bool ClipQueueModel::UpdateOutputName(const QUuid& segmentId, const QString& outputFileName)
{
    Segment* segment = FindSegment(segmentId);
    const QString normalized = outputFileName;
    const QFileInfo fileInfo(normalized);

    if (segment == nullptr)
    {
        return false;
    }

    const QString baseName = fileInfo.suffix().isEmpty() ? normalized : fileInfo.completeBaseName();
    const QString extension = segment->OutputExtension;

    if (segment->OutputBaseName == baseName && segment->OutputExtension == extension)
    {
        return true;
    }

    segment->OutputBaseName = baseName;
    segment->OutputExtension = extension;
    const int row = RowForSegment(segmentId);
    emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn),
                     {Qt::DisplayRole, Qt::EditRole, OutputFileNameRole});

    return true;
}

bool ClipQueueModel::UpdateNamingTemplateData(const QUuid& segmentId, const QString& outputBaseName,
                                              std::optional<QString> namingTemplatePattern)
{
    Segment* segment = FindSegment(segmentId);

    if (segment == nullptr) return false;

    const bool changed = segment->OutputBaseName != outputBaseName ||
                         segment->NamingTemplatePattern != namingTemplatePattern;
    segment->OutputBaseName = outputBaseName;
    segment->NamingTemplatePattern = std::move(namingTemplatePattern);

    if (changed)
    {
        const int row = RowForSegment(segmentId);
        emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn),
                         {Qt::DisplayRole, Qt::EditRole, OutputFileNameRole, NamingTemplateRole});
    }

    return true;
}

bool ClipQueueModel::UpdateTrimRange(const QUuid& segmentId, const TimeRange& range, QString* error)
{
    for (Clip& clip : Clips_)
    {
        for (Segment& segment : clip.Segments)
        {
            if (segment.Id != segmentId)
            {
                continue;
            }

            if (!range.IsValid(clip.MediaInformation.Duration, false, error))
            {
                return false;
            }

            segment.TrimRangeUserEdited = true;
            if (segment.Range == range)
            {
                return true;
            }

            segment.Range = range;
            const int row = RowForSegment(segmentId);
            emit dataChanged(index(row, StartColumn), index(row, DurationColumn),
                             {Qt::DisplayRole, StartMillisecondsRole, EndMillisecondsRole, DurationMillisecondsRole});

            return true;
        }
    }

    if (error != nullptr)
    {
        *error = QStringLiteral("Segment was not found.");
    }

    return false;
}

bool ClipQueueModel::UpdateMediaDuration(const QUuid& clipId, const std::chrono::milliseconds duration, QString* error)
{
    if (duration.count() < 0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Media duration cannot be negative.");
        }

        return false;
    }

    Clip* clip = FindClip(clipId);

    if (clip == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Clip was not found.");
        }

        return false;
    }

    clip->MediaInformation.Duration = duration;
    clip->MediaInformation.ProbeStatus = EProbeStatus::Ready;

    for (Segment& segment : clip->Segments)
    {
        if (!segment.TrimRangeUserEdited && segment.Range.Duration().count() == 0)
            segment.Range = *TimeRange::Create(std::chrono::milliseconds{0}, duration);
    }

    const int firstRow = RowForClip(clipId);

    if (firstRow >= 0)
    {
        emit dataChanged(index(firstRow, StartColumn), index(firstRow + clip->Segments.size() - 1, StatusColumn),
                         {Qt::DisplayRole, MediaProbeRole});
    }

    return true;
}

bool ClipQueueModel::UpdateMediaInfo(const QUuid& clipId, const QString& expectedSourcePath, const MediaInfo& info)
{
    Clip* clip = FindClip(clipId);
    if (clip == nullptr || QDir::cleanPath(clip->SourcePath) != QDir::cleanPath(expectedSourcePath))
        return false;
    clip->MediaInformation = info;
    if (info.ProbeStatus == EProbeStatus::Ready && info.Duration.has_value())
    {
        for (Segment& segment : clip->Segments)
        {
            if (!segment.TrimRangeUserEdited && segment.Range.Duration().count() == 0)
                segment.Range = *TimeRange::Create(std::chrono::milliseconds{0}, *info.Duration);
        }
    }
    const int firstRow = RowForClip(clipId);
    if (firstRow >= 0)
        emit dataChanged(index(firstRow, StartColumn), index(firstRow + clip->Segments.size() - 1, StatusColumn),
                         {Qt::DisplayRole, MediaProbeRole});
    return true;
}

bool ClipQueueModel::RelinkClipSource(const QUuid& clipId, const QString& replacementPath, QString* error)
{
    const QFileInfo replacement(replacementPath);
    if (!replacement.exists() || !replacement.isFile())
    {
        if (error != nullptr) *error = QStringLiteral("Replacement source does not exist.");
        return false;
    }
    const QString canonical = QDir::cleanPath(replacement.canonicalFilePath().isEmpty()
                                                 ? replacement.absoluteFilePath() : replacement.canonicalFilePath());
    for (const Clip& candidate : Clips_)
    {
        if (candidate.Id != clipId && QDir::cleanPath(candidate.SourcePath).compare(canonical, Qt::CaseInsensitive) == 0)
        {
            if (error != nullptr) *error = QStringLiteral("Replacement source is already in the queue.");
            return false;
        }
    }
    Clip* clip = FindClip(clipId);
    if (clip == nullptr)
    {
        if (error != nullptr) *error = QStringLiteral("Clip was not found.");
        return false;
    }
    clip->SourcePath = canonical;
    clip->OriginalFileName = replacement.fileName();
    clip->MediaInformation = {};
    clip->MediaInformation.ProbeStatus = EProbeStatus::Probing;
    for (Segment& segment : clip->Segments)
        if (const OutputProfile* profile = OutputProfiles::Find(segment.ExportProfileId))
            segment.OutputExtension = OutputProfiles::ExtensionFor(*profile, canonical);
    const int first = RowForClip(clipId);
    if (first >= 0)
        emit dataChanged(index(first, SourceNameColumn), index(first + clip->Segments.size() - 1, StatusColumn));
    return true;
}

bool ClipQueueModel::ApplyPrefix(const QUuid& segmentId, const QString& prefix)
{
    Segment* segment = FindSegment(segmentId);

    if (segment == nullptr)
    {
        return false;
    }

    const QString normalized = prefix.trimmed();
    segment->Prefix = normalized.isEmpty() ? std::nullopt : std::optional<QString>(normalized);
    const int row = RowForSegment(segmentId);
    emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn), {Qt::DisplayRole, OutputFileNameRole});

    return true;
}

bool ClipQueueModel::ClearPrefix(const QUuid& segmentId)
{
    Segment* segment = FindSegment(segmentId);

    if (segment == nullptr)
    {
        return false;
    }

    if (!segment->Prefix.has_value())
    {
        return true;
    }

    segment->Prefix.reset();
    const int row = RowForSegment(segmentId);
    emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn), {Qt::DisplayRole, OutputFileNameRole});

    return true;
}

bool ClipQueueModel::ApplyPrefixTo(const QSet<QUuid>& segmentIds, const QString& prefix)
{
    bool found = segmentIds.isEmpty();
    for (int row = 0; row < rowCount(); ++row)
    {
        const QUuid id = SegmentIdAtRow(row);
        if (!segmentIds.isEmpty() && !segmentIds.contains(id)) continue;
        found = ApplyPrefix(id, prefix) || found;
    }
    return found;
}

bool ClipQueueModel::ClearPrefixes(const QSet<QUuid>& segmentIds)
{
    bool found = segmentIds.isEmpty();
    for (int row = 0; row < rowCount(); ++row)
    {
        const QUuid id = SegmentIdAtRow(row);
        if (!segmentIds.isEmpty() && !segmentIds.contains(id)) continue;
        found = ClearPrefix(id) || found;
    }
    return found;
}

bool ClipQueueModel::ApplyNamingTemplate(const QSet<QUuid>& segmentIds, const QString& pattern,
                                         const QDate& date, QString* error)
{
    QVector<QUuid> ids;
    QVector<NamingTemplateContext> contexts;
    for (int row = 0; row < rowCount(); ++row)
    {
        const QUuid id = SegmentIdAtRow(row);
        if (!segmentIds.isEmpty() && !segmentIds.contains(id)) continue;
        const RowRef ref = GetRowRef(row);
        NamingTemplateContext context;
        context.Original = QFileInfo(ref.ClipValue->OriginalFileName).completeBaseName();
        context.Prefix = ref.SegmentValue->Prefix.value_or(QString());
        context.Index = row + 1;
        context.Date = date;
        context.Profile = ref.SegmentValue->ExportProfileId;
        int segmentNumber = 1;
        for (const Segment& candidate : ref.ClipValue->Segments)
        {
            if (candidate.Id == id) break;
            ++segmentNumber;
        }
        context.Segment = segmentNumber;
        ids.append(id);
        contexts.append(context);
    }
    const NamingBatchResult rendered = NamingTemplate::RenderBatch(pattern, contexts);
    if (!rendered.IsValid())
    {
        if (error != nullptr)
        {
            QStringList messages = rendered.Errors;
            if (!rendered.Duplicates.isEmpty()) messages.append(QStringLiteral("Duplicate rendered outputs: %1").arg(rendered.Duplicates.join(QStringLiteral(", "))));
            *error = messages.join(QLatin1Char('\n'));
        }
        return false;
    }
    for (int index = 0; index < ids.size(); ++index)
    {
        UpdateNamingTemplateData(ids.at(index), rendered.Values.at(index), pattern);
    }
    return true;
}

bool ClipQueueModel::ApplyExportProfile(const QSet<QUuid>& segmentIds, const QString& profileId)
{
    const OutputProfile* profile = OutputProfiles::Find(profileId);
    if (profile == nullptr) return false;
    bool found = segmentIds.isEmpty();
    for (int row = 0; row < rowCount(); ++row)
    {
        const QUuid id = SegmentIdAtRow(row);
        if (!segmentIds.isEmpty() && !segmentIds.contains(id)) continue;
        RowRef ref = GetRowRef(row);
        found = true;
        ref.SegmentValue->ExportProfileId = profileId;
        ref.SegmentValue->OutputExtension = OutputProfiles::ExtensionFor(*profile, ref.ClipValue->SourcePath);
        emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn),
                         {Qt::DisplayRole, OutputFileNameRole, ExportProfileRole});
    }
    return found;
}

bool ClipQueueModel::ResetTrimRanges(const QSet<QUuid>& segmentIds)
{
    bool found = segmentIds.isEmpty();
    for (int row = 0; row < rowCount(); ++row)
    {
        const QUuid id = SegmentIdAtRow(row);
        if (!segmentIds.isEmpty() && !segmentIds.contains(id)) continue;
        RowRef ref = GetRowRef(row);
        if (!ref.ClipValue->MediaInformation.Duration.has_value()) continue;
        ref.SegmentValue->Range = *TimeRange::Create(std::chrono::milliseconds{0}, *ref.ClipValue->MediaInformation.Duration);
        ref.SegmentValue->TrimRangeUserEdited = false;
        found = true;
        emit dataChanged(index(row, StartColumn), index(row, DurationColumn),
                         {Qt::DisplayRole, StartMillisecondsRole, EndMillisecondsRole, DurationMillisecondsRole});
    }
    return found;
}

void ClipQueueModel::ClearPrefixFromAll(const QString& prefix)
{
    for (int row = 0; row < rowCount(); ++row)
    {
        RowRef rowRef = GetRowRef(row);

        if (rowRef.SegmentValue->Prefix.has_value() &&
            rowRef.SegmentValue->Prefix->compare(prefix, Qt::CaseInsensitive) == 0)
        {
            rowRef.SegmentValue->Prefix.reset();
            emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn),
                             {Qt::DisplayRole, OutputFileNameRole});
        }
    }
}

void ClipQueueModel::UpdateAllExportProfiles(const QString& profileId)
{
    const OutputProfile* profile = OutputProfiles::Find(profileId);
    if (profile == nullptr) return;
    bool changed = false;

    for (Clip& clip : Clips_)
    {
        for (Segment& segment : clip.Segments)
        {
            changed = changed || segment.ExportProfileId != profileId;
            segment.ExportProfileId = profileId;
            segment.OutputExtension = OutputProfiles::ExtensionFor(*profile, clip.SourcePath);
        }
    }

    if (changed && rowCount() > 0)
    {
        emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1), {ExportProfileRole});
    }
}

bool ClipQueueModel::UpdateExportState(const QUuid& segmentId, const EExportState state)
{
    Segment* segment = FindSegment(segmentId);

    if (segment == nullptr || segment->ExportState == state)
    {
        return segment != nullptr;
    }

    segment->ExportState = state;
    const int row = RowForSegment(segmentId);
    emit dataChanged(index(row, StatusColumn), index(row, StatusColumn), {Qt::DisplayRole, ExportStateRole});

    return true;
}

bool ClipQueueModel::UpdateExportProgress(const QUuid& segmentId, std::optional<double> progress)
{
    Segment* segment = FindSegment(segmentId);

    if (segment == nullptr)
    {
        return false;
    }

    segment->ExportProgress = progress;
    const int row = RowForSegment(segmentId);
    emit dataChanged(index(row, StatusColumn), index(row, StatusColumn), {Qt::DisplayRole, ExportProgressRole});

    return true;
}

bool ClipQueueModel::AppendExportLog(const QUuid& segmentId, const QString& text)
{
    Segment* segment = FindSegment(segmentId);

    if (segment == nullptr)
    {
        return false;
    }

    segment->ExportLog.append(text);

    if (segment->ExportLog.size() > KMaximumExportLogCharacters)
    {
        segment->ExportLog.remove(0, segment->ExportLog.size() - KMaximumExportLogCharacters);
    }

    const int row = RowForSegment(segmentId);
    emit dataChanged(index(row, StatusColumn), index(row, StatusColumn), {Qt::ToolTipRole, ExportLogRole});

    return true;
}

bool ClipQueueModel::ResetExportJobRuntime(const QUuid& segmentId)
{
    Segment* segment = FindSegment(segmentId);

    if (segment == nullptr)
    {
        return false;
    }

    segment->ExportState = EExportState::Pending;
    segment->ExportProgress.reset();
    segment->ExportLog.clear();
    const int row = RowForSegment(segmentId);
    emit dataChanged(index(row, StatusColumn), index(row, StatusColumn),
                     {Qt::DisplayRole, Qt::ToolTipRole, ExportStateRole, ExportProgressRole, ExportLogRole});

    return true;
}

void ClipQueueModel::SetExportRuntimeLocked(const bool locked)
{
    for (Clip& clip : Clips_)
    {
        for (Segment& segment : clip.Segments)
        {
            segment.ExportLocked = locked;
        }
    }

    if (rowCount() > 0)
    {
        emit dataChanged(index(0, SkipColumn), index(rowCount() - 1, OutputNameColumn));
    }
}

void ClipQueueModel::ResetExportRuntime()
{
    for (Clip& clip : Clips_)
    {
        for (Segment& segment : clip.Segments)
        {
            segment.ExportState = segment.Skipped ? EExportState::Skipped : EExportState::Pending;
            segment.ExportProgress.reset();
            segment.ExportLog.clear();
            segment.ExportLocked = false;
        }
    }

    if (rowCount() > 0)
    {
        emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
    }
}

QVector<ExportSegment> ClipQueueModel::ExportSegments(QStringList* errors) const
{
    QVector<ExportSegment> result;

    if (errors != nullptr)
    {
        errors->clear();
    }

    for (const Clip& clip : Clips_)
    {
        for (const Segment& segment : clip.Segments)
        {
            QString error;

            if (!segment.Skipped && !clip.MediaInformation.IsReliableForExport())
            {
                error = clip.MediaInformation.ProbeStatus == EProbeStatus::Failed
                            ? clip.MediaInformation.ProbeError.value_or(QStringLiteral("Media probing failed."))
                            : QStringLiteral("Media inspection has not completed with a reliable duration and video stream.");
                if (errors != nullptr) errors->append(QStringLiteral("%1: %2").arg(clip.OriginalFileName, error));
                continue;
            }
            if (!segment.Skipped && !segment.Range.IsValid(clip.MediaInformation.Duration, true, &error))
            {
                if (errors != nullptr)
                {
                    errors->append(QStringLiteral("%1: %2").arg(clip.OriginalFileName, error));
                }

                continue;
            }

            ExportSegment exportSegment
            {
                clip.Id,
                segment.Id,
                clip.SourcePath,
                (segment.NamingTemplatePattern.has_value() ? QString() : segment.Prefix.value_or(QString())) +
                    segment.OutputBaseName,
                segment.OutputExtension,
                segment.OutputFileName(),
                segment.Range,
                segment.ExportProfileId,
                clip.MediaInformation,
                segment.Skipped
            };
            result.append(std::move(exportSegment));
        }
    }

    return result;
}

QVector<ExportSegment> ClipQueueModel::ExportableSegments(QStringList* errors) const
{
    const QVector<ExportSegment> allSegments = ExportSegments(errors);
    QVector<ExportSegment> result;

    for (const ExportSegment& segment : allSegments)
    {
        if (!segment.Skipped)
        {
            result.append(segment);
        }
    }

    return result;
}

QSet<QString> ClipQueueModel::CanonicalSourcePaths() const
{
    QSet<QString> paths;

    for (const Clip& clip : Clips_)
    {
        paths.insert(QDir::cleanPath(clip.SourcePath).toCaseFolded());
    }

    return paths;
}

const std::vector<Clip>& ClipQueueModel::Clips() const noexcept
{
    return Clips_;
}

ClipQueueModel::RowRef ClipQueueModel::GetRowRef(const int row)
{
    if (row < 0)
    {
        return {};
    }

    int currentRow = 0;

    for (Clip& clip : Clips_)
    {
        for (Segment& segment : clip.Segments)
        {
            if (currentRow == row)
            {
                return {&clip, &segment};
            }

            ++currentRow;
        }
    }

    return {};
}

ClipQueueModel::ConstRowRef ClipQueueModel::GetRowRef(const int row) const
{
    if (row < 0)
    {
        return {};
    }

    int currentRow = 0;

    for (const Clip& clip : Clips_)
    {
        for (const Segment& segment : clip.Segments)
        {
            if (currentRow == row)
            {
                return {&clip, &segment};
            }

            ++currentRow;
        }
    }

    return {};
}

int ClipQueueModel::FirstRowForClipIndex(const int clipIndex) const
{
    if (clipIndex < 0 || clipIndex > static_cast<int>(Clips_.size())) return -1;
    int row = 0;
    for (int index = 0; index < clipIndex; ++index)
        row += Clips_.at(static_cast<std::size_t>(index)).Segments.size();
    return row;
}

QString ClipQueueModel::StatusText(const Clip& clip, const Segment& segment)
{
    if (clip.MediaInformation.ProbeStatus == EProbeStatus::Probing) return QStringLiteral("Probing…");
    if (clip.MediaInformation.ProbeStatus == EProbeStatus::Failed) return QStringLiteral("Probe failed — see details");
    if (clip.MediaInformation.ProbeStatus == EProbeStatus::NotProbed) return QStringLiteral("Awaiting probe");
    const QString state = ExportStateText(segment.ExportState);

    if (!segment.ExportProgress.has_value())
    {
        return state;
    }

    const int percentage = qRound(std::clamp(*segment.ExportProgress, 0.0, 1.0) * 100.0);

    return QStringLiteral("%1 (%2%)").arg(state).arg(percentage);
}

QString ClipQueueModel::TimeText(const std::chrono::milliseconds value)
{
    return Utility::GetTimeStringFromMilliseconds(value.count());
}

bool ClipQueueModel::IsRuntimeEditable(const EExportState state)
{
    return state == EExportState::Pending || state == EExportState::Failed || state == EExportState::Cancelled ||
           state == EExportState::Skipped || state == EExportState::Succeeded;
}
} // namespace ClipCutter
