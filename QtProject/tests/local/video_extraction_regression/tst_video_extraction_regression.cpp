#include <QtTest>

#include "local/utils/fixture_paths.h"
#include "local/utils/video_corpus_test_helpers.h"

/*
 * Historical per-video extraction/reference-suite timings. These observations
 * are hardware-dependent; CTest's executable timeout remains the enforced
 * limit. No-cache took about 35s on macOS Intel, 36s on Windows Intel, and 17s
 * on an M3 Pro ARM build (Oct 2024). Cached took about 8s on macOS Intel, 9s on
 * Windows Intel, and 2.5s (2.494s, 2.606s, ...) on that M3 Pro ARM build.
 */
class TestVideoExtractionRegression : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase() { VideoCorpusTestHelpers::initialize(); }
    void emptyDb() { VideoCorpusTestHelpers::emptyDatabase(); }

    void test_check_refvidparams_nocache_data()
    {
        VideoCorpusTestHelpers::addReferenceVideoRows(LocalFixturePaths::videos(), 229);
    }
    void test_check_refvidparams_nocache()
    {
        VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::NO_CACHE, true);
    }
    void test_check_refvidparams_withcache_data()
    {
        VideoCorpusTestHelpers::addReferenceVideoRows(LocalFixturePaths::videos(), 229);
    }
    void test_check_refvidparams_withcache()
    {
        VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::WITH_CACHE, true);
    }
    void test_check_refvidparams_withCacheOnly_data()
    {
        VideoCorpusTestHelpers::addReferenceVideoRows(LocalFixturePaths::videos(), 229);
    }
    void test_check_refvidparams_withCacheOnly()
    {
        VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::CACHE_ONLY, true);
    }
};

QTEST_MAIN(TestVideoExtractionRegression)

#include "tst_video_extraction_regression.moc"
