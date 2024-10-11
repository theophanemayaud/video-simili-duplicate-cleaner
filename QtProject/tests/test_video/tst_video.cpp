#include <QtTest>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrent>
#include <QElapsedTimer>

/* NB : Thumbnails are generated without cache : must run test without cache (clean all before)
 * Also sometimes, for some unknown reason, thumbnails don't come out the same.
 * But if you re-run tests a few times, it should get fixed
 * (or check visually with ENABLE_MANUAL_THUMBNAIL_VERIF) */
#define ENABLE_THUMBNAIL_VERIF
#define ENABLE_MANUAL_THUMBNAIL_VERIF // also disables test duration limit as manual checks can take time !

// Sometimes hashes go crazy, so we can manually disable them to see if other problems exist
// #define ENABLE_HASHES_VERIFICATION

// When moving over to library, audio metadata sometimes changes but when manually checked, is actually identical
// Disable following define to skip testing audio comparison
#define ENABLE_AUDIO_COMPARISON

// inside the app it default to 100, but for tests it's more interesting if lower
// (and also in previous versions it was 89 so the new default 100 could break old tests)
#define COMPARISON_THRESHOLD 100

// add necessary includes here
#include "../../app/video.h"
#include "../../app/prefs.h"

#include "../../app/mainwindow.h"
#include "../../app/comparison.h"
#include "../../app/ui_comparison.h"
#include "../../app/db.h"

#include "video_test_helpers.cpp"

class TestVideo : public QObject
{
    Q_OBJECT

public:
    TestVideo();
    ~TestVideo();

    static void compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(
        const QByteArray ref_thumbnail,
        const VideoParam videoParam,
        const Video *vid,
        const bool compareThumbs=true);

private:
#ifdef Q_OS_WIN
    QDir _videoDir = QDir("Y:/Videos/");
    const QDir _thumbnailDir_nocache = QDir("Y:/Thumbnails-nocache/");
    const QFileInfo _csvInfo_nocache = QFileInfo("C:/Dev/video-simili-duplicate-cleaner/QtProject/tests/test_video/ressources/tests-nocache.csv");

    const QDir _thumbnailDir_cached = QDir("Y:/Thumbnails-cached/");
    const QFileInfo _csvInfo_cached = QFileInfo("C:/Dev/video-simili-duplicate-cleaner/QtProject/tests/test_video/ressources/tests-cached.csv");

    QDir _100GBvideoDir = QDir("");
    const QDir _100GBthumbnailDir_nocache = QDir("");
    const QFileInfo _100GBcsvInfo_nocache = QFileInfo("");
#elif defined(Q_OS_MACOS)
    QDir _videoDir = QDir("/Users/theophanemayaud/Dev/Videos across all formats with duplicates of all kinds/Videos");
    const QDir _thumbnailDir_nocache = QDir("/Users/theophanemayaud/Dev/Videos across all formats with duplicates of all kinds/Thumbnails-nocache/");
    const QFileInfo _csvInfo_nocache = QFileInfo("/Users/theophanemayaud/Dev/Videos across all formats with duplicates of all kinds/tests-nocache.csv");

    const QDir _thumbnailDir_cached = QDir("/Users/theophanemayaud/Dev/Videos across all formats with duplicates of all kinds/Thumbnails-cached/");
    const QFileInfo _csvInfo_cached = QFileInfo("/Users/theophanemayaud/Dev/Videos across all formats with duplicates of all kinds/tests-cached.csv");

    QDir _100GBvideoDir = QDir("/Volumes/4TBSSD/Video duplicates - just for checking later my video duplicate program still works/Videos/");
    const QDir _100GBthumbnailDir_nocache = QDir("/Volumes/4TBSSD/Video duplicates - just for checking later my video duplicate program still works/Thumbnails-nocache/");
    const QFileInfo _100GBcsvInfo_nocache = QFileInfo("/Volumes/4TBSSD/Video duplicates - just for checking later my video duplicate program still works/100GBtests-nocache.csv");
#endif

private slots:
    void initTestCase();

    void emptyDb(){ Prefs prefs; Db::initDbAndCacheLocation(prefs); Db::emptyAllDb(prefs); }

//    void createRefVidParams_nocache();
//    void createRefVidParams_cached();
    void test_check_refvidparams_nocache();
    void test_check_refvidparams_cached();

    void test_whole_app();
    void test_whole_app_nocache();
    void test_whole_app_cached();
    void test_whole_app_cache_only();

//    void create100GBrefVidParams_nocache();
    void test_100GBcheckRefVidParams();
    void test_100GBcheckRefVidParams_nocache();
    void test_100GBcheckRefVidParams_cached();

    void test_100GBwholeApp();
    void test_100GBwholeApp_nocache();
    void test_100GBwholeApp_cached();

    void cleanupTestCase();
};

TestVideo::TestVideo(){}

TestVideo::~TestVideo(){}

// Run before all tests
void TestVideo::initTestCase(){
    qSetMessagePattern("%{file}(%{line}) %{function}: %{message}");
    Prefs().resetSettings();
    qDebug() << "Cleared settings";
}

// Run after all tests
void TestVideo::cleanupTestCase(){}

// ------------------------------------------------------------------------------------
// ---------------------------- START : smaller video sets tests ---------------------

// used to create cache if not already available
// is used by test_whole_app_cached and test_check_refvidparams_cached as first run,
// to make sure cache is loaded
void TestVideo::test_whole_app(){
    QElapsedTimer timer;
    timer.start();

    MainWindow w;
    w.ui->thresholdSlider->setValue(COMPARISON_THRESHOLD);
    w.show();
    w.on_actionEmpty_cache_triggered(); // clear cache to have a consistent test, to recreate cache

    QVERIFY(_videoDir.exists());

    w.findVideos(_videoDir);
    w.processVideos();
    w.ui->blocksizeCombo->setAcceptDrops(true);

    qDebug() << "Found "<<w._everyVideo.count()<<" files of which "<< w._videoList.count()<<" valid ones";
    qDebug() << "TIMER:test_whole_app before match report took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    Comparison comp(w.sortVideosBySize(), w._prefs);
    int matchingVideoNb = comp.reportMatchingVideos();
    qDebug() << QString("Found %1 matching vids").arg(matchingVideoNb);

    qDebug() << "TIMER:test_whole_app took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    comp.show();

    QTest::qWait(1000);
    comp.on_nextVideo_clicked();
    comp.ui->tabWidget->setCurrentIndex(2);
    QTest::qWait(1000);

    w.destroy();
}

