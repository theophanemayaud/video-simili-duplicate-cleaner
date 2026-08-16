#include <QtTest>

#include "external/utils/fixture_paths.h"
#include "external/utils/video_corpus_test_helpers.h"

/*
 * Historical mounted-corpus observations. Timings are hardware-dependent; the
 * executable limits used by the tests remain beside each expectation below.
 *
 * Extraction/reference suite:
 *   No-cache: 37min with library metadata and executable captures; 48min on an
 *   Intel i5 with library metadata/captures; 26min (1568s, 1594s, ...) on an M3
 *   Pro ARM build in Oct 2024.
 *   Cached: 39min with mixed library/executable metadata and executable
 *   captures; 12min with library metadata and executable captures; 12min with
 *   library metadata/captures; 3min27s (205s, ...) on an M3 Pro ARM build with
 *   thumbnail checks disabled in Oct 2024.
 *
 * Total videos, no-cache and cached:
 *   12,505 historically; 12,506 after the matching-improvements audit in Aug
 *   2026.
 *
 * Valid videos:
 *   No-cache: 12,328 after the cache rewrite; 12,330 on an M3 Pro ARM build in
 *   Oct 2024; 12,331 before removing the custom thread pool in Nov 2025; 12,352
 *   after matching improvements in Aug 2026.
 *   Cached: 12,330 after the cache rewrite and on the Oct 2024 M3 Pro build;
 *   12,331 in Nov 2025; 12,351 after matching improvements in Aug 2026.
 *
 * Matching videos:
 *   No-cache: 6,626 with mixed library/executable metadata and executable
 *   captures; 6,562 with library metadata/captures; 6,558 at 97.1GB on an ARM
 *   machine running an Intel build; 6,568 at 97.2GB on an M3 Pro ARM build in
 *   Oct 2024; 6,535 at 97.0GB before removing the custom thread pool in Nov
 *   2025; 6,552 with rotations off and 6,556 on after matching improvements in
 *   Aug 2026.
 *   Cached: 6,626 with mixed library/executable metadata and executable
 *   captures; 6,553 with library metadata/captures; 6,550 at 97.1GB after the
 *   cache rewrite; 6,555 at 97.1GB on the Oct 2024 M3 Pro build; 6,527 at
 *   97.0GB before removing the custom thread pool; 6,543 after the Aug 2026
 *   warm-cache matching improvements.
 *
 * Whole-scan timings:
 *   No-cache: 36min with mixed library/executable metadata and executable
 *   captures; 30min with library metadata and executable captures; 17min with
 *   library metadata/captures; 6min30s (418s, ...) on an M3 Pro ARM build in
 *   Oct 2024; about 5min (250s, 263s, 287s, ...) after ordered-list work in Dec
 *   2024 and with the custom task pool in Mar 2025; about 5min (273.736s,
 *   255.859s) before removing that pool in Nov 2025; about 4min (237.977s,
 *   236.576s, 234.792s) afterward.
 *   Cached: 17min with mixed metadata and executable captures; 6min with
 *   library metadata and executable captures; 6min with library
 *   metadata/captures; 2min33s on an M3 Pro ARM build in Oct 2024; about 50s
 *   (44s, 46s, 51s, 43s, ...) after ordered-list work and with the custom task
 *   pool; about 50s (42s, 44s, 42.262s) before removing that pool; about 50s
 *   (45.397s, 43.660s, 44.903s) afterward.
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
};

QTEST_MAIN(TestLargeVideoCorpus)

#include "tst_large_video_corpus.moc"
