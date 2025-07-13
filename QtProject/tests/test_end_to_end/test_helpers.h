#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QVector>
#include <QString>
#include <QDebug>

#include "video.h"
#include "prefs.h"
#include "mainwindow.h"
#include "comparison.h"

class TestHelpers 
{
public:
    // Test directory management
    static QDir createTestDirectory(const QString& testName);
    static bool copyVideoFiles(const QDir& sourceDir, const QDir& targetDir);
    static void cleanupTestDirectory(const QDir& testDir);
    
    // Video processing helpers
    static QVector<Video*> scanVideoDirectory(const QDir& directory, const Prefs& prefs);
    static int countValidVideos(const QVector<Video*>& videos);
    static int countDuplicateVideos(const QVector<Video*>& videos);
    static qint64 calculateSpaceToSave(const QVector<Video*>& videos, const Prefs& prefs);
    
    // Duplicate detection helpers
    static QVector<QPair<Video*, Video*>> findDuplicatePairs(const QVector<Video*>& videos, const Prefs& prefs);
    static bool areVideosDuplicate(const Video* video1, const Video* video2, const Prefs& prefs);
    
    // Auto-deletion helpers
    static void performAutoDeleteBySize(const QVector<Video*>& videos, const Prefs& prefs);
    static Video* getLargerVideo(const Video* video1, const Video* video2);
    static Video* getSmallerVideo(const Video* video1, const Video* video2);
    
    // File system helpers
    static bool fileExists(const QString& filePath);
    static bool fileInTrash(const QString& filePath);
    static qint64 getFileSize(const QString& filePath);
    
    // Sample video paths
    static QString getSampleVideoPath(const QString& filename);
    static QStringList getSampleVideoList();
    
    // Test constants
    static const QString SMALL_VIDEO_NAME;
    static const QString LARGE_VIDEO_NAME;
    
private:
    // Private constructor - this is a utility class
    TestHelpers() = delete;
};

#endif // TEST_HELPERS_H