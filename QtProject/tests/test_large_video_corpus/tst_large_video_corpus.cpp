#include <QtTest>

#include "../video_corpus_test_helpers.h"

namespace
{
QDir largeCorpus()
{
#ifdef Q_OS_MACOS
    return QDir(QStringLiteral(
        "/Volumes/4TBSSD/Video duplicates - just for checking later my video duplicate program still works/Videos/"));
#else
    return {};
#endif
}
} // namespace

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
        VideoCorpusTestHelpers::addReferenceVideoRows(largeCorpus(), 12506);
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
        VideoCorpusTestHelpers::runWholeAppScan(largeCorpus(), expectation);
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
        VideoCorpusTestHelpers::runWholeAppScan(largeCorpus(), expectation);
    }
    void test_wholeAppCached()
    {
        WholeAppScanExpectation expectation;
        expectation.cacheOption = Prefs::WITH_CACHE;
        expectation.expectedVideos = 12506;
        expectation.expectedValidVideos = 12351;
        expectation.expectedMatchingVideos = 6543;
        expectation.maximumElapsedMs = 50 * 1000;
        VideoCorpusTestHelpers::runWholeAppScan(largeCorpus(), expectation);
    }
};

QTEST_MAIN(TestLargeVideoCorpus)

#include "tst_large_video_corpus.moc"
