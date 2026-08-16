#include <QElapsedTimer>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

#include "../../app/comparison/internal/videopairmatcher.h"
#include "../../app/db.h"
#include "../../app/prefs.h"
#include "../../app/video.h"
#include "../video_matching_fixture_manifest.h"

#include <memory>

namespace
{
constexpr int MATCH_THRESHOLD_BITS = 58;      // approximately the 90% UI setting
constexpr int PILLAR_NEGATIVE_THRESHOLD = 53; // approximately the issue #138 83% setting

struct ProcessedFixture {
    MatchingFixtureRecord record;
    std::shared_ptr<Video> video;
    QString processingError;
};

struct FixtureScan {
    QList<ProcessedFixture> fixtures;
    qint64 elapsedMs = 0;
};

QString pairKey(const QString& left, const QString& right)
{
    return left < right ? left + QStringLiteral(" <> ") + right : right + QStringLiteral(" <> ") + left;
}

QString formatSetDifference(const QString& label, const QSet<QString>& pairs)
{
    if (pairs.isEmpty())
        return {};
    QStringList sorted(pairs.begin(), pairs.end());
    sorted.sort();
    return QStringLiteral("%1 (%2):\n  %3").arg(label).arg(sorted.size()).arg(sorted.join(QStringLiteral("\n  ")));
}

QString projectRoot()
{
    QDir candidate(QDir::currentPath());
    while (!candidate.exists(QStringLiteral("samples/videos/matching-ground-truth.csv")))
        if (!candidate.cdUp())
            return {};
    return candidate.absolutePath();
}

QString externalFixtureRoot()
{
    QString root = qEnvironmentVariable("VIDEO_SIMILI_EXTERNAL_FIXTURES");
#ifdef Q_OS_MACOS
    if (root.isEmpty())
        root = QStringLiteral("/Users/theophanemayaud/Dev/Videos across all formats with duplicates of all kinds");
#endif
    return root;
}

QList<MatchingFixtureRecord> recordsWithTag(const QList<MatchingFixtureRecord>& manifest, const QString& tag)
{
    QList<MatchingFixtureRecord> selected;
    for (const MatchingFixtureRecord& record : manifest)
        if (record.tags.contains(tag))
            selected.append(record);
    return selected;
}

VideoPairMatchConfig matchConfig(int thumbnailMode, int thresholdBits, bool detectRotatedCopies = false)
{
    VideoPairMatchConfig config;
    config.comparisonMode = Prefs::_PHASH;
    config.thumbnailsMode = thumbnailMode;
    config.thresholdPhash = thresholdBits;
    config.sameDurationModifier = 1;
    config.differentDurationModifier = 4;
    config.detectRotatedCopies = detectRotatedCopies;
    return config;
}

FixtureScan scanFixtures(const QString& fixtureRoot, const QList<MatchingFixtureRecord>& manifest,
                         Prefs::USE_CACHE_OPTION cacheMode, const QString& cachePath, int thumbnailMode,
                         bool detectRotatedCopies = false)
{
    Prefs prefs;
    prefs.resetSettings();
    (void)prefs.thumbnailsMode();
    prefs.thumbnailsMode(thumbnailMode);
    (void)prefs.comparisonMode();
    prefs.comparisonMode(Prefs::_PHASH);
    (void)prefs.useCacheOption();
    prefs.useCacheOption(cacheMode);
    prefs.cacheFilePathName(cachePath);
    prefs._thresholdPhash = MATCH_THRESHOLD_BITS;
    prefs._sameDurationModifier = 1;
    prefs._differentDurationModifier = 4;
    prefs.detectRotatedCopies(detectRotatedCopies);

    if (cacheMode != Prefs::NO_CACHE)
        Db::initDbAndCacheLocation(prefs);

    QElapsedTimer timer;
    timer.start();

    FixtureScan scan;
    for (const MatchingFixtureRecord& record : manifest) {
        ProcessedFixture fixture;
        fixture.record = record;
        fixture.video = std::make_shared<Video>(prefs, QDir(fixtureRoot).filePath(record.file));
        const Video::ProcessingResult result = fixture.video->process();
        if (!result.success)
            fixture.processingError = result.errorMsg;
        scan.fixtures.append(std::move(fixture));
    }
    scan.elapsedMs = timer.elapsed();
    return scan;
}

const ProcessedFixture* findFixture(const FixtureScan& scan, const QString& file)
{
    for (const ProcessedFixture& fixture : scan.fixtures)
        if (fixture.record.file == file)
            return &fixture;
    return nullptr;
}

QString validatePairMatrix(const FixtureScan& scan, bool rotatedMatchingEnabled, int thresholdBits)
{
    QStringList processingProblems;
    for (const ProcessedFixture& fixture : scan.fixtures) {
        const bool succeeded = fixture.processingError.isEmpty();
        if (succeeded != fixture.record.expectedProcessing) {
            processingProblems.append(
                QStringLiteral("%1 expected %2 but got %3")
                    .arg(fixture.record.file,
                         fixture.record.expectedProcessing ? QStringLiteral("success") : QStringLiteral("failure"),
                         succeeded ? QStringLiteral("success") : fixture.processingError));
        }
    }

    QSet<QString> expectedPairs;
    QSet<QString> allowedPairs;
    QSet<QString> actualPairs;
    const VideoPairMatchConfig config = matchConfig(cutEnds, thresholdBits, rotatedMatchingEnabled);
    for (qsizetype leftIndex = 0; leftIndex < scan.fixtures.size(); ++leftIndex) {
        const ProcessedFixture& left = scan.fixtures.at(leftIndex);
        for (qsizetype rightIndex = leftIndex + 1; rightIndex < scan.fixtures.size(); ++rightIndex) {
            const ProcessedFixture& right = scan.fixtures.at(rightIndex);
            const QString key = pairKey(left.record.file, right.record.file);
            const bool sameContent = left.record.contentGroup == right.record.contentGroup;
            const bool sameOrientation =
                left.record.matchingOrientationDegrees == right.record.matchingOrientationDegrees;
            if (sameContent && (rotatedMatchingEnabled || sameOrientation) && left.record.expectedProcessing
                && right.record.expectedProcessing) {
                allowedPairs.insert(key);
                // Substantially different-duration and partial-clip matching
                // is explicitly outside this feature's required recall scope.
                if (qAbs(left.video->duration - right.video->duration) <= 1000)
                    expectedPairs.insert(key);
            }

            if (left.processingError.isEmpty() && right.processingError.isEmpty()
                && VideoPairMatcher::match(*left.video, *right.video, config).matches)
                actualPairs.insert(key);
        }
    }

    const QSet<QString> missing = expectedPairs - actualPairs;
    const QSet<QString> unexpected = actualPairs - allowedPairs;
    QStringList problems;
    if (!processingProblems.isEmpty())
        problems.append(QStringLiteral("Processing mismatches (%1):\n  %2")
                            .arg(processingProblems.size())
                            .arg(processingProblems.join(QStringLiteral("\n  "))));
    const QString missingText = formatSetDifference(QStringLiteral("Missing pairs"), missing);
    const QString unexpectedText = formatSetDifference(QStringLiteral("Unexpected pairs"), unexpected);
    if (!missingText.isEmpty())
        problems.append(missingText);
    if (!unexpectedText.isEmpty())
        problems.append(unexpectedText);
    return problems.join(QStringLiteral("\n"));
}

QString validateBaseline(const FixtureScan& scan)
{
    QStringList processingProblems;
    QSet<QString> expectedEstablishedPairs;
    QSet<QString> actualPairs;
    QSet<QString> actualCrossGroupPairs;
    const VideoPairMatchConfig config = matchConfig(cutEnds, 64, false);

    for (const ProcessedFixture& fixture : scan.fixtures) {
        if (fixture.record.tags.contains(QStringLiteral("expected_recovery")))
            continue;
        const bool succeeded = fixture.processingError.isEmpty();
        if (succeeded != fixture.record.expectedProcessing) {
            processingProblems.append(
                QStringLiteral("%1 expected %2 but got %3")
                    .arg(fixture.record.file,
                         fixture.record.expectedProcessing ? QStringLiteral("success") : QStringLiteral("failure"),
                         succeeded ? QStringLiteral("success") : fixture.processingError));
        }
    }

    for (qsizetype leftIndex = 0; leftIndex < scan.fixtures.size(); ++leftIndex) {
        const ProcessedFixture& left = scan.fixtures.at(leftIndex);
        for (qsizetype rightIndex = leftIndex + 1; rightIndex < scan.fixtures.size(); ++rightIndex) {
            const ProcessedFixture& right = scan.fixtures.at(rightIndex);
            const QString key = pairKey(left.record.file, right.record.file);
            const bool sameGroup = left.record.contentGroup == right.record.contentGroup;
            const bool recoveryPair = left.record.tags.contains(QStringLiteral("expected_recovery"))
                                      || right.record.tags.contains(QStringLiteral("expected_recovery"));
            if (sameGroup && !recoveryPair && left.record.expectedProcessing && right.record.expectedProcessing)
                expectedEstablishedPairs.insert(key);

            if (left.processingError.isEmpty() && right.processingError.isEmpty()
                && VideoPairMatcher::match(*left.video, *right.video, config).matches)
            {
                actualPairs.insert(key);
                if (!sameGroup)
                    actualCrossGroupPairs.insert(key);
            }
        }
    }

    QStringList problems;
    if (!processingProblems.isEmpty())
        problems.append(QStringLiteral("Processing mismatches (%1):\n  %2")
                            .arg(processingProblems.size())
                            .arg(processingProblems.join(QStringLiteral("\n  "))));
    const QString missingText = formatSetDifference(QStringLiteral("Missing established baseline pairs"),
                                                    expectedEstablishedPairs - actualPairs);
    const QString unexpectedText =
        formatSetDifference(QStringLiteral("Unexpected cross-group pairs"), actualCrossGroupPairs);
    if (!missingText.isEmpty())
        problems.append(missingText);
    if (!unexpectedText.isEmpty())
        problems.append(unexpectedText);
    return problems.join(QStringLiteral("\n"));
}
} // namespace

