#include <QCoreApplication>
#include <QtTest>

#include <cmath>
#include <memory>
#include <utility>

// add necessary includes here
#include "../../app/comparison/comparison.h"
#include "../../app/comparison/internal/backgroundmatchdiscovery.h"
#include "../../app/comparison/internal/ssim.h"
#include "../../app/comparison/internal/videopairmatcher.h"
#include "../../app/comparison/internal/videopairspace.h"
#include "../../app/videometadata.h"

class CacheModeRestore
{
  public:
    explicit CacheModeRestore(Prefs& prefs) : _prefs(prefs), _originalMode(prefs.useCacheOption()) {}
    ~CacheModeRestore() { _prefs.useCacheOption(_originalMode); }

  private:
    Prefs& _prefs;
    const Prefs::USE_CACHE_OPTION _originalMode;
};

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
#ifdef Q_OS_MACOS
    void test_applePhotosNameLookupIsSynchronousAndSessionOnly();
#endif
    void test_videoPairSpaceRoundTrip();
    void test_videoPairMatcherUsesConfigSnapshot();
    void test_ssimUsesEachBlockForMeans();
    void test_rotatedMatcherRequiresSsimSafeguard();
    void test_rotatedMatcherAppliesDurationModifierToSsimThreshold();
    void test_backgroundDiscoveryFindsMatchesAndCompletesSafePrefix();

  private:
#ifdef Q_OS_MACOS
    std::unique_ptr<Comparison> makeApplePhotosComparison(const QVector<Video*>& videos, Prefs& prefs,
                                                          std::function<QString(const QString&)> lookup);
#endif
};

test_comparison::test_comparison() {}

test_comparison::~test_comparison() {}

void test_comparison::initTestCase() {}

void test_comparison::cleanupTestCase() {}

#ifdef Q_OS_MACOS
std::unique_ptr<Comparison> test_comparison::makeApplePhotosComparison(const QVector<Video*>& videos, Prefs& prefs,
                                                                       std::function<QString(const QString&)> lookup)
{
    auto comparison = std::make_unique<Comparison>(videos, prefs, QRect());
    comparison->_backgroundDiscovery->stop();
    comparison->_videos = videos; // Keep indices deterministic despite the user's saved sort preference.
    comparison->_applePhotosNameLookup = std::move(lookup);
    return comparison;
}
#endif

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

#ifdef Q_OS_MACOS
void test_comparison::test_applePhotosNameLookupIsSynchronousAndSessionOnly()
{
    Prefs prefs;
    CacheModeRestore restoreCacheMode(prefs);
    // Apple Photos names are lightweight display metadata, so even cache-only
    // scans resolve them live when a comparison is shown.
    prefs.useCacheOption(Prefs::CACHE_ONLY);

    const QString foundPhotosPath = QStringLiteral("/Library.photoslibrary/originals/A/ABC123.mov");
    const QString missingPhotosPath = QStringLiteral("/Library.photoslibrary/originals/B/DEF456.mov");
    const QString filesystemName = QStringLiteral("filesystem.mp4");

    Video foundPhotosVideo(prefs, foundPhotosPath);
    Video missingPhotosVideo(prefs, missingPhotosPath);
    Video filesystemVideo(prefs, QStringLiteral("/tmp/") + filesystemName);
    foundPhotosVideo.size = 3;
    missingPhotosVideo.size = 2;
    filesystemVideo.size = 1;
    QVector<Video*> videos = {&foundPhotosVideo, &missingPhotosVideo, &filesystemVideo};

    QStringList lookedUpIds;
    auto comparison = makeApplePhotosComparison(videos, prefs, [&lookedUpIds](const QString& id) {
        lookedUpIds.append(id);
        if (id == QStringLiteral("ABC123"))
            return QStringLiteral("Name from Apple Photos.mov");
        return QStringLiteral(OBJ_C_FAILURE_STRING);
    });
    QSignalSpy statusMessages(comparison.get(), &Comparison::sendStatusMessage);

    // The lookup resolves inline before the filename label is painted. The
    // regular filesystem video does not trigger PhotoKit.
    comparison->displayMatchedPair({0, 2, 1, 0, 0.0});
    QCOMPARE(lookedUpIds, QStringList({QStringLiteral("ABC123")}));
    QCOMPARE(foundPhotosVideo.nameInApplePhotos, QStringLiteral("Name from Apple Photos.mov"));
    QCOMPARE(comparison->findChild<ClickableLabel*>(QStringLiteral("leftFileName"))->text(),
             QStringLiteral("Name from Apple Photos.mov"));
    QCOMPARE(comparison->findChild<ClickableLabel*>(QStringLiteral("rightFileName"))->text(), filesystemName);

    // The Video object retains a successful name for this comparison session.
    comparison->showVideo(QStringLiteral("left"));
    QCOMPARE(lookedUpIds.size(), 1);

    // A missing asset immediately falls back to the on-disk UUID filename.
    comparison->displayMatchedPair({1, 2, 2, 0, 0.0});
    QCOMPARE(lookedUpIds, QStringList({QStringLiteral("ABC123"), QStringLiteral("DEF456")}));
    QCOMPARE(statusMessages.count(), 1);
    QVERIFY(missingPhotosVideo.nameInApplePhotos.isEmpty());
    QCOMPARE(comparison->findChild<ClickableLabel*>(QStringLiteral("leftFileName"))->text(),
             QStringLiteral("DEF456.mov"));
}

#endif

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
    left.fingerprint(0).phash = 0xAAAAAAAAAAAAAAAA;
    right.fingerprint(0).phash = 0xAAAAAAAAAAAAAAAB;
    left.fingerprint(0).usable = true;
    right.fingerprint(0).usable = true;
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

