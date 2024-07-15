#include <QTreeWidgetItem>
#include <QMessageBox>

#include "queueitem.h"

QString QueueItem::GetOutputName() const
{
    if (keyword == "")
    {
        return VideoName;
    }
    else
    {
        return keyword + VideoName;
    }
}

void QueueItem::UpdateSkip(QTreeWidgetItem* item, int column)
{
    if (item == TreeItem && column == 0)
    {
        Skip = TreeItem->checkState(0);
    }
}
