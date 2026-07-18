#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest>

// add necessary includes here
#include "../../app/comparison/comparison.h"
#include "../../app/comparison/internal/backgroundmatchdiscovery.h"
#include "../../app/comparison/internal/videopairmatcher.h"
#include "../../app/comparison/internal/videopairspace.h"
#include "../../app/videometadata.h"

class test_comparison : public QObject
{
    Q_OBJECT

  public:
    test_comparison();
    ~test_comparison();

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_videoToDelete_OnlyTimeDiffs();
    void test_videoPairSpaceRoundTrip();
    void test_videoPairMatcherUsesConfigSnapshot();
    void test_backgroundDiscoveryFindsMatchesAndCompletesSafePrefix();
};

test_comparison::test_comparison() {}

test_comparison::~test_comparison() {}

void test_comparison::initTestCase() {}

void test_comparison::cleanupTestCase() {}

void test_comparison::test_videoToDelete_OnlyTimeDiffs()
{
    QDateTime refDate(QDate(2000, 1, 1), QTime(1, 0, 0));
    QDateTime earlierDate(QDate(1999, 1, 1), QTime(1, 0, 0));
    QDateTime laterDate(QDate(2001, 1, 1), QTime(1, 0, 0));

    VideoMetadata meta1, meta2;
    meta1.filename = meta2.filename = "/users/test/videos/vid1.mp4";
    meta1.size = meta2.size = 36 * 10 * 1024;
    meta1.duration = meta2.duration = 36 * 1000;
    meta1.width = meta2.width = 1080;
    meta1.height = meta2.height = 720;
    meta1.framerate = meta2.framerate = 30.31;
    meta1.codec = meta2.codec = "hevc";
    meta1.bitrate = meta2.bitrate = 10 * 1024;
    meta1.audio = meta2.audio = "mp3";
    meta1.nameInApplePhotos = meta2.nameInApplePhotos = "";
    meta1.modified = meta2.modified = meta1._fileCreateDate = meta2._fileCreateDate =
        QDateTime(QDate(2000, 1, 1), QTime(1, 0, 0));

    Comparison::AutoDeleteConfig autoDelConf(Comparison::AUTO_DELETE_ONLY_TIMES_DIFF);
    Comparison::AutoDeleteUserSettings userSet(false); // trash later video as is default

    //check default case, all exact same so should not be covered in this auto mode
    const VideoMetadata* vidToDeleteMetaPtr = autoDelConf.videoToDelete(&meta1, &meta2, userSet);
    QVERIFY2(vidToDeleteMetaPtr == nullptr, "Vids date metadata same but auto date comparison said they're different");

    meta1._fileCreateDate = earlierDate;
    meta2._fileCreateDate = laterDate;
    QVERIFY2(autoDelConf.videoToDelete(&meta1, &meta2, userSet) == &meta2,
             "Should have delete later creation date video but selected ealier");

    meta1._fileCreateDate = laterDate;
    meta2._fileCreateDate = earlierDate;
    QVERIFY2(autoDelConf.videoToDelete(&meta1, &meta2, userSet) == &meta1,
             "Should have delete later creation date video but selected ealier");

    meta1._fileCreateDate = refDate;
    meta2._fileCreateDate = refDate;
    meta1.modified = earlierDate;
    meta2.modified = laterDate;
    QVERIFY2(autoDelConf.videoToDelete(&meta1, &meta2, userSet) == &meta2,
             "Should have delete later modified date video but selected ealier");

    meta1._fileCreateDate = refDate;
    meta2._fileCreateDate = refDate;
    meta1.modified = laterDate;
    meta2.modified = earlierDate;
    QVERIFY2(autoDelConf.videoToDelete(&meta1, &meta2, userSet) == &meta1,
             "Should have delete later modified date video but selected ealier");

    meta1._fileCreateDate = earlierDate;
    meta2._fileCreateDate = laterDate;
    meta1.modified = laterDate;
    meta2.modified = earlierDate;
    QVERIFY2(autoDelConf.videoToDelete(&meta1, &meta2, userSet) == &meta2,
             "Later creation date should be deleted but instead later modified date or else was");

    // TODO could add more interesting tests with small differences, and check more specifically the outcomes
}

