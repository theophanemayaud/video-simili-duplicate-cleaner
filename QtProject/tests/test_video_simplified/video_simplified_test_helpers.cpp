#include "video_simplified_test_helpers.h"
#include "../../app/video.h"
#include "../../app/db.h"
#include "../../app/comparison.h"
#include <QFile>
#include <QTextStream>
#include <QImage>
#include <QDebug>
#include "opencv2/imgproc.hpp"

using namespace cv;

// ------------------- Public Helper Functions -------------------

VideoParam SimplifiedTestHelpers::scanVideoMetadata(const QString& videoPath, Prefs& prefs)
{
    VideoParam param;
    
    // Initialize database if using cache
    if (prefs.useCacheOption() != Prefs::NO_CACHE) {
        Db::initDbAndCacheLocation(prefs);
    }
    
    // Create and process video
    Video vid(prefs, videoPath);
    auto result = vid.process();
    
    if (!result.success) {
        qWarning() << "Failed to process video:" << videoPath << "Error:" << result.errorMsg;
        return param;
    }
    
    // Fill VideoParam with metadata
    param.videoInfo = QFileInfo(videoPath);
    param.size = vid.size;
    param.modified = vid.modified;
    param.duration = vid.duration;
    param.bitrate = vid.bitrate;
    param.framerate = vid.framerate;
    param.codec = vid.codec;
    param.audio = vid.audio;
    param.width = vid.width;
    param.height = vid.height;
    param.hash1 = vid.hash[0];
    param.hash2 = vid.hash[1];
    param.thumbnail = vid.thumbnail;
    
    return param;
}

bool SimplifiedTestHelpers::saveMetadataToFile(const VideoParam& param, const QString& filePath, const QDir& videoBaseDir)
{
    if (QFileInfo::exists(filePath)) {
        qWarning() << "Metadata file already exists, not overwriting:" << filePath;
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open metadata file for writing:" << filePath;
        return false;
    }
    
    QTextStream output(&file);
    
    // Write key-value pairs separated by newlines
    QString relativeVideoPath = param.videoInfo.absoluteFilePath();
    if (!videoBaseDir.path().isEmpty()) {
        relativeVideoPath = relativeVideoPath.remove(videoBaseDir.path() + "/");
    }
    
    output << "videoFilename:" << relativeVideoPath << "\n";
    output << "thumbnailFilename:" << param.videoInfo.fileName() << "\n";
    output << "size:" << param.size << "\n";
    output << "modified:" << param.modified.toString(VideoParam::timeformat()) << "\n";
    output << "duration:" << param.duration << "\n";
    output << "bitrate:" << param.bitrate << "\n";
    output << "framerate:" << param.framerate << "\n";
    output << "codec:" << param.codec << "\n";
    output << "audio:" << param.audio << "\n";
    output << "width:" << param.width << "\n";
    output << "height:" << param.height << "\n";
    output << "hash1:" << param.hash1 << "\n";
    output << "hash2:" << param.hash2 << "\n";
    
    file.close();
    return true;
}

VideoParam SimplifiedTestHelpers::loadMetadataFromFile(const QString& filePath, const QDir& videoBaseDir, const QDir& thumbDir)
{
    VideoParam param;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open metadata file for reading:" << filePath;
        return param;
    }
    
    QTextStream input(&file);
    QMap<QString, QString> data;
    
    // Read key-value pairs
    while (!input.atEnd()) {
        QString line = input.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        int colonPos = line.indexOf(':');
        if (colonPos == -1) {
            qWarning() << "Invalid line format (no colon):" << line;
            continue;
        }
        
        QString key = line.left(colonPos);
        QString value = line.mid(colonPos + 1);
        data[key] = value;
    }
    
    file.close();
    
    // Parse the data into VideoParam
    if (data.contains("videoFilename")) {
        param.videoInfo = QFileInfo(videoBaseDir.path() + "/" + data["videoFilename"]);
    }
    if (data.contains("thumbnailFilename")) {
        param.thumbnailInfo = QFileInfo(thumbDir.path() + "/" + data["thumbnailFilename"]);
    }
    if (data.contains("size")) {
        param.size = data["size"].toLongLong();
    }
    if (data.contains("modified")) {
        param.modified = QDateTime::fromString(data["modified"], VideoParam::timeformat());
    }
    if (data.contains("duration")) {
        param.duration = data["duration"].toLongLong();
    }
    if (data.contains("bitrate")) {
        param.bitrate = data["bitrate"].toInt();
    }
    if (data.contains("framerate")) {
        param.framerate = data["framerate"].toDouble();
    }
    if (data.contains("codec")) {
        param.codec = data["codec"];
    }
    if (data.contains("audio")) {
        param.audio = data["audio"];
    }
    if (data.contains("width")) {
        param.width = data["width"].toShort();
    }
    if (data.contains("height")) {
        param.height = data["height"].toShort();
    }
    if (data.contains("hash1")) {
        param.hash1 = data["hash1"].toULongLong();
    }
    if (data.contains("hash2")) {
        param.hash2 = data["hash2"].toULongLong();
    }
    
    return param;
}

