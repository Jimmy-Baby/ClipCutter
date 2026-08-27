#include <QAbstractItemModelTester>
#include <QFile>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QTemporaryDir>
#include <QtTest>

#include "App/Models/ClipQueueModel.h"
#include "ClipLogic.h"
#include "Core/Import/ClipImporter.h"
#include "Utility.h"

#include <chrono>

using namespace std::chrono_literals;

namespace
{
ClipCutter::Clip MakeClip(const QString& name, qint64 duration = 10'000)
{
    ClipCutter::Clip clip;
    clip.SourcePath = QStringLiteral("C:/media/%1").arg(name);
    clip.OriginalFileName = name;
    clip.MediaInformation.Duration = std::chrono::milliseconds{duration};
    clip.MediaInformation.ProbeStatus = ClipCutter::EProbeStatus::Ready;
    ClipCutter::Segment segment;
    const QFileInfo info(name);
    segment.OutputBaseName = info.completeBaseName();
    segment.OutputExtension = QStringLiteral(".") + info.suffix();
    segment.Range = *ClipCutter::TimeRange::Create(0ms, std::chrono::milliseconds{duration});
    clip.Segments.append(segment);
    return clip;
}

QString CreateFile(const QDir& directory, const QString& name)
{
    const QString path = directory.absoluteFilePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return {};
    }
    file.close();
    return path;
}
} // namespace

class NameEditor : public QObject
{
    Q_OBJECT

public:
    void SetText(const QString& text)
    {
        Text_ = text;
        emit TextChanged(text);
    }

signals:
    void TextChanged(const QString& text);

private:
    QString Text_;
};

class RegressionTests : public QObject
{
    Q_OBJECT

private slots:
    void DurationFormatting_data();
    void DurationFormatting();
    void NegativeDurationIsRejected();
    void KeywordNormalization();
    void DuplicateKeywordsAreCaseInsensitive();
    void OutputNaming();
    void OutputDirectoryIsCreatedAndReusable();
    void OutputDirectoryRejectsFileCollision();
    void TimeRangeValidationAndClamping();
    void ClipAndSegmentIdentityIsStable();
    void ModelRowsColumnsAndRoles();
    void ModelEditingSkipNamingAndRange();
    void ModelStableLookupRemovalAndClear();
    void ModelExportSnapshotAndSkipAll();
    void ImporterFiltersDuplicatesAndSorts();
    void ProgrammaticSelectionDoesNotRename();
};

