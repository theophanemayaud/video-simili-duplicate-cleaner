#ifndef PREFS_H
#define PREFS_H

#include <QWidget>
#include <QDir>
#include "thumbnail.h"

#define SSIM_THRESHOLD 1.0

class Prefs
{
public:
    enum _modes { _PHASH, _SSIM };
    enum DeletionModes { STANDARD_TRASH, CUSTOM_TRASH, DIRECT_DELETION };

    QWidget *_mainwPtr = nullptr;               //pointer to MainWindow, for connecting signals to it's slots

    int _comparisonMode = _PHASH;
    int _thumbnails = cutEnds;
    int _numberOfVideos = 0;
    int _ssimBlockSize = 16;

    double _thresholdSSIM = SSIM_THRESHOLD;
    int _thresholdPhash = 57;

    int _differentDurationModifier = 4;
    int _sameDurationModifier = 1;

    DeletionModes delMode = STANDARD_TRASH;
    QDir trashDir = QDir::root();

    QString cacheFilePathName = "";

    QString appVersion = "undefined";
};

#endif // PREFS_H