void TestVideo::test_whole_app_nocache(){
    const int nb_vids_to_find = 200; //before remove big file tests 207;
    const int nb_valid_vids_to_find = 197; //before remove big file tests 204;
    const int nb_matching_vids_to_find = 71;

    // macOS universal on arm m3 pro mbp : 3'120ms, 3710ms so cap around 5s
    // macOS arm on M1        : 6 sec
    // macOS intel on intel   : 13 sec (before remove big file tests)
    // windows intel on intel : 13.5 sec (before remove big file tests)
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
   const qint64 ref_ms_time = 5*1000;
#endif
    QElapsedTimer timer;
    timer.start();

    MainWindow w;
    w.ui->thresholdSlider->setValue(COMPARISON_THRESHOLD);
    w.show();
    w.ui->radio_UseCacheNo->click(); // disable loading from and saving to cache
    w.on_actionEmpty_cache_triggered(); // clear cache too, just in case

    QVERIFY(_videoDir.exists());

    w.findVideos(_videoDir);
    w.processVideos();
    w.ui->blocksizeCombo->setAcceptDrops(true);

    qDebug() << "Found "<<w._everyVideo.count()<<" files of which "<< w._videoList.count()<<" valid ones";
    qDebug() << "TIMER:test_whole_app_nocache before match report took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    Comparison comp(w.sortVideosBySize(), w._prefs);
    int matchingVideoNb = comp.reportMatchingVideos();
    qDebug() << QString("Found %1 matching vids").arg(matchingVideoNb);

    qDebug() << "TIMER:test_whole_app_nocache took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    QVERIFY2(w._everyVideo.count()==nb_vids_to_find, QString("Found %1 files but should be %2").arg(w._everyVideo.count()).arg(nb_vids_to_find).toStdString().c_str());
    QVERIFY2(w._videoList.count()==nb_valid_vids_to_find, QString("Found %1 valid files but should be %2").arg(w._videoList.count()).arg(nb_valid_vids_to_find).toStdString().c_str());
    QVERIFY2(matchingVideoNb==nb_matching_vids_to_find, QString("Found %1 matching vids but should be %2").arg(matchingVideoNb).arg(nb_matching_vids_to_find).toStdString().c_str());

#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_whole_app_nocache took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif

    comp.show();

    QTest::qWait(1000);
    comp.on_nextVideo_clicked();
    comp.ui->tabWidget->setCurrentIndex(2);
    QTest::qWait(1000);

    w.destroy();
}

void TestVideo::test_whole_app_cached(){
    const int nb_vids_to_find = 200;
    const int nb_valid_vids_to_find = 197;
    const int nb_matching_vids_to_find = 72;

    // macOS universal on arm m3 pro mbp : 612ms, 627ms, ... so cap around 1s
    // macOS arm on M1 : 1 sec
    // macOS intel on intel : 3 sec (before remove big file tests 207)
    // windows intel on intel : 2.75 sec (before remove big file tests 207)
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
   const qint64 ref_ms_time = 1*1000;
#endif
   // run a first time to make sure all data is cached
   test_whole_app();

    QElapsedTimer timer;
    timer.start();

    MainWindow w;
    w.ui->thresholdSlider->setValue(COMPARISON_THRESHOLD);
    w.show();
    w.ui->radio_UseCacheYes->click(); // enable loading from and saving to cache (nb it's actually the default so this is redundant)

    QVERIFY(_videoDir.exists());

    w.findVideos(_videoDir);
    w.processVideos();
    w.ui->blocksizeCombo->setAcceptDrops(true);

    qDebug() << "Found "<<w._everyVideo.count()<<" files of which "<< w._videoList.count()<<" valid ones";
    qDebug() << "TIMER:test_whole_app_cached before match report took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    Comparison comp(w.sortVideosBySize(), w._prefs);
    int matchingVideoNb = comp.reportMatchingVideos();
    qDebug() << QString("Found %1 matching vids").arg(matchingVideoNb);

    qDebug() << "TIMER:test_whole_app_cached took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    QVERIFY2(w._everyVideo.count()==nb_vids_to_find, QString("Found %1 files but should be %2").arg(w._everyVideo.count()).arg(nb_vids_to_find).toStdString().c_str());
    QVERIFY2(w._videoList.count()==nb_valid_vids_to_find, QString("Found %1 valid files but should be %2").arg(w._videoList.count()).arg(nb_valid_vids_to_find).toStdString().c_str());
    QVERIFY2(matchingVideoNb==nb_matching_vids_to_find, QString("Found %1 matching vids but should be %2").arg(matchingVideoNb).arg(nb_matching_vids_to_find).toStdString().c_str());

#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_whole_app_cached took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif

    comp.show();

    QTest::qWait(1000);
    comp.on_nextVideo_clicked();
    comp.ui->tabWidget->setCurrentIndex(2);
    QTest::qWait(1000);

    w.destroy();
}

