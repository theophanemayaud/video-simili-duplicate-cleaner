#include <QtTest>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrent>
#include <QElapsedTimer>

#include<algorithm>

// add necessary includes here
#include "../../app/video.h"
#include "../../app/prefs.h"

#include "../../app/mainwindow.h"
#include "../../app/comparison.h"
#include "../../app/ui_comparison.h"
#include "../../app/db.h"

#include "../test_video_simplified/video_simplified_test_helpers.h"

#include "video_test_helpers.h"

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

    // Check ref params with no cache - DATA-DRIVEN TESTS
    void test_check_refvidparams_nocache_data() {
        populateRefVidParamsTestData(_videoDir);
    };
    void test_check_refvidparams_nocache(){
        refVidParamsTestConfig conf;
        conf.testDescr = __FUNCTION__;
        conf.cacheOption = Prefs::NO_CACHE;

        conf.acceptSmallDurationDiff = true;

        checkSingleVideoParams(conf);
    };   

    void test_check_refvidparams_withcache_data(){
        populateRefVidParamsTestData(_videoDir);
    };
    void test_check_refvidparams_withcache(){
        refVidParamsTestConfig conf;
        conf.testDescr = __FUNCTION__;
        conf.cacheOption = Prefs::WITH_CACHE;
        conf.acceptSmallDurationDiff = true;
        checkSingleVideoParams(conf);
    };
    void test_check_refvidparams_withCacheOnly_data(){
        populateRefVidParamsTestData(_videoDir);
    };
    void test_check_refvidparams_withCacheOnly(){
        refVidParamsTestConfig conf;
        conf.testDescr = __FUNCTION__;
        conf.cacheOption = Prefs::CACHE_ONLY;
        conf.acceptSmallDurationDiff = true;
        checkSingleVideoParams(conf);
    };
    
    void test_check_refvidparams_nocache_manualCompare_data(){
        populateRefVidParamsTestData(_videoDir);
    };
    void test_check_refvidparams_nocache_manualCompare(){
        refVidParamsTestConfig conf;
        conf.testDescr = __FUNCTION__;
        conf.cacheOption = Prefs::NO_CACHE;
        conf.acceptSmallDurationDiff = true;
        conf.compareThumbsVisualConfig->manualCompareIfThumbsVisualDiff = true;
        conf.compareThumbsVisualConfig->sampledManualThumbVerifInterval = 1;
        checkSingleVideoParams(conf);
    };

    void test_check_refvidparams_nocache_manualCompare10Sampled_data(){
        populateRefVidParamsTestData(_videoDir);
    };
    void test_check_refvidparams_nocache_manualCompare10Sampled(){
        refVidParamsTestConfig conf;
        conf.testDescr = __FUNCTION__;
        conf.cacheOption = Prefs::NO_CACHE;
        conf.paramsCSV = _csvInfo_nocache;
        conf.thumbsDir = _thumbnailDir_nocache;
        conf.videoDir = _videoDir;
        conf.acceptSmallDurationDiff = true;
        conf.compareThumbsVisualConfig->manualCompareIfThumbsVisualDiff = true;
        conf.compareThumbsVisualConfig->sampledManualThumbVerifInterval = 10;
        checkSingleVideoParams(conf);
    };

    // // Utilities to regenerate reference data - uncomment when needed
    // void createRefVidParams_nocache() {
    //     createRefVidParams(
    //         _videoDir,
    //         Prefs::NO_CACHE
    //     );
    // };
    // void createRefVidParams_withcache() {
    //     createRefVidParams(
    //         _videoDir,
    //         Prefs::WITH_CACHE
    //     );
    // };
    
    // Test whole app
    void test_whole_app_nocache(){
        wholeAppTestConfig conf;
        conf.cacheOption = Prefs::NO_CACHE;
        conf.ref_ms_time = 4.5*1000; // after adding 9 new videos: 3.996s, 4.3s, 4.13s ...
        conf.nb_vids_to_find = 209;
        conf.nb_valid_vids_to_find = 205;
        conf.nb_matching_vids_to_find = 75;
        runWholeAppScan(
            _videoDir,
            &conf
        );
    };

    void test_whole_app_cached(){
        wholeAppTestConfig conf;
        conf.cacheOption = Prefs::WITH_CACHE;
        conf.ref_ms_time = 2.0*1000; // 1.136s, 1.109s, 1.125s ... 
        conf.nb_vids_to_find = 209;
        conf.nb_valid_vids_to_find = 205;
        conf.nb_matching_vids_to_find = 75;
        runWholeAppScan(
            _videoDir,
            &conf
            );
    };

    void test_whole_app_cache_only(){
        wholeAppTestConfig conf;
        conf.cacheOption = Prefs::CACHE_ONLY;
        conf.ref_ms_time = 1.0*1000; // after adding 9 new videos: 0.571s, 0.567s, 0.568s ...
        conf.nb_vids_to_find = 209;
        conf.nb_valid_vids_to_find = 205;
        conf.nb_matching_vids_to_find = 75;
        runWholeAppScan(
            _videoDir,
            &conf
        );
    };

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
         *      -- 209: after adding 9 new videos, oct 2025
         *  - Cached
         *      -- 207: before remove big files
         *      -- 200
         *      -- 209: after adding 9 new videos, oct 2025
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
         *      -- 205: after adding 9 new videos, oct 2025
         *  - Cached
         *      -- 204: before remove big file tests
         *      -- 197
         *      -- 205: after adding 9 new videos, oct 2025
         *  - Cache only
         *      -- 204: before remove big file tests
         *      -- 197
         *      -- 205: after adding 9 new videos, oct 2025
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
         *      -- 71 (before some changes that made it go down 1)
         *      -- 70 noticed as of 2025 oct.
         *      -- 75: after adding 9 new videos, oct 2025
         *  - Cached
         *      -- 72 (before some changes that made it go down 1)
         *      -- 71 noticed as of 2025 oct.
         *      -- 75: after adding 9 new videos, oct 2025
         *  - Cache only
         *      -- 74: before remove big file tests
         *      -- 72 (before some changes that made it go down 1)
         *      -- 71 noticed as of 2025 oct.
         *      -- 75: after adding 9 new videos, oct 2025
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
         *      -- 4.5 sec (3.996s, 4.3s, 4.13s...): after adding 9 new videos, m4 MBP oct 2025
         *  - Cached
         *      -- 2.75 sec: windows intel on intel (before remove big file tests 207)
         *      -- 3 sec: macOS intel on intel (before remove big file tests 207)
         *      -- 1 sec: macOS arm on M1
         *      -- 650 ms (0.572, 0.570,...): arm m3 Pro with arm build & arm lib - 2024 oct.
         *      -- 2.0 sec (1.136s, 1.109s, 1.125s...): after adding 9 new videos, m4 MBP oct 2025
         *  - Cache only
         *      -- 3.203 sec: macos intel on intel (before remove big file tests 207)
         *      -- <1 sec: macOS arm on M1
         *      -- <1 sec (0.566, 0.538,...): arm m3 Pro with arm build & arm lib - 2024 oct.
         *      -- 1.0 sec (0.571s, 0.567s, 0.568s...): after adding 9 new videos, m4 MBP oct 2025
         * 100GB test set
         *  - No cache
         *      -- 36min: mix lib&exec metadata, exec captures
         *      -- 30min: lib(only) metadata, exec captures
         *      -- 17min: lib(only) metadata, lib(only) captures
         *      -- 17min: lib(only) metadata, lib(only) captures
         *      -- 6min 30s (418s, ...): arm m3 Pro with arm build & arm lib - 2024 oct.
         *      -- 5min (250s, 263s, 287s...): arm m3 Pro with arm build & arm lib - 2024 dec. after ordered lists reworking, and 2025 march with custom task pool
         *  - Cached
         *      -- 17min: mix lib&exec metadata, exec captures
         *      -- 6min: lib(only) metadata, exec captures
         *      -- 6min: lib(only) metadata, lib(only) captures
         *      -- 2min 33s: arm m3 Pro with arm build & arm lib - 2024 oct.
         *      -- 50s (44s, 46s, 51s, 43s,...): arm m3 Pro with arm build & arm lib - 2024 dec. after ordered lists reworking, and 2025 march with custom task pool
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
         *      -- 35s: macOS intel on intel (before remove big file tests)
         *      -- 36s: windows intel on intel (before remove big file tests)
         *      -- 17s: macOS arm m3 Pro with arm build & arm lib - 2024 oct.
         *  - Cached
         *      -- 8s: macOS intel on intel (before remove big file tests)
         *      -- 9s: windows intel on intel (before remove big file tests)
         *      -- 2.5s (2'494ms, 2'606ms, ...): macOS arm m3 Pro with arm build & arm lib - 2024 oct.
         * 100GB test set
         *  - No cache
         *      -- 37min: library(only) metadata, exec captures
         *      -- 48min: intel i5 lib(only) metadata, lib(only) captures
         *      -- 26min (1568s, 1594s, ...): arm m3 Pro with arm build & arm lib - 2024 oct.
         *  - Cached
         *      -- 39min: mix lib&exec metadata, exec captures
         *      -- 12min: library(only) metadata, exec captures
         *      -- 12min: lib(only) metadata, lib(only) captures
         *      -- 3min 27s (205s, ...: arm m3 Pro with arm build & arm lib (no thumb check) - 2024 oct. */
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
    
    // Helper to populate test data for data-driven tests
    void populateRefVidParamsTestData(const QDir videoDir);
    
    // Helper to perform single video test (called for each test data row)
    void checkSingleVideoParams(const refVidParamsTestConfig conf);
    
    static void compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(
        const QByteArray ref_thumbnail,
        const VideoParam videoParam,
        const Video *vid,
        const refVidParamsTestConfig conf = refVidParamsTestConfig()
    );

    void createRefVidParams(
        QDir videoDir,
        Prefs::USE_CACHE_OPTION cacheOption
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
    conf.ref_ms_time = 5*60*1000;
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
    conf.ref_ms_time = 50*1000;
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

    if(conf != nullptr && conf->cacheOption != Prefs::NO_CACHE)
        runWholeAppScan(videoDir); // run once to make sure all is cached

    MainWindow w = MainWindow();
    w.show();

    if(conf != nullptr){
        w.on_thresholdSlider_valueChanged(conf->videoSimilarityThreshold);

        switch (conf->cacheOption) {
        case Prefs::CACHE_ONLY:
            w.ui->radio_UseCacheOnly->click();
            break;
        case Prefs::WITH_CACHE:
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

    Comparison comp(w._videoList, w._prefs, w.geometry());
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
    prefs.useCacheOption(conf.cacheOption);
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
        auto res = vid->process();

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

// Helper to populate test data for data-driven tests
void TestVideo::populateRefVidParamsTestData(
    const QDir videoDir
) {
    emptyDb();
    QTest::addColumn<QString>("videoPath");
    
    QVERIFY(videoDir.exists());
    
    // Load extensions
    MainWindow w;
    w.loadExtensions();
    QVERIFY(!w._extensionList.isEmpty());
    w.close();
    
    // Find all videos in directory
    QDir scanDir = videoDir;
    scanDir.setNameFilters(w._extensionList);
    QDirIterator iter(scanDir, QDirIterator::Subdirectories);
    
    QStringList videoPaths;
    while(iter.hasNext()) {
        QString videoPath = iter.next();
        QFileInfo videoInfo(videoPath);
        
        // Skip if it's a directory (shouldn't happen with nameFilters but be safe)
        if (videoInfo.isDir()) continue;
        
        videoPaths.append(videoPath);
    }
    
    // Verify we found the expected number of videos (200 for the standard test set)
    const int expectedVideoCount = 209;
    QVERIFY2(videoPaths.count() == expectedVideoCount, 
        QString("Expected %1 videos but found %2 in %3")
            .arg(expectedVideoCount)
            .arg(videoPaths.count())
            .arg(videoDir.path())
            .toUtf8());
    
    // Add each video as a test row
    foreach(const QString& videoPath, videoPaths) {
        QFileInfo videoInfo(videoPath);
        // Use relative path from video directory to ensure unique test row names
        QString relativePath = videoDir.relativeFilePath(videoPath);
        QTest::newRow(relativePath.toStdString().c_str()) 
            << videoPath;
    }
}

// Helper to perform single video test (called for each test data row)
void TestVideo::checkSingleVideoParams(const refVidParamsTestConfig conf)
{
    QFETCH(QString, videoPath);
    
    QFileInfo videoInfo(videoPath);
    QVERIFY2(videoInfo.exists(), videoPath.toUtf8());
    
    // Determine suffix based on cache option
    QString suffix = (conf.cacheOption == Prefs::NO_CACHE) ? "nocache" : "withcache";
    
    // Load reference metadata from individual file (video.mp4.nocache.txt or video.mp4.withcache.txt)
    QString metadataPath = videoPath + "." + suffix + ".txt";
    QFileInfo metadataInfo(metadataPath);
    QVERIFY2(metadataInfo.exists(), 
        QString("Metadata file not found: %1").arg(metadataPath).toUtf8());
    
    VideoParam videoParam;
    SimplifiedTestHelpers::loadMetadataFromFile(
        metadataPath, 
        videoParam
    );
    
    // Load reference thumbnail from individual file (video.mp4.nocache.jpg or video.mp4.withcache.jpg)
    QString thumbnailPath = videoPath + "." + suffix + ".jpg";
    QByteArray ref_thumbnail = SimplifiedTestHelpers::loadThumbnailFromFile(thumbnailPath);
    
    Prefs prefs;
    prefs.useCacheOption(conf.cacheOption);
    
    // run scan once to populate cache
    if(conf.cacheOption != Prefs::NO_CACHE) {
        QVERIFY2(Db::initDbAndCacheLocation(prefs), "Failed to initialize cache");
        auto prefsWithCache = prefs;
        prefsWithCache.useCacheOption(Prefs::WITH_CACHE); // Need to force generate cache even if CACHE_ONLY is set
        Video *vid = new Video(prefsWithCache, videoPath);
        auto res = vid->process();
        delete vid;
    }

    // Process video with current settings    
    Video *vid = new Video(prefs, videoPath);
    auto res = vid->process();
    
    // Compare
    compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(ref_thumbnail, videoParam, vid, conf);
    
    delete vid;
}


void TestVideo::compareVideoParamToVideoAndUpdateThumbIfVisuallyIdentifcal(
    const QByteArray ref_thumbnail,
    const VideoParam videoParam,
    const Video *vid,
    const refVidParamsTestConfig conf
    ) {
    const QString forVid = QString("For %1").arg(videoParam.thumbnailInfo.absoluteFilePath());

    bool manuallyAccepted = false, pHashAccepted = false, ssimAccepted = false;

    // pHash thumbnail comparison with original hashes computed and saved as metadata
    // pHash works by comparing number of equal bits between two hashes
    if(conf.compareThumbsVisualConfig->compareThumbHashes){
        int distance1 = 64, distance2 = 64;
        uint64_t differentBits1 = videoParam.hash1 ^ vid->hash[0]; //XOR to value (only ones for differing bits)
        uint64_t differentBits2 = videoParam.hash2 ^ vid->hash[1];
        while(differentBits1) {
            differentBits1 &= differentBits1 - 1; //count number of bits of value
            distance1--;
        }
        while(differentBits2) {
            differentBits2 &= differentBits2 - 1; //count number of bits of value
            distance2--;
        }
        // TODO add min pHash distance as test parameter
        const int minDistance = 60; // x same bits out of 64

        if(distance1 >= minDistance && distance2 >= minDistance)
            pHashAccepted = true;
        else
            qWarning() << QString("hash1 distance=%1/64 hash2 distance=%2/64, expected at least %3 bits to be the same - %4").arg(distance1).arg(distance2).arg(minDistance).arg(forVid).toUtf8();
    }

    // SSIM thumbnail comparison
    auto ssim = SimplifiedTestHelpers::compareThumbnails(ref_thumbnail, vid->thumbnail);
    // TODO add ssim tolerance as test parameter
    if(ssim < 0.80)
        qWarning() << QString("got ssim %1 pct expected at least 95.0\% - %2").arg(ssim * 100).arg(forVid).toUtf8();
    else
        ssimAccepted = true;

    QVERIFY2(videoParam.size == vid->size, QString("ref size=%1 new size=%2 - %3").arg(videoParam.size).arg(vid->size).arg(forVid).toUtf8());
    
    if(conf.compareModifiedDates) {
        const auto ref_modified = videoParam.modified.toString(VideoParam::timeformat());
        const auto new_modified = vid->modified.toString(VideoParam::timeformat());
        QVERIFY2(ref_modified == new_modified , QString("Date diff : ref modified=%1 new modified=%2").arg(ref_modified).arg(new_modified).toUtf8());
    }
    uint acceptedDurationDiff = 0;
    // TODO add duration tolerance size ie 50ms or 1% of ref as test parameter
    if(conf.acceptSmallDurationDiff)
        acceptedDurationDiff = std::max(50, int(0.01 * videoParam.duration)); // biggest of 50 ms or 1% of ref duration
    QVERIFY2(abs(videoParam.duration - vid->duration) <= acceptedDurationDiff, 
        QString("ref duration=%1 new duration=%2, max accepted diff %3- %4")
        .arg(videoParam.duration).arg(vid->duration).arg(acceptedDurationDiff).arg(forVid).toUtf8());

    QVERIFY2(abs(videoParam.bitrate - vid->bitrate) <= 50, QString("ref bitrate=%1 new bitrate=%2 - %3").arg(videoParam.bitrate).arg(vid->bitrate).arg(forVid).toUtf8());
    QVERIFY2(videoParam.framerate == vid->framerate, QString("framerate ref=%1 new=%2 - %3").arg(videoParam.framerate).arg(vid->framerate).arg(forVid).toUtf8());
    QVERIFY2(videoParam.codec == vid->codec, QString("codec ref=%1 new=%2 - %3").arg(videoParam.codec).arg(vid->codec).arg(forVid).toUtf8());
    if(conf.compareAudio)
        QVERIFY2(videoParam.audio == vid->audio, QString("audio ref=%1 new=%2 - %3").arg(videoParam.audio).arg(vid->audio).arg(forVid).toUtf8());
    // Sometimes ffmpeg swaps detected width and height but that's ok as long as thumbnails are the same
    auto width = videoParam.width, height = videoParam.height;
    if (videoParam.width == vid->height && videoParam.height == vid->width) {
        width = videoParam.height;
        height = videoParam.width;
    }
    QVERIFY2(width == vid->width, QString("width x height ref=%1x%2 new=%3x%4 - %5").arg(width).arg(height).arg(vid->width).arg(vid->height).arg(forVid).toUtf8());
    QVERIFY2(height == vid->height, QString("width x height ref=%1x%2 new=%3x%4 - %5").arg(width).arg(height).arg(vid->width).arg(vid->height).arg(forVid).toUtf8());

    QVERIFY2(ref_thumbnail.isNull() == vid->thumbnail.isNull(), QString("Ref thumb empty=%1 but new thumb empty=%2 - %3").arg(ref_thumbnail.isNull()).arg(vid->thumbnail.isNull()).arg(forVid).toUtf8());

    if(conf.compareThumbsVisualConfig == nullptr)
        return;

    if (ref_thumbnail.isEmpty() && vid->thumbnail.isEmpty())
        return;

    if(!ssimAccepted || !pHashAccepted) {
        if(!conf.compareThumbsVisualConfig->manualCompareIfThumbsVisualDiff)
            QFAIL(QString("Thumbnails not exactly identical, pHashAccepted=%1, ssim Accepted=%2 - %3").arg(pHashAccepted).arg(ssimAccepted).arg(forVid).toUtf8());
        else 
            if(TestHelpers::doThumbnailsLookSameWindow(ref_thumbnail,
                vid->thumbnail,
                QString("Thumbnail %1").arg(videoParam.thumbnailInfo.absoluteFilePath())
            ))
                manuallyAccepted = true;
            else
                QFAIL(QString("Thumbnails not exactly identical and manually rejected - %1").arg(forVid).toUtf8());
    }

    // Update saved thumbnail if manually accepted
    if(manuallyAccepted) {
        QFile thumbFile(videoParam.thumbnailInfo.absoluteFilePath());
        if(!thumbFile.open(QIODevice::WriteOnly))
            qWarning() << "Failed to open thumbnail file for writing:" << videoParam.thumbnailInfo.absoluteFilePath();
        else
            thumbFile.write(vid->thumbnail);
        thumbFile.close();
    }

}

// Method to generate .txt, .jpg and ref metadata and thumbnail files
void TestVideo::createRefVidParams(
    QDir videoDir,
    Prefs::USE_CACHE_OPTION cacheOption
)
{
    emptyDb();
    QElapsedTimer timer;
    timer.start();

    runWholeAppScan(videoDir);

    QVERIFY(!videoDir.isEmpty());
    MainWindow w;
    w.loadExtensions();
    QVERIFY(!w._extensionList.isEmpty());
    w.close();
    
    // Determine suffix based on cache option
    QString suffix = (cacheOption == Prefs::NO_CACHE) ? "nocache" : "withcache";
    qDebug() << "====== Generating" << suffix << "reference data ======";
    
    videoDir.setNameFilters(w._extensionList);
    QDirIterator iter(videoDir, QDirIterator::Subdirectories);

    int processedCount = 0;
    int errorCount = 0;

    Prefs prefs;
    prefs.useCacheOption(cacheOption);
    QVERIFY2(Db::initDbAndCacheLocation(prefs), "Failed to initialize cache");

    while(iter.hasNext())
    {
        const QFile vidFile(iter.next());
        const QFileInfo vidInfo = QFileInfo(vidFile);
        
        if (vidInfo.isDir()) continue;

        QString videoPath = vidInfo.absoluteFilePath();
        // run scan once to populate cache
        if(cacheOption != Prefs::NO_CACHE) {
            Video *vid = new Video(prefs, videoDir.path());
            auto result = vid->process();
            delete vid;
        }
        QString metadataPath = videoPath + "." + suffix + ".txt";
        QString thumbPath = videoPath + "." + suffix + ".jpg";

        // Process video
        Video *vid = new Video(prefs, videoPath);
        auto result = vid->process();
        
        // Create VideoParam
        VideoParam videoParam;
        videoParam.videoInfo = vidInfo;
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
        videoParam.thumbnail = vid->thumbnail;

        // Remove old files if they exist
        if (QFileInfo::exists(metadataPath)) {
            QFile::remove(metadataPath);
        }
        if (QFileInfo::exists(thumbPath)) {
            QFile::remove(thumbPath);
        }

        // Save metadata
        if (!SimplifiedTestHelpers::saveMetadataToFile(videoParam, metadataPath, videoDir)) {
            qWarning() << "Failed to save metadata for:" << videoPath;
            errorCount++;
            delete vid;
            continue;
        }

        // Save thumbnail
        if (!SimplifiedTestHelpers::saveThumbnail(videoParam.thumbnail, thumbPath)) {
            qWarning() << "Failed to save thumbnail for:" << videoPath;
            errorCount++;
            delete vid;
            continue;
        }

        processedCount++;
        if (processedCount % 10 == 0) {
            qDebug() << "Progress:" << processedCount << "videos processed...";
        }
        
        delete vid;
    }

    qDebug() << "TIMER: createRefVidParams took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
    qDebug() << "Successfully processed:" << processedCount << "videos";
    qDebug() << "Errors:" << errorCount << "videos";
    QVERIFY2(errorCount == 0, QString("Some videos had errors: %1").arg(errorCount).toUtf8());
}

// ---------------------------- END : helper testing functions ------------------------
// ----------------------------------------------------------------------------------
QTEST_MAIN(TestVideo)

#include "tst_video.moc"
