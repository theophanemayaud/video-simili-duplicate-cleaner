#include <QAction>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QUuid>
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

void writePlausibleMetadata(const Db& cache, const Prefs& prefs, const QString& path, qint64 size)
{
    Video metadata(prefs, path);
    metadata.size = size;
    metadata.duration = 1000;
    metadata.bitrate = 100;
    metadata.framerate = 25;
    metadata.codec = QStringLiteral("test");
    metadata.width = 16;
    metadata.height = 16;
    cache.writeMetadata(metadata);
}

Db::FailedVideoCacheLookup lookup(const QString& cachePath, const QString& path, int thumbnailMode)
{
    const QFileInfo signature(path);
    return Db(cachePath).lookupFailedVideo(path, signature.size(), signature.lastModified().toMSecsSinceEpoch(),
                                           thumbnailMode);
}

QByteArray blackCapture()
{
    QImage image(16, 16, QImage::Format_RGB888);
    image.fill(Qt::black);
    QByteArray encoded;
    QBuffer buffer(&encoded);
    image.save(&buffer, QByteArrayLiteral("JPG"));
    return encoded;
}
} // namespace

class TestFailedVideoCache : public QObject
{
    Q_OBJECT

  private slots:
    void init();
    void cleanup();
    void test_processingCachePolicy();
    void test_lookupErrorStopsProcessing();
    void test_databaseClearingAndRemoval();
    void test_clearActionRetriesVideo();
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
    const QString transientPath = temporary.filePath(QStringLiteral("transient.mp4"));
    QVERIFY(writeInvalidVideo(transientPath, QByteArrayLiteral("not a video")));

    const Prefs prefs = cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(Db::emptyAllDb(prefs));

