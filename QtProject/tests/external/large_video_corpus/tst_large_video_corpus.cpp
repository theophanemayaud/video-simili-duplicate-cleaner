#include <QtTest>

#include "external/utils/fixture_paths.h"
#include "external/utils/video_corpus_test_helpers.h"

/* Historical mounted-corpus observations. Timings depend on hardware and
 * workload; the executable limits remain beside each expectation below.
 *
 * Extraction/reference suite
 * 100GB test set
 *  - No cache
 *      -- 37min: library(only) metadata, exec captures
 *      -- 48min: intel i5 lib(only) metadata, lib(only) captures
 *      -- 26min (1568s, 1594s, ...): arm m3 Pro with arm build & arm lib - 2024 oct.
 *  - Cached
 *      -- 39min: mix lib&exec metadata, exec captures
 *      -- 12min: library(only) metadata, exec captures
 *      -- 12min: lib(only) metadata, lib(only) captures
 *      -- 3min 27s (205s, ...: arm m3 Pro with arm build & arm lib (no thumb check) - 2024 oct.
 *
 * Total videos
 * 100GB test set
 *  - No cache
 *      -- 12 505
 *      -- 12 506: matching improvements audit, aug 2026
 *  - Cached
 *      -- 12 505
 *      -- 12 506: matching improvements audit, aug 2026
 *
 * Valid videos
 * 100GB test set
 *  - No cache
 *      -- 12 328: after redo of caching
 *      -- 12 330: arm m3 Pro with arm build & arm lib - 2024 oct.
 *      -- 12 331: November 2025 update, before delete custom threadpool
 *      -- 12 352: matching improvements, aug 2026
 *  - Cached
 *      -- 12 330: after redo of caching
 *      -- 12 330: arm m3 Pro with arm build & arm lib - 2024 oct.
 *      -- 12 331: November 2025 update, before delete custom threadpool
 *      -- 12 351: warm-cache matching improvements, aug 2026
 *
 * Matching videos
 * 100GB test set
 *  - No cache
 *      -- 6 626: mix lib&exec metadata, exec captures
 *      -- 6 562: lib(only) metadata, lib(only) captures
 *      -- 6 558 (97.1GB): arm but intel build
 *      -- 6 568 (97.2 GB): arm m3 Pro with arm build & arm lib - 2024 oct.
 *      -- 6 535 (97.0 GB): November 2025 update, before delete custom threadpool
 *      -- 6 552 off / 6 556 rotation on: matching improvements, aug 2026
 *  - Cached
 *      -- 6 626: mix lib&exec metadata, exec captures
 *      -- 6 553: lib(only) metadata, lib(only) captures
 *      -- 6 550 (97.1GB): after redo of caching
 *      -- 6 555 (97.1 GB): arm m3 Pro with arm build & arm lib - 2024 oct.
 *      -- 6 527 (97.0 GB): November 2025 update, before delete custom threadpool
 *      -- 6 543: warm-cache matching improvements, aug 2026
 *
 * Whole-scan timings
 * 100GB test set
 *  - No cache
 *      -- 36min: mix lib&exec metadata, exec captures
 *      -- 30min: lib(only) metadata, exec captures
 *      -- 17min: lib(only) metadata, lib(only) captures
 *      -- 17min: lib(only) metadata, lib(only) captures
 *      -- 6min 30s (418s, ...): arm m3 Pro with arm build & arm lib - 2024 oct.
 *      -- 5min (250s, 263s, 287s...): arm m3 Pro with arm build & arm lib - 2024 dec. after ordered lists reworking, and 2025 march with custom task pool
 *      -- 5min (273.736s, 255.859s): arm m3 Pro - November 2025 update, before delete custom threadpool
 *      -- 4min (237.977s, 236.576s, 234.792s): arm m3 pro - nov 2025 - after delete custom threadpool
 *  - Cached
 *      -- 17min: mix lib&exec metadata, exec captures
 *      -- 6min: lib(only) metadata, exec captures
 *      -- 6min: lib(only) metadata, lib(only) captures
 *      -- 2min 33s: arm m3 Pro with arm build & arm lib - 2024 oct.
 *      -- 50s (44s, 46s, 51s, 43s,...): arm m3 Pro with arm build & arm lib - 2024 dec. after ordered lists reworking, and 2025 march with custom task pool
 *      -- 50s (42s, 44s, 42.262s): arm m3 Pro - November 2025 update, before delete custom threadpool
 *      -- 50s (45.397s, 43.660s, 44.903s): arm m3 pro - nov 2025 - after delete custom threadpool
 */
class TestLargeVideoCorpus : public QObject
{
    Q_OBJECT

  public:
    static void initMain() { qputenv("QTEST_FUNCTION_TIMEOUT", "7200000"); }

  private slots:
    void initTestCase() { VideoCorpusTestHelpers::initialize(); }
    void emptyDb() { VideoCorpusTestHelpers::emptyDatabase(); }
    void test_referenceVideosNoCache_data()
    {
        VideoCorpusTestHelpers::addReferenceVideoRows(ExternalFixturePaths::mountedLargeVideos(), 12506);
    }
    void test_referenceVideosNoCache()
    {
        VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::NO_CACHE, true);
    }
    void test_wholeAppNoCache()
    {
        WholeAppScanExpectation expectation;
        expectation.cacheOption = Prefs::NO_CACHE;
        expectation.expectedVideos = 12506;
        expectation.expectedValidVideos = 12352;
        expectation.expectedMatchingVideos = 6552;
        expectation.maximumElapsedMs = 4 * 60 * 1000;
        VideoCorpusTestHelpers::runWholeAppScan(ExternalFixturePaths::mountedLargeVideos(), expectation);
    }
    void test_wholeAppNoCacheWithRotations()
    {
        WholeAppScanExpectation expectation;
        expectation.cacheOption = Prefs::NO_CACHE;
        expectation.detectRotatedCopies = true;
        expectation.expectedVideos = 12506;
        expectation.expectedValidVideos = 12352;
        expectation.expectedMatchingVideos = 6556;
        expectation.maximumElapsedMs = 6 * 60 * 1000;
        VideoCorpusTestHelpers::runWholeAppScan(ExternalFixturePaths::mountedLargeVideos(), expectation);
    }
    void test_wholeAppCached()
    {
        WholeAppScanExpectation expectation;
        expectation.cacheOption = Prefs::WITH_CACHE;
        expectation.expectedVideos = 12506;
        expectation.expectedValidVideos = 12351;
        expectation.expectedMatchingVideos = 6543;
        expectation.maximumElapsedMs = 50 * 1000;
        VideoCorpusTestHelpers::runWholeAppScan(ExternalFixturePaths::mountedLargeVideos(), expectation);
    }

    // Maintenance-only mounted-corpus reference regeneration. Enable only
    // when the external reference files intentionally need to be refreshed.
#if 0
    void regenerateLargeReferenceDataNoCache()
    {
        VideoCorpusTestHelpers::generateReferenceData(ExternalFixturePaths::mountedLargeVideos(), Prefs::NO_CACHE);
    }
#endif
};

QTEST_MAIN(TestLargeVideoCorpus)

#include "tst_large_video_corpus.moc"
