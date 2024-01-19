#ifndef QUEUEITEM_H
#define QUEUEITEM_H

#include <QObject>

class QTreeWidgetItem;

enum EReEncodeQuality
{
    QUALITY_COPY,
    QUALITY_LOW,
    QUALITY_MEDIUM,
    QUALITY_HIGH
};

enum EReEncodeSpeed
{
    ENCODE_FAST,
    ENCODE_MEDIUM,
    ENCODE_SLOW
};

struct QueueItem : public QObject
{
    quint64 ListIndex;
    QTreeWidgetItem* TreeItem = nullptr;
    bool HasBeenOpened = false;
    bool Skip = false;
    qint64 StartTimeMs = 0;
    qint64 EndTimeMs = 0;
    QString VideoName;
    QString keyword;
    QString OriginalPath;

    QString GetOutputName() const;
    void UpdateSkip(QTreeWidgetItem* item, int column);
};

#endif // QUEUEITEM_H
