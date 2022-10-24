#ifndef VIDEO_H
#define VIDEO_H

#include <QDebug>               //generic includes go here as video.h is used by many files
#include <QRunnable>
#include <QProcess>
#include <QBuffer>
#include <QTemporaryDir>
#include <QPainter>

#include "opencv2/imgproc.hpp"

#include "prefs.h"
#include "db.h"
#include "ffmpeg.h"

class Db;

class Video : public QObject, public QRunnable
{
    Q_OBJECT

public:
    enum USE_CACHE_OPTION : int {
        NO_CACHE,
        WITH_CACHE,
        CACHE_ONLY
    };

    Video(const Prefs &prefsParam, const QString &filenameParam, const USE_CACHE_OPTION cacheOption=WITH_CACHE);
    void run();

    QString filename;
    QString nameInApplePhotos; // used externally only, as it is too slow to get at first
    int64_t size = 0; // in bytes
    QDateTime modified;
    QDateTime _fileCreateDate;
    int64_t duration = 0; // in miliseconds
    int bitrate = 0; // in kb/s
    double framerate = 0; // avg, in frames per second
    QString codec;
    QString audio;
    short width = 0;
    short height = 0;
    QByteArray thumbnail;
    cv::Mat grayThumb [2];
    uint64_t hash [2] = { 0, 0 };
    bool trashed = false;

private slots:
    bool getMetadata(const QString &filename); // return success=true or error=false. It handles video rejection on those with error
    int takeScreenCaptures(const Db &cache);
    void processThumbnail(QImage &thumbnail, const int &hashes);
    uint64_t computePhash(const cv::Mat &input) const;
    QImage minimizeImage(const QImage &image) const;
    QString msToHHMMSS(const int64_t &time) const;
    QImage getQImageFromFrame(const ffmpeg::AVFrame* pFrame) const;

public slots:
    // returns empty image if ffmpegLib_captureAt failed and returned empty image
    QImage ffmpegLib_captureAt(const int percent, const int ofDuration=100);   // new methods for capture of image, using ffmpeg library

signals:
    void acceptVideo(Video *addMe) const;
    void rejectVideo(Video *deleteMe, QString errorMsg) const;

private:
    int _rotateAngle=0;

    USE_CACHE_OPTION _useCacheDb = WITH_CACHE;

    static Prefs _prefs;
    static int _jpegQuality;

    enum _returnValues { _success, _failure };

    static constexpr int _okJpegQuality      = 60;
    static constexpr int _lowJpegQuality     = 25;
    static constexpr int _hugeAmountVideos   = 200000;
    static constexpr int _goBackwardsPercent = 6;       //if capture fails, retry but omit this much from end
    static constexpr int _videoStillUsable   = 90;      //90% of video duration is considered usable
    static constexpr int _thumbnailMaxWidth  = 448;     //small size to save memory and cache space
    static constexpr int _thumbnailMaxHeight = 336;
    static constexpr int _pHashSize          = 32;      //phash generated from 32x32 image
    static constexpr int _ssimSize           = 16;      //larger than 16x16 seems to have slower comparison
    static constexpr int _almostBlackBitmap  = 1500;    //monochrome thumbnail if less shades of gray than this
};

class VideoMetadata: public QObject
{
public:
    VideoMetadata(){};
    VideoMetadata(const Video *);

    QString filename;
    QString nameInApplePhotos; // used externally only, as it is too slow to get at first
    int64_t size = 0; // in bytes
    QDateTime _fileCreateDate;
    QDateTime modified;
    int64_t duration = 0; // in miliseconds
    int bitrate = 0;
    double framerate = 0; // avg, in frames per second
    QString codec;
    QString audio;
    short width = 0;
    short height = 0; 
};
#endif // VIDEO_H
