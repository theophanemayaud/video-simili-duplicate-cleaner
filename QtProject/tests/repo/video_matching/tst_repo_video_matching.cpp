#include <QFileInfo>
#include <QPainter>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../app/db.h"
#include "../../../app/prefs.h"
#include "../../../app/visualfingerprint.h"
#include "shared/video_matching_fixture_manifest.h"
#include "shared/video_matching_test_helpers.h"

namespace
{
constexpr int MATCH_THRESHOLD_BITS = 58;      // approximately the 90% UI setting
constexpr int PILLAR_NEGATIVE_THRESHOLD = 53; // approximately the issue #138 83% setting

QString projectRoot()
{
    QDir candidate(QDir::currentPath());
    while (!candidate.exists(QStringLiteral("samples/videos/matching-ground-truth.csv")))
        if (!candidate.cdUp())
            return {};
    return candidate.absolutePath();
}
} // namespace

class TestRepoVideoMatching : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanup();
    void test_manifestAndSizeBudget();
    void test_frameAnalysisAndBlackBarConsensus();
    void test_rotatedPreferenceDefaultsOffAndPersists();
    void test_existingCaptureCacheIsReusedWithoutInvalidation();
    void test_displayMatrixPresentationIsNormalized();
    void test_monochromeCutEndsAreSubstituted();
    void test_blackBarsAreNormalized_data();
    void test_blackBarsAreNormalized();
    void test_physicalRotationsMatchWhenEnabled_data();
    void test_physicalRotationsMatchWhenEnabled();
    void test_samePillarBarsDoNotMatchDifferentContent();
    void test_trackedEndToEndMatrix_data();
    void test_trackedEndToEndMatrix();

  private:
    VideoMatchingTestHelpers::FixtureScan trackedScan(Prefs::USE_CACHE_OPTION mode, const QString& cachePath,
                                                       int thumbnailMode = cutEnds,
                                                       bool detectRotatedCopies = false) const;
    QString _projectRoot;
    QString _trackedRoot;
    QList<MatchingFixtureRecord> _trackedManifest;
};

void TestRepoVideoMatching::initTestCase()
{
    qSetMessagePattern(QStringLiteral("%{file}(%{line}) %{function}: %{message}"));
    _projectRoot = projectRoot();
    QVERIFY2(!_projectRoot.isEmpty(), "Could not find the repository root");
    _trackedRoot = QDir(_projectRoot).filePath(QStringLiteral("samples/videos"));
    QString error;
    QVERIFY2(MatchingFixtureManifest::load(QDir(_trackedRoot).filePath(QStringLiteral("matching-ground-truth.csv")),
                                           _trackedManifest, error),
             qPrintable(error));
}

void TestRepoVideoMatching::cleanup()
{
    Prefs().resetSettings();
}

VideoMatchingTestHelpers::FixtureScan TestRepoVideoMatching::trackedScan(Prefs::USE_CACHE_OPTION mode,
                                                                          const QString& cachePath,
                                                                          int thumbnailMode,
                                                                          bool detectRotatedCopies) const
{
    return VideoMatchingTestHelpers::scanFixtures(_trackedRoot, _trackedManifest, mode, cachePath, thumbnailMode,
                                                  MATCH_THRESHOLD_BITS, detectRotatedCopies);
}

void TestRepoVideoMatching::test_manifestAndSizeBudget()
{
    QCOMPARE(_trackedManifest.size(), 12);
    qint64 totalVideoBytes = 0;
    QSet<QString> groups;
    for (const MatchingFixtureRecord& fixture : _trackedManifest) {
        const QFileInfo video(QDir(_trackedRoot).filePath(fixture.file));
        QVERIFY2(video.isFile(), qPrintable(QStringLiteral("Missing tracked fixture: %1").arg(video.filePath())));
        totalVideoBytes += video.size();
        groups.insert(fixture.contentGroup);
    }
    QCOMPARE(groups, QSet<QString>({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("Nice")}));
    QVERIFY2(totalVideoBytes <= 2000000,
             qPrintable(QStringLiteral("Tracked matching videos use %1 bytes; budget is 2000000").arg(totalVideoBytes)));
    qInfo() << "Tracked matching video bytes:" << totalVideoBytes;
}

