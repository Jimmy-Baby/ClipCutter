#ifndef CLIPCUTTER_APP_TIMELINE_TIMELINEWIDGET_H
#define CLIPCUTTER_APP_TIMELINE_TIMELINEWIDGET_H

#include "App/Timeline/ThumbnailProvider.h"
#include "Core/Timeline/TimelineMath.h"

#include <QMap>
#include <QWidget>

class QScrollBar;
class QTimer;

namespace ClipCutter
{
class TimelineWidget final : public QWidget
{
    Q_OBJECT

public:
    enum class EHandle
    {
        None,
        Playhead,
        InMarker,
        OutMarker
    };
    Q_ENUM(EHandle)

    explicit TimelineWidget(QWidget* parent = nullptr);

    void SetDuration(std::optional<std::chrono::milliseconds> duration);
    void SetPlayhead(std::chrono::milliseconds position);
    void SetActiveRange(std::optional<TimeRange> range);
    void SetOtherSegmentRanges(QVector<TimeRange> ranges);
    void SetSourcePath(QString sourcePath);
    void SetThumbnailProvider(ThumbnailProvider* provider);
    void SetThumbnailsEnabled(bool enabled);
    bool ThumbnailsEnabled() const noexcept;

    std::optional<std::chrono::milliseconds> Duration() const noexcept;
    std::chrono::milliseconds Playhead() const noexcept;
    std::optional<TimeRange> ActiveRange() const;
    const TimelineCoordinateMapper& CoordinateMapper() const noexcept;
    double ZoomFactor() const noexcept;
    void SetZoomFactor(double factor);

public slots:
    void ZoomIn();
    void ZoomOut();
    void ZoomToSelection();
    void ZoomToFull();
    void ScrollToTime(std::chrono::milliseconds start);
    void JumpToIn();
    void JumpToOut();

signals:
    void SeekRequested(qint64 positionMs);
    void PlayheadEdited(qint64 positionMs);
    void RangeEditPreview(qint64 startMs, qint64 endMs);
    void RangeEditCommitted(qint64 oldStartMs, qint64 oldEndMs, qint64 newStartMs, qint64 newEndMs,
                            ClipCutter::TimelineWidget::EHandle handle);
    void ViewportChanged(qint64 visibleStartMs, qint64 visibleEndMs, double zoomFactor);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF TimelineRect() const;
    QRectF ThumbnailRect() const;
    EHandle HandleAt(const QPointF& position) const;
    std::chrono::milliseconds TimeAt(double x) const;
    void ApplyDrag(std::chrono::milliseconds time);
    void UpdateScrollBar();
    void ScheduleThumbnails();
    void RequestThumbnails();
    void EmitViewport();
    QString TimeText(std::chrono::milliseconds value) const;

    TimelineCoordinateMapper Mapper_;
    std::chrono::milliseconds Playhead_{0};
    std::optional<TimeRange> ActiveRange_;
    QVector<TimeRange> OtherRanges_;
    EHandle DragHandle_ = EHandle::None;
    std::optional<TimeRange> DragOriginalRange_;
    QString SourcePath_;
    ThumbnailProvider* ThumbnailProvider_ = nullptr;
    quint64 ThumbnailGeneration_ = 0;
    QMap<qint64, QImage> Thumbnails_;
    bool ThumbnailsEnabled_ = true;
    QScrollBar* ScrollBar_;
    QTimer* ThumbnailTimer_;
};
} // namespace ClipCutter

Q_DECLARE_METATYPE(ClipCutter::TimelineWidget::EHandle)

#endif
