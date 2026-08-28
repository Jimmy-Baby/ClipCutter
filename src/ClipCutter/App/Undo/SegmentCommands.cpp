#include "App/Undo/SegmentCommands.h"

#include <algorithm>

namespace ClipCutter
{
namespace
{
constexpr int KMoveInMergeId = 0x434301;
constexpr int KMoveOutMergeId = 0x434302;
constexpr int KRenameMergeId = 0x434303;

TimeRange MarkerRange(const ClipQueueModel* model, const QUuid& segmentId,
                      const std::optional<std::chrono::milliseconds> start,
                      const std::optional<std::chrono::milliseconds> end)
{
    const Segment* segment = model == nullptr ? nullptr : model->FindSegment(segmentId);
    if (segment == nullptr) return {};
    const Clip* clip = model->FindClip(model->ClipIdForSegment(segmentId));
    const auto duration = clip == nullptr ? std::optional<std::chrono::milliseconds>{}
                                          : clip->MediaInformation.Duration;
    const auto result = TimeRange::Create(start.value_or(segment->Range.Start()),
                                          end.value_or(segment->Range.End()), duration);
    return result.value_or(segment->Range);
}
} // namespace

RangeCommand::RangeCommand(ClipQueueModel* model, QUuid segmentId, TimeRange before, TimeRange after,
                           QString text, const int mergeId, QUndoCommand* parent)
    : QUndoCommand(std::move(text), parent), Model_(model), SegmentId_(std::move(segmentId)),
      Before_(before), After_(after), MergeId_(mergeId)
{
}

void RangeCommand::undo() { if (Model_ != nullptr) Model_->UpdateTrimRange(SegmentId_, Before_); }
void RangeCommand::redo() { if (Model_ != nullptr) Model_->UpdateTrimRange(SegmentId_, After_); }
int RangeCommand::id() const { return MergeId_; }

bool RangeCommand::mergeWith(const QUndoCommand* command)
{
    const auto* other = dynamic_cast<const RangeCommand*>(command);
    if (other == nullptr || MergeId_ < 0 || other->MergeId_ != MergeId_ || other->SegmentId_ != SegmentId_)
        return false;
    After_ = other->After_;
    return true;
}

MoveInMarkerCommand::MoveInMarkerCommand(ClipQueueModel* model, const QUuid& segmentId,
                                         const std::chrono::milliseconds newStart, QUndoCommand* parent)
    : RangeCommand(model, segmentId, model->FindSegment(segmentId)->Range,
                   MarkerRange(model, segmentId, newStart, std::nullopt), QStringLiteral("Move in marker"),
                   KMoveInMergeId, parent)
{
}

MoveOutMarkerCommand::MoveOutMarkerCommand(ClipQueueModel* model, const QUuid& segmentId,
                                           const std::chrono::milliseconds newEnd, QUndoCommand* parent)
    : RangeCommand(model, segmentId, model->FindSegment(segmentId)->Range,
                   MarkerRange(model, segmentId, std::nullopt, newEnd), QStringLiteral("Move out marker"),
                   KMoveOutMergeId, parent)
{
}

ReplaceRangeCommand::ReplaceRangeCommand(ClipQueueModel* model, const QUuid& segmentId,
                                         const TimeRange& range, QUndoCommand* parent)
    : RangeCommand(model, segmentId, model->FindSegment(segmentId)->Range, range,
                   QStringLiteral("Replace segment range"), -1, parent)
{
}

AddSegmentCommand::AddSegmentCommand(ClipQueueModel* model, QUuid clipId, Segment segment,
                                     const int segmentIndex, QString text, QUndoCommand* parent)
    : QUndoCommand(std::move(text), parent), Model_(model), ClipId_(std::move(clipId)),
      Segment_(std::move(segment)), SegmentIndex_(segmentIndex)
{
}

void AddSegmentCommand::undo() { if (Model_ != nullptr) Model_->RemoveSegment(Segment_.Id); }
void AddSegmentCommand::redo() { if (Model_ != nullptr) Model_->InsertSegment(ClipId_, Segment_, SegmentIndex_); }
QUuid AddSegmentCommand::SegmentId() const noexcept { return Segment_.Id; }

DuplicateSegmentCommand::DuplicateSegmentCommand(ClipQueueModel* model, QUuid clipId, Segment segment,
                                                 const int segmentIndex, QUndoCommand* parent)
    : AddSegmentCommand(model, std::move(clipId), std::move(segment), segmentIndex,
                        QStringLiteral("Duplicate segment"), parent)
{
}

DeleteSegmentCommand::DeleteSegmentCommand(ClipQueueModel* model, const QUuid& segmentId, QUndoCommand* parent)
    : QUndoCommand(QStringLiteral("Delete segment"), parent), Model_(model)
{
    if (model == nullptr) return;
    const QUuid clipId = model->ClipIdForSegment(segmentId);
    const Clip* clip = model->FindClip(clipId);
    const Segment* segment = model->FindSegment(segmentId);
    if (clip == nullptr || segment == nullptr) return;
    ClipSnapshot_ = *clip;
    SegmentSnapshot_ = *segment;
    ClipIndex_ = model->ClipIndex(clipId);
    SegmentIndex_ = model->SegmentIndex(segmentId);
    RemovesClip_ = clip->Segments.size() == 1;
}

void DeleteSegmentCommand::undo()
{
    if (Model_ == nullptr || SegmentIndex_ < 0) return;
    if (RemovesClip_) Model_->InsertClip(ClipIndex_, ClipSnapshot_);
    else Model_->InsertSegment(ClipSnapshot_.Id, SegmentSnapshot_, SegmentIndex_);
}

void DeleteSegmentCommand::redo()
{
    if (Model_ == nullptr || SegmentSnapshot_.Id.isNull()) return;
    if (const Clip* clip = Model_->FindClip(ClipSnapshot_.Id); clip != nullptr && clip->Segments.size() == 1)
    {
        RemovesClip_ = true;
        ClipSnapshot_ = *clip;
        ClipIndex_ = Model_->ClipIndex(clip->Id);
        SegmentIndex_ = 0;
        SegmentSnapshot_ = clip->Segments.constFirst();
    }
    Model_->RemoveSegment(SegmentSnapshot_.Id);
}

ReorderSegmentCommand::ReorderSegmentCommand(ClipQueueModel* model, const QUuid& segmentId,
                                             const int destinationIndex, QUndoCommand* parent)
    : QUndoCommand(QStringLiteral("Reorder segment"), parent), Model_(model), SegmentId_(segmentId),
      SourceIndex_(model == nullptr ? -1 : model->SegmentIndex(segmentId)), DestinationIndex_(destinationIndex)
{
}

void ReorderSegmentCommand::undo() { if (Model_ != nullptr) Model_->MoveSegment(SegmentId_, SourceIndex_); }
void ReorderSegmentCommand::redo() { if (Model_ != nullptr) Model_->MoveSegment(SegmentId_, DestinationIndex_); }

RenameSegmentCommand::RenameSegmentCommand(ClipQueueModel* model, const QUuid& segmentId, QString name,
                                           QUndoCommand* parent)
    : QUndoCommand(QStringLiteral("Rename segment"), parent), Model_(model), SegmentId_(segmentId),
      Before_(model == nullptr || model->FindSegment(segmentId) == nullptr
                  ? QString() : model->FindSegment(segmentId)->OutputBaseName), After_(std::move(name)),
      BeforePattern_(model == nullptr || model->FindSegment(segmentId) == nullptr
                         ? std::nullopt : model->FindSegment(segmentId)->NamingTemplatePattern)
{
}

void RenameSegmentCommand::undo()
{
    if (Model_ != nullptr) Model_->UpdateNamingTemplateData(SegmentId_, Before_, BeforePattern_);
}
void RenameSegmentCommand::redo()
{
    if (Model_ != nullptr) Model_->UpdateNamingTemplateData(SegmentId_, After_, std::nullopt);
}
int RenameSegmentCommand::id() const { return KRenameMergeId; }
bool RenameSegmentCommand::mergeWith(const QUndoCommand* command)
{
    const auto* other = dynamic_cast<const RenameSegmentCommand*>(command);
    if (other == nullptr || other->SegmentId_ != SegmentId_) return false;
    After_ = other->After_;
    return true;
}

ChangePrefixCommand::ChangePrefixCommand(ClipQueueModel* model, const QUuid& segmentId,
                                         std::optional<QString> prefix, QUndoCommand* parent)
    : QUndoCommand(QStringLiteral("Change segment prefix"), parent), Model_(model), SegmentId_(segmentId),
      Before_(model == nullptr || model->FindSegment(segmentId) == nullptr
                  ? std::nullopt : model->FindSegment(segmentId)->Prefix), After_(std::move(prefix))
{
}
void ChangePrefixCommand::Apply(const std::optional<QString>& value)
{
    if (Model_ == nullptr) return;
    value.has_value() ? static_cast<void>(Model_->ApplyPrefix(SegmentId_, *value))
                      : static_cast<void>(Model_->ClearPrefix(SegmentId_));
}
void ChangePrefixCommand::undo() { Apply(Before_); }
void ChangePrefixCommand::redo() { Apply(After_); }

ChangeSkipStateCommand::ChangeSkipStateCommand(ClipQueueModel* model, const QUuid& segmentId,
                                               const bool skipped, QUndoCommand* parent)
    : QUndoCommand(skipped ? QStringLiteral("Skip segment") : QStringLiteral("Keep segment"), parent),
      Model_(model), SegmentId_(segmentId),
      Before_(model != nullptr && model->FindSegment(segmentId) != nullptr && model->FindSegment(segmentId)->Skipped),
      After_(skipped)
{
}
void ChangeSkipStateCommand::undo() { if (Model_ != nullptr) Model_->UpdateSkipState(SegmentId_, Before_); }
void ChangeSkipStateCommand::redo() { if (Model_ != nullptr) Model_->UpdateSkipState(SegmentId_, After_); }

ChangeOutputProfileCommand::ChangeOutputProfileCommand(ClipQueueModel* model, const QUuid& segmentId,
                                                       QString profileId, QUndoCommand* parent)
    : QUndoCommand(QStringLiteral("Change output profile"), parent), Model_(model), SegmentId_(segmentId),
      Before_(model == nullptr || model->FindSegment(segmentId) == nullptr
                  ? QString() : model->FindSegment(segmentId)->ExportProfileId), After_(std::move(profileId))
{
}
void ChangeOutputProfileCommand::undo() { if (Model_ != nullptr) Model_->ApplyExportProfile({SegmentId_}, Before_); }
void ChangeOutputProfileCommand::redo() { if (Model_ != nullptr) Model_->ApplyExportProfile({SegmentId_}, After_); }

ChangeNamingTemplateCommand::ChangeNamingTemplateCommand(ClipQueueModel* model, const QUuid& segmentId,
                                                         QString pattern, QDate date, QUndoCommand* parent)
    : QUndoCommand(QStringLiteral("Change naming template"), parent), Model_(model), SegmentId_(segmentId),
      BeforeName_(model == nullptr || model->FindSegment(segmentId) == nullptr
                      ? QString() : model->FindSegment(segmentId)->OutputBaseName),
      BeforePattern_(model == nullptr || model->FindSegment(segmentId) == nullptr
                         ? std::nullopt : model->FindSegment(segmentId)->NamingTemplatePattern),
      AfterPattern_(std::move(pattern)), Date_(std::move(date))
{
}
void ChangeNamingTemplateCommand::undo()
{
    if (Model_ == nullptr) return;
    Model_->UpdateNamingTemplateData(SegmentId_, BeforeName_, BeforePattern_);
}
void ChangeNamingTemplateCommand::redo()
{
    if (Model_ != nullptr) Model_->ApplyNamingTemplate({SegmentId_}, AfterPattern_, Date_);
}

BatchSkipStateCommand::BatchSkipStateCommand(ClipQueueModel* model, QSet<QUuid> segmentIds,
                                             const bool skipped, QUndoCommand* parent)
    : QUndoCommand(skipped ? QStringLiteral("Skip segments") : QStringLiteral("Keep segments"), parent),
      Model_(model), After_(skipped)
{
    if (model == nullptr) return;
    if (segmentIds.isEmpty())
        for (int row = 0; row < model->rowCount(); ++row) segmentIds.insert(model->SegmentIdAtRow(row));
    for (const QUuid& id : segmentIds)
        if (const Segment* segment = model->FindSegment(id)) Before_.insert(id, segment->Skipped);
}
void BatchSkipStateCommand::undo()
{
    if (Model_ == nullptr) return;
    for (auto it = Before_.cbegin(); it != Before_.cend(); ++it) Model_->UpdateSkipState(it.key(), it.value());
}
void BatchSkipStateCommand::redo()
{
    if (Model_ != nullptr) Model_->UpdateSkipStates(QSet<QUuid>(Before_.keyBegin(), Before_.keyEnd()), After_);
}
} // namespace ClipCutter
