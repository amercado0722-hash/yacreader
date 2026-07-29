#ifndef __MAGNIFYING_GLASS
#define __MAGNIFYING_GLASS

#include <QElapsedTimer>
#include <QLabel>
#include <QMouseEvent>
#include <QSize>
#include <QWidget>

class MagnifyingGlass : public QLabel
{
    Q_OBJECT
private:
    float zoomLevel;
    // The rectangle the user configures via the size gestures. This is the source of
    // truth for sizing and the only value ever persisted to MAG_GLASS_SIZE. The widget's
    // actual geometry (see displaySize()) may differ from this in circular mode.
    QSize logicalSize;
    // When true the loupe is rendered as a circle whose diameter is the wider of the two
    // logicalSize dimensions. The widget grows to a square for display, but logicalSize
    // (and therefore the saved setting) is left untouched.
    bool circular;
    // When true (and circular), a bezel ring is drawn along the circle boundary to hide
    // the aliased edge left by the circular mask. Has no effect in rectangular mode.
    bool ring;

    // Wheel/scroll accumulation for the resize & zoom gestures. Rather than stepping on the
    // mere sign of each wheel event (which makes a trackpad's many tiny high-resolution
    // events each fire a full step), we sum the signed angle delta of the active gesture and
    // only take a step once it crosses scrollStepThreshold. A real mouse wheel delivers 120
    // units per notch in a single event, so it still steps once per notch; a light trackpad
    // brush no longer does anything.
    int scrollAccumulator = 0;
    Qt::KeyboardModifiers lastScrollModifiers = Qt::NoModifier;
    QElapsedTimer scrollTimer;

    void setup(const QSize &size);
    void resizeAndUpdate(int w, int h);
    // The widget geometry to use for the current mode: a max(w, h) square when circular,
    // otherwise the logical rectangle.
    QSize displaySize() const;
    // Masks the widget to a circle (or clears the mask) to match the current mode.
    void applyShape();

    // The following 4 functions increase/decrease their argument and return true,
    // unless the maximum dimension value has been reached, in which case they
    // do not modify the argument and return false.
    bool growWidth(int &w) const;
    bool shrinkWidth(int &w) const;
    bool growHeight(int &h) const;
    bool shrinkHeight(int &h) const;

public:
    MagnifyingGlass(int width, int height, float zoomLevel, bool circular, bool ring, QWidget *parent);
    MagnifyingGlass(const QSize &size, float zoomLevel, bool circular, bool ring, QWidget *parent);
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
public slots:
    void updateImage(int x, int y);
    void updateImage();
    void wheelEvent(QWheelEvent *event) override;
    void zoomIn();
    void zoomOut();
    void sizeUp();
    void sizeDown();
    void heightUp();
    void heightDown();
    void widthUp();
    void widthDown();
    void setCircular(bool circular);
    void setRing(bool ring);
    void reset();

signals:
    void sizeChanged(QSize newSize);
    void zoomChanged(float newZoomLevel);
};

#endif