bool SimplifiedTestHelpers::saveThumbnail(const QByteArray& thumbnail, const QString& path)
{
    if (QFileInfo::exists(path)) {
        qWarning() << "Thumbnail already exists, not overwriting:" << path;
        return false;
    }
    
    QFile thumbFile(path);
    if (!thumbFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open thumbnail file for writing:" << path;
        return false;
    }
    
    qint64 written = thumbFile.write(thumbnail);
    thumbFile.close();
    
    if (written != thumbnail.size()) {
        qWarning() << "Failed to write complete thumbnail data";
        return false;
    }
    
    return true;
}

QByteArray SimplifiedTestHelpers::loadThumbnailFromFile(const QString& path)
{
    QFile thumbFile(path);
    if (!thumbFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open thumbnail file for reading:" << path;
        return QByteArray();
    }
    
    QByteArray data = thumbFile.readAll();
    thumbFile.close();
    
    return data;
}

double SimplifiedTestHelpers::compareThumbnails(const QByteArray& thumb1, const QByteArray& thumb2)
{
    if (thumb1.isEmpty() || thumb2.isEmpty()) {
        qWarning() << "One or both thumbnails are empty";
        return 0.0;
    }
    
    // Convert to grayscale OpenCV Mat
    Mat gray1 = thumbnailToGrayMat(thumb1);
    Mat gray2 = thumbnailToGrayMat(thumb2);
    
    if (gray1.empty() || gray2.empty()) {
        qWarning() << "Failed to convert thumbnails to grayscale";
        return 0.0;
    }
    
    // Check dimensions match
    if (gray1.size() != gray2.size()) {
        qWarning() << "Thumbnail dimensions don't match:" 
                   << gray1.cols << "x" << gray1.rows << "vs" 
                   << gray2.cols << "x" << gray2.rows;
        return 0.0;
    }
    
    // Use SSIM with block size of 16 (same as app default)
    const int blockSize = 16;
    return Comparison::ssim(gray1, gray2, blockSize);
}

bool SimplifiedTestHelpers::compareMetadata(const VideoParam& ref, const VideoParam& current, QString& errorMsg)
{
    errorMsg.clear();
    
    if (ref.size != current.size) {
        errorMsg = QString("Size mismatch: ref=%1 current=%2").arg(ref.size).arg(current.size);
        return false;
    }
    
    // allow 50ms tolerance
    if (qAbs(ref.duration - current.duration) > 50) {
        errorMsg = QString("Duration mismatch: ref=%1 current=%2").arg(ref.duration).arg(current.duration);
        return false;
    }
    
    // allow 50 kb/s tolerance
    if (qAbs(ref.bitrate - current.bitrate) > 50) {
        errorMsg = QString("Bitrate mismatch: ref=%1 current=%2").arg(ref.bitrate).arg(current.bitrate);
        return false;
    }
    
    if (ref.framerate != current.framerate) {
        errorMsg = QString("Framerate mismatch: ref=%1 current=%2").arg(ref.framerate).arg(current.framerate);
        return false;
    }
    
    if (ref.codec != current.codec) {
        errorMsg = QString("Codec mismatch: ref=%1 current=%2").arg(ref.codec, current.codec);
        return false;
    }
    
    if (ref.audio != current.audio) {
        errorMsg = QString("Audio mismatch: ref=%1 current=%2").arg(ref.audio, current.audio);
        return false;
    }
    
    if (ref.width != current.width || ref.height != current.height) {
        errorMsg = QString("Dimensions mismatch: ref=%1x%2 current=%3x%4")
            .arg(ref.width).arg(ref.height).arg(current.width).arg(current.height);
        return false;
    }
    
    if (ref.hash1 != current.hash1) {
        errorMsg = QString("Hash1 mismatch: ref=%1 current=%2").arg(ref.hash1).arg(current.hash1);
        return false;
    }
    
    if (ref.hash2 != current.hash2) {
        errorMsg = QString("Hash2 mismatch: ref=%1 current=%2").arg(ref.hash2).arg(current.hash2);
        return false;
    }
    
    return true;
}

// ------------------- Private Helper Functions -------------------

Mat SimplifiedTestHelpers::thumbnailToGrayMat(const QByteArray& thumbnail)
{
    // Load thumbnail as QImage
    QImage image;
    if (!image.loadFromData(thumbnail, "JPG")) {
        qWarning() << "Failed to load thumbnail from QByteArray";
        return Mat();
    }
    
    // Convert to grayscale QImage
    QImage grayImage = image.convertToFormat(QImage::Format_Grayscale8);
    
    // Convert to OpenCV Mat
    Mat mat(grayImage.height(), grayImage.width(), CV_8UC1, 
            const_cast<uchar*>(grayImage.bits()), grayImage.bytesPerLine());
    
    // Clone to ensure we have our own copy
    Mat result = mat.clone();
    
    // Convert to float for SSIM calculation
    result.convertTo(result, CV_32F);
    
    return result;
}

