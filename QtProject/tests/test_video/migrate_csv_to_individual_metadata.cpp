#include <QtCore>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

#include "../helpers.h"
#include "../test_video_simplified/video_simplified_test_helpers.h"
#include "video_test_helpers.h"

// Standalone program to migrate CSV metadata to individual .txt files
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "====== CSV to Individual Metadata Migration Tool ======";
    qDebug() << "Migrating 100GB test dataset";
    
    // Determine platform and set paths accordingly
#ifdef Q_OS_MACOS
    QDir videoDir("/Volumes/4TBSSD/Video duplicates - just for checking later my video duplicate program still works/Videos/");
    QDir thumbnailDir_nocache("/Volumes/4TBSSD/Video duplicates - just for checking later my video duplicate program still works/Thumbnails-nocache/");
    QFileInfo csvInfo_nocache("/Volumes/4TBSSD/Video duplicates - just for checking later my video duplicate program still works/100GBtests-nocache.csv");
    
    // No cached version for 100GB dataset
    QDir thumbnailDir_cached;
    QFileInfo csvInfo_cached;
#elif defined(Q_OS_WIN)
    // Add Windows paths if needed
    qCritical() << "100GB test paths not configured for Windows";
    return 1;
#endif
    
    qDebug() << "Video directory:" << videoDir.path();
    qDebug() << "CSV (nocache):" << csvInfo_nocache.absoluteFilePath();
    qDebug() << "Thumbnail dir (nocache):" << thumbnailDir_nocache.path();
    
    // Verify paths exist
    if (!videoDir.exists()) {
        qCritical() << "Video directory not found:" << videoDir.path();
        qCritical() << "Migration aborted.";
        return 1;
    }
    
    if (!csvInfo_nocache.exists()) {
        qCritical() << "CSV file not found:" << csvInfo_nocache.absoluteFilePath();
        qCritical() << "Migration aborted.";
        return 1;
    }
    
    if (!thumbnailDir_nocache.exists()) {
        qCritical() << "Thumbnail directory not found:" << thumbnailDir_nocache.path();
        qCritical() << "Migration aborted.";
        return 1;
    }
    
    // Helper lambda to migrate one CSV dataset
    auto migrateCsvData = [&videoDir](const QFileInfo& csvInfo, const QDir& thumbDir, const QString& suffix) -> bool {
        qDebug() << "\n====== Migrating" << suffix << "data ======";
        
        if (!csvInfo.exists()) {
            qWarning() << "CSV file not found:" << csvInfo.absoluteFilePath();
            qWarning() << "Skipping" << suffix << "migration.";
            return false;
        }
        
        // Load CSV data
        QList<VideoParam> videoParamList = TestHelpers::importCSVtoVideoParamQList(csvInfo, videoDir, thumbDir);
        
        if (videoParamList.isEmpty()) {
            qCritical() << "Failed to load CSV data or CSV is empty";
            return false;
        }
        
        qDebug() << "Loaded" << videoParamList.count() << "video entries from CSV";
        
        int successCount = 0;
        int skipCount = 0;
        int errorCount = 0;
        
        // Process each video
        foreach(const VideoParam& param, videoParamList) {
            QString videoPath = param.videoInfo.absoluteFilePath();
            
            if (!param.videoInfo.exists()) {
                qWarning() << "Video file not found:" << videoPath;
                errorCount++;
                continue;
            }
            
            // Metadata file will be video.mp4.nocache.txt or video.mp4.withcache.txt
            QString metadataPath = videoPath + "." + suffix + ".txt";
            
            // Thumbnail will be video.mp4.nocache.jpg or video.mp4.withcache.jpg
            QString thumbPath = videoPath + "." + suffix + ".jpg";
            
            // Check if metadata file already exists
            if (QFileInfo::exists(metadataPath)) {
                qDebug() << "Metadata already exists, skipping:" << metadataPath;
                skipCount++;
                continue;
            }
            
            // Save metadata
            if (!SimplifiedTestHelpers::saveMetadataToFile(param, metadataPath, videoDir)) {
                qWarning() << "Failed to save metadata for:" << videoPath;
                errorCount++;
                continue;
            }
            
            // Load thumbnail from old location
            if (!param.thumbnailInfo.exists()) {
                qWarning() << "Thumbnail file not found:" << param.thumbnailInfo.absoluteFilePath();
                errorCount++;
                continue;
            }
            
            QFile thumbFile(param.thumbnailInfo.absoluteFilePath());
            if (!thumbFile.open(QIODevice::ReadOnly)) {
                qWarning() << "Failed to open thumbnail:" << param.thumbnailInfo.absoluteFilePath();
                errorCount++;
                continue;
            }
            
            QByteArray thumbnail = thumbFile.readAll();
            thumbFile.close();
            
            // Check if thumbnail already exists
            if (QFileInfo::exists(thumbPath)) {
                qDebug() << "Thumbnail already exists, skipping:" << thumbPath;
                // Still count as success since metadata was saved
                successCount++;
                continue;
            }
            
            // Save thumbnail to new location
            QFile newThumbFile(thumbPath);
            if (!newThumbFile.open(QIODevice::WriteOnly)) {
                qWarning() << "Failed to create thumbnail file:" << thumbPath;
                errorCount++;
                continue;
            }
            
            qint64 written = newThumbFile.write(thumbnail);
            newThumbFile.close();
            
            if (written != thumbnail.size()) {
                qWarning() << "Failed to write complete thumbnail data for:" << thumbPath;
                errorCount++;
                continue;
            }
            
            successCount++;
            
            if (successCount % 10 == 0) {
                qDebug() << "Progress:" << successCount << "videos migrated...";
            }
        }
        
        qDebug() << "\n====== Migration Complete for" << suffix << "======";
        qDebug() << "Successfully migrated:" << successCount << "videos";
        qDebug() << "Skipped (already exists):" << skipCount << "videos";
        qDebug() << "Errors:" << errorCount << "videos";
        
        if (errorCount > 0) {
            qWarning() << "\nSome videos failed to migrate. Check the warnings above.";
            return false;
        }
        
        qDebug() << "\nAll" << suffix << "videos successfully migrated!";
        return true;
    };
    
    // Migrate nocache data
    bool nocacheSuccess = migrateCsvData(csvInfo_nocache, thumbnailDir_nocache, "nocache");
    
    qDebug() << "\n====== Overall Migration Summary ======";
    qDebug() << "NO_CACHE migration:" << (nocacheSuccess ? "SUCCESS" : "FAILED/SKIPPED");
    qDebug() << "(No cached version for 100GB dataset)";
    
    if (!nocacheSuccess) {
        qCritical() << "\nMigration failed!";
        return 1;
    }
    
    qDebug() << "\nYou can now use individual .nocache.txt/.withcache.txt metadata files alongside each video.";
    
    return 0;
}

