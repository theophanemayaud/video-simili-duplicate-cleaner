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

class TestLocalWholeAppScan : public QObject
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

QTEST_MAIN(TestLocalWholeAppScan)

#include "tst_local_whole_app_scan.moc"
