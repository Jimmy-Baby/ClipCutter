#include <QTreeWidgetItem>
#include <QMessageBox>

#include "queueitem.h"

void QueueItem::UpdateSkip(QTreeWidgetItem* item, int column)
{
    if (item == TreeItem && column == 0)
    {
        Skip = TreeItem->checkState(0);
    }
}
