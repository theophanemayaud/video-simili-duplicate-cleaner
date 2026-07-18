#ifndef DISCOVERYPROGRESSSLIDER_H
#define DISCOVERYPROGRESSSLIDER_H

#include <QSlider>

class DiscoveryProgressSlider : public QSlider
{
    Q_OBJECT

  public:
    using QSlider::QSlider;

    void setDiscoveredValue(int value);
    int discoveredValue() const { return _discoveredValue; }

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    int _discoveredValue = 0;
};

#endif // DISCOVERYPROGRESSSLIDER_H
