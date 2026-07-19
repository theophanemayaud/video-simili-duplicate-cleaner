#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <functional>

#include "../../app/comparison/comparison.h"
#include "../../app/comparison/ui_comparison.h"
#include "../../app/db.h"
#include "../../app/mainwindow.h"
#include "../../app/prefs.h"

class TestAutoDelete : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void test_keepBiggestMovesSmallerVideo();
    void test_keepSmallestMovesBiggerVideo();

  private:
    QString samplesDirPath() const;
    void acceptMessageBoxesDuring(const int expectedMessageBoxCount, const std::function<void()>& action) const;
    void runAutoDeleteBySize(const bool keepBiggest) const;
};

void TestAutoDelete::initTestCase()
{
    qSetMessagePattern("%{file}(%{line}) %{function}: %{message}");
    Prefs().resetSettings();
    Prefs prefs;
    Db::initDbAndCacheLocation(prefs);
    Db::emptyAllDb(prefs);
}

void TestAutoDelete::cleanupTestCase()
{
    Prefs prefs;
    Db::emptyAllDb(prefs);
}

QString TestAutoDelete::samplesDirPath() const
{
    auto projectRoot = QDir::currentPath();
    while (!QFileInfo::exists(projectRoot + "/samples/videos") && projectRoot != "/") {
        QDir dir(projectRoot);
        if (!dir.cdUp())
            break;
        projectRoot = dir.absolutePath();
    }

    return projectRoot + "/samples/videos";
}

void TestAutoDelete::acceptMessageBoxesDuring(const int expectedMessageBoxCount,
                                              const std::function<void()>& action) const
{
    QTimer timer;
    int acceptedMessageBoxes = 0;
    timer.setInterval(10);

    QObject::connect(&timer, &QTimer::timeout, [&acceptedMessageBoxes] {
        auto* messageBox = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (messageBox == nullptr) {
            for (auto* widget : QApplication::allWidgets()) {
                messageBox = qobject_cast<QMessageBox*>(widget);
                if (messageBox != nullptr)
                    break;
            }
        }
        if (messageBox == nullptr)
            return;
        if (messageBox->property("testAutoDeleteAccepted").toBool())
            return;

        messageBox->setProperty("testAutoDeleteAccepted", true);
        if (auto* okButton = messageBox->button(QMessageBox::Ok))
            okButton->click();
        else if (auto* yesButton = messageBox->button(QMessageBox::Yes))
            yesButton->click();
        else if (auto* defaultButton = messageBox->defaultButton())
            defaultButton->click();
        else
            messageBox->done(QMessageBox::Ok);

        acceptedMessageBoxes++;
    });

    timer.start();
    action();
    timer.stop();

    QCOMPARE(acceptedMessageBoxes, expectedMessageBoxCount);
}

void TestAutoDelete::test_keepBiggestMovesSmallerVideo()
{
    runAutoDeleteBySize(true);
}

void TestAutoDelete::test_keepSmallestMovesBiggerVideo()
{
    runAutoDeleteBySize(false);
}

