#include <QtTest>
#include <QCoreApplication>

// add necessary includes here
#include "../../app/video.h"
#include "../../app/comparison.h"

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

};

test_comparison::test_comparison()
{

}

test_comparison::~test_comparison()
{

}

void test_comparison::initTestCase()
{

}

void test_comparison::cleanupTestCase()
{

}

void test_comparison::test_videoToDelete_OnlyTimeDiffs()
{
    QDateTime refDate(QDate(2000, 1, 1), QTime(1, 0, 0));
    QDateTime earlierDate(QDate(1999, 1, 1), QTime(1, 0, 0));
    QDateTime laterDate(QDate(2001, 1, 1), QTime(1, 0, 0));

    VideoMetadata meta1, meta2;
    meta1.filename =    meta1.filename = "/users/test/videos/vid1.mp4";
    meta1.size =        meta2.size = 36*10*1024;
    meta1.duration =    meta2.duration = 36*1000;
    meta1.width =       meta2.width = 1080;
    meta1.height =      meta2.height = 720;
    meta1.framerate =   meta2.framerate = 30.31;
    meta1.codec =       meta2.codec = "hevc";
    meta1.bitrate =     meta2.bitrate = 10*1024;
    meta1.audio =       meta2.audio = "mp3";
    meta1.nameInApplePhotos = meta2.nameInApplePhotos = "";
    meta1.modified =    meta2.modified =
            meta1._fileCreateDate = meta2._fileCreateDate
            = QDateTime(QDate(2000, 1, 1), QTime(1, 0, 0));

    Comparison::AutoDeleteConfig autoDelConf(Comparison::AUTO_DELETE_ONLY_TIMES_DIFF);
    Comparison::AutoDeleteUserSettings userSet(false); // trash later video as is default

    //check default case, all exact same so should not be covered in this auto mode
    const VideoMetadata* vidToDeleteMetaPtr = autoDelConf.videoToDelete(&meta1, &meta2, userSet);
    QVERIFY2(vidToDeleteMetaPtr==nullptr, "Vids date metadata same but auto date comparison said they're different");

    meta1._fileCreateDate = earlierDate; meta2._fileCreateDate = laterDate;
    QVERIFY2(autoDelConf.videoToDelete(&meta1, &meta2, userSet) == &meta2, "Should have delete later creation date video but selected ealier");

    meta1._fileCreateDate = laterDate; meta2._fileCreateDate = earlierDate;
    QVERIFY2(autoDelConf.videoToDelete(&meta1, &meta2, userSet) == &meta1, "Should have delete later creation date video but selected ealier");

    meta1._fileCreateDate = refDate; meta2._fileCreateDate = refDate;
    meta1.modified = earlierDate; meta2.modified = laterDate;
    QVERIFY2(autoDelConf.videoToDelete(&meta1, &meta2, userSet) == &meta2, "Should have delete later modified date video but selected ealier");

    meta1._fileCreateDate = refDate; meta2._fileCreateDate = refDate;
    meta1.modified = laterDate; meta2.modified = earlierDate;
    QVERIFY2(autoDelConf.videoToDelete(&meta1, &meta2, userSet) == &meta1, "Should have delete later modified date video but selected ealier");

    meta1._fileCreateDate = earlierDate; meta2._fileCreateDate = laterDate;
    meta1.modified = laterDate; meta2.modified = earlierDate;
    QVERIFY2(autoDelConf.videoToDelete(&meta1, &meta2, userSet) == &meta2, "Later creation date should be deleted but instead later modified date or else was");

    // TODO could add more interesting tests with small differences, and check more specifically the outcomes
}

QTEST_MAIN(test_comparison)

#include "tst_test_comparison.moc"
