#include <QtTest>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrent>
#include <QElapsedTimer>

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
    void test_check_refvidparams_nocache_exact_identical();
    void test_check_refvidparams_nocache_manualCompare10Sampled_AcceptSmallDurationDiffs();
    void test_check_refvidparams_nocache_noThumbHashesCheck();
    void test_check_refvidparams_nocache_noVisualThumbCheck();

    void test_check_refvidparams_cached_exact_identical();
    void test_check_refvidparams_cached_manualCompare10Sampled_AcceptSmallDurationDiffs();
    void test_check_refvidparams_cached_noThumbHashesCheck();
    void test_check_refvidparams_cached_noVisualThumbCheck();

    void test_whole_app_nocache();
    void test_whole_app_cached();
    void test_whole_app_cache_only();

//    void create100GBrefVidParams_nocache();
    void test_100GBcheckRefVidParams_nocache_noVisualThumbCheck();
    void test_100GBcheckRefVidParams_nocache_noVisualThumbCheck_AcceptSmallDurationDiffs_ignoreModifiedDates();
    void test_100GBcheckRefVidParams_nocache_manualCompare500Sampled_AcceptSmallDurationDiffs();

    void test_100GBcheckRefVidParams_cache_manualCompare500Sampled_AcceptSmallDurationDiffs();
    void test_100GBcheckRefVidParams_cache_noVisualThumbCheck();
    void test_100GBcheckRefVidParams_cache_noVisualThumbCheck_AcceptSmallDurationDiffs_ignoreModifiedDates();

    void test_100GBwholeApp_nocache();
    void test_100GBwholeApp_cached();

    // Runs at end of all test run (not once per test)
    void cleanupTestCase(){};

