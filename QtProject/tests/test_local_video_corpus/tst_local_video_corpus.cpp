#include <QtTest>

#include "../video_corpus_test_helpers.h"

namespace
{
QDir developmentCorpus()
{
#ifdef Q_OS_WIN
    return QDir(QStringLiteral("Y:/Videos/"));
#elif defined(Q_OS_MACOS)
    return QDir(QStringLiteral("/Users/theophanemayaud/Dev/Videos across all formats with duplicates of all kinds/Videos"));
#else
    return {};
#endif
}
} // namespace

class TestLocalVideoCorpus : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase() { VideoCorpusTestHelpers::initialize(); }
    void emptyDb() { VideoCorpusTestHelpers::emptyDatabase(); }

    void test_check_refvidparams_nocache_data()
    {
        VideoCorpusTestHelpers::addReferenceVideoRows(developmentCorpus(), 229);
    }
    void test_check_refvidparams_nocache()
    {
        VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::NO_CACHE, true);
    }
    void test_check_refvidparams_withcache_data()
    {
        VideoCorpusTestHelpers::addReferenceVideoRows(developmentCorpus(), 229);
    }
    void test_check_refvidparams_withcache()
    {
        VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::WITH_CACHE, true);
    }
    void test_check_refvidparams_withCacheOnly_data()
    {
        VideoCorpusTestHelpers::addReferenceVideoRows(developmentCorpus(), 229);
    }
    void test_check_refvidparams_withCacheOnly()
    {
        VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::CACHE_ONLY, true);
    }

    void test_whole_app_nocache()
    {
        WholeAppScanExpectation expectation;
        expectation.cacheOption = Prefs::NO_CACHE;
        expectation.expectedVideos = 229;
        expectation.expectedValidVideos = 226;
        expectation.expectedMatchingVideos = 89;
        expectation.maximumElapsedMs = 4000;
        VideoCorpusTestHelpers::runWholeAppScan(developmentCorpus(), expectation);
    }
    void test_whole_app_cached()
    {
        WholeAppScanExpectation expectation;
        expectation.cacheOption = Prefs::WITH_CACHE;
        expectation.expectedVideos = 229;
        expectation.expectedValidVideos = 226;
        expectation.expectedMatchingVideos = 89;
        expectation.maximumElapsedMs = 1500;
        VideoCorpusTestHelpers::runWholeAppScan(developmentCorpus(), expectation);
    }
    void test_whole_app_cache_only()
    {
        WholeAppScanExpectation expectation;
        expectation.cacheOption = Prefs::CACHE_ONLY;
        expectation.expectedVideos = 229;
        expectation.expectedValidVideos = 226;
        expectation.expectedMatchingVideos = 89;
        expectation.maximumElapsedMs = 800;
        VideoCorpusTestHelpers::runWholeAppScan(developmentCorpus(), expectation);
    }
};

QTEST_MAIN(TestLocalVideoCorpus)

#include "tst_local_video_corpus.moc"
