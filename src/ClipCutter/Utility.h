#ifndef CLIPCUTTER_UTILITY_H
#define CLIPCUTTER_UTILITY_H

#include <QDir>
#include <QString>

namespace ClipCutter::Utility
{
QString GetTimeStringFromMilliseconds(qint64 milliseconds);
bool PrepareOutputDirectory(const QDir& videoDirectory, QString& outputDirectory, QString& errorMessage);
} // namespace ClipCutter::Utility

#endif
