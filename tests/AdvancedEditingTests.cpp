#include "App/Models/ClipQueueModel.h"
#include "App/Timeline/ThumbnailProvider.h"
#include "App/Timeline/TimelineWidget.h"
#include "App/Undo/SegmentCommands.h"
#include "Core/Naming/NamingTemplate.h"
#include "Core/Session/SessionRepository.h"
#include "Core/Timeline/TimelineMath.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

using namespace std::chrono_literals;

namespace
{
ClipCutter::Clip MakeClip(const QString& source = QStringLiteral("C:/media/source.mp4"))
{
    ClipCutter::Clip clip;
    clip.SourcePath = source;
    clip.OriginalFileName = QFileInfo(source).fileName();
    clip.MediaInformation.Duration = 10s;
    clip.MediaInformation.HasVideo = true;
    clip.MediaInformation.VideoStreamIndices.append(0);
    clip.MediaInformation.ProbeStatus = ClipCutter::EProbeStatus::Ready;
    clip.MediaInformation.FrameRateNumerator = 30'000;
    clip.MediaInformation.FrameRateDenominator = 1'001;
    clip.MediaInformation.VariableFrameRate = false;
    ClipCutter::Segment segment;
    segment.Range = *ClipCutter::TimeRange::Create(1s, 9s);
    segment.OutputBaseName = QStringLiteral("source_01");
    segment.OutputExtension = QStringLiteral(".mp4");
    segment.ExportProfileId = QStringLiteral("fast-copy");
    clip.Segments.append(segment);
    return clip;
}
} // namespace

class AdvancedEditingTests : public QObject
{
    Q_OBJECT

private slots:
    void MultiSegmentLifecycleAndStableIds();
    void SegmentTokenPaddingAndUniqueDefaults();
    void VersionOneSingleSegmentMigration();
    void UndoCommandsAndMerging();
    void TimelineMappingAndZoomBounds();
    void TimelineWidgetClickSeekAndMarkerRules();
    void LoopBoundaryBehaviour();
    void RationalFrameStepping();
    void ThumbnailKeysAndCancellation();
    void SegmentExportSnapshots();
};

void AdvancedEditingTests::MultiSegmentLifecycleAndStableIds()
{
    ClipCutter::ClipQueueModel model;
    ClipCutter::Clip clip = MakeClip();
    const QUuid clipId = clip.Id;
    const QUuid firstId = clip.Segments.constFirst().Id;
    model.AddClip(clip);
    QString error;
    const auto secondId = model.CreateSegment(clipId, *ClipCutter::TimeRange::Create(2s, 4s),
                                               QStringLiteral("{original}_{segment:02}"), QDate(2026, 8, 28), &error);
    QVERIFY2(secondId.has_value(), qPrintable(error));
    const auto thirdId = model.DuplicateSegment(*secondId, QStringLiteral("{original}_{segment:02}"));
    QVERIFY(thirdId.has_value());
    QCOMPARE(model.FindClip(clipId)->Segments.size(), 3);
    QCOMPARE(model.FindClip(clipId)->Segments.at(0).Id, firstId);
    QCOMPARE(model.FindClip(clipId)->Segments.at(1).Id, *secondId);
    QCOMPARE(model.FindClip(clipId)->Segments.at(2).Id, *thirdId);
    QVERIFY(model.MoveSegment(*thirdId, 0));
    QCOMPARE(model.FindClip(clipId)->Segments.constFirst().Id, *thirdId);
    QCOMPARE(model.FindSegment(firstId)->Id, firstId);
    QVERIFY(model.RemoveSegment(*secondId));
    QVERIFY(model.FindSegment(*secondId) == nullptr);
    QCOMPARE(model.FindSegment(firstId)->Id, firstId);
}