void TestVideo::test_whole_app_cache_only(){
    const int nb_vids_to_find =  200; // before remove big file tests 207
    const int nb_valid_vids_to_find = 197; // before remove big file tests 204
    const int nb_matching_vids_to_find = 72; // before remove big file tests 74

    // macOS universal on arm m3 pro mbp : 712ms, 583ms ... so cap around 1s
    // macOS arm on M1 :     <1 sec
    // macos intel on intel : 3.203 sec (before remove big file tests 207)
    // windows : ?? sec
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
   const qint64 ref_ms_time = 1*1000;
#endif
   // run a first time to make sure all data is cached
   test_whole_app();

    QElapsedTimer timer;
    timer.start();

    MainWindow w;
    w.ui->thresholdSlider->setValue(COMPARISON_THRESHOLD);
    w.show();
    w.ui->radio_UseCacheOnly->click(); // force loading from and saving to cache

    QVERIFY(_videoDir.exists());

    w.findVideos(_videoDir);
    w.processVideos();
    w.ui->blocksizeCombo->setAcceptDrops(true);

    qDebug() << "Found "<<w._everyVideo.count()<<" files of which "<< w._videoList.count()<<" valid ones";
    qDebug() << "TIMER:test_whole_app_cache_only before match report took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    Comparison comp(w.sortVideosBySize(), w._prefs);
    int matchingVideoNb = comp.reportMatchingVideos();
    qDebug() << QString("Found %1 matching vids").arg(matchingVideoNb);

    qDebug() << "TIMER:test_whole_app_cache_only took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    QVERIFY2(w._everyVideo.count()==nb_vids_to_find, QString("Found %1 files but should be %2").arg(w._everyVideo.count()).arg(nb_vids_to_find).toStdString().c_str());
    // TODO check which specific pairs !
    QVERIFY2(w._videoList.count()==nb_valid_vids_to_find, QString("Found %1 valid files but should be %2").arg(w._videoList.count()).arg(nb_valid_vids_to_find).toStdString().c_str());
    QVERIFY2(matchingVideoNb==nb_matching_vids_to_find, QString("Found %1 matching vids but should be %2").arg(matchingVideoNb).arg(nb_matching_vids_to_find).toStdString().c_str());

#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_whole_app_cache_only took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif

    comp.show();

    QTest::qWait(750);
    comp.on_nextVideo_clicked();
    QTest::qWait(750);
    comp.ui->tabWidget->setCurrentIndex(2);
    QTest::qWait(750);

    w.destroy();
};
void TestVideo::test_check_refvidparams_nocache(){
    // macOS universal on arm m3 pro mbp : 16'305ms, 16'751ms, 17'117ms, so cap around 20'000ms
    // macOS intel on intel : 35 sec (before remove big file tests)
    // windows intel on intel : 47 sec, another time run : 36sec (before remove big file tests)
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    const qint64 ref_ms_time = 20*1000;
#endif
    QElapsedTimer timer;
    timer.start();

    // read csv file
    QVERIFY(_csvInfo_nocache.exists());
    QList<VideoParam> videoParamList = TestHelpers::importCSVtoVideoParamQList(_csvInfo_nocache, _videoDir, _thumbnailDir_nocache);
    QVERIFY(!videoParamList.isEmpty());

    // compute params for all videos
    QVERIFY(!_videoDir.isEmpty());
    foreach(VideoParam videoParam, videoParamList){
        QVERIFY2(videoParam.videoInfo.exists(), videoParam.videoInfo.absoluteFilePath().toUtf8().constData());
        QVERIFY2(videoParam.thumbnailInfo.exists(), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());

        //read thumbnail from disk
        QFile ref_thumbFile(videoParam.thumbnailInfo.absoluteFilePath());
        QVERIFY2(ref_thumbFile.open(QIODevice::ReadOnly), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());
        QByteArray ref_thumbnail = ref_thumbFile.readAll();
        ref_thumbFile.close();

        Prefs prefs;
        Video *vid = new Video(prefs, videoParam.videoInfo.absoluteFilePath(), Video::NO_CACHE);
        vid->run();

        compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(ref_thumbnail, videoParam, vid);
    }
    qDebug() << "TIMER:test_check_refvidparams_nocache took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_check_refvidparams_nocache took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif
}

void TestVideo::test_check_refvidparams_cached(){
    // macOS universal on arm m3 pro mbp : 2'494ms, 2'606ms, ... so cap around 5s
    // macOS intel on intel : 8 sec (before remove big file tests)
    // windows intel on intel : 9 sec (before remove big file tests)
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    const qint64 ref_ms_time = 5*1000;
#endif
    test_whole_app(); // create the cache if it didn't exist before

    QElapsedTimer timer;
    timer.start();

    // read csv file
    QVERIFY(_csvInfo_cached.exists());
    QList<VideoParam> videoParamList = TestHelpers::importCSVtoVideoParamQList(_csvInfo_cached, _videoDir, _thumbnailDir_cached);
    QVERIFY(!videoParamList.isEmpty());

    // compute params for all videos
    QVERIFY(!_videoDir.isEmpty());
    Prefs prefs;
    Db::initDbAndCacheLocation(prefs);
    foreach(VideoParam videoParam, videoParamList){
        QVERIFY2(videoParam.videoInfo.exists(), videoParam.videoInfo.absoluteFilePath().toUtf8().constData());
        QVERIFY2(videoParam.thumbnailInfo.exists(), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());

        QFile ref_thumbFile(videoParam.thumbnailInfo.absoluteFilePath());
        QVERIFY2(ref_thumbFile.open(QIODevice::ReadOnly), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());
        QByteArray ref_thumbnail = ref_thumbFile.readAll();
        ref_thumbFile.close();

        Video *vid = new Video(prefs, videoParam.videoInfo.absoluteFilePath(), Video::WITH_CACHE);
        vid->run();

        compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(ref_thumbnail, videoParam, vid, true); // because we're loading cached thumbnails, we can compare thumbnails !
    }
    qDebug() << "TIMER:test_check_refvidparams_cached took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_check_refvidparams_cached took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif
}

