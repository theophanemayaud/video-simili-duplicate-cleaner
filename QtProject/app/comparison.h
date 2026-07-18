#ifndef COMPARISON_H
#define COMPARISON_H

#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QWheelEvent>

#include <memory>

#ifdef Q_OS_MACOS
#include <QProcess> // for running apple scripts and opening file in explorer

#include "obj-c.h"
#endif

#include "videopairmatcher.h"

namespace Ui
{
class Comparison;
}

class BackgroundMatchDiscovery;

class Comparison : public QDialog
{
    Q_OBJECT
    friend class TestVideo;
    friend class TestAutoDelete;
    friend class test_comparison;

  public:
    Comparison(const QVector<Video*>& videosParam, Prefs& prefsParam, const QRect& mainWindowGeometry);
    ~Comparison();

  private:
    Ui::Comparison* ui;

    QVector<Video*> _videos;
    Prefs& _prefs;
    const int64_t _maxComparisons;
    std::unique_ptr<BackgroundMatchDiscovery> _backgroundDiscovery;
    int _leftVideo = 0;  // index in the video list, of the currently displayed left video
    int _rightVideo = 0; // index in the video list, of the currently displayed right video
    int _videosDeleted = 0;
    int64_t _spaceSaved = 0;
    bool _seekForwards = true;

    int _phashSimilarity = 0;
    double _ssimSimilarity = 0.0;

    int _zoomLevel = 0;
    QPixmap _leftZoomed;
    int _leftW = 0;
    int _leftH = 0;
    QPixmap _rightZoomed;
    int _rightW = 0;
    int _rightH = 0;

    int whichFilenameContainsTheOther(QString leftFileNamepath, QString rightFileNamepath) const;
    bool _someWereMovedInApplePhotosLibrary = false;
    bool _firstScriptingAskPermission = true;

    void seekFromSliderPosition(int position);
    void restartBackgroundDiscovery();
    void updateDiscoveryProgress(int64_t safeEnd);
    bool navigateForwardFrom(int64_t currentPosition);
    bool findNextSynchronouslyFrom(int64_t firstPosition);
    bool findPreviousSynchronouslyFrom(int64_t firstPosition, int64_t lastPosition);
    bool isPairStillDisplayable(const MatchedVideoPair& pair) const;
    void displayMatchedPair(const MatchedVideoPair& pair);

    void loadLockedFolderFromPrefs();
    void addLockedFolderToList(QString folderPath);

    // --- \\
    // --- auto deletion internal stuff
    enum AUTO_DELETE_CONFIG : int {
        AUTO_DELETE_ONLY_TIMES_DIFF
    };

    class AutoDeleteConfig;
    class AutoDeleteUserSettings;

    class AutoDeleteConfig
    {
      public:
        AutoDeleteConfig(const AUTO_DELETE_CONFIG config) : _autoDelConfig(config) {};

        QString getDeleteByText() const;

        const VideoMetadata* videoToDelete(const VideoMetadata*, const VideoMetadata*,
                                           const AutoDeleteUserSettings) const; //returns null if none should be deleted

      private:
        const AUTO_DELETE_CONFIG _autoDelConfig;
    };

    class AutoDeleteUserSettings
    {
      public:
        AutoDeleteUserSettings(const bool trashEarlierIsChecked) : trashEarlierIsChecked(trashEarlierIsChecked) {};
        const bool trashEarlierIsChecked;
    };

    void autoDeleteLoopthrough(const AutoDeleteConfig);

    // -- end auto deletion internal stuff
    // --- //

  public slots:
    int reportMatchingVideos(); // returns number of matching videos found

  private slots:
    void dragEnterEvent(QDragEnterEvent* event); // drag and drop for locked folders list
    void dropEvent(QDropEvent* event);

    void confirmToExit();
    void on_prevVideo_clicked();
    void on_nextVideo_clicked();
    bool bothVideosMatch(const Video* left, const Video* right);

    void showVideo(const QString& side);
    QString readableDuration(const int64_t& milliseconds) const;
    QString readableFileSize(const int64_t& filesize) const;
    QString readableBitRate(const double& kbps) const;
    void highlightBetterProperties() const;
    void updateUI();
    int64_t comparisonsSoFar() const;
    int progressBarValue(int64_t comparisons) const;
    void onProgressSliderReleased();

    void on_selectPhash_clicked(const bool& checked);
    void on_selectSSIM_clicked(const bool& checked);

    void on_leftImage_clicked() { openMedia(_videos[_leftVideo]->_filePathName); }
    void on_rightImage_clicked() { openMedia(_videos[_rightVideo]->_filePathName); }
    void openMedia(const QString filenamepath);

    void on_leftFileName_clicked() { openFileManager(_videos[_leftVideo]->_filePathName); }
    void on_rightFileName_clicked() { openFileManager(_videos[_rightVideo]->_filePathName); }
    void openFileManager(const QString& filename);

    void on_leftDelete_clicked() { deleteVideo(_leftVideo); }
    void on_rightDelete_clicked() { deleteVideo(_rightVideo); }
    void deleteVideo(const int& side, const bool auto_trash_mode = false);

    void on_leftMove_clicked() { moveVideo(_videos[_leftVideo]->_filePathName, _videos[_rightVideo]->_filePathName); }
    void on_rightMove_clicked() { moveVideo(_videos[_rightVideo]->_filePathName, _videos[_leftVideo]->_filePathName); }
    void moveVideo(const QString& from, const QString& to);
    void on_swapFilenames_clicked() const;

    void on_thresholdSlider_valueChanged(const int& value);
    void resizeEvent(QResizeEvent* event);
    void wheelEvent(QWheelEvent* event);

    // auto delete methods
    void on_identicalFilesAutoTrash_clicked();
    void on_autoDelOnlySizeDiffersButton_clicked();
    void on_pushButton_onlyTimeDiffersAutoTrash_clicked()
    {
        autoDeleteLoopthrough(AutoDeleteConfig(AUTO_DELETE_ONLY_TIMES_DIFF));
    }

    void on_pushButton_importantFoldersAdd_clicked();
    void on_lockedFolderButton_clicked();
    void showImportantFolderContextMenu(const QPoint&);
    void showLockedFolderContextMenu(const QPoint&);
    void eraseImportantFolderItem();
    void eraseLockedFolderItem();
    void clearImportantFolderList();
    void clearLockedFolderList();

    bool isFileInProtectedFolder(const QString filePathName) const;
    void displayApplePhotosAlbumDeletionMessage();

    void on_settingNamesInAnotherCheckbox_stateChanged(int arg1);

    void on_ignoreDuplicatePairButton_clicked();

    void initSortOrder();
    void onSortOrderChanged(int index);
    void applySortOrder();

  signals:
    void sendStatusMessage(const QString& message) const;
    void switchComparisonMode(const int& mode) const;
    void adjustThresholdSlider(const int& value) const;
};

class ClickableLabel : public QLabel
{
    Q_OBJECT
  public:
    explicit ClickableLabel(QWidget* parent) { Q_UNUSED(parent) }

  protected:
    void mousePressEvent(QMouseEvent* event)
    {
        Q_UNUSED(event)
        emit clicked();
    }
  signals:
    void clicked();
};

#endif // COMPARISON_H
