#include <gtest/gtest.h>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "mainwindow.h"
#include "comparison.h"
#include "video.h"
#include "prefs.h"
#include "db.h"

class VideoEndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Qt application if not already done
        if (!QApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {const_cast<char*>("test"), nullptr};
            app = new QApplication(argc, argv);
        }
        
        // Create temporary test directory
        testDir = std::make_unique<QTemporaryDir>();
        ASSERT_TRUE(testDir->isValid()) << "Failed to create temporary test directory";
        
        testDirPath = testDir->path();
        qDebug() << "Test directory:" << testDirPath;
        
        // Copy sample videos to test directory
        QDir samplesDir(SAMPLES_DIR "/videos");
        ASSERT_TRUE(samplesDir.exists()) << "Samples directory does not exist: " << SAMPLES_DIR "/videos";
        
        QStringList videoFiles = samplesDir.entryList(QStringList() << "*.mp4", QDir::Files);
        ASSERT_EQ(videoFiles.size(), 2) << "Expected 2 sample videos, found " << videoFiles.size();
        
        for (const QString& videoFile : videoFiles) {
            QString sourcePath = samplesDir.absoluteFilePath(videoFile);
            QString destPath = QDir(testDirPath).absoluteFilePath(videoFile);
            ASSERT_TRUE(QFile::copy(sourcePath, destPath)) 
                << "Failed to copy " << videoFile << " to test directory";
            qDebug() << "Copied" << videoFile << "to test directory";
        }
        
        // Set up preferences for testing
        prefs.setVerbose(true);
        prefs.comparisonMode(Prefs::_PHASH);
        prefs.setMatchSimilarityThreshold(95);
        prefs._mainwPtr = nullptr; // No main window in test
        
        // Initialize database
        Db::initDbAndCacheLocation(prefs);
        Db::emptyAllDb(prefs);
        
        qDebug() << "Test setup completed";
    }
    
    void TearDown() override {
        // Clean up videos
        for (Video* video : videoList) {
            delete video;
        }
        videoList.clear();
        
        qDebug() << "Test teardown completed";
    }
    
    // Helper method to scan videos using actual MainWindow functionality
    void scanVideos() {
        QDir testQDir(testDirPath);
        
        // Use the actual MainWindow scanning logic
        QStringList extensionList = {"*.mp4", "*.avi", "*.mov", "*.mkv"}; // Common video extensions
        testQDir.setNameFilters(extensionList);
        
        QDirIterator iter(testQDir, QDirIterator::Subdirectories);
        QSet<QString> everyVideo;
        
        while (iter.hasNext()) {
            const QString filePathName = iter.nextFileInfo().canonicalFilePath();
            if (!everyVideo.contains(filePathName)) {
                everyVideo.insert(filePathName);
            }
        }
        
        ASSERT_EQ(everyVideo.size(), 2) << "Expected to find 2 videos during scan";
        
        // Process videos like MainWindow does
        for (const QString& videoPath : everyVideo) {
            Video* video = new Video(prefs, videoPath);
            QString error = video->internalProcess(); // This does the actual video processing
            
            if (error.isEmpty() && video->duration > 0) {
                videoList.append(video);
                qDebug() << "Successfully processed video:" << video->_filePathName 
                         << "Size:" << video->size << "Duration:" << video->duration;
            } else {
                qDebug() << "Failed to process video:" << videoPath << "Error:" << error;
                delete video;
            }
        }
        
        ASSERT_EQ(videoList.size(), 2) << "Expected 2 valid processed videos";
    }
    
    // Helper to get video by filename
    Video* getVideoByFilename(const QString& filename) {
        for (Video* video : videoList) {
            if (QFileInfo(video->_filePathName).fileName() == filename) {
                return video;
            }
        }
        return nullptr;
    }
    
    QApplication* app = nullptr;
    std::unique_ptr<QTemporaryDir> testDir;
    QString testDirPath;
    Prefs prefs;
    QVector<Video*> videoList;
    
    // Expected filenames from samples
    const QString LARGE_VIDEO = "Nice_720p_1000kbps.mp4";
    const QString SMALL_VIDEO = "Nice_383p_500kbps.mp4";
};

// Test 1: Basic video scanning
TEST_F(VideoEndToEndTest, VideoScanningTest) {
    qDebug() << "=== Running VideoScanningTest ===";
    
    scanVideos();
    
    // Verify we have both expected videos
    Video* largeVideo = getVideoByFilename(LARGE_VIDEO);
    Video* smallVideo = getVideoByFilename(SMALL_VIDEO);
    
    ASSERT_NE(largeVideo, nullptr) << "Large video not found: " << LARGE_VIDEO.toStdString();
    ASSERT_NE(smallVideo, nullptr) << "Small video not found: " << SMALL_VIDEO.toStdString();
    
    // Verify the large video is indeed larger
    EXPECT_GT(largeVideo->size, smallVideo->size) << "Large video should have bigger file size";
    
    qDebug() << "Large video:" << largeVideo->_filePathName << "Size:" << largeVideo->size;
    qDebug() << "Small video:" << smallVideo->_filePathName << "Size:" << smallVideo->size;
    
    qDebug() << "VideoScanningTest completed successfully";
}

