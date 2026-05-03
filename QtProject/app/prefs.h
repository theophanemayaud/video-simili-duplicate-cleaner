#ifndef PREFS_H
#define PREFS_H

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QWidget>

#include "thumbnail.h"

class Prefs
{
  public:
    enum VisualComparisonModes {
        _PHASH,
        _SSIM
    };
    enum DeletionModes {
        STANDARD_TRASH,
        CUSTOM_TRASH,
        DIRECT_DELETION
    };
    enum SortCriterion {
        BySizeDescending,
        ByNameAscending,
        ByCreationDateAscending
    };
    static constexpr double DEFAULT_SSIM_THRESHOLD = 1.0;
    static constexpr int DEFAULT_MATCH_SIMILARITY_THRESHOLD = 100; // used for slider, as in % similarity

    QWidget* _mainwPtr = nullptr; //pointer to MainWindow, for connecting signals to it's slots

    VisualComparisonModes comparisonMode() const
    {
        if (this->compMode == nullptr) {
            auto readOk = false;
            auto mode = static_cast<VisualComparisonModes>(
                QSettings(APP_NAME, APP_NAME).value("comparison_mode").toInt(&readOk));
            if (!readOk)
                mode = _PHASH;
            this->compMode = std::make_unique<VisualComparisonModes>(mode);
        }
        return *this->compMode;
    }
    void comparisonMode(const VisualComparisonModes mode)
    {
        if (this->compMode == nullptr)
            this->compMode = std::make_unique<VisualComparisonModes>(_PHASH);
        else
            *this->compMode = mode;
        QSettings(APP_NAME, APP_NAME).setValue("comparison_mode", mode);
    }

    int matchSimilarityThreshold() const
    {
        auto readOk = false;
        auto threshold = QSettings(APP_NAME, APP_NAME).value("match_similarity_threshold").toInt(&readOk);
        if (!readOk)
            return DEFAULT_MATCH_SIMILARITY_THRESHOLD;
        return threshold;
    }
    void matchSimilarityThreshold(const int threshold)
    {
        QSettings(APP_NAME, APP_NAME).setValue("match_similarity_threshold", threshold);
    }

    int _numberOfVideos = 0;
    int _ssimBlockSize = 16;

    double _thresholdSSIM = DEFAULT_SSIM_THRESHOLD;
    int _thresholdPhash = 57;

    int _differentDurationModifier = 4;
    int _sameDurationModifier = 1;

    DeletionModes delMode = STANDARD_TRASH;

    QString appVersion = "undefined";

