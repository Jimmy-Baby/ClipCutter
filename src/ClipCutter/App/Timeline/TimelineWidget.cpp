#include "App/Timeline/TimelineWidget.h"

#include "Utility.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyleOption>
#include <QTimer>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace ClipCutter
{
namespace
{
constexpr int KScrollMaximum = 1'000'000;
constexpr double KHandleTolerance = 8.0;
constexpr int KScrollBarHeight = 14;
} // namespace

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent), ScrollBar_(new QScrollBar(Qt::Horizontal, this)), ThumbnailTimer_(new QTimer(this))
{
    setMinimumHeight(112);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    ThumbnailTimer_->setSingleShot(true);
    ThumbnailTimer_->setInterval(120);
    connect(ThumbnailTimer_, &QTimer::timeout, this, &TimelineWidget::RequestThumbnails);
    connect(ScrollBar_, &QScrollBar::valueChanged, this,
            [this](const int value)
            {
                if (!Mapper_.IsUsable()) return;
                const qint64 maximumStart = std::max<qint64>(0, Mapper_.Duration()->count() - Mapper_.VisibleDuration().count());
                const qint64 start = maximumStart == 0 ? 0
                    : static_cast<qint64>(static_cast<long double>(value) * maximumStart / KScrollMaximum);
                Mapper_.ScrollTo(std::chrono::milliseconds{start});
                update();
                EmitViewport();
                ScheduleThumbnails();
            });
}

void TimelineWidget::SetDuration(const std::optional<std::chrono::milliseconds> duration)
{
    Mapper_.SetDuration(duration);
    if (duration.has_value()) Playhead_ = std::clamp(Playhead_, std::chrono::milliseconds{0}, *duration);
    else Playhead_ = std::chrono::milliseconds{0};
    UpdateScrollBar();
    ScheduleThumbnails();
    update();
}

void TimelineWidget::SetPlayhead(const std::chrono::milliseconds position)
{
    Playhead_ = Mapper_.Duration().has_value()
                    ? std::clamp(position, std::chrono::milliseconds{0}, *Mapper_.Duration())
                    : std::max(position, std::chrono::milliseconds{0});
    update();
}

void TimelineWidget::SetActiveRange(std::optional<TimeRange> range)
{
    if (range.has_value() && !range->IsValid(Mapper_.Duration())) range.reset();
    ActiveRange_ = std::move(range);
    update();
}

void TimelineWidget::SetOtherSegmentRanges(QVector<TimeRange> ranges)
{
    OtherRanges_ = std::move(ranges);
    update();
}

void TimelineWidget::SetSourcePath(QString sourcePath)
{
    if (SourcePath_ == sourcePath) return;
    SourcePath_ = std::move(sourcePath);
    Thumbnails_.clear();
    if (ThumbnailProvider_ != nullptr) ThumbnailProvider_->Cancel();
    ScheduleThumbnails();
    update();
}

void TimelineWidget::SetThumbnailProvider(ThumbnailProvider* provider)
{
    if (ThumbnailProvider_ == provider) return;
    if (ThumbnailProvider_ != nullptr) disconnect(ThumbnailProvider_, nullptr, this, nullptr);
    ThumbnailProvider_ = provider;
    Thumbnails_.clear();
    if (ThumbnailProvider_ != nullptr)
    {
        connect(ThumbnailProvider_, &ThumbnailProvider::ThumbnailReady, this,
                [this](const quint64 generation, const qint64 timestamp, const QImage& image)
                {
                    if (generation != ThumbnailGeneration_ || image.isNull()) return;
                    Thumbnails_.insert(timestamp, image);
                    update();
                });
    }
    ScheduleThumbnails();
}

void TimelineWidget::SetThumbnailsEnabled(const bool enabled)
{
    ThumbnailsEnabled_ = enabled;
    if (!enabled && ThumbnailProvider_ != nullptr) ThumbnailProvider_->Cancel();
    ScheduleThumbnails();
    update();
}

bool TimelineWidget::ThumbnailsEnabled() const noexcept { return ThumbnailsEnabled_; }
std::optional<std::chrono::milliseconds> TimelineWidget::Duration() const noexcept { return Mapper_.Duration(); }
std::chrono::milliseconds TimelineWidget::Playhead() const noexcept { return Playhead_; }
std::optional<TimeRange> TimelineWidget::ActiveRange() const { return ActiveRange_; }
const TimelineCoordinateMapper& TimelineWidget::CoordinateMapper() const noexcept { return Mapper_; }
double TimelineWidget::ZoomFactor() const noexcept { return Mapper_.ZoomFactor(); }
void TimelineWidget::SetZoomFactor(const double factor)
{
    Mapper_.SetZoomFactor(factor, Playhead_);
    UpdateScrollBar(); update(); EmitViewport(); ScheduleThumbnails();
}

