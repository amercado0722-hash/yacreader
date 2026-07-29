#include "magnifying_glass.h"

#include "viewer.h"

#include <QPainter>
#include <QPainterPath>

MagnifyingGlass::MagnifyingGlass(int w, int h, float zoomLevel, bool circular, bool ring, QWidget *parent)
    : QLabel(parent), zoomLevel(zoomLevel), circular(circular), ring(ring)
{
    setup(QSize(w, h));
}

MagnifyingGlass::MagnifyingGlass(const QSize &size, float zoomLevel, bool circular, bool ring, QWidget *parent)
    : QLabel(parent), zoomLevel(zoomLevel), circular(circular), ring(ring)
{
    setup(size);
}

void MagnifyingGlass::setup(const QSize &size)
{
    logicalSize = size;
    resize(displaySize());
    setScaledContents(true);
    setMouseTracking(true);
    setCursor(QCursor(QBitmap(1, 1), QBitmap(1, 1)));
    applyShape();
}

QSize MagnifyingGlass::displaySize() const
{
    if (circular) {
        const int side = qMax(logicalSize.width(), logicalSize.height());
        return QSize(side, side);
    }
    return logicalSize;
}

void MagnifyingGlass::applyShape()
{
    if (circular)
        setMask(QRegion(rect(), QRegion::Ellipse));
    else
        clearMask();
}

void MagnifyingGlass::setCircular(bool circular)
{
    if (this->circular == circular)
        return;
    this->circular = circular;
    // Only the display geometry and mask change; logicalSize (and thus the saved
    // MAG_GLASS_SIZE) must not be touched, so do not emit sizeChanged here.
    resize(displaySize());
    applyShape();
    updateImage();
}

void MagnifyingGlass::setRing(bool ring)
{
    if (this->ring == ring)
        return;
    this->ring = ring;
    if (circular)
        update(); // ring only affects the circular rendering; repaint, no geometry change
}

void MagnifyingGlass::paintEvent(QPaintEvent *event)
{
    if (!circular) {
        QLabel::paintEvent(event);
        return;
    }

    const QPixmap pm = pixmap();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF fullRect(rect());

    if (!ring) {
        QPainterPath clip;
        clip.addEllipse(fullRect);
        painter.setClipPath(clip);
        if (!pm.isNull())
            painter.drawPixmap(rect(), pm); // mirrors setScaledContents: scale to fill
        return;
    }

    // Circular + ring. The widget mask (setMask) is a hard-edged ellipse, so anything
    // drawn out to the widget boundary keeps that aliased silhouette. Instead, inset the
    // whole loupe a couple of pixels inside the mask and let the bezel's own antialiased
    // outer edge be the silhouette: the thin margin between bezel and mask stays unpainted
    // (transparent) so the page shows through and the antialiased edge blends into it.
    const qreal bezelWidth = qMax(2.0, width() / 80.0);
    const qreal outerInset = 1.5; // transparent margin left for the antialiased blend
    const QRectF outerRect = fullRect.adjusted(outerInset, outerInset, -outerInset, -outerInset);
    const QRectF innerRect = outerRect.adjusted(bezelWidth, bezelWidth, -bezelWidth, -bezelWidth);

    // Content clipped to just past the bezel's inner edge, so the content's own (hard)
    // clip edge is hidden underneath the opaque part of the bezel.
    QPainterPath contentClip;
    contentClip.addEllipse(innerRect.adjusted(-0.5, -0.5, 0.5, 0.5));
    painter.setClipPath(contentClip);
    if (!pm.isNull())
        painter.drawPixmap(rect(), pm);
    painter.setClipping(false);

    // Bezel as a filled annulus so both edges are antialiased: the inner edge blends onto
    // the content, the outer edge blends onto the page.
    QPainterPath bezel;
    bezel.setFillRule(Qt::OddEvenFill);
    bezel.addEllipse(outerRect);
    bezel.addEllipse(innerRect);
    painter.fillPath(bezel, QColor(30, 30, 30));
}

void MagnifyingGlass::mouseMoveEvent(QMouseEvent *event)
{
    updateImage();
    event->accept();
}

void MagnifyingGlass::updateImage(int x, int y)
{
    auto *const viewer = qobject_cast<const Viewer *>(parentWidget());
    // The loupe widget follows the cursor (and may overhang the window edge, as before). Its
    // *content* is sampled at the eased center, so the zoomed image swims a little toward the
    // edges within the loupe — bounded by the loupe's own half-size so the cursor's point
    // never leaves the view.
    const QPoint sampleCenter = viewer->easeViewerPos(QPoint(x, y), size(), circular);
    QImage img = viewer->grabMagnifiedRegion(sampleCenter, size(), zoomLevel);
    setPixmap(QPixmap::fromImage(img));
    move(static_cast<int>(x - float(width()) / 2), static_cast<int>(y - float(height()) / 2));
}

