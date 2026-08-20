#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QPushButton>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include <atomic>
#include <cmath>
#include <memory>

// add necessary includes here
#include "../../app/comparison/comparison.h"
#include "../../app/comparison/internal/backgroundmatchdiscovery.h"
#include "../../app/comparison/internal/ssim.h"
#include "../../app/comparison/internal/videopairmatcher.h"
#include "../../app/comparison/internal/videopairspace.h"
#include "../../app/db.h"
#include "../../app/videometadata.h"

class CachePathRestore
{
  public:
    explicit CachePathRestore(Prefs& prefs) : _prefs(prefs), _originalPath(prefs.cacheFilePathName()) {}
    ~CachePathRestore() { _prefs.cacheFilePathName(_originalPath); }

  private:
    Prefs& _prefs;
    const QString _originalPath;
};

class CacheModeRestore
{
  public:
    explicit CacheModeRestore(Prefs& prefs) : _prefs(prefs), _originalMode(prefs.useCacheOption()) {}
    ~CacheModeRestore() { _prefs.useCacheOption(_originalMode); }

  private:
    Prefs& _prefs;
    const Prefs::USE_CACHE_OPTION _originalMode;
};

class ApplePhotosLookupRaceTestState
{
  public:
    QSemaphore finishFirstLookup;
    QSemaphore finishFreshLookup;
    std::atomic_bool firstLookupStarted = false;
    std::atomic_bool freshLookupStarted = false;
    std::atomic_bool queuedLookupExecuted = false;
    std::atomic_int lookupCalls = 0;
};

class ReleaseApplePhotosLookupRaceOnExit
{
  public:
    explicit ReleaseApplePhotosLookupRaceOnExit(std::shared_ptr<ApplePhotosLookupRaceTestState> state) : _state(state) {}
    ~ReleaseApplePhotosLookupRaceOnExit()
    {
        _state->finishFirstLookup.release();
        _state->finishFreshLookup.release();
    }