private:
    class wholeAppTestConfig
    {
    public:
        Prefs::USE_CACHE_OPTION cacheOption;

        // inside the app it default to 100, but for tests it's could be interesting if lower
        uint videoSimilarityThreshold = 100;

        /* Small test set
         *  - No cache
         *      -- 207: before remove big files
         *      -- 200
         *  - Cached
         *      -- 207: before remove big files
         *      -- 200
         * 100GB test set
         *  - No cache
         *      -- 12 505
         *  - Cached
         *      -- 12 505
        */
        int nb_vids_to_find;

        /* Small test set
         *  - No cache
         *      -- 204: before remove big file tests
         *      -- 197
         *  - Cached
         *      -- 204: before remove big file tests
         *      -- 197
         *  - Cache only
         *      -- 204: before remove big file tests
         *      -- 197
         * 100GB test set
         *  - No cache
         *      -- 12 328: after redo of caching
         *      -- 12 330: arm m3 Pro with arm build & arm lib - 2024 oct.
         *  - Cached
         *      -- 12 330: after redo of caching
         *      -- 12 330: arm m3 Pro with arm build & arm lib - 2024 oct.
         */
        int nb_valid_vids_to_find;

        /* Small test set
         *  - No cache
         *      -- 71
         *  - Cached
         *      -- 72
         *  - Cache only
         *      -- 74: before remove big file tests
         *      -- 72
         * 100GB test set
         *  - No cache
         *      -- 6 626: mix lib&exec metadata, exec captures
         *      -- 6 562: lib(only) metadata, lib(only) captures
         *      -- 6 558 (97.1GB): arm but intel build
         *      -- 6 568 (97.2 GB): arm m3 Pro with arm build & arm lib - 2024 oct.
         *  - Cached
         *      -- 6 626: mix lib&exec metadata, exec captures
         *      -- 6 553: lib(only) metadata, lib(only) captures
         *      -- 6 550 (97.1GB): after redo of caching
         *      -- 6 555 (97.1 GB): arm m3 Pro with arm build & arm lib - 2024 oct.
         */
        int nb_matching_vids_to_find;

        /* Small test set
         *  - No cache
         *      -- 13.5 sec: windows intel on intel (before remove big file tests)
         *      -- 13 sec: macOS intel on intel (before remove big file tests)
         *      -- 6 sec: macOS arm on M1
         *      -- 3.122 sec (3.37, 2.995,...): arm m3 Pro with arm build & arm lib - 2024 oct
         *  - Cached
         *      -- 2.75 sec: windows intel on intel (before remove big file tests 207)
         *      -- 3 sec: macOS intel on intel (before remove big file tests 207)
         *      -- 1 sec: macOS arm on M1
         *      -- 650 ms (0.572, 0.570,...): arm m3 Pro with arm build & arm lib - 2024 oct.
         *  - Cache only
         *      -- 3.203 sec: macos intel on intel (before remove big file tests 207)
         *      -- <1 sec: macOS arm on M1
         *      -- <1 sec (0.566, 0.538,...): arm m3 Pro with arm build & arm lib - 2024 oct.
         * 100GB test set
         *  - No cache
         *      -- 36 min: mix lib&exec metadata, exec captures
         *      -- 30 min: lib(only) metadata, exec captures
         *      -- 17 min: lib(only) metadata, lib(only) captures
         *      -- 17 min: lib(only) metadata, lib(only) captures
         *      -- 6 min 30s (418s, ...): arm m3 Pro with arm build & arm lib - 2024 oct.
         *  - Cached
         *      -- 17 min: mix lib&exec metadata, exec captures
         *      -- 6 min: lib(only) metadata, exec captures
         *      -- 6 min: lib(only) metadata, lib(only) captures
         *      -- 2 min 33 s: arm m3 Pro with arm build & arm lib - 2024 oct.
         *  */
        qint64 ref_ms_time;
    };

    class refVidParamsTestConfig
    {
    private:
        class visualCompareTestConfig;

    public:
        refVidParamsTestConfig() {}

        QString testDescr;
        QFileInfo paramsCSV;
        QDir videoDir;
        QDir thumbsDir;

        /* Thumbnails are generated without cache : must run test without cache (clean all before)
         * Also sometimes, for some unknown reason, thumbnails don't come out the same.
         * But if you re-run tests a few times, it should get fixed
         * (or check visually with compareThumbsVisualConfig) */
        Prefs::USE_CACHE_OPTION cacheOption;

        /* Small test set
         *  - No cache
         *      -- 35 s: macOS intel on intel (before remove big file tests)
         *      -- 36 s: windows intel on intel (before remove big file tests)
         *      -- 17 s: macOS arm m3 Pro with arm build & arm lib - 2024 oct.
         *  - Cached
         *      -- 8 s: macOS intel on intel (before remove big file tests)
         *      -- 9 s: windows intel on intel (before remove big file tests)
         *      -- 2.5 s: macOS arm m3 Pro with arm build & arm lib (2'494ms, 2'606ms, ... so cap around 5s) - 2024 oct.
         * 100GB test set
         *  - No cache
         *      -- 37 min: library(only) metadata, exec captures
         *      -- 48 min: intel i5 lib(only) metadata, lib(only) captures
         *      -- 26 min 36 s: arm m3 Pro with arm build & arm lib - 2024 oct.
         *  - Cached
         *      -- 39 min: mix lib&exec metadata, exec captures
         *      -- 12 min: library(only) metadata, exec captures
         *      -- 12 min: lib(only) metadata, lib(only) captures
         *      - 3 min 27 s: arm m3 Pro with arm build & arm lib (no thumb check) - 2024 oct. */
        qint64 refDuration_ms;

        // inside the app it default to 100, but for tests it's could be interesting if lower
        uint videoSimilarityThreshold = 100;

        // if not set, thumbnail visual details will not be checked
        visualCompareTestConfig* compareThumbsVisualConfig = new visualCompareTestConfig();

        // When moving over to library, audio metadata sometimes changes but when manually checked, is actually identical
        // Disable following define to skip testing audio comparison
        bool compareAudio = true;

        bool compareModifiedDates = true;
        bool acceptSmallDurationDiff = false;

    private:
        class visualCompareTestConfig
        {
        public:
            visualCompareTestConfig() {}

            bool manualCompareIfThumbsVisualDiff = false;

            // Sometimes hashes go crazy, so we can manually disable them to see if other problems exist
            bool compareThumbHashes = true;

            uint sampledManualThumbVerifInterval = 1;
        };
    };

    /*
     * Runs the whole app for the given folder, by default using the cache but not checking any details
     * If provided a conf, it will make sure to test the chosen cache option, and verify the number of elements found
    */
    void runWholeAppScan(QDir videoDir, wholeAppTestConfig *conf = nullptr);
    void checkRefVidParamsList(const refVidParamsTestConfig conf);
    static void compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(
        const QByteArray ref_thumbnail,
        const VideoParam videoParam,
        const Video *vid,
        const refVidParamsTestConfig conf = refVidParamsTestConfig()
    );
};

