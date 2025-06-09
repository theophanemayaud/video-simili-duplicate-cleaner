#ifndef VIDEOMETADATA_H
#define VIDEOMETADATA_H

#include "QtCore/qdatetime.h"
#include <QObject>

class VideoMetadata
{
public:
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
    QString gpsCoordinates;
    QMap<QString, QString> fileMetadata;
};

#endif // VIDEOMETADATA_H
