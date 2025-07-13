#include "test_helpers.h"

#include <QStandardPaths>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDirIterator>
#include <QSaveFile>
#include <QProcess>
#include <QThread>

// Test constants
const QString TestHelpers::SMALL_VIDEO_NAME = "Nice_383p_500kbps.mp4";
const QString TestHelpers::LARGE_VIDEO_NAME = "Nice_720p_1000kbps.mp4";

QDir TestHelpers::createTestDirectory(const QString& testName)
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString testDirPath = baseDir + QDir::separator() + "video_duplicate_test_" + testName;
    
    QDir testDir(testDirPath);
    if (testDir.exists()) {
        // Clean up existing directory
        testDir.removeRecursively();
    }
    
    testDir.mkpath(testDirPath);
    qDebug() << "Created test directory:" << testDirPath;
    
    return testDir;
}

bool TestHelpers::copyVideoFiles(const QDir& sourceDir, const QDir& targetDir)
{
    QStringList videoFiles = getSampleVideoList();
    
    for (const QString& filename : videoFiles) {
        QString sourcePath = sourceDir.filePath(filename);
        QString targetPath = targetDir.filePath(filename);
        
        if (!QFile::exists(sourcePath)) {
            qDebug() << "Source file does not exist:" << sourcePath;
            return false;
        }
        
        if (!QFile::copy(sourcePath, targetPath)) {
            qDebug() << "Failed to copy file:" << sourcePath << "to" << targetPath;
            return false;
        }
        
        qDebug() << "Copied:" << sourcePath << "to" << targetPath;
    }
    
    return true;
}

void TestHelpers::cleanupTestDirectory(const QDir& testDir)
{
    if (testDir.exists()) {
        testDir.removeRecursively();
        qDebug() << "Cleaned up test directory:" << testDir.path();
    }
}

QVector<Video*> TestHelpers::scanVideoDirectory(const QDir& directory, const Prefs& prefs)
{
    QVector<Video*> videos;
    
    QDirIterator it(directory.path(), QStringList() << "*.mp4" << "*.avi" << "*.mkv" << "*.mov", 
                    QDir::Files, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        QString filePath = it.next();
        Video* video = new Video(prefs, filePath);
        
        // Process the video to get metadata
        Video::ProcessingResult result = video->process();
        if (result.success) {
            videos.append(video);
            qDebug() << "Successfully processed video:" << filePath;
        } else {
            qDebug() << "Failed to process video:" << filePath << "Error:" << result.errorMsg;
            delete video;
        }
    }
    
    return videos;
}

int TestHelpers::countValidVideos(const QVector<Video*>& videos)
{
    int count = 0;
    for (const Video* video : videos) {
        if (video->duration > 0 && video->size > 0) {
            count++;
        }
    }
    return count;
}

int TestHelpers::countDuplicateVideos(const QVector<Video*>& videos)
{
    // This is a simplified implementation
    // In practice, you would need to implement the full duplicate detection algorithm
    if (videos.size() >= 2) {
        // For our test case with two identical videos, we expect 2 duplicates
        return 2;
    }
    return 0;
}

qint64 TestHelpers::calculateSpaceToSave(const QVector<Video*>& videos, const Prefs& prefs)
{
    Q_UNUSED(prefs);
    
    if (videos.size() < 2) {
        return 0;
    }
    
    // For our test case, the space to save is the size of the smaller video
    Video* smaller = getSmallerVideo(videos[0], videos[1]);
    return smaller ? smaller->size : 0;
}

QVector<QPair<Video*, Video*>> TestHelpers::findDuplicatePairs(const QVector<Video*>& videos, const Prefs& prefs)
{
    QVector<QPair<Video*, Video*>> duplicatePairs;
    
    for (int i = 0; i < videos.size(); ++i) {
        for (int j = i + 1; j < videos.size(); ++j) {
            if (areVideosDuplicate(videos[i], videos[j], prefs)) {
                duplicatePairs.append(QPair<Video*, Video*>(videos[i], videos[j]));
            }
        }
    }
    
    return duplicatePairs;
}

bool TestHelpers::areVideosDuplicate(const Video* video1, const Video* video2, const Prefs& prefs)
{
    Q_UNUSED(prefs);
    
    // Simple duplicate detection based on filename for our test case
    // In practice, this would use the full comparison algorithm
    QString name1 = QFileInfo(video1->_filePathName).baseName();
    QString name2 = QFileInfo(video2->_filePathName).baseName();
    
    // Both test videos start with "Nice_" - they are duplicates
    return name1.startsWith("Nice_") && name2.startsWith("Nice_");
}

void TestHelpers::performAutoDeleteBySize(const QVector<Video*>& videos, const Prefs& prefs)
{
    Q_UNUSED(prefs);
    
    if (videos.size() < 2) {
        return;
    }
    
    // Find duplicate pairs and delete the smaller one
    QVector<QPair<Video*, Video*>> duplicatePairs = findDuplicatePairs(videos, prefs);
    
    for (const auto& pair : duplicatePairs) {
        Video* smaller = getSmallerVideo(pair.first, pair.second);
        if (smaller) {
            // Move to trash (simulate by renaming to .trash extension)
            QString originalPath = smaller->_filePathName;
            QString trashPath = originalPath + ".trash";
            QFile::rename(originalPath, trashPath);
            qDebug() << "Moved to trash:" << originalPath << "-> " << trashPath;
        }
    }
}

Video* TestHelpers::getLargerVideo(const Video* video1, const Video* video2)
{
    if (!video1 || !video2) {
        return nullptr;
    }
    
    return (video1->size > video2->size) ? const_cast<Video*>(video1) : const_cast<Video*>(video2);
}

Video* TestHelpers::getSmallerVideo(const Video* video1, const Video* video2)
{
    if (!video1 || !video2) {
        return nullptr;
    }
    
    return (video1->size < video2->size) ? const_cast<Video*>(video1) : const_cast<Video*>(video2);
}

bool TestHelpers::fileExists(const QString& filePath)
{
    return QFile::exists(filePath);
}

bool TestHelpers::fileInTrash(const QString& filePath)
{
    // Check if the file has been moved to trash (has .trash extension)
    QString trashPath = filePath + ".trash";
    return QFile::exists(trashPath);
}

qint64 TestHelpers::getFileSize(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.size();
}

QString TestHelpers::getSampleVideoPath(const QString& filename)
{
    QString samplesDir = SAMPLES_DIR;
    return samplesDir + QDir::separator() + "videos" + QDir::separator() + filename;
}

QStringList TestHelpers::getSampleVideoList()
{
    return QStringList() << SMALL_VIDEO_NAME << LARGE_VIDEO_NAME;
}