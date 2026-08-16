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
#include "../../app/ui_mainwindow.h"

class TestAutoDelete : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void test_keepBiggestMovesSmallerVideo();
    void test_keepSmallestMovesBiggerVideo();
    void test_matchingFeaturePairs_data();
    void test_matchingFeaturePairs();
    void test_keepBiggestMovesLetterboxedCopy();

  private:
    QString samplesDirPath() const;
    QString matchingFixturePath(const QString& fileName) const;
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
    prefs.resetSettings();
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

QString TestAutoDelete::matchingFixturePath(const QString& fileName) const
{
    return QDir(samplesDirPath()).filePath(QStringLiteral("matching/") + fileName);
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

void TestAutoDelete::test_matchingFeaturePairs_data()
{
    QTest::addColumn<QString>("left");
    QTest::addColumn<QString>("right");
    QTest::addColumn<bool>("detectRotatedCopies");
    QTest::addColumn<int>("expectedMatches");

    QTest::newRow("letterbox") << QStringLiteral("a-original.mp4") << QStringLiteral("a-letterbox.mp4") << false
                                << 1;
    QTest::newRow("monochrome-cut-ends") << QStringLiteral("a-original.mp4")
                                           << QStringLiteral("a-monochrome-ends.mp4") << false << 1;
    QTest::newRow("physical-rotation-default-off") << QStringLiteral("a-original.mp4")
                                                     << QStringLiteral("a-rot90.mp4") << false << 0;
    QTest::newRow("physical-rotation-enabled") << QStringLiteral("a-original.mp4") << QStringLiteral("a-rot90.mp4")
                                                 << true << 1;
    QTest::newRow("same-pillarbox-different-content") << QStringLiteral("a-pillarbox.mp4")
                                                        << QStringLiteral("b-pillarbox.mp4") << false << 0;
}

// Exercise the regular MainWindow and Comparison path for one pair at a time. The dedicated
// matching test owns the complete pair matrix; keeping this set pair-scoped makes its UI result
// unambiguous and makes failures point to app integration rather than fixture combinatorics.
void TestAutoDelete::test_matchingFeaturePairs()
{
    QFETCH(QString, left);
    QFETCH(QString, right);
    QFETCH(bool, detectRotatedCopies);
    QFETCH(int, expectedMatches);

    const QString leftSource = matchingFixturePath(left);
    const QString rightSource = matchingFixturePath(right);
    QVERIFY2(QFileInfo::exists(leftSource), qPrintable(QStringLiteral("Video not found: %1").arg(leftSource)));
    QVERIFY2(QFileInfo::exists(rightSource), qPrintable(QStringLiteral("Video not found: %1").arg(rightSource)));

    QTemporaryDir testVideosDir;
    QVERIFY2(testVideosDir.isValid(), "Could not create temporary video folder");
    QVERIFY(QFile::copy(leftSource, testVideosDir.filePath(left)));
    QVERIFY(QFile::copy(rightSource, testVideosDir.filePath(right)));

    Prefs prefs;
    Db::emptyAllDb(prefs);

    MainWindow w;
    w.show();
    w._prefs.useCacheOption(Prefs::NO_CACHE);
    w.on_thresholdSlider_valueChanged(90);
    w.ui->detectRotatedCopiesCheckbox->setChecked(detectRotatedCopies);
    QDir videoDir(testVideosDir.path());
    w.findVideos(videoDir);
    w.processVideos();

    QCOMPARE(w._everyVideo.count(), 2);
    QCOMPARE(w._videoList.count(), 2);
    Comparison comp(w._videoList, w._prefs, w.geometry());
    QCOMPARE(comp.reportMatchingVideos(), expectedMatches);

    w.close();
    QCoreApplication::processEvents();
}

void TestAutoDelete::test_keepBiggestMovesLetterboxedCopy()
{
    const QString originalName = QStringLiteral("a-original.mp4");
    const QString letterboxName = QStringLiteral("a-letterbox.mp4");
    const QString originalSource = matchingFixturePath(originalName);
    const QString letterboxSource = matchingFixturePath(letterboxName);
    QVERIFY2(QFileInfo::exists(originalSource), qPrintable(QStringLiteral("Video not found: %1").arg(originalSource)));
    QVERIFY2(QFileInfo::exists(letterboxSource),
             qPrintable(QStringLiteral("Video not found: %1").arg(letterboxSource)));

    QTemporaryDir testVideosDir;
    QVERIFY2(testVideosDir.isValid(), "Could not create temporary video folder");
    QTemporaryDir trashDir;
    QVERIFY2(trashDir.isValid(), "Could not create temporary trash folder");

    const QString originalPath = testVideosDir.filePath(originalName);
    const QString letterboxPath = testVideosDir.filePath(letterboxName);
    QVERIFY(QFile::copy(originalSource, originalPath));
    QVERIFY(QFile::copy(letterboxSource, letterboxPath));

    // The auto-delete-by-size workflow intentionally ignores differences below 100 KiB. Appending
    // inert trailing bytes keeps the fixture's media content unchanged while making this UI rule testable.
    QFile paddedCopy(letterboxPath);
    QVERIFY(paddedCopy.open(QIODevice::Append));
    QCOMPARE(paddedCopy.write(QByteArray(101 * 1024, '\0')), qint64(101 * 1024));
    paddedCopy.close();

    Prefs prefs;
    Db::emptyAllDb(prefs);

    MainWindow w;
    w.show();
    w._prefs.useCacheOption(Prefs::NO_CACHE);
    w._prefs.delMode = Prefs::CUSTOM_TRASH;
    w._prefs.customTrashFolder(QDir(trashDir.path()));
    w.on_thresholdSlider_valueChanged(90);
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
    acceptMessageBoxesDuring(2, [&comp] { comp.on_autoDelOnlySizeDiffersButton_clicked(); });

    QVERIFY2(!QFileInfo::exists(originalPath), qPrintable(QStringLiteral("Expected moved video: %1").arg(originalPath)));
    QVERIFY2(QFileInfo::exists(letterboxPath),
             qPrintable(QStringLiteral("Expected retained video: %1").arg(letterboxPath)));
    QVERIFY2(QFileInfo::exists(trashDir.filePath(originalName)),
             qPrintable(QStringLiteral("Expected video in trash: %1").arg(originalName)));

    w.close();
    QCoreApplication::processEvents();
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
    if (!keepBiggest)
        comp.ui->radioButton_onlySizeDiffers_keepSmallest->setChecked(true);

    acceptMessageBoxesDuring(2, [&comp] { comp.on_autoDelOnlySizeDiffersButton_clicked(); });

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
}

QTEST_MAIN(TestAutoDelete)

#include "tst_auto_delete.moc"