// Test 2: Auto-deletion by size using actual Comparison class
TEST_F(VideoEndToEndTest, AutoDeleteBySizeTest) {
    qDebug() << "=== Running AutoDeleteBySizeTest ===";
    
    scanVideos();
    
    Video* largeVideo = getVideoByFilename(LARGE_VIDEO);
    Video* smallVideo = getVideoByFilename(SMALL_VIDEO);
    
    ASSERT_NE(largeVideo, nullptr) << "Large video not found";
    ASSERT_NE(smallVideo, nullptr) << "Small video not found";
    
    QString largeVideoPath = largeVideo->_filePathName;
    QString smallVideoPath = smallVideo->_filePathName;
    
    // Verify both files exist before deletion
    EXPECT_TRUE(QFile::exists(largeVideoPath)) << "Large video file should exist before test";
    EXPECT_TRUE(QFile::exists(smallVideoPath)) << "Small video file should exist before test";
    
    // Create Comparison object with our video list (like MainWindow does)
    QRect dummyGeometry(100, 100, 800, 600);
    Comparison comparison(videoList, prefs, dummyGeometry);
    
    // Set up comparison for auto-deletion
    comparison.ui->disableDeleteConfirmationCheckbox->setChecked(true); // Disable confirmations for test
    
    // Perform the actual auto-deletion by size using the real method
    comparison.on_autoDelOnlySizeDiffersButton_clicked();
    
    // Verify results: larger video should still exist, smaller should be marked as trashed
    EXPECT_TRUE(QFile::exists(largeVideoPath)) 
        << "Large video (" << LARGE_VIDEO.toStdString() << ") should still exist after auto-deletion";
    
    // Check if the smaller video was marked as trashed (the app marks it as trashed)
    EXPECT_TRUE(smallVideo->trashed) 
        << "Small video (" << SMALL_VIDEO.toStdString() << ") should be marked as trashed";
    
    qDebug() << "Auto-deletion completed:";
    qDebug() << "  Large video exists:" << QFile::exists(largeVideoPath);
    qDebug() << "  Small video trashed:" << smallVideo->trashed;
    
    qDebug() << "AutoDeleteBySizeTest completed successfully";
}

// Test 3: Duplicate detection using actual Comparison methods
TEST_F(VideoEndToEndTest, DuplicateDetectionTest) {
    qDebug() << "=== Running DuplicateDetectionTest ===";
    
    scanVideos();
    
    Video* largeVideo = getVideoByFilename(LARGE_VIDEO);
    Video* smallVideo = getVideoByFilename(SMALL_VIDEO);
    
    ASSERT_NE(largeVideo, nullptr) << "Large video not found";
    ASSERT_NE(smallVideo, nullptr) << "Small video not found";
    
    // Create Comparison object and test duplicate detection
    QRect dummyGeometry(100, 100, 800, 600);
    Comparison comparison(videoList, prefs, dummyGeometry);
    
    // Use the actual duplicate detection method
    bool areMatching = comparison.bothVideosMatch(largeVideo, smallVideo);
    EXPECT_TRUE(areMatching) << "Videos should be detected as duplicates";
    
    // Test the matching video report functionality
    int matchingVideos = comparison.reportMatchingVideos();
    EXPECT_GT(matchingVideos, 0) << "Should find at least one matching video pair";
    
    qDebug() << "Videos are matching:" << areMatching;
    qDebug() << "Total matching videos found:" << matchingVideos;
    
    qDebug() << "DuplicateDetectionTest completed successfully";
}

// Test 4: End-to-end workflow test
TEST_F(VideoEndToEndTest, CompleteWorkflowTest) {
    qDebug() << "=== Running CompleteWorkflowTest ===";
    
    // Step 1: Scan videos
    scanVideos();
    ASSERT_EQ(videoList.size(), 2) << "Step 1 failed: Should have 2 videos";
    
    // Step 2: Create comparison and detect duplicates
    QRect dummyGeometry(100, 100, 800, 600);
    Comparison comparison(videoList, prefs, dummyGeometry);
    
    int matchingVideos = comparison.reportMatchingVideos();
    EXPECT_GT(matchingVideos, 0) << "Step 2 failed: Should find matching videos";
    
    // Step 3: Perform auto-deletion
    Video* largeVideo = getVideoByFilename(LARGE_VIDEO);
    Video* smallVideo = getVideoByFilename(SMALL_VIDEO);
    
    ASSERT_NE(largeVideo, nullptr) << "Large video should exist";
    ASSERT_NE(smallVideo, nullptr) << "Small video should exist";
    
    QString largeVideoPath = largeVideo->_filePathName;
    
    comparison.ui->disableDeleteConfirmationCheckbox->setChecked(true);
    comparison.on_autoDelOnlySizeDiffersButton_clicked();
    
    // Step 4: Verify final state
    EXPECT_TRUE(QFile::exists(largeVideoPath)) << "Step 4 failed: Large video should remain";
    EXPECT_TRUE(smallVideo->trashed) << "Step 4 failed: Small video should be trashed";
    
    qDebug() << "Complete workflow test results:";
    qDebug() << "  Videos scanned:" << videoList.size();
    qDebug() << "  Matching videos:" << matchingVideos;
    qDebug() << "  Large video retained:" << QFile::exists(largeVideoPath);
    qDebug() << "  Small video trashed:" << smallVideo->trashed;
    
    qDebug() << "CompleteWorkflowTest completed successfully";
}

// Main function for the test executable
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}