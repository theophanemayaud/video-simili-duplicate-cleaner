#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

#include "db.h"
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

// The message Video reports when the cached failure made it skip processing, so tests assert on one wording.
QString skippedMessage(const QString& failure)
{
    return QString("skipped, cache indicated it had failed in a previous scan: %1").arg(failure);
}

bool writeInvalidVideo(const QString& path, const QByteArray& contents, QIODevice::OpenMode mode = QIODevice::WriteOnly)
{
    QFile file(path);
    if (!file.open(mode))
        return false;
    return file.write(contents) == contents.size();
}

void writePlausibleMetadata(const Db& cache, const Prefs& prefs, const QString& path, qint64 size,
                            const QString& failure = {})
{
    Video metadata(prefs, path);
    metadata.size = size;
    metadata.duration = 1000;
    metadata.bitrate = 100;
    metadata.framerate = 25;
    metadata.codec = QStringLiteral("test");
    metadata.width = 16;
    metadata.height = 16;
    metadata.cachedFailure = failure;
    cache.writeMetadata(metadata);
}

bool hasMetadata(const QString& cachePath, const QString& path)
{
    Video video(cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds), path);
    return Db(cachePath).readMetadata(video);
}

QString cachedFailure(const QString& cachePath, const QString& path)
{
    Video video(cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds), path);
    if (!Db(cachePath).readMetadata(video))
        return {};
    return video.cachedFailure;
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
    void test_databaseClearingAndRemoval();
    void test_failureSkipsUntilCacheBypassedOrEmptied();
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
    QVERIFY(cachedFailure(cachePath, transientPath).isEmpty());
    const QString secondMetadataError = processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(secondMetadataError.contains(QStringLiteral("could not read metadata")));
    QVERIFY(!secondMetadataError.startsWith(QStringLiteral("skipped, cache indicated")));

    // Cached metadata gets the invalid file into the capture/decode path, whose failures may also be transient.
    {
        Db cache(cachePath);
        writePlausibleMetadata(cache, prefs, transientPath, QFileInfo(transientPath).size());
    }
    const QString captureError = processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(captureError.contains(QStringLiteral("capture failed")));
    QVERIFY(cachedFailure(cachePath, transientPath).isEmpty());
    const QString retryCaptureError = processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(retryCaptureError.contains(QStringLiteral("capture failed")));
    QVERIFY(!retryCaptureError.startsWith(QStringLiteral("skipped, cache indicated")));

    const QString otherModeError = processError(transientPath, cachePath, Prefs::WITH_CACHE, thumb1);
    QVERIFY(otherModeError.contains(QStringLiteral("capture failed")));
    QVERIFY(!otherModeError.startsWith(QStringLiteral("skipped, cache indicated")));

    const QString noCacheError = processError(transientPath, cachePath, Prefs::NO_CACHE, cutEnds);
    QVERIFY(noCacheError.contains(QStringLiteral("could not read metadata")));
    QVERIFY(!noCacheError.startsWith(QStringLiteral("skipped, cache indicated")));
    QVERIFY(cachedFailure(cachePath, transientPath).isEmpty());

    const QString cacheOnlyError = processError(transientPath, cachePath, Prefs::CACHE_ONLY, cutEnds);
    QVERIFY(!cacheOnlyError.isEmpty());
    QVERIFY(!cacheOnlyError.startsWith(QStringLiteral("skipped, cache indicated")));
    const QString cacheOnlyMiss = processError(transientPath, cachePath, Prefs::CACHE_ONLY, thumb3);
    QVERIFY(!cacheOnlyMiss.isEmpty());
    QVERIFY(!cacheOnlyMiss.startsWith(QStringLiteral("skipped, cache indicated")));

    const QString terminalPath = temporary.filePath(QStringLiteral("all-black.mp4"));
    QVERIFY(writeInvalidVideo(terminalPath, QByteArrayLiteral("not a video")));
    {
        Db cache(cachePath);
        writePlausibleMetadata(cache, prefs, terminalPath, QFileInfo(terminalPath).size());
        const QByteArray black = blackCapture();
        for (const int percentage : Thumbnail(cutEnds).percentages())
            cache.writeCapture(terminalPath, percentage, black);
    }
    // A substitute decode can fail temporarily even though the cached primary frame is black.
    const QString substituteError = processError(terminalPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(substituteError.contains(QStringLiteral("could not capture substitute frame")));
    QVERIFY(cachedFailure(cachePath, terminalPath).isEmpty());
    const QString retrySubstituteError = processError(terminalPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(retrySubstituteError.contains(QStringLiteral("could not capture substitute frame")));
    QVERIFY(!retrySubstituteError.startsWith(QStringLiteral("skipped, cache indicated")));

    const QString missingPath = temporary.filePath(QStringLiteral("missing.mp4"));
    QVERIFY(processError(missingPath, cachePath, Prefs::WITH_CACHE, cutEnds)
                .contains(QStringLiteral("doesn't seem to exist")));
    QVERIFY(!hasMetadata(cachePath, missingPath));

    const QString emptyPath = temporary.filePath(QStringLiteral("empty.mp4"));
    QVERIFY(writeInvalidVideo(emptyPath, {}));
    QVERIFY(processError(emptyPath, cachePath, Prefs::WITH_CACHE, cutEnds).contains(QStringLiteral("file size = 0")));
    QVERIFY(cachedFailure(cachePath, emptyPath).isEmpty());
}

void TestFailedVideoCache::test_databaseClearingAndRemoval()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString cachePath = temporary.filePath(QStringLiteral("cache.sqlite"));
    const Prefs prefs = cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(Db::emptyAllDb(prefs));

    const QString removedPath = temporary.filePath(QStringLiteral("removed.mp4"));
    const QString keptPath = temporary.filePath(QStringLiteral("kept.mp4"));
    {
        Db cache(cachePath);
        writePlausibleMetadata(cache, prefs, removedPath, 1, QStringLiteral("removed"));
        QVERIFY(cache.removeVideo(removedPath));
        QVERIFY(!hasMetadata(cachePath, removedPath));

        writePlausibleMetadata(cache, prefs, keptPath, 10, QStringLiteral("kept"));
        cache.writeCapture(keptPath, 8, QByteArrayLiteral("kept capture"));
        QVERIFY(cache.removeVideo(keptPath));

        QVERIFY(!hasMetadata(cachePath, keptPath));
        QVERIFY(cache.readCapture(keptPath, 8).isNull());

        writePlausibleMetadata(cache, prefs, keptPath, 10, QStringLiteral("kept"));
    }
    QVERIFY(Db::emptyAllDb(prefs));
    QVERIFY(!hasMetadata(cachePath, keptPath));
}