void TimelineWidget::ZoomIn()
{
    Mapper_.ZoomBy(2.0, Playhead_);
    UpdateScrollBar(); update(); EmitViewport(); ScheduleThumbnails();
}
void TimelineWidget::ZoomOut()
{
    Mapper_.ZoomBy(0.5, Playhead_);
    UpdateScrollBar(); update(); EmitViewport(); ScheduleThumbnails();
}
void TimelineWidget::ZoomToSelection()
{
    if (ActiveRange_.has_value() && Mapper_.ZoomToSelection(*ActiveRange_))
    {
        UpdateScrollBar(); update(); EmitViewport(); ScheduleThumbnails();
    }
}
void TimelineWidget::ZoomToFull()
{
    Mapper_.ZoomToFull(); UpdateScrollBar(); update(); EmitViewport(); ScheduleThumbnails();
}
void TimelineWidget::ScrollToTime(const std::chrono::milliseconds start)
{
    Mapper_.ScrollTo(start); UpdateScrollBar(); update(); EmitViewport(); ScheduleThumbnails();
}
void TimelineWidget::JumpToIn()
{
    if (!ActiveRange_.has_value()) return;
    SetPlayhead(ActiveRange_->Start());
    emit SeekRequested(Playhead_.count());
}
void TimelineWidget::JumpToOut()
{
    if (!ActiveRange_.has_value()) return;
    SetPlayhead(ActiveRange_->End());
    emit SeekRequested(Playhead_.count());
}

QRectF TimelineWidget::TimelineRect() const
{
    const int top = ThumbnailsEnabled_ ? 60 : 8;
    return QRectF(8.0, top, std::max(1, width() - 16), std::max(24, height() - top - KScrollBarHeight - 8));
}

QRectF TimelineWidget::ThumbnailRect() const
{
    return QRectF(8.0, 6.0, std::max(1, width() - 16), 50.0);
}

void TimelineWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().brush(QPalette::Base));
    const QRectF timeline = TimelineRect();
    painter.fillRect(timeline, palette().brush(QPalette::AlternateBase));
    if (!Mapper_.IsUsable())
    {
        painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Duration unavailable — seeking and markers remain disabled"));
        return;
    }

    if (ThumbnailsEnabled_)
    {
        const QRectF thumbnailArea = ThumbnailRect();
        painter.fillRect(thumbnailArea, palette().brush(QPalette::Midlight));
        const int slotCount = std::max(1, static_cast<int>(std::ceil(thumbnailArea.width() / 120.0)));
        const double slotWidth = thumbnailArea.width() / slotCount;
        for (int index = 0; index < slotCount; ++index)
        {
            const QRectF slot(thumbnailArea.left() + index * slotWidth, thumbnailArea.top(), slotWidth, thumbnailArea.height());
            painter.setPen(palette().color(QPalette::Mid));
            painter.drawRect(slot.adjusted(0, 0, -1, -1));
        }
        for (auto it = Thumbnails_.cbegin(); it != Thumbnails_.cend(); ++it)
        {
            const double center = thumbnailArea.left() + Mapper_.TimeToPixel(std::chrono::milliseconds{it.key()});
            const QRectF target(center - slotWidth / 2.0, thumbnailArea.top(), slotWidth, thumbnailArea.height());
            painter.drawImage(target.intersected(thumbnailArea), it.value());
        }
    }

    for (const TimeRange& range : OtherRanges_)
    {
        const double left = timeline.left() + Mapper_.TimeToPixel(range.Start());
        const double right = timeline.left() + Mapper_.TimeToPixel(range.End());
        painter.fillRect(QRectF(left, timeline.top() + 5, right - left, 6), QColor(100, 150, 220, 90));
    }
    if (ActiveRange_.has_value())
    {
        const double left = timeline.left() + Mapper_.TimeToPixel(ActiveRange_->Start());
        const double right = timeline.left() + Mapper_.TimeToPixel(ActiveRange_->End());
        painter.fillRect(QRectF(left, timeline.top(), right - left, timeline.height()), QColor(50, 140, 230, 65));
        painter.setPen(QPen(QColor(40, 180, 90), 2));
        painter.drawLine(QPointF(left, timeline.top()), QPointF(left, timeline.bottom()));
        painter.setPen(QPen(QColor(230, 90, 60), 2));
        painter.drawLine(QPointF(right, timeline.top()), QPointF(right, timeline.bottom()));
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(QRectF(left + 4, timeline.top() + 2, 170, 18),
                         QStringLiteral("IN %1").arg(TimeText(ActiveRange_->Start())));
        painter.drawText(QRectF(std::max(left + 4, right - 174), timeline.bottom() - 20, 170, 18), Qt::AlignRight,
                         QStringLiteral("OUT %1").arg(TimeText(ActiveRange_->End())));
    }
    const double playheadX = timeline.left() + Mapper_.TimeToPixel(Playhead_);
    painter.setPen(QPen(QColor(240, 190, 35), 2));
    painter.drawLine(QPointF(playheadX, timeline.top() - 3), QPointF(playheadX, timeline.bottom()));
    painter.setBrush(QColor(240, 190, 35));
    painter.drawPolygon(QPolygonF{QPointF(playheadX - 5, timeline.top() - 5), QPointF(playheadX + 5, timeline.top() - 5),
                                  QPointF(playheadX, timeline.top() + 2)});

    painter.setPen(palette().color(QPalette::Text));
    const int tickCount = std::clamp(width() / 140, 2, 12);
    for (int index = 0; index <= tickCount; ++index)
    {
        const double x = timeline.left() + timeline.width() * index / tickCount;
        const auto time = Mapper_.PixelToTime(timeline.width() * index / tickCount);
        painter.drawLine(QPointF(x, timeline.bottom() - 5), QPointF(x, timeline.bottom()));
        painter.drawText(QRectF(x - 65, timeline.bottom() - 22, 130, 16), Qt::AlignCenter, TimeText(time));
    }
}

void TimelineWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    ScrollBar_->setGeometry(8, height() - KScrollBarHeight, std::max(1, width() - 16), KScrollBarHeight);
    Mapper_.SetViewportWidth(TimelineRect().width());
    UpdateScrollBar();
    ScheduleThumbnails();
}

TimelineWidget::EHandle TimelineWidget::HandleAt(const QPointF& position) const
{
    const QRectF timeline = TimelineRect();
    if (!timeline.adjusted(-KHandleTolerance, -KHandleTolerance, KHandleTolerance, KHandleTolerance).contains(position))
        return EHandle::None;
    if (ActiveRange_.has_value())
    {
        const double inX = timeline.left() + Mapper_.TimeToPixel(ActiveRange_->Start());
        const double outX = timeline.left() + Mapper_.TimeToPixel(ActiveRange_->End());
        if (std::abs(position.x() - inX) <= KHandleTolerance) return EHandle::InMarker;
        if (std::abs(position.x() - outX) <= KHandleTolerance) return EHandle::OutMarker;
    }
    const double playheadX = timeline.left() + Mapper_.TimeToPixel(Playhead_);
    return std::abs(position.x() - playheadX) <= KHandleTolerance ? EHandle::Playhead : EHandle::None;
}

std::chrono::milliseconds TimelineWidget::TimeAt(const double x) const
{
    return Mapper_.PixelToTime(x - TimelineRect().left());
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !Mapper_.IsUsable()) return;
    DragHandle_ = HandleAt(event->position());
    if (DragHandle_ == EHandle::None) DragHandle_ = EHandle::Playhead;
    DragOriginalRange_ = ActiveRange_;
    ApplyDrag(TimeAt(event->position().x()));
    event->accept();
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (DragHandle_ != EHandle::None) ApplyDrag(TimeAt(event->position().x()));
    const EHandle hover = HandleAt(event->position());
    setCursor(hover == EHandle::InMarker || hover == EHandle::OutMarker ? Qt::SizeHorCursor : Qt::PointingHandCursor);
    const auto time = TimeAt(event->position().x());
    QToolTip::showText(event->globalPosition().toPoint(), TimeText(time), this);
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || DragHandle_ == EHandle::None) return;
    ApplyDrag(TimeAt(event->position().x()));
    if ((DragHandle_ == EHandle::InMarker || DragHandle_ == EHandle::OutMarker) &&
        DragOriginalRange_.has_value() && ActiveRange_.has_value() && *DragOriginalRange_ != *ActiveRange_)
        emit RangeEditCommitted(DragOriginalRange_->Start().count(), DragOriginalRange_->End().count(),
                                ActiveRange_->Start().count(), ActiveRange_->End().count(), DragHandle_);
    DragHandle_ = EHandle::None;
    DragOriginalRange_.reset();
    event->accept();
}