/*void TestVideo::createRefVidParams_nocache()
{
    QElapsedTimer timer;
    timer.start();

    QVERIFY(!_csvInfo_nocache.exists());    // we don't want to overwrite it !

    QVERIFY(!_videoDir.isEmpty());
    MainWindow w;
    w.loadExtensions();
    QVERIFY(!w._extensionList.isEmpty());
    _videoDir.setNameFilters(w._extensionList);
    QDirIterator iter(_videoDir, QDirIterator::Subdirectories);

    QList<VideoParam> videoParamList;

    while(iter.hasNext())
    {
        const QFile vidFile(iter.next());
        const QFileInfo vidInfo = QFileInfo(vidFile);

        VideoParam videoParam;
        videoParam.videoInfo = vidInfo;

        Prefs prefs;
        Video *vid = new Video(prefs, vidInfo.absoluteFilePath(), false);
        vid->run();

        // videos can be in sub folders !
        QString thumbPath = _thumbnailDir_nocache.path() + "/" + vidInfo.absoluteFilePath().remove(_videoDir.path()+"/").replace('/','-').replace('\\','-') + "-" + vidInfo.fileName() + ".thumbnail";
        QFile thumbFile(thumbPath);
        QVERIFY2(!thumbFile.exists(), QString("Thumnail already exists %1").arg(thumbFile.fileName()).toStdString().c_str()); // we don't want to overwrite !
        thumbFile.open(QIODevice::WriteOnly);
        thumbFile.write(vid->thumbnail);
        thumbFile.close();
        QVERIFY2(thumbFile.exists(), QString("Thumnail couldn't be saved for %1").arg(thumbFile.fileName()).toStdString().c_str());

        videoParam.thumbnailInfo = QFileInfo(thumbFile);
        videoParam.size = vid->size;
        videoParam.modified = vid->modified;
        videoParam.duration = vid->duration;
        videoParam.bitrate = vid->bitrate;
        videoParam.framerate = vid->framerate;
        videoParam.codec = vid->codec;
        videoParam.audio = vid->audio;
        videoParam.width = vid->width;
        videoParam.height = vid->height;
        videoParam.hash1 = vid->hash[0];
        videoParam.hash2 = vid->hash[1];

        videoParamList.append(videoParam);
    }

    QVERIFY(!videoParamList.isEmpty());
    QVERIFY(TestHelpers::saveVideoParamQListToCSV(videoParamList, _csvInfo_nocache, _videoDir));
    qDebug() << "TIMER:test_createRefVidParams_nocache took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
}*/

/*void TestVideo::createRefVidParams_cached()
{
    QElapsedTimer timer;
    timer.start();

    QVERIFY(!_csvInfo_cached.exists());    // we don't want to overwrite it !

    test_whole_app(); // run first to make sure all is cached

    QVERIFY(!_videoDir.isEmpty());
    MainWindow w;
    w.loadExtensions();
    QVERIFY(!w._extensionList.isEmpty());
    _videoDir.setNameFilters(w._extensionList);
    QDirIterator iter(_videoDir, QDirIterator::Subdirectories);

    QList<VideoParam> videoParamList;

    while(iter.hasNext())
    {
        const QFile vidFile(iter.next());
        const QFileInfo vidInfo = QFileInfo(vidFile);

        VideoParam videoParam;
        videoParam.videoInfo = vidInfo;

        Prefs prefs;
        Video *vid = new Video(prefs, vidInfo.absoluteFilePath(), true); // true to activate cache
        vid->run();

        // videos can be in sub folders !
        QString thumbPath = _thumbnailDir_cached.path() + "/" + vidInfo.absoluteFilePath().remove(_videoDir.path()+"/").replace('/','-').replace('\\','-') + "-" + vidInfo.fileName() + ".thumbnail";
        QFile thumbFile(thumbPath);
        QVERIFY2(!thumbFile.exists(), QString("Thumnail already exists %1").arg(thumbFile.fileName()).toStdString().c_str()); // we don't want to overwrite !
        thumbFile.open(QIODevice::WriteOnly);
        thumbFile.write(vid->thumbnail);
        thumbFile.close();
        QVERIFY2(thumbFile.exists(), QString("Thumnail couldn't be saved for %1").arg(thumbFile.fileName()).toStdString().c_str());

        videoParam.thumbnailInfo = QFileInfo(thumbFile);
        videoParam.size = vid->size;
        videoParam.modified = vid->modified;
        videoParam.duration = vid->duration;
        videoParam.bitrate = vid->bitrate;
        videoParam.framerate = vid->framerate;
        videoParam.codec = vid->codec;
        videoParam.audio = vid->audio;
        videoParam.width = vid->width;
        videoParam.height = vid->height;
        videoParam.hash1 = vid->hash[0];
        videoParam.hash2 = vid->hash[1];

        videoParamList.append(videoParam);
    }

    QVERIFY(!videoParamList.isEmpty());
    QVERIFY(TestHelpers::saveVideoParamQListToCSV(videoParamList, _csvInfo_cached, _videoDir));
    qDebug() << "TIMER:test_createRefVidParams_cached took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
}*/
// ---------------------------- END : smaller video sets tests ---------------------
// ------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------
// ---------------------------- START : 100GB tests from SSD ---------------------
/*void TestVideo::create100GBrefVidParams_nocache()
{
    QElapsedTimer timer;
    timer.start();

    QVERIFY(!_100GBvideoDir.isEmpty());
    QVERIFY(!_100GBcsvInfo_nocache.exists());    // we don't want to overwrite it !

    MainWindow w;
    w.loadExtensions();
    QVERIFY(!w._extensionList.isEmpty());
    _100GBvideoDir.setNameFilters(w._extensionList);
    QDirIterator iter(_100GBvideoDir, QDirIterator::Subdirectories);

    QList<VideoParam> videoParamList;

    while(iter.hasNext())
    {
        const QFile vidFile(iter.next());
        const QFileInfo vidInfo = QFileInfo(vidFile);

        VideoParam videoParam;
        videoParam.videoInfo = vidInfo;
//        qDebug() << "Video file path : "<< vidInfo.absoluteFilePath();
        QVERIFY2(vidInfo.exists(), QString("File not found %1").arg(vidInfo.absoluteFilePath()).toStdString().c_str());

        Prefs prefs;
        Video *vid = new Video(prefs, vidInfo.absoluteFilePath(), false);
        vid->run();

        // videos can be in sub folders !
        QString thumbPath = _100GBthumbnailDir_nocache.path() + "/" + vidInfo.absoluteFilePath().remove(_100GBvideoDir.path()+"/").replace('/','-').replace('\\','-') + "-" + vidInfo.fileName() + ".thumbnail";
        QFile thumbFile(thumbPath);
        QVERIFY2(!thumbFile.exists(), QString("Thumnail already exists %1").arg(thumbFile.fileName()).toStdString().c_str()); // we don't want to overwrite !
        thumbFile.open(QIODevice::WriteOnly);
        thumbFile.write(vid->thumbnail);
        thumbFile.close();
        QVERIFY2(thumbFile.exists(), QString("Thumnail couldn't be save for %1").arg(thumbFile.fileName()).toStdString().c_str());

        videoParam.thumbnailInfo = QFileInfo(thumbFile);
        videoParam.size = vid->size;
        videoParam.modified = vid->modified;
        videoParam.duration = vid->duration;
        videoParam.bitrate = vid->bitrate;
        videoParam.framerate = vid->framerate;
        videoParam.codec = vid->codec;
        videoParam.audio = vid->audio;
        videoParam.width = vid->width;
        videoParam.height = vid->height;
        videoParam.hash1 = vid->hash[0];
        videoParam.hash2 = vid->hash[1];

        videoParamList.append(videoParam);
    }

    qDebug() << "TIMER:test_100GBcreateRefVidParams_nocache took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
    QVERIFY(!videoParamList.isEmpty());
    QVERIFY(TestHelpers::saveVideoParamQListToCSV(videoParamList, _100GBcsvInfo_nocache, _100GBvideoDir));
}*/

