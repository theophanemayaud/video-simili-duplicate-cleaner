#include <QtTest>
#include <QCoreApplication>

//#define CREATE_REFERENCE_TEST_DATA
//#define ENABLE_MANUAL_THUMBNAIL_VERIF

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
#ifdef CREATE_REFERENCE_TEST_DATA
    void test_create_save_thumbnails();
#endif
    void test_whole_app();
    void test_check_thumbnails();

};

class TestHelpers {
public:
    static bool doThumbnailsLookSameWindow(const QByteArray ref_thumb, const QByteArray new_thumb, const QString title);
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

#ifdef CREATE_REFERENCE_TEST_DATA
void TestVideo::test_create_save_thumbnails()
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
#endif

// -------------------------------------------------
// ------------START TestHelpers -----------------
bool TestHelpers::doThumbnailsLookSameWindow(const QByteArray ref_thumb, const QByteArray new_thumb, const QString title){
    QWidget *ui_image = new QWidget;
    ui_image->setWindowTitle(title);
    QVBoxLayout *layout = new QVBoxLayout(ui_image);

    // Create two image labels
    QLabel *label = new QLabel(ui_image);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QPixmap mPixmap;
    mPixmap.loadFromData(ref_thumb,"JPG");
    label->setPixmap(mPixmap);
    label->setScaledContents(true);
    QLabel *label2 = new QLabel(ui_image);
    label2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QPixmap mPixmap2;
    mPixmap2.loadFromData(new_thumb,"JPG");
    label2->setPixmap(mPixmap2);
    label2->setScaledContents(true);

    // Create buttons and texts
    QLabel *refText = new QLabel(ui_image);
    refText->setText("Ref thumbnail");
    QLabel *newText = new QLabel(ui_image);
    newText->setText("New thumbnail");
    QHBoxLayout *checkLayout = new QHBoxLayout(ui_image);
    QCheckBox *accept = new QCheckBox(ui_image);
    accept->setText("Identical looking");
    QCheckBox *reject = new QCheckBox(ui_image);
    reject->setText("Different looking");
    checkLayout->addWidget(accept);
    checkLayout->addWidget(reject);

    //Put all together
    layout->addWidget(refText);
    layout->addWidget(label);
    layout->addWidget(newText);
    layout->addWidget(label2);
    layout->addLayout(checkLayout);

    ui_image->setLayout(layout);
    ui_image->showMaximized();
    while(ui_image->isVisible()){
        QTest::qWait(100);
        if(accept->isChecked()){
            ui_image->close();
            return true;
        }
        if(reject->isChecked()){
            ui_image->close();
            return false;
        }
    }

    return false;
}

// --------------END TestHelpers -------------------------
// -------------------------------------------------
QTEST_MAIN(TestVideo)

#include "tst_video.moc"
