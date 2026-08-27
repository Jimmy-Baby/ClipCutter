#include <QFile>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QTemporaryDir>
#include <QtTest>
#include <QAbstractItemModelTester>

#include "app/models/ClipQueueModel.h"
#include "ClipLogic.h"
#include "core/import/ClipImporter.h"
#include "Utility.h"

#include <chrono>

using namespace std::chrono_literals;

namespace
{
clipcutter::Clip makeClip(const QString& name, qint64 duration = 10'000)
{
    clipcutter::Clip clip;
    clip.sourcePath = QStringLiteral("C:/media/%1").arg(name);
    clip.originalFileName = name;
    clip.mediaInfo.duration = std::chrono::milliseconds{duration};
    clip.mediaInfo.probeStatus = clipcutter::ProbeStatus::Ready;
    clipcutter::Segment segment;
    const QFileInfo info(name);
    segment.outputBaseName = info.completeBaseName();
    segment.outputExtension = QStringLiteral(".") + info.suffix();
    segment.range = *clipcutter::TimeRange::create(0ms, std::chrono::milliseconds{duration});
    clip.segments.append(segment);
    return clip;
}

QString createFile(const QDir& directory, const QString& name)
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
}

class NameEditor : public QObject
{
    Q_OBJECT

public:
    void setText(const QString& text)
    {
        m_text = text;
        emit textChanged(text);
    }

signals:
    void textChanged(const QString& text);

private:
    QString m_text;
};

class RegressionTests : public QObject
{
    Q_OBJECT

private slots:
    void durationFormatting_data();
    void durationFormatting();
    void negativeDurationIsRejected();
    void keywordNormalization();
    void duplicateKeywordsAreCaseInsensitive();
    void outputNaming();
    void outputDirectoryIsCreatedAndReusable();
    void outputDirectoryRejectsFileCollision();
    void timeRangeValidationAndClamping();
    void clipAndSegmentIdentityIsStable();
    void modelRowsColumnsAndRoles();
    void modelEditingSkipNamingAndRange();
    void modelStableLookupRemovalAndClear();
    void modelExportSnapshotAndSkipAll();
    void importerFiltersDuplicatesAndSorts();
    void programmaticSelectionDoesNotRename();
};

void RegressionTests::durationFormatting_data()
{
    QTest::addColumn<qint64>("milliseconds");
    QTest::addColumn<QString>("expected");

    QTest::newRow("zero") << qint64(0) << QStringLiteral("00:00:00.000");
    QTest::newRow("sub-second") << qint64(987) << QStringLiteral("00:00:00.987");
    QTest::newRow("one-minute") << qint64(60'000) << QStringLiteral("00:01:00.000");
    QTest::newRow("one-hour") << qint64(3'600'000) << QStringLiteral("01:00:00.000");
    QTest::newRow("59-hours") << qint64(59) * 3'600'000 << QStringLiteral("59:00:00.000");
    QTest::newRow("60-hours") << qint64(60) * 3'600'000 << QStringLiteral("60:00:00.000");
    QTest::newRow("over-100-hours")
        << qint64(123) * 3'600'000 + 45 * 60'000 + 6'007
        << QStringLiteral("123:45:06.007");
}

void RegressionTests::durationFormatting()
{
    QFETCH(qint64, milliseconds);
    QFETCH(QString, expected);

    QCOMPARE(Utility::GetTimeStringFromMilli(milliseconds), expected);
}

void RegressionTests::negativeDurationIsRejected()
{
    QVERIFY(Utility::GetTimeStringFromMilli(-1).isEmpty());
}

void RegressionTests::keywordNormalization()
{
    QCOMPARE(ClipLogic::NormalizeKeyword(QStringLiteral("  highlights \t")), QStringLiteral("highlights"));
    QVERIFY(ClipLogic::NormalizeKeyword(QStringLiteral(" \t\r\n ")).isEmpty());
}

void RegressionTests::duplicateKeywordsAreCaseInsensitive()
{
    const QStringList keywords{ QStringLiteral("Highlights"), QStringLiteral("Short_") };

    QVERIFY(ClipLogic::ContainsKeyword(keywords, QStringLiteral(" highlights ")));
    QVERIFY(ClipLogic::ContainsKeyword(keywords, QStringLiteral("SHORT_")));
    QVERIFY(!ClipLogic::ContainsKeyword(keywords, QStringLiteral("Archive")));
}

void RegressionTests::outputNaming()
{
    QCOMPARE(ClipLogic::GetOutputName(QStringLiteral("clip.mp4"), QString()), QStringLiteral("clip.mp4"));
    QCOMPARE(
        ClipLogic::GetOutputName(QStringLiteral("clip.mp4"), QStringLiteral("Highlights_")),
        QStringLiteral("Highlights_clip.mp4"));
}

void RegressionTests::outputDirectoryIsCreatedAndReusable()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QString outputDirectory;
    QString errorMessage;
    QVERIFY2(
        Utility::PrepareOutputDirectory(QDir(temporaryDirectory.path()), outputDirectory, errorMessage),
        qPrintable(errorMessage));
    QVERIFY(QFileInfo(outputDirectory).isDir());

    QString existingOutputDirectory;
    QVERIFY2(
        Utility::PrepareOutputDirectory(QDir(temporaryDirectory.path()), existingOutputDirectory, errorMessage),
        qPrintable(errorMessage));
    QCOMPARE(existingOutputDirectory, outputDirectory);
}

void RegressionTests::outputDirectoryRejectsFileCollision()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString collisionPath = QDir(temporaryDirectory.path()).absoluteFilePath(QStringLiteral("ClipCutterOutput"));
    QFile collisionFile(collisionPath);
    QVERIFY(collisionFile.open(QIODevice::WriteOnly));
    collisionFile.close();

