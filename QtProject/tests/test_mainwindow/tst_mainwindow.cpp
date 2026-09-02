#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "../../app/mainwindow.h"
#include "../../app/videodiscovery.h"

// Discovery only looks at file names, so empty placeholder files describe a Photos library layout faithfully
// enough here and keep the fixture out of the repository.
class TestMainWindow : public QObject
{
    Q_OBJECT

  private slots:
    void init();
    void test_photosLibraryInsideScannedFolderOnlyYieldsOriginals();
    void test_photosLibrarySelectedDirectlyOnlyYieldsOriginals();
    void test_discoveryCanBeCancelled();
    void test_discoveryCanBeCancelledWhileSkippingNonVideos();
    void test_videoExtensionsMatchRegardlessOfCase();
    void test_hiddenEntriesAreDiscovered();
    void test_loadVideoExtensionFilters();
    void test_emptyFolderScanLeavesNoSearchingMessage();

  private:
    std::unique_ptr<QTemporaryDir> _scanRoot;

    QString libraryPath() const;
    QString createFile(const QString& relativePath) const;
    QSet<QString> discoveredVideosIn(const QString& directoryPath) const;
};

void TestMainWindow::init()
{
    qSetMessagePattern("%{file}(%{line}) %{function}: %{message}");
    // A fresh tree per test function, so files staged by one case cannot show up in another.
    _scanRoot = std::make_unique<QTemporaryDir>();
    QVERIFY(_scanRoot->isValid());
}

QString TestMainWindow::libraryPath() const
{
    return QDir(_scanRoot->path()).filePath(QStringLiteral("Photos Library.photoslibrary"));
}

QString TestMainWindow::createFile(const QString& relativePath) const
{
    const QString absolutePath = QDir(_scanRoot->path()).filePath(relativePath);
    if (!QDir().mkpath(QFileInfo(absolutePath).absolutePath()))
        return {};
    QFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.close();
    return QFileInfo(absolutePath).canonicalFilePath();
}

QSet<QString> TestMainWindow::discoveredVideosIn(const QString& directoryPath) const
{
    return discoverVideos({directoryPath}, {QStringLiteral("*.mp4"), QStringLiteral("*.mov")}).videos;
}

void TestMainWindow::test_photosLibraryInsideScannedFolderOnlyYieldsOriginals()
{
    const QString originalVideo = createFile(QStringLiteral("Photos Library.photoslibrary/originals/4/kept.mov"));
    const QString plainVideo = createFile(QStringLiteral("Movies/holiday.mp4"));
    QVERIFY(!originalVideo.isEmpty());
    QVERIFY(!plainVideo.isEmpty());
    // resources/ is where a real library keeps its hundreds of thousands of derivatives and thumbnails.
    QVERIFY(
        !createFile(QStringLiteral("Photos Library.photoslibrary/resources/derivatives/4/kept_1_105_c.mov")).isEmpty());
    QVERIFY(!createFile(QStringLiteral("Photos Library.photoslibrary/private/com.apple.Photos/cached.mp4")).isEmpty());
    // A library with no originals/ holds nothing the scan would accept, so none of it should be walked either.
    QVERIFY(!createFile(QStringLiteral("Empty Library.photoslibrary/resources/derivatives/orphan.mov")).isEmpty());

    QCOMPARE(discoveredVideosIn(_scanRoot->path()), QSet<QString>({originalVideo, plainVideo}));
}

void TestMainWindow::test_photosLibrarySelectedDirectlyOnlyYieldsOriginals()
{
    // The Apple Photos browse button puts the .photoslibrary bundle itself in the scan list.
    const QString originalVideo = createFile(QStringLiteral("Photos Library.photoslibrary/originals/1/kept.mp4"));
    QVERIFY(!originalVideo.isEmpty());
    QVERIFY(!createFile(QStringLiteral("Photos Library.photoslibrary/resources/renders/kept_rendered.mp4")).isEmpty());

    QCOMPARE(discoveredVideosIn(libraryPath()), QSet<QString>({originalVideo}));
}

void TestMainWindow::test_discoveryCanBeCancelled()
{
    const QString firstVideo = createFile(QStringLiteral("Movies/first.mp4"));
    QVERIFY(!firstVideo.isEmpty());
    QVERIFY(!createFile(QStringLiteral("Movies/second.mp4")).isEmpty());

    const VideoDiscoveryResult result =
        discoverVideos({_scanRoot->path()}, {QStringLiteral("*.mp4")}, [](int, const QString&) { return false; });

    QVERIFY(result.cancelled);
    QVERIFY(result.videos.isEmpty());
}

