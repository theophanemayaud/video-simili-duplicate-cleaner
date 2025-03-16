#ifndef VIDEO_H
#define VIDEO_H

#include <QDebug>               //generic includes go here as video.h is used by many files
#include <QProcess>
#include <QBuffer>
#include <QTemporaryDir>
#include <QPainter>
#include <QReadWriteLock>

#include "opencv2/imgproc.hpp"
#include "ffmpeg.h"

#include "prefs.h"
#include "db.h"
#include "videometadata.h"
#include <QtCore/qmutex.h>

class Db;

class Video : public QObject
{
    Q_OBJECT

public:
    struct ProcessingResult {
        bool success;
        QString errorMsg; // Empty if success is true
        Video *video;
    };

public:
    Video(const Prefs &prefsParam, const QString &filenameParam);
    ProcessingResult process();

    uint getProgress(){
        QMutexLocker locker(&progressLock);
        return progress;
    };

    VideoMetadata meta;
    QString _filePathName;
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

    static VideoMetadata videoToMetadata(const Video& vid);

    // returns empty image if ffmpegLib_captureAt failed and returned empty image
    QImage ffmpegLib_captureAt(const int percent, const int ofDuration=100);   // new methods for capture of image, using ffmpeg library

private:
    const QString getMetadata(const QString &filename); // returns error message or empty string if success
    const QString takeScreenCaptures(const Db &cache);
    QString internalProcess();
    void processThumbnail(QImage &thumbnail, const int &hashes);
    uint64_t computePhash(const cv::Mat &input) const;
    QImage minimizeImage(const QImage &image) const;
    QString msToHHMMSS(const int64_t &time) const;
    QImage getQImageFromFrame(const ffmpeg::AVFrame* pFrame) const;

    uint progress = 1; // to detect scenarios where ffmpeg gets stuck we update progress from time to time
    bool shouldStop = false;
    QMutex progressLock; // lock write when updating progress or terminating thread

private:
    int _rotateAngle=0;

    static Prefs _prefs;
    static int _jpegQuality;

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

#endif // VIDEO_H