TestVideo::TestVideo(){}

TestVideo::~TestVideo(){}

// Runs once before test run (not once per test)
void TestVideo::initTestCase(){
    qSetMessagePattern("%{file}(%{line}) %{function}: %{message}");
    Prefs().resetSettings();
    qDebug() << "Cleared settings";
}

// ------------------------------------------------------------------------------------
// ---------------------------- START : smaller video sets tests ---------------------

// Test whole app
void TestVideo::test_whole_app_nocache(){
    wholeAppTestConfig conf;
    conf.cacheOption = Prefs::NO_CACHE;
    conf.ref_ms_time = 3.5*1000;
    conf.nb_vids_to_find = 200;
    conf.nb_valid_vids_to_find = 197;
    conf.nb_matching_vids_to_find = 71;
    runWholeAppScan(
        _videoDir,
        &conf
    );
}

void TestVideo::test_whole_app_cached(){
    wholeAppTestConfig conf;
    conf.cacheOption = Prefs::WITH_CACHE;
    conf.ref_ms_time = 5*1000;
    conf.nb_vids_to_find = 200;
    conf.nb_valid_vids_to_find = 197;
    conf.nb_matching_vids_to_find = 72;
    runWholeAppScan(
        _videoDir,
        &conf
        );
}

void TestVideo::test_whole_app_cache_only(){
        wholeAppTestConfig conf;
        conf.cacheOption = Prefs::CACHE_ONLY;
        conf.ref_ms_time = 1*1000;
        conf.nb_vids_to_find = 200;
        conf.nb_valid_vids_to_find = 197;
        conf.nb_matching_vids_to_find = 72;
        runWholeAppScan(
            _videoDir,
            &conf
        );
    }

// Check ref params with no cache
void TestVideo::test_check_refvidparams_nocache_exact_identical(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::NO_CACHE;
    conf.paramsCSV = _csvInfo_nocache;
    conf.thumbsDir = _thumbnailDir_nocache;
    conf.videoDir = _videoDir;
    conf.refDuration_ms = 20*1000;
    checkRefVidParamsList(conf);
}

void TestVideo::test_check_refvidparams_nocache_manualCompare10Sampled_AcceptSmallDurationDiffs(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::NO_CACHE;
    conf.paramsCSV = _csvInfo_nocache;
    conf.thumbsDir = _thumbnailDir_nocache;
    conf.videoDir = _videoDir;
    conf.acceptSmallDurationDiff = true;
    conf.compareThumbsVisualConfig->manualCompareIfThumbsVisualDiff = true;
    conf.compareThumbsVisualConfig->sampledManualThumbVerifInterval = 10;
    checkRefVidParamsList(conf);
}

void TestVideo::test_check_refvidparams_nocache_noThumbHashesCheck(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::NO_CACHE;
    conf.paramsCSV = _csvInfo_nocache;
    conf.thumbsDir = _thumbnailDir_nocache;
    conf.videoDir = _videoDir;
    conf.compareThumbsVisualConfig->compareThumbHashes = false;
    conf.refDuration_ms = 20*1000;
    checkRefVidParamsList(conf);
}

void TestVideo::test_check_refvidparams_nocache_noVisualThumbCheck(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::NO_CACHE;
    conf.paramsCSV = _csvInfo_nocache;
    conf.thumbsDir = _thumbnailDir_nocache;
    conf.videoDir = _videoDir;
    delete conf.compareThumbsVisualConfig;
    conf.compareThumbsVisualConfig = nullptr;
    conf.refDuration_ms = 20*1000;
    checkRefVidParamsList(conf);
}

