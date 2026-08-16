#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QtTest>

#include "db.h"
#include "mainwindow.h"
#include "prefs.h"
#include "thumbnail.h"
#include "video.h"

namespace
{
Prefs cachePrefs(const QString& cachePath, Prefs::USE_CACHE_OPTION cacheOption, int thumbnailMode)
{
    Prefs prefs;
    prefs.cacheFilePathName(cachePath);
    prefs.useCacheOption(cacheOption);
    prefs.thumbnailsMode(thumbnailMode);
    prefs.appVersion = QStringLiteral("test");
    return prefs;
}

QString processError(const QString& path, const QString& cachePath, Prefs::USE_CACHE_OPTION cacheOption,
                     int thumbnailMode)
{
    const Prefs prefs = cachePrefs(cachePath, cacheOption, thumbnailMode);
    Video video(prefs, path);
    return video.process().errorMsg;
}

bool writeInvalidVideo(const QString& path, const QByteArray& contents, QIODevice::OpenMode mode = QIODevice::WriteOnly)
{
    QFile file(path);
    if (!file.open(mode))
        return false;
    return file.write(contents) == contents.size();
}
} // namespace

class TestFailedVideoCache : public QObject
{
    Q_OBJECT

  private slots:
    void init();
    void cleanup();
    void test_processingCachePolicy();
    void test_databaseClearingAndRemoval();
    void test_clearAction();
};

void TestFailedVideoCache::init()
{
    Prefs().resetSettings();
}

void TestFailedVideoCache::cleanup()
{
    Prefs().resetSettings();
}

