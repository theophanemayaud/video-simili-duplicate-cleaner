#include <QtTest>
#include <QCoreApplication>
#include <QFile>

#include "../../app/video.h"
#include "../../app/prefs.h"
#include "../../app/db.h"
#include "video_simplified_test_helpers.h"

class TestVideoSimplified : public QObject
{
    Q_OBJECT

public:
    TestVideoSimplified();
    ~TestVideoSimplified();

private slots:
    void initTestCase();
    
    // Data-driven test for video scanning
    void test_videoScanning_data();
    void test_videoScanning();
    
    // Reference data generation (commented out when not needed to avoid accidental execution)
    // void generateReferenceData();
    
    void cleanupTestCase();

private:
    QString samplesDir;
    QString projectRoot;
};

TestVideoSimplified::TestVideoSimplified()
{
}

TestVideoSimplified::~TestVideoSimplified()
{
}

void TestVideoSimplified::initTestCase()
{
    qSetMessagePattern("%{file}(%{line}) %{function}: %{message}");
    Prefs().resetSettings();
    qDebug() << "Cleared settings";
    
    // Setup paths relative to project root
    projectRoot = QDir::currentPath();
    while (!QFileInfo::exists(projectRoot + "/samples/videos") && projectRoot != "/") {
        projectRoot = QDir(projectRoot).absolutePath();
        QDir dir(projectRoot);
        if (!dir.cdUp()) break;
        projectRoot = dir.absolutePath();
    }
    
    samplesDir = projectRoot + "/samples/videos";
    
    qDebug() << "Project root:" << projectRoot;
    qDebug() << "Samples directory:" << samplesDir;
    
    QVERIFY2(QFileInfo::exists(samplesDir), 
        QString("Samples directory not found: %1").arg(samplesDir).toUtf8());
}

void TestVideoSimplified::test_videoScanning_data()
{
    QTest::addColumn<QString>("videoName");
    QTest::addColumn<int>("cacheMode");
    QTest::addColumn<double>("ssimThreshold");
    
    QStringList videos = {"Nice_383p_500kbps.mp4", "Nice_720p_1000kbps.mp4"};
    QList<int> cacheModes = {Prefs::NO_CACHE, Prefs::WITH_CACHE, Prefs::CACHE_ONLY};
    QList<int> ssimPercents = {100, 99, 98, 97, 96, 95, 90};
    
    for (const QString& video : videos) {
        for (int cacheMode : cacheModes) {
            for (int ssimPercent : ssimPercents) {
                QString cacheName = (cacheMode == Prefs::NO_CACHE) ? "nocache" :
                                   (cacheMode == Prefs::WITH_CACHE) ? "cache" : "cacheonly";
                QString rowName = QString("%1-%2-%3pct")
                    .arg(video.left(video.indexOf('.')))
                    .arg(cacheName)
                    .arg(ssimPercent);
                
                QTest::newRow(rowName.toStdString().c_str()) 
                    << video << cacheMode << (ssimPercent / 100.0);
            }
        }
    }
}

