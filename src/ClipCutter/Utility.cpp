#include <QFileInfo>
#include <QString>
#include <QTemporaryFile>

#include "Utility.h"

namespace ClipCutter::Utility
{
QString GetTimeStringFromMilliseconds(qint64 millisecondsTotal)
{
    if (millisecondsTotal < 0)
    {
        return {};
    }

    const qint64 milliseconds = millisecondsTotal % 1000;
    const qint64 seconds = millisecondsTotal / 1000 % 60;
    const qint64 minutes = millisecondsTotal / 1000 / 60 % 60;
    const qint64 hours = millisecondsTotal / 1000 / 60 / 60;

    return QStringLiteral("%1:%2:%3.%4")
        .arg(hours, 2, 10, QChar(u'0'))
        .arg(minutes, 2, 10, QChar(u'0'))
        .arg(seconds, 2, 10, QChar(u'0'))
        .arg(milliseconds, 3, 10, QChar(u'0'));
}

bool PrepareOutputDirectory(const QDir& videoDirectory, QString& outputDirectory, QString& errorMessage)
{
    outputDirectory = videoDirectory.absoluteFilePath(QStringLiteral("ClipCutterOutput"));
    const QFileInfo existingPath(outputDirectory);

    if (existingPath.exists() && !existingPath.isDir())
    {
        errorMessage = QStringLiteral("The output path exists but is not a directory:\n%1").arg(outputDirectory);
        return false;
    }

    if (!existingPath.exists() && !videoDirectory.mkpath(QStringLiteral("ClipCutterOutput")))
    {
        errorMessage = QStringLiteral("Could not create the output directory:\n%1").arg(outputDirectory);
        return false;
    }

    if (!QFileInfo(outputDirectory).isDir())
    {
        errorMessage = QStringLiteral("The output path is not a directory:\n%1").arg(outputDirectory);
        return false;
    }

    QTemporaryFile writeTest(QDir(outputDirectory).absoluteFilePath(QStringLiteral(".clipcutter-write-test-XXXXXX")));
    if (!writeTest.open())
    {
        errorMessage = QStringLiteral("The output directory is not writable:\n%1").arg(outputDirectory);
        return false;
    }

    return true;
}
} // namespace ClipCutter::Utility
