#include <QtTest>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrent>
#include <QElapsedTimer>

/* NB : sometimes, for some unknown reason, thumbnails don't come out the same.
 * But if you re-run tests a few times, it should get fixed
 * (or check visually with ENABLE_MANUAL_THUMBNAIL_VERIF) */
//#define ENABLE_MANUAL_THUMBNAIL_VERIF

// Sometimes hashes go crazy, so we can manually disable them to see if other problems exist
//#define ENABLE_HASHES_VERIFICATION

// add necessary includes here
#include "../../app/video.h"
#include "../../app/prefs.h"

#include "../../app/mainwindow.h"
#include "../../app/comparison.h"
#include "../../app/ui_comparison.h"

#include "video_test_helpers.cpp"

class TestVideo : public QObject
{
    Q_OBJECT

public:
    TestVideo();
    ~TestVideo();

    static void compareVideoParamToVideo(const QByteArray ref_thumbnail, const VideoParam videoParam, const Video *vid);

private:
    const QFileInfo _ffmpegInfo = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/QtProject/app/deps/ffmpeg");

    QDir _videoDir = QDir("/Users/theophanemayaud/Dev/Programming videos dupplicates/Videos across all formats with duplicates of all kinds/Videos/");
    const QDir _thumbnailDir = QDir("/Users/theophanemayaud/Dev/Programming videos dupplicates/Videos across all formats with duplicates of all kinds/Thumbnails/");
    const QFileInfo _csvInfo = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/QtProject/tests/test_video/tests.csv");

    QDir _100GBvideoDir = QDir("/Volumes/Mays2TOSSD/ZZ - Temporaires pas backup/Video duplicates - just for checking later my video duplicate program still works/Videos/");
    const QDir _100GBthumbnailDir = QDir("/Volumes/Mays2TOSSD/ZZ - Temporaires pas backup/Video duplicates - just for checking later my video duplicate program still works/Thumbnails/");
    const QFileInfo _100GBcsvInfo = QFileInfo("/Volumes/Mays2TOSSD/ZZ - Temporaires pas backup/Video duplicates - just for checking later my video duplicate program still works/100GBtests.csv");

    MainWindow *w = nullptr;

private slots:
    void initTestCase();
//    void test_create_save_thumbnails();
//    void test_create_reference_video_params();

    void test_check_samples_thumbnails();

    void test_check_reference_video_params();

    void test_whole_app();

//    void test_100GB_create_reference_video_params();
    void test_100GBcheck_reference_video_params();
    void test_whole_app_100GB();

    void cleanupTestCase();

};

TestVideo::TestVideo()
{

}

TestVideo::~TestVideo()
{

}

// Run before all tests
void TestVideo::initTestCase(){
    qSetMessagePattern("%{file}(%{line}) %{function}: %{message}");
}

// Run after all tests
void TestVideo::cleanupTestCase()
{

}

void TestVideo::test_whole_app(){
    // constants of the test
    const int nb_vids_to_find = 190;
    const int nb_valid_vids_to_find = 184;
    const qint64 ref_ms_time = 10*1000;

    QElapsedTimer timer;
    timer.start();

    w = new MainWindow;
    w->show();
    QVERIFY(w->detectffmpeg());

    QVERIFY(_videoDir.exists());
//    w->ui->directoryBox->insert(QStringLiteral(";%1").arg(_videoDir.absolutePath()));
    //w->on_findDuplicates_clicked();

    w->findVideos(_videoDir);
    w->processVideos();
    w->ui->blocksizeCombo->setAcceptDrops(true);

    qDebug() << "Found "<<w->_everyVideo.count()<<" files of which "<< w->_videoList.count()<<" valid ones";
    QVERIFY2(w->_everyVideo.count()==nb_vids_to_find, QString("Only found %1 files").arg(w->_everyVideo.count()).toStdString().c_str());
    QVERIFY2(w->_videoList.count()==nb_valid_vids_to_find, QString("Only found %1 valid files").arg(w->_videoList.count()).toStdString().c_str());

    qDebug() << "TIMER:test_whole_app took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_whole_app took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());

    Comparison comp(w->sortVideosBySize(), w->_prefs);
    comp.reportMatchingVideos();

    comp.show();

    QTest::qWait(1000);
    comp.on_nextVideo_clicked();
    comp.ui->tabWidget->setCurrentIndex(2);
    QTest::qWait(1000);
};

