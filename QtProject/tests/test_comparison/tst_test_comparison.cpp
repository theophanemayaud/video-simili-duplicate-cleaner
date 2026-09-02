#include <QBuffer>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QListWidget>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

// add necessary includes here
#include "../../app/comparison/comparison.h"
#include "../../app/comparison/internal/backgroundmatchdiscovery.h"
#include "../../app/comparison/internal/duplicatesetbuilder.h"
#include "../../app/comparison/internal/ssim.h"
#include "../../app/comparison/internal/videopairmatcher.h"
#include "../../app/comparison/internal/videopairspace.h"
#include "../../app/db.h"
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

class CachePathRestore
{
  public:
    explicit CachePathRestore(Prefs& prefs) : _prefs(prefs), _originalPath(prefs.cacheFilePathName()) {}
    ~CachePathRestore() { _prefs.cacheFilePathName(_originalPath); }

  private:
    Prefs& _prefs;
    const QString _originalPath;
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
    void test_applePhotosNameLookupReportsRefusedAccess();
#endif
    void test_videoPairSpaceRoundTrip();
    void test_videoPairMatcherUsesConfigSnapshot();
    void test_ssimUsesEachBlockForMeans();
    void test_rotatedMatcherRequiresSsimSafeguard();
    void test_rotatedMatcherAppliesDurationModifierToSsimThreshold();
    void test_manualSetBrowserGeometryAtMinimumSize();
    void test_ignoredPairsLoadNormalized();
    void test_rebuildDuplicateSetsExcludesIgnoredEdge();
    void test_missingSetMemberRebuildsSynchronously();
    void test_missingBridgeRebuildsSynchronously();
    void test_discoveryPublishesSetsOnlyWhenComplete();
    void test_selectedMemberCanBecomeReferenceForDirectEdge();
    void test_selectedReferencePersistsAcrossExplicitRebuild();
    void test_activeSetRemainsActionableWhenAnotherSetIsStale();
    void test_stoppedDiscoveryCannotReplaceCleanupPairIndexes();
    void test_cleanupCompletionDoesNotNavigateForeground();
    void test_rebuildKeepsActiveSetWhenSelectedMemberDisappears();
    void test_duplicateSetBuilderConnectedComponents();
    void test_backgroundDiscoveryHidesOutOfOrderMatchesUntilPrefixCompletes();
    void test_backgroundDiscoveryFindsMatchesAndCompletesSafePrefix();

  private:
    void markDiscoveryComplete(Comparison& comparison, int64_t maxPosition);
#ifdef Q_OS_MACOS
    std::unique_ptr<Comparison> makeApplePhotosComparison(const QVector<Video*>& videos, Prefs& prefs,
                                                          std::function<QString(const QString&)> lookup);
#endif
};

test_comparison::test_comparison() {}

test_comparison::~test_comparison() {}

void test_comparison::initTestCase() {}

void test_comparison::cleanupTestCase() {}

void test_comparison::markDiscoveryComplete(Comparison& comparison, int64_t maxPosition)
{
    comparison._backgroundDiscovery->_maxPosition = maxPosition;
    comparison._backgroundDiscovery->_lastContiguousScannedPairPosition = maxPosition;
    comparison._backgroundDiscovery->_started = true;
}

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
    QVERIFY(statusMessages.takeFirst().first().toString().contains(QStringLiteral("System Photo Library")));
}

