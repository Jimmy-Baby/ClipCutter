#ifndef CLIPCUTTER_APP_MODELS_CLIPQUEUEFILTERMODEL_H
#define CLIPCUTTER_APP_MODELS_CLIPQUEUEFILTERMODEL_H

#include "Core/Export/ExportState.h"

#include <QSortFilterProxyModel>

#include <optional>

namespace ClipCutter
{
class ClipQueueFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ClipQueueFilterModel(QObject* parent = nullptr);

    void SetSearchText(QString text);
    void SetExportState(std::optional<EExportState> state);
    void SetSkipped(std::optional<bool> skipped);
    void ClearFilters();
    bool HasActiveFilter() const noexcept;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    void RefreshFilter();
    QString SearchText_;
    std::optional<EExportState> ExportState_;
    std::optional<bool> Skipped_;
};
} // namespace ClipCutter

#endif
