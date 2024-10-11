#ifndef PREFS_H
#define PREFS_H

#include <QWidget>
#include <QDir>
#include <QSettings>

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

    QString appVersion = "undefined";

    QString cacheFilePathName() const {return QSettings(APP_NAME, APP_NAME).value("cache_file_path_name").toString();}
    void cacheFilePathName(const QString cacheFilePathName) {QSettings(APP_NAME, APP_NAME).setValue("cache_file_path_name", cacheFilePathName);}

    QString browseApplePhotosLastPath() const {return QSettings(APP_NAME, APP_NAME).value("browse_apple_photos_last_path").toString();}
    void browseApplePhotosLastPath(const QString dirPath) {QSettings(APP_NAME, APP_NAME).setValue("browse_apple_photos_last_path", dirPath);}

    QString browseFoldersLastPath() const {return QSettings(APP_NAME, APP_NAME).value("browse_folders_last_path").toString();}
    void browseFoldersLastPath(const QString dirPath) {QSettings(APP_NAME, APP_NAME).setValue("browse_folders_last_path", dirPath);}

    void resetSettings() {QSettings(APP_NAME, APP_NAME).clear();}
};

#endif // PREFS_H