void TestVideoSimplified::test_videoScanning()
{
    QFETCH(QString, videoName);
    QFETCH(int, cacheMode);
    QFETCH(double, ssimThreshold);
    
    // Setup paths
    QString videoPath = samplesDir + "/" + videoName;
    QString refMetadataPath = samplesDir + "/" + videoName + ".txt";
    QString refThumbPath = samplesDir + "/" + videoName + ".jpg";
    
    QVERIFY2(QFileInfo::exists(videoPath), 
        QString("Video not found: %1").arg(videoPath).toUtf8());
    QVERIFY2(QFileInfo::exists(refMetadataPath), 
        QString("Reference metadata not found: %1. Run generateReferenceData() first.").arg(refMetadataPath).toUtf8());
    QVERIFY2(QFileInfo::exists(refThumbPath), 
        QString("Reference thumbnail not found: %1. Run generateReferenceData() first.").arg(refThumbPath).toUtf8());
    
    // Need to ensure cache exists when we want the test to use it
    if (cacheMode == Prefs::CACHE_ONLY || cacheMode == Prefs::WITH_CACHE) {
        Prefs cachePrefs;
        cachePrefs.useCacheOption(Prefs::WITH_CACHE);
        Db::initDbAndCacheLocation(cachePrefs);
        
        // Do a scan with cache to populate if needed
        VideoParam warmupParam = SimplifiedTestHelpers::scanVideoMetadata(videoPath, cachePrefs);
        QVERIFY2(!warmupParam.thumbnail.isEmpty(), 
            QString("Failed to warm up cache for: %1").arg(videoPath).toUtf8());
    }
    
    // Load reference data
    VideoParam refParam;
    SimplifiedTestHelpers::loadMetadataFromFile(refMetadataPath, refParam);
    QByteArray refThumbnail = SimplifiedTestHelpers::loadThumbnailFromFile(refThumbPath);
    
    QVERIFY2(!refThumbnail.isEmpty(), 
        QString("Failed to load reference thumbnail: %1").arg(refThumbPath).toUtf8());
    
    // Scan video with current settings
    Prefs prefs;
    prefs.useCacheOption(static_cast<Prefs::USE_CACHE_OPTION>(cacheMode));
    
    VideoParam currentParam = SimplifiedTestHelpers::scanVideoMetadata(videoPath, prefs);
    
    QVERIFY2(!currentParam.thumbnail.isEmpty(), 
        QString("Failed to scan video: %1").arg(videoPath).toUtf8());
    
    // Compare metadata
    QString errorMsg;
    QVERIFY2(SimplifiedTestHelpers::compareMetadata(refParam, currentParam, errorMsg), 
        errorMsg.toUtf8());
    
    // Compare thumbnails
    double ssim = SimplifiedTestHelpers::compareThumbnails(refThumbnail, currentParam.thumbnail);
        
    // we expect some combinations to fail and some to pass
    bool shouldPass = false;
    if (cacheMode == Prefs::NO_CACHE)
        shouldPass = true; // all thresholds should pass in no cache mode
    else if (cacheMode == Prefs::WITH_CACHE && ssimThreshold <= 0.97)
        shouldPass = true;
    else if (cacheMode == Prefs::CACHE_ONLY && ssimThreshold <= 0.97)
        shouldPass = true;

    if (shouldPass)
        QVERIFY2(ssim >= ssimThreshold, 
            QString("SSIM %1 below threshold %2 for %3").arg(ssim).arg(ssimThreshold).arg(videoName).toUtf8());
    else
        QVERIFY2(ssim < ssimThreshold, 
            QString("SSIM %1 above threshold %2 for %3, expected to not have a match").arg(ssim).arg(ssimThreshold).arg(videoName).toUtf8());
 
}

// Uncomment this function and add it to private slots to generate reference data
/*
// Generate reference data for test videos
// To use: run: ./test_video_simplified generateReferenceData
void TestVideoSimplified::generateReferenceData()
{
    // Run with NO_CACHE to generate baseline reference data
    Prefs prefs;
    prefs.useCacheOption(Prefs::NO_CACHE);
    
    QStringList videos = {"Nice_383p_500kbps.mp4", "Nice_720p_1000kbps.mp4"};
    
    for (const QString& video : videos) {
        qDebug() << "\nGenerating reference data for:" << video;
        
        QString videoPath = samplesDir + "/" + video;
        QVERIFY2(QFileInfo::exists(videoPath), 
            QString("Video not found: %1").arg(videoPath).toUtf8());
        
        VideoParam param = SimplifiedTestHelpers::scanVideoMetadata(videoPath, prefs);
        QVERIFY2(!param.thumbnail.isEmpty(), 
            QString("Failed to process video: %1").arg(videoPath).toUtf8());
        
        QString metadataPath = samplesDir + "/" + video + ".txt";
        QString thumbPath = samplesDir + "/" + video + ".jpg";
        
        // Remove existing files if they exist
        if (QFileInfo::exists(metadataPath)) {
            qDebug() << "Removing existing metadata file:" << metadataPath;
            QFile::remove(metadataPath);
        }
        if (QFileInfo::exists(thumbPath)) {
            qDebug() << "Removing existing thumbnail:" << thumbPath;
            QFile::remove(thumbPath);
        }
        
        QVERIFY2(SimplifiedTestHelpers::saveMetadataToFile(param, metadataPath, QDir(samplesDir)),
            QString("Failed to save metadata: %1").arg(metadataPath).toUtf8());
        QVERIFY2(SimplifiedTestHelpers::saveThumbnail(param.thumbnail, thumbPath),
            QString("Failed to save thumbnail: %1").arg(thumbPath).toUtf8());
        
        qDebug() << "Successfully generated reference data for:" << video;
        qDebug() << "  Metadata:" << metadataPath;
        qDebug() << "  Thumbnail:" << thumbPath;
        qDebug() << "  Video size:" << param.size << "bytes";
        qDebug() << "  Duration:" << param.duration << "ms";
        qDebug() << "  Dimensions:" << param.width << "x" << param.height;
        qDebug() << "  Codec:" << param.codec;
        qDebug() << "  Framerate:" << param.framerate;
    }
    
    qDebug() << "\nReference data generation complete!";
}
*/

void TestVideoSimplified::cleanupTestCase()
{
    // Clean up database if needed
    Prefs prefs;
    Db::emptyAllDb(prefs);
}

QTEST_MAIN(TestVideoSimplified)

#include "tst_video_simplified.moc"