void TestVideo::test_check_samples_thumbnails(){
    QVERIFY2(TestHelpers::detectFfmpeg(_ffmpegInfo), "couldn't detect ffmpeg");

    QFileInfo vid1Info = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/samples/videos/Nice_720p_1000kbps.mp4");
    QFileInfo vid2Info = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/samples/videos/Nice_383p_500kbps.mp4");
    QVERIFY(vid1Info.exists());
    QVERIFY(vid2Info.exists());

    Prefs prefs;
    Video *vid1 = new Video(prefs, _ffmpegInfo.absoluteFilePath(), vid1Info.absoluteFilePath());
    Video *vid2 = new Video(prefs, _ffmpegInfo.absoluteFilePath(), vid2Info.absoluteFilePath());
    vid1->run();
    vid2->run();

    QByteArray thumbnail1;
    QByteArray thumbnail2;
    QFile file1("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/QtProject/tests/test_video/ressources/Nice_720p_1000kbps.thumbnail");
    QVERIFY(file1.exists());
    file1.open(QIODevice::ReadOnly);
    thumbnail1 = file1.readAll();
    file1.close();
    QFile file2("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/QtProject/tests/test_video/ressources/Nice_383p_500kbps.thumbnail");
    file2.open(QIODevice::ReadOnly);
    thumbnail2 = file2.readAll();
    file2.close();

    QVERIFY(thumbnail1 == vid1->thumbnail
        #ifdef ENABLE_MANUAL_THUMBNAIL_VERIF
            || TestHelpers::doThumbnailsLookSameWindow(thumbnail1, vid1->thumbnail, "Thumbnail 1 vs 1")
        #endif
            );
    QVERIFY(thumbnail2 == vid2->thumbnail
        #ifdef ENABLE_MANUAL_THUMBNAIL_VERIF
            || TestHelpers::doThumbnailsLookSameWindow(thumbnail2, vid2->thumbnail, "Thumbnail 1 vs 1")
        #endif
            );
}

void TestVideo::test_check_reference_video_params(){
    const qint64 ref_ms_time = 30*1000;

    QElapsedTimer timer;
    timer.start();
    QVERIFY2(TestHelpers::detectFfmpeg(_ffmpegInfo), "couldn't detect ffmpeg");

    // read csv file
    QVERIFY(_csvInfo.exists());
    QList<VideoParam> videoParamList = TestHelpers::importCSVtoVideoParamQList(_csvInfo);
    QVERIFY(!videoParamList.isEmpty());

    // compute params for all videos
    QVERIFY(!_videoDir.isEmpty());
    foreach(VideoParam videoParam, videoParamList){
        QVERIFY2(videoParam.videoInfo.exists(), videoParam.videoInfo.absoluteFilePath().toUtf8().constData());
        QVERIFY2(videoParam.thumbnailInfo.exists(), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());

        //read thumbnail from disk
        QFile ref_thumbFile(videoParam.thumbnailInfo.absoluteFilePath());
        QVERIFY2(ref_thumbFile.open(QIODevice::ReadOnly), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());
        QByteArray ref_thumbnail = ref_thumbFile.readAll();
        ref_thumbFile.close();

        Prefs prefs;
        Video *vid = new Video(prefs, _ffmpegInfo.absoluteFilePath(), videoParam.videoInfo.absoluteFilePath());
        vid->run();

        compareVideoParamToVideo(ref_thumbnail, videoParam, vid);
    }
    qDebug() << "TIMER:test_check_reference_video_params took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_check_reference_video_params took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
}

