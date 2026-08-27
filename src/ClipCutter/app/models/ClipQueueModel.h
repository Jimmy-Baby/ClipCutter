#ifndef CLIPCUTTER_APP_MODELS_CLIPQUEUEMODEL_H
#define CLIPCUTTER_APP_MODELS_CLIPQUEUEMODEL_H

#include "core/clip/Clip.h"

#include <QAbstractTableModel>
#include <QSet>

#include <vector>

namespace clipcutter
{
struct ExportSegment
{
    QUuid clipId;
    QUuid segmentId;
    QString sourcePath;
    QString outputFileName;
    TimeRange range;
    QString exportProfileId;
};

class ClipQueueModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        SkipColumn,
        SourceNameColumn,
        OutputNameColumn,
        StartColumn,
        EndColumn,
        DurationColumn,
        StatusColumn,
        ColumnCount
    };

    enum Role
    {
        ClipIdRole = Qt::UserRole + 1,
        SegmentIdRole,
        SourcePathRole,
        StartMillisecondsRole,
        EndMillisecondsRole,
        DurationMillisecondsRole,
        SkippedRole,
        OutputFileNameRole,
        ExportProfileRole
    };

    explicit ClipQueueModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    void addClip(Clip clip);
    void addClips(QVector<Clip> clips);
    void clearClips();
    bool removeEntries(const QSet<QUuid>& segmentIds);

    Clip* findClip(const QUuid& clipId);
    const Clip* findClip(const QUuid& clipId) const;
    Segment* findSegment(const QUuid& segmentId);
    const Segment* findSegment(const QUuid& segmentId) const;
    int rowForClip(const QUuid& clipId) const;
    int rowForSegment(const QUuid& segmentId) const;
    QUuid clipIdAtRow(int row) const;
    QUuid segmentIdAtRow(int row) const;

    bool updateSkipState(const QUuid& segmentId, bool skipped);
    void updateAllSkipStates(bool skipped);
    bool updateOutputName(const QUuid& segmentId, const QString& outputFileName);
    bool updateOutputBaseName(const QUuid& segmentId, const QString& outputBaseName);
    bool updateTrimRange(const QUuid& segmentId, const TimeRange& range, QString* error = nullptr);
    bool updateMediaDuration(const QUuid& clipId, std::chrono::milliseconds duration, QString* error = nullptr);
    bool applyPrefix(const QUuid& segmentId, const QString& prefix);
    bool clearPrefix(const QUuid& segmentId);
    void clearPrefixFromAll(const QString& prefix);
    void updateAllExportProfiles(const QString& profileId);

    QVector<ExportSegment> exportableSegments(QStringList* errors = nullptr) const;
    QSet<QString> canonicalSourcePaths() const;
    const std::vector<Clip>& clips() const noexcept;

private:
    struct RowRef
    {
        Clip* clip;
        Segment* segment;
    };
    struct ConstRowRef
    {
        const Clip* clip;
        const Segment* segment;
    };

    RowRef rowRef(int row);
    ConstRowRef rowRef(int row) const;
    int segmentCount(const Clip& clip) const;
    static QString statusText(ExportStatus status);
    static QString timeText(std::chrono::milliseconds value);

    std::vector<Clip> m_clips;
};
}

#endif