// keep this so manually do things sometimes, it will load thumbnails from non cache
// saves, but it will try to compare to cached thumbnails if available !
// it is useful to see the difference with ENABLE_MANUAL_THUMBNAIL_VERIF to see
// the difference between cached thumnails and non cached thumbnails
void TestVideo::test_100GBcheckRefVidParams(){
    // cached thumbs, mix lib&exec metadata, exec captures : 39 min
    // no cached thumbs, library(only) metadata, exec captures : 37
    // cached thumbs, library(only) metadata, exec captures : 12 min
    // no cached thumbs, lib(only) metadata, lib(only) captures : 48 min
    // cached thumbs, lib(only) metadata, lib(only) captures : 12 min
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    const qint64 ref_ms_time = 50*60*1000;
#endif
    QElapsedTimer timer;
    timer.start();

    // read csv file
    QVERIFY(_100GBcsvInfo_nocache.exists());
    QList<VideoParam> videoParamList = TestHelpers::importCSVtoVideoParamQList(_100GBcsvInfo_nocache, _100GBvideoDir, _100GBthumbnailDir_nocache);
    QVERIFY(!videoParamList.isEmpty());

    // TODO : find a way to make this multithreaded, otherwise for all the videos it's too long !
    // compute params for all videos
    QVERIFY(!_100GBvideoDir.isEmpty());
    int test_nb = 0;
    const int nb_tests = videoParamList.count();
    foreach(VideoParam videoParam, videoParamList){

        QVERIFY2(videoParam.videoInfo.exists(), videoParam.videoInfo.absoluteFilePath().toUtf8().constData());
        QVERIFY2(videoParam.thumbnailInfo.exists(), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());

        //read thumbnail from disk
        QFile ref_thumbFile(videoParam.thumbnailInfo.absoluteFilePath());
        QVERIFY2(ref_thumbFile.open(QIODevice::ReadOnly), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());
        QByteArray ref_thumbnail = ref_thumbFile.readAll();
        ref_thumbFile.close();

        Prefs prefs;
        Video *vid = new Video(prefs, videoParam.videoInfo.absoluteFilePath(), Video::WITH_CACHE);
        vid->run();

        compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(ref_thumbnail, videoParam, vid);

        if(test_nb%100==0){
            qDebug() << "Processing "<<test_nb<<"/" << nb_tests << " for "<< videoParam.videoInfo.absoluteFilePath();
        }
        test_nb++;
    }

    qDebug() << "TIMER:test_100GBcheckRefVidParams took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_100GBcheckRefVidParams took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif
}

void TestVideo::test_100GBcheckRefVidParams_nocache(){
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    // 37 min: no cached thumbs, library(only) metadata, exec captures
    // 48 min: intel i5 no cached thumbs, lib(only) metadata, lib(only) captures
    // 26 min 36 s: arm m3 Pro with arm build & arm lib 2024 oct. with no thumbnail check
    const qint64 ref_ms_time = 28*60*1000;
#endif
    QElapsedTimer timer;
    timer.start();

    // read csv file
    QVERIFY(_100GBcsvInfo_nocache.exists());
    QList<VideoParam> videoParamList = TestHelpers::importCSVtoVideoParamQList(_100GBcsvInfo_nocache, _100GBvideoDir, _100GBthumbnailDir_nocache);
    QVERIFY(!videoParamList.isEmpty());

    // TODO : find a way to make this multithreaded, otherwise for all the videos it's too long !
    // compute params for all videos
    QVERIFY(!_100GBvideoDir.isEmpty());
    int test_nb = 0;
    const int nb_tests = videoParamList.count();
    foreach(VideoParam videoParam, videoParamList){

        QVERIFY2(videoParam.videoInfo.exists(), videoParam.videoInfo.absoluteFilePath().toUtf8().constData());
        QVERIFY2(videoParam.thumbnailInfo.exists(), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());

        //read thumbnail from disk
        QFile ref_thumbFile(videoParam.thumbnailInfo.absoluteFilePath());
        QVERIFY2(ref_thumbFile.open(QIODevice::ReadOnly), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());
        QByteArray ref_thumbnail = ref_thumbFile.readAll();
        ref_thumbFile.close();

        Prefs prefs;
        Video *vid = new Video(prefs, videoParam.videoInfo.absoluteFilePath(), Video::NO_CACHE);
        vid->run();

        compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(ref_thumbnail, videoParam, vid);

        if(test_nb%100==0){
            qDebug() << "Processing "<<test_nb<<"/" << nb_tests << " for "<< videoParam.videoInfo.absoluteFilePath();
        }
        test_nb++;
    }

    qDebug() << "TIMER:test_100GBcheckRefVidParams_nocache took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_100GBcheckRefVidParams_nocache took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif
}