    // Generic metadata/open failures may be environmental or transient, so unchanged files must remain retryable.
    const QString firstMetadataError = processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(firstMetadataError.contains(QStringLiteral("could not read metadata")));
    QCOMPARE(lookup(cachePath, transientPath, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
    const QString secondMetadataError = processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(secondMetadataError.contains(QStringLiteral("could not read metadata")));
    QVERIFY(!secondMetadataError.startsWith(QStringLiteral("previously failed")));

    // Cached metadata gets the invalid file into the capture/decode path, whose failures may also be transient.
    {
        Db cache(cachePath);
        writePlausibleMetadata(cache, prefs, transientPath, QFileInfo(transientPath).size());
    }
    const QString captureError = processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(captureError.contains(QStringLiteral("capture failed")));
    QCOMPARE(lookup(cachePath, transientPath, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
    const QString retryCaptureError = processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(retryCaptureError.contains(QStringLiteral("capture failed")));
    QVERIFY(!retryCaptureError.startsWith(QStringLiteral("previously failed")));

    const QString otherModeError = processError(transientPath, cachePath, Prefs::WITH_CACHE, thumb1);
    QVERIFY(otherModeError.contains(QStringLiteral("capture failed")));
    QVERIFY(!otherModeError.startsWith(QStringLiteral("previously failed")));
    QCOMPARE(lookup(cachePath, transientPath, thumb1).state, Db::FailedVideoCacheLookup::NotFound);

    const QString noCacheError = processError(transientPath, cachePath, Prefs::NO_CACHE, cutEnds);
    QVERIFY(noCacheError.contains(QStringLiteral("could not read metadata")));
    QVERIFY(!noCacheError.startsWith(QStringLiteral("previously failed")));
    const QString noCacheNewModeError = processError(transientPath, cachePath, Prefs::NO_CACHE, thumb2);
    QVERIFY(!noCacheNewModeError.isEmpty());
    QCOMPARE(lookup(cachePath, transientPath, thumb2).state, Db::FailedVideoCacheLookup::NotFound);

    const QString cacheOnlyError = processError(transientPath, cachePath, Prefs::CACHE_ONLY, cutEnds);
    QVERIFY(!cacheOnlyError.isEmpty());
    QVERIFY(!cacheOnlyError.startsWith(QStringLiteral("previously failed")));
    const QString cacheOnlyMiss = processError(transientPath, cachePath, Prefs::CACHE_ONLY, thumb3);
    QVERIFY(!cacheOnlyMiss.isEmpty());
    QVERIFY(!cacheOnlyMiss.startsWith(QStringLiteral("previously failed")));
    QCOMPARE(lookup(cachePath, transientPath, thumb3).state, Db::FailedVideoCacheLookup::NotFound);

    const QString terminalPath = temporary.filePath(QStringLiteral("all-black.mp4"));
    const QString otherPath = temporary.filePath(QStringLiteral("other.mp4"));
    QVERIFY(writeInvalidVideo(terminalPath, QByteArrayLiteral("not a video")));
    {
        Db cache(cachePath);
        writePlausibleMetadata(cache, prefs, terminalPath, QFileInfo(terminalPath).size());
        const QByteArray black = blackCapture();
        for (const int percentage : Thumbnail(cutEnds).percentages())
            cache.writeCapture(terminalPath, percentage, black);
        cache.writeApplePhotosName(terminalPath, QStringLiteral("stale name"));
        cache.writePairToIgnore(terminalPath, otherPath);
    }
    // A substitute decode can fail temporarily even though the cached primary frame is black.
    const QString substituteError = processError(terminalPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(substituteError.contains(QStringLiteral("capture failed: could not capture substitute frame")));
    QCOMPARE(lookup(cachePath, terminalPath, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
    const QString retrySubstituteError = processError(terminalPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(retrySubstituteError.contains(QStringLiteral("capture failed: could not capture substitute frame")));
    QVERIFY(!retrySubstituteError.startsWith(QStringLiteral("previously failed")));

    {
        Db cache(cachePath);
        const QFileInfo signature(terminalPath);
        QVERIFY(cache.writeFailedVideo(terminalPath, signature.size(), signature.lastModified().toMSecsSinceEpoch(),
                                       cutEnds, QStringLiteral("previous failure")));
    }

    QVERIFY(writeInvalidVideo(terminalPath, QByteArrayLiteral(" with changed size"),
                              QIODevice::WriteOnly | QIODevice::Append));
    QCOMPARE(lookup(cachePath, terminalPath, cutEnds).state, Db::FailedVideoCacheLookup::Changed);
    const QString lockConnectionName = QUuid::createUuid().toString();
    {
        QSqlDatabase lock = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), lockConnectionName);
        lock.setDatabaseName(cachePath);
        QVERIFY(lock.open());
        QSqlQuery lockQuery(lock);
        QVERIFY(lockQuery.exec(QStringLiteral("BEGIN IMMEDIATE;")));

        QCOMPARE(processError(terminalPath, cachePath, Prefs::WITH_CACHE, cutEnds),
                 QStringLiteral("could not invalidate changed video cache; will retry next scan"));
        QCOMPARE(lookup(cachePath, terminalPath, cutEnds).state, Db::FailedVideoCacheLookup::Changed);
        Db cache(cachePath);
        Video cachedVideo(prefs, terminalPath);
        QVERIFY(cache.readMetadata(cachedVideo));
        QVERIFY(!cache.readCapture(terminalPath, 8).isNull());
        QCOMPARE(cache.readApplePhotosName(terminalPath).state, Db::ApplePhotosNameCacheEntry::Found);
        QVERIFY(cache.isPairToIgnore(terminalPath, otherPath));

        QVERIFY(lockQuery.exec(QStringLiteral("ROLLBACK;")));
        lock.close();
    }
    QSqlDatabase::removeDatabase(lockConnectionName);

    const QString changedFileError = processError(terminalPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(changedFileError.contains(QStringLiteral("could not read metadata")));
    QVERIFY(!changedFileError.startsWith(QStringLiteral("previously failed")));
    QCOMPARE(lookup(cachePath, terminalPath, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
    {
        Db cache(cachePath);
        Video cachedVideo(prefs, terminalPath);
        QVERIFY(!cache.readMetadata(cachedVideo));
        QVERIFY(cache.readCapture(terminalPath, 8).isNull());
        QCOMPARE(cache.readApplePhotosName(terminalPath).state, Db::ApplePhotosNameCacheEntry::Unknown);
        QVERIFY(!cache.isPairToIgnore(terminalPath, otherPath));
    }

    const QString missingPath = temporary.filePath(QStringLiteral("missing.mp4"));
    QVERIFY(processError(missingPath, cachePath, Prefs::WITH_CACHE, cutEnds)
                .contains(QStringLiteral("doesn't seem to exist")));
    QCOMPARE(Db(cachePath).lookupFailedVideo(missingPath, 0, 0, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);

    const QString emptyPath = temporary.filePath(QStringLiteral("empty.mp4"));
    QVERIFY(writeInvalidVideo(emptyPath, {}));
    QVERIFY(processError(emptyPath, cachePath, Prefs::WITH_CACHE, cutEnds).contains(QStringLiteral("file size = 0")));
    QCOMPARE(lookup(cachePath, emptyPath, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
}

void TestFailedVideoCache::test_lookupErrorStopsProcessing()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString cachePath = temporary.filePath(QStringLiteral("cache.sqlite"));
    const QString videoPath = temporary.filePath(QStringLiteral("changed.mp4"));
    QVERIFY(writeInvalidVideo(videoPath, QByteArrayLiteral("not a video")));
    const Prefs prefs = cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(Db::emptyAllDb(prefs));
    {
        Db cache(cachePath);
        writePlausibleMetadata(cache, prefs, videoPath, QFileInfo(videoPath).size());
        cache.writeCapture(videoPath, 8, QByteArrayLiteral("stale capture"));
    }

    const QString connectionName = QUuid::createUuid().toString();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(cachePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("DROP TABLE failed_videos;")));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QCOMPARE(lookup(cachePath, videoPath, cutEnds).state, Db::FailedVideoCacheLookup::LookupError);
    QCOMPARE(processError(videoPath, cachePath, Prefs::WITH_CACHE, cutEnds),
             QStringLiteral("could not look up failed video cache; will retry next scan"));
    {
        Db cache(cachePath);
        Video cachedVideo(prefs, videoPath);
        QVERIFY(cache.readMetadata(cachedVideo));
        QCOMPARE(cache.readCapture(videoPath, 8), QByteArrayLiteral("stale capture"));
    }
}

void TestFailedVideoCache::test_databaseClearingAndRemoval()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString cachePath = temporary.filePath(QStringLiteral("cache.sqlite"));
    const Prefs prefs = cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(Db::emptyAllDb(prefs));

    const QString removedPath = temporary.filePath(QStringLiteral("removed.mp4"));
    const QString firstPath = temporary.filePath(QStringLiteral("first.mp4"));
    const QString secondPath = temporary.filePath(QStringLiteral("second.mp4"));
    const QString successfulPath = temporary.filePath(QStringLiteral("successful.mp4"));
    {
        Db cache(cachePath);
        QVERIFY(cache.writeFailedVideo(removedPath, 1, 2, cutEnds, QStringLiteral("removed")));
        QVERIFY(!cache.removeVideo(removedPath));
        QCOMPARE(cache.lookupFailedVideo(removedPath, 1, 2, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);

        QVERIFY(cache.writeFailedVideo(firstPath, 10, 20, cutEnds, QStringLiteral("first")));
        QVERIFY(cache.writeFailedVideo(firstPath, 10, 20, thumb1, QStringLiteral("first other mode")));
        QVERIFY(cache.writeFailedVideo(secondPath, 30, 40, cutEnds, QStringLiteral("second")));
        writePlausibleMetadata(cache, prefs, firstPath, 10);
        writePlausibleMetadata(cache, prefs, secondPath, 30);
        writePlausibleMetadata(cache, prefs, successfulPath, 50);
        cache.writeCapture(firstPath, 8, QByteArrayLiteral("first capture"));
        cache.writeCapture(secondPath, 8, QByteArrayLiteral("second capture"));
        cache.writeCapture(successfulPath, 8, QByteArrayLiteral("successful capture"));
        cache.writeApplePhotosName(firstPath, QStringLiteral("failed photo name"));
        cache.writeApplePhotosName(successfulPath, QStringLiteral("successful photo name"));
        cache.writePairToIgnore(firstPath, successfulPath);

        QCOMPARE(cache.clearFailedVideos(), 2); // paths, not path-and-mode rows
        QCOMPARE(cache.lookupFailedVideo(firstPath, 10, 20, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
        QCOMPARE(cache.lookupFailedVideo(secondPath, 30, 40, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);

        Video firstMetadata(prefs, firstPath);
        Video secondMetadata(prefs, secondPath);
        Video successfulMetadata(prefs, successfulPath);
        QVERIFY(!cache.readMetadata(firstMetadata));
        QVERIFY(!cache.readMetadata(secondMetadata));
        QVERIFY(cache.readMetadata(successfulMetadata));
        QVERIFY(cache.readCapture(firstPath, 8).isNull());
        QVERIFY(cache.readCapture(secondPath, 8).isNull());
        QCOMPARE(cache.readCapture(successfulPath, 8), QByteArrayLiteral("successful capture"));
        QCOMPARE(cache.readApplePhotosName(firstPath).state, Db::ApplePhotosNameCacheEntry::Found);
        QCOMPARE(cache.readApplePhotosName(successfulPath).state, Db::ApplePhotosNameCacheEntry::Found);
        QVERIFY(!cache.isPairToIgnore(firstPath, successfulPath));

        QVERIFY(cache.writeFailedVideo(firstPath, 10, 20, cutEnds, QStringLiteral("first")));
    }
    QVERIFY(Db::emptyAllDb(prefs));
    QCOMPARE(Db(cachePath).lookupFailedVideo(firstPath, 10, 20, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
}

void TestFailedVideoCache::test_clearActionRetriesVideo()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString cachePath = temporary.filePath(QStringLiteral("cache.sqlite"));
    const QString videoPath = temporary.filePath(QStringLiteral("broken.mp4"));
    QVERIFY(writeInvalidVideo(videoPath, QByteArrayLiteral("not a video")));
    const Prefs prefs = cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(Db::emptyAllDb(prefs));
    {
        Db cache(cachePath);
        writePlausibleMetadata(cache, prefs, videoPath, QFileInfo(videoPath).size());
    }
    const QString captureError = processError(videoPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(captureError.contains(QStringLiteral("capture failed")));
    {
        Db cache(cachePath);
        const QFileInfo signature(videoPath);
        QVERIFY(cache.writeFailedVideo(videoPath, signature.size(), signature.lastModified().toMSecsSinceEpoch(),
                                       thumb1, captureError));
        QVERIFY(cache.writeFailedVideo(QStringLiteral("second.mp4"), 30, 40, cutEnds, QStringLiteral("second")));
    }

    MainWindow window;
    QAction* clearAction = window.findChild<QAction*>(QStringLiteral("actionClear_failed_videos_from_cache"));
    QVERIFY(clearAction);
    clearAction->trigger();

    QTextEdit* status = window.findChild<QTextEdit*>(QStringLiteral("statusBox"));
    QVERIFY(status);
    QVERIFY(status->toPlainText().contains(QStringLiteral("Cleared 2 failed videos from cache.")));
    const QString retryError = processError(videoPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(retryError.contains(QStringLiteral("could not read metadata")));
    QVERIFY(!retryError.startsWith(QStringLiteral("previously failed")));
    QCOMPARE(lookup(cachePath, videoPath, cutEnds).state, Db::FailedVideoCacheLookup::NotFound);
}

QTEST_MAIN(TestFailedVideoCache)

#include "tst_failed_video_cache.moc"
