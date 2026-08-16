#include "video_corpus_test_helpers.h"

#include "../../../app/comparison/comparison.h"
#include "../../../app/comparison/ui_comparison.h"
#include "../../../app/db.h"
#include "../../../app/mainwindow.h"
#include "../../../app/video.h"
#include "shared/video_extraction_test_helpers.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTest>

#include <algorithm>
#include <memory>

namespace
{
void compareReferenceVideo(const QByteArray& referenceThumbnail, const VideoParam& reference, const Video& video,
                           bool acceptSmallDurationDiff)
{
    const QString label = QStringLiteral("For %1").arg(reference.thumbnailInfo.absoluteFilePath());
    int firstSimilarity = 64;
    int secondSimilarity = 64;
    for (uint64_t bits = reference.hash1 ^ video.fingerprint(0).phash; bits; bits &= bits - 1)
        --firstSimilarity;
    for (uint64_t bits = reference.hash2 ^ video.fingerprint(1).phash; bits; bits &= bits - 1)
        --secondSimilarity;
    const double ssim = VideoExtractionTestHelpers::compareThumbnails(referenceThumbnail, video.thumbnail);

    QVERIFY2(reference.size == video.size,
             qPrintable(QStringLiteral("ref size=%1 new size=%2 - %3").arg(reference.size).arg(video.size).arg(label)));
    QCOMPARE(reference.modified.toString(VideoParam::timeformat()), video.modified.toString(VideoParam::timeformat()));
    const qint64 durationTolerance = acceptSmallDurationDiff ? qMax(50, qRound(reference.duration * 0.01)) : 0;
    QVERIFY2(qAbs(reference.duration - video.duration) <= durationTolerance,
             qPrintable(QStringLiteral("ref duration=%1 new duration=%2, max accepted diff %3 - %4")
                            .arg(reference.duration)
                            .arg(video.duration)
                            .arg(durationTolerance)
                            .arg(label)));
    QVERIFY2(qAbs(reference.bitrate - video.bitrate) <= 50,
             qPrintable(QStringLiteral("ref bitrate=%1 new bitrate=%2 - %3")
                            .arg(reference.bitrate)
                            .arg(video.bitrate)
                            .arg(label)));
    QCOMPARE(reference.framerate, video.framerate);
    QCOMPARE(reference.codec, video.codec);
    QCOMPARE(reference.audio, video.audio);

    short expectedWidth = reference.width;
    short expectedHeight = reference.height;
    if (reference.width == video.height && reference.height == video.width)
        std::swap(expectedWidth, expectedHeight);
    QCOMPARE(expectedWidth, video.width);
    QCOMPARE(expectedHeight, video.height);
    QCOMPARE(referenceThumbnail.isNull(), video.thumbnail.isNull());
    if (referenceThumbnail.isEmpty() && video.thumbnail.isEmpty())
        return;
    QVERIFY2(firstSimilarity >= 60 && secondSimilarity >= 60 && ssim >= 0.90,
             qPrintable(QStringLiteral("Thumbnail changed: pHash=%1/64,%2/64 SSIM=%3 - %4")
                            .arg(firstSimilarity)
                            .arg(secondSimilarity)
                            .arg(ssim)
                            .arg(label)));
}
} // namespace

void VideoCorpusTestHelpers::initialize()
{
    qSetMessagePattern(QStringLiteral("%{file}(%{line}) %{function}: %{message}"));
    Prefs().resetSettings();
}

void VideoCorpusTestHelpers::emptyDatabase()
{
    Prefs prefs;
    QVERIFY(Db::initDbAndCacheLocation(prefs));
    Db::emptyAllDb(prefs);
}

void VideoCorpusTestHelpers::addReferenceVideoRows(const QDir& videoDir, int expectedVideoCount)
{
    emptyDatabase();
    QTest::addColumn<QString>("videoPath");
    QVERIFY2(videoDir.exists(), qPrintable(QStringLiteral("Corpus is not available: %1").arg(videoDir.path())));

    MainWindow window;
    window.loadExtensions();
    QVERIFY(!window._extensionList.isEmpty());
    window.close();

    QDir scanDir = videoDir;
    scanDir.setNameFilters(window._extensionList);
    QDirIterator iterator(scanDir, QDirIterator::Subdirectories);
    QStringList videoPaths;
    while (iterator.hasNext()) {
        const QString videoPath = iterator.next();
        if (!QFileInfo(videoPath).isDir())
            videoPaths.append(videoPath);
    }
    QCOMPARE(videoPaths.count(), expectedVideoCount);
    for (const QString& videoPath : videoPaths)
        QTest::newRow(qPrintable(videoDir.relativeFilePath(videoPath))) << videoPath;
}