void TestRepoVideoMatching::test_frameAnalysisAndBlackBarConsensus()
{
    QImage uniform(100, 60, QImage::Format_RGB888);
    uniform.fill(QColor(8, 8, 8));
    QVERIFY(!VisualFingerprintBuilder::analyzeFrame(uniform).informative);

    QImage darkDetail = uniform;
    QPainter detailPainter(&darkDetail);
    detailPainter.fillRect(QRect(35, 10, 30, 40), QColor(40, 40, 40));
    detailPainter.end();
    QVERIFY(VisualFingerprintBuilder::analyzeFrame(darkDetail).informative);

    QImage letterbox(100, 60, QImage::Format_RGB888);
    letterbox.fill(Qt::black);
    QPainter letterboxPainter(&letterbox);
    letterboxPainter.fillRect(QRect(0, 10, 100, 40), QColor(30, 120, 220));
    letterboxPainter.end();
    const FrameAnalysis bars = VisualFingerprintBuilder::analyzeFrame(letterbox);
    QVERIFY(bars.informative);
    QCOMPARE(bars.barAxis, BlackBarAxis::horizontal);
    const auto crop = VisualFingerprintBuilder::unanimousBlackBarCrop({bars, bars});
    QVERIFY(crop.has_value());
    QCOMPARE(crop->axis, BlackBarAxis::horizontal);

    QImage oneSided = letterbox;
    QPainter oneSidedPainter(&oneSided);
    oneSidedPainter.fillRect(QRect(0, 50, 100, 10), QColor(30, 120, 220));
    oneSidedPainter.end();
    const FrameAnalysis dissent = VisualFingerprintBuilder::analyzeFrame(oneSided);
    QCOMPARE(dissent.barAxis, BlackBarAxis::none);
    QVERIFY(!VisualFingerprintBuilder::unanimousBlackBarCrop({bars, dissent}).has_value());
}

void TestRepoVideoMatching::test_rotatedPreferenceDefaultsOffAndPersists()
{
    Prefs prefs;
    prefs.resetSettings();
    QVERIFY(!prefs.detectRotatedCopies());
    prefs.detectRotatedCopies(true);
    QVERIFY(Prefs().detectRotatedCopies());
}

void TestRepoVideoMatching::test_existingCaptureCacheIsReusedWithoutInvalidation()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString cachePath = temporary.filePath(QStringLiteral("legacy.sqlite"));
    const QString connectionName = QStringLiteral("legacy-cache-setup");
    {
        QSqlDatabase legacy = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        legacy.setDatabaseName(cachePath);
        QVERIFY(legacy.open());
        QSqlQuery query(legacy);
        QVERIFY(query.exec(QStringLiteral("CREATE TABLE capture (id TEXT PRIMARY KEY, at8 BLOB);")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO capture (id, at8) VALUES ('old', X'00');")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE ignored_pairs (pathName1 TEXT, pathName2 TEXT, PRIMARY KEY (pathName1, pathName2));")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO ignored_pairs VALUES ('left.mp4', 'right.mp4');")));
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 1;")));
        legacy.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    Prefs prefs;
    prefs.cacheFilePathName(cachePath);
    QVERIFY(Db::initDbAndCacheLocation(prefs));
    const QString verifyConnection = QStringLiteral("legacy-cache-verify");
    {
        QSqlDatabase cache = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyConnection);
        cache.setDatabaseName(cachePath);
        QVERIFY(cache.open());
        QSqlQuery query(cache);
        QVERIFY(query.exec(QStringLiteral("SELECT at8 FROM capture WHERE id = 'old';")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toByteArray(), QByteArray::fromHex("00"));
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM ignored_pairs "
                                          "WHERE pathName1 = 'left.mp4' AND pathName2 = 'right.mp4';")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version;")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        cache.close();
    }
    QSqlDatabase::removeDatabase(verifyConnection);
}

void TestRepoVideoMatching::test_displayMatrixPresentationIsNormalized()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const auto scan = trackedScan(Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")));
    const auto* base = VideoMatchingTestHelpers::findFixture(scan, QStringLiteral("matching/a-original.mp4"));
    const auto* metadata =
        VideoMatchingTestHelpers::findFixture(scan, QStringLiteral("matching/a-display-matrix.mp4"));
    QVERIFY(base && metadata);
    QVERIFY2(base->processingError.isEmpty(), qPrintable(base->processingError));
    QVERIFY2(metadata->processingError.isEmpty(), qPrintable(metadata->processingError));
    QCOMPARE(metadata->video->width, base->video->width);
    QCOMPARE(metadata->video->height, base->video->height);
    QVERIFY2(VideoPairMatcher::match(*base->video, *metadata->video,
                                     VideoMatchingTestHelpers::matchConfig(cutEnds, MATCH_THRESHOLD_BITS))
                 .matches,
             "A Display Matrix-normalized copy should match without physical-rotation fallback");
}

void TestRepoVideoMatching::test_monochromeCutEndsAreSubstituted()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const auto scan = trackedScan(Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")));
    const auto* base = VideoMatchingTestHelpers::findFixture(scan, QStringLiteral("matching/a-original.mp4"));
    const auto* monochrome =
        VideoMatchingTestHelpers::findFixture(scan, QStringLiteral("matching/a-monochrome-ends.mp4"));
    QVERIFY(base && monochrome);
    QVERIFY2(monochrome->processingError.isEmpty(), qPrintable(monochrome->processingError));
    const VideoPairMatchResult result = VideoPairMatcher::match(
        *base->video, *monochrome->video, VideoMatchingTestHelpers::matchConfig(cutEnds, MATCH_THRESHOLD_BITS));
    QVERIFY2(result.matches, "+/-2% informative substitutes should make the cutEnds copy match A");
}

void TestRepoVideoMatching::test_blackBarsAreNormalized_data()
{
    QTest::addColumn<QString>("variant");
    QTest::newRow("letterbox") << QStringLiteral("matching/a-letterbox.mp4");
    QTest::newRow("pillarbox") << QStringLiteral("matching/a-pillarbox.mp4");
}

