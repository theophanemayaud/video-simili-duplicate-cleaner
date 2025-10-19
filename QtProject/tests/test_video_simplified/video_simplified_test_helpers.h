#ifndef VIDEO_SIMPLIFIED_TEST_HELPERS_H
#define VIDEO_SIMPLIFIED_TEST_HELPERS_H

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QByteArray>
#include "opencv2/core.hpp"
#include "../../app/prefs.h"

// Reuse VideoParam struct from test_video helpers
class VideoParam {
public:
    static const int nb_params = 13;
    static const char sep = '"';
    static const QString timeformat;

    QFileInfo videoInfo;
    QFileInfo thumbnailInfo;
    int64_t size;
    QDateTime modified;
    int64_t duration;
    int bitrate;
    double framerate;
    QString codec;
    QString audio;
    short width;
    short height;
    uint64_t hash1;
    uint64_t hash2;
    QByteArray thumbnail; // Added for convenience
};

class SimplifiedTestHelpers {
public:
    // Scan a video and return all its metadata and thumbnail
    static VideoParam scanVideoMetadata(const QString& videoPath, Prefs& prefs);
    
    // Save single video metadata to text file
    static bool saveMetadataToFile(const VideoParam& param, const QString& filePath, const QDir& videoBaseDir);
    
    // Load metadata from text file
    static VideoParam loadMetadataFromFile(const QString& filePath, const QDir& videoBaseDir, const QDir& thumbDir);
    
    // Save thumbnail to file
    static bool saveThumbnail(const QByteArray& thumbnail, const QString& path);
    
    // Load thumbnail from file
    static QByteArray loadThumbnailFromFile(const QString& path);
    
    // Compare two thumbnails using SSIM, returns score 0.0-1.0 (1.0 = identical)
    static double compareThumbnails(const QByteArray& thumb1, const QByteArray& thumb2);
    
    // Compare metadata between reference and current, returns true if match
    // errorMsg will contain details if comparison fails
    static bool compareMetadata(const VideoParam& ref, const VideoParam& current, QString& errorMsg);

private:
    // Convert QByteArray thumbnail to grayscale OpenCV Mat
    static cv::Mat thumbnailToGrayMat(const QByteArray& thumbnail);
};

#endif // VIDEO_SIMPLIFIED_TEST_HELPERS_H

