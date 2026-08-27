#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "ClipLogic.h"
#include "Utility.h"

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

QTEST_APPLESS_MAIN(RegressionTests)

#include "RegressionTests.moc"
