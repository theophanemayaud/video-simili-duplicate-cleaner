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

class TestLocalVideoExtractionRegression : public QObject
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
};

QTEST_MAIN(TestLocalVideoExtractionRegression)

#include "tst_local_video_extraction_regression.moc"
