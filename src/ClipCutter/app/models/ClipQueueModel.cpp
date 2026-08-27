#include "app/models/ClipQueueModel.h"

#include "Utility.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace clipcutter
{
ClipQueueModel::ClipQueueModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int ClipQueueModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    int count = 0;
    for (const Clip& clip : m_clips)
    {
        count += segmentCount(clip);
    }
    return count;
}

int ClipQueueModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ClipQueueModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.column() < 0 || index.column() >= ColumnCount)
    {
        return {};
    }
    const ConstRowRef ref = rowRef(index.row());
    if (ref.clip == nullptr || ref.segment == nullptr)
    {
        return {};
    }

    switch (role)
    {
    case ClipIdRole:
        return ref.clip->id;
    case SegmentIdRole:
        return ref.segment->id;
    case SourcePathRole:
        return ref.clip->sourcePath;
    case StartMillisecondsRole:
        return QVariant::fromValue<qint64>(ref.segment->range.start().count());
    case EndMillisecondsRole:
        return QVariant::fromValue<qint64>(ref.segment->range.end().count());
    case DurationMillisecondsRole:
        return QVariant::fromValue<qint64>(ref.segment->range.duration().count());
    case SkippedRole:
        return ref.segment->skipped;
    case OutputFileNameRole:
        return ref.segment->outputFileName();
    case ExportProfileRole:
        return ref.segment->exportProfileId;
    case Qt::CheckStateRole:
        return index.column() == SkipColumn
            ? QVariant::fromValue(ref.segment->skipped ? Qt::Checked : Qt::Unchecked)
            : QVariant();
    case Qt::EditRole:
        if (index.column() == OutputNameColumn)
        {
            return ref.segment->outputBaseName;
        }
        break;
    case Qt::DisplayRole:
        switch (index.column())
        {
        case SkipColumn: return {};
        case SourceNameColumn: return ref.clip->originalFileName;
        case OutputNameColumn: return ref.segment->outputFileName();
        case StartColumn: return timeText(ref.segment->range.start());
        case EndColumn: return timeText(ref.segment->range.end());
        case DurationColumn: return timeText(ref.segment->range.duration());
        case StatusColumn: return statusText(ref.segment->exportStatus);
        default: return {};
        }
    default:
        break;
    }
    return {};
}

QVariant ClipQueueModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section)
    {
    case SkipColumn: return QStringLiteral("Skip?");
    case SourceNameColumn: return QStringLiteral("Source");
    case OutputNameColumn: return QStringLiteral("Output");
    case StartColumn: return QStringLiteral("Start");
    case EndColumn: return QStringLiteral("End");
    case DurationColumn: return QStringLiteral("Selected");
    case StatusColumn: return QStringLiteral("Status");
    default: return {};
    }
}