void test_comparison::test_videoPairSpaceRoundTrip()
{
    QCOMPARE(VideoPairSpace::comparisonCount(0), 0);
    QCOMPARE(VideoPairSpace::comparisonCount(1), 0);
    QCOMPARE(VideoPairSpace::comparisonCount(4), 6);

    const QVector<VideoPairPosition> expected = {
        {0, 1, 1}, {0, 2, 2}, {0, 3, 3}, {1, 2, 4}, {1, 3, 5}, {2, 3, 6},
    };
    for (const auto& pair : expected) {
        const auto resolved = VideoPairSpace::pairAtPosition(4, pair.position);
        QCOMPARE(resolved.left, pair.left);
        QCOMPARE(resolved.right, pair.right);
        QCOMPARE(VideoPairSpace::positionForPair(4, pair.left, pair.right), pair.position);
    }

    auto forward = expected.first();
    for (int index = 0; index < expected.size(); ++index) {
        QCOMPARE(forward.left, expected[index].left);
        QCOMPARE(forward.right, expected[index].right);
        QCOMPARE(forward.position, expected[index].position);
        if (index + 1 < expected.size())
            VideoPairSpace::advancePair(4, forward);
    }

    auto backward = expected.last();
    for (int index = expected.size() - 1; index >= 0; --index) {
        QCOMPARE(backward.left, expected[index].left);
        QCOMPARE(backward.right, expected[index].right);
        QCOMPARE(backward.position, expected[index].position);
        if (index > 0)
            VideoPairSpace::retreatPair(4, backward);
    }
}

void test_comparison::test_videoPairMatcherUsesConfigSnapshot()
{
    Prefs prefs;
    Video left(prefs, QStringLiteral("left"));
    Video right(prefs, QStringLiteral("right"));
    left.hash[0] = 0xAAAAAAAAAAAAAAAA;
    right.hash[0] = 0xAAAAAAAAAAAAAAAB;
    left.duration = right.duration = 1000;

    VideoPairMatchConfig config;
    config.thumbnailsMode = thumb1;
    config.comparisonMode = Prefs::_PHASH;
    config.sameDurationModifier = 0;
    config.differentDurationModifier = 0;
    config.thresholdPhash = 64;
    QVERIFY(!VideoPairMatcher::match(left, right, config).matches);

    config.thresholdPhash = 63;
    const auto result = VideoPairMatcher::match(left, right, config);
    QVERIFY(result.matches);
    QCOMPARE(result.phashSimilarity, 63);
}

void test_comparison::test_backgroundDiscoveryFindsMatchesAndCompletesSafePrefix()
{
    Prefs prefs;
    Video first(prefs, QStringLiteral("first"));
    Video second(prefs, QStringLiteral("second"));
    Video third(prefs, QStringLiteral("third"));
    Video fourth(prefs, QStringLiteral("fourth"));
    first.hash[0] = fourth.hash[0] = 0xFF00FF00FF00FF00;
    second.hash[0] = 0xAAAAAAAAAAAAAAAA;
    third.hash[0] = 0x5555555555555555;

    VideoPairMatchConfig config;
    config.thumbnailsMode = thumb1;
    config.comparisonMode = Prefs::_PHASH;
    config.thresholdPhash = 64;
    config.sameDurationModifier = 0;
    config.differentDurationModifier = 0;

    BackgroundMatchDiscovery discovery(1, 3);
    QSignalSpy finishedSpy(&discovery, &BackgroundMatchDiscovery::finished);
    discovery.start({&first, &second, &third, &fourth}, config);

    QTRY_COMPARE_WITH_TIMEOUT(discovery.safeEnd(), 6, 5000);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(discovery.discoveredMatchCount(), 1);

    const auto next = discovery.nextCandidateAfter(0);
    QVERIFY(next.has_value());
    QCOMPARE(next->left, 0);
    QCOMPARE(next->right, 3);
    QCOMPARE(next->position, 3);
    QVERIFY(!discovery.nextCandidateAfter(next->position).has_value());

    const auto previous = discovery.previousCandidateBefore(7);
    QVERIFY(previous.has_value());
    QCOMPARE(previous->position, 3);
}

QTEST_MAIN(test_comparison)

#include "tst_test_comparison.moc"