void TestVideo::test_100GBcheckRefVidParams_cached(){
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    // 39 min: cached thumbs, mix lib&exec metadata, exec captures
    // 12 min: cached thumbs, library(only) metadata, exec captures
    // 12 min: cached thumbs, lib(only) metadata, lib(only) captures
    //
    const qint64 ref_ms_time = 50*60*1000;
#endif
    test_100GBwholeApp(); // make sure everything is cached

    QElapsedTimer timer;
    timer.start();

    // read csv file
    QVERIFY(_100GBcsvInfo_nocache.exists());
    QList<VideoParam> videoParamList = TestHelpers::importCSVtoVideoParamQList(_100GBcsvInfo_nocache, _100GBvideoDir, _100GBthumbnailDir_nocache);
    QVERIFY(!videoParamList.isEmpty());

    // TODO : find a way to make this multithreaded, otherwise for all the videos it's too long !
    // compute params for all videos
    QVERIFY(!_100GBvideoDir.isEmpty());
    int test_nb = 0;
    const int nb_tests = videoParamList.count();
    Prefs prefs;
    Db::initDbAndCacheLocation(prefs);
    foreach(VideoParam videoParam, videoParamList){

        QVERIFY2(videoParam.videoInfo.exists(), videoParam.videoInfo.absoluteFilePath().toUtf8().constData());
        QVERIFY2(videoParam.thumbnailInfo.exists(), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());

        //read thumbnail from disk
        QFile ref_thumbFile(videoParam.thumbnailInfo.absoluteFilePath());
        QVERIFY2(ref_thumbFile.open(QIODevice::ReadOnly), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());
        QByteArray ref_thumbnail = ref_thumbFile.readAll();
        ref_thumbFile.close();

        Video *vid = new Video(prefs, videoParam.videoInfo.absoluteFilePath(), Video::WITH_CACHE);
        vid->run();

        compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(ref_thumbnail, videoParam, vid);

        if(test_nb%100==0){
            qDebug() << "Processing "<<test_nb<<"/" << nb_tests << " for "<< videoParam.videoInfo.absoluteFilePath();
        }
        test_nb++;
    }

    qDebug() << "TIMER:test_100GBcheckRefVidParams_cached took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_100GBcheckRefVidParams_cached took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif
}

// used to create cache if not already available, but will use cache if already available !
// is used by test_100GBwholeApp_cached and test_100GBcheckRefVidParams_cachedNoThumbsCheck as first run,
// to make sure cache is loaded
void TestVideo::test_100GBwholeApp(){
    // Constants of the test
    const int nb_vids_to_find = 12505;
//    const int nb_valid_vids_to_find = 12328; // disable these two because of mismatch with cache or no caches
                                               // This could be due to thumbnails cached and not cached being different,
                                               // possibly looking black from cache but not originally, maybe ?
//    const int nb_matching_vids_to_find = 6558;
    // mix lib&exec metadata, exec captures : finds 6626 videos with one or more matches
    // no cached thumbs, lib(only) metadata, lib(only) captures :
    //                          finds 6562 videos with one or more matches.
    //                          When cached finds 6553...
    // after redo of caching :
    //      finds 12328 valid vids when not cached, but 12330 when cached... ?!
    //      finds 6558 matches with no cache (97.1GB) but 6550 matches when cached (97.0GB) ?!

    // no cached thumbs, mix lib&exec metadata, exec captures : 36 min
    // cached thumbs, mix lib&exec metadata, exec captures : 17 min
    // no cached thumbs, lib(only) metadata, exec captures : 30 min
    // cached thumbs, lib(only) metadata, exec captures : 6 min
    // no cached thumbs, lib(only) metadata, lib(only) captures : 17 min
    // cached thumbs, lib(only) metadata, lib(only) captures : 6 min
#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    const qint64 ref_ms_time = 20*60*1000;
#endif
    QElapsedTimer timer;
    timer.start();

    MainWindow w;
    w.ui->thresholdSlider->setValue(COMPARISON_THRESHOLD);
    w.show();

    QVERIFY(_100GBvideoDir.exists());

    w.findVideos(_100GBvideoDir);
    w.processVideos();
    w.ui->blocksizeCombo->setAcceptDrops(true);

    qDebug() << "Found "<<w._everyVideo.count()<<" files of which "<< w._videoList.count()<<" valid ones";
    qDebug() << "TIMER:test_whole_app_100GB before match report took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    Comparison comp(w.sortVideosBySize(), w._prefs);
    int matchingVideoNb = comp.reportMatchingVideos();
    qDebug() << QString("Found %1 matching vids").arg(matchingVideoNb);

    qDebug() << "TIMER:test_whole_app_100GB took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    QVERIFY2(w._everyVideo.count()==nb_vids_to_find, QString("Found %1 files but should be %2").arg(w._everyVideo.count()).arg(nb_vids_to_find).toStdString().c_str());
    // Two below have problems because different if cached or not... this doesn't seem normal...
//    QVERIFY2(w._videoList.count()==nb_valid_vids_to_find, QString("Found %1 valid files but should be %2").arg(w._videoList.count()).arg(nb_valid_vids_to_find).toStdString().c_str());
//    QVERIFY2(matchingVideoNb==nb_matching_vids_to_find, QString("Found %1 matching vids but should be %2").arg(matchingVideoNb).arg(nb_matching_vids_to_find).toStdString().c_str());

#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_whole_app_100GB took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif

    comp.show();

    QTest::qWait(1000);
    comp.on_nextVideo_clicked();
    comp.ui->tabWidget->setCurrentIndex(2);
    QTest::qWait(1000);
    w.destroy();
};

