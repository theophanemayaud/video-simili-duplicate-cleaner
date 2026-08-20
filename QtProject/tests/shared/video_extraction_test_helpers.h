#ifndef VIDEO_EXTRACTION_TEST_HELPERS_H
#define VIDEO_EXTRACTION_TEST_HELPERS_H

#include "../app/prefs.h"
#include "video_params.h"

#include "opencv2/core.hpp"

#include <QByteArray>
#include <QDir>
#include <QString>

class VideoExtractionTestHelpers
{
  public:
    static VideoParam scanVideoMetadata(const QString& videoPath, Prefs& prefs);
    static bool saveMetadataToFile(const VideoParam& param, const QString& filePath, const QDir& videoBaseDir);
    static void loadMetadataFromFile(const QString& filePath, VideoParam& param);
    static bool saveThumbnail(const QByteArray& thumbnail, const QString& path);
    static QByteArray loadThumbnailFromFile(const QString& path);
    static double compareThumbnails(const QByteArray& thumb1, const QByteArray& thumb2);
    static bool compareMetadata(const VideoParam& ref, const VideoParam& current, QString& errorMsg);

  private:
    static cv::Mat thumbnailToGrayMat(const QByteArray& thumbnail);
};

#endif // VIDEO_EXTRACTION_TEST_HELPERS_H
