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
    QImage img = viewer->grabMagnifiedRegion(QPoint(x, y), size(), zoomLevel);
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
    switch (event->modifiers()) {
    // size
    case Qt::NoModifier:
        if (event->angleDelta().y() < 0)
            sizeUp();
        else
            sizeDown();
        break;
    // size height
    case Qt::ControlModifier:
        if (event->angleDelta().y() < 0)
            heightUp();
        else
            heightDown();
        break;
    // size width
    case Qt::AltModifier: // alt modifier can actually modify the behavior of the event delta, so let's check both x & y
        if (event->angleDelta().y() < 0 || event->angleDelta().x() < 0)
            widthUp();
        else
            widthDown();
        break;
    // zoom level
    case Qt::ShiftModifier:
        if (event->angleDelta().y() < 0)
            zoomIn();
        else
            zoomOut();
        break;
    default:
        break; // Never propagate a wheel event to the parent widget, even if we ignore it.
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
