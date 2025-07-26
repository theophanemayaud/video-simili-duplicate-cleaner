#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QtConcurrent/QtConcurrent>
#include <QScrollBar>
#include <QDesktopServices>

#include "ui_mainwindow.h"

#include "video.h"
#include "comparison.h"
#include "db.h"
#include "prefs.h"

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

    friend class TestVideo;

public:
    MainWindow();
    ~MainWindow() { deleteTemporaryFiles(); delete ui; }

private:
    Ui::MainWindow *ui;
    Comparison *_comparison = nullptr;

    QVector<Video *> _videoList;
    QSet<QString> _everyVideo; // set as we need to avoid duplicates
    QStringList _rejectedVideos;
    QStringList _extensionList;

    Prefs _prefs;
    bool _userPressedStop = false;
    bool shouldScan = true;

private slots:
    void deleteTemporaryFiles() const;
    void closeEvent(QCloseEvent *event) { Q_UNUSED (event) _userPressedStop = true; }
    void dragEnterEvent(QDragEnterEvent *event) { if(event->mimeData()->hasUrls()) event->acceptProposedAction(); }
    void dropEvent(QDropEvent *event);
    void loadExtensions();

    void setComparisonMode(const int &mode);
    void on_selectThumbnails_activated(const int &index);
    void on_selectPhash_clicked(const bool &checked);
    void on_selectSSIM_clicked(const bool &checked);
    void on_blocksizeCombo_activated(const int &index);
    void on_differentDurationCombo_activated(const int &index);
    void on_sameDurationCombo_activated(const int &index);
    void on_thresholdSlider_valueChanged(const int &value);
    void setMatchSimilarityThreshold(const int &value);
    void setUseCacheOption(Prefs::USE_CACHE_OPTION opt);

    void on_directoryBox_textChanged(const QString &arg1);
    void on_browseFolders_clicked();
    void on_browseApplePhotos_clicked();
    void on_directoryBox_returnPressed() { on_findDuplicates_clicked(); }
    void on_findDuplicates_clicked();
    void findVideos(QDir &dir);
    void processVideos();
    void videoSummary();

    void addStatusMessage(const QString &message) const;
    void addVideo(Video *addMe);
    void removeVideo(Video *deleteMe, QString errorMsg);
    void on_actionAbout_triggered();
    void on_actionCredits_triggered();
    void on_actionContact_triggered();

    void on_actionChange_trash_folder_triggered();
    void on_actionEnable_direct_deletion_instead_of_trash_triggered();
    void on_actionRestoreMoveToTrash_triggered();

    void on_actionEmpty_cache_triggered();
    void on_actionSet_custom_cache_location_triggered();
    void on_actionRestore_default_cache_location_triggered();

    void on_verboseCheckbox_stateChanged(int arg1) {this->_prefs.setVerbose(arg1 == 2); /* 2 -> checked, 0 -> unchecked*/ };

    void on_actionRestore_all_settings_triggered();

    // cache options
    void on_radio_UseCacheNo_clicked() { setUseCacheOption(Prefs::NO_CACHE); };
    void on_radio_UseCacheYes_clicked() { setUseCacheOption(Prefs::WITH_CACHE); };
    void on_radio_UseCacheOnly_clicked() { setUseCacheOption(Prefs::CACHE_ONLY); };
    void on_actionDelete_log_files_triggered();
    void on_actionOpen_logs_folder_triggered();
};

#endif // MAINWINDOW_H