Qt::ItemFlags ClipQueueModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == SkipColumn)
    {
        result |= Qt::ItemIsUserCheckable;
    }
    if (index.column() == OutputNameColumn)
    {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

bool ClipQueueModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
    {
        return false;
    }
    const QUuid segmentId = segmentIdAtRow(index.row());
    if (segmentId.isNull())
    {
        return false;
    }
    if (index.column() == SkipColumn && role == Qt::CheckStateRole)
    {
        return updateSkipState(segmentId, value.toInt() == Qt::Checked);
    }
    if (index.column() == OutputNameColumn && role == Qt::EditRole)
    {
        return updateOutputName(segmentId, value.toString());
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
    return names;
}

void ClipQueueModel::addClip(Clip clip)
{
    QVector<Clip> clips;
    clips.append(std::move(clip));
    addClips(std::move(clips));
}

void ClipQueueModel::addClips(QVector<Clip> clips)
{
    if (clips.isEmpty())
    {
        return;
    }
    int addedRows = 0;
    for (const Clip& clip : clips)
    {
        addedRows += segmentCount(clip);
    }
    const int first = rowCount();
    if (addedRows > 0)
    {
        beginInsertRows({}, first, first + addedRows - 1);
    }
    m_clips.reserve(m_clips.size() + static_cast<std::size_t>(clips.size()));
    for (Clip& clip : clips)
    {
        m_clips.push_back(std::move(clip));
    }
    if (addedRows > 0)
    {
        endInsertRows();
    }
}

void ClipQueueModel::clearClips()
{
    if (m_clips.empty())
    {
        return;
    }
    beginResetModel();
    m_clips.clear();
    endResetModel();
}

bool ClipQueueModel::removeEntries(const QSet<QUuid>& segmentIds)
{
    bool removed = false;
    for (int row = rowCount() - 1; row >= 0; --row)
    {
        const QUuid id = segmentIdAtRow(row);
        if (!segmentIds.contains(id))
        {
            continue;
        }
        beginRemoveRows({}, row, row);
        for (auto clipIt = m_clips.begin(); clipIt != m_clips.end(); ++clipIt)
        {
            auto& segments = clipIt->segments;
            const auto segmentIt = std::find_if(segments.begin(), segments.end(), [&id](const Segment& segment)
            {
                return segment.id == id;
            });
            if (segmentIt != segments.end())
            {
                segments.erase(segmentIt);
                if (segments.isEmpty())
                {
                    m_clips.erase(clipIt);
                }
                break;
            }
        }
        endRemoveRows();
        removed = true;
    }
    return removed;
}

Clip* ClipQueueModel::findClip(const QUuid& clipId)
{
    const auto found = std::find_if(m_clips.begin(), m_clips.end(), [&clipId](const Clip& clip)
    {
        return clip.id == clipId;
    });
    return found == m_clips.end() ? nullptr : &*found;
}

const Clip* ClipQueueModel::findClip(const QUuid& clipId) const
{
    const auto found = std::find_if(m_clips.cbegin(), m_clips.cend(), [&clipId](const Clip& clip)
    {
        return clip.id == clipId;
    });
    return found == m_clips.cend() ? nullptr : &*found;
}

Segment* ClipQueueModel::findSegment(const QUuid& segmentId)
{
    for (Clip& clip : m_clips)
    {
        const auto found = std::find_if(clip.segments.begin(), clip.segments.end(), [&segmentId](const Segment& segment)
        {
            return segment.id == segmentId;
        });
        if (found != clip.segments.end())
        {
            return &*found;
        }
    }
    return nullptr;
}

const Segment* ClipQueueModel::findSegment(const QUuid& segmentId) const
{
    for (const Clip& clip : m_clips)
    {
        const auto found = std::find_if(clip.segments.cbegin(), clip.segments.cend(), [&segmentId](const Segment& segment)
        {
            return segment.id == segmentId;
        });
        if (found != clip.segments.cend())
        {
            return &*found;
        }
    }
    return nullptr;
}

int ClipQueueModel::rowForClip(const QUuid& clipId) const
{
    int row = 0;
    for (const Clip& clip : m_clips)
    {
        if (clip.id == clipId)
        {
            return clip.segments.isEmpty() ? -1 : row;
        }
        row += segmentCount(clip);
    }
    return -1;
}

int ClipQueueModel::rowForSegment(const QUuid& segmentId) const
{
    int row = 0;
    for (const Clip& clip : m_clips)
    {
        for (const Segment& segment : clip.segments)
        {
            if (segment.id == segmentId)
            {
                return row;
            }
            ++row;
        }
    }
    return -1;
}

QUuid ClipQueueModel::clipIdAtRow(int row) const
{
    const ConstRowRef ref = rowRef(row);
    return ref.clip == nullptr ? QUuid() : ref.clip->id;
}

QUuid ClipQueueModel::segmentIdAtRow(int row) const
{
    const ConstRowRef ref = rowRef(row);
    return ref.segment == nullptr ? QUuid() : ref.segment->id;
}

bool ClipQueueModel::updateSkipState(const QUuid& segmentId, bool skipped)
{
    Segment* segment = findSegment(segmentId);
    if (segment == nullptr || segment->skipped == skipped)
    {
        return segment != nullptr;
    }
    segment->skipped = skipped;
    const int row = rowForSegment(segmentId);
    emit dataChanged(index(row, SkipColumn), index(row, SkipColumn), { Qt::CheckStateRole, SkippedRole });
    return true;
}

void ClipQueueModel::updateAllSkipStates(bool skipped)
{
    bool changed = false;
    for (Clip& clip : m_clips)
    {
        for (Segment& segment : clip.segments)
        {
            changed = changed || segment.skipped != skipped;
            segment.skipped = skipped;
        }
    }
    if (changed && rowCount() > 0)
    {
        emit dataChanged(index(0, SkipColumn), index(rowCount() - 1, SkipColumn), { Qt::CheckStateRole, SkippedRole });
    }
}

bool ClipQueueModel::updateOutputBaseName(const QUuid& segmentId, const QString& outputBaseName)
{
    Segment* segment = findSegment(segmentId);
    const QString normalized = outputBaseName.trimmed();
    if (segment == nullptr || normalized.isEmpty())
    {
        return false;
    }
    if (segment->outputBaseName == normalized)
    {
        return true;
    }
    segment->outputBaseName = normalized;
    const int row = rowForSegment(segmentId);
    emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn),
                     { Qt::DisplayRole, Qt::EditRole, OutputFileNameRole });
    return true;
}

