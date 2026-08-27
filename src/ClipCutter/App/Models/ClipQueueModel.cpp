#include "App/Models/ClipQueueModel.h"

#include "Utility.h"
#include "Core/Export/OutputProfile.h"

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
    const QString extension =
        fileInfo.suffix().isEmpty() ? segment->OutputExtension : QStringLiteral(".") + fileInfo.suffix();

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
        emit dataChanged(index(firstRow, StartColumn), index(firstRow + clip->Segments.size() - 1, StatusColumn));
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
        emit dataChanged(index(firstRow, StartColumn), index(firstRow + clip->Segments.size() - 1, StatusColumn));
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
                segment.Prefix.value_or(QString()) + segment.OutputBaseName,
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
