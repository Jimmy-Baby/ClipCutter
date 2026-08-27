#ifndef UTILITY_H
#define UTILITY_H

#include <QDir>
#include <QString>

namespace Utility
{
    QString GetTimeStringFromMilli(qint64 milliseconds);
    bool PrepareOutputDirectory(const QDir& videoDirectory, QString& outputDirectory, QString& errorMessage);
}

#endif // UTILITY_H