void RegressionTests::DurationFormatting_data()
{
    QTest::addColumn<qint64>("milliseconds");
    QTest::addColumn<QString>("expected");

    QTest::newRow("zero") << qint64(0) << QStringLiteral("00:00:00.000");
    QTest::newRow("sub-second") << qint64(987) << QStringLiteral("00:00:00.987");
    QTest::newRow("one-minute") << qint64(60'000) << QStringLiteral("00:01:00.000");
    QTest::newRow("one-hour") << qint64(3'600'000) << QStringLiteral("01:00:00.000");
    QTest::newRow("59-hours") << qint64(59) * 3'600'000 << QStringLiteral("59:00:00.000");
    QTest::newRow("60-hours") << qint64(60) * 3'600'000 << QStringLiteral("60:00:00.000");
    QTest::newRow("over-100-hours") << qint64(123) * 3'600'000 + 45 * 60'000 + 6'007 << QStringLiteral("123:45:06.007");
}

void RegressionTests::DurationFormatting()
{
    QFETCH(qint64, milliseconds);
    QFETCH(QString, expected);

    QCOMPARE(ClipCutter::Utility::GetTimeStringFromMilliseconds(milliseconds), expected);
}

void RegressionTests::NegativeDurationIsRejected()
{
    QVERIFY(ClipCutter::Utility::GetTimeStringFromMilliseconds(-1).isEmpty());
}

void RegressionTests::KeywordNormalization()
{
    QCOMPARE(ClipCutter::ClipLogic::NormalizeKeyword(QStringLiteral("  highlights \t")), QStringLiteral("highlights"));
    QVERIFY(ClipCutter::ClipLogic::NormalizeKeyword(QStringLiteral(" \t\r\n ")).isEmpty());
}

void RegressionTests::DuplicateKeywordsAreCaseInsensitive()
{
    const QStringList keywords{QStringLiteral("Highlights"), QStringLiteral("Short_")};

    QVERIFY(ClipCutter::ClipLogic::ContainsKeyword(keywords, QStringLiteral(" highlights ")));
    QVERIFY(ClipCutter::ClipLogic::ContainsKeyword(keywords, QStringLiteral("SHORT_")));
    QVERIFY(!ClipCutter::ClipLogic::ContainsKeyword(keywords, QStringLiteral("Archive")));
}

void RegressionTests::OutputNaming()
{
    QCOMPARE(ClipCutter::ClipLogic::GetOutputName(QStringLiteral("clip.mp4"), QString()), QStringLiteral("clip.mp4"));
    QCOMPARE(ClipCutter::ClipLogic::GetOutputName(QStringLiteral("clip.mp4"), QStringLiteral("Highlights_")),
             QStringLiteral("Highlights_clip.mp4"));
}

void RegressionTests::OutputDirectoryIsCreatedAndReusable()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QString outputDirectory;
    QString errorMessage;
    QVERIFY2(ClipCutter::Utility::PrepareOutputDirectory(QDir(temporaryDirectory.path()), outputDirectory, errorMessage),
             qPrintable(errorMessage));
    QVERIFY(QFileInfo(outputDirectory).isDir());

    QString existingOutputDirectory;
    QVERIFY2(ClipCutter::Utility::PrepareOutputDirectory(QDir(temporaryDirectory.path()), existingOutputDirectory, errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(existingOutputDirectory, outputDirectory);
}

void RegressionTests::OutputDirectoryRejectsFileCollision()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString collisionPath = QDir(temporaryDirectory.path()).absoluteFilePath(QStringLiteral("ClipCutterOutput"));
    QFile collisionFile(collisionPath);
    QVERIFY(collisionFile.open(QIODevice::WriteOnly));
    collisionFile.close();

    QString outputDirectory;
    QString errorMessage;
    QVERIFY(!ClipCutter::Utility::PrepareOutputDirectory(QDir(temporaryDirectory.path()), outputDirectory, errorMessage));
    QVERIFY(errorMessage.contains(collisionPath));
}

void RegressionTests::TimeRangeValidationAndClamping()
{
    QString error;
    QVERIFY(!ClipCutter::TimeRange::Create(-1ms, 1ms, std::nullopt, false, &error).has_value());
    QVERIFY(!error.isEmpty());
    QVERIFY(!ClipCutter::TimeRange::Create(2ms, 1ms).has_value());
    QVERIFY(ClipCutter::TimeRange::Create(1ms, 1ms).has_value());
    QVERIFY(!ClipCutter::TimeRange::Create(1ms, 1ms, std::nullopt, true).has_value());
    QVERIFY(!ClipCutter::TimeRange::Create(0ms, 11ms, 10ms).has_value());

    const auto range = ClipCutter::TimeRange::Create(2ms, 9ms, 10ms, true);
    QVERIFY(range.has_value());
    QCOMPARE(range->Start(), 2ms);
    QCOMPARE(range->End(), 9ms);
    QCOMPARE(range->Duration(), 7ms);

    const ClipCutter::TimeRange clamped = ClipCutter::TimeRange::Clamped(-5ms, 20ms, 10ms);
    QCOMPARE(clamped.Start(), 0ms);
    QCOMPARE(clamped.End(), 10ms);
}

void RegressionTests::ClipAndSegmentIdentityIsStable()
{
    const ClipCutter::Clip first = MakeClip(QStringLiteral("one.mp4"));
    const ClipCutter::Clip second = MakeClip(QStringLiteral("two.mp4"));
    QVERIFY(!first.Id.isNull());
    QVERIFY(!first.Segments.constFirst().Id.isNull());
    QVERIFY(first.Id != second.Id);
    QVERIFY(first.Segments.constFirst().Id != second.Segments.constFirst().Id);

    const ClipCutter::Clip copy = first;
    QCOMPARE(copy.Id, first.Id);
    QCOMPARE(copy.Segments.constFirst().Id, first.Segments.constFirst().Id);
}

void RegressionTests::ModelRowsColumnsAndRoles()
{
    ClipCutter::ClipQueueModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const ClipCutter::Clip clip = MakeClip(QStringLiteral("source.mp4"));
    const QUuid clipId = clip.Id;
    const QUuid segmentId = clip.Segments.constFirst().Id;
    model.AddClip(clip);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.columnCount(), int(ClipCutter::ClipQueueModel::ColumnCount));
    QCOMPARE(model.data(model.index(0, ClipCutter::ClipQueueModel::SourceNameColumn)).toString(),
             QStringLiteral("source.mp4"));
    QCOMPARE(model.data(model.index(0, 0), ClipCutter::ClipQueueModel::ClipIdRole).toUuid(), clipId);
    QCOMPARE(model.data(model.index(0, 0), ClipCutter::ClipQueueModel::SegmentIdRole).toUuid(), segmentId);
    QCOMPARE(model.RowForClip(clipId), 0);
    QCOMPARE(model.RowForSegment(segmentId), 0);
}

void RegressionTests::ModelEditingSkipNamingAndRange()
{
    ClipCutter::ClipQueueModel model;
    ClipCutter::Clip clip = MakeClip(QStringLiteral("source.mp4"));
    const QUuid segmentId = clip.Segments.constFirst().Id;
    model.AddClip(std::move(clip));

    const QModelIndex skip = model.index(0, ClipCutter::ClipQueueModel::SkipColumn);
    QVERIFY(model.setData(skip, Qt::Checked, Qt::CheckStateRole));
    QVERIFY(model.FindSegment(segmentId)->Skipped);
    QCOMPARE(model.data(skip, Qt::CheckStateRole).toInt(), int(Qt::Checked));

    const QModelIndex output = model.index(0, ClipCutter::ClipQueueModel::OutputNameColumn);
    QVERIFY(model.setData(output, QStringLiteral("renamed"), Qt::EditRole));
    QCOMPARE(model.FindSegment(segmentId)->OutputFileName(), QStringLiteral("renamed.mp4"));
    QVERIFY(model.ApplyPrefix(segmentId, QStringLiteral("Highlights_")));
    QCOMPARE(model.FindSegment(segmentId)->OutputFileName(), QStringLiteral("Highlights_renamed.mp4"));
    QVERIFY(model.ClearPrefix(segmentId));

    QString error;
    const auto range = ClipCutter::TimeRange::Create(1s, 7s, 10s, true);
    QVERIFY(range.has_value());
    QVERIFY(model.UpdateTrimRange(segmentId, *range, &error));
    QCOMPARE(model.FindSegment(segmentId)->Range.Duration(), 6s);
    QVERIFY(!model.UpdateTrimRange(segmentId, ClipCutter::TimeRange::Clamped(0ms, 12s, 12s), &error));
}

void RegressionTests::ModelStableLookupRemovalAndClear()
{
    ClipCutter::ClipQueueModel model;
    ClipCutter::Clip first = MakeClip(QStringLiteral("one.mp4"));
    ClipCutter::Clip second = MakeClip(QStringLiteral("two.mp4"));
    ClipCutter::Clip third = MakeClip(QStringLiteral("three.mp4"));
    const QUuid firstId = first.Id;
    const QUuid secondId = second.Id;
    const QUuid secondSegmentId = second.Segments.constFirst().Id;
    const QUuid thirdId = third.Id;
    model.AddClip(std::move(first));
    model.AddClip(std::move(second));
    model.AddClip(std::move(third));

    QVERIFY(model.RemoveEntries({secondSegmentId}));
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.FindClip(secondId) == nullptr);
    QCOMPARE(model.RowForClip(firstId), 0);
    QCOMPARE(model.RowForClip(thirdId), 1);
    QCOMPARE(model.ClipIdAtRow(1), thirdId);
    model.ClearClips();
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.FindClip(firstId) == nullptr);
}