// Check ref params with cache
void TestVideo::test_check_refvidparams_cached_exact_identical(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::WITH_CACHE;
    conf.paramsCSV = _csvInfo_nocache;
    conf.thumbsDir = _thumbnailDir_nocache;
    conf.videoDir = _videoDir;
    conf.refDuration_ms = 5*1000;
    checkRefVidParamsList(conf);
}

void TestVideo::test_check_refvidparams_cached_manualCompare10Sampled_AcceptSmallDurationDiffs(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::WITH_CACHE;
    conf.paramsCSV = _csvInfo_nocache;
    conf.thumbsDir = _thumbnailDir_nocache;
    conf.videoDir = _videoDir;
    conf.acceptSmallDurationDiff = true;
    conf.compareThumbsVisualConfig->manualCompareIfThumbsVisualDiff = true;
    conf.compareThumbsVisualConfig->sampledManualThumbVerifInterval = 10;
    checkRefVidParamsList(conf);
}

void TestVideo::test_check_refvidparams_cached_noThumbHashesCheck(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::WITH_CACHE;
    conf.paramsCSV = _csvInfo_nocache;
    conf.thumbsDir = _thumbnailDir_nocache;
    conf.videoDir = _videoDir;
    conf.compareThumbsVisualConfig->compareThumbHashes = false;
    conf.refDuration_ms = 5*1000;
    checkRefVidParamsList(conf);
}

