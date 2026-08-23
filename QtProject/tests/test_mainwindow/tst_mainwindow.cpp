#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "../../app/mainwindow.h"

// Discovery only looks at file names, so empty placeholder files describe a Photos library layout faithfully
// enough here and keep the fixture out of the repository.
class TestMainWindow : public QObject
{
    Q_OBJECT

  private slots:
    void init();
    void test_photosLibraryInsideScannedFolderOnlyYieldsOriginals();
    void test_photosLibrarySelectedDirectlyOnlyYieldsOriginals();

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
    MainWindow window;
    window.loadExtensions();
    QDir directory(directoryPath);
    window.findVideos(directory);
    window.close();
    return window._everyVideo;
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

QTEST_MAIN(TestMainWindow)

#include "tst_mainwindow.moc"