    QString outputDirectory;
    QString errorMessage;
    QVERIFY(!Utility::PrepareOutputDirectory(QDir(temporaryDirectory.path()), outputDirectory, errorMessage));
    QVERIFY(errorMessage.contains(collisionPath));
}

void RegressionTests::timeRangeValidationAndClamping()
{
    QString error;
    QVERIFY(!clipcutter::TimeRange::create(-1ms, 1ms, std::nullopt, false, &error).has_value());
    QVERIFY(!error.isEmpty());
    QVERIFY(!clipcutter::TimeRange::create(2ms, 1ms).has_value());
    QVERIFY(clipcutter::TimeRange::create(1ms, 1ms).has_value());
    QVERIFY(!clipcutter::TimeRange::create(1ms, 1ms, std::nullopt, true).has_value());
    QVERIFY(!clipcutter::TimeRange::create(0ms, 11ms, 10ms).has_value());

    const auto range = clipcutter::TimeRange::create(2ms, 9ms, 10ms, true);
    QVERIFY(range.has_value());
    QCOMPARE(range->start(), 2ms);
    QCOMPARE(range->end(), 9ms);
    QCOMPARE(range->duration(), 7ms);

    const clipcutter::TimeRange clamped = clipcutter::TimeRange::clamped(-5ms, 20ms, 10ms);
    QCOMPARE(clamped.start(), 0ms);
    QCOMPARE(clamped.end(), 10ms);
}

void RegressionTests::clipAndSegmentIdentityIsStable()
{
    const clipcutter::Clip first = makeClip(QStringLiteral("one.mp4"));
    const clipcutter::Clip second = makeClip(QStringLiteral("two.mp4"));
    QVERIFY(!first.id.isNull());
    QVERIFY(!first.segments.constFirst().id.isNull());
    QVERIFY(first.id != second.id);
    QVERIFY(first.segments.constFirst().id != second.segments.constFirst().id);

    const clipcutter::Clip copy = first;
    QCOMPARE(copy.id, first.id);
    QCOMPARE(copy.segments.constFirst().id, first.segments.constFirst().id);
}

