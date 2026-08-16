#include <QtTest>

#include "external/utils/fixture_paths.h"
#include "external/utils/video_corpus_test_helpers.h"

/*
 * Historical ~/Dev whole-app observations. These timings depend on the machine
 * and workload; the executable limits used by the tests remain beside each
 * expectation below.
 *
 * Total videos, no-cache and cached:
 *   207 before removing the big-file fixtures; 200 afterward; 209 after adding
 *   9 videos including GPS cases (Oct 2025); 218 after adding 9 Apple Photos
 *   videos (Nov 2025); 229 after adding 11 matching-feature fixtures (Aug 2026).
 *
 * Valid videos:
 *   No-cache: 204 before removing the big-file fixtures; 197 afterward; 205
 *   after the GPS additions; 214 after the Apple Photos additions; 224 after
 *   adding the matching fixtures; 226 after matching improvements (Aug 2026).
 *   Cached: the same 204, 197, 205, 214, 224, and 226 milestones.
 *   Cache-only: 204 before removing the big-file fixtures; 197 afterward; 205
 *   after the GPS additions.
 *
 * Matching videos:
 *   No-cache: 71 before a change reduced it by one; 70 observed in Oct 2025;
 *   75 after the GPS additions; 84 after the Apple Photos additions; 85 after
 *   adding the matching fixtures; 89 after matching improvements (Aug 2026).
 *   Cached: 72 before a change reduced it by one; then 71, 75, 84, 85, and 89
 *   at the same milestones.
 *   Cache-only: 74 before removing the big-file fixtures; then 72, 71, 75, 84,
 *   85, and 89 at the same milestones.
 *
 * Whole-scan timings:
 *   No-cache: 13.5s on Windows Intel and 13s on macOS Intel before removing the
 *   big-file fixtures; 6s on Apple M1; 3.122s (3.37s, 2.995s, ...) on an M3 Pro
 *   ARM build in Oct 2024; about 4.5s (3.996s, 4.3s, 4.13s, ...) after the GPS
 *   additions in Oct 2025; about 4.0s (3.701s, 3.871s, 3.803s) after removing
 *   the custom thread pool in Nov 2025.
 *   Cached: 2.75s on Windows Intel and 3s on macOS Intel before removing the
 *   big-file fixtures; 1s on Apple M1; about 650ms (572ms, 570ms, ...) on M3
 *   Pro in Oct 2024; about 2.0s (1.136s, 1.109s, 1.125s, ...) after the GPS
 *   additions; about 1.5s (1.115s, 1.153s, 1.141s) after removing the custom
 *   thread pool, with system-scheduling runs around 1.1s or 1.9s.
 *   Cache-only: 3.203s on macOS Intel before removing the big-file fixtures;
 *   under 1s on Apple M1; 566ms/538ms on M3 Pro in Oct 2024; about 1.0s
 *   (571ms, 567ms, 568ms, ...) after the GPS additions; about 0.8s (694ms,
 *   642ms, 701ms) after removing the custom thread pool in Nov 2025.
 */
class TestWholeAppScan : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase() { VideoCorpusTestHelpers::initialize(); }
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
