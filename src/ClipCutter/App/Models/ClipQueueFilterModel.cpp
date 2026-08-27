#include "App/Models/ClipQueueFilterModel.h"

#include "App/Models/ClipQueueModel.h"
#include "Core/Export/ExportState.h"

namespace ClipCutter
{
ClipQueueFilterModel::ClipQueueFilterModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void ClipQueueFilterModel::SetSearchText(QString text)
{
    text = text.trimmed();
    if (SearchText_ == text) return;
    SearchText_ = std::move(text);
    RefreshFilter();
}

void ClipQueueFilterModel::SetExportState(const std::optional<EExportState> state)
{
    ExportState_ = state;
    RefreshFilter();
}

void ClipQueueFilterModel::SetSkipped(const std::optional<bool> skipped)
{
    Skipped_ = skipped;
    RefreshFilter();
}

void ClipQueueFilterModel::ClearFilters()
{
    SearchText_.clear();
    ExportState_.reset();
    Skipped_.reset();
    RefreshFilter();
}

bool ClipQueueFilterModel::HasActiveFilter() const noexcept
{
    return !SearchText_.isEmpty() || ExportState_.has_value() || Skipped_.has_value();
}

void ClipQueueFilterModel::RefreshFilter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
    endFilterChange(Direction::Rows);
#else
    invalidateRowsFilter();
#endif
}

bool ClipQueueFilterModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QAbstractItemModel* model = sourceModel();
    if (model == nullptr) return false;
    const QModelIndex index = model->index(sourceRow, 0, sourceParent);
    if (Skipped_.has_value() && index.data(ClipQueueModel::SkippedRole).toBool() != *Skipped_) return false;
    if (ExportState_.has_value() && index.data(ClipQueueModel::ExportStateRole).value<EExportState>() != *ExportState_)
        return false;
    if (SearchText_.isEmpty()) return true;
    const QStringList fields{
        model->index(sourceRow, ClipQueueModel::SourceNameColumn, sourceParent).data().toString(),
        model->index(sourceRow, ClipQueueModel::OutputNameColumn, sourceParent).data().toString(),
        index.data(ClipQueueModel::PrefixRole).toString(),
        ExportStateText(index.data(ClipQueueModel::ExportStateRole).value<EExportState>()),
        index.data(ClipQueueModel::SkippedRole).toBool() ? QStringLiteral("skip skipped") : QStringLiteral("keep kept")};
    for (const QString& field : fields)
        if (field.contains(SearchText_, Qt::CaseInsensitive)) return true;
    return false;
}
} // namespace ClipCutter