// A folder can hold far more non-video files than videos, and skipping those must not delay a Stop request: name
// filters handed to QDirIterator would be applied inside hasNext(), hiding the whole enumeration behind one call.
void TestMainWindow::test_discoveryCanBeCancelledWhileSkippingNonVideos()
{
    QVERIFY(!createFile(QStringLiteral("Movies/first.mp4")).isEmpty());
    for (int index = 0; index < 50; index++)
        QVERIFY(!createFile(QStringLiteral("Movies/note%1.txt").arg(index)).isEmpty());

    int reportedEntries = 0;
    const VideoDiscoveryResult result =
        discoverVideos({_scanRoot->path()}, {QStringLiteral("*.mp4")}, [&reportedEntries](int, const QString&) {
            reportedEntries++;
            // Cancel partway through the folder, once enumeration is under way but well before the last entry.
            return reportedEntries < 10;
        });

    QVERIFY(result.cancelled);
    QCOMPARE(reportedEntries, 10);
}

void TestMainWindow::test_videoExtensionsMatchRegardlessOfCase()
{
    const QString shouting = createFile(QStringLiteral("Movies/HOLIDAY.MP4"));
    const QString mixed = createFile(QStringLiteral("Movies/Trip.Mov"));
    QVERIFY(!shouting.isEmpty());
    QVERIFY(!mixed.isEmpty());
    QVERIFY(!createFile(QStringLiteral("Movies/notes.TXT")).isEmpty());

    QCOMPARE(discoveredVideosIn(_scanRoot->path()), QSet<QString>({shouting, mixed}));
}

void TestMainWindow::test_hiddenEntriesAreDiscovered()
{
    const QString inHiddenDir = createFile(QStringLiteral(".private/cache/video.mp4"));
    const QString hiddenFile = createFile(QStringLiteral("Movies/.hidden.mov"));
    const QString normal = createFile(QStringLiteral("Movies/normal.mp4"));
    QVERIFY(!inHiddenDir.isEmpty());
    QVERIFY(!hiddenFile.isEmpty());
    QVERIFY(!normal.isEmpty());

    QCOMPARE(discoveredVideosIn(_scanRoot->path()), QSet<QString>({inHiddenDir, hiddenFile, normal}));
}

void TestMainWindow::test_loadVideoExtensionFilters()
{
    const QStringList filters = loadVideoExtensionFilters();
    QVERIFY(filters.contains(QStringLiteral("*.mp4")));
    QVERIFY(filters.contains(QStringLiteral("*.mov")));
    QVERIFY(!filters.contains(QString()));
}

// Discovery runs on a worker thread, so the window has to settle back to an idle state on its own. A folder with no
// videos is the path that skips processing entirely, which is exactly where a leftover progress message would show up.
void TestMainWindow::test_emptyFolderScanLeavesNoSearchingMessage()
{
    QVERIFY(!createFile(QStringLiteral("Documents/notes.txt")).isEmpty());

    MainWindow window;
    window.show();
    auto* noCache = window.findChild<QRadioButton*>(QStringLiteral("radio_UseCacheNo"));
    auto* directoryBox = window.findChild<QLineEdit*>(QStringLiteral("directoryBox"));
    auto* findDuplicates = window.findChild<QPushButton*>(QStringLiteral("findDuplicates"));
    auto* statusBar = window.findChild<QStatusBar*>(QStringLiteral("statusBar"));
    QVERIFY(noCache);
    QVERIFY(directoryBox);
    QVERIFY(findDuplicates);
    QVERIFY(statusBar);

    noCache->click();
    directoryBox->setText(QDir(_scanRoot->path()).filePath(QStringLiteral("Documents")));
    findDuplicates->click();
    QTRY_COMPARE_WITH_TIMEOUT(findDuplicates->text(), QStringLiteral("Find duplicates"), 30000);

    QCOMPARE(statusBar->currentMessage(), QString());
    QVERIFY(directoryBox->isEnabled());

    window.close();
    QCoreApplication::processEvents();
}

QTEST_MAIN(TestMainWindow)

#include "tst_mainwindow.moc"