void test_comparison::test_ssimUsesEachBlockForMeans()
{
    cv::Mat left(16, 16, CV_8UC1);
    cv::Mat right(16, 16, CV_8UC1);
    for (int row = 0; row < 16; ++row) {
        for (int column = 0; column < 16; ++column) {
            left.at<uchar>(row, column) = static_cast<uchar>((row * 17 + column * 7) % 191);
            right.at<uchar>(row, column) = static_cast<uchar>((row * 13 + column * 11 + 29) % 191);
        }
    }

    // These non-default block sizes expose an indexing error that sampled the
    // means from (k, l) instead of each block's origin (k * blockSize, l * blockSize).
    QVERIFY(std::abs(Ssim::calculate(left, right, 2) - 0.65639372860369649) < 1e-9);
    QVERIFY(std::abs(Ssim::calculate(left, right, 4) - 0.46552455674718085) < 1e-9);
    QVERIFY(std::abs(Ssim::calculate(left, right, 8) - 0.46507329051917068) < 1e-9);
}

void test_comparison::test_rotatedMatcherRequiresSsimSafeguard()
{
    Prefs prefs;
    Video left(prefs, QStringLiteral("left"));
    Video right(prefs, QStringLiteral("right"));
    left.duration = right.duration = 1000;
    left.fingerprint(0).usable = true;
    right.fingerprint(0).usable = true;
    right.fingerprint(0, FingerprintRotation::clockwise90).usable = true;
    left.fingerprint(0).phash = 0;
    right.fingerprint(0).phash = UINT64_MAX;
    right.fingerprint(0, FingerprintRotation::clockwise90).phash = 0;
    right.fingerprint(0, FingerprintRotation::clockwise90).ssimPixels.fill(255);

    VideoPairMatchConfig config;
    config.thumbnailsMode = thumb1;
    config.comparisonMode = Prefs::_PHASH;
    config.thresholdPhash = 57;
    config.sameDurationModifier = 0;
    config.differentDurationModifier = 0;
    config.detectRotatedCopies = true;
    QVERIFY(!VideoPairMatcher::match(left, right, config).matches);

    right.fingerprint(0, FingerprintRotation::clockwise90).ssimPixels = left.fingerprint(0).ssimPixels;
    const VideoPairMatchResult result = VideoPairMatcher::match(left, right, config);
    QVERIFY(result.matches);
}

void test_comparison::test_rotatedMatcherAppliesDurationModifierToSsimThreshold()
{
    Prefs prefs;
    Video left(prefs, QStringLiteral("left"));
    Video right(prefs, QStringLiteral("right"));
    VisualFingerprint& leftFingerprint = left.fingerprint(0);
    VisualFingerprint& rotatedFingerprint = right.fingerprint(0, FingerprintRotation::clockwise90);
    leftFingerprint.usable = rotatedFingerprint.usable = true;
    leftFingerprint.phash = rotatedFingerprint.phash = 0;
    for (size_t index = 0; index < leftFingerprint.ssimPixels.size(); ++index)
        leftFingerprint.ssimPixels[index] = rotatedFingerprint.ssimPixels[index] = static_cast<uint8_t>(index);
    rotatedFingerprint.ssimPixels[0] = 32;

    const cv::Mat leftPixels(16, 16, CV_8UC1, leftFingerprint.ssimPixels.data());
    const cv::Mat rightPixels(16, 16, CV_8UC1, rotatedFingerprint.ssimPixels.data());
    const double rawSsim = Ssim::calculate(leftPixels, rightPixels, 16);
    QVERIFY(rawSsim > 0.90);
    QVERIFY(rawSsim < 1.0);

    VideoPairMatchConfig config;
    config.thumbnailsMode = thumb1;
    config.comparisonMode = Prefs::_SSIM;
    config.thresholdPhash = 57;
    config.detectRotatedCopies = true;

    left.duration = right.duration = 1000;
    config.sameDurationModifier = 1;
    config.thresholdSSIM = rawSsim + 0.5 / 64.0;
    QVERIFY(VideoPairMatcher::match(left, right, config).matches);

    right.duration = 3000;
    config.differentDurationModifier = 4;
    config.thresholdSSIM = rawSsim - 2.0 / 64.0;
    QVERIFY(!VideoPairMatcher::match(left, right, config).matches);
}

void test_comparison::test_backgroundDiscoveryFindsMatchesAndCompletesSafePrefix()
{
    Prefs prefs;
    Video first(prefs, QStringLiteral("first"));
    Video second(prefs, QStringLiteral("second"));
    Video third(prefs, QStringLiteral("third"));
    Video fourth(prefs, QStringLiteral("fourth"));
    first.fingerprint(0).phash = fourth.fingerprint(0).phash = 0xFF00FF00FF00FF00;
    second.fingerprint(0).phash = 0xAAAAAAAAAAAAAAAA;
    third.fingerprint(0).phash = 0x5555555555555555;
    first.fingerprint(0).usable = second.fingerprint(0).usable = third.fingerprint(0).usable =
        fourth.fingerprint(0).usable = true;

    VideoPairMatchConfig config;
    config.thumbnailsMode = thumb1;
    config.comparisonMode = Prefs::_PHASH;
    config.thresholdPhash = 64;
    config.sameDurationModifier = 0;
    config.differentDurationModifier = 0;

    BackgroundMatchDiscovery discovery(1, 3);
    discovery.start({&first, &second, &third, &fourth}, config);

    QTRY_COMPARE_WITH_TIMEOUT(discovery.preScannedEnd(), 6, 5000);
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