bool ClipQueueModel::updateOutputName(const QUuid& segmentId, const QString& outputFileName)
{
    Segment* segment = findSegment(segmentId);
    const QString normalized = outputFileName.trimmed();
    const QFileInfo fileInfo(normalized);
    if (segment == nullptr || normalized.isEmpty())
    {
        return false;
    }
    const QString baseName = fileInfo.suffix().isEmpty() ? normalized : fileInfo.completeBaseName();
    const QString extension = fileInfo.suffix().isEmpty()
        ? segment->outputExtension
        : QStringLiteral(".") + fileInfo.suffix();
    if (baseName.isEmpty())
    {
        return false;
    }
    if (segment->outputBaseName == baseName && segment->outputExtension == extension)
    {
        return true;
    }
    segment->outputBaseName = baseName;
    segment->outputExtension = extension;
    const int row = rowForSegment(segmentId);
    emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn),
                     { Qt::DisplayRole, Qt::EditRole, OutputFileNameRole });
    return true;
}

bool ClipQueueModel::updateTrimRange(const QUuid& segmentId, const TimeRange& range, QString* error)
{
    for (Clip& clip : m_clips)
    {
        for (Segment& segment : clip.segments)
        {
            if (segment.id != segmentId)
            {
                continue;
            }
            if (!range.isValid(clip.mediaInfo.duration, false, error))
            {
                return false;
            }
            if (segment.range == range)
            {
                return true;
            }
            segment.range = range;
            const int row = rowForSegment(segmentId);
            emit dataChanged(index(row, StartColumn), index(row, DurationColumn),
                             { Qt::DisplayRole, StartMillisecondsRole, EndMillisecondsRole, DurationMillisecondsRole });
            return true;
        }
    }
    if (error != nullptr)
    {
        *error = QStringLiteral("Segment was not found.");
    }
    return false;
}

bool ClipQueueModel::updateMediaDuration(const QUuid& clipId, std::chrono::milliseconds duration, QString* error)
{
    if (duration.count() < 0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Media duration cannot be negative.");
        }
        return false;
    }
    Clip* clip = findClip(clipId);
    if (clip == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Clip was not found.");
        }
        return false;
    }
    clip->mediaInfo.duration = duration;
    clip->mediaInfo.probeStatus = ProbeStatus::Ready;
    for (Segment& segment : clip->segments)
    {
        segment.range = segment.range.duration().count() == 0
            ? *TimeRange::create(std::chrono::milliseconds{0}, duration)
            : TimeRange::clamped(segment.range.start(), segment.range.end(), duration);
    }
    const int first = rowForClip(clipId);
    if (first >= 0)
    {
        emit dataChanged(index(first, StartColumn), index(first + clip->segments.size() - 1, StatusColumn));
    }
    return true;
}

bool ClipQueueModel::applyPrefix(const QUuid& segmentId, const QString& prefix)
{
    Segment* segment = findSegment(segmentId);
    if (segment == nullptr)
    {
        return false;
    }
    const QString normalized = prefix.trimmed();
    segment->prefix = normalized.isEmpty() ? std::nullopt : std::optional<QString>(normalized);
    const int row = rowForSegment(segmentId);
    emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn), { Qt::DisplayRole, OutputFileNameRole });
    return true;
}

