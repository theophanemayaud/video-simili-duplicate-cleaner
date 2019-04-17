#ifndef COMPARISON_H
#define COMPARISON_H

#include <QDialog>
#include <QDesktopServices>
#include <QUrl>
#include "video.h"
#include "prefs.h"

using namespace cv;

namespace Ui {
class Comparison;
}

class Comparison : public QDialog
{
    Q_OBJECT

public:
    explicit Comparison(QVector<Video *> &userVideos, Prefs &userPrefs, QWidget &parent);
    ~Comparison();
    QVector<Video *> _videos;
    Prefs _prefs;
    QWidget *_mainWindow = nullptr;
    int _leftVideo = 0;
    int _rightVideo = 0;
    uint _videosDeleted = 0;
    qint64 _spaceSaved = 0;
    bool _seekForwards = true;

    int _zoomLevel = 0;
    QPixmap _leftZoomed;
    int _leftW = 0;
    int _leftH = 0;
    QPixmap _rightZoomed;
    int _rightW = 0;
    int _rightH = 0;

private slots:
    void on_prevVideo_clicked();
    void on_nextVideo_clicked();
    bool bothVideosMatch(const int &left, const int &right) const;
    short phashSimilarity(const int &left, const int &right) const;

    void showVideo(const QString &side) const;
    QString readableDuration(const qint64 &milliseconds) const;
    QString readableFileSize(const qint64 &filesize) const;
    QString readableBitRate(const double &kbps) const;
    void highlightBetterProperties() const;
    void updateUI();
    void updateProgressbar(bool alsoReport = false) const;

    void on_selectPhash_clicked(const bool &checked) { if(checked) _prefs._ComparisonMode = _prefs._PHASH; }
    void on_selectSSIM_clicked(const bool &checked) { if(checked) _prefs._ComparisonMode = _prefs._SSIM; }
    void on_leftImage_clicked() { QDesktopServices::openUrl(QUrl::fromLocalFile(_videos[_leftVideo]->filename)); }
    void on_rightImage_clicked() { QDesktopServices::openUrl(QUrl::fromLocalFile(_videos[_rightVideo]->filename)); }
    void on_leftFileName_clicked() const;
    void on_rightFileName_clicked() const;
    void on_leftDelete_clicked();
    void on_rightDelete_clicked();
    void on_leftMove_clicked();
    void on_rightMove_clicked();
    void on_swapFilenames_clicked();

    void on_thresholdSlider_valueChanged(const int &value);
    void resizeEvent(QResizeEvent *event);
    void wheelEvent(QWheelEvent *event);

    double sigma(const Mat &m, const int &i, const int &j, const int &block_size) const;
    double covariance(const Mat &m0, const Mat &m1, const int &i, const int &j, const int &block_size) const;
    double ssim(const Mat &m0, const Mat &m1, const int &block_size) const;

signals:
    void sendStatusMessage(const QString) const;

private:
    Ui::Comparison *ui;
};

#endif // COMPARISON_H