#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>

#include "test_helpers.h"
#include "video.h"
#include "prefs.h"
#include "db.h"

class VideoEndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Qt application if not already done
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {const_cast<char*>("test"), nullptr};
            app = new QCoreApplication(argc, argv);
        }
        
        // Set up preferences
        prefs.setVerbose(true);
        prefs.comparisonMode(Prefs::_PHASH);
        prefs.setMatchSimilarityThreshold(95);
        
        // Initialize database
        Db::initDbAndCacheLocation(prefs);
        Db::emptyAllDb(prefs);
        
        // Create test directory
        testDir = TestHelpers::createTestDirectory("basic_test");
        
        // Copy sample videos to test directory
        QDir samplesDir(SAMPLES_DIR "/videos");
        ASSERT_TRUE(samplesDir.exists()) << "Samples directory does not exist: " << SAMPLES_DIR "/videos";
        ASSERT_TRUE(TestHelpers::copyVideoFiles(samplesDir, testDir)) << "Failed to copy sample videos";
        
        qDebug() << "Test setup completed. Test directory:" << testDir.path();
    }
    
    void TearDown() override {
        // Clean up videos
        for (Video* video : videos) {
            delete video;
        }
        videos.clear();
        
        // Clean up test directory
        TestHelpers::cleanupTestDirectory(testDir);
        
        qDebug() << "Test teardown completed";
    }
    
    QCoreApplication* app = nullptr;
    Prefs prefs;
    QDir testDir;
    QVector<Video*> videos;
};

// Test 1: Basic video scanning and validation
TEST_F(VideoEndToEndTest, BasicVideoScanningTest) {
    qDebug() << "=== Running BasicVideoScanningTest ===";
    
    // Scan the test directory for videos
    videos = TestHelpers::scanVideoDirectory(testDir, prefs);
    
    // Verify we found the expected number of videos
    ASSERT_EQ(videos.size(), 2) << "Expected 2 videos, found " << videos.size();
    
    // Verify both videos are valid
    int validVideos = TestHelpers::countValidVideos(videos);
    EXPECT_EQ(validVideos, 2) << "Expected 2 valid videos, found " << validVideos;
    
    // Verify we have the expected files
    bool foundSmallVideo = false;
    bool foundLargeVideo = false;
    
    for (const Video* video : videos) {
        QString fileName = QFileInfo(video->_filePathName).fileName();
        if (fileName == TestHelpers::SMALL_VIDEO_NAME) {
            foundSmallVideo = true;
            EXPECT_GT(video->size, 0) << "Small video should have positive size";
            EXPECT_GT(video->duration, 0) << "Small video should have positive duration";
        } else if (fileName == TestHelpers::LARGE_VIDEO_NAME) {
            foundLargeVideo = true;
            EXPECT_GT(video->size, 0) << "Large video should have positive size";
            EXPECT_GT(video->duration, 0) << "Large video should have positive duration";
        }
    }
    
    EXPECT_TRUE(foundSmallVideo) << "Small video not found: " << TestHelpers::SMALL_VIDEO_NAME;
    EXPECT_TRUE(foundLargeVideo) << "Large video not found: " << TestHelpers::LARGE_VIDEO_NAME;
    
    qDebug() << "BasicVideoScanningTest completed successfully";
}

// Test 2: Duplicate detection test
TEST_F(VideoEndToEndTest, DuplicateDetectionTest) {
    qDebug() << "=== Running DuplicateDetectionTest ===";
    
    // Scan the test directory for videos
    videos = TestHelpers::scanVideoDirectory(testDir, prefs);
    ASSERT_EQ(videos.size(), 2) << "Expected 2 videos for duplicate detection test";
    
    // Check for duplicate videos
    int duplicateCount = TestHelpers::countDuplicateVideos(videos);
    EXPECT_EQ(duplicateCount, 2) << "Expected 2 duplicate videos, found " << duplicateCount;
    
    // Find duplicate pairs
    QVector<QPair<Video*, Video*>> duplicatePairs = TestHelpers::findDuplicatePairs(videos, prefs);
    EXPECT_EQ(duplicatePairs.size(), 1) << "Expected 1 duplicate pair, found " << duplicatePairs.size();
    
    if (duplicatePairs.size() > 0) {
        Video* video1 = duplicatePairs[0].first;
        Video* video2 = duplicatePairs[0].second;
        
        // Verify they are indeed duplicates
        EXPECT_TRUE(TestHelpers::areVideosDuplicate(video1, video2, prefs)) 
            << "Videos should be detected as duplicates";
        
        // Verify they have different sizes (one is 720p, one is 383p)
        EXPECT_NE(video1->size, video2->size) << "Duplicate videos should have different sizes";
        
        qDebug() << "Found duplicate pair:";
        qDebug() << "  Video 1:" << video1->_filePathName << "Size:" << video1->size;
        qDebug() << "  Video 2:" << video2->_filePathName << "Size:" << video2->size;
    }
    
    qDebug() << "DuplicateDetectionTest completed successfully";
}

// Test 3: Space calculation test
TEST_F(VideoEndToEndTest, SpaceCalculationTest) {
    qDebug() << "=== Running SpaceCalculationTest ===";
    
    // Scan the test directory for videos
    videos = TestHelpers::scanVideoDirectory(testDir, prefs);
    ASSERT_EQ(videos.size(), 2) << "Expected 2 videos for space calculation test";
    
    // Calculate space to be saved
    qint64 spaceToSave = TestHelpers::calculateSpaceToSave(videos, prefs);
    EXPECT_GT(spaceToSave, 0) << "Space to save should be positive";
    
    // The space to save should be the size of the smaller video
    Video* smallerVideo = TestHelpers::getSmallerVideo(videos[0], videos[1]);
    ASSERT_NE(smallerVideo, nullptr) << "Should be able to identify smaller video";
    
    EXPECT_EQ(spaceToSave, smallerVideo->size) 
        << "Space to save should equal the size of the smaller video";
    
    qDebug() << "Space to save:" << spaceToSave << "bytes";
    qDebug() << "Smaller video size:" << smallerVideo->size << "bytes";
    
    qDebug() << "SpaceCalculationTest completed successfully";
}

// Test 4: Auto-deletion by size test
TEST_F(VideoEndToEndTest, AutoDeleteBySizeTest) {
    qDebug() << "=== Running AutoDeleteBySizeTest ===";
    
    // Scan the test directory for videos
    videos = TestHelpers::scanVideoDirectory(testDir, prefs);
    ASSERT_EQ(videos.size(), 2) << "Expected 2 videos for auto-deletion test";
    
    // Identify the larger and smaller videos before deletion
    Video* largerVideo = TestHelpers::getLargerVideo(videos[0], videos[1]);
    Video* smallerVideo = TestHelpers::getSmallerVideo(videos[0], videos[1]);
    
    ASSERT_NE(largerVideo, nullptr) << "Should be able to identify larger video";
    ASSERT_NE(smallerVideo, nullptr) << "Should be able to identify smaller video";
    
    QString largerVideoPath = largerVideo->_filePathName;
    QString smallerVideoPath = smallerVideo->_filePathName;
    
    // Verify the larger video is indeed the 720p version
    EXPECT_TRUE(QFileInfo(largerVideoPath).fileName() == TestHelpers::LARGE_VIDEO_NAME)
        << "Larger video should be " << TestHelpers::LARGE_VIDEO_NAME.toStdString();
    
    // Verify the smaller video is the 383p version
    EXPECT_TRUE(QFileInfo(smallerVideoPath).fileName() == TestHelpers::SMALL_VIDEO_NAME)
        << "Smaller video should be " << TestHelpers::SMALL_VIDEO_NAME.toStdString();
    
    // Verify both files exist before deletion
    EXPECT_TRUE(TestHelpers::fileExists(largerVideoPath)) << "Larger video should exist before deletion";
    EXPECT_TRUE(TestHelpers::fileExists(smallerVideoPath)) << "Smaller video should exist before deletion";
    
    // Perform auto-deletion by size
    TestHelpers::performAutoDeleteBySize(videos, prefs);
    
    // Verify the larger video still exists
    EXPECT_TRUE(TestHelpers::fileExists(largerVideoPath)) 
        << "Larger video (" << TestHelpers::LARGE_VIDEO_NAME.toStdString() << ") should still exist after auto-deletion";
    
    // Verify the smaller video has been moved to trash
    EXPECT_FALSE(TestHelpers::fileExists(smallerVideoPath)) 
        << "Smaller video (" << TestHelpers::SMALL_VIDEO_NAME.toStdString() << ") should be moved to trash";
    
    EXPECT_TRUE(TestHelpers::fileInTrash(smallerVideoPath)) 
        << "Smaller video should be in trash";
    
    qDebug() << "Auto-deletion completed:";
    qDebug() << "  Retained (larger):" << largerVideoPath;
    qDebug() << "  Trashed (smaller):" << smallerVideoPath;
    
    qDebug() << "AutoDeleteBySizeTest completed successfully";
}

// Test 5: Complete end-to-end workflow test
TEST_F(VideoEndToEndTest, CompleteWorkflowTest) {
    qDebug() << "=== Running CompleteWorkflowTest ===";
    
    // Step 1: Scan directory
    videos = TestHelpers::scanVideoDirectory(testDir, prefs);
    ASSERT_EQ(videos.size(), 2) << "Step 1 failed: Expected 2 videos";
    
    // Step 2: Validate videos
    int validVideos = TestHelpers::countValidVideos(videos);
    EXPECT_EQ(validVideos, 2) << "Step 2 failed: Expected 2 valid videos";
    
    // Step 3: Detect duplicates
    int duplicateCount = TestHelpers::countDuplicateVideos(videos);
    EXPECT_EQ(duplicateCount, 2) << "Step 3 failed: Expected 2 duplicate videos";
    
    // Step 4: Calculate space to save
    qint64 spaceToSave = TestHelpers::calculateSpaceToSave(videos, prefs);
    EXPECT_GT(spaceToSave, 0) << "Step 4 failed: Space to save should be positive";
    
    // Step 5: Perform auto-deletion
    QString largerVideoPath = TestHelpers::getLargerVideo(videos[0], videos[1])->_filePathName;
    QString smallerVideoPath = TestHelpers::getSmallerVideo(videos[0], videos[1])->_filePathName;
    
    TestHelpers::performAutoDeleteBySize(videos, prefs);
    
    // Step 6: Verify results
    EXPECT_TRUE(TestHelpers::fileExists(largerVideoPath)) << "Step 6 failed: Larger video should remain";
    EXPECT_TRUE(TestHelpers::fileInTrash(smallerVideoPath)) << "Step 6 failed: Smaller video should be in trash";
    
    qDebug() << "Complete workflow test results:";
    qDebug() << "  Valid videos found:" << validVideos;
    qDebug() << "  Duplicate videos:" << duplicateCount;
    qDebug() << "  Space to save:" << spaceToSave << "bytes";
    qDebug() << "  Larger video retained:" << largerVideoPath;
    qDebug() << "  Smaller video trashed:" << smallerVideoPath;
    
    qDebug() << "CompleteWorkflowTest completed successfully";
}

// Main function for the test executable
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}