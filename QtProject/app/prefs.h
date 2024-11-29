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
    int _numberOfVideos = 0;
    int _ssimBlockSize = 16;

    double _thresholdSSIM = SSIM_THRESHOLD;
    int _thresholdPhash = 57;

    int _differentDurationModifier = 4;
    int _sameDurationModifier = 1;

    DeletionModes delMode = STANDARD_TRASH;
    QDir trashDir = QDir::root();

    QString appVersion = "undefined";

    QString cacheFilePathName() const {return QFileInfo(QSettings(APP_NAME, APP_NAME).value("cache_file_path_name").toString()).absoluteFilePath();}
    void cacheFilePathName(const QString cacheFilePathName) {QSettings(APP_NAME, APP_NAME).setValue("cache_file_path_name", cacheFilePathName);}

    QString browseApplePhotosLastPath() const {return QSettings(APP_NAME, APP_NAME).value("browse_apple_photos_last_path").toString();}
    void browseApplePhotosLastPath(const QString dirPath) {QSettings(APP_NAME, APP_NAME).setValue("browse_apple_photos_last_path", dirPath);}

    QString browseFoldersLastPath() const {return QSettings(APP_NAME, APP_NAME).value("browse_folders_last_path").toString();}
    void browseFoldersLastPath(const QString dirPath) {QSettings(APP_NAME, APP_NAME).setValue("browse_folders_last_path", dirPath);}

    QStringList scanLocations() const {return QSettings(APP_NAME, APP_NAME).value("scan_locations").toStringList();}
    void scanLocations(const QStringList folders) {QSettings(APP_NAME, APP_NAME).setValue("scan_locations", folders);}

    QStringList lockedFoldersList() const {return QSettings(APP_NAME, APP_NAME).value("locked_folders_list").toStringList();}
    void lockedFoldersList(const QStringList folders) {QSettings(APP_NAME, APP_NAME).setValue("locked_folders_list", folders);}

    QString browseLockedFoldersLastPath() const {return QSettings(APP_NAME, APP_NAME).value("browse_locked_folders_last_path").toString();}
    void browseLockedFoldersLastPath(const QString dirPath) {QSettings(APP_NAME, APP_NAME).setValue("browse_locked_folders_last_path", dirPath);}

    int thumbnailsMode() const {
        auto readOk = false;
        auto thumbMode = QSettings(APP_NAME, APP_NAME).value("thumbnails_mode").toInt(&readOk);
        if(!readOk)
            return cutEnds;
        return thumbMode;
    }
    void thumbnailsMode(const int mode) {QSettings(APP_NAME, APP_NAME).setValue("thumbnails_mode", mode);}

    bool isVerbose() const {return QSettings(APP_NAME, APP_NAME).value("verbose_logging").toBool();}
    void setVerbose(const bool verbose) {QSettings(APP_NAME, APP_NAME).setValue("verbose_logging", verbose);}

    void resetSettings() {QSettings(APP_NAME, APP_NAME).clear();}
};

class Message: public QObject {
    Q_OBJECT
public:
    static Message* Get() {
        static Message instance;
        return &instance;
    }
    void add(const QString& message){
        emit statusMessage(message);
    };

signals:
    void statusMessage(const QString& message);
private:
    Message() = default; // Private constructor to enforce singleton
    Q_DISABLE_COPY(Message) // Prevent copying
};

#endif // PREFS_H