void RegressionTests::modelRowsColumnsAndRoles()
{
    clipcutter::ClipQueueModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const clipcutter::Clip clip = makeClip(QStringLiteral("source.mp4"));
    const QUuid clipId = clip.id;
    const QUuid segmentId = clip.segments.constFirst().id;
    model.addClip(clip);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.columnCount(), int(clipcutter::ClipQueueModel::ColumnCount));
    QCOMPARE(model.data(model.index(0, clipcutter::ClipQueueModel::SourceNameColumn)).toString(),
             QStringLiteral("source.mp4"));
    QCOMPARE(model.data(model.index(0, 0), clipcutter::ClipQueueModel::ClipIdRole).toUuid(), clipId);
    QCOMPARE(model.data(model.index(0, 0), clipcutter::ClipQueueModel::SegmentIdRole).toUuid(), segmentId);
    QCOMPARE(model.rowForClip(clipId), 0);
    QCOMPARE(model.rowForSegment(segmentId), 0);
}

void RegressionTests::modelEditingSkipNamingAndRange()
{
    clipcutter::ClipQueueModel model;
    clipcutter::Clip clip = makeClip(QStringLiteral("source.mp4"));
    const QUuid segmentId = clip.segments.constFirst().id;
    model.addClip(std::move(clip));

    const QModelIndex skip = model.index(0, clipcutter::ClipQueueModel::SkipColumn);
    QVERIFY(model.setData(skip, Qt::Checked, Qt::CheckStateRole));
    QVERIFY(model.findSegment(segmentId)->skipped);
    QCOMPARE(model.data(skip, Qt::CheckStateRole).toInt(), int(Qt::Checked));

    const QModelIndex output = model.index(0, clipcutter::ClipQueueModel::OutputNameColumn);
    QVERIFY(model.setData(output, QStringLiteral("renamed"), Qt::EditRole));
    QCOMPARE(model.findSegment(segmentId)->outputFileName(), QStringLiteral("renamed.mp4"));
    QVERIFY(model.applyPrefix(segmentId, QStringLiteral("Highlights_")));
    QCOMPARE(model.findSegment(segmentId)->outputFileName(), QStringLiteral("Highlights_renamed.mp4"));
    QVERIFY(model.clearPrefix(segmentId));

    QString error;
    const auto range = clipcutter::TimeRange::create(1s, 7s, 10s, true);
    QVERIFY(range.has_value());
    QVERIFY(model.updateTrimRange(segmentId, *range, &error));
    QCOMPARE(model.findSegment(segmentId)->range.duration(), 6s);
    QVERIFY(!model.updateTrimRange(segmentId, clipcutter::TimeRange::clamped(0ms, 12s, 12s), &error));
}

void RegressionTests::modelStableLookupRemovalAndClear()
{
    clipcutter::ClipQueueModel model;
    clipcutter::Clip first = makeClip(QStringLiteral("one.mp4"));
    clipcutter::Clip second = makeClip(QStringLiteral("two.mp4"));
    clipcutter::Clip third = makeClip(QStringLiteral("three.mp4"));
    const QUuid firstId = first.id;
    const QUuid secondId = second.id;
    const QUuid secondSegmentId = second.segments.constFirst().id;
    const QUuid thirdId = third.id;
    model.addClip(std::move(first));
    model.addClip(std::move(second));
    model.addClip(std::move(third));

    QVERIFY(model.removeEntries({ secondSegmentId }));
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.findClip(secondId) == nullptr);
    QCOMPARE(model.rowForClip(firstId), 0);
    QCOMPARE(model.rowForClip(thirdId), 1);
    QCOMPARE(model.clipIdAtRow(1), thirdId);
    model.clearClips();
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.findClip(firstId) == nullptr);
}

