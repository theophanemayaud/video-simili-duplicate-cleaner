#ifndef VIDEO_SIMPLIFIED_TEST_HELPERS_H
#define VIDEO_SIMPLIFIED_TEST_HELPERS_H

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QByteArray>
#include "opencv2/core.hpp"
#include "../../app/prefs.h"
#include "../helpers.h"

class SimplifiedTestHelpers {
public:
    // Scan a video and return all its metadata and thumbnail
    static VideoParam scanVideoMetadata(const QString& videoPath, Prefs& prefs);
    
    // Save single video metadata to text file
    static bool saveMetadataToFile(const VideoParam& param, const QString& filePath, const QDir& videoBaseDir);
    
    // Load metadata from text file
    static void loadMetadataFromFile(const QString& filePath, VideoParam& param);
    
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

