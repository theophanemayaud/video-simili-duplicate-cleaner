#include <QtTest>

#include "external/utils/fixture_paths.h"
#include "external/utils/video_corpus_test_helpers.h"

/* Historical ~/Dev whole-app observations. Timings depend on hardware and
 * workload; the executable limits remain beside each expectation below.
 *
 * Total videos
 * Small test set
 *  - No cache
 *      -- 207: before remove big files
 *      -- 200
 *      -- 209: after adding 9 new videos including gps, oct 2025
 *      -- 218: after adding 9 more videos in apple photos library, nov 2025
 *      -- 229: after adding 11 matching feature fixtures, aug 2026
 *  - Cached
 *      -- 207: before remove big files
 *      -- 200
 *      -- 209: after adding 9 new videos including gps, oct 2025
 *      -- 218: after adding 9 more videos in apple photos library, nov 2025
 *      -- 229: after adding 11 matching feature fixtures, aug 2026
 *
 * Valid videos
 * Small test set
 *  - No cache
 *      -- 204: before remove big file tests
 *      -- 197
 *      -- 205: after adding 9 new videos including gps, oct 2025
 *      -- 214: after adding 9 more videos in apple photos library, nov 2025
 *      -- 224: after adding 11 matching feature fixtures, before matching improvements, aug 2026
 *      -- 226: after matching improvements recover two feature fixtures, aug 2026
 *  - Cached
 *      -- 204: before remove big file tests
 *      -- 197
 *      -- 205: after adding 9 new videos including gps, oct 2025
 *      -- 214: after adding 9 more videos in apple photos library, nov 2025
 *      -- 224: after adding 11 matching feature fixtures, before matching improvements, aug 2026
 *      -- 226: after matching improvements recover two feature fixtures, aug 2026
 *  - Cache only
 *      -- 204: before remove big file tests
 *      -- 197
 *      -- 205: after adding 9 new videos including gps, oct 2025
 *
 * Matching videos
 * Small test set
 *  - No cache
 *      -- 71 (before some changes that made it go down 1)
 *      -- 70 noticed as of 2025 oct.
 *      -- 75: after adding 9 new videos including gps, oct 2025
 *      -- 84: after adding 9 more videos in apple photos library, nov 2025
 *      -- 85: after adding 11 matching feature fixtures, before matching improvements, aug 2026
 *      -- 89: after matching improvements, aug 2026
 *  - Cached
 *      -- 72 (before some changes that made it go down 1)
 *      -- 71 noticed as of 2025 oct.
 *      -- 75: after adding 9 new videos including gps, oct 2025
 *      -- 84: after adding 9 more videos in apple photos library, nov 2025
 *      -- 85: after adding 11 matching feature fixtures, before matching improvements, aug 2026
 *      -- 89: after matching improvements, aug 2026
 *  - Cache only
 *      -- 74: before remove big file tests
 *      -- 72 (before some changes that made it go down 1)
 *      -- 71 noticed as of 2025 oct.
 *      -- 75: after adding 9 new videos including gps, oct 2025
 *      -- 84: after adding 9 more videos in apple photos library, nov 2025
 *      -- 85: after adding 11 matching feature fixtures, before matching improvements, aug 2026
 *      -- 89: after matching improvements, aug 2026
 *
 * Whole-scan timings
 * Small test set
 *  - No cache
 *      -- 13.5 sec: windows intel on intel (before remove big file tests)
 *      -- 13 sec: macOS intel on intel (before remove big file tests)
 *      -- 6 sec: macOS arm on M1
 *      -- 3.122 sec (3.37, 2.995,...): arm m3 Pro with arm build & arm lib - 2024 oct
 *      -- 4.5 sec (3.996s, 4.3s, 4.13s...): after adding 9 new videos including gps, m3 MBP oct 2025
 *      -- 4.0 sec (3.701s, 3.871s, 3.803s): arm m3 pro - nov 2025 - after removing custom threadpool
 *  - Cached
 *      -- 2.75 sec: windows intel on intel (before remove big file tests 207)
 *      -- 3 sec: macOS intel on intel (before remove big file tests 207)
 *      -- 1 sec: macOS arm on M1
 *      -- 650 ms (0.572, 0.570,...): arm m3 Pro with arm build & arm lib - 2024 oct.
 *      -- 2.0 sec (1.136s, 1.109s, 1.125s...): after adding 9 new videos including gps, m3 MBP oct 2025
 *      -- 1.5 sec (1.115s, 1.153s, 1.141s) arm m3 pro - nov 2025 - after removing custom threadpool (bimodal: ~1.1s or ~1.9s due to system scheduling)
 *  - Cache only
 *      -- 3.203 sec: macos intel on intel (before remove big file tests 207)
 *      -- <1 sec: macOS arm on M1
 *      -- <1 sec (0.566, 0.538,...): arm m3 Pro with arm build & arm lib - 2024 oct.
 *      -- 1.0 sec (0.571s, 0.567s, 0.568s...): after adding 9 new videos including gps, m3 MBP oct 2025
 *      -- 0.8 sec (0.694s, 0.642s, 0.701s): arm m3 pro - nov 2025 - after removing custom threadpool
 */
class TestWholeAppScan : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        VideoCorpusTestHelpers::initialize();
        const QDir videos = ExternalFixturePaths::videos();
        if (!videos.exists())
            QSKIP(qPrintable(QStringLiteral("Optional external corpus unavailable: %1").arg(videos.path())));
    }
    void emptyDb() { VideoCorpusTestHelpers::emptyDatabase(); }

    void test_whole_app_nocache()
    {
        WholeAppScanExpectation expectation;
        expectation.cacheOption = Prefs::NO_CACHE;
        expectation.expectedVideos = 229;
        expectation.expectedValidVideos = 226;
        expectation.expectedMatchingVideos = 89;
        expectation.maximumElapsedMs = 4000;
        VideoCorpusTestHelpers::runWholeAppScan(ExternalFixturePaths::videos(), expectation);
    }
    void test_whole_app_cached()
    {
        WholeAppScanExpectation expectation;
        expectation.cacheOption = Prefs::WITH_CACHE;
        expectation.expectedVideos = 229;
        expectation.expectedValidVideos = 226;
        expectation.expectedMatchingVideos = 89;
        expectation.maximumElapsedMs = 1500;
        VideoCorpusTestHelpers::runWholeAppScan(ExternalFixturePaths::videos(), expectation);
    }
    void test_whole_app_cache_only()
    {
        WholeAppScanExpectation expectation;
        expectation.cacheOption = Prefs::CACHE_ONLY;
        expectation.expectedVideos = 229;
        expectation.expectedValidVideos = 226;
        expectation.expectedMatchingVideos = 89;
        expectation.maximumElapsedMs = 800;
        VideoCorpusTestHelpers::runWholeAppScan(ExternalFixturePaths::videos(), expectation);
    }
};

QTEST_MAIN(TestWholeAppScan)

#include "tst_whole_app_scan.moc"