// Tests auto deletion by file size ignoring resolution and fps.
// Copies two sample videos into a temporary folder, runs auto deletion with a custom trash folder,
// and verifies the selected keep-by-size choice controls which file is moved.
void TestAutoDelete::runAutoDeleteBySize(const bool keepBiggest) const
{
    const QDir samplesDir(samplesDirPath());
    QVERIFY2(samplesDir.exists(), QString("Samples directory not found: %1").arg(samplesDir.path()).toUtf8());

    const QString smallerVideoName = "Nice_383p_500kbps.mp4";
    const QString biggerVideoName = "Nice_720p_1000kbps.mp4";
    const QString smallerVideoPath = samplesDir.absoluteFilePath(smallerVideoName);
    const QString biggerVideoPath = samplesDir.absoluteFilePath(biggerVideoName);
    QVERIFY2(QFileInfo::exists(smallerVideoPath), QString("Video not found: %1").arg(smallerVideoPath).toUtf8());
    QVERIFY2(QFileInfo::exists(biggerVideoPath), QString("Video not found: %1").arg(biggerVideoPath).toUtf8());

    QTemporaryDir testVideosDir;
    QVERIFY2(testVideosDir.isValid(), "Could not create temporary video folder");
    QTemporaryDir trashDir;
    QVERIFY2(trashDir.isValid(), "Could not create temporary trash folder");

    const QString testSmallerVideoPath = testVideosDir.filePath(smallerVideoName);
    const QString testBiggerVideoPath = testVideosDir.filePath(biggerVideoName);
    QVERIFY2(QFile::copy(smallerVideoPath, testSmallerVideoPath),
             QString("Could not copy %1 to %2").arg(smallerVideoPath, testSmallerVideoPath).toUtf8());
    QVERIFY2(QFile::copy(biggerVideoPath, testBiggerVideoPath),
             QString("Could not copy %1 to %2").arg(biggerVideoPath, testBiggerVideoPath).toUtf8());

    Prefs prefs;
    Db::emptyAllDb(prefs);

    qInfo() << "Auto-delete test: constructing main window";
    MainWindow w;
    qInfo() << "Auto-delete test: main window constructed";
    w.show();
    w._prefs.useCacheOption(Prefs::NO_CACHE);
    w._prefs.delMode = Prefs::CUSTOM_TRASH;
    w._prefs.customTrashFolder(QDir(trashDir.path()));
    w.on_thresholdSlider_valueChanged(100);

    QDir videoDir(testVideosDir.path());
    qInfo() << "Auto-delete test: finding videos";
    w.findVideos(videoDir);
    qInfo() << "Auto-delete test: processing videos";
    w.processVideos();
    qInfo() << "Auto-delete test: videos processed";

    QCOMPARE(w._everyVideo.count(), 2);
    QCOMPARE(w._videoList.count(), 2);

    qInfo() << "Auto-delete test: constructing comparison";
    Comparison comp(w._videoList, w._prefs, w.geometry());
    qInfo() << "Auto-delete test: comparison constructed";
    QCOMPARE(comp.reportMatchingVideos(), 1);
    qInfo() << "Auto-delete test: matching videos reported";
    comp.ui->disableDeleteConfirmationCheckbox->setChecked(true);
    comp.ui->autoOnlySizeDontCheckResFpsCheckbox->setChecked(true);
    QVERIFY(comp.ui->radioButton_onlySizeDiffers_keepBiggest->isChecked());
    if (!keepBiggest)
        comp.ui->radioButton_onlySizeDiffers_keepSmallest->setChecked(true);

    qInfo() << "Auto-delete test: running automatic deletion";
    acceptMessageBoxesDuring(2, [&comp] { comp.on_autoDelOnlySizeDiffersButton_clicked(); });
    qInfo() << "Auto-delete test: automatic deletion finished";

    const QString keptVideoPath = keepBiggest ? testBiggerVideoPath : testSmallerVideoPath;
    const QString movedVideoPath = keepBiggest ? testSmallerVideoPath : testBiggerVideoPath;
    const QString keptVideoName = keepBiggest ? biggerVideoName : smallerVideoName;
    const QString movedVideoName = keepBiggest ? smallerVideoName : biggerVideoName;

    QVERIFY2(QFileInfo::exists(keptVideoPath), QString("Expected video to be kept: %1").arg(keptVideoPath).toUtf8());
    QVERIFY2(!QFileInfo::exists(movedVideoPath),
             QString("Expected video to be moved: %1").arg(movedVideoPath).toUtf8());
    QVERIFY2(QFileInfo::exists(trashDir.filePath(movedVideoName)),
             QString("Expected video in trash folder: %1").arg(trashDir.filePath(movedVideoName)).toUtf8());
    QVERIFY2(!QFileInfo::exists(trashDir.filePath(keptVideoName)),
             QString("Did not expect video in trash folder: %1").arg(trashDir.filePath(keptVideoName)).toUtf8());

    w.close();
    QCoreApplication::processEvents();
    qInfo() << "Auto-delete test: assertions complete";
}

QTEST_MAIN(TestAutoDelete)

#include "tst_auto_delete.moc"