void test_comparison::test_applePhotosNameLookupReportsRefusedAccess()
{
    Prefs prefs;
    Video photosVideo(prefs, QStringLiteral("/Library.photoslibrary/originals/A/ABC123.mov"));
    Video filesystemVideo(prefs, QStringLiteral("/tmp/filesystem.mp4"));
    photosVideo.size = 2;
    filesystemVideo.size = 1;
    QVector<Video*> videos = {&photosVideo, &filesystemVideo};

    auto comparison = makeApplePhotosComparison(
        videos, prefs, [](const QString&) { return QStringLiteral(OBJ_C_NO_PHOTOS_ACCESS_STRING); });
    QSignalSpy statusMessages(comparison.get(), &Comparison::sendStatusMessage);

    // Refused access is reported as actionable, not as an unknown lookup error.
    comparison->displayMatchedPair({0, 1, 1, 0, 0.0});
    QVERIFY(photosVideo.nameInApplePhotos.isEmpty());
    QCOMPARE(statusMessages.count(), 1);
    const QString message = statusMessages.takeFirst().first().toString();
    QVERIFY(message.contains(QStringLiteral("access to the library was refused")));
    QVERIFY(message.contains(QStringLiteral("Privacy & Security")));
    QCOMPARE(comparison->findChild<ClickableLabel*>(QStringLiteral("leftFileName"))->text(),
             QStringLiteral("ABC123.mov"));
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

void test_comparison::test_manualSetBrowserGeometryAtMinimumSize()
{
    Prefs prefs;
    prefs._numberOfVideos = 3;
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString referencePath = fixture.filePath(QStringLiteral("A-reference.mp4"));
    const QString selectedPath = fixture.filePath(QStringLiteral("A-copy-3.mp4"));
    const QString otherPath = fixture.filePath(QStringLiteral("A-copy-2.mp4"));
    for (const QString& path : {referencePath, selectedPath, otherPath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }
    Video reference(prefs, referencePath);
    Video selected(prefs, selectedPath);
    Video other(prefs, otherPath);
    reference.size = 30 * 1024;
    selected.size = 20 * 1024;
    other.size = 10 * 1024;

    const QDateTime timestamp(QDate(2026, 8, 23), QTime(10, 41, 38));
    for (Video* video : QVector<Video*>{&reference, &selected, &other}) {
        video->duration = 10 * 1000;
        video->width = 40;
        video->height = 160;
        video->framerate = 10;
        video->bitrate = 8;
        video->codec = QStringLiteral("h264");
        video->modified = video->_fileCreateDate = timestamp;
        video->meta.additionalMetadata = {
            {QStringLiteral("compatible_brands"), QStringLiteral("isomiso2avc1mp41")},
            {QStringLiteral("encoder"), QStringLiteral("Lavf62.3.100")},
            {QStringLiteral("major_brand"), QStringLiteral("isom")},
            {QStringLiteral("minor_version"), QStringLiteral("512")},
        };
    }

    QImage thumbnailPreview(448, 336, QImage::Format_RGB32);
    thumbnailPreview.fill(Qt::blue);
    QByteArray thumbnail;
    QBuffer thumbnailBuffer(&thumbnail);
    thumbnailBuffer.open(QIODevice::WriteOnly);
    QVERIFY(thumbnailPreview.save(&thumbnailBuffer, "JPG"));
    reference.thumbnail = selected.thumbnail = other.thumbnail = thumbnail;

    Comparison comparison({&reference, &selected, &other}, prefs, QRect(0, 0, 1120, 720));
    comparison._backgroundDiscovery->stop();
    comparison._videos = {&reference, &selected, &other};
    comparison.resize(1120, 720);
    comparison.show();
    QCoreApplication::processEvents();

    comparison._backgroundDiscovery->_matches = {
        {1, MatchedVideoPair{0, 1, 1, 64, 1.0}},
        {2, MatchedVideoPair{0, 2, 2, 64, 1.0}},
    };
    markDiscoveryComplete(comparison, 2);
    comparison.rebuildDuplicateSets();
    QCoreApplication::processEvents();

    auto* sets = comparison.findChild<QListWidget*>(QStringLiteral("duplicateSets"));
    auto* leftImage = comparison.findChild<ClickableLabel*>(QStringLiteral("leftImage"));
    auto* rightImage = comparison.findChild<ClickableLabel*>(QStringLiteral("rightImage"));
    auto* leftFileName = comparison.findChild<ClickableLabel*>(QStringLiteral("leftFileName"));
    auto* rightFileName = comparison.findChild<ClickableLabel*>(QStringLiteral("rightFileName"));
    auto* leftPaneTitle = comparison.findChild<QLabel*>(QStringLiteral("leftPaneTitle"));
    auto* rightPaneTitle = comparison.findChild<QLabel*>(QStringLiteral("rightPaneTitle"));
    auto* membersTitle = comparison.findChild<QWidget*>(QStringLiteral("duplicateSetMembersTitle"));
    auto* useAsReference = comparison.findChild<QPushButton*>(QStringLiteral("useSelectedAsReferenceButton"));
    auto* previous = comparison.findChild<QPushButton*>(QStringLiteral("prevVideo"));
    auto* leftDelete = comparison.findChild<QPushButton*>(QStringLiteral("leftDelete"));
    auto* rightDelete = comparison.findChild<QPushButton*>(QStringLiteral("rightDelete"));
    auto* next = comparison.findChild<QPushButton*>(QStringLiteral("nextVideo"));

    QVERIFY(sets);
    QVERIFY(leftImage);
    QVERIFY(rightImage);
    QVERIFY(leftFileName);
    QVERIFY(rightFileName);
    QVERIFY(leftPaneTitle);
    QVERIFY(rightPaneTitle);
    QVERIFY(membersTitle);
    QVERIFY(useAsReference);
    QVERIFY(previous);
    QVERIFY(leftDelete);
    QVERIFY(rightDelete);
    QVERIFY(next);
    QCOMPARE(comparison.minimumHeight(), 720);
    QCOMPARE(comparison.height(), 720);
    QCOMPARE(leftPaneTitle->text(), QStringLiteral("Reference"));
    QCOMPARE(rightPaneTitle->text(), QStringLiteral("Selected member"));
    QVERIFY(useAsReference->isEnabled());
    QVERIFY(previous->toolTip().isEmpty());
    QVERIFY(leftDelete->toolTip().isEmpty());
    QVERIFY(rightDelete->toolTip().isEmpty());
    QVERIFY(next->toolTip().isEmpty());

    QVERIFY2(leftImage->mapTo(&comparison, QPoint(0, leftImage->height())).y()
                 <= leftFileName->mapTo(&comparison, QPoint()).y(),
             "The reference preview must end before its file-name row starts.");
    QVERIFY2(rightImage->mapTo(&comparison, QPoint(0, rightImage->height())).y()
                 <= rightFileName->mapTo(&comparison, QPoint()).y(),
             "The selected-member preview must end before its file-name row starts.");

    const auto previewsFitLabels = [leftImage, rightImage]() {
        return leftImage->pixmap().width() <= leftImage->contentsRect().width()
               && leftImage->pixmap().height() <= leftImage->contentsRect().height()
               && rightImage->pixmap().width() <= rightImage->contentsRect().width()
               && rightImage->pixmap().height() <= rightImage->contentsRect().height();
    };
    QVERIFY2(previewsFitLabels(), "Displayed preview pixmaps must fit their labels at minimum size.");

    auto* members = comparison.findChild<QListWidget*>(QStringLiteral("duplicateSetMembers"));
    QVERIFY(members);
    const QSize sourceThumbnailSize(448, 336);
    QCOMPARE(sets->item(0)->icon().availableSizes(),
             QList<QSize>{sourceThumbnailSize.scaled(sets->iconSize(), Qt::KeepAspectRatio)});
    QCOMPARE(members->item(0)->icon().availableSizes(),
             QList<QSize>{sourceThumbnailSize.scaled(members->iconSize(), Qt::KeepAspectRatio)});
    QCOMPARE(members->item(0)->text(), QStringLiteral("Reference: A-reference.mp4"));
    QCOMPARE(members->item(1)->text(), QStringLiteral("Selected: A-copy-3.mp4"));
    QVERIFY(!members->item(0)->text().contains('\n'));
    QVERIFY(!members->item(1)->text().contains('\n'));
    const QRect referenceCard = members->visualItemRect(members->item(0));
    const QRect selectedCard = members->visualItemRect(members->item(1));
    QVERIFY2(referenceCard.right() < selectedCard.left(), "Member cards must not overlap horizontally.");
    const auto left = [&comparison](QWidget* widget) { return widget->mapTo(&comparison, QPoint()).x(); };
    const auto right = [&left](QWidget* widget) { return left(widget) + widget->width(); };
    const auto topAtMinimum = [&comparison](QWidget* widget) { return widget->mapTo(&comparison, QPoint()).y(); };
    const auto bottomAtMinimum = [&topAtMinimum](QWidget* widget) { return topAtMinimum(widget) + widget->height(); };
    QVERIFY2(right(membersTitle) <= left(useAsReference), "The reference control must not overlap the Members title.");
    QVERIFY2(bottomAtMinimum(useAsReference) <= topAtMinimum(members),
             "The reference control must stay above the member cards at minimum size.");

    comparison.resize(1228, 768);
    QCoreApplication::processEvents();
    QVERIFY2(previewsFitLabels(), "Displayed preview pixmaps must fit their labels after resizing.");

    auto* metadata = comparison.findChild<QWidget*>(QStringLiteral("textEdit_leftMetadata"));
    auto* evidence = comparison.findChild<QWidget*>(QStringLiteral("duplicateSetEvidence"));
    auto* swap = comparison.findChild<QWidget*>(QStringLiteral("swapFilenames"));
    auto* manualTab = comparison.findChild<QWidget*>(QStringLiteral("tabManual"));
    const auto top = [&comparison](QWidget* widget) { return widget->mapTo(&comparison, QPoint()).y(); };
    const auto bottom = [&top](QWidget* widget) { return top(widget) + widget->height(); };
    QVERIFY2(bottom(metadata) <= top(membersTitle), "Metadata must not overlap the member controls.");
    QVERIFY2(bottom(members) <= top(evidence), "Member cards must not overlap their evidence label.");
    QVERIFY2(bottom(evidence) <= top(previous), "Evidence must not overlap the review actions.");
    QVERIFY2(bottom(swap) < bottom(manualTab), "Review actions must remain inside the manual tab.");
}

void test_comparison::test_ignoredPairsLoadNormalized()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(fixture.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    Db cache(prefs.cacheFilePathName());
    cache.writePairToIgnore(QStringLiteral("z-video.mp4"), QStringLiteral("a-video.mp4"));
    cache.writePairToIgnore(QStringLiteral("b-video.mp4"), QStringLiteral("c-video.mp4"));
    const QVector<QPair<QString, QString>> ignored = cache.ignoredPairs();

    QCOMPARE(ignored.size(), 2);
    QVERIFY(ignored.contains({QStringLiteral("a-video.mp4"), QStringLiteral("z-video.mp4")}));
    QVERIFY(ignored.contains({QStringLiteral("b-video.mp4"), QStringLiteral("c-video.mp4")}));
}

void test_comparison::test_rebuildDuplicateSetsExcludesIgnoredEdge()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(fixture.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    std::vector<std::unique_ptr<Video>> ownedVideos;
    QVector<Video*> videos;
    for (const QString& name : {QStringLiteral("first.mp4"), QStringLiteral("second.mp4"), QStringLiteral("third.mp4")})
    {
        const QString path = fixture.filePath(name);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        ownedVideos.push_back(std::make_unique<Video>(prefs, path));
        videos.append(ownedVideos.back().get());
    }
    Db(prefs.cacheFilePathName()).writePairToIgnore(videos[0]->_filePathName, videos[1]->_filePathName);

    Comparison comparison(videos, prefs, QRect(0, 0, 1120, 720));
    comparison._backgroundDiscovery->stop();
    comparison._videos = videos;
    comparison._backgroundDiscovery->_matches = {
        {1, MatchedVideoPair{0, 1, 1, 64, 1.0}},
        {2, MatchedVideoPair{1, 2, 2, 64, 1.0}},
    };
    markDiscoveryComplete(comparison, 2);

    comparison.rebuildDuplicateSets();

    QCOMPARE(comparison._duplicateSets.size(), 1);
    QCOMPARE(comparison._duplicateSets.first().members, QVector<int>({1, 2}));
    QCOMPARE(comparison._duplicateSets.first().edges.size(), 1);
    QCOMPARE(comparison._duplicateSets.first().edges.first().left, 1);
    QCOMPARE(comparison._duplicateSets.first().edges.first().right, 2);
}

void test_comparison::test_missingSetMemberRebuildsSynchronously()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString referencePath = fixture.filePath(QStringLiteral("reference.mp4"));
    const QString candidatePath = fixture.filePath(QStringLiteral("candidate.mp4"));
    for (const QString& path : {referencePath, candidatePath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    Prefs prefs;
    prefs._numberOfVideos = 2;
    Video reference(prefs, referencePath);
    Video candidate(prefs, candidatePath);
    Comparison comparison({&reference, &candidate}, prefs, QRect(0, 0, 1120, 720));
    comparison._backgroundDiscovery->stop();
    comparison._videos = {&reference, &candidate};
    comparison._backgroundDiscovery->_matches = {{1, MatchedVideoPair{0, 1, 1, 64, 1.0}}};
    markDiscoveryComplete(comparison, 1);
    comparison.rebuildDuplicateSets();
    QVERIFY(comparison.findChild<QWidget*>(QStringLiteral("leftDelete"))->isEnabled());

    QVERIFY(QFile::remove(candidatePath));
    comparison.showSetMember(1);
    QVERIFY(!comparison.findChild<QWidget*>(QStringLiteral("leftDelete"))->isEnabled());
    QVERIFY(!comparison.findChild<QWidget*>(QStringLiteral("rightDelete"))->isEnabled());
    QVERIFY(comparison._duplicateSets.isEmpty());
    QVERIFY(!comparison.hasActiveManualComparison());
}

void test_comparison::test_missingBridgeRebuildsSynchronously()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(fixture.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    std::vector<std::unique_ptr<Video>> ownedVideos;
    QVector<Video*> videos;
    for (const QString& name : {QStringLiteral("A.mp4"), QStringLiteral("B.mp4"), QStringLiteral("C.mp4")}) {
        const QString path = fixture.filePath(name);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        ownedVideos.push_back(std::make_unique<Video>(prefs, path));
        videos.append(ownedVideos.back().get());
    }

    Comparison comparison(videos, prefs, QRect(0, 0, 1120, 720));
    comparison._backgroundDiscovery->stop();
    comparison._videos = videos;
    comparison._backgroundDiscovery->_matches = {
        {1, MatchedVideoPair{0, 1, 1, 64, 1.0}},
        {2, MatchedVideoPair{1, 2, 2, 64, 1.0}},
    };
    markDiscoveryComplete(comparison, 2);
    comparison.rebuildDuplicateSets();
    QCOMPARE(comparison._duplicateSets.first().members, QVector<int>({0, 1, 2}));
    QVERIFY(comparison.findChild<QWidget*>(QStringLiteral("leftDelete"))->isEnabled());

    QVERIFY(QFile::remove(videos[1]->_filePathName));
    comparison.showSetMember(2);
    comparison.showSetMember(2);

    QVERIFY(!comparison.findChild<QWidget*>(QStringLiteral("leftDelete"))->isEnabled());
    QVERIFY(!comparison.findChild<QWidget*>(QStringLiteral("rightDelete"))->isEnabled());
    QVERIFY(!comparison.findChild<QWidget*>(QStringLiteral("ignoreDuplicatePairButton"))->isEnabled());
    QVERIFY(!comparison.findChild<QWidget*>(QStringLiteral("useSelectedAsReferenceButton"))->isEnabled());
    QVERIFY(comparison._duplicateSets.isEmpty());
    QVERIFY(!comparison.hasActiveManualComparison());
}

void test_comparison::test_discoveryPublishesSetsOnlyWhenComplete()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(fixture.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    std::vector<std::unique_ptr<Video>> ownedVideos;
    QVector<Video*> videos;
    for (int index = 0; index < 3; ++index) {
        const QString path = fixture.filePath(QStringLiteral("video-%1.mp4").arg(index));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        ownedVideos.push_back(std::make_unique<Video>(prefs, path));
        videos.append(ownedVideos.back().get());
    }

    Comparison comparison(videos, prefs, QRect(0, 0, 1120, 720));
    comparison._backgroundDiscovery->stop();
    comparison._videos = videos;
    comparison.findChild<QTabWidget*>(QStringLiteral("tabWidget"))->setCurrentIndex(1);
    comparison._backgroundDiscovery->_matches = {{1, MatchedVideoPair{0, 1, 1, 64, 1.0}}};
    comparison._backgroundDiscovery->_lastContiguousScannedPairPosition = 1;
    comparison._backgroundDiscovery->_maxPosition = 3;
    comparison._backgroundDiscovery->_started = true;

    comparison.updateDiscoveryProgress(1);

    auto* sets = comparison.findChild<QListWidget*>(QStringLiteral("duplicateSets"));
    auto* status = comparison.findChild<QLabel*>(QStringLiteral("duplicateSetsStatus"));
    auto* progress = comparison.findChild<QProgressBar*>(QStringLiteral("progressBar"));
    QVERIFY(sets);
    QVERIFY(status);
    QVERIFY(progress);
    QVERIFY(comparison._duplicateSets.isEmpty());
    QVERIFY(!sets->isEnabled());
    QCOMPARE(status->text(), QStringLiteral("Scanning duplicate sets…"));
    QCOMPARE(comparison._leftVideo, 0);
    QCOMPARE(comparison._rightVideo, 0);
    QCOMPARE(progress->value(), comparison.progressBarValue(1));
    QCOMPARE(progress->format(), QStringLiteral("Pair scan: 1 of 3 pair comparisons"));

    comparison._backgroundDiscovery->_lastContiguousScannedPairPosition = 3;
    comparison.updateDiscoveryProgress(3);

    QCOMPARE(comparison._duplicateSets.size(), 1);
    QVERIFY(sets->isEnabled());
    QCOMPARE(comparison._leftVideo, 0);
    QCOMPARE(comparison._rightVideo, 1);
    QCOMPARE(progress->value(), comparison.progressBarValue(3));
    QCOMPARE(progress->format(), QStringLiteral("Pair scan complete: 3 of 3 pair comparisons"));
}

void test_comparison::test_selectedMemberCanBecomeReferenceForDirectEdge()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(fixture.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    std::vector<std::unique_ptr<Video>> ownedVideos;
    QVector<Video*> videos;
    for (const QString& name : {QStringLiteral("A.mp4"), QStringLiteral("B.mp4"), QStringLiteral("C.mp4")}) {
        const QString path = fixture.filePath(name);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        ownedVideos.push_back(std::make_unique<Video>(prefs, path));
        videos.append(ownedVideos.back().get());
    }

    Comparison comparison(videos, prefs, QRect(0, 0, 1120, 720));
    comparison._backgroundDiscovery->stop();
    comparison._videos = videos;
    auto* useAsReference = comparison.findChild<QPushButton*>(QStringLiteral("useSelectedAsReferenceButton"));
    auto* members = comparison.findChild<QListWidget*>(QStringLiteral("duplicateSetMembers"));
    auto* ignore = comparison.findChild<QPushButton*>(QStringLiteral("ignoreDuplicatePairButton"));
    QVERIFY(useAsReference);
    QVERIFY(members);
    QVERIFY(ignore);
    QVERIFY(!useAsReference->isEnabled());

    comparison._duplicateSets = DuplicateSetBuilder::build(3, {{0, 1, 1, 64, 1.0}, {1, 2, 2, 64, 1.0}});
    comparison.selectDuplicateSet(0, 1);
    QVERIFY(useAsReference->isEnabled());

    useAsReference->click();
    QCOMPARE(comparison._duplicateSets.first().members, QVector<int>({1, 0, 2}));
    QCOMPARE(comparison._leftVideo, 1);
    QCOMPARE(comparison._rightVideo, 0);

    members->setCurrentRow(2);
    QCOMPARE(comparison._leftVideo, 1);
    QCOMPARE(comparison._rightVideo, 2);
    QCOMPARE(comparison.findChild<QLabel*>(QStringLiteral("duplicateSetEvidence"))->text(),
             QStringLiteral("Direct discovered match"));
    QVERIFY(ignore->isEnabled());
    QVERIFY(useAsReference->isEnabled());

    comparison.clearDuplicateSets();
    QVERIFY(!useAsReference->isEnabled());
}

void test_comparison::test_selectedReferencePersistsAcrossExplicitRebuild()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(fixture.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    std::vector<std::unique_ptr<Video>> ownedVideos;
    QVector<Video*> videos;
    for (const QString& name : {QStringLiteral("A.mp4"), QStringLiteral("B.mp4"), QStringLiteral("C.mp4")}) {
        const QString path = fixture.filePath(name);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        ownedVideos.push_back(std::make_unique<Video>(prefs, path));
        videos.append(ownedVideos.back().get());
    }

    Comparison comparison(videos, prefs, QRect(0, 0, 1120, 720));
    comparison._backgroundDiscovery->stop();
    comparison._videos = videos;
    comparison._backgroundDiscovery->_matches = {
        {1, MatchedVideoPair{0, 1, 1, 64, 1.0}},
        {3, MatchedVideoPair{1, 2, 3, 64, 1.0}},
    };
    markDiscoveryComplete(comparison, 3);
    comparison.rebuildDuplicateSets();

    auto* useAsReference = comparison.findChild<QPushButton*>(QStringLiteral("useSelectedAsReferenceButton"));
    auto* members = comparison.findChild<QListWidget*>(QStringLiteral("duplicateSetMembers"));
    auto* ignore = comparison.findChild<QPushButton*>(QStringLiteral("ignoreDuplicatePairButton"));
    QVERIFY(useAsReference);
    QVERIFY(members);
    QVERIFY(ignore);
    useAsReference->click();
    QCOMPARE(comparison._duplicateSets.first().members, QVector<int>({1, 0, 2}));

    comparison.rebuildDuplicateSets();

    QCOMPARE(comparison._duplicateSets.first().members, QVector<int>({1, 0, 2}));
    QCOMPARE(comparison._leftVideo, 1);
    QCOMPARE(comparison._rightVideo, 0);
    members->setCurrentRow(2);
    QCOMPARE(comparison._leftVideo, 1);
    QCOMPARE(comparison._rightVideo, 2);
    QCOMPARE(comparison.findChild<QLabel*>(QStringLiteral("duplicateSetEvidence"))->text(),
             QStringLiteral("Direct discovered match"));
    QVERIFY(ignore->isEnabled());
}

void test_comparison::test_activeSetRemainsActionableWhenAnotherSetIsStale()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(fixture.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    std::vector<std::unique_ptr<Video>> ownedVideos;
    QVector<Video*> videos;
    for (int index = 0; index < 8; ++index) {
        const QString path = fixture.filePath(QStringLiteral("video-%1.mp4").arg(index));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        ownedVideos.push_back(std::make_unique<Video>(prefs, path));
        videos.append(ownedVideos.back().get());
    }

    QVector<MatchedVideoPair> matches{{0, 1, 1, 64, 1.0}};
    int64_t position = 2;
    for (int left = 2; left < videos.size(); ++left)
        for (int right = left + 1; right < videos.size(); ++right)
            matches.append({left, right, position++, 64, 1.0});

    Comparison comparison(videos, prefs, QRect(0, 0, 1120, 720));
    comparison._backgroundDiscovery->stop();
    comparison._videos = videos;
    comparison._duplicateSets = DuplicateSetBuilder::build(videos.size(), std::move(matches));
    QCOMPARE(comparison._duplicateSets.size(), 2);
    QCOMPARE(comparison._duplicateSets[0].members, QVector<int>({0, 1}));
    QCOMPARE(comparison._duplicateSets[0].edges.size(), 1);
    QCOMPARE(comparison._duplicateSets[1].edges.size(), 15);

    // A stale, unrelated family must not disable review of the active family.
    videos.last()->trashed = true;
    comparison.selectDuplicateSet(0, 1);

    QVERIFY(comparison.findChild<QWidget*>(QStringLiteral("leftDelete"))->isEnabled());
    QVERIFY(comparison.findChild<QWidget*>(QStringLiteral("rightDelete"))->isEnabled());
}

void test_comparison::test_stoppedDiscoveryCannotReplaceCleanupPairIndexes()
{
    Prefs prefs;
    prefs._numberOfVideos = 2;
    Video first(prefs, QStringLiteral("first.mp4"));
    Video second(prefs, QStringLiteral("second.mp4"));
    Comparison comparison({&first, &second}, prefs, QRect(0, 0, 1120, 720));
    comparison._backgroundDiscovery->stop();
    comparison._videos = {&first, &second};
    comparison.clearDuplicateSets();
    comparison._leftVideo = 1;
    comparison._rightVideo = 0;

    QCheckBox* namesFilter = comparison.findChild<QCheckBox*>(QStringLiteral("settingNamesInAnotherCheckbox"));
    QVERIFY(namesFilter);
    QTimer::singleShot(0, namesFilter, [namesFilter]() { namesFilter->setChecked(!namesFilter->isChecked()); });
    QCoreApplication::processEvents();

    QCOMPARE(comparison._leftVideo, 1);
    QCOMPARE(comparison._rightVideo, 0);
    QCOMPARE(comparison._selectedDuplicateSet, -1);
}

void test_comparison::test_cleanupCompletionDoesNotNavigateForeground()
{
    for (const int videoCount : {0, 1}) {
        Prefs prefs;
        prefs._numberOfVideos = videoCount;
        std::unique_ptr<Video> onlyVideo;
        QVector<Video*> videos;
        if (videoCount == 1) {
            onlyVideo = std::make_unique<Video>(prefs, QStringLiteral("only.mp4"));
            videos.append(onlyVideo.get());
        }
        Comparison comparison(videos, prefs, QRect(0, 0, 1120, 720));
        comparison._backgroundDiscovery->stop();
        comparison._seekForwards = false;
        QTabWidget* tabs = comparison.findChild<QTabWidget*>(QStringLiteral("tabWidget"));
        QVERIFY(tabs);
        tabs->setCurrentIndex(1);

        comparison.finishAutomaticCleanupRefresh();

        // on_nextVideo_clicked always flips this flag before it can scan or show
        // confirmToExit, so preserving it proves cleanup only restarted discovery.
        QVERIFY(!comparison._seekForwards);
        QVERIFY(comparison._backgroundDiscovery->hasStarted());
        QVERIFY(comparison._backgroundDiscovery->isComplete());
        QCOMPARE(tabs->currentIndex(), 1);
        auto* progress = comparison.findChild<QProgressBar*>(QStringLiteral("progressBar"));
        QVERIFY(progress);
        QCOMPARE(progress->format(), QStringLiteral("Pair scan complete: 0 of 0 pair comparisons"));
        QCOMPARE(comparison.findChild<QLabel*>(QStringLiteral("duplicateSetsStatus"))->text(),
                 QStringLiteral("No duplicate sets found."));
        QVERIFY(!comparison.findChild<QListWidget*>(QStringLiteral("duplicateSets"))->isEnabled());
    }
}

void test_comparison::test_rebuildKeepsActiveSetWhenSelectedMemberDisappears()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(fixture.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    std::vector<std::unique_ptr<Video>> ownedVideos;
    QVector<Video*> videos;
    for (int index = 0; index < 5; ++index) {
        const QString path = fixture.filePath(QStringLiteral("video-%1.mp4").arg(index));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        ownedVideos.push_back(std::make_unique<Video>(prefs, path));
        videos.append(ownedVideos.back().get());
    }

    Comparison comparison(videos, prefs, QRect(0, 0, 1120, 720));
    comparison._backgroundDiscovery->stop();
    comparison._videos = videos;
    comparison._backgroundDiscovery->_matches = {
        {1, MatchedVideoPair{0, 1, 1, 64, 1.0}},
        {2, MatchedVideoPair{2, 3, 2, 64, 1.0}},
        {3, MatchedVideoPair{2, 4, 3, 64, 1.0}},
    };
    markDiscoveryComplete(comparison, 3);
    comparison.rebuildDuplicateSets();
    QCOMPARE(comparison._duplicateSets.size(), 2);
    comparison.selectDuplicateSet(1, 2);
    QCOMPARE(comparison._selectedSetMember, 2);

    videos[4]->trashed = true;
    comparison.rebuildDuplicateSets();

    QCOMPARE(comparison._duplicateSets.size(), 2);
    QCOMPARE(comparison._selectedDuplicateSet, 1);
    QCOMPARE(comparison._duplicateSets[1].members, QVector<int>({2, 3}));
    QCOMPARE(comparison._leftVideo, 2);
    QCOMPARE(comparison._rightVideo, 3);
}

void test_comparison::test_duplicateSetBuilderConnectedComponents()
{
    const auto pair = [](int left, int right, int64_t position) {
        return MatchedVideoPair{left, right, position, 0, 0.0};
    };

    const QVector<DuplicateSet> pairSet = DuplicateSetBuilder::build(3, {pair(0, 1, 1)});
    QCOMPARE(pairSet.size(), 1);
    QCOMPARE(pairSet[0].members, QVector<int>({0, 1}));
    QCOMPARE(pairSet[0].edges.size(), 1);
    QCOMPARE(pairSet[0].edges[0].position, 1);

    const QVector<DuplicateSet> chainsAndDuplicates =
        DuplicateSetBuilder::build(7, {pair(1, 2, 1), pair(2, 3, 2), pair(1, 3, 3), pair(1, 2, 4), pair(4, 6, 5)});
    QCOMPARE(chainsAndDuplicates.size(), 2);
    QCOMPARE(chainsAndDuplicates[0].members, QVector<int>({1, 2, 3}));
    QCOMPARE(chainsAndDuplicates[1].members, QVector<int>({4, 6}));
    QCOMPARE(chainsAndDuplicates[0].edges.size(), 4);
    QCOMPARE(chainsAndDuplicates[1].edges.size(), 1);

    // Input edge order does not affect either component or member order.
    const QVector<DuplicateSet> reversed =
        DuplicateSetBuilder::build(7, {pair(4, 6, 5), pair(1, 2, 4), pair(1, 3, 3), pair(2, 3, 2), pair(1, 2, 1)});
    QCOMPARE(reversed[0].members, chainsAndDuplicates[0].members);
    QCOMPARE(reversed[1].members, chainsAndDuplicates[1].members);
}

void test_comparison::test_backgroundDiscoveryHidesOutOfOrderMatchesUntilPrefixCompletes()
{
    const auto pair = [](int left, int right, int64_t position) {
        return MatchedVideoPair{left, right, position, 0, 0.0};
    };

    BackgroundMatchDiscovery discovery(2, 1);
    discovery._maxPosition = 6;
    discovery._chunkSize = 2;
    discovery._started = true;
    discovery._completedChunks = QBitArray(3, false);

    discovery.acceptCompletedChunk(0, 1, {pair(0, 3, 3)});
    QCOMPARE(discovery.preScannedEnd(), 0);
    int safeMatchCount = 0;
    discovery.forEachSafeMatch([&safeMatchCount](const MatchedVideoPair&) { ++safeMatchCount; });
    QCOMPARE(safeMatchCount, 0);
    QVERIFY(!discovery.isComplete());

    discovery.acceptCompletedChunk(0, 0, {pair(0, 1, 1)});
    QCOMPARE(discovery.preScannedEnd(), 4);
    safeMatchCount = 0;
    discovery.forEachSafeMatch([&safeMatchCount](const MatchedVideoPair&) { ++safeMatchCount; });
    QCOMPARE(safeMatchCount, 2);
    QVERIFY(!discovery.isComplete());

    discovery.acceptCompletedChunk(0, 2, {pair(1, 3, 5)});
    QCOMPARE(discovery.preScannedEnd(), 6);
    safeMatchCount = 0;
    discovery.forEachSafeMatch([&safeMatchCount](const MatchedVideoPair&) { ++safeMatchCount; });
    QCOMPARE(safeMatchCount, 3);
    QVERIFY(discovery.isComplete());
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
    QVERIFY(discovery.isComplete());
    QCOMPARE(discovery.discoveredMatchCount(), 1);

    int safeMatchCount = 0;
    int64_t safeMatchPosition = 0;
    discovery.forEachSafeMatch([&safeMatchCount, &safeMatchPosition](const MatchedVideoPair& match) {
        ++safeMatchCount;
        safeMatchPosition = match.position;
    });
    QCOMPARE(safeMatchCount, 1);
    QCOMPARE(safeMatchPosition, 3);

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
