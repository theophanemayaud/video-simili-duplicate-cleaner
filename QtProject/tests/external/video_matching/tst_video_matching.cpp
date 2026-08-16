#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include "external/utils/fixture_paths.h"
#include "prefs.h"
#include "shared/video_matching_fixture_manifest.h"
#include "shared/video_matching_test_helpers.h"

namespace
{
constexpr int MATCH_THRESHOLD_BITS = 58; // approximately the 90% UI setting

} // namespace

class TestVideoMatching : public QObject
{
    Q_OBJECT

  private slots:
    void cleanup();
    void test_externalEndToEndMatrix_data();
    void test_externalEndToEndMatrix();
    void test_externalBaselineNoNewPairs();
};

void TestVideoMatching::cleanup()
{
    Prefs().resetSettings();
}

void TestVideoMatching::test_externalEndToEndMatrix_data()
{
    QTest::addColumn<int>("cacheScenario");
    QTest::addColumn<bool>("rotatedMatchingEnabled");
    QTest::newRow("no-cache-rotation-off") << 0 << false;
    QTest::newRow("no-cache-rotation-on") << 0 << true;
    QTest::newRow("warm-cache-rotation-on") << 1 << true;
    QTest::newRow("cache-only-rotation-on") << 2 << true;
}

void TestVideoMatching::test_externalEndToEndMatrix()
{
    QFETCH(int, cacheScenario);
    QFETCH(bool, rotatedMatchingEnabled);
    const QString externalRoot = LocalFixturePaths::root();
    const QString manifestPath = QDir(externalRoot).filePath(QStringLiteral("matching-ground-truth.csv"));
    if (externalRoot.isEmpty() || !QFileInfo::exists(manifestPath))
        QSKIP("Set VIDEO_SIMILI_EXTERNAL_FIXTURES to run the optional external matching corpus");

    QList<MatchingFixtureRecord> manifest;
    QString error;
    QVERIFY2(MatchingFixtureManifest::load(manifestPath, manifest, error), qPrintable(error));
    const auto featureManifest = VideoMatchingTestHelpers::recordsWithTag(manifest, QStringLiteral("feature_scope"));
    QCOMPARE(featureManifest.size(), 22);
    qint64 featureFixtureBytes = 0;
    for (const MatchingFixtureRecord& fixture : featureManifest) {
        const QFileInfo file(QDir(externalRoot).filePath(fixture.file));
        QVERIFY2(file.isFile(), qPrintable(QStringLiteral("Missing external fixture: %1").arg(file.filePath())));
        if (fixture.tags.contains(QStringLiteral("feature_fixture")))
            featureFixtureBytes += file.size();
    }
    QVERIFY2(featureFixtureBytes <= 250000000,
             qPrintable(QStringLiteral("External derivatives use %1 bytes; budget is 250000000").arg(featureFixtureBytes)));

    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const QString cachePath = cache.filePath(QStringLiteral("cache.sqlite"));
    if (cacheScenario != 0)
        VideoMatchingTestHelpers::scanFixtures(externalRoot, featureManifest, Prefs::WITH_CACHE, cachePath, cutEnds,
                                               MATCH_THRESHOLD_BITS, rotatedMatchingEnabled);
    const Prefs::USE_CACHE_OPTION mode = cacheScenario == 0   ? Prefs::NO_CACHE
                                         : cacheScenario == 1 ? Prefs::WITH_CACHE
                                                              : Prefs::CACHE_ONLY;
    const auto scan = VideoMatchingTestHelpers::scanFixtures(externalRoot, featureManifest, mode, cachePath, cutEnds,
                                                              MATCH_THRESHOLD_BITS, rotatedMatchingEnabled);
    qInfo() << "External assessed scan:" << featureManifest.size() << "videos in" << scan.elapsedMs << "ms";
    const QString problems =
        VideoMatchingTestHelpers::validatePairMatrix(scan, rotatedMatchingEnabled, MATCH_THRESHOLD_BITS);
    QVERIFY2(problems.isEmpty(), qPrintable(problems));
}

void TestVideoMatching::test_externalBaselineNoNewPairs()
{
    const QString externalRoot = LocalFixturePaths::root();
    const QString manifestPath = QDir(externalRoot).filePath(QStringLiteral("matching-ground-truth.csv"));
    if (externalRoot.isEmpty() || !QFileInfo::exists(manifestPath))
        QSKIP("Set VIDEO_SIMILI_EXTERNAL_FIXTURES to run the optional external matching corpus");

    QList<MatchingFixtureRecord> manifest;
    QString error;
    QVERIFY2(MatchingFixtureManifest::load(manifestPath, manifest, error), qPrintable(error));
    const auto baseline = VideoMatchingTestHelpers::recordsWithTag(manifest, QStringLiteral("baseline"));
    QCOMPARE(baseline.size(), 218);
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const auto scan = VideoMatchingTestHelpers::scanFixtures(
        externalRoot, baseline, Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")), cutEnds,
        MATCH_THRESHOLD_BITS);
    qInfo() << "External 218-video baseline assessed in" << scan.elapsedMs << "ms";
    const QString problems = VideoMatchingTestHelpers::validateBaseline(scan);
    QVERIFY2(problems.isEmpty(), qPrintable(problems));
}

QTEST_MAIN(TestVideoMatching)

#include "tst_video_matching.moc"