void RegressionTests::modelExportSnapshotAndSkipAll()
{
    clipcutter::ClipQueueModel model;
    model.addClip(makeClip(QStringLiteral("one.mp4")));
    model.addClip(makeClip(QStringLiteral("two.mp4")));
    model.updateAllExportProfiles(QStringLiteral("high"));
    QVERIFY(model.applyPrefix(model.segmentIdAtRow(0), QStringLiteral("cut_")));

    QStringList errors;
    const QVector<clipcutter::ExportSegment> exports = model.exportableSegments(&errors);
    QVERIFY(errors.isEmpty());
    QCOMPARE(exports.size(), 2);
    QCOMPARE(exports.constFirst().outputFileName, QStringLiteral("cut_one.mp4"));
    QCOMPARE(exports.constFirst().exportProfileId, QStringLiteral("high"));

    model.updateAllSkipStates(true);
    QVERIFY(model.findSegment(model.segmentIdAtRow(0))->skipped);
    QVERIFY(model.findSegment(model.segmentIdAtRow(1))->skipped);
    QVERIFY(model.exportableSegments().isEmpty());
}

void RegressionTests::importerFiltersDuplicatesAndSorts()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir directory(temporaryDirectory.path());
    const QString zeta = createFile(directory, QStringLiteral("zeta.MP4"));
    const QString alpha = createFile(directory, QStringLiteral("Alpha.mov"));
    createFile(directory, QStringLiteral("notes.txt"));
    QVERIFY(!zeta.isEmpty());
    QVERIFY(!alpha.isEmpty());

    clipcutter::ClipImporter importer;
    const clipcutter::ImportResult directoryResult = importer.importDirectory(directory);
    QCOMPARE(directoryResult.imported.size(), 2);
    QCOMPARE(directoryResult.imported.at(0).originalFileName, QStringLiteral("Alpha.mov"));
    QCOMPARE(directoryResult.imported.at(1).originalFileName, QStringLiteral("zeta.MP4"));
    QCOMPARE(directoryResult.skipped.size(), 1);

    const clipcutter::ImportResult duplicateResult = importer.importFiles({ zeta, alpha, zeta });
    QCOMPARE(duplicateResult.imported.size(), 2);
    QCOMPARE(duplicateResult.skipped.size(), 1);
    QCOMPARE(duplicateResult.imported.at(0).originalFileName, QStringLiteral("Alpha.mov"));
    QVERIFY(!clipcutter::ClipImporter::isSupported(QStringLiteral("archive.txt")));
    QVERIFY(clipcutter::ClipImporter::isSupported(QStringLiteral("movie.MKV")));
}

void RegressionTests::programmaticSelectionDoesNotRename()
{
    clipcutter::ClipQueueModel model;
    model.addClip(makeClip(QStringLiteral("first.mp4")));
    model.addClip(makeClip(QStringLiteral("second.mp4")));
    QItemSelectionModel selection(&model);
    NameEditor editor;
    QUuid currentSegmentId;
    connect(&editor, &NameEditor::textChanged, &model, [&model, &currentSegmentId](const QString& text)
    {
        model.updateOutputBaseName(currentSegmentId, text);
    });
    connect(&selection, &QItemSelectionModel::currentChanged, &model,
            [&model, &editor, &currentSegmentId](const QModelIndex& current)
    {
        currentSegmentId = model.segmentIdAtRow(current.row());
        const QSignalBlocker blocker(&editor);
        editor.setText(model.findSegment(currentSegmentId)->outputBaseName);
    });

    selection.setCurrentIndex(model.index(0, 0), QItemSelectionModel::ClearAndSelect);
    selection.setCurrentIndex(model.index(1, 0), QItemSelectionModel::ClearAndSelect);
    selection.setCurrentIndex(model.index(0, 0), QItemSelectionModel::ClearAndSelect);
    QCOMPARE(model.findSegment(model.segmentIdAtRow(0))->outputBaseName, QStringLiteral("first"));
    QCOMPARE(model.findSegment(model.segmentIdAtRow(1))->outputBaseName, QStringLiteral("second"));
}

QTEST_APPLESS_MAIN(RegressionTests)

#include "RegressionTests.moc"
