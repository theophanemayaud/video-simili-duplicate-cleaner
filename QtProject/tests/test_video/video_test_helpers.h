#ifndef VIDEO_TEST_HELPERS_H
#define VIDEO_TEST_HELPERS_H

#include <QFileInfo>
#include <QDir>
#include <QList>
#include <QDateTime>

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
};

class TestHelpers {
public:
    static bool doThumbnailsLookSameWindow(const QByteArray ref_thumb, const QByteArray new_thumb, const QString title);
    // returns an empty list if either csv didn't exist, or something else happened.
    static QList<VideoParam> importCSVtoVideoParamQList(const QFileInfo csvInfo, const QDir videoDir, const QDir thumbDir); // CSV : sep  seperated values

    // will not overwrite it already exists
    static bool saveVideoParamQListToCSV(const QList<VideoParam> videoParamQList, const QFileInfo csvInfo, const QDir videoBaseDir); // false->failed to save
};

#endif // VIDEO_TEST_HELPERS_H