void RegressionTests::ModelExportSnapshotAndSkipAll()
{
    ClipCutter::ClipQueueModel model;
    model.AddClip(MakeClip(QStringLiteral("one.mp4")));
    model.AddClip(MakeClip(QStringLiteral("two.mp4")));
    model.UpdateAllExportProfiles(QStringLiteral("high"));
    QVERIFY(model.ApplyPrefix(model.SegmentIdAtRow(0), QStringLiteral("cut_")));

    QStringList errors;
    const QVector<ClipCutter::ExportSegment> exports = model.ExportableSegments(&errors);
    QVERIFY(errors.isEmpty());
    QCOMPARE(exports.size(), 2);
    QCOMPARE(exports.constFirst().OutputFileName, QStringLiteral("cut_one.mp4"));
    QCOMPARE(exports.constFirst().ExportProfileId, QStringLiteral("high"));

    model.UpdateAllSkipStates(true);
    QVERIFY(model.FindSegment(model.SegmentIdAtRow(0))->Skipped);
    QVERIFY(model.FindSegment(model.SegmentIdAtRow(1))->Skipped);
    QVERIFY(model.ExportableSegments().isEmpty());
}

void RegressionTests::ImporterFiltersDuplicatesAndSorts()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir directory(temporaryDirectory.path());
    const QString zeta = CreateFile(directory, QStringLiteral("zeta.MP4"));
    const QString alpha = CreateFile(directory, QStringLiteral("Alpha.mov"));
    CreateFile(directory, QStringLiteral("notes.txt"));
    QVERIFY(!zeta.isEmpty());
    QVERIFY(!alpha.isEmpty());

    ClipCutter::ClipImporter importer;
    const ClipCutter::ImportResult directoryResult = importer.ImportDirectory(directory);
    QCOMPARE(directoryResult.Imported.size(), 2);
    QCOMPARE(directoryResult.Imported.at(0).OriginalFileName, QStringLiteral("Alpha.mov"));
    QCOMPARE(directoryResult.Imported.at(1).OriginalFileName, QStringLiteral("zeta.MP4"));
    QCOMPARE(directoryResult.Skipped.size(), 1);

    const ClipCutter::ImportResult duplicateResult = importer.ImportFiles({zeta, alpha, zeta});
    QCOMPARE(duplicateResult.Imported.size(), 2);
    QCOMPARE(duplicateResult.Skipped.size(), 1);
    QCOMPARE(duplicateResult.Imported.at(0).OriginalFileName, QStringLiteral("Alpha.mov"));
    QVERIFY(!ClipCutter::ClipImporter::IsSupported(QStringLiteral("archive.txt")));
    QVERIFY(ClipCutter::ClipImporter::IsSupported(QStringLiteral("movie.MKV")));
}

void RegressionTests::ProgrammaticSelectionDoesNotRename()
{
    ClipCutter::ClipQueueModel model;
    model.AddClip(MakeClip(QStringLiteral("first.mp4")));
    model.AddClip(MakeClip(QStringLiteral("second.mp4")));
    QItemSelectionModel selection(&model);
    NameEditor editor;
    QUuid currentSegmentId;
    connect(&editor, &NameEditor::TextChanged, &model,
            [&model, &currentSegmentId](const QString& text) { model.UpdateOutputBaseName(currentSegmentId, text); });
    connect(&selection, &QItemSelectionModel::currentChanged, &model,
            [&model, &editor, &currentSegmentId](const QModelIndex& current)
            {
                currentSegmentId = model.SegmentIdAtRow(current.row());
                const QSignalBlocker blocker(&editor);
                editor.SetText(model.FindSegment(currentSegmentId)->OutputBaseName);
            });

    selection.setCurrentIndex(model.index(0, 0), QItemSelectionModel::ClearAndSelect);
    selection.setCurrentIndex(model.index(1, 0), QItemSelectionModel::ClearAndSelect);
    selection.setCurrentIndex(model.index(0, 0), QItemSelectionModel::ClearAndSelect);
    QCOMPARE(model.FindSegment(model.SegmentIdAtRow(0))->OutputBaseName, QStringLiteral("first"));
    QCOMPARE(model.FindSegment(model.SegmentIdAtRow(1))->OutputBaseName, QStringLiteral("second"));
}

QTEST_APPLESS_MAIN(RegressionTests)

#include "RegressionTests.moc"
