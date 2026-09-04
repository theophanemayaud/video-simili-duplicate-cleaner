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
    return QString("skipped, cache indicated it had failed in a previous scan with: %1").arg(failure);
}

bool writeInvalidVideo(const QString& path, const QByteArray& contents, QIODevice::OpenMode mode = QIODevice::WriteOnly)
{
    QFile file(path);
    if (!file.open(mode))
        return false;
    return file.write(contents) == contents.size();
}

void writePlausibleMetadata(const Db& cache, const Prefs& prefs, const QString& path, qint64 size,
                            const QString& failure = {}, short width = 16, short height = 16)
{
    Video metadata(prefs, path);
    metadata.size = size;
    metadata.duration = 1000;
    metadata.bitrate = 100;
    metadata.framerate = 25;
    metadata.codec = QStringLiteral("test");
    metadata.width = width;
    metadata.height = height;
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
    void test_metadataDatesAreCached();
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

    const QString firstMetadataError = processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(firstMetadataError.contains(QStringLiteral("could not read metadata")));
    QCOMPARE(cachedFailure(cachePath, transientPath), firstMetadataError);
    QCOMPARE(processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds), skippedMessage(firstMetadataError));

    {
        Db cache(cachePath);
        writePlausibleMetadata(cache, prefs, transientPath, QFileInfo(transientPath).size());
    }
    const QString captureError = processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(captureError.contains(QStringLiteral("capture failed")));
    QCOMPARE(cachedFailure(cachePath, transientPath), captureError);
    QCOMPARE(processError(transientPath, cachePath, Prefs::WITH_CACHE, cutEnds), skippedMessage(captureError));
    QCOMPARE(processError(transientPath, cachePath, Prefs::WITH_CACHE, thumb1), skippedMessage(captureError));

    const QString noCacheError = processError(transientPath, cachePath, Prefs::NO_CACHE, cutEnds);
    QVERIFY(noCacheError.contains(QStringLiteral("could not read metadata")));
    QVERIFY(!noCacheError.startsWith(QStringLiteral("skipped, cache indicated")));
    QCOMPARE(cachedFailure(cachePath, transientPath), captureError);

    QCOMPARE(processError(transientPath, cachePath, Prefs::CACHE_ONLY, cutEnds), skippedMessage(captureError));
    QCOMPARE(processError(transientPath, cachePath, Prefs::CACHE_ONLY, thumb1), skippedMessage(captureError));

    const QString uncachedPath = temporary.filePath(QStringLiteral("uncached.mp4"));
    QVERIFY(writeInvalidVideo(uncachedPath, QByteArrayLiteral("not a video")));
    QVERIFY(processError(uncachedPath, cachePath, Prefs::CACHE_ONLY, cutEnds)
                .contains(QStringLiteral("video was not fully cached")));

    const QString terminalPath = temporary.filePath(QStringLiteral("all-black.mp4"));
    QVERIFY(writeInvalidVideo(terminalPath, QByteArrayLiteral("not a video")));
    {
        Db cache(cachePath);
        writePlausibleMetadata(cache, prefs, terminalPath, QFileInfo(terminalPath).size());
        const QByteArray black = blackCapture();
        for (const int percentage : Thumbnail(cutEnds).percentages())
            cache.writeCapture(terminalPath, percentage, black);
    }
    const QString substituteError = processError(terminalPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(substituteError.contains(QStringLiteral("was blank and an attempted replacement")));
    QCOMPARE(cachedFailure(cachePath, terminalPath), substituteError);
    QCOMPARE(processError(terminalPath, cachePath, Prefs::WITH_CACHE, cutEnds), skippedMessage(substituteError));

    const QString missingPath = temporary.filePath(QStringLiteral("missing.mp4"));
    const QString missingError = processError(missingPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(missingError.contains(QStringLiteral("doesn't seem to exist")));
    QVERIFY(hasMetadata(cachePath, missingPath));
    QCOMPARE(cachedFailure(cachePath, missingPath), missingError);

    const QString emptyPath = temporary.filePath(QStringLiteral("empty.mp4"));
    QVERIFY(writeInvalidVideo(emptyPath, {}));
    const QString emptyError = processError(emptyPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(emptyError.contains(QStringLiteral("file size = 0")));
    QCOMPARE(cachedFailure(cachePath, emptyPath), emptyError);

    const QString zeroDimensionsPath = temporary.filePath(QStringLiteral("zero-dimensions.mp4"));
    QVERIFY(writeInvalidVideo(zeroDimensionsPath, QByteArrayLiteral("not a video")));
    {
        Db cache(cachePath);
        writePlausibleMetadata(cache, prefs, zeroDimensionsPath, QFileInfo(zeroDimensionsPath).size(), {}, 0, 0);
    }
    const QString zeroDimensionsError = processError(zeroDimensionsPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(zeroDimensionsError.contains(QStringLiteral("= 0")));
    QCOMPARE(cachedFailure(cachePath, zeroDimensionsPath), zeroDimensionsError);
    {
        Video cached(cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds), zeroDimensionsPath);
        QVERIFY(Db(cachePath).readMetadata(cached));
        QCOMPARE(cached.size, 0);
        QCOMPARE(cached.duration, 0);
        QCOMPARE(cached.bitrate, 0);
        QCOMPARE(cached.codec, QString());
        QCOMPARE(cached.cachedFailure, zeroDimensionsError);
    }
    QCOMPARE(processError(zeroDimensionsPath, cachePath, Prefs::WITH_CACHE, cutEnds),
             skippedMessage(zeroDimensionsError));
}

void TestFailedVideoCache::test_metadataDatesAreCached()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString cachePath = temporary.filePath(QStringLiteral("cache.sqlite"));
    const QString videoPath = temporary.filePath(QStringLiteral("dated.mp4"));
    QVERIFY(writeInvalidVideo(videoPath, QByteArrayLiteral("not a video")));

    const Prefs prefs = cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(Db::emptyAllDb(prefs));

    const QDateTime modified = QDateTime(QDate(2024, 3, 15), QTime(10, 30, 0));
    const QDateTime birthTime = QDateTime(QDate(2020, 1, 2), QTime(8, 0, 0));
    {
        Db cache(cachePath);
        Video metadata(prefs, videoPath);
        metadata.size = 12;
        metadata.duration = 1000;
        metadata.bitrate = 100;
        metadata.framerate = 25;
        metadata.codec = QStringLiteral("test");
        metadata.width = 16;
        metadata.height = 16;
        metadata.modified = modified;
        metadata._fileCreateDate = birthTime;
        cache.writeMetadata(metadata);
    }

    Video loaded(cachePrefs(cachePath, Prefs::WITH_CACHE, cutEnds), videoPath);
    QVERIFY(Db(cachePath).readMetadata(loaded));
    QCOMPARE(loaded.modified, modified);
    QCOMPARE(loaded._fileCreateDate, birthTime);
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
                               QStringLiteral("all extracted frames were blank"));
    }

    QCOMPARE(cachedFailure(cachePath, videoPath), QStringLiteral("all extracted frames were blank"));
    QCOMPARE(processError(videoPath, cachePath, Prefs::WITH_CACHE, cutEnds),
             skippedMessage(QStringLiteral("all extracted frames were blank")));
    QCOMPARE(processError(videoPath, cachePath, Prefs::WITH_CACHE, thumb1),
             skippedMessage(QStringLiteral("all extracted frames were blank")));

    // Scanning without the cache must reach FFmpeg again, and must not overwrite the cached failure.
    const QString bypassedError = processError(videoPath, cachePath, Prefs::NO_CACHE, cutEnds);
    QVERIFY(bypassedError.contains(QStringLiteral("could not read metadata")));
    QCOMPARE(cachedFailure(cachePath, videoPath), QStringLiteral("all extracted frames were blank"));

    QVERIFY(Db::emptyAllDb(prefs));
    QVERIFY(cachedFailure(cachePath, videoPath).isEmpty());
    const QString retryError = processError(videoPath, cachePath, Prefs::WITH_CACHE, cutEnds);
    QVERIFY(retryError.contains(QStringLiteral("could not read metadata")));
    QCOMPARE(cachedFailure(cachePath, videoPath), retryError);
}

QTEST_MAIN(TestFailedVideoCache)

#include "tst_failed_video_cache.moc"