void TimelineWidget::ApplyDrag(const std::chrono::milliseconds time)
{
    if (DragHandle_ == EHandle::Playhead)
    {
        SetPlayhead(time);
        emit PlayheadEdited(Playhead_.count());
        emit SeekRequested(Playhead_.count());
        return;
    }
    if (!ActiveRange_.has_value()) return;
    const auto start = DragHandle_ == EHandle::InMarker ? std::min(time, ActiveRange_->End()) : ActiveRange_->Start();
    const auto end = DragHandle_ == EHandle::OutMarker ? std::max(time, ActiveRange_->Start()) : ActiveRange_->End();
    ActiveRange_ = TimeRange::Create(start, end, Mapper_.Duration()).value_or(*ActiveRange_);
    emit RangeEditPreview(ActiveRange_->Start().count(), ActiveRange_->End().count());
    update();
}

void TimelineWidget::wheelEvent(QWheelEvent* event)
{
    if (!Mapper_.IsUsable()) return;
    const auto anchor = TimeAt(event->position().x());
    if (event->modifiers().testFlag(Qt::ControlModifier))
        Mapper_.ZoomBy(event->angleDelta().y() > 0 ? 1.5 : 1.0 / 1.5, anchor);
    else
    {
        const qint64 step = std::max<qint64>(1, Mapper_.VisibleDuration().count() / 8);
        Mapper_.ScrollBy(std::chrono::milliseconds{event->angleDelta().y() > 0 ? -step : step});
    }
    UpdateScrollBar(); update(); EmitViewport(); ScheduleThumbnails(); event->accept();
}

void TimelineWidget::leaveEvent(QEvent* event)
{
    QToolTip::hideText();
    QWidget::leaveEvent(event);
}

void TimelineWidget::UpdateScrollBar()
{
    ScrollBar_->blockSignals(true);
    ScrollBar_->setRange(0, KScrollMaximum);
    const qint64 maximumStart = Mapper_.IsUsable()
                                    ? std::max<qint64>(0, Mapper_.Duration()->count() - Mapper_.VisibleDuration().count()) : 0;
    const int value = maximumStart == 0 ? 0
        : static_cast<int>(static_cast<long double>(Mapper_.VisibleStart().count()) * KScrollMaximum / maximumStart);
    ScrollBar_->setValue(value);
    ScrollBar_->setPageStep(Mapper_.IsUsable()
                                ? std::max(1, static_cast<int>(KScrollMaximum / Mapper_.ZoomFactor())) : KScrollMaximum);
    ScrollBar_->setEnabled(maximumStart > 0);
    ScrollBar_->blockSignals(false);
}

void TimelineWidget::ScheduleThumbnails()
{
    if (ThumbnailProvider_ == nullptr || !ThumbnailsEnabled_ || SourcePath_.isEmpty() || !Mapper_.IsUsable()) return;
    ThumbnailTimer_->start();
}

void TimelineWidget::RequestThumbnails()
{
    if (ThumbnailProvider_ == nullptr || !ThumbnailsEnabled_ || SourcePath_.isEmpty() || !Mapper_.IsUsable()) return;
    Thumbnails_.clear();
    const auto range = TimeRange::Create(Mapper_.VisibleStart(), Mapper_.VisibleEnd(), Mapper_.Duration(), true);
    if (!range.has_value()) return;
    const QSize size(120, 50);
    const int count = std::clamp(static_cast<int>(std::ceil(ThumbnailRect().width() / size.width())) + 2, 1, 64);
    ThumbnailGeneration_ = ThumbnailProvider_->RequestVisible(SourcePath_, *range, size, count);
    update();
}

void TimelineWidget::EmitViewport()
{
    emit ViewportChanged(Mapper_.VisibleStart().count(), Mapper_.VisibleEnd().count(), Mapper_.ZoomFactor());
}

QString TimelineWidget::TimeText(const std::chrono::milliseconds value) const
{
    return Utility::GetTimeStringFromMilliseconds(value.count());
}
} // namespace ClipCutter