/* void TestVideo::test_create_reference_video_params()
{
    QVERIFY2(TestHelpers::detectFfmpeg(_ffmpegInfo), "couldn't detect ffmpeg");
    QVERIFY(!_csvInfo.exists());    // we don't want to overwrite it !

    QVERIFY(!_videoDir.isEmpty());
    MainWindow w;
    w->loadExtensions();
    QVERIFY(!w->_extensionList.isEmpty());
    _videoDir.setNameFilters(w->_extensionList);
    QDirIterator iter(_videoDir, QDirIterator::Subdirectories);

    QList<VideoParam> videoParamList;

    while(iter.hasNext())
    {
        const QFile vidFile(iter.next());
        const QFileInfo vidInfo = QFileInfo(vidFile);

        VideoParam videoParam;
        videoParam.videoInfo = vidInfo;

        Prefs prefs;
        Video *vid = new Video(prefs, _ffmpegInfo.absoluteFilePath(), vidInfo.absoluteFilePath());
        vid->run();
        QFile thumbFile(_thumbnailDir.path() + "/" + vidInfo.fileName() + ".thumbnail");
        qDebug() << thumbFile.fileName();
        QVERIFY(!thumbFile.exists()); // we don't want to overwrite !
        thumbFile.open(QIODevice::WriteOnly);
        thumbFile.write(vid->thumbnail);
        thumbFile.close();
        videoParam.thumbnailInfo = QFileInfo(thumbFile);
        videoParam.size = vid->size;
        videoParam.modified = vid->modified;
        videoParam.duration = vid->duration;
        videoParam.bitrate = vid->bitrate;
        videoParam.framerate = vid->framerate;
        videoParam.codec = vid->codec;
        videoParam.audio = vid->audio;
        videoParam.width = vid->width;
        videoParam.height = vid->height;
        videoParam.hash1 = vid->hash[0];
        videoParam.hash2 = vid->hash[1];

        videoParamList.append(videoParam);
    }

    QVERIFY(!videoParamList.isEmpty());
    QVERIFY(TestHelpers::saveVideoParamQListToCSV(videoParamList, _csvInfo));
}
*/
/* void TestVideo::test_create_save_thumbnails()
{
    QVERIFY(_ffmpegInfo.exists());
    QProcess ffmpeg;
    ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
    ffmpeg.setProgram(_ffmpegInfo.absoluteFilePath());
    ffmpeg.start();
    ffmpeg.waitForFinished();
    QVERIFY(!ffmpeg.readAllStandardOutput().isEmpty());

    QFileInfo vid1Info = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/samples/videos/Nice_720p_1000kbps.mp4");
    QFileInfo vid2Info = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/samples/videos/Nice_383p_500kbps.mp4");
    QVERIFY(vid1Info.exists());
    QVERIFY(vid2Info.exists());

    Prefs prefs;
    Video *vid1 = new Video(prefs, _ffmpegInfo.absoluteFilePath(), vid1Info.absoluteFilePath());
    Video *vid2 = new Video(prefs, _ffmpegInfo.absoluteFilePath(), vid2Info.absoluteFilePath());
    vid1->run();
    vid2->run();

    qDebug() << "Vid 1 codec:"<<vid1->codec<<
                " duration:"<<vid1->duration<<
                " framerate:"<< vid1->framerate<<
                " bitrate:" << vid1->bitrate <<
                " name:"<<vid1->filename;
    QFile file1("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/QtProject/tests/ressources/Nice_720p_1000kbps.thumbnail");
    file1.open(QIODevice::WriteOnly);
    file1.write(vid1->thumbnail);
    file1.close();
    qDebug() << "Vid 2 codec:"<<vid2->codec<<
                " duration:"<<vid2->duration<<
                " framerate:"<< vid2->framerate<<
                " bitrate:" << vid2->bitrate<<
                " name:"<<vid2->filename;
    QFile file2("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/QtProject/tests/ressources/Nice_383p_500kbps.thumbnail");
    file2.open(QIODevice::WriteOnly);
    file2.write(vid2->thumbnail);
    file2.close();
}
*/