  private:
    const std::shared_ptr<ApplePhotosLookupRaceTestState> _state;
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
    void test_applePhotosNameCacheTriState();
#ifdef Q_OS_MACOS
    void test_applePhotosNameLookupCancelsAndCoalesces();
    void test_applePhotosNameLookupSuccessCaches();
    void test_applePhotosNameLookupClosingComparisonFails();
    void test_applePhotosNameLookupsAreSerialized();
    void test_applePhotosNameLookupHonorsCacheModes();
#endif
    void test_videoPairSpaceRoundTrip();
    void test_videoPairMatcherUsesConfigSnapshot();
    void test_ssimUsesEachBlockForMeans();
    void test_rotatedMatcherRequiresSsimSafeguard();
    void test_rotatedMatcherAppliesDurationModifierToSsimThreshold();
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

void test_comparison::test_applePhotosNameCacheTriState()
{
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());

    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(cacheDirectory.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    const QString successfulPath = QStringLiteral("/Library.photoslibrary/originals/A/ABC123.mov");
    const QString failedPath = QStringLiteral("/Library.photoslibrary/originals/B/DEF456.mov");
    {
        Db cache(prefs.cacheFilePathName());
        QCOMPARE(cache.readApplePhotosName(successfulPath).state, Db::ApplePhotosNameCacheEntry::Unknown);

        cache.writeApplePhotosName(successfulPath, QStringLiteral("Holiday clip.mov"));
        const Db::ApplePhotosNameCacheEntry successful = cache.readApplePhotosName(successfulPath);
        QCOMPARE(successful.state, Db::ApplePhotosNameCacheEntry::Found);
        QCOMPARE(successful.name, QStringLiteral("Holiday clip.mov"));

        cache.writeApplePhotosNameFailure(failedPath);
        const Db::ApplePhotosNameCacheEntry failed = cache.readApplePhotosName(failedPath);
        QCOMPARE(failed.state, Db::ApplePhotosNameCacheEntry::Failed);
        QVERIFY(failed.name.isEmpty());
    }

    QVERIFY(Db::emptyAllDb(prefs));
    {
        Db cache(prefs.cacheFilePathName());
        QCOMPARE(cache.readApplePhotosName(successfulPath).state, Db::ApplePhotosNameCacheEntry::Unknown);
        QCOMPARE(cache.readApplePhotosName(failedPath).state, Db::ApplePhotosNameCacheEntry::Unknown);
    }
}

#ifdef Q_OS_MACOS
void test_comparison::test_applePhotosNameLookupCancelsAndCoalesces()
{
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());

    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(cacheDirectory.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    const QString photosPath = QStringLiteral("/Library.photoslibrary/originals/A/ABC123.mov");
    const QString failedPhotosPath = QStringLiteral("/Library.photoslibrary/originals/B/DEF456.mov");
    const QString freshPhotosPath = QStringLiteral("/Library.photoslibrary/originals/C/FRESH789.mov");
    const QString filesystemName = QStringLiteral("filesystem.mp4");

    Video photosVideo(prefs, photosPath);
    Video failedPhotosVideo(prefs, failedPhotosPath);
    Video freshPhotosVideo(prefs, freshPhotosPath);
    Video filesystemVideo(prefs, QStringLiteral("/tmp/") + filesystemName);
    photosVideo.size = 4;
    failedPhotosVideo.size = 3;
    freshPhotosVideo.size = 2;
    filesystemVideo.size = 1;
    QVector<Video*> videos = {&photosVideo, &failedPhotosVideo, &freshPhotosVideo, &filesystemVideo};

    const auto lookupState = std::make_shared<ApplePhotosLookupRaceTestState>();
    ReleaseApplePhotosLookupRaceOnExit releaseLookupsOnExit(lookupState);
    {
        Comparison comparison(videos, prefs, QRect());
        comparison._backgroundDiscovery->stop();
        comparison._videos = videos; // Keep the pair indices deterministic despite the user's saved sort preference.
        comparison._applePhotosNameLookup = [lookupState](const QString& id) {
            lookupState->lookupCalls.fetch_add(1);
            if (id == QStringLiteral("ABC123")) {
                lookupState->firstLookupStarted.store(true);
                lookupState->finishFirstLookup.acquire();
                return QStringLiteral("Late first result.mov");
            }
            if (id == QStringLiteral("DEF456")) {
                lookupState->queuedLookupExecuted.store(true);
                return QStringLiteral("Queued lookup should not run.mov");
            }

            lookupState->freshLookupStarted.store(true);
            lookupState->finishFreshLookup.acquire();
            return QStringLiteral("Fresh name from Apple Photos.mov");
        };

        // Both Photos entries queue behind the one serialized worker. Cancel
        // while the first is active: the second must become terminal before it
        // reaches the provider.
        comparison.displayMatchedPair({0, 1, 1, 0, 0.0});
        QTRY_VERIFY_WITH_TIMEOUT(lookupState->firstLookupStarted.load(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(comparison._applePhotosNameWaitingDialog
                                     && comparison._applePhotosNameWaitingDialog->isVisible(),
                                 5000);

        auto* cancelButton = comparison._applePhotosNameWaitingDialog->findChild<QPushButton*>();
        QVERIFY(cancelButton != nullptr);
        cancelButton->click();
        QTRY_VERIFY_WITH_TIMEOUT(!comparison._applePhotosNameWaitingDialog, 5000);
        {
            Db cache(prefs.cacheFilePathName());
            QCOMPARE(cache.readApplePhotosName(photosPath).state, Db::ApplePhotosNameCacheEntry::Failed);
            QCOMPARE(cache.readApplePhotosName(failedPhotosPath).state, Db::ApplePhotosNameCacheEntry::Failed);
        }

        // Queue a fresh request before cancelled A and B drain. Their finished
        // callbacks must not close C's new waiting dialog.
        comparison.displayMatchedPair({2, 3, 2, 0, 0.0});
        QTRY_VERIFY_WITH_TIMEOUT(comparison._applePhotosNameWaitingDialog
                                     && comparison._applePhotosNameWaitingDialog->isVisible(),
                                 5000);
        lookupState->finishFirstLookup.release();
        QTRY_VERIFY_WITH_TIMEOUT(lookupState->freshLookupStarted.load(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(comparison._applePhotosNameRequests.size() == 1
                                     && comparison._applePhotosNameRequests.contains(freshPhotosPath),
                                 5000);
        QVERIFY(comparison._applePhotosNameWaitingDialog && comparison._applePhotosNameWaitingDialog->isVisible());
        QVERIFY(!lookupState->queuedLookupExecuted.load());
        QCOMPARE(lookupState->lookupCalls.load(), 2);

        // Repainting the fresh pending side must not launch another AppleScript.
        comparison.showVideo(QStringLiteral("left"));
        comparison.showVideo(QStringLiteral("left"));
        QCOMPARE(lookupState->lookupCalls.load(), 2);

        lookupState->finishFreshLookup.release();
        QTRY_COMPARE_WITH_TIMEOUT(freshPhotosVideo.nameInApplePhotos, QStringLiteral("Fresh name from Apple Photos.mov"),
                                  5000);
        QTRY_VERIFY_WITH_TIMEOUT(comparison._applePhotosNameRequests.isEmpty(), 5000);
        QCOMPARE(comparison.findChild<ClickableLabel*>(QStringLiteral("leftFileName"))->text(),
                 QStringLiteral("Fresh name from Apple Photos.mov"));
        QCOMPARE(comparison.findChild<ClickableLabel*>(QStringLiteral("rightFileName"))->text(), filesystemName);
        QVERIFY(photosVideo.nameInApplePhotos.isEmpty());
        QVERIFY(failedPhotosVideo.nameInApplePhotos.isEmpty());
    }

    {
        Db cache(prefs.cacheFilePathName());
        const Db::ApplePhotosNameCacheEntry cachedName = cache.readApplePhotosName(photosPath);
        QCOMPARE(cachedName.state, Db::ApplePhotosNameCacheEntry::Failed);
        QVERIFY(cachedName.name.isEmpty());
        const Db::ApplePhotosNameCacheEntry cachedFailure = cache.readApplePhotosName(failedPhotosPath);
        QCOMPARE(cachedFailure.state, Db::ApplePhotosNameCacheEntry::Failed);
        const Db::ApplePhotosNameCacheEntry freshName = cache.readApplePhotosName(freshPhotosPath);
        QCOMPARE(freshName.state, Db::ApplePhotosNameCacheEntry::Found);
        QCOMPARE(freshName.name, QStringLiteral("Fresh name from Apple Photos.mov"));
    }

    Video cachedPhotosVideo(prefs, photosPath);
    Video cachedFailureVideo(prefs, failedPhotosPath);
    Video otherVideo(prefs, QStringLiteral("/tmp/other.mp4"));
    cachedPhotosVideo.size = 3;
    cachedFailureVideo.size = 2;
    otherVideo.size = 1;
    QVector<Video*> cachedVideos = {&cachedPhotosVideo, &cachedFailureVideo, &otherVideo};
    {
        Comparison cachedComparison(cachedVideos, prefs, QRect());
        cachedComparison._backgroundDiscovery->stop();
        cachedComparison._videos = cachedVideos;
        std::atomic_int unexpectedLookupCalls = 0;
        cachedComparison._applePhotosNameLookup = [&unexpectedLookupCalls](const QString&) {
            unexpectedLookupCalls.fetch_add(1);
            return QStringLiteral("Should not be queried");
        };

        cachedComparison.displayMatchedPair({0, 2, 1, 0, 0.0});
        QCOMPARE(cachedComparison.findChild<ClickableLabel*>(QStringLiteral("leftFileName"))->text(),
                 QStringLiteral("ABC123.mov"));

        cachedComparison.displayMatchedPair({1, 2, 2, 0, 0.0});
        QCOMPARE(cachedComparison.findChild<ClickableLabel*>(QStringLiteral("leftFileName"))->text(),
                 QStringLiteral("DEF456.mov"));
        QCOMPARE(unexpectedLookupCalls.load(), 0);
    }

    QVERIFY(Db::emptyAllDb(prefs));
}

void test_comparison::test_applePhotosNameLookupSuccessCaches()
{
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());

    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(cacheDirectory.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    const QString photosPath = QStringLiteral("/Library.photoslibrary/originals/A/ABC123.mov");
    Video photosVideo(prefs, photosPath);
    Video otherVideo(prefs, QStringLiteral("/tmp/other.mp4"));
    photosVideo.size = 2;
    otherVideo.size = 1;
    QVector<Video*> videos = {&photosVideo, &otherVideo};

    {
        Comparison comparison(videos, prefs, QRect());
        comparison._backgroundDiscovery->stop();
        comparison._videos = videos;
        comparison._applePhotosNameLookup = [](const QString&) {
            QThread::msleep(100);
            return QStringLiteral("Name from Apple Photos.mov");
        };

        comparison.displayMatchedPair({0, 1, 1, 0, 0.0});
        QTRY_COMPARE_WITH_TIMEOUT(photosVideo.nameInApplePhotos, QStringLiteral("Name from Apple Photos.mov"), 5000);
        QCOMPARE(comparison.findChild<ClickableLabel*>(QStringLiteral("leftFileName"))->text(),
                 QStringLiteral("Name from Apple Photos.mov"));
        QTRY_VERIFY_WITH_TIMEOUT(comparison._applePhotosNameRequests.isEmpty(), 5000);
    }

    {
        Db cache(prefs.cacheFilePathName());
        const Db::ApplePhotosNameCacheEntry cachedName = cache.readApplePhotosName(photosPath);
        QCOMPARE(cachedName.state, Db::ApplePhotosNameCacheEntry::Found);
        QCOMPARE(cachedName.name, QStringLiteral("Name from Apple Photos.mov"));
    }

    Video cachedPhotosVideo(prefs, photosPath);
    Video cachedOtherVideo(prefs, QStringLiteral("/tmp/other.mp4"));
    cachedPhotosVideo.size = 2;
    cachedOtherVideo.size = 1;
    QVector<Video*> cachedVideos = {&cachedPhotosVideo, &cachedOtherVideo};
    {
        Comparison cachedComparison(cachedVideos, prefs, QRect());
        cachedComparison._backgroundDiscovery->stop();
        cachedComparison._videos = cachedVideos;
        std::atomic_int providerCalls = 0;
        cachedComparison._applePhotosNameLookup = [&providerCalls](const QString&) {
            providerCalls.fetch_add(1);
            return QStringLiteral("Should not be queried");
        };

        cachedComparison.displayMatchedPair({0, 1, 1, 0, 0.0});
        QCOMPARE(cachedPhotosVideo.nameInApplePhotos, QStringLiteral("Name from Apple Photos.mov"));
        QCOMPARE(cachedComparison.findChild<ClickableLabel*>(QStringLiteral("leftFileName"))->text(),
                 QStringLiteral("Name from Apple Photos.mov"));
        QCOMPARE(providerCalls.load(), 0);
    }

    QVERIFY(Db::emptyAllDb(prefs));
}

void test_comparison::test_applePhotosNameLookupClosingComparisonFails()
{
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());

    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(cacheDirectory.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    const QString photosPath = QStringLiteral("/Library.photoslibrary/originals/A/CLOSE123.mov");
    Video photosVideo(prefs, photosPath);
    Video otherVideo(prefs, QStringLiteral("/tmp/other.mp4"));
    photosVideo.size = 2;
    otherVideo.size = 1;
    QVector<Video*> videos = {&photosVideo, &otherVideo};
    std::atomic_bool lookupStarted = false;
    QElapsedTimer closeTimer;
    {
        Comparison comparison(videos, prefs, QRect());
        comparison._backgroundDiscovery->stop();
        comparison._videos = videos;
        comparison._applePhotosNameLookup = [&lookupStarted](const QString&) {
            lookupStarted.store(true);
            QThread::msleep(750);
            return QStringLiteral("Late result must be ignored.mov");
        };

        comparison.displayMatchedPair({0, 1, 1, 0, 0.0});
        QTRY_VERIFY_WITH_TIMEOUT(lookupStarted.load(), 5000);
        closeTimer.start();
    }
    QVERIFY2(closeTimer.elapsed() < 300, "Closing Comparison waited for the Photos lookup");

    {
        Db cache(prefs.cacheFilePathName());
        QCOMPARE(cache.readApplePhotosName(photosPath).state, Db::ApplePhotosNameCacheEntry::Failed);
    }
    QTest::qWait(800);
    {
        Db cache(prefs.cacheFilePathName());
        QCOMPARE(cache.readApplePhotosName(photosPath).state, Db::ApplePhotosNameCacheEntry::Failed);
    }

    QVERIFY(Db::emptyAllDb(prefs));
}

void test_comparison::test_applePhotosNameLookupsAreSerialized()
{
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());

    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    prefs.cacheFilePathName(cacheDirectory.filePath(QStringLiteral("cache.db")));
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    Video leftPhotosVideo(prefs, QStringLiteral("/Library.photoslibrary/originals/A/LEFT123.mov"));
    Video rightPhotosVideo(prefs, QStringLiteral("/Library.photoslibrary/originals/B/RIGHT456.mov"));
    leftPhotosVideo.size = 2;
    rightPhotosVideo.size = 1;
    QVector<Video*> videos = {&leftPhotosVideo, &rightPhotosVideo};
    std::atomic_int lookupCalls = 0;
    std::atomic_int activeLookups = 0;
    std::atomic_int maximumActiveLookups = 0;
    {
        Comparison comparison(videos, prefs, QRect());
        comparison._backgroundDiscovery->stop();
        comparison._videos = videos;
        comparison._applePhotosNameLookup = [&lookupCalls, &activeLookups, &maximumActiveLookups](const QString& id) {
            lookupCalls.fetch_add(1);
            const int active = activeLookups.fetch_add(1) + 1;
            int maximum = maximumActiveLookups.load();
            while (maximum < active && !maximumActiveLookups.compare_exchange_weak(maximum, active)) {
            }
            QThread::msleep(100);
            activeLookups.fetch_sub(1);
            return QStringLiteral("Name %1.mov").arg(id);
        };

        comparison.displayMatchedPair({0, 1, 1, 0, 0.0});
        QTRY_VERIFY_WITH_TIMEOUT(comparison._applePhotosNameRequests.isEmpty(), 5000);
        QCOMPARE(lookupCalls.load(), 2);
        QCOMPARE(maximumActiveLookups.load(), 1);
    }

    QVERIFY(Db::emptyAllDb(prefs));
}

void test_comparison::test_applePhotosNameLookupHonorsCacheModes()
{
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());

    Prefs prefs;
    CachePathRestore restoreCachePath(prefs);
    CacheModeRestore restoreCacheMode(prefs);
    prefs.cacheFilePathName(cacheDirectory.filePath(QStringLiteral("cache.db")));
    prefs.useCacheOption(Prefs::WITH_CACHE);
    QVERIFY(Db::initDbAndCacheLocation(prefs));

    const QString cachedPath = QStringLiteral("/Library.photoslibrary/originals/A/CACHED123.mov");
    const QString cacheOnlyPath = QStringLiteral("/Library.photoslibrary/originals/B/CACHEONLY456.mov");
    const QString otherPath = QStringLiteral("/tmp/other.mp4");
    {
        Db cache(prefs.cacheFilePathName());
        cache.writeApplePhotosName(cachedPath, QStringLiteral("Cached name.mov"));
    }

    Video noCacheVideo(prefs, cachedPath);
    Video otherVideo(prefs, otherPath);
    noCacheVideo.size = 2;
    otherVideo.size = 1;
    QVector<Video*> noCacheVideos = {&noCacheVideo, &otherVideo};
    std::atomic_int withCacheLookupCalls = 0;
    {
        Comparison comparison(noCacheVideos, prefs, QRect());
        comparison._backgroundDiscovery->stop();
        comparison._videos = noCacheVideos;
        comparison._applePhotosNameLookup = [&withCacheLookupCalls](const QString&) {
            withCacheLookupCalls.fetch_add(1);
            return QStringLiteral("Must not be called.mov");
        };

        comparison.displayMatchedPair({0, 1, 1, 0, 0.0});
        QCOMPARE(noCacheVideo.nameInApplePhotos, QStringLiteral("Cached name.mov"));
    }
    QCOMPARE(withCacheLookupCalls.load(), 0);

    // Reopen the same Video objects after switching modes: NO_CACHE must not
    // reuse the name retained from the previous WITH_CACHE comparison.
    prefs.useCacheOption(Prefs::NO_CACHE);
    std::atomic_int noCacheLookupCalls = 0;
    {
        Comparison comparison(noCacheVideos, prefs, QRect());
        comparison._backgroundDiscovery->stop();
        comparison._videos = noCacheVideos;
        QSignalSpy statusMessages(&comparison, &Comparison::sendStatusMessage);
        comparison._applePhotosNameLookup = [&noCacheLookupCalls](const QString&) {
            noCacheLookupCalls.fetch_add(1);
            return QStringLiteral(OBJ_C_FAILURE_STRING);
        };

        comparison.displayMatchedPair({0, 1, 1, 0, 0.0});
        QTRY_COMPARE_WITH_TIMEOUT(statusMessages.count(), 1, 5000);
        QVERIFY(statusMessages.first().first().toString().contains(QStringLiteral("Unknown error getting name")));
        QCOMPARE(noCacheVideo.nameInApplePhotos, QString());
    }
    QCOMPARE(noCacheLookupCalls.load(), 1);
    {
        Db cache(prefs.cacheFilePathName());
        const Db::ApplePhotosNameCacheEntry cached = cache.readApplePhotosName(cachedPath);
        QCOMPARE(cached.state, Db::ApplePhotosNameCacheEntry::Found);
        QCOMPARE(cached.name, QStringLiteral("Cached name.mov"));
    }

    const QString cancelledPath = QStringLiteral("/Library.photoslibrary/originals/C/CANCEL789.mov");
    Video cancelledVideo(prefs, cancelledPath);
    cancelledVideo.size = 2;
    QVector<Video*> cancelledVideos = {&cancelledVideo, &otherVideo};
    QSemaphore finishCancelledLookup;
    std::atomic_bool cancelledLookupStarted = false;
    {
        Comparison comparison(cancelledVideos, prefs, QRect());
        comparison._backgroundDiscovery->stop();
        comparison._videos = cancelledVideos;
        comparison._applePhotosNameLookup = [&cancelledLookupStarted, &finishCancelledLookup](const QString&) {
            cancelledLookupStarted.store(true);
            finishCancelledLookup.acquire();
            return QStringLiteral("Cancelled no-cache name.mov");
        };

        comparison.displayMatchedPair({0, 1, 1, 0, 0.0});
        QTRY_VERIFY_WITH_TIMEOUT(cancelledLookupStarted.load(), 5000);
        auto* cancelButton = comparison._applePhotosNameWaitingDialog->findChild<QPushButton*>();
        QVERIFY(cancelButton != nullptr);
        cancelButton->click();
        QTRY_VERIFY_WITH_TIMEOUT(!comparison._applePhotosNameWaitingDialog, 5000);
        finishCancelledLookup.release();
        QTRY_VERIFY_WITH_TIMEOUT(comparison._applePhotosNameRequests.isEmpty(), 5000);
    }
    {
        Db cache(prefs.cacheFilePathName());
        QCOMPARE(cache.readApplePhotosName(cancelledPath).state, Db::ApplePhotosNameCacheEntry::Unknown);
    }

    Video cacheOnlyVideo(prefs, cacheOnlyPath);
    Video cacheOnlyOtherVideo(prefs, otherPath);
    cacheOnlyVideo.size = 2;
    cacheOnlyOtherVideo.size = 1;
    QVector<Video*> cacheOnlyVideos = {&cacheOnlyVideo, &cacheOnlyOtherVideo};
    std::atomic_int noCacheSuccessLookupCalls = 0;
    {
        Comparison comparison(cacheOnlyVideos, prefs, QRect());
        comparison._backgroundDiscovery->stop();
        comparison._videos = cacheOnlyVideos;
        comparison._applePhotosNameLookup = [&noCacheSuccessLookupCalls](const QString&) {
            noCacheSuccessLookupCalls.fetch_add(1);
            return QStringLiteral("Live no-cache name.mov");
        };

        comparison.displayMatchedPair({0, 1, 1, 0, 0.0});
        QTRY_COMPARE_WITH_TIMEOUT(cacheOnlyVideo.nameInApplePhotos, QStringLiteral("Live no-cache name.mov"), 5000);
    }
    QCOMPARE(noCacheSuccessLookupCalls.load(), 1);

    // CACHE_ONLY must use only its database entry, not the live name retained
    // by reopening the same Video objects from the previous NO_CACHE result.
    prefs.useCacheOption(Prefs::CACHE_ONLY);
    std::atomic_int cacheOnlyLookupCalls = 0;
    {
        Comparison comparison(cacheOnlyVideos, prefs, QRect());
        comparison._backgroundDiscovery->stop();
        comparison._videos = cacheOnlyVideos;
        comparison._applePhotosNameLookup = [&cacheOnlyLookupCalls](const QString&) {
            cacheOnlyLookupCalls.fetch_add(1);
            return QStringLiteral("Must not be called.mov");
        };

        comparison.displayMatchedPair({0, 1, 1, 0, 0.0});
        QCoreApplication::processEvents();
        QCOMPARE(cacheOnlyVideo.nameInApplePhotos, QString());
        QCOMPARE(comparison.findChild<ClickableLabel*>(QStringLiteral("leftFileName"))->text(),
                 QStringLiteral("CACHEONLY456.mov"));
    }
    QCOMPARE(cacheOnlyLookupCalls.load(), 0);
    {
        Db cache(prefs.cacheFilePathName());
        QCOMPARE(cache.readApplePhotosName(cacheOnlyPath).state, Db::ApplePhotosNameCacheEntry::Unknown);
    }

    QVERIFY(Db::emptyAllDb(prefs));
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