#if !defined(VIDEO_SIMILI_LOCAL_VIDEO_MATCHING)
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
    FixtureScan trackedScan(Prefs::USE_CACHE_OPTION mode, const QString& cachePath, int thumbnailMode = cutEnds,
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

FixtureScan TestRepoVideoMatching::trackedScan(Prefs::USE_CACHE_OPTION mode, const QString& cachePath,
                                               int thumbnailMode, bool detectRotatedCopies) const
{
    return scanFixtures(_trackedRoot, _trackedManifest, mode, cachePath, thumbnailMode, detectRotatedCopies);
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
    QVERIFY2(
        totalVideoBytes <= 2000000,
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
    const FixtureScan scan = trackedScan(Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")));
    const ProcessedFixture* base = findFixture(scan, QStringLiteral("matching/a-original.mp4"));
    const ProcessedFixture* metadata = findFixture(scan, QStringLiteral("matching/a-display-matrix.mp4"));
    QVERIFY(base && metadata);
    QVERIFY2(base->processingError.isEmpty(), qPrintable(base->processingError));
    QVERIFY2(metadata->processingError.isEmpty(), qPrintable(metadata->processingError));
    QCOMPARE(metadata->video->width, base->video->width);
    QCOMPARE(metadata->video->height, base->video->height);
    QVERIFY2(
        VideoPairMatcher::match(*base->video, *metadata->video, matchConfig(cutEnds, MATCH_THRESHOLD_BITS)).matches,
        "A Display Matrix-normalized copy should match without physical-rotation fallback");
}

void TestRepoVideoMatching::test_monochromeCutEndsAreSubstituted()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const FixtureScan scan = trackedScan(Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")));
    const ProcessedFixture* base = findFixture(scan, QStringLiteral("matching/a-original.mp4"));
    const ProcessedFixture* monochrome = findFixture(scan, QStringLiteral("matching/a-monochrome-ends.mp4"));
    QVERIFY(base && monochrome);
    QVERIFY2(monochrome->processingError.isEmpty(), qPrintable(monochrome->processingError));
    const VideoPairMatchResult result =
        VideoPairMatcher::match(*base->video, *monochrome->video, matchConfig(cutEnds, MATCH_THRESHOLD_BITS));
    qInfo() << "Monochrome substitute similarities:" << result.phashSimilarity << result.ssimSimilarity
            << "usable" << monochrome->video->fingerprint(0).usable << monochrome->video->fingerprint(1).usable;
    QVERIFY2(
        result.matches,
        "The +/-2% informative substitutes should make the cutEnds copy match A");
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
    const FixtureScan scan = trackedScan(Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")));
    const ProcessedFixture* base = findFixture(scan, QStringLiteral("matching/a-original.mp4"));
    const ProcessedFixture* barred = findFixture(scan, variant);
    QVERIFY(base && barred);
    QVERIFY2(base->processingError.isEmpty(), qPrintable(base->processingError));
    QVERIFY2(barred->processingError.isEmpty(), qPrintable(barred->processingError));
    QVERIFY2(VideoPairMatcher::match(*base->video, *barred->video, matchConfig(cutEnds, MATCH_THRESHOLD_BITS)).matches,
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
    const FixtureScan scan =
        trackedScan(Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")), cutEnds, true);
    const ProcessedFixture* base = findFixture(scan, QStringLiteral("matching/a-original.mp4"));
    const ProcessedFixture* rotated = findFixture(scan, variant);
    QVERIFY(base && rotated);
    QVERIFY2(base->processingError.isEmpty(), qPrintable(base->processingError));
    QVERIFY2(rotated->processingError.isEmpty(), qPrintable(rotated->processingError));

    QVERIFY2(VideoPairMatcher::match(*base->video, *rotated->video, matchConfig(cutEnds, MATCH_THRESHOLD_BITS, true))
                 .matches,
             qPrintable(QStringLiteral("Rotated matching did not recover %1").arg(variant)));
}

void TestRepoVideoMatching::test_samePillarBarsDoNotMatchDifferentContent()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const FixtureScan scan = trackedScan(Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")));
    const ProcessedFixture* a = findFixture(scan, QStringLiteral("matching/a-pillarbox.mp4"));
    const ProcessedFixture* b = findFixture(scan, QStringLiteral("matching/b-pillarbox.mp4"));
    QVERIFY(a && b);
    QVERIFY2(a->processingError.isEmpty(), qPrintable(a->processingError));
    QVERIFY2(b->processingError.isEmpty(), qPrintable(b->processingError));
    const VideoPairMatchResult result =
        VideoPairMatcher::match(*a->video, *b->video, matchConfig(cutEnds, PILLAR_NEGATIVE_THRESHOLD));
    qInfo() << "Tracked same-bars negative pHash similarity:" << result.phashSimilarity;
    QVERIFY2(!result.matches,
             "Different portrait videos with identical pillar bars matched at the issue #138 threshold");
}

void TestRepoVideoMatching::test_trackedEndToEndMatrix_data()
{
    QTest::addColumn<int>("cacheScenario");
    QTest::addColumn<bool>("rotatedMatchingEnabled");
    for (int cacheScenario = 0; cacheScenario < 3; ++cacheScenario) {
        const QString cacheName = cacheScenario == 0   ? QStringLiteral("no-cache")
                                  : cacheScenario == 1 ? QStringLiteral("warm-cache")
                                                       : QStringLiteral("cache-only");
        for (bool rotated : {false, true}) {
            const QString row =
                cacheName + (rotated ? QStringLiteral("-rotation-on") : QStringLiteral("-rotation-off"));
            QTest::newRow(qPrintable(row)) << cacheScenario << rotated;
        }
    }
}

void TestRepoVideoMatching::test_trackedEndToEndMatrix()
{
    QFETCH(int, cacheScenario);
    QFETCH(bool, rotatedMatchingEnabled);
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const QString cachePath = cache.filePath(QStringLiteral("cache.sqlite"));

    if (cacheScenario != 0) {
        const FixtureScan population = trackedScan(Prefs::WITH_CACHE, cachePath, cutEnds, rotatedMatchingEnabled);
        qInfo() << "Tracked cache population:" << population.elapsedMs << "ms";
    }

    const Prefs::USE_CACHE_OPTION mode = cacheScenario == 0   ? Prefs::NO_CACHE
                                         : cacheScenario == 1 ? Prefs::WITH_CACHE
                                                              : Prefs::CACHE_ONLY;
    const FixtureScan scan = trackedScan(mode, cachePath, cutEnds, rotatedMatchingEnabled);
    qInfo() << "Tracked assessed scan:" << scan.elapsedMs << "ms";
    const QString problems = validatePairMatrix(scan, rotatedMatchingEnabled, MATCH_THRESHOLD_BITS);
    QVERIFY2(problems.isEmpty(), qPrintable(problems));
}

QTEST_MAIN(TestRepoVideoMatching)

#else

class TestLocalVideoMatching : public QObject
{
    Q_OBJECT

  private slots:
    void cleanup();
    void test_externalEndToEndMatrix_data();
    void test_externalEndToEndMatrix();
    void test_externalBaselineNoNewPairs();
};

void TestLocalVideoMatching::cleanup()
{
    Prefs().resetSettings();
}

void TestLocalVideoMatching::test_externalEndToEndMatrix()
{
    QFETCH(int, cacheScenario);
    QFETCH(bool, rotatedMatchingEnabled);
    const QString externalRoot = externalFixtureRoot();
    const QString manifestPath = QDir(externalRoot).filePath(QStringLiteral("matching-ground-truth.csv"));
    if (externalRoot.isEmpty() || !QFileInfo::exists(manifestPath))
        QSKIP("Set VIDEO_SIMILI_EXTERNAL_FIXTURES to run the optional external matching corpus");

    QList<MatchingFixtureRecord> manifest;
    QString error;
    QVERIFY2(MatchingFixtureManifest::load(manifestPath, manifest, error), qPrintable(error));
    const QList<MatchingFixtureRecord> featureManifest = recordsWithTag(manifest, QStringLiteral("feature_scope"));
    QCOMPARE(featureManifest.size(), 22);
    qint64 featureFixtureBytes = 0;
    for (const MatchingFixtureRecord& fixture : featureManifest) {
        const QFileInfo file(QDir(externalRoot).filePath(fixture.file));
        QVERIFY2(file.isFile(), qPrintable(QStringLiteral("Missing external fixture: %1").arg(file.filePath())));
        if (fixture.tags.contains(QStringLiteral("feature_fixture")))
            featureFixtureBytes += file.size();
    }
    QVERIFY2(
        featureFixtureBytes <= 250000000,
        qPrintable(QStringLiteral("External derivatives use %1 bytes; budget is 250000000").arg(featureFixtureBytes)));
    qInfo() << "External derivative bytes:" << featureFixtureBytes;

    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const QString cachePath = cache.filePath(QStringLiteral("cache.sqlite"));
    if (cacheScenario != 0) {
        const FixtureScan population =
            scanFixtures(externalRoot, featureManifest, Prefs::WITH_CACHE, cachePath, cutEnds,
                         rotatedMatchingEnabled);
        qInfo() << "External cache population:" << featureManifest.size() << "videos in" << population.elapsedMs
                << "ms";
    }

    const Prefs::USE_CACHE_OPTION mode = cacheScenario == 0   ? Prefs::NO_CACHE
                                         : cacheScenario == 1 ? Prefs::WITH_CACHE
                                                              : Prefs::CACHE_ONLY;
    const FixtureScan scan =
        scanFixtures(externalRoot, featureManifest, mode, cachePath, cutEnds, rotatedMatchingEnabled);
    qInfo() << "External assessed scan:" << featureManifest.size() << "videos in" << scan.elapsedMs << "ms";
    const ProcessedFixture* coast = findFixture(scan, QStringLiteral("Videos/IMG_0689.mp4"));
    const ProcessedFixture* coastPillar =
        findFixture(scan, QStringLiteral("Videos/Matching feature fixtures/coast-pillarbox.mp4"));
    const ProcessedFixture* indoorPillar =
        findFixture(scan, QStringLiteral("Videos/Matching feature fixtures/indoor-pillarbox.mp4"));
    const ProcessedFixture* coastRot90 =
        findFixture(scan, QStringLiteral("Videos/Matching feature fixtures/coast-rot90.mp4"));
    const ProcessedFixture* indoor = findFixture(scan, QStringLiteral("Videos/IMG_4848.mp4"));
    const ProcessedFixture* indoorAligned =
        findFixture(scan, QStringLiteral("Videos/Matching feature fixtures/indoor-duration-aligned.mp4"));
    const ProcessedFixture* img3561a =
        findFixture(scan, QStringLiteral("Videos/subfolder1/subsubfolder/IMG_3561.MOV"));
    const ProcessedFixture* img3561b = findFixture(scan, QStringLiteral("Videos/subfolder2/IMG_3561.MOV"));
    const VideoPairMatchConfig diagnostic = matchConfig(cutEnds, 64, rotatedMatchingEnabled);
    if (coast && coastPillar && indoorPillar && coastRot90 && coast->processingError.isEmpty()
        && coastPillar->processingError.isEmpty() && indoorPillar->processingError.isEmpty()
        && coastRot90->processingError.isEmpty())
    {
        qInfo() << "External current pHash similarities: bar-positive"
                << VideoPairMatcher::match(*coast->video, *coastPillar->video, diagnostic).phashSimilarity
                << "same-bars-negative"
                << VideoPairMatcher::match(*coastPillar->video, *indoorPillar->video, diagnostic).phashSimilarity
                << "physical-90"
                << VideoPairMatcher::match(*coast->video, *coastRot90->video, diagnostic).phashSimilarity;
    }
    if (indoor && indoorAligned && img3561a && img3561b && indoor->processingError.isEmpty()
        && indoorAligned->processingError.isEmpty() && img3561a->processingError.isEmpty()
        && img3561b->processingError.isEmpty())
        qInfo() << "External recovery pHash similarities: indoor-aligned"
                << VideoPairMatcher::match(*indoor->video, *indoorAligned->video, diagnostic).phashSimilarity
                << "IMG_3561"
                << VideoPairMatcher::match(*img3561a->video, *img3561b->video, diagnostic).phashSimilarity;
    const QString problems = validatePairMatrix(scan, rotatedMatchingEnabled, MATCH_THRESHOLD_BITS);
    QVERIFY2(problems.isEmpty(), qPrintable(problems));
}

void TestLocalVideoMatching::test_externalEndToEndMatrix_data()
{
    QTest::addColumn<int>("cacheScenario");
    QTest::addColumn<bool>("rotatedMatchingEnabled");
    QTest::newRow("no-cache-rotation-off") << 0 << false;
    QTest::newRow("no-cache-rotation-on") << 0 << true;
    QTest::newRow("warm-cache-rotation-on") << 1 << true;
    QTest::newRow("cache-only-rotation-on") << 2 << true;
}

void TestLocalVideoMatching::test_externalBaselineNoNewPairs()
{
    const QString externalRoot = externalFixtureRoot();
    const QString manifestPath = QDir(externalRoot).filePath(QStringLiteral("matching-ground-truth.csv"));
    if (externalRoot.isEmpty() || !QFileInfo::exists(manifestPath))
        QSKIP("Set VIDEO_SIMILI_EXTERNAL_FIXTURES to run the optional external matching corpus");

    QList<MatchingFixtureRecord> manifest;
    QString error;
    QVERIFY2(MatchingFixtureManifest::load(manifestPath, manifest, error), qPrintable(error));
    const QList<MatchingFixtureRecord> baseline = recordsWithTag(manifest, QStringLiteral("baseline"));
    QCOMPARE(baseline.size(), 218);

    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const FixtureScan scan =
        scanFixtures(externalRoot, baseline, Prefs::NO_CACHE, cache.filePath(QStringLiteral("cache.sqlite")), cutEnds);
    qInfo() << "External 218-video baseline assessed in" << scan.elapsedMs << "ms";
    const QString problems = validateBaseline(scan);
    QVERIFY2(problems.isEmpty(), qPrintable(problems));
}

QTEST_MAIN(TestLocalVideoMatching)

#endif

#include "tst_video_matching.moc"