void TestVideo::test_check_refvidparams_cached_noVisualThumbCheck(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::WITH_CACHE;
    conf.paramsCSV = _csvInfo_nocache;
    conf.thumbsDir = _thumbnailDir_nocache;
    conf.videoDir = _videoDir;
    delete conf.compareThumbsVisualConfig;
    conf.compareThumbsVisualConfig = nullptr;
    conf.refDuration_ms = 5*1000;
    checkRefVidParamsList(conf);
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

// Test whole app
void TestVideo::test_100GBwholeApp_nocache(){
    wholeAppTestConfig conf;
    conf.cacheOption = Prefs::NO_CACHE;
    conf.ref_ms_time = 7*60*1000;
    conf.nb_vids_to_find = 12505;
    conf.nb_valid_vids_to_find = 12330;
    conf.nb_matching_vids_to_find = 6568;
    runWholeAppScan(
        _100GBvideoDir,
        &conf
        );
}

void TestVideo::test_100GBwholeApp_cached(){
    wholeAppTestConfig conf;
    conf.cacheOption = Prefs::WITH_CACHE;
    conf.ref_ms_time = 3*60*1000;
    conf.nb_vids_to_find = 12505;
    conf.nb_valid_vids_to_find = 12330;
    conf.nb_matching_vids_to_find = 6555;
    runWholeAppScan(
        _100GBvideoDir,
        &conf
        );
}

// Check ref params with no cache
void TestVideo::test_100GBcheckRefVidParams_nocache_manualCompare500Sampled_AcceptSmallDurationDiffs(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::NO_CACHE;
    conf.paramsCSV = _100GBcsvInfo_nocache;
    conf.videoDir = _100GBvideoDir;
    conf.thumbsDir = _100GBthumbnailDir_nocache;
    conf.acceptSmallDurationDiff = true;
    conf.compareModifiedDates = false;
    conf.compareThumbsVisualConfig->manualCompareIfThumbsVisualDiff = true;
    conf.compareThumbsVisualConfig->sampledManualThumbVerifInterval = 500;
    checkRefVidParamsList(conf);
}

void TestVideo::test_100GBcheckRefVidParams_nocache_noVisualThumbCheck(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::NO_CACHE;
    conf.paramsCSV = _100GBcsvInfo_nocache;
    conf.videoDir = _100GBvideoDir;
    conf.thumbsDir = _100GBthumbnailDir_nocache;
    delete conf.compareThumbsVisualConfig;
    conf.compareThumbsVisualConfig = nullptr;
    conf.refDuration_ms = 28*60*1000;

    checkRefVidParamsList(conf);
}

void TestVideo::test_100GBcheckRefVidParams_nocache_noVisualThumbCheck_AcceptSmallDurationDiffs_ignoreModifiedDates(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::NO_CACHE;
    conf.paramsCSV = _100GBcsvInfo_nocache;
    conf.videoDir = _100GBvideoDir;
    conf.thumbsDir = _100GBthumbnailDir_nocache;
    conf.refDuration_ms = 28*60*1000;
    conf.acceptSmallDurationDiff = true;
    conf.compareModifiedDates = false;
    delete conf.compareThumbsVisualConfig;
    conf.compareThumbsVisualConfig = nullptr;

    checkRefVidParamsList(conf);
}

// Check ref params with cache
void TestVideo::test_100GBcheckRefVidParams_cache_manualCompare500Sampled_AcceptSmallDurationDiffs(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::WITH_CACHE;
    conf.paramsCSV = _100GBcsvInfo_nocache;
    conf.videoDir = _100GBvideoDir;
    conf.thumbsDir = _100GBthumbnailDir_nocache;
    conf.acceptSmallDurationDiff = true;
    conf.compareThumbsVisualConfig->manualCompareIfThumbsVisualDiff = true;
    conf.compareThumbsVisualConfig->sampledManualThumbVerifInterval = 500;
    checkRefVidParamsList(conf);
}

void TestVideo::test_100GBcheckRefVidParams_cache_noVisualThumbCheck(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::WITH_CACHE;
    conf.paramsCSV = _100GBcsvInfo_nocache;
    conf.videoDir = _100GBvideoDir;
    conf.thumbsDir = _100GBthumbnailDir_nocache;
    delete conf.compareThumbsVisualConfig;
    conf.compareThumbsVisualConfig = nullptr;
    conf.refDuration_ms = 4*60*1000;

    checkRefVidParamsList(conf);
}

void TestVideo::test_100GBcheckRefVidParams_cache_noVisualThumbCheck_AcceptSmallDurationDiffs_ignoreModifiedDates(){
    refVidParamsTestConfig conf;
    conf.testDescr = __FUNCTION__;
    conf.cacheOption = Prefs::WITH_CACHE;
    conf.paramsCSV = _100GBcsvInfo_nocache;
    conf.videoDir = _100GBvideoDir;
    conf.thumbsDir = _100GBthumbnailDir_nocache;
    conf.refDuration_ms = 4*60*1000;
    conf.acceptSmallDurationDiff = true;
    conf.compareModifiedDates = false;
    delete conf.compareThumbsVisualConfig;
    conf.compareThumbsVisualConfig = nullptr;

    checkRefVidParamsList(conf);
}

// ---------------------------- END : 100GB tests from SSD ----------------------------
// ------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------
// ---------------------------- START : helper testing functions ----------------------
void TestVideo::runWholeAppScan(
        QDir videoDir,
        wholeAppTestConfig *conf
    ){
    QVERIFY(videoDir.exists());

    MainWindow w = MainWindow();
    w.show();

    if(conf != nullptr){
        w.on_thresholdSlider_valueChanged(conf->videoSimilarityThreshold);

        switch (conf->cacheOption) {
        case Prefs::CACHE_ONLY:
            runWholeAppScan(videoDir); // run once to make sure all is cached
            w.ui->radio_UseCacheOnly->click();
            break;
        case Prefs::WITH_CACHE:
            runWholeAppScan(videoDir);
            w.ui->radio_UseCacheYes->click();
            break;
        case Prefs::NO_CACHE:
            w.ui->radio_UseCacheNo->click();
        default:
            break;
        }
    }
    else
        w.ui->radio_UseCacheYes->click();

    QElapsedTimer timer;
    timer.start();

    w.findVideos(videoDir);
    w.processVideos();

    qDebug() << "runWholeAppScan found "<< w._everyVideo.count() << " files of which " << w._videoList.count() << " valid ones";
    qDebug() << "runWholeAppScan before match report took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    Comparison comp(w.sortVideosBySize(), w._prefs);
    int matchingVideoNb = comp.reportMatchingVideos();
    qDebug() << "runWholeAppScan found " << matchingVideoNb << " matching vids";

    qDebug() << "runWholeAppScan took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";

    if(conf != nullptr) {
        QVERIFY2(w._everyVideo.count()==conf->nb_vids_to_find, QString("runWholeAppScan found %1 files but should be %2").arg(w._everyVideo.count()).arg(conf->nb_vids_to_find).toStdString().c_str());
        QVERIFY2(w._videoList.count()==conf->nb_valid_vids_to_find, QString("runWholeAppScan found %1 valid files but should be %2").arg(w._videoList.count()).arg(conf->nb_valid_vids_to_find).toStdString().c_str());
        QVERIFY2(matchingVideoNb==conf->nb_matching_vids_to_find, QString("runWholeAppScan found %1 matching vids but should be %2").arg(matchingVideoNb).arg(conf->nb_matching_vids_to_find).toStdString().c_str());
        QVERIFY2(timer.elapsed()<conf->ref_ms_time, QString("runWholeAppScan took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(conf->ref_ms_time/1000).arg(conf->ref_ms_time%1000).toStdString().c_str());
    }

    w.close();
    QCoreApplication::processEvents();
}

void TestVideo::checkRefVidParamsList(
        const refVidParamsTestConfig conf
    ){
    QVERIFY(conf.paramsCSV.exists());
    QVERIFY(conf.thumbsDir.exists());
    QVERIFY(conf.videoDir.exists());

    if(conf.cacheOption != Prefs::NO_CACHE)
        runWholeAppScan(conf.videoDir); // create the cache if it didn't exist before

    QElapsedTimer timer;
    timer.start();

    // read csv file
    QList<VideoParam> videoParamList = TestHelpers::importCSVtoVideoParamQList(conf.paramsCSV, conf.videoDir, conf.thumbsDir);
    QVERIFY(!videoParamList.isEmpty());

    // compute params for all videos
    Prefs prefs;
    Db::initDbAndCacheLocation(prefs);

    int test_nb = 0;
    const int nb_tests = videoParamList.count();
    foreach(VideoParam videoParam, videoParamList){
        if(test_nb%100==0){
            qDebug() << "Processing "<<test_nb<<"/" << nb_tests << " for "<< videoParam.videoInfo.absoluteFilePath();
        }
        test_nb++;

        QVERIFY2(videoParam.videoInfo.exists(), videoParam.videoInfo.absoluteFilePath().toUtf8());
        QVERIFY2(videoParam.thumbnailInfo.exists(), videoParam.thumbnailInfo.absoluteFilePath().toUtf8());

        QFile ref_thumbFile(videoParam.thumbnailInfo.absoluteFilePath());
        QVERIFY2(ref_thumbFile.open(QIODevice::ReadOnly), videoParam.thumbnailInfo.absoluteFilePath().toUtf8());
        QByteArray ref_thumbnail = ref_thumbFile.readAll();
        ref_thumbFile.close();

        Video *vid = new Video(prefs, videoParam.videoInfo.absoluteFilePath());
        vid->run();

        if(conf.compareThumbsVisualConfig != nullptr
            && (test_nb % conf.compareThumbsVisualConfig->sampledManualThumbVerifInterval != 0)) {
            continue;
        }
        compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(ref_thumbnail, videoParam, vid, conf); // because we're loading cached thumbnails, we can compare thumbnails !
        delete vid;
    }

    qDebug() << "TIMER:"<<conf.testDescr<<" took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
    if(conf.compareThumbsVisualConfig == nullptr)
        QVERIFY2(timer.elapsed()<conf.refDuration_ms, QString("%1 took : %2.%3s but should be below %4.%5s").arg(conf.testDescr).arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(conf.refDuration_ms/1000).arg(conf.refDuration_ms%1000).toUtf8());
}

void TestVideo::compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(
    const QByteArray ref_thumbnail,
    const VideoParam videoParam,
    const Video *vid,
    const refVidParamsTestConfig conf
    ) {
    const QString forVid = QString("For %1").arg(videoParam.thumbnailInfo.absoluteFilePath());

    QVERIFY2(videoParam.size == vid->size, QString("ref size=%1 new size=%2 - %3").arg(videoParam.size).arg(vid->size).arg(forVid).toUtf8());
    if(conf.compareModifiedDates)
        QVERIFY2(videoParam.modified.toString(VideoParam::timeformat) == vid->modified.toString(VideoParam::timeformat) , QString("Date diff : ref modified=%1 new modified=%2").arg(videoParam.modified.toString(VideoParam::timeformat)).arg(vid->modified.toString(VideoParam::timeformat)).toUtf8());
    if(conf.acceptSmallDurationDiff)
        QVERIFY2(abs(videoParam.duration - vid->duration) <= 1 , QString("ref duration=%1 new duration=%2 - %3").arg(videoParam.duration).arg(vid->duration).arg(forVid).toUtf8());
    else
        QVERIFY2(videoParam.duration == vid->duration, QString("ref duration=%1 new duration=%2 for %3").arg(videoParam.duration).arg(vid->duration).arg(videoParam.videoInfo.absoluteFilePath()).toUtf8());
    QVERIFY2(videoParam.bitrate == vid->bitrate, QString("ref bitrate=%1 new bitrate=%2 - %3").arg(videoParam.bitrate).arg(vid->bitrate).arg(forVid).toUtf8());
    QVERIFY2(videoParam.framerate == vid->framerate, QString("framerate ref=%1 new=%2 - %3").arg(videoParam.framerate).arg(vid->framerate).arg(forVid).toUtf8());
    QVERIFY2(videoParam.codec == vid->codec, QString("codec ref=%1 new=%2 - %3").arg(videoParam.codec).arg(vid->codec).arg(forVid).toUtf8());
    if(conf.compareAudio)
        QVERIFY2(videoParam.audio == vid->audio, QString("audio ref=%1 new=%2 - %3").arg(videoParam.audio).arg(vid->audio).arg(forVid).toUtf8());
    QVERIFY2(videoParam.width == vid->width, QString("width x height ref=%1x%2 new=%3x%4 - %5").arg(videoParam.width).arg(videoParam.height).arg(vid->width).arg(vid->height).arg(forVid).toUtf8());
    QVERIFY2(videoParam.height == vid->height, QString("width x height ref=%1x%2 new=%3x%4 - %5").arg(videoParam.width).arg(videoParam.height).arg(vid->width).arg(vid->height).arg(forVid).toUtf8());

    QVERIFY2(ref_thumbnail.isNull() == vid->thumbnail.isNull(), QString("Ref thumb empty=%1 but new thumb empty=%2 - %3").arg(ref_thumbnail.isNull()).arg(vid->thumbnail.isNull()).arg(forVid).toUtf8());

    if(conf.compareThumbsVisualConfig == nullptr)
        return;
    bool manuallyAccepted = false;
    if(ref_thumbnail != vid->thumbnail){
        if(!conf.compareThumbsVisualConfig->manualCompareIfThumbsVisualDiff)
            QFAIL(QString("Thumbnails not exactly identical - %1").arg(forVid).toUtf8());
        else {
            if(TestHelpers::doThumbnailsLookSameWindow(ref_thumbnail,
                                                        vid->thumbnail,
                                                        QString("Thumbnail %1").arg(videoParam.thumbnailInfo.absoluteFilePath())))
                manuallyAccepted = true;
            else
                QFAIL(QString("Thumbnails not exactly identical and manually rejected - %1").arg(forVid).toUtf8());
        }
    }
    if(conf.compareThumbsVisualConfig->compareThumbHashes){
        if(manuallyAccepted == false){ // hash will be different anyways if thumbnails look different, so must skip these tests
            QVERIFY2(videoParam.hash1 == vid->hash[0], QString("ref hash1=%1 new hash1=%2 - %3").arg(videoParam.hash1).arg(vid->hash[0]).arg(forVid).toUtf8());
            QVERIFY2(videoParam.hash2 == vid->hash[1], QString("ref hash2=%1 new hash2=%2 - %3").arg(videoParam.hash2).arg(vid->hash[1]).arg(forVid).toUtf8());
        }
    }
}

// ---------------------------- END : helper testing functions ------------------------
// ----------------------------------------------------------------------------------
QTEST_MAIN(TestVideo)

#include "tst_video.moc"
