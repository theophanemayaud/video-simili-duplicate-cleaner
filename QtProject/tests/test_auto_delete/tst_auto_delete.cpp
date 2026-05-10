#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include "../../app/comparison.h"
#include "../../app/db.h"
#include "../../app/mainwindow.h"
#include "../../app/prefs.h"
#include "../../app/ui_comparison.h"

class TestAutoDelete : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void test_keepBiggestMovesSmallerVideo();

  private:
    QString samplesDirPath() const;
    void acceptNextMessageBoxes(const int messageBoxCount) const;
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

void TestAutoDelete::acceptNextMessageBoxes(const int messageBoxCount) const
{
    auto* timer = new QTimer(QApplication::instance());
    auto* acceptedMessageBoxes = new int(0);
    timer->setInterval(10);

    QObject::connect(timer, &QTimer::timeout, [timer, acceptedMessageBoxes, messageBoxCount] {
        auto* messageBox = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (messageBox == nullptr)
            return;

        if (auto* yesButton = messageBox->button(QMessageBox::Yes))
            yesButton->click();
        else
            messageBox->accept();

        ++(*acceptedMessageBoxes);
        if (*acceptedMessageBoxes >= messageBoxCount) {
            timer->stop();
            timer->deleteLater();
            delete acceptedMessageBoxes;
        }
    });

    timer->start();
}

// Tests auto deletion by file size ignoring duration and fps
// Copies the two sample videos, and runs auto deletion with the biggest video kept
// And a custom trash folder set
// Then verifies the smaller video is moved to trash and the bigger video is kept
void TestAutoDelete::test_keepBiggestMovesSmallerVideo()
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

    MainWindow w;
    w.show();
    w._prefs.useCacheOption(Prefs::NO_CACHE);
    w._prefs.delMode = Prefs::CUSTOM_TRASH;
    w._prefs.customTrashFolder(QDir(trashDir.path()));
    w.on_thresholdSlider_valueChanged(100);

    QDir videoDir(testVideosDir.path());
    w.findVideos(videoDir);
    w.processVideos();

    QCOMPARE(w._everyVideo.count(), 2);
    QCOMPARE(w._videoList.count(), 2);

    Comparison comp(w._videoList, w._prefs, w.geometry());
    QCOMPARE(comp.reportMatchingVideos(), 1);
    comp.ui->disableDeleteConfirmationCheckbox->setChecked(true);
    comp.ui->autoOnlySizeDontCheckResFpsCheckbox->setChecked(true);
    QVERIFY(comp.ui->radioButton_onlySizeDiffers_keepBiggest->isChecked());

    acceptNextMessageBoxes(2);
    comp.on_autoDelOnlySizeDiffersButton_clicked();

    QVERIFY2(QFileInfo::exists(testBiggerVideoPath),
             QString("Expected bigger video to be kept: %1").arg(testBiggerVideoPath).toUtf8());
    QVERIFY2(!QFileInfo::exists(testSmallerVideoPath),
             QString("Expected smaller video to be moved: %1").arg(testSmallerVideoPath).toUtf8());
    QVERIFY2(QFileInfo::exists(trashDir.filePath(smallerVideoName)),
             QString("Expected smaller video in trash folder: %1").arg(trashDir.filePath(smallerVideoName)).toUtf8());
    QVERIFY2(
        !QFileInfo::exists(trashDir.filePath(biggerVideoName)),
        QString("Did not expect bigger video in trash folder: %1").arg(trashDir.filePath(biggerVideoName)).toUtf8());

    w.close();
    QCoreApplication::processEvents();
}

QTEST_MAIN(TestAutoDelete)

#include "tst_auto_delete.moc"