bool ClipQueueModel::clearPrefix(const QUuid& segmentId)
{
    Segment* segment = findSegment(segmentId);
    if (segment == nullptr)
    {
        return false;
    }
    if (!segment->prefix.has_value())
    {
        return true;
    }
    segment->prefix.reset();
    const int row = rowForSegment(segmentId);
    emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn), { Qt::DisplayRole, OutputFileNameRole });
    return true;
}

void ClipQueueModel::clearPrefixFromAll(const QString& prefix)
{
    for (int row = 0; row < rowCount(); ++row)
    {
        RowRef ref = rowRef(row);
        if (ref.segment->prefix.has_value()
            && ref.segment->prefix->compare(prefix, Qt::CaseInsensitive) == 0)
        {
            ref.segment->prefix.reset();
            emit dataChanged(index(row, OutputNameColumn), index(row, OutputNameColumn),
                             { Qt::DisplayRole, OutputFileNameRole });
        }
    }
}

void ClipQueueModel::updateAllExportProfiles(const QString& profileId)
{
    bool changed = false;
    for (Clip& clip : m_clips)
    {
        for (Segment& segment : clip.segments)
        {
            changed = changed || segment.exportProfileId != profileId;
            segment.exportProfileId = profileId;
        }
    }
    if (changed && rowCount() > 0)
    {
        emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1), { ExportProfileRole });
    }
}

QVector<ExportSegment> ClipQueueModel::exportableSegments(QStringList* errors) const
{
    QVector<ExportSegment> result;
    if (errors != nullptr)
    {
        errors->clear();
    }
    for (const Clip& clip : m_clips)
    {
        for (const Segment& segment : clip.segments)
        {
            if (segment.skipped)
            {
                continue;
            }
            QString error;
            if (!segment.range.isValid(clip.mediaInfo.duration, true, &error))
            {
                if (errors != nullptr)
                {
                    errors->append(QStringLiteral("%1: %2").arg(clip.originalFileName, error));
                }
                continue;
            }
            result.append({ clip.id, segment.id, clip.sourcePath, segment.outputFileName(),
                            segment.range, segment.exportProfileId });
        }
    }
    return result;
}

QSet<QString> ClipQueueModel::canonicalSourcePaths() const
{
    QSet<QString> paths;
    for (const Clip& clip : m_clips)
    {
        paths.insert(QDir::cleanPath(clip.sourcePath).toCaseFolded());
    }
    return paths;
}

const std::vector<Clip>& ClipQueueModel::clips() const noexcept
{
    return m_clips;
}

ClipQueueModel::RowRef ClipQueueModel::rowRef(int row)
{
    if (row < 0)
    {
        return { nullptr, nullptr };
    }
    int current = 0;
    for (Clip& clip : m_clips)
    {
        for (Segment& segment : clip.segments)
        {
            if (current == row)
            {
                return { &clip, &segment };
            }
            ++current;
        }
    }
    return { nullptr, nullptr };
}

ClipQueueModel::ConstRowRef ClipQueueModel::rowRef(int row) const
{
    if (row < 0)
    {
        return { nullptr, nullptr };
    }
    int current = 0;
    for (const Clip& clip : m_clips)
    {
        for (const Segment& segment : clip.segments)
        {
            if (current == row)
            {
                return { &clip, &segment };
            }
            ++current;
        }
    }
    return { nullptr, nullptr };
}

int ClipQueueModel::segmentCount(const Clip& clip) const
{
    return clip.segments.size();
}

QString ClipQueueModel::statusText(ExportStatus status)
{
    switch (status)
    {
    case ExportStatus::Pending: return QStringLiteral("Pending");
    case ExportStatus::Processing: return QStringLiteral("Processing");
    case ExportStatus::Complete: return QStringLiteral("Complete");
    case ExportStatus::Failed: return QStringLiteral("Failed");
    }
    return {};
}

QString ClipQueueModel::timeText(std::chrono::milliseconds value)
{
    return Utility::GetTimeStringFromMilli(value.count());
}
}