void TestVideo::test_100GBwholeApp_nocache(){
    const int nb_vids_to_find = 12505;

    // 12 328 after redo of caching
    // 12 330 arm m3 Pro with arm build & arm lib 2024 oct.
    const int nb_valid_vids_to_find = 12330;

    // Number of vids to find with one or more matches when not cached
    //      6626 videos with one or more matches : mix lib&exec metadata, exec captures
    //      6562 videos with one or more matches: lib(only) metadata, lib(only) captures
    //      6558 matches (97.1GB) arm but intel build
    //      6568 video(s) (97.2 GB) arm m3 Pro with arm build & arm lib 2024 oct.
    const int nb_matching_vids_to_find = 6568;

#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    // 36 min: mix lib&exec metadata, exec captures
    // 30 min: lib(only) metadata, exec captures
    // 17 min: no cached thumbs, lib(only) metadata, lib(only) captures
    // 17 min: lib(only) metadata, lib(only) captures
    // 6 min 30s: arm m3 Pro with arm build & arm lib 2024 oct.
    const qint64 ref_ms_time = 7*60*1000;
#endif
    QElapsedTimer timer;
    timer.start();

    MainWindow w;
    w.ui->thresholdSlider->setValue(COMPARISON_THRESHOLD);
    w.show();
    w.ui->radio_UseCacheNo->click(); // disable loading from and saving to cache

    QVERIFY(_100GBvideoDir.exists());

    w.findVideos(_100GBvideoDir);
    w.processVideos();
    w.ui->blocksizeCombo->setAcceptDrops(true);

    qDebug() << "Found "<<w._everyVideo.count()<<" files of which "<< w._videoList.count()<<" valid ones";
    qDebug() << "TIMER:test_wholeApp100GB_nocache before match report took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    Comparison comp(w.sortVideosBySize(), w._prefs);
    int matchingVideoNb = comp.reportMatchingVideos();
    qDebug() << QString("Found %1 matching vids").arg(matchingVideoNb);

    qDebug() << "TIMER:test_wholeApp100GB_nocache took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    QVERIFY2(w._everyVideo.count()==nb_vids_to_find, QString("Found %1 files but should be %2").arg(w._everyVideo.count()).arg(nb_vids_to_find).toStdString().c_str());
    QVERIFY2(w._videoList.count()==nb_valid_vids_to_find, QString("Found %1 valid files but should be %2").arg(w._videoList.count()).arg(nb_valid_vids_to_find).toStdString().c_str());
    QVERIFY2(matchingVideoNb==nb_matching_vids_to_find, QString("Found %1 matching vids but should be %2").arg(matchingVideoNb).arg(nb_matching_vids_to_find).toStdString().c_str());

#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_wholeApp100GB_nocache took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif

    comp.show();

    QTest::qWait(1000);
    comp.on_nextVideo_clicked();
    comp.ui->tabWidget->setCurrentIndex(2);
    QTest::qWait(1000);
    w.destroy();
}

void TestVideo::test_100GBwholeApp_cached(){
    const int nb_vids_to_find = 12505;

    // 12 330 after redo of caching
    // 12 330 arm m3 Pro with arm build & arm lib 2024 oct.
    const int nb_valid_vids_to_find = 12330;

    // Number of vids to find with one or more matches when cached
    //      6626: mix lib&exec metadata, exec captures
    //      6553: lib(only) metadata, lib(only) captures
    //      6550 (97.1GB): after redo of caching
    //      6555 (97.1 GB): arm with arm build & arm lib oct. 2024
    const int nb_matching_vids_to_find = 6555;

#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    // 17 min: cached thumbs, mix lib&exec metadata, exec captures
    // 6 min: cached thumbs, lib(only) metadata, exec captures
    // 6 min: cached thumbs, lib(only) metadata, lib(only) captures
    // 2 min 33 s: arm m3 Pro with arm build & arm lib 2024 oct.
    const qint64 ref_ms_time = 3*60*1000;
#endif
    test_100GBwholeApp(); // run a first time to make sure everything is cached

    QElapsedTimer timer;
    timer.start();

    MainWindow w;
    w.ui->thresholdSlider->setValue(COMPARISON_THRESHOLD);
    w.show();
    w.ui->radio_UseCacheYes->click(); // enable loading from and saving to cache (should be on by default)

    QVERIFY(_100GBvideoDir.exists());

    w.findVideos(_100GBvideoDir);
    w.processVideos();
    w.ui->blocksizeCombo->setAcceptDrops(true);

    qDebug() << "Found "<<w._everyVideo.count()<<" files of which "<< w._videoList.count()<<" valid ones";
    qDebug() << "TIMER:test_wholeApp100GB_cached before match report took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    Comparison comp(w.sortVideosBySize(), w._prefs);
    int matchingVideoNb = comp.reportMatchingVideos();
    qDebug() << QString("Found %1 matching vids").arg(matchingVideoNb);

    qDebug() << "TIMER:test_wholeApp100GB_cached took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    QVERIFY2(w._everyVideo.count()==nb_vids_to_find, QString("Found %1 files but should be %2").arg(w._everyVideo.count()).arg(nb_vids_to_find).toStdString().c_str());
    QVERIFY2(w._videoList.count()==nb_valid_vids_to_find, QString("Found %1 valid files but should be %2").arg(w._videoList.count()).arg(nb_valid_vids_to_find).toStdString().c_str());
    QVERIFY2(matchingVideoNb==nb_matching_vids_to_find, QString("Found %1 matching vids but should be %2").arg(matchingVideoNb).arg(nb_matching_vids_to_find).toStdString().c_str());

#ifndef ENABLE_MANUAL_THUMBNAIL_VERIF
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_wholeApp100GB_cached took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
#endif

    comp.show();

    QTest::qWait(1000);
    comp.on_nextVideo_clicked();
    comp.ui->tabWidget->setCurrentIndex(2);
    QTest::qWait(1000);
    w.destroy();
}

