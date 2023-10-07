#include <QString>
#include <QMediaPlayer>
#include <QMessageBox>

#include "utility.h"

namespace Utility
{
    QString GetTimeStringFromMilli(quint64 ms)
    {
        const quint64 milliseconds = ms % 1000;
        const quint64 seconds = ms / 1000 % 60;
        const quint64 minutes = ms / 1000 / 60 % 60;
        const quint64 hours = ms / 1000 / 60 / 60 % 60;

        return QString("%1:%2:%3.%4")
            .arg(QString::number(hours), 2, u'0')
            .arg(QString::number(minutes), 2, u'0')
            .arg(QString::number(seconds), 2, u'0')
            .arg(QString::number(milliseconds), 3, u'0');
    }

    quint64 GetVideoLength(QUrl videoUrl)
    {
        std::unique_ptr<QMediaPlayer> mediaPlayer = std::make_unique<QMediaPlayer>();
        mediaPlayer->setSource(videoUrl);

        // Get duration
        const quint64 duration = mediaPlayer->duration();

        return duration;
    }
}