void VideoCorpusTestHelpers::verifyReferenceVideo(Prefs::USE_CACHE_OPTION cacheOption,
                                                   bool acceptSmallDurationDiff)
{
    QFETCH(QString, videoPath);
    QVERIFY2(QFileInfo::exists(videoPath), qPrintable(videoPath));
    const QString suffix = cacheOption == Prefs::NO_CACHE ? QStringLiteral("nocache") : QStringLiteral("withcache");
    const QString metadataPath = videoPath + QStringLiteral(".") + suffix + QStringLiteral(".txt");
    QVERIFY2(QFileInfo::exists(metadataPath), qPrintable(QStringLiteral("Metadata file not found: %1").arg(metadataPath)));
    VideoParam reference;
    VideoExtractionTestHelpers::loadMetadataFromFile(metadataPath, reference);
    const QByteArray referenceThumbnail =
        VideoExtractionTestHelpers::loadThumbnailFromFile(videoPath + QStringLiteral(".") + suffix + QStringLiteral(".jpg"));

    Prefs prefs;
    prefs.useCacheOption(cacheOption);
    if (cacheOption != Prefs::NO_CACHE) {
        QVERIFY(Db::initDbAndCacheLocation(prefs));
        Prefs warmupPrefs = prefs;
        warmupPrefs.useCacheOption(Prefs::WITH_CACHE);
        Video warmup(warmupPrefs, videoPath);
        QVERIFY2(warmup.process().success, "Could not populate the capture cache");
    }
    Video video(prefs, videoPath);
    QVERIFY2(video.process().success, "Could not process reference video");
    compareReferenceVideo(referenceThumbnail, reference, video, acceptSmallDurationDiff);
}

void VideoCorpusTestHelpers::runWholeAppScan(const QDir& videoDir, const WholeAppScanExpectation& expectation)
{
    QVERIFY2(videoDir.exists(), qPrintable(QStringLiteral("Corpus is not available: %1").arg(videoDir.path())));
    if (expectation.cacheOption != Prefs::NO_CACHE) {
        MainWindow warmup;
        warmup.ui->radio_UseCacheYes->click();
        QDir warmupDir = videoDir;
        warmup.findVideos(warmupDir);
        warmup.processVideos();
        warmup.close();
        QCoreApplication::processEvents();
    }

    MainWindow window;
    window.show();
    window.on_thresholdSlider_valueChanged(expectation.similarityThreshold);
    window.ui->detectRotatedCopiesCheckbox->setChecked(expectation.detectRotatedCopies);
    switch (expectation.cacheOption) {
    case Prefs::CACHE_ONLY:
        window.ui->radio_UseCacheOnly->click();
        break;
    case Prefs::WITH_CACHE:
        window.ui->radio_UseCacheYes->click();
        break;
    case Prefs::NO_CACHE:
        window.ui->radio_UseCacheNo->click();
        break;
    }

    QElapsedTimer timer;
    timer.start();
    QDir scanDir = videoDir;
    window.findVideos(scanDir);
    window.processVideos();
    Comparison comparison(window._videoList, window._prefs, window.geometry());
    const int matchingVideos = comparison.reportMatchingVideos();

    QCOMPARE(window._everyVideo.count(), expectation.expectedVideos);
    QCOMPARE(window._videoList.count(), expectation.expectedValidVideos);
    QCOMPARE(matchingVideos, expectation.expectedMatchingVideos);
    QVERIFY2(timer.elapsed() < expectation.maximumElapsedMs,
             qPrintable(QStringLiteral("Whole scan took %1ms; limit is %2ms")
                            .arg(timer.elapsed())
                            .arg(expectation.maximumElapsedMs)));
    window.close();
    QCoreApplication::processEvents();
}
