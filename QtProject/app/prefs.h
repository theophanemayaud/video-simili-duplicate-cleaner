#ifndef PREFS_H
#define PREFS_H

#include <QWidget>
#include <QDir>
#include <QSettings>

#include "thumbnail.h"

class Prefs
{
public:
    enum VisualComparisonModes { _PHASH, _SSIM };
    enum DeletionModes { STANDARD_TRASH, CUSTOM_TRASH, DIRECT_DELETION };
    static constexpr double DEFAULT_SSIM_THRESHOLD = 1.0;
    static constexpr int DEFAULT_MATCH_SIMILARITY_THRESHOLD = 100; // used for slider, as in % similarity
    enum USE_CACHE_OPTION : int {
        NO_CACHE,
        WITH_CACHE,
        CACHE_ONLY
    };

    QWidget *_mainwPtr = nullptr;               //pointer to MainWindow, for connecting signals to it's slots

    VisualComparisonModes comparisonMode() const {
        auto readOk = false;
        auto thumbMode = QSettings(APP_NAME, APP_NAME).value("comparison_mode").toInt(&readOk);
        if(!readOk)
            return _PHASH;
        return (VisualComparisonModes)thumbMode;
    }
    void comparisonMode(const VisualComparisonModes mode) {QSettings(APP_NAME, APP_NAME).setValue("comparison_mode", mode);}

    int matchSimilarityThreshold() const {
        auto readOk = false;
        auto threshold = QSettings(APP_NAME, APP_NAME).value("match_similarity_threshold").toInt(&readOk);
        if(!readOk)
            return DEFAULT_MATCH_SIMILARITY_THRESHOLD;
        return threshold;
    }
    void matchSimilarityThreshold(const int threshold) {QSettings(APP_NAME, APP_NAME).setValue("match_similarity_threshold", threshold);}

    int _numberOfVideos = 0;
    int _ssimBlockSize = 16;

    double _thresholdSSIM = DEFAULT_SSIM_THRESHOLD;
    int _thresholdPhash = 57;

    int _differentDurationModifier = 4;
    int _sameDurationModifier = 1;

    DeletionModes delMode = STANDARD_TRASH;
    QDir trashDir = QDir::root();

    QString appVersion = "undefined";

    USE_CACHE_OPTION useCacheOption() const {
        auto readOk = false;
        auto opt = QSettings(APP_NAME, APP_NAME).value("use_cache_option").toInt(&readOk);
        if(!readOk)
            return WITH_CACHE;
        return static_cast<USE_CACHE_OPTION>(opt);
    }
    void useCacheOption(const USE_CACHE_OPTION opt) {QSettings(APP_NAME, APP_NAME).setValue("use_cache_option", opt);}

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
