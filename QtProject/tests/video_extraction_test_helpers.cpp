#include "video_extraction_test_helpers.h"

#include "../app/comparison/internal/ssim.h"
#include "../app/db.h"
#include "../app/video.h"

#include "opencv2/imgproc.hpp"

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTest>
#include <QTextStream>

using namespace cv;

VideoParam VideoExtractionTestHelpers::scanVideoMetadata(const QString& videoPath, Prefs& prefs)
{
    VideoParam param;
    if (prefs.useCacheOption() != Prefs::NO_CACHE)
        Db::initDbAndCacheLocation(prefs);

    Video video(prefs, videoPath);
    const auto result = video.process();
    if (!result.success) {
        qWarning() << "Failed to process video:" << videoPath << "Error:" << result.errorMsg;
        return param;
    }

    param.videoInfo = QFileInfo(videoPath);
    param.size = video.size;
    param.modified = video.modified;
    param.duration = video.duration;
    param.bitrate = video.bitrate;
    param.framerate = video.framerate;
    param.codec = video.codec;
    param.audio = video.audio;
    param.width = video.width;
    param.height = video.height;
    param.hash1 = video.fingerprint(0).phash;
    param.hash2 = video.fingerprint(1).phash;
    param.thumbnail = video.thumbnail;
    return param;
}

void VideoExtractionTestHelpers::loadMetadataFromFile(const QString& filePath, VideoParam& param)
{
    QFile file(filePath);
    QVERIFY2(file.open(QIODevice::ReadOnly),
             QString("Failed to open metadata file for reading: %1").arg(filePath).toUtf8());
    QTextStream input(&file);
    QMap<QString, QString> data;
    while (!input.atEnd()) {
        const QString line = input.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const int colon = line.indexOf(':');
        if (colon >= 0)
            data.insert(line.left(colon), line.mid(colon + 1));
    }
    file.close();

    QVERIFY2(data.contains(QStringLiteral("videoFilename")), qPrintable(QStringLiteral("videoFilename not found in %1").arg(filePath)));
    param.videoInfo =
        QFileInfo(QFileInfo(filePath).absoluteDir().filePath(data.value(QStringLiteral("videoFilename"))));
    QVERIFY2(param.videoInfo.exists(), qPrintable(QStringLiteral("Video file not found: %1").arg(param.videoInfo.absoluteFilePath())));
    param.thumbnailInfo = QFileInfo(filePath.left(filePath.lastIndexOf('.')) + QStringLiteral(".jpg"));
    QVERIFY2(param.thumbnailInfo.exists(),
             qPrintable(QStringLiteral("Thumbnail file not found: %1").arg(param.thumbnailInfo.absoluteFilePath())));
    const QStringList required = {QStringLiteral("size"),      QStringLiteral("modified"),
                                  QStringLiteral("duration"),  QStringLiteral("bitrate"),
                                  QStringLiteral("framerate"), QStringLiteral("codec"),
                                  QStringLiteral("audio"),     QStringLiteral("width"),
                                  QStringLiteral("height"),    QStringLiteral("hash1"),
                                  QStringLiteral("hash2")};
    for (const QString& key : required)
        QVERIFY2(data.contains(key), qPrintable(QStringLiteral("%1 not found in %2").arg(key, filePath)));
    param.size = data.value(QStringLiteral("size")).toLongLong();
    param.modified = QDateTime::fromString(data.value(QStringLiteral("modified")), VideoParam::timeformat());
    param.duration = data.value(QStringLiteral("duration")).toLongLong();
    param.bitrate = data.value(QStringLiteral("bitrate")).toInt();
    param.framerate = data.value(QStringLiteral("framerate")).toDouble();
    param.codec = data.value(QStringLiteral("codec"));
    param.audio = data.value(QStringLiteral("audio"));
    param.width = data.value(QStringLiteral("width")).toShort();
    param.height = data.value(QStringLiteral("height")).toShort();
    param.hash1 = data.value(QStringLiteral("hash1")).toULongLong();
    param.hash2 = data.value(QStringLiteral("hash2")).toULongLong();
}

QByteArray VideoExtractionTestHelpers::loadThumbnailFromFile(const QString& path)
{
    QFile thumbnail(path);
    if (!thumbnail.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open thumbnail:" << path;
        return {};
    }
    return thumbnail.readAll();
}

double VideoExtractionTestHelpers::compareThumbnails(const QByteArray& thumb1, const QByteArray& thumb2)
{
    if (thumb1.isEmpty() || thumb2.isEmpty())
        return 0.0;
    const Mat gray1 = thumbnailToGrayMat(thumb1);
    const Mat gray2 = thumbnailToGrayMat(thumb2);
    if (gray1.empty() || gray2.empty() || gray1.size() != gray2.size())
        return 0.0;
    return Ssim::calculate(gray1, gray2, 16);
}

bool VideoExtractionTestHelpers::compareMetadata(const VideoParam& ref, const VideoParam& current, QString& errorMsg)
{
    errorMsg.clear();
    if (ref.size != current.size)
        errorMsg = QString("Size mismatch: ref=%1 current=%2").arg(ref.size).arg(current.size);
    else if (qAbs(ref.duration - current.duration) > 50)
        errorMsg = QString("Duration mismatch: ref=%1 current=%2").arg(ref.duration).arg(current.duration);
    else if (qAbs(ref.bitrate - current.bitrate) > 50)
        errorMsg = QString("Bitrate mismatch: ref=%1 current=%2").arg(ref.bitrate).arg(current.bitrate);
    else if (ref.framerate != current.framerate)
        errorMsg = QString("Framerate mismatch: ref=%1 current=%2").arg(ref.framerate).arg(current.framerate);
    else if (ref.codec != current.codec)
        errorMsg = QString("Codec mismatch: ref=%1 current=%2").arg(ref.codec, current.codec);
    else if (ref.audio != current.audio)
        errorMsg = QString("Audio mismatch: ref=%1 current=%2").arg(ref.audio, current.audio);
    else if (ref.width != current.width || ref.height != current.height)
        errorMsg = QString("Dimensions mismatch: ref=%1x%2 current=%3x%4")
                       .arg(ref.width)
                       .arg(ref.height)
                       .arg(current.width)
                       .arg(current.height);
    else if (ref.hash1 != current.hash1)
        errorMsg = QString("Hash1 mismatch: ref=%1 current=%2").arg(ref.hash1).arg(current.hash1);
    else if (ref.hash2 != current.hash2)
        errorMsg = QString("Hash2 mismatch: ref=%1 current=%2").arg(ref.hash2).arg(current.hash2);
    return errorMsg.isEmpty();
}

Mat VideoExtractionTestHelpers::thumbnailToGrayMat(const QByteArray& thumbnail)
{
    QImage image;
    if (!image.loadFromData(thumbnail, "JPG"))
        return {};
    const QImage grayscale = image.convertToFormat(QImage::Format_Grayscale8);
    Mat mat(grayscale.height(), grayscale.width(), CV_8UC1, const_cast<uchar*>(grayscale.bits()),
            grayscale.bytesPerLine());
    Mat result = mat.clone();
    result.convertTo(result, CV_32F);
    return result;
}