    bool hasCustomTrashFolder() const
    {
        if (this->customTrashFolderConfiguredStatic == nullptr) {
            auto settings = QSettings(APP_NAME, APP_NAME);
            bool hasConfiguredFolder = false;
            if (settings.contains("custom_trash_folder")) {
                hasConfiguredFolder = QDir(settings.value("custom_trash_folder").toString()).exists();
                if (!hasConfiguredFolder)
                    settings.remove("custom_trash_folder");
            }
            this->customTrashFolderConfiguredStatic = std::make_unique<bool>(hasConfiguredFolder);
        }
        return *this->customTrashFolderConfiguredStatic;
    }
    // Only call this after ensuring hasCustomTrashFolder() is true
    QDir customTrashFolder() const
    {
        if (this->customTrashFolderStatic == nullptr) {
            auto settings = QSettings(APP_NAME, APP_NAME);
            auto dir = QDir();
            if (hasCustomTrashFolder()) {
                dir = QDir(settings.value("custom_trash_folder").toString());
            }
            else {
                // This should not happen,
                const auto standardVideosFolders = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation);
                dir = QDir(standardVideosFolders.isEmpty() ? QDir::homePath() : standardVideosFolders.first());
            }
            this->customTrashFolderStatic = std::make_unique<QDir>(dir);
        }
        return *this->customTrashFolderStatic;
    }
    void customTrashFolder(const QDir dir)
    {
        if (this->customTrashFolderStatic == nullptr)
            this->customTrashFolderStatic = std::make_unique<QDir>(dir);
        else
            *this->customTrashFolderStatic = dir;
        if (this->customTrashFolderConfiguredStatic == nullptr)
            this->customTrashFolderConfiguredStatic = std::make_unique<bool>(true);
        else
            *this->customTrashFolderConfiguredStatic = true;
        QSettings(APP_NAME, APP_NAME).setValue("custom_trash_folder", dir.absolutePath());
    }
    void clearCustomTrashFolder()
    {
        this->customTrashFolderStatic = nullptr;
        if (this->customTrashFolderConfiguredStatic == nullptr)
            this->customTrashFolderConfiguredStatic = std::make_unique<bool>(false);
        else
            *this->customTrashFolderConfiguredStatic = false;
        QSettings(APP_NAME, APP_NAME).remove("custom_trash_folder");
    }

    // Error video modes: just leave as is (skip) or move to selected folder (move)
    enum ErrorVideoModes {
        SKIP_ERROR_VIDEOS,
        MOVE_ERROR_VIDEOS
    };

    ErrorVideoModes errorVideoMode() const
    {
        if (this->errorVideoModeStatic == nullptr) {
            auto settings = QSettings(APP_NAME, APP_NAME);
            auto mode = SKIP_ERROR_VIDEOS;
            if (settings.contains("error_videos_folder")) {
                const bool hasConfiguredFolder = QDir(settings.value("error_videos_folder").toString()).exists();
                if (hasConfiguredFolder)
                    mode = MOVE_ERROR_VIDEOS;
                else
                    settings.remove("error_videos_folder");
            }
            this->errorVideoModeStatic = std::make_unique<ErrorVideoModes>(mode);
        }
        return *this->errorVideoModeStatic;
    }

    // Only call this after ensuring errorVideoMode() is set to MOVE_ERROR_VIDEOS
    // Otherwise will return the standard videos folder
    QDir moveErrorVideosToFolder() const
    {
        if (this->errorVideosFolderStatic == nullptr) {
            auto settings = QSettings(APP_NAME, APP_NAME);
            auto dir = QDir();
            if (errorVideoMode() == MOVE_ERROR_VIDEOS) {
                dir = QDir(settings.value("error_videos_folder").toString());
            }
            else {
                // This should never happen, and is actually invalid state
                const auto standardVideosFolders = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation);
                dir = QDir(standardVideosFolders.isEmpty() ? QDir::homePath() : standardVideosFolders.first());
            }
            this->errorVideosFolderStatic = std::make_unique<QDir>(dir);
        }
        return *this->errorVideosFolderStatic;
    }
    void moveErrorVideosToFolder(const QDir dir)
    {
        if (this->errorVideosFolderStatic == nullptr)
            this->errorVideosFolderStatic = std::make_unique<QDir>(dir);
        else
            *this->errorVideosFolderStatic = dir;
        if (this->errorVideoModeStatic == nullptr)
            this->errorVideoModeStatic = std::make_unique<ErrorVideoModes>(MOVE_ERROR_VIDEOS);
        else
            *this->errorVideoModeStatic = MOVE_ERROR_VIDEOS;
        QSettings(APP_NAME, APP_NAME).setValue("error_videos_folder", dir.absolutePath());
    }
    void clearErrorVideosFolder()
    {
        this->errorVideosFolderStatic = nullptr;
        if (this->errorVideoModeStatic == nullptr)
            this->errorVideoModeStatic = std::make_unique<ErrorVideoModes>(SKIP_ERROR_VIDEOS);
        else
            *this->errorVideoModeStatic = SKIP_ERROR_VIDEOS;
        QSettings(APP_NAME, APP_NAME).remove("error_videos_folder");
    }

    // Cache options: no cache, with cache, or cache only
    enum USE_CACHE_OPTION : int {
        NO_CACHE,
        WITH_CACHE,
        CACHE_ONLY
    };

    USE_CACHE_OPTION useCacheOption() const
    {
        if (this->useCacheOptionStatic == nullptr) {
            auto readOk = false;
            auto mode =
                static_cast<USE_CACHE_OPTION>(QSettings(APP_NAME, APP_NAME).value("use_cache_option").toInt(&readOk));
            if (!readOk)
                mode = WITH_CACHE;
            this->useCacheOptionStatic = std::make_unique<USE_CACHE_OPTION>(mode);
        }
        return *this->useCacheOptionStatic;
    }
    void useCacheOption(const USE_CACHE_OPTION opt)
    {
        if (this->useCacheOptionStatic == nullptr)
            this->useCacheOptionStatic = std::make_unique<USE_CACHE_OPTION>(WITH_CACHE);
        else
            *this->useCacheOptionStatic = opt;

        QSettings(APP_NAME, APP_NAME).setValue("use_cache_option", opt);
    }

    QString cacheFilePathName() const
    {
        if (cacheFilePathNameStatic == nullptr) {
            auto path =
                QFileInfo(QSettings(APP_NAME, APP_NAME).value("cache_file_path_name").toString()).absoluteFilePath();
            this->cacheFilePathNameStatic = std::make_unique<QString>(path);
        }
        return *this->cacheFilePathNameStatic;
    }
    void cacheFilePathName(const QString cacheFilePathName)
    {
        if (this->cacheFilePathNameStatic == nullptr)
            this->cacheFilePathNameStatic = std::make_unique<QString>(cacheFilePathName);
        else
            *this->cacheFilePathNameStatic = cacheFilePathName;
        QSettings(APP_NAME, APP_NAME).setValue("cache_file_path_name", cacheFilePathName);
    }

    // Other small settings
    QString browseApplePhotosLastPath() const
    {
        return QSettings(APP_NAME, APP_NAME).value("browse_apple_photos_last_path").toString();
    }
    void browseApplePhotosLastPath(const QString dirPath)
    {
        QSettings(APP_NAME, APP_NAME).setValue("browse_apple_photos_last_path", dirPath);
    }

    QString browseFoldersLastPath() const
    {
        return QSettings(APP_NAME, APP_NAME).value("browse_folders_last_path").toString();
    }
    void browseFoldersLastPath(const QString dirPath)
    {
        QSettings(APP_NAME, APP_NAME).setValue("browse_folders_last_path", dirPath);
    }

    QStringList scanLocations() const
    {
        // TODO temporarily disabled as it doesn't work on macOS because of sandbox
        // Need to implement bookmark system to store access rights before we can use this again
        // return QSettings(APP_NAME, APP_NAME).value("scan_locations").toStringList();}
        return QStringList();
    }
    void scanLocations(const QStringList folders) { QSettings(APP_NAME, APP_NAME).setValue("scan_locations", folders); }

    QStringList lockedFoldersList() const
    {
        return QSettings(APP_NAME, APP_NAME).value("locked_folders_list").toStringList();
    }
    void lockedFoldersList(const QStringList folders)
    {
        QSettings(APP_NAME, APP_NAME).setValue("locked_folders_list", folders);
    }

    QString browseLockedFoldersLastPath() const
    {
        return QSettings(APP_NAME, APP_NAME).value("browse_locked_folders_last_path").toString();
    }
    void browseLockedFoldersLastPath(const QString dirPath)
    {
        QSettings(APP_NAME, APP_NAME).setValue("browse_locked_folders_last_path", dirPath);
    }

    int thumbnailsMode() const
    {
        if (this->thumbMode == nullptr) {
            auto readOk = false;
            this->thumbMode =
                std::make_unique<int>(QSettings(APP_NAME, APP_NAME).value("thumbnails_mode").toInt(&readOk));
            if (!readOk)
                *this->thumbMode = cutEnds;
        }
        return *this->thumbMode;
    }
    void thumbnailsMode(const int mode)
    {
        if (this->thumbMode == nullptr)
            this->thumbMode = std::make_unique<int>(cutEnds);
        else
            *this->thumbMode = mode;
        QSettings(APP_NAME, APP_NAME).setValue("thumbnails_mode", mode);
    }

    bool isVerbose() const
    {
        if (this->verboseStatic == nullptr) {
            this->verboseStatic =
                std::make_unique<bool>(QSettings(APP_NAME, APP_NAME).value("verbose_logging").toBool());
        }

        return *this->verboseStatic;
    }
    void setVerbose(const bool verbose)
    {
        if (this->verboseStatic == nullptr)
            this->verboseStatic = std::make_unique<bool>(verbose);
        else
            *this->verboseStatic = verbose;
        QSettings(APP_NAME, APP_NAME).setValue("verbose_logging", verbose);
    }

    SortCriterion sortCriterion() const
    {
        if (this->sortCriterionStatic == nullptr) {
            auto readOk = false;
            auto criterion =
                static_cast<SortCriterion>(QSettings(APP_NAME, APP_NAME).value("sort_criterion").toInt(&readOk));
            if (!readOk)
                criterion = BySizeDescending;
            this->sortCriterionStatic = std::make_unique<SortCriterion>(criterion);
        }
        return *this->sortCriterionStatic;
    }
    void sortCriterion(const SortCriterion criterion)
    {
        if (this->sortCriterionStatic == nullptr)
            this->sortCriterionStatic = std::make_unique<SortCriterion>(BySizeDescending);
        else
            *this->sortCriterionStatic = criterion;
        QSettings(APP_NAME, APP_NAME).setValue("sort_criterion", criterion);
    }

    void resetSettings()
    {
        this->thumbMode = nullptr;
        this->compMode = nullptr;
        this->cacheFilePathNameStatic = nullptr;
        this->useCacheOptionStatic = nullptr;
        this->verboseStatic = nullptr;
        this->sortCriterionStatic = nullptr;
        this->customTrashFolderConfiguredStatic = nullptr;
        this->customTrashFolderStatic = nullptr;
        this->errorVideoModeStatic = nullptr;
        this->errorVideosFolderStatic = nullptr;
        QSettings(APP_NAME, APP_NAME).clear();
    }

  private:
    // QSettings operations are quite slow, so we cache the values in memory
    inline static std::unique_ptr<int> thumbMode = nullptr;
    inline static std::unique_ptr<VisualComparisonModes> compMode = nullptr;
    inline static std::unique_ptr<QString> cacheFilePathNameStatic = nullptr;
    inline static std::unique_ptr<USE_CACHE_OPTION> useCacheOptionStatic = nullptr;
    inline static std::unique_ptr<bool> verboseStatic = nullptr;
    inline static std::unique_ptr<SortCriterion> sortCriterionStatic = nullptr;
    // To know whether the custom trash folder setting was loaded from QSettings we need an extra flag
    // because a QDir has no good state of "initialized but no value", it would just be root directory
    inline static std::unique_ptr<bool> customTrashFolderConfiguredStatic = nullptr;
    inline static std::unique_ptr<QDir> customTrashFolderStatic = nullptr;
    // Similar for error video folder: we need an extra flag to differentiate between not loaded vs loaded from QSettings
    inline static std::unique_ptr<ErrorVideoModes> errorVideoModeStatic = nullptr;
    inline static std::unique_ptr<QDir> errorVideosFolderStatic = nullptr;
};

class Message : public QObject
{
    Q_OBJECT
  public:
    static Message* Get()
    {
        static Message instance;
        return &instance;
    }
    void add(const QString& message) { emit statusMessage(message); };

  signals:
    // DO NOT USE SIGNAL, this is only for internal use / mainwindow connection
    // Instead use Mesage::Get().add("your message")
    void statusMessage(const QString& message);

  private:
    Message() = default;    // Private constructor to enforce singleton
    Q_DISABLE_COPY(Message) // Prevent copying
};

#endif // PREFS_H
