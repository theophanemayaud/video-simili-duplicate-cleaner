#include <QtTest>
#include <QCoreApplication>

// add necessary includes here
#include "../app/video.h"
#include "../app/prefs.h"

#include "../app/mainwindow.h"

class TestVideo : public QObject
{
    Q_OBJECT

public:
    TestVideo();
    ~TestVideo();

private slots:
    void initTestCase();
    void cleanupTestCase();
//    void test_create_save_thumbnails();
    void test_check_thumbnails();
    void test_whole_app();

};

TestVideo::TestVideo()
{

}

TestVideo::~TestVideo()
{

}

void TestVideo::initTestCase()
{

}

void TestVideo::cleanupTestCase()
{

}

void TestVideo::test_whole_app(){
    qSetMessagePattern("%{file}(%{line}) %{function}: %{message}");
    qDebug() << "Program start by Théophane with path :" << QDir::currentPath();

//    QApplication a();
    MainWindow w;
    w.show();

    qDebug() << w.height();
    QVERIFY(w.detectffmpeg());

    const QDir dir("/Users/theophanemayaud/Movies");
    qDebug() << dir.absolutePath();
    QVERIFY(dir.exists());
    w.ui->directoryBox->insert(QStringLiteral(";%1").arg(dir.absolutePath()));
    w.on_findDuplicates_clicked();

    qDebug() << "found "<<w._prefs._numberOfVideos<<" videos";

};

void TestVideo::test_check_thumbnails(){
    QFileInfo ffmpegInfo = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/QtProject/app/deps/ffmpeg");
    QVERIFY(ffmpegInfo.exists());
    QProcess ffmpeg;
    ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
    ffmpeg.setProgram(ffmpegInfo.absoluteFilePath());
    ffmpeg.start();
    ffmpeg.waitForFinished();
    QVERIFY(!ffmpeg.readAllStandardOutput().isEmpty());

    QFileInfo vid1Info = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/samples/videos/Nice_720p_1000kbps.mp4");
    QFileInfo vid2Info = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/samples/videos/Nice_383p_500kbps.mp4");
    QVERIFY(vid1Info.exists());
    QVERIFY(vid2Info.exists());

    Prefs prefs;
    Video *vid1 = new Video(prefs, ffmpegInfo.absoluteFilePath(), vid1Info.absoluteFilePath());
    Video *vid2 = new Video(prefs, ffmpegInfo.absoluteFilePath(), vid2Info.absoluteFilePath());
    vid1->run();
    vid2->run();

    QByteArray thumbnail1;
    QByteArray thumbnail2;
    QFile file1("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/QtProject/tests/ressources/Nice_720p_1000kbps.thumbnail");
    QVERIFY(file1.exists());
    file1.open(QIODevice::ReadOnly);
    thumbnail1 = file1.readAll();
    file1.close();
    QFile file2("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/QtProject/tests/ressources/Nice_383p_500kbps.thumbnail");
    file2.open(QIODevice::ReadOnly);
    thumbnail2 = file2.readAll();
    file2.close();

    QVERIFY(thumbnail1 == vid1->thumbnail);
    QVERIFY(thumbnail2 == vid2->thumbnail);
}

/*void TestVideo::test_create_save_thumbnails()
{
    QFileInfo ffmpegInfo = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/QtProject/app/deps/ffmpeg");
    QVERIFY(ffmpegInfo.exists());
    QProcess ffmpeg;
    ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
    ffmpeg.setProgram(ffmpegInfo.absoluteFilePath());
    ffmpeg.start();
    ffmpeg.waitForFinished();
    QVERIFY(!ffmpeg.readAllStandardOutput().isEmpty());

    QFileInfo vid1Info = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/samples/videos/Nice_720p_1000kbps.mp4");
    QFileInfo vid2Info = QFileInfo("/Users/theophanemayaud/Dev/Programming videos dupplicates/video-simili-duplicate-cleaner/samples/videos/Nice_383p_500kbps.mp4");
    QVERIFY(vid1Info.exists());
    QVERIFY(vid2Info.exists());

    Prefs prefs;
    Video *vid1 = new Video(prefs, ffmpegInfo.absoluteFilePath(), vid1Info.absoluteFilePath());
    Video *vid2 = new Video(prefs, ffmpegInfo.absoluteFilePath(), vid2Info.absoluteFilePath());
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
QTEST_MAIN(TestVideo)

#include "tst_video.moc"
