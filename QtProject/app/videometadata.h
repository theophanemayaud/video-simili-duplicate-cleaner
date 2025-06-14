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
    QMap<QString, QString> additionalMetadata;

    void setRelevantValuesFromAdditionalMetadata(){
        // This function is called to set the relevant values from additional metadata
        // It can be used to set values like GPS coordinates, or in the future if we start using others, camera model, etc.

        for (auto meta = this->additionalMetadata.constBegin(); meta != this->additionalMetadata.constEnd(); ++meta) {
            if(this->gpsCoordinates.isEmpty()
                && meta.key() == "location") {
                this->gpsCoordinates = meta.value();
            }
        };
    };
};

#endif // VIDEOMETADATA_H
