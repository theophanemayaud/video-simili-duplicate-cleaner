#include <QtTest>

#include "external/utils/fixture_paths.h"
#include "external/utils/video_corpus_test_helpers.h"

/* Historical per-video extraction/reference-suite timings. These observations
 * are hardware-dependent; CTest's executable timeout remains the enforced
 * limit.
 *
 * Small test set
 *  - No cache
 *      -- 35s: macOS intel on intel (before remove big file tests)
 *      -- 36s: windows intel on intel (before remove big file tests)
 *      -- 17s: macOS arm m3 Pro with arm build & arm lib - 2024 oct.
 *  - Cached
 *      -- 8s: macOS intel on intel (before remove big file tests)
 *      -- 9s: windows intel on intel (before remove big file tests)
 *      -- 2.5s (2'494ms, 2'606ms, ...): macOS arm m3 Pro with arm build & arm lib - 2024 oct.
 */
class TestVideoExtractionRegression : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase() { VideoCorpusTestHelpers::initialize(); }
    void emptyDb() { VideoCorpusTestHelpers::emptyDatabase(); }

    void test_check_refvidparams_nocache_data()
    {
        VideoCorpusTestHelpers::addReferenceVideoRows(ExternalFixturePaths::videos(), 229);
    }
    void test_check_refvidparams_nocache()
    {
        VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::NO_CACHE, true);
    }
    void test_check_refvidparams_withcache_data()
    {
        VideoCorpusTestHelpers::addReferenceVideoRows(ExternalFixturePaths::videos(), 229);
    }
    void test_check_refvidparams_withcache()
    {
        VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::WITH_CACHE, true);
    }
    void test_check_refvidparams_withCacheOnly_data()
    {
        VideoCorpusTestHelpers::addReferenceVideoRows(ExternalFixturePaths::videos(), 229);
    }
    void test_check_refvidparams_withCacheOnly()
    {
        VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::CACHE_ONLY, true);
    }

    // Maintenance-only reference regeneration. Enable one wrapper when the
    // external reference files intentionally need to be refreshed.
#if 0
    void regenerateReferenceDataNoCache()
    {
        VideoCorpusTestHelpers::generateReferenceData(ExternalFixturePaths::videos(), Prefs::NO_CACHE);
    }
    void regenerateReferenceDataWithCache()
    {
        VideoCorpusTestHelpers::generateReferenceData(ExternalFixturePaths::videos(), Prefs::WITH_CACHE);
    }
#endif
};

QTEST_MAIN(TestVideoExtractionRegression)

#include "tst_video_extraction_regression.moc"
