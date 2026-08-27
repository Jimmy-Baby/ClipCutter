#ifndef CLIPCUTTER_APP_MODELS_CLIPQUEUEMODEL_H
#define CLIPCUTTER_APP_MODELS_CLIPQUEUEMODEL_H

#include "Core/Clip/Clip.h"

#include <QAbstractTableModel>
#include <QSet>
#include <QDate>

#include <vector>

namespace ClipCutter
{
struct ExportSegment
{
    QUuid ClipId;
    QUuid SegmentId;
    QString SourcePath;
    QString OutputBaseName;
    QString OutputExtension;
    QString OutputFileName;
    TimeRange Range;
    QString ExportProfileId;
    MediaInfo SourceMediaInfo;
    bool Skipped = false;
};

class ClipQueueModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum EColumn
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

    enum ERole
    {
        ClipIdRole = Qt::UserRole + 1,
        SegmentIdRole,
        SourcePathRole,
        StartMillisecondsRole,
        EndMillisecondsRole,
        DurationMillisecondsRole,
        SkippedRole,
        OutputFileNameRole,
        ExportProfileRole,
        ExportStateRole,
        ExportProgressRole,
        ExportLogRole,
        PrefixRole,
        NamingTemplateRole,
        MediaProbeRole
    };

    explicit ClipQueueModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    void AddClip(Clip clip);
    void AddClips(QVector<Clip> clips);
    void ReplaceClips(std::vector<Clip> clips);
    void ClearClips();
    bool RemoveEntries(const QSet<QUuid>& segmentIds);

    Clip* FindClip(const QUuid& clipId);
    const Clip* FindClip(const QUuid& clipId) const;
    Segment* FindSegment(const QUuid& segmentId);
    const Segment* FindSegment(const QUuid& segmentId) const;
    int RowForClip(const QUuid& clipId) const;
    int RowForSegment(const QUuid& segmentId) const;
    QUuid ClipIdAtRow(int row) const;
    QUuid SegmentIdAtRow(int row) const;

    bool UpdateSkipState(const QUuid& segmentId, bool skipped);
    void UpdateAllSkipStates(bool skipped);
    bool UpdateSkipStates(const QSet<QUuid>& segmentIds, bool skipped);
    bool InvertSkipStates(const QSet<QUuid>& segmentIds = {});
    bool UpdateOutputName(const QUuid& segmentId, const QString& outputFileName);
    bool UpdateOutputBaseName(const QUuid& segmentId, const QString& outputBaseName);
    bool UpdateTrimRange(const QUuid& segmentId, const TimeRange& range, QString* error = nullptr);
    bool UpdateMediaDuration(const QUuid& clipId, std::chrono::milliseconds duration, QString* error = nullptr);
    bool UpdateMediaInfo(const QUuid& clipId, const QString& expectedSourcePath, const MediaInfo& info);
    bool RelinkClipSource(const QUuid& clipId, const QString& replacementPath, QString* error = nullptr);
    bool ApplyPrefix(const QUuid& segmentId, const QString& prefix);
    bool ClearPrefix(const QUuid& segmentId);
    bool ApplyPrefixTo(const QSet<QUuid>& segmentIds, const QString& prefix);
    bool ClearPrefixes(const QSet<QUuid>& segmentIds = {});
    bool ApplyNamingTemplate(const QSet<QUuid>& segmentIds, const QString& pattern,
                             const QDate& date = QDate::currentDate(), QString* error = nullptr);
    bool ApplyExportProfile(const QSet<QUuid>& segmentIds, const QString& profileId);
    bool ResetTrimRanges(const QSet<QUuid>& segmentIds = {});
    void ClearPrefixFromAll(const QString& prefix);
    void UpdateAllExportProfiles(const QString& profileId);
    bool UpdateExportState(const QUuid& segmentId, EExportState state);
    bool UpdateExportProgress(const QUuid& segmentId, std::optional<double> progress);
    bool AppendExportLog(const QUuid& segmentId, const QString& text);
    bool ResetExportJobRuntime(const QUuid& segmentId);
    void SetExportRuntimeLocked(bool locked);
    void ResetExportRuntime();

    QVector<ExportSegment> ExportSegments(QStringList* errors = nullptr) const;
    QVector<ExportSegment> ExportableSegments(QStringList* errors = nullptr) const;
    QSet<QString> CanonicalSourcePaths() const;
    const std::vector<Clip>& Clips() const noexcept;

private:
    static constexpr qsizetype KMaximumExportLogCharacters = 512 * 1024;

    struct RowRef
    {
        Clip* ClipValue = nullptr;
        Segment* SegmentValue = nullptr;
    };

    struct ConstRowRef
    {
        const Clip* ClipValue = nullptr;
        const Segment* SegmentValue = nullptr;
    };

    RowRef GetRowRef(int row);
    ConstRowRef GetRowRef(int row) const;
    static QString StatusText(const Clip& clip, const Segment& segment);
    static QString TimeText(std::chrono::milliseconds value);
    static bool IsRuntimeEditable(EExportState state);

    std::vector<Clip> Clips_;
};
} // namespace ClipCutter

#endif
