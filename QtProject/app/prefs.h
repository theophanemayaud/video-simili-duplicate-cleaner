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
        if(this->compMode == nullptr){
            auto readOk = false;
            auto mode = static_cast<VisualComparisonModes>(QSettings(APP_NAME, APP_NAME).value("comparison_mode").toInt(&readOk));
            if(!readOk)
                mode = _PHASH;
            this->compMode = std::make_unique<VisualComparisonModes>(mode);
        }
        return *this->compMode;
    }
    void comparisonMode(const VisualComparisonModes mode) {
        if(this->compMode == nullptr)
            this->compMode = std::make_unique<VisualComparisonModes>(_PHASH);
        else
            *this->compMode = mode;
        QSettings(APP_NAME, APP_NAME).setValue("comparison_mode", mode);}

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
        if(this->useCacheOptionStatic == nullptr){
            auto readOk = false;
            auto mode = static_cast<USE_CACHE_OPTION>(QSettings(APP_NAME, APP_NAME).value("use_cache_option").toInt(&readOk));
            if(!readOk)
                mode = WITH_CACHE;
            this->useCacheOptionStatic = std::make_unique<USE_CACHE_OPTION>(mode);
        }
        return *this->useCacheOptionStatic;
    }
    void useCacheOption(const USE_CACHE_OPTION opt) {
        if(this->useCacheOptionStatic == nullptr)
            this->useCacheOptionStatic = std::make_unique<USE_CACHE_OPTION>(WITH_CACHE);
        else
            *this->useCacheOptionStatic = opt;

        QSettings(APP_NAME, APP_NAME).setValue("use_cache_option", opt);
    }

    QString cacheFilePathName() const {
        if(cacheFilePathNameStatic == nullptr){
            auto path = QFileInfo(QSettings(APP_NAME, APP_NAME).value("cache_file_path_name").toString()).absoluteFilePath();
            this->cacheFilePathNameStatic = std::make_unique<QString>(path);
        }
        return *this->cacheFilePathNameStatic;
    }
    void cacheFilePathName(const QString cacheFilePathName) {
        if(this->cacheFilePathNameStatic == nullptr)
            this->cacheFilePathNameStatic = std::make_unique<QString>(cacheFilePathName);
        else
            *this->cacheFilePathNameStatic = cacheFilePathName;
        QSettings(APP_NAME, APP_NAME).setValue("cache_file_path_name", cacheFilePathName);
    }

    QString browseApplePhotosLastPath() const {return QSettings(APP_NAME, APP_NAME).value("browse_apple_photos_last_path").toString();}
    void browseApplePhotosLastPath(const QString dirPath) {QSettings(APP_NAME, APP_NAME).setValue("browse_apple_photos_last_path", dirPath);}

    QString browseFoldersLastPath() const {return QSettings(APP_NAME, APP_NAME).value("browse_folders_last_path").toString();}
    void browseFoldersLastPath(const QString dirPath) {QSettings(APP_NAME, APP_NAME).setValue("browse_folders_last_path", dirPath);}

    QStringList scanLocations() const {
        // TODO temporarily disabled as it doesn't work on macOS because of sandbox
        // Need to implement bookmark system to store access rights before we can use this again
        // return QSettings(APP_NAME, APP_NAME).value("scan_locations").toStringList();}
        return QStringList();
    }
    void scanLocations(const QStringList folders) {QSettings(APP_NAME, APP_NAME).setValue("scan_locations", folders);}

    QStringList lockedFoldersList() const {return QSettings(APP_NAME, APP_NAME).value("locked_folders_list").toStringList();}
    void lockedFoldersList(const QStringList folders) {QSettings(APP_NAME, APP_NAME).setValue("locked_folders_list", folders);}

    QString browseLockedFoldersLastPath() const {return QSettings(APP_NAME, APP_NAME).value("browse_locked_folders_last_path").toString();}
    void browseLockedFoldersLastPath(const QString dirPath) {QSettings(APP_NAME, APP_NAME).setValue("browse_locked_folders_last_path", dirPath);}

    int thumbnailsMode() const {
        if(this->thumbMode == nullptr){
            auto readOk = false;
            this->thumbMode = std::make_unique<int>(QSettings(APP_NAME, APP_NAME).value("thumbnails_mode").toInt(&readOk));
            if(!readOk){
                *this->thumbMode = cutEnds;
            }
        }
        return *this->thumbMode;
    }
    void thumbnailsMode(const int mode) {
        if(this->thumbMode == nullptr)
            this->thumbMode = std::make_unique<int>(cutEnds);
        else
            *this->thumbMode = mode;
        QSettings(APP_NAME, APP_NAME).setValue("thumbnails_mode", mode);
    }

    bool isVerbose() const {
        if(this->verboseStatic == nullptr){
            this->verboseStatic = std::make_unique<bool>(QSettings(APP_NAME, APP_NAME).value("verbose_logging").toBool());
        }

        return *this->verboseStatic;
    }
    void setVerbose(const bool verbose) {
        if(this->verboseStatic == nullptr)
            this->verboseStatic = std::make_unique<bool>(verbose);
        else
            *this->verboseStatic = verbose;
        QSettings(APP_NAME, APP_NAME).setValue("verbose_logging", verbose);
    }

    void resetSettings() {
        this->thumbMode = nullptr;
        this->compMode = nullptr;
        this->cacheFilePathNameStatic = nullptr;
        this->useCacheOptionStatic = nullptr;
        this->verboseStatic = nullptr;
        QSettings(APP_NAME, APP_NAME).clear();
    }
private:
    inline static std::unique_ptr<int> thumbMode = nullptr;
    inline static std::unique_ptr<VisualComparisonModes> compMode = nullptr;
    inline static std::unique_ptr<QString> cacheFilePathNameStatic = nullptr;
    inline static std::unique_ptr<USE_CACHE_OPTION> useCacheOptionStatic = nullptr;
    inline static std::unique_ptr<bool> verboseStatic = nullptr;
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
    // DO NOT USE SIGNAL, this is only for internal use / mainwindow connection
    // Instead use Mesage::Get().add("your message")
    void statusMessage(const QString& message);
private:
    Message() = default; // Private constructor to enforce singleton
    Q_DISABLE_COPY(Message) // Prevent copying
};

#endif // PREFS_H