void TestRepoVideoMatching::test_blackBarsAreNormalized()
{
    QFETCH(QString, variant);
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const auto scan = trackedScan(Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")));
    const auto* base = VideoMatchingTestHelpers::findFixture(scan, QStringLiteral("matching/a-original.mp4"));
    const auto* barred = VideoMatchingTestHelpers::findFixture(scan, variant);
    QVERIFY(base && barred);
    QVERIFY2(base->processingError.isEmpty(), qPrintable(base->processingError));
    QVERIFY2(barred->processingError.isEmpty(), qPrintable(barred->processingError));
    QVERIFY2(VideoPairMatcher::match(*base->video, *barred->video,
                                     VideoMatchingTestHelpers::matchConfig(cutEnds, MATCH_THRESHOLD_BITS))
                 .matches,
             qPrintable(QStringLiteral("Symmetric bars were not normalized for %1").arg(variant)));
}

void TestRepoVideoMatching::test_physicalRotationsMatchWhenEnabled_data()
{
    QTest::addColumn<QString>("variant");
    QTest::newRow("clockwise-90") << QStringLiteral("matching/a-rot90.mp4");
    QTest::newRow("rotated-180") << QStringLiteral("matching/a-rot180.mp4");
    QTest::newRow("counter-clockwise-90") << QStringLiteral("matching/a-rot270.mp4");
}

void TestRepoVideoMatching::test_physicalRotationsMatchWhenEnabled()
{
    QFETCH(QString, variant);
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const auto scan = trackedScan(Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")), cutEnds, true);
    const auto* base = VideoMatchingTestHelpers::findFixture(scan, QStringLiteral("matching/a-original.mp4"));
    const auto* rotated = VideoMatchingTestHelpers::findFixture(scan, variant);
    QVERIFY(base && rotated);
    QVERIFY2(base->processingError.isEmpty(), qPrintable(base->processingError));
    QVERIFY2(rotated->processingError.isEmpty(), qPrintable(rotated->processingError));
    QVERIFY2(VideoPairMatcher::match(*base->video, *rotated->video,
                                     VideoMatchingTestHelpers::matchConfig(cutEnds, MATCH_THRESHOLD_BITS, true))
                 .matches,
             qPrintable(QStringLiteral("Rotated matching did not recover %1").arg(variant)));
}

void TestRepoVideoMatching::test_samePillarBarsDoNotMatchDifferentContent()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const auto scan = trackedScan(Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")));
    const auto* a = VideoMatchingTestHelpers::findFixture(scan, QStringLiteral("matching/a-pillarbox.mp4"));
    const auto* b = VideoMatchingTestHelpers::findFixture(scan, QStringLiteral("matching/b-pillarbox.mp4"));
    QVERIFY(a && b);
    QVERIFY2(a->processingError.isEmpty(), qPrintable(a->processingError));
    QVERIFY2(b->processingError.isEmpty(), qPrintable(b->processingError));
    const VideoPairMatchResult result = VideoPairMatcher::match(
        *a->video, *b->video, VideoMatchingTestHelpers::matchConfig(cutEnds, PILLAR_NEGATIVE_THRESHOLD));
    QVERIFY2(!result.matches, "Different portrait videos with identical pillar bars matched at the issue #138 threshold");
}

void TestRepoVideoMatching::test_trackedEndToEndMatrix_data()
{
    QTest::addColumn<int>("cacheScenario");
    QTest::addColumn<bool>("rotatedMatchingEnabled");
    for (int cacheScenario = 0; cacheScenario < 3; ++cacheScenario) {
        const QString cacheName = cacheScenario == 0   ? QStringLiteral("no-cache")
                                  : cacheScenario == 1 ? QStringLiteral("warm-cache")
                                                       : QStringLiteral("cache-only");
        for (bool rotated : {false, true})
            QTest::newRow(qPrintable(cacheName + (rotated ? QStringLiteral("-rotation-on")
                                                            : QStringLiteral("-rotation-off"))))
                << cacheScenario << rotated;
    }
}

void TestRepoVideoMatching::test_trackedEndToEndMatrix()
{
    QFETCH(int, cacheScenario);
    QFETCH(bool, rotatedMatchingEnabled);
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const QString cachePath = cache.filePath(QStringLiteral("cache.sqlite"));
    if (cacheScenario != 0)
        trackedScan(Prefs::WITH_CACHE, cachePath, cutEnds, rotatedMatchingEnabled);
    const Prefs::USE_CACHE_OPTION mode = cacheScenario == 0   ? Prefs::NO_CACHE
                                         : cacheScenario == 1 ? Prefs::WITH_CACHE
                                                              : Prefs::CACHE_ONLY;
    const auto scan = trackedScan(mode, cachePath, cutEnds, rotatedMatchingEnabled);
    const QString problems =
        VideoMatchingTestHelpers::validatePairMatrix(scan, rotatedMatchingEnabled, MATCH_THRESHOLD_BITS);
    QVERIFY2(problems.isEmpty(), qPrintable(problems));
}

QTEST_MAIN(TestRepoVideoMatching)

#include "tst_repo_video_matching.moc"