// Failure lives on the metadata row, so one readMetadata() both loads properties and decides whether to skip.
// Thumbnail mode is ignored: retrying is bypassing the cache or emptying it.
void TestFailedVideoCache::test_failureSkipsUntilCacheBypassedOrEmptied()
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
        writePlausibleMetadata(cache, prefs, videoPath, QFileInfo(videoPath).size(),
                               QStringLiteral("all screen captures black"));
    }

    QCOMPARE(cachedFailure(cachePath, videoPath), QStringLiteral("all screen captures black"));
    QCOMPARE(processError(videoPath, cachePath, Prefs::WITH_CACHE, cutEnds),
             skippedMessage(QStringLiteral("all screen captures black")));
    QCOMPARE(processError(videoPath, cachePath, Prefs::WITH_CACHE, thumb1),
             skippedMessage(QStringLiteral("all screen captures black")));

    // Scanning without the cache must reach FFmpeg again, and must not record a new failure.
    const QString bypassedError = processError(videoPath, cachePath, Prefs::NO_CACHE, cutEnds);
    QVERIFY(bypassedError.contains(QStringLiteral("could not read metadata")));
    QCOMPARE(cachedFailure(cachePath, videoPath), QStringLiteral("all screen captures black"));

    QVERIFY(Db::emptyAllDb(prefs));
    QVERIFY(cachedFailure(cachePath, videoPath).isEmpty());
    const QString retryError = processError(videoPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(retryError.contains(QStringLiteral("could not read metadata")));
    QVERIFY(!retryError.startsWith(QStringLiteral("skipped, cache indicated")));
}

QTEST_MAIN(TestFailedVideoCache)

#include "tst_failed_video_cache.moc"