void TestFailedVideoCache::test_processingCachePolicy()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString cachePath = temporary.filePath(QStringLiteral("cache.sqlite"));
    const QString videoPath = temporary.filePath(QStringLiteral("broken.mp4"));
    QVERIFY(writeInvalidVideo(videoPath, QByteArrayLiteral("not a video")));

    Prefs prefs = cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(Db::emptyAllDb(prefs));

    const QString firstError = processError(videoPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(!firstError.isEmpty());
    QVERIFY(!firstError.startsWith(QStringLiteral("previously failed")));

    QFileInfo signature(videoPath);
    Db::FailedVideoCacheLookup marker = Db(cachePath).lookupFailedVideo(
        videoPath, signature.size(), signature.lastModified().toMSecsSinceEpoch(), cutEnds);
    QCOMPARE(marker.state, Db::FailedVideoCacheLookup::Unchanged);
    QCOMPARE(marker.error, firstError);

    const QString skippedError = processError(videoPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QCOMPARE(skippedError, QStringLiteral("previously failed; skipped processing: %1").arg(firstError));

    {
        Db cache(cachePath);
        Video staleMetadata(prefs, videoPath);
        staleMetadata.size = signature.size();
        staleMetadata.duration = 1000;
        staleMetadata.width = 16;
        staleMetadata.height = 16;
        cache.writeMetadata(staleMetadata);
        cache.writeCapture(videoPath, 8, QByteArrayLiteral("stale capture"));
        cache.writeApplePhotosName(videoPath, QStringLiteral("stale name"));
    }
    QVERIFY(writeInvalidVideo(videoPath, QByteArrayLiteral(" with changed size"),
                              QIODevice::WriteOnly | QIODevice::Append));
    signature.refresh();
    marker = Db(cachePath).lookupFailedVideo(videoPath, signature.size(), signature.lastModified().toMSecsSinceEpoch(),
                                             cutEnds);
    QCOMPARE(marker.state, Db::FailedVideoCacheLookup::Changed);

    const QString changedFileError = processError(videoPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(!changedFileError.isEmpty());
    QVERIFY(!changedFileError.startsWith(QStringLiteral("previously failed")));
    marker = Db(cachePath).lookupFailedVideo(videoPath, signature.size(), signature.lastModified().toMSecsSinceEpoch(),
                                             cutEnds);
    QCOMPARE(marker.state, Db::FailedVideoCacheLookup::Unchanged);
    QCOMPARE(marker.error, changedFileError);
    {
        Db cache(cachePath);
        Video cachedVideo(prefs, videoPath);
        QVERIFY(!cache.readMetadata(cachedVideo));
        QVERIFY(cache.readCapture(videoPath, 8).isNull());
        QCOMPARE(cache.readApplePhotosName(videoPath).state, Db::ApplePhotosNameCacheEntry::Unknown);
    }

    const QString otherModeError = processError(videoPath, cachePath, Prefs::WITH_CACHE, thumb1);
    QVERIFY(!otherModeError.isEmpty());
    QVERIFY(!otherModeError.startsWith(QStringLiteral("previously failed")));
    marker = Db(cachePath).lookupFailedVideo(videoPath, signature.size(), signature.lastModified().toMSecsSinceEpoch(),
                                             thumb1);
    QCOMPARE(marker.state, Db::FailedVideoCacheLookup::Unchanged);

    const QString noCacheError = processError(videoPath, cachePath, Prefs::NO_CACHE, cutEnds);
    QVERIFY(!noCacheError.isEmpty());
    QVERIFY(!noCacheError.startsWith(QStringLiteral("previously failed")));
    const QString noCacheNewModeError = processError(videoPath, cachePath, Prefs::NO_CACHE, thumb2);
    QVERIFY(!noCacheNewModeError.isEmpty());
    marker = Db(cachePath).lookupFailedVideo(videoPath, signature.size(), signature.lastModified().toMSecsSinceEpoch(),
                                             thumb2);
    QCOMPARE(marker.state, Db::FailedVideoCacheLookup::NotFound);

    const QString cacheOnlyError = processError(videoPath, cachePath, Prefs::CACHE_ONLY, cutEnds);
    QCOMPARE(cacheOnlyError, QStringLiteral("previously failed; skipped processing: %1").arg(changedFileError));
    const QString cacheOnlyMiss = processError(videoPath, cachePath, Prefs::CACHE_ONLY, thumb3);
    QVERIFY(!cacheOnlyMiss.isEmpty());
    QVERIFY(!cacheOnlyMiss.startsWith(QStringLiteral("previously failed")));
    marker = Db(cachePath).lookupFailedVideo(videoPath, signature.size(), signature.lastModified().toMSecsSinceEpoch(),
                                             thumb3);
    QCOMPARE(marker.state, Db::FailedVideoCacheLookup::NotFound);

    const QString missingPath = temporary.filePath(QStringLiteral("missing.mp4"));
    const QString missingError = processError(missingPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(missingError.contains(QStringLiteral("doesn't seem to exist")));
    QCOMPARE(Db(cachePath).lookupFailedVideo(missingPath, 0, 0, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
}

void TestFailedVideoCache::test_databaseClearingAndRemoval()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString cachePath = temporary.filePath(QStringLiteral("cache.sqlite"));
    Prefs prefs = cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(Db::emptyAllDb(prefs));

    const QString firstPath = temporary.filePath(QStringLiteral("first.mp4"));
    const QString secondPath = temporary.filePath(QStringLiteral("second.mp4"));
    {
        Db cache(cachePath);
        QVERIFY(cache.writeFailedVideo(firstPath, 10, 20, cutEnds, QStringLiteral("first error")));
        QVERIFY(cache.writeFailedVideo(secondPath, 30, 40, cutEnds, QStringLiteral("second error")));

        // A failed file can have no metadata row, but moving or deleting it must still remove its marker.
        QVERIFY(!cache.removeVideo(firstPath));
        QCOMPARE(cache.lookupFailedVideo(firstPath, 10, 20, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
        QCOMPARE(cache.clearFailedVideos(), 1);
        QCOMPARE(cache.lookupFailedVideo(secondPath, 30, 40, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);

        QVERIFY(cache.writeFailedVideo(firstPath, 10, 20, cutEnds, QStringLiteral("first error")));
    }
    QVERIFY(Db::emptyAllDb(prefs));
    QCOMPARE(Db(cachePath).lookupFailedVideo(firstPath, 10, 20, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
}

void TestFailedVideoCache::test_clearAction()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString cachePath = temporary.filePath(QStringLiteral("cache.sqlite"));
    Prefs prefs = cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(Db::emptyAllDb(prefs));
    {
        Db cache(cachePath);
        QVERIFY(cache.writeFailedVideo(QStringLiteral("first.mp4"), 10, 20, cutEnds, QStringLiteral("first")));
        QVERIFY(cache.writeFailedVideo(QStringLiteral("second.mp4"), 30, 40, cutEnds, QStringLiteral("second")));
    }

    MainWindow window;
    QAction* clearAction = window.findChild<QAction*>(QStringLiteral("actionClear_failed_videos_from_cache"));
    QVERIFY(clearAction);
    clearAction->trigger();

    QCOMPARE(Db(cachePath).clearFailedVideos(), 0);
    QTextEdit* status = window.findChild<QTextEdit*>(QStringLiteral("statusBox"));
    QVERIFY(status);
    QVERIFY(status->toPlainText().contains(QStringLiteral("Cleared 2 failed videos from cache.")));
}

QTEST_MAIN(TestFailedVideoCache)

#include "tst_failed_video_cache.moc"