void AdvancedEditingTests::SegmentTokenPaddingAndUniqueDefaults()
{
    ClipCutter::NamingTemplateContext context;
    context.Original = QStringLiteral("recording");
    context.Segment = 7;
    QCOMPARE(ClipCutter::NamingTemplate::Render(QStringLiteral("{original}_{segment:02}"), context).Value,
             QStringLiteral("recording_07"));
    QVERIFY(ClipCutter::NamingTemplate::Validate(QStringLiteral("{segment:02}")).isEmpty());

    ClipCutter::ClipQueueModel model;
    ClipCutter::Clip clip = MakeClip();
    const QUuid clipId = clip.Id;
    model.AddClip(clip);
    const auto second = model.CreateSegment(clipId, *ClipCutter::TimeRange::Create(2s, 3s));
    const auto third = model.CreateSegment(clipId, *ClipCutter::TimeRange::Create(4s, 5s));
    QVERIFY(second.has_value() && third.has_value());
    QCOMPARE(model.FindSegment(*second)->OutputBaseName, QStringLiteral("source_02"));
    QCOMPARE(model.FindSegment(*third)->OutputBaseName, QStringLiteral("source_03"));
}

void AdvancedEditingTests::VersionOneSingleSegmentMigration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = QDir(directory.path()).absoluteFilePath(QStringLiteral("source.mp4"));
    QFile media(source);
    QVERIFY(media.open(QIODevice::WriteOnly));
    media.write("media");
    media.close();
    const QUuid clipId = QUuid::createUuid();
    const QUuid segmentId = QUuid::createUuid();
    QJsonObject clip{{QStringLiteral("id"), clipId.toString(QUuid::WithoutBraces)},
                     {QStringLiteral("segmentId"), segmentId.toString(QUuid::WithoutBraces)},
                     {QStringLiteral("sourcePath"), QStringLiteral("source.mp4")},
                     {QStringLiteral("originalFileName"), QStringLiteral("source.mp4")},
                     {QStringLiteral("startMs"), 1000}, {QStringLiteral("endMs"), 4000},
                     {QStringLiteral("outputBaseName"), QStringLiteral("legacy")},
                     {QStringLiteral("outputExtension"), QStringLiteral(".mp4")},
                     {QStringLiteral("outputProfile"), QStringLiteral("fast-copy")},
                     {QStringLiteral("skipped"), true}};
    const QJsonObject root{{QStringLiteral("schemaVersion"), 1},
                           {QStringLiteral("clips"), QJsonArray{clip}}};
    const QString sessionPath = QDir(directory.path()).absoluteFilePath(QStringLiteral("legacy.json"));
    QFile session(sessionPath);
    QVERIFY(session.open(QIODevice::WriteOnly));
    session.write(QJsonDocument(root).toJson());
    session.close();
    const auto loaded = ClipCutter::SessionRepository::Load(sessionPath);
    QVERIFY2(loaded.IsValid(), qPrintable(loaded.Error));
    QCOMPARE(loaded.Session.SchemaVersion, ClipCutter::SessionData::CurrentSchemaVersion);
    QCOMPARE(loaded.Session.Clips.size(), std::size_t{1});
    QCOMPARE(loaded.Session.Clips.front().Segments.size(), 1);
    QCOMPARE(loaded.Session.Clips.front().Segments.constFirst().Id, segmentId);
    QCOMPARE(loaded.Session.Clips.front().Segments.constFirst().Range, *ClipCutter::TimeRange::Create(1s, 4s));
    QVERIFY(loaded.Session.Clips.front().Segments.constFirst().Skipped);
    QVERIFY(!loaded.Warnings.isEmpty());
}

