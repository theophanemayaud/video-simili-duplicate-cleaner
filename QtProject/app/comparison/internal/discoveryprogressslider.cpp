#include "discoveryprogressslider.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QStylePainter>

void DiscoveryProgressSlider::setDiscoveredValue(int value)
{
    const int boundedValue = qBound(0, value, maximum());
    if (_discoveredValue == boundedValue)
        return;

    _discoveredValue = boundedValue;
    update();
}

void DiscoveryProgressSlider::paintEvent(QPaintEvent* event)
{
    QSlider::paintEvent(event);
    if (_discoveredValue < minimum() || maximum() <= minimum())
        return;

    QStyleOptionSlider option;
    initStyleOption(&option);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
    const int availableWidth = qMax(0, groove.width() - handle.width());
    const int safeOffset =
        QStyle::sliderPositionFromValue(minimum(), maximum(), _discoveredValue, availableWidth, option.upsideDown);
    const int safeRight = groove.left() + handle.width() / 2 + safeOffset;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRect safeGroove(groove.left(), groove.center().y() - 2, qMax(1, safeRight - groove.left()), 4);
    painter.fillRect(safeGroove, QColor(44, 160, 70, 180));

    // Keep the handle visually above the discovery overlay.
    QStylePainter handlePainter(this);
    option.subControls = QStyle::SC_SliderHandle;
    handlePainter.drawComplexControl(QStyle::CC_Slider, option);
}
