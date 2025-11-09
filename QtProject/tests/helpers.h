#ifndef VIDEO_PARAM_HELPERS_H
#define VIDEO_PARAM_HELPERS_H

#include <QString>
#include <QFileInfo>
#include <QDateTime>
#include <QByteArray>

class VideoParam {
public:
    static const int nb_params = 13;
    static const char sep = '"';
    
    static QString timeformat() {
        return "yyyy-MM-dd HH:mm:ss";
    }

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

    QByteArray thumbnail; // Optional: thumbnail image data
};

#endif // VIDEO_PARAM_HELPERS_H