// ------------------------------------------------------------------------------------
// ---------------------------- START : 100GB tests from SSD ---------------------
/*void TestVideo::test_100GB_create_reference_video_params()
{
    QElapsedTimer timer;
    timer.start();

    QVERIFY2(TestHelpers::detectFfmpeg(_ffmpegInfo), "couldn't detect ffmpeg");

    QVERIFY(!_100GBvideoDir.isEmpty());
    QVERIFY(!_100GBcsvInfo.exists());    // we don't want to overwrite it !

    MainWindow w;
    w.loadExtensions();
    QVERIFY(!w._extensionList.isEmpty());
    _100GBvideoDir.setNameFilters(w._extensionList);
    QDirIterator iter(_100GBvideoDir, QDirIterator::Subdirectories);

    QList<VideoParam> videoParamList;

    while(iter.hasNext())
    {
        const QFile vidFile(iter.next());
        const QFileInfo vidInfo = QFileInfo(vidFile);

        VideoParam videoParam;
        videoParam.videoInfo = vidInfo;
//        qDebug() << "Video file path : "<< vidInfo.absoluteFilePath();
        QVERIFY2(vidInfo.exists(), QString("File not found %1").arg(vidInfo.absoluteFilePath()).toStdString().c_str());

        Prefs prefs;
        Video *vid = new Video(prefs, _ffmpegInfo.absoluteFilePath(), vidInfo.absoluteFilePath());
        vid->run();
        QString thumbPath = _100GBthumbnailDir.path() + "/" + vidInfo.path().remove(_100GBvideoDir.path()) + "-" + vidInfo.fileName() + ".thumbnail";
//        qDebug() << "Thumbmail file path" << thumbPath;
        QFile thumbFile(thumbPath);
        QVERIFY(!thumbFile.exists()); // we don't want to overwrite !
        thumbFile.open(QIODevice::WriteOnly);
        thumbFile.write(vid->thumbnail);
        thumbFile.close();
        videoParam.thumbnailInfo = QFileInfo(thumbFile);
        videoParam.size = vid->size;
        videoParam.modified = vid->modified;
        videoParam.duration = vid->duration;
        videoParam.bitrate = vid->bitrate;
        videoParam.framerate = vid->framerate;
        videoParam.codec = vid->codec;
        videoParam.audio = vid->audio;
        videoParam.width = vid->width;
        videoParam.height = vid->height;
        videoParam.hash1 = vid->hash[0];
        videoParam.hash2 = vid->hash[1];

        videoParamList.append(videoParam);
    }

    QVERIFY(!videoParamList.isEmpty());
    QVERIFY(TestHelpers::saveVideoParamQListToCSV(videoParamList, _100GBcsvInfo));
    qDebug() << "TIMER:test_100GB_create_reference_video_params took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
}*/

void TestVideo::test_100GBcheck_reference_video_params(){
    const qint64 ref_ms_time = 40*60*1000; // measured once at 37 min

    QElapsedTimer timer;
    timer.start();
    QVERIFY2(TestHelpers::detectFfmpeg(_ffmpegInfo), "couldn't detect ffmpeg");

    // read csv file
    QVERIFY(_100GBcsvInfo.exists());
    QList<VideoParam> videoParamList = TestHelpers::importCSVtoVideoParamQList(_100GBcsvInfo);
    QVERIFY(!videoParamList.isEmpty());

    // TODO : find a way to make this multithreaded, otherwise for all the videos it's too long !
    // compute params for all videos
    QVERIFY(!_100GBvideoDir.isEmpty());
    int test_nb = 0;
    const int nb_tests = videoParamList.count();
    foreach(VideoParam videoParam, videoParamList){

        QVERIFY2(videoParam.videoInfo.exists(), videoParam.videoInfo.absoluteFilePath().toUtf8().constData());
        QVERIFY2(videoParam.thumbnailInfo.exists(), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());

        //read thumbnail from disk
        QFile ref_thumbFile(videoParam.thumbnailInfo.absoluteFilePath());
        QVERIFY2(ref_thumbFile.open(QIODevice::ReadOnly), videoParam.thumbnailInfo.absoluteFilePath().toUtf8().constData());
        QByteArray ref_thumbnail = ref_thumbFile.readAll();
        ref_thumbFile.close();

        Prefs prefs;
        Video *vid = new Video(prefs, _ffmpegInfo.absoluteFilePath(), videoParam.videoInfo.absoluteFilePath());
        vid->run();

        compareVideoParamToVideo(ref_thumbnail, videoParam, vid);

        if(test_nb%100==0){
            qDebug() << "Processing "<<test_nb<<"/" << nb_tests << " for "<< videoParam.videoInfo.absoluteFilePath();
        }
        test_nb++;
    }

    qDebug() << "TIMER:test_100GBcheck_reference_video_params took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_100GBcheck_reference_video_params took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());
}