void AdvancedEditingTests::UndoCommandsAndMerging()
{
    ClipCutter::ClipQueueModel model;
    ClipCutter::Clip clip = MakeClip();
    const QUuid clipId = clip.Id;
    const QUuid first = clip.Segments.constFirst().Id;
    model.AddClip(clip);
    QUndoStack stack;
    stack.push(new ClipCutter::MoveInMarkerCommand(&model, first, 1100ms));
    stack.push(new ClipCutter::MoveInMarkerCommand(&model, first, 1200ms));
    QCOMPARE(stack.count(), 1);
    QCOMPARE(model.FindSegment(first)->Range.Start(), 1200ms);
    stack.undo();
    QCOMPARE(model.FindSegment(first)->Range.Start(), 1s);
    stack.redo();

    stack.push(new ClipCutter::MoveOutMarkerCommand(&model, first, 8s));
    stack.push(new ClipCutter::ReplaceRangeCommand(&model, first, *ClipCutter::TimeRange::Create(2s, 7s)));
    stack.push(new ClipCutter::RenameSegmentCommand(&model, first, QStringLiteral("renamed")));
    stack.push(new ClipCutter::ChangePrefixCommand(&model, first, QStringLiteral("keep_")));
    stack.push(new ClipCutter::ChangeSkipStateCommand(&model, first, true));
    stack.push(new ClipCutter::ChangeOutputProfileCommand(&model, first, QStringLiteral("compact")));
    QCOMPARE(model.FindSegment(first)->Range, *ClipCutter::TimeRange::Create(2s, 7s));
    QCOMPARE(model.FindSegment(first)->OutputBaseName, QStringLiteral("renamed"));
    QCOMPARE(model.FindSegment(first)->Prefix.value(), QStringLiteral("keep_"));
    QVERIFY(model.FindSegment(first)->Skipped);
    QCOMPARE(model.FindSegment(first)->ExportProfileId, QStringLiteral("compact"));

    ClipCutter::Segment added;
    added.Range = *ClipCutter::TimeRange::Create(3s, 4s);
    added.OutputBaseName = QStringLiteral("added");
    added.OutputExtension = QStringLiteral(".mp4");
    added.ExportProfileId = QStringLiteral("fast-copy");
    const QUuid addedId = added.Id;
    stack.push(new ClipCutter::AddSegmentCommand(&model, clipId, added, 1));
    QCOMPARE(model.FindClip(clipId)->Segments.size(), 2);
    stack.push(new ClipCutter::ReorderSegmentCommand(&model, addedId, 0));
    QCOMPARE(model.FindClip(clipId)->Segments.constFirst().Id, addedId);
    stack.push(new ClipCutter::DeleteSegmentCommand(&model, first));
    QVERIFY(model.FindSegment(first) == nullptr);
    stack.undo();
    QCOMPARE(model.FindSegment(first)->Id, first);
    stack.undo();
    QCOMPARE(model.FindClip(clipId)->Segments.at(1).Id, addedId);

    ClipCutter::ClipQueueModel singleModel;
    ClipCutter::Clip single = MakeClip(QStringLiteral("C:/media/single.mp4"));
    const QUuid singleClipId = single.Id;
    const QUuid singleSegmentId = single.Segments.constFirst().Id;
    singleModel.AddClip(single);
    QUndoStack deleteStack;
    deleteStack.push(new ClipCutter::DeleteSegmentCommand(&singleModel, singleSegmentId));
    QVERIFY(singleModel.FindClip(singleClipId) == nullptr);
    deleteStack.undo();
    QCOMPARE(singleModel.FindClip(singleClipId)->Segments.constFirst().Id, singleSegmentId);

    ClipCutter::Segment duplicate = singleModel.FindClip(singleClipId)->Segments.constFirst();
    duplicate.Id = QUuid::createUuid();
    duplicate.OutputBaseName = QStringLiteral("single_02");
    QUndoStack additional;
    additional.push(new ClipCutter::DuplicateSegmentCommand(&singleModel, singleClipId, duplicate, 1));
    QCOMPARE(singleModel.FindClip(singleClipId)->Segments.size(), 2);
    additional.undo();
    QVERIFY(singleModel.FindSegment(duplicate.Id) == nullptr);
    additional.redo();
    QCOMPARE(singleModel.FindSegment(duplicate.Id)->Id, duplicate.Id);

    additional.push(new ClipCutter::ChangeNamingTemplateCommand(
        &singleModel, singleSegmentId, QStringLiteral("{original}_{segment:02}"), QDate(2026, 8, 28)));
    QCOMPARE(singleModel.FindSegment(singleSegmentId)->OutputBaseName, QStringLiteral("single_01"));
    additional.undo();
    QCOMPARE(singleModel.FindSegment(singleSegmentId)->OutputBaseName, QStringLiteral("source_01"));
    additional.redo();

    additional.push(new ClipCutter::BatchSkipStateCommand(&singleModel, {}, true));
    QVERIFY(singleModel.FindSegment(singleSegmentId)->Skipped);
    QVERIFY(singleModel.FindSegment(duplicate.Id)->Skipped);
    additional.undo();
    QVERIFY(!singleModel.FindSegment(singleSegmentId)->Skipped);
    QVERIFY(!singleModel.FindSegment(duplicate.Id)->Skipped);
    additional.redo();
}