// ---------------------------- END : 100GB tests from SSD ----------------------------
// ------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------
// ---------------------------- START : helper testing functions ----------------------
void TestVideo::compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(
    const QByteArray ref_thumbnail,
    const VideoParam videoParam,
    const Video *vid,
    const bool compareThumbs
    ) {
    QVERIFY2(videoParam.size == vid->size, QString("ref size=%1 new size=%2").arg(videoParam.size).arg(vid->size).toUtf8().constData());
    // TODO finish 100GB tests with QVERIFY2(videoParam.modified.toString(VideoParam::timeformat) == vid->modified.toString(VideoParam::timeformat) , QString("Date diff : ref modified=%1 new modified=%2").arg(videoParam.modified.toString(VideoParam::timeformat)).arg(vid->modified.toString(VideoParam::timeformat)).toUtf8().constData());
    QVERIFY2(videoParam.modified.toString(VideoParam::timeformat) == vid->modified.toString(VideoParam::timeformat) , QString("Date diff : ref modified=%1 new modified=%2").arg(videoParam.modified.toString(VideoParam::timeformat)).arg(vid->modified.toString(VideoParam::timeformat)).toUtf8().constData());
    // TODO finish 100GB tests with QVERIFY2(abs(videoParam.duration - vid->duration) <= 1 , QString("ref duration=%1 new duration=%2 for %3").arg(videoParam.duration).arg(vid->duration).arg(videoParam.videoInfo.absoluteFilePath()).toUtf8().constData());
    QVERIFY2(videoParam.duration == vid->duration, QString("ref duration=%1 new duration=%2 for %3").arg(videoParam.duration).arg(vid->duration).arg(videoParam.videoInfo.absoluteFilePath()).toUtf8().constData());
    QVERIFY2(videoParam.bitrate == vid->bitrate, QString("ref bitrate=%1 new bitrate=%2").arg(videoParam.bitrate).arg(vid->bitrate).toUtf8().constData());
    QVERIFY2(videoParam.framerate == vid->framerate, QString("framerate ref=%1 new=%2").arg(videoParam.framerate).arg(vid->framerate).toUtf8().constData());
    QVERIFY2(videoParam.codec == vid->codec, QString("codec ref=%1 new=%2").arg(videoParam.codec).arg(vid->codec).toUtf8().constData());
#ifdef ENABLE_AUDIO_COMPARISON
    QVERIFY2(videoParam.audio == vid->audio, QString("audio ref=%1 new=%2").arg(videoParam.audio).arg(vid->audio).toUtf8().constData());
#endif
    QVERIFY2(videoParam.width == vid->width, QString("width x height ref=%1x%2 new=%3x%4").arg(videoParam.width).arg(videoParam.height).arg(vid->width).arg(vid->height).toUtf8().constData());
    QVERIFY2(videoParam.height == vid->height, QString("width x height ref=%1x%2 new=%3x%4").arg(videoParam.width).arg(videoParam.height).arg(vid->width).arg(vid->height).toUtf8().constData());

#ifdef ENABLE_THUMBNAIL_VERIF
    QVERIFY2(!(!ref_thumbnail.isNull() && vid->thumbnail.isNull()),  QString("Ref thumb not empty but new thumb is empty file %1").arg(videoParam.thumbnailInfo.absoluteFilePath()).toStdString().c_str());
    bool manuallyAccepted = false;
    if(ref_thumbnail != vid->thumbnail){
#ifdef ENABLE_MANUAL_THUMBNAIL_VERIF
        if(TestHelpers::doThumbnailsLookSameWindow(ref_thumbnail,
                                                    vid->thumbnail,
                                                    QString("Thumbnail %1").arg(videoParam.thumbnailInfo.absoluteFilePath())))
            manuallyAccepted = true;
        else
#endif
            QVERIFY2(compareThumbs==false, QString("Different thumbnails for %1").arg(videoParam.thumbnailInfo.absoluteFilePath()).toUtf8().constData());
    }
#ifdef ENABLE_HASHES_VERIFICATION
    if(manuallyAccepted == false && compareThumbs){ // hash will be different anyways if thumbnails look different, so must skip these tests
        QVERIFY2(videoParam.hash1 == vid->hash[0], QString("ref hash1=%1 new hash1=%2").arg(videoParam.hash1).arg(vid->hash[0]).toUtf8().constData());
        QVERIFY2(videoParam.hash2 == vid->hash[1], QString("ref hash2=%1 new hash2=%2").arg(videoParam.hash2).arg(vid->hash[1]).toUtf8().constData());
    }
#endif
    if(manuallyAccepted == true){ // on met a jour le thumbnail enregistré sur le disque
        qDebug() << "Updating thumbnail for  " + videoParam.thumbnailInfo.fileName();
        QFile thumbFile(videoParam.thumbnailInfo.canonicalFilePath());
        thumbFile.open(QIODevice::WriteOnly);
        thumbFile.write(vid->thumbnail);
        thumbFile.close();
        QVERIFY2(thumbFile.exists(), QString("Thumnail couldn't be updated for %1").arg(thumbFile.fileName()).toStdString().c_str());
    }
#endif
}

// ---------------------------- END : helper testing functions ------------------------
// ----------------------------------------------------------------------------------
QTEST_MAIN(TestVideo)

#include "tst_video.moc"