void MagnifyingGlass::updateImage()
{
    if (isVisible()) {
        QPoint p = QPoint(cursor().pos().x(), cursor().pos().y());
        p = this->parentWidget()->mapFromGlobal(p);
        updateImage(p.x(), p.y());
    }
}
void MagnifyingGlass::wheelEvent(QWheelEvent *event)
{
    // One notch of a real mouse wheel is 120 angle-delta units in a single event, so this
    // threshold makes a mouse still step once per notch while a trackpad's tiny events must
    // sum to 120 before stepping — the "intent" that stops a faint brush from resizing.
    static constexpr int scrollStepThreshold = 120;
    // Drop a partial accumulation that has gone stale, so an old half-finished gesture can't
    // leak into an unrelated later one.
    static constexpr qint64 scrollResetMs = 400;

    const Qt::KeyboardModifiers modifiers = event->modifiers();

    // The active gesture reads a single signed axis. Alt (width) can swap the delta onto the
    // x axis, so for it take whichever axis carries the larger movement.
    int delta = 0;
    if (modifiers == Qt::AltModifier) {
        const int dy = event->angleDelta().y();
        const int dx = event->angleDelta().x();
        delta = (qAbs(dx) > qAbs(dy)) ? dx : dy;
    } else {
        delta = event->angleDelta().y();
    }

    // Only the four handled gestures accumulate; anything else is swallowed (never propagated
    // to the parent) without touching the accumulator.
    const bool handled = modifiers == Qt::NoModifier || modifiers == Qt::ControlModifier || modifiers == Qt::AltModifier || modifiers == Qt::ShiftModifier;
    if (!handled || delta == 0) {
        event->setAccepted(true);
        return;
    }

    // Reset the running total when the gesture changes (different modifier) or when too much
    // time has passed since the last wheel event of this gesture.
    if (modifiers != lastScrollModifiers || !scrollTimer.isValid() || scrollTimer.elapsed() > scrollResetMs)
        scrollAccumulator = 0;
    lastScrollModifiers = modifiers;
    scrollTimer.restart();

    scrollAccumulator += delta;

    // A fast, high-magnitude event may cross the threshold several times over; step once per
    // crossing and keep the remainder so accumulation stays smooth.
    while (qAbs(scrollAccumulator) >= scrollStepThreshold) {
        const bool up = scrollAccumulator < 0; // convention: negative delta grows the loupe
        switch (modifiers) {
        case Qt::NoModifier:
            up ? sizeUp() : sizeDown();
            break;
        case Qt::ControlModifier:
            up ? heightUp() : heightDown();
            break;
        case Qt::AltModifier:
            up ? widthUp() : widthDown();
            break;
        case Qt::ShiftModifier:
            up ? zoomIn() : zoomOut();
            break;
        default:
            break;
        }
        scrollAccumulator -= up ? -scrollStepThreshold : scrollStepThreshold;
    }

    event->setAccepted(true);
}
void MagnifyingGlass::zoomIn()
{
    if (zoomLevel > 0.2f) {
        zoomLevel -= 0.025f;
        emit zoomChanged(zoomLevel);
        updateImage();
    }
}

void MagnifyingGlass::zoomOut()
{
    if (zoomLevel < 0.9f) {
        zoomLevel += 0.025f;
        emit zoomChanged(zoomLevel);
        updateImage();
    }
}

void MagnifyingGlass::sizeUp()
{
    auto w = logicalSize.width();
    auto h = logicalSize.height();
    if (growWidth(w) | growHeight(h)) // bitwise OR prevents short-circuiting
        resizeAndUpdate(w, h);
}

void MagnifyingGlass::sizeDown()
{
    auto w = logicalSize.width();
    auto h = logicalSize.height();
    if (shrinkWidth(w) | shrinkHeight(h)) // bitwise OR prevents short-circuiting
        resizeAndUpdate(w, h);
}

void MagnifyingGlass::heightUp()
{
    auto h = logicalSize.height();
    if (growHeight(h))
        resizeAndUpdate(logicalSize.width(), h);
}

void MagnifyingGlass::heightDown()
{
    auto h = logicalSize.height();
    if (shrinkHeight(h))
        resizeAndUpdate(logicalSize.width(), h);
}

void MagnifyingGlass::widthUp()
{
    auto w = logicalSize.width();
    if (growWidth(w))
        resizeAndUpdate(w, logicalSize.height());
}

void MagnifyingGlass::widthDown()
{
    auto w = logicalSize.width();
    if (shrinkWidth(w))
        resizeAndUpdate(w, logicalSize.height());
}

void MagnifyingGlass::reset()
{
    zoomLevel = 0.5f;
    emit zoomChanged(zoomLevel);
    resizeAndUpdate(350, 175);
}

void MagnifyingGlass::resizeAndUpdate(int w, int h)
{
    logicalSize = QSize(w, h);
    resize(displaySize());
    applyShape();
    emit sizeChanged(logicalSize); // persist the rectangle, never the circular square
    updateImage();
}

static constexpr auto maxRelativeDimension = 0.9;
static constexpr auto widthStep = 30;
static constexpr auto heightStep = 15;

bool MagnifyingGlass::growWidth(int &w) const
{
    const auto maxWidth = parentWidget()->width() * maxRelativeDimension;
    if (w >= maxWidth)
        return false;
    w += widthStep;
    return true;
}

bool MagnifyingGlass::shrinkWidth(int &w) const
{
    constexpr auto minWidth = 175;
    if (w <= minWidth)
        return false;
    w -= widthStep;
    return true;
}

bool MagnifyingGlass::growHeight(int &h) const
{
    const auto maxHeight = parentWidget()->height() * maxRelativeDimension;
    if (h >= maxHeight)
        return false;
    h += heightStep;
    return true;
}

bool MagnifyingGlass::shrinkHeight(int &h) const
{
    constexpr auto minHeight = 80;
    if (h <= minHeight)
        return false;
    h -= heightStep;
    return true;
}