void AdvancedEditingTests::TimelineMappingAndZoomBounds()
{
    ClipCutter::TimelineCoordinateMapper mapper;
    mapper.SetViewportWidth(1000.0);
    mapper.SetDuration(10s);
    QCOMPARE(mapper.TimeToPixel(5s), 500.0);
    QCOMPARE(mapper.PixelToTime(250.0), 2500ms);
    mapper.SetZoomFactor(2.0, 5s);
    QCOMPARE(mapper.VisibleStart(), 2500ms);
    QCOMPARE(mapper.VisibleEnd(), 7500ms);
    QCOMPARE(mapper.TimeToPixel(5s), 500.0);
    QVERIFY(mapper.ZoomToSelection(*ClipCutter::TimeRange::Create(4990ms, 5010ms)));
    QCOMPARE(mapper.VisibleDuration(), 20ms);
    mapper.SetZoomFactor(1e100);
    QVERIFY(mapper.ZoomFactor() <= mapper.MaximumZoomFactor());
    mapper.ScrollTo(100s);
    QVERIFY(mapper.VisibleEnd() <= *mapper.Duration());
    mapper.SetDuration(std::nullopt);
    QVERIFY(!mapper.IsUsable());
    QCOMPARE(mapper.PixelToTime(500), 0ms);
}

void AdvancedEditingTests::TimelineWidgetClickSeekAndMarkerRules()
{
    ClipCutter::TimelineWidget widget;
    widget.SetThumbnailsEnabled(false);
    widget.resize(1000, 130);
    widget.SetDuration(10s);
    widget.SetActiveRange(*ClipCutter::TimeRange::Create(2s, 8s));
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QSignalSpy seekSpy(&widget, &ClipCutter::TimelineWidget::SeekRequested);
    const int middleX = qRound(8 + widget.CoordinateMapper().TimeToPixel(5s));
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(middleX, 70));
    QVERIFY(!seekSpy.isEmpty());
    QVERIFY(qAbs(seekSpy.constLast().constFirst().toLongLong() - 5000) < 30);

    QSignalSpy commitSpy(&widget, &ClipCutter::TimelineWidget::RangeEditCommitted);
    const int inX = qRound(8 + widget.CoordinateMapper().TimeToPixel(2s));
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(inX, 70));
    QTest::mouseMove(&widget, QPoint(900, 70));
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(900, 70));
    QVERIFY(!commitSpy.isEmpty());
    const auto range = widget.ActiveRange();
    QVERIFY(range.has_value());
    QVERIFY(range->Start() <= range->End());
    QCOMPARE(range->Start(), range->End());
}

void AdvancedEditingTests::LoopBoundaryBehaviour()
{
    ClipCutter::LoopRangeController loop;
    loop.SetRange(*ClipCutter::TimeRange::Create(1s, 2s));
    loop.SetEnabled(true);
    QVERIFY(!loop.Evaluate(1999ms).has_value());
    QCOMPARE(loop.Evaluate(2500ms), std::optional<std::chrono::milliseconds>{1s});
    QVERIFY(!loop.Evaluate(2600ms).has_value());
    QVERIFY(!loop.Evaluate(1s).has_value());
    QCOMPARE(loop.Evaluate(2s), std::optional<std::chrono::milliseconds>{1s});
    loop.SetEnabled(false);
    QVERIFY(!loop.Evaluate(3s).has_value());
}

