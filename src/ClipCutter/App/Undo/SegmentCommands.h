#ifndef CLIPCUTTER_APP_UNDO_SEGMENTCOMMANDS_H
#define CLIPCUTTER_APP_UNDO_SEGMENTCOMMANDS_H

#include "App/Models/ClipQueueModel.h"

#include <QDate>
#include <QUndoCommand>

#include <optional>

namespace ClipCutter
{
class RangeCommand : public QUndoCommand
{
public:
    RangeCommand(ClipQueueModel* model, QUuid segmentId, TimeRange before, TimeRange after, QString text,
                 int mergeId = -1, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand* command) override;

protected:
    ClipQueueModel* Model_;
    QUuid SegmentId_;
    TimeRange Before_;
    TimeRange After_;
    int MergeId_;
};

class MoveInMarkerCommand final : public RangeCommand
{
public:
    MoveInMarkerCommand(ClipQueueModel* model, const QUuid& segmentId, std::chrono::milliseconds newStart,
                        QUndoCommand* parent = nullptr);
};

class MoveOutMarkerCommand final : public RangeCommand
{
public:
    MoveOutMarkerCommand(ClipQueueModel* model, const QUuid& segmentId, std::chrono::milliseconds newEnd,
                         QUndoCommand* parent = nullptr);
};

class ReplaceRangeCommand final : public RangeCommand
{
public:
    ReplaceRangeCommand(ClipQueueModel* model, const QUuid& segmentId, const TimeRange& range,
                        QUndoCommand* parent = nullptr);
};

class AddSegmentCommand : public QUndoCommand
{
public:
    AddSegmentCommand(ClipQueueModel* model, QUuid clipId, Segment segment, int segmentIndex,
                      QString text = QStringLiteral("Add segment"), QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
    QUuid SegmentId() const noexcept;

protected:
    ClipQueueModel* Model_;
    QUuid ClipId_;
    Segment Segment_;
    int SegmentIndex_;
};

class DuplicateSegmentCommand final : public AddSegmentCommand
{
public:
    DuplicateSegmentCommand(ClipQueueModel* model, QUuid clipId, Segment segment, int segmentIndex,
                            QUndoCommand* parent = nullptr);
};

class DeleteSegmentCommand final : public QUndoCommand
{
public:
    DeleteSegmentCommand(ClipQueueModel* model, const QUuid& segmentId, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ClipQueueModel* Model_;
    Clip ClipSnapshot_;
    Segment SegmentSnapshot_;
    int ClipIndex_ = -1;
    int SegmentIndex_ = -1;
    bool RemovesClip_ = false;
};

class ReorderSegmentCommand final : public QUndoCommand
{
public:
    ReorderSegmentCommand(ClipQueueModel* model, const QUuid& segmentId, int destinationIndex,
                          QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ClipQueueModel* Model_;
    QUuid SegmentId_;
    int SourceIndex_;
    int DestinationIndex_;
};

class RenameSegmentCommand final : public QUndoCommand
{
public:
    RenameSegmentCommand(ClipQueueModel* model, const QUuid& segmentId, QString name,
                         QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand* command) override;

private:
    ClipQueueModel* Model_;
    QUuid SegmentId_;
    QString Before_;
    QString After_;
    std::optional<QString> BeforePattern_;
};

class ChangePrefixCommand final : public QUndoCommand
{
public:
    ChangePrefixCommand(ClipQueueModel* model, const QUuid& segmentId, std::optional<QString> prefix,
                        QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    void Apply(const std::optional<QString>& value);
    ClipQueueModel* Model_;
    QUuid SegmentId_;
    std::optional<QString> Before_;
    std::optional<QString> After_;
};

class ChangeSkipStateCommand final : public QUndoCommand
{
public:
    ChangeSkipStateCommand(ClipQueueModel* model, const QUuid& segmentId, bool skipped,
                           QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ClipQueueModel* Model_;
    QUuid SegmentId_;
    bool Before_;
    bool After_;
};

class ChangeOutputProfileCommand final : public QUndoCommand
{
public:
    ChangeOutputProfileCommand(ClipQueueModel* model, const QUuid& segmentId, QString profileId,
                               QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ClipQueueModel* Model_;
    QUuid SegmentId_;
    QString Before_;
    QString After_;
};

class ChangeNamingTemplateCommand final : public QUndoCommand
{
public:
    ChangeNamingTemplateCommand(ClipQueueModel* model, const QUuid& segmentId, QString pattern,
                                QDate date = QDate::currentDate(), QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ClipQueueModel* Model_;
    QUuid SegmentId_;
    QString BeforeName_;
    std::optional<QString> BeforePattern_;
    QString AfterPattern_;
    QDate Date_;
};

class BatchSkipStateCommand final : public QUndoCommand
{
public:
    BatchSkipStateCommand(ClipQueueModel* model, QSet<QUuid> segmentIds, bool skipped,
                          QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ClipQueueModel* Model_;
    QHash<QUuid, bool> Before_;
    bool After_;
};
} // namespace ClipCutter

#endif
