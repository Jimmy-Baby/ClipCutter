#ifndef UTILITY_H
#define UTILITY_H

#include <QObject>

namespace Utility
{
    QString GetTimeStringFromMilli(quint64 ms);
    quint64 GetVideoLength(QUrl videoUrl);
}

#endif // UTILITY_H
