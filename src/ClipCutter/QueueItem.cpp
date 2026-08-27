#include <QTreeWidgetItem>
#include "QueueItem.h"
#include "ClipLogic.h"

QString QueueItem::GetOutputName() const
{
    return ClipLogic::GetOutputName(VideoName, keyword);
}

void QueueItem::UpdateSkip(QTreeWidgetItem* item, int column)
{
    if (item == TreeItem && column == 0)
    {
        Skip = TreeItem->checkState(0) == Qt::CheckState::Checked;
    }
}