void AdvancedEditingTests::RationalFrameStepping()
{
    ClipCutter::MediaInfo media;
    media.FrameRateNumerator = 30'000;
    media.FrameRateDenominator = 1'001;
    media.VariableFrameRate = false;
    const auto duration = ClipCutter::FrameStepper::FrameDurationMilliseconds(media);
    QVERIFY(duration.has_value());
    QVERIFY(qAbs(static_cast<double>(*duration - 33.3666666667L)) < 0.000001);
    const auto three = ClipCutter::FrameStepper::Step(media, 0ms, 3, 10s);
    QCOMPARE(three.Position, 100ms);
    QVERIFY(!three.Approximate);
    media.VariableFrameRate = true;
    QVERIFY(ClipCutter::FrameStepper::Step(media, 0ms, 1).Approximate);
    media.FrameRateNumerator.reset();
    media.FrameRateDenominator.reset();
    QCOMPARE(ClipCutter::FrameStepper::Step(media, 0ms, 1).Position, 40ms);
}

void AdvancedEditingTests::ThumbnailKeysAndCancellation()
{
    const QString a = ClipCutter::FfmpegThumbnailProvider::BuildCacheKey(QStringLiteral("fingerprint"), 1000,
                                                                         QSize(120, 50));
    const QString b = ClipCutter::FfmpegThumbnailProvider::BuildCacheKey(QStringLiteral("fingerprint"), 1001,
                                                                         QSize(120, 50));
    QCOMPARE(a.size(), 64);
    QVERIFY(a != b);
    ClipCutter::FfmpegThumbnailProvider provider(nullptr, QStringLiteral("missing-ffmpeg"));
    const quint64 first = provider.Generation();
    provider.Cancel();
    QVERIFY(provider.Generation() > first);
    QVERIFY(!provider.IsCurrentGeneration(first));
    QVERIFY(provider.IsCurrentGeneration(provider.Generation()));
    QTemporaryDir directory;
    const QString source = QDir(directory.path()).absoluteFilePath(QStringLiteral("source.mp4"));
    QFile file(source);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not-real-media");
    file.close();
    QVERIFY(!ClipCutter::FfmpegThumbnailProvider::BuildSourceFingerprint(source).isEmpty());
    const auto range = *ClipCutter::TimeRange::Create(0ms, 1s);
    const quint64 requestOne = provider.RequestVisible(source, range, QSize(120, 50), 2);
    const quint64 requestTwo = provider.RequestVisible(source, range, QSize(120, 50), 2);
    QVERIFY(requestTwo > requestOne);
    QVERIFY(!provider.IsCurrentGeneration(requestOne));
    QVERIFY(provider.IsCurrentGeneration(requestTwo));
}

void AdvancedEditingTests::SegmentExportSnapshots()
{
    ClipCutter::ClipQueueModel model;
    ClipCutter::Clip clip = MakeClip();
    const QUuid clipId = clip.Id;
    model.AddClip(clip);
    QVERIFY(model.CreateSegment(clipId, *ClipCutter::TimeRange::Create(2s, 4s)).has_value());
    QVERIFY(model.CreateSegment(clipId, *ClipCutter::TimeRange::Create(5s, 7s)).has_value());
    model.UpdateSkipState(model.SegmentIdAtRow(1), true);
    QStringList errors;
    const auto all = model.ExportSegments(&errors);
    QVERIFY(errors.isEmpty());
    QCOMPARE(all.size(), 3);
    QCOMPARE(model.ExportableSegments().size(), 2);
    QSet<QUuid> ids;
    for (const auto& segment : all) ids.insert(segment.SegmentId);
    QCOMPARE(ids.size(), 3);
}

QTEST_MAIN(AdvancedEditingTests)

#include "AdvancedEditingTests.moc"