void TestVideo::test_whole_app_100GB(){
    // Constants of the test
    const int nb_vids_to_find = 12505;
    const int nb_valid_vids_to_find = 11988;
    const qint64 ref_ms_time = 20*60*1000;

    QElapsedTimer timer;
    timer.start();

    w = new MainWindow;
    w->show();
    QVERIFY(w->detectffmpeg());

    QVERIFY(_100GBvideoDir.exists());

    w->findVideos(_100GBvideoDir);
    w->processVideos();
    w->ui->blocksizeCombo->setAcceptDrops(true);

    qDebug() << "Found "<<w->_everyVideo.count()<<" files of which "<< w->_videoList.count()<<" valid ones";
    QVERIFY2(w->_everyVideo.count()==nb_vids_to_find, QString("Only found %1 files").arg(w->_everyVideo.count()).toStdString().c_str());
    QVERIFY2(w->_videoList.count()==nb_valid_vids_to_find, QString("Only found %1 valid files").arg(w->_videoList.count()).toStdString().c_str());

    qDebug() << "TIMER:test_whole_app_100GB took" << timer.elapsed()/1000 << "."<< timer.elapsed()%1000 << " secs";
    QVERIFY2(timer.elapsed()<ref_ms_time, QString("test_whole_app_100GB took : %1.%2s but should be below %3.%4s").arg(timer.elapsed()/1000).arg(timer.elapsed()%1000).arg(ref_ms_time/1000).arg(ref_ms_time%1000).toStdString().c_str());

    Comparison comp(w->sortVideosBySize(), w->_prefs);
    comp.reportMatchingVideos();

    comp.show();

    QTest::qWait(1000);
    comp.on_nextVideo_clicked();
    comp.ui->tabWidget->setCurrentIndex(2);
    QTest::qWait(1000);
};

// ---------------------------- END : 100GB tests from SSD ---------------------
// ------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------
// ---------------------------- START : helper testing functions ---------------------
void TestVideo::compareVideoParamToVideo(const QByteArray ref_thumbnail, const VideoParam videoParam, const Video *vid) {
    QVERIFY2(videoParam.size == vid->size, QString("ref size=%1 new size=%2").arg(videoParam.size).arg(vid->size).toUtf8().constData());
    QVERIFY2(videoParam.modified.toString(VideoParam::timeformat) == vid->modified.toString(VideoParam::timeformat) , QString("Date diff : ref modified=%1 new modified=%2").arg(videoParam.modified.toString(VideoParam::timeformat)).arg(vid->modified.toString(VideoParam::timeformat)).toUtf8().constData());
    QVERIFY2(videoParam.duration == vid->duration, QString("ref duration=%1 new duration=%2").arg(videoParam.duration).arg(vid->duration).toUtf8().constData());
    QVERIFY2(videoParam.bitrate == vid->bitrate, QString("ref bitrate=%1 new bitrate=%2").arg(videoParam.bitrate).arg(vid->bitrate).toUtf8().constData());
    QVERIFY2(videoParam.framerate == vid->framerate, QString("ref framerate=%1 new framerate=%2").arg(videoParam.framerate).arg(vid->framerate).toUtf8().constData());
    QVERIFY2(videoParam.codec == vid->codec, QString("ref codec=%1 new codec=%2").arg(videoParam.codec).arg(vid->codec).toUtf8().constData());
    QVERIFY2(videoParam.audio == vid->audio, QString("ref audio=%1 new audio=%2").arg(videoParam.audio).arg(vid->audio).toUtf8().constData());
    QVERIFY2(videoParam.width == vid->width, QString("ref width=%1 new width=%2").arg(videoParam.width).arg(vid->width).toUtf8().constData());
    QVERIFY2(videoParam.height == vid->height, QString("ref height=%1 new height=%2").arg(videoParam.height).arg(vid->height).toUtf8().constData());

    bool manuallyAccepted = false; // hash will be different anyways if thumbnails look different, so must skip these tests !
    if(!(ref_thumbnail == vid->thumbnail)){
#ifdef ENABLE_MANUAL_THUMBNAIL_VERIF
        if(TestHelpers::doThumbnailsLookSameWindow(ref_thumbnail,
                                           vid->thumbnail,
                                           QString("Thumbnail %1").arg(videoParam.thumbnailInfo.absoluteFilePath())))
            manuallyAccepted = true;
        else
            QVERIFY2(manuallyAccepted, QString("Different thumbnails for %1").arg(videoParam.thumbnailInfo.absoluteFilePath()).toUtf8().constData());
#endif

    }
#ifdef ENABLE_HASHES_VERIFICATION
    if(manuallyAccepted == false){ // hash will be different anyways if thumbnails look different, so must skip these tests
        QVERIFY2(videoParam.hash1 == vid->hash[0], QString("ref hash1=%1 new hash1=%2").arg(videoParam.hash1).arg(vid->hash[0]).toUtf8().constData());
        QVERIFY2(videoParam.hash2 == vid->hash[1], QString("ref hash2=%1 new hash2=%2").arg(videoParam.hash2).arg(vid->hash[1]).toUtf8().constData());
    }
#endif

}

// ---------------------------- END : helper testing functions ---------------------
// ------------------------------------------------------------------------------------
QTEST_MAIN(TestVideo)

#include "tst_video.moc"
