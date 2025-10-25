#include <QtTest>
#include <QCoreApplication>
#include <QDir>

#include "../../app/video.h"
#include "../../app/prefs.h"
#include "../../app/db.h"
#include "video_simplified_test_helpers.h"

// Standalone program to generate reference data
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Setup paths relative to project root
    QString projectRoot = QDir::currentPath();
    while (!QFileInfo::exists(projectRoot + "/samples/videos") && projectRoot != "/") {
        projectRoot = QDir(projectRoot).absolutePath();
        QDir dir(projectRoot);
        if (!dir.cdUp()) break;
        projectRoot = dir.absolutePath();
    }
    
    QString samplesDir = projectRoot + "/samples/videos";
    QString referenceDir = projectRoot + "/QtProject/tests/test_video_simplified/reference_data";
    
    qDebug() << "Project root:" << projectRoot;
    qDebug() << "Samples directory:" << samplesDir;
    qDebug() << "Reference directory:" << referenceDir;
    
    if (!QFileInfo::exists(samplesDir)) {
        qCritical() << "Samples directory not found:" << samplesDir;
        return 1;
    }
    
    if (!QFileInfo::exists(referenceDir)) {
        qCritical() << "Reference directory not found:" << referenceDir;
        return 1;
    }
    
    // Run with NO_CACHE to generate baseline reference data
    Prefs prefs;
    prefs.useCacheOption(Prefs::NO_CACHE);
    
    QStringList videos = {"Nice_383p_500kbps.mp4", "Nice_720p_1000kbps.mp4"};
    
    for (const QString& video : videos) {
        qDebug() << "\nGenerating reference data for:" << video;
        
        QString videoPath = samplesDir + "/" + video;
        if (!QFileInfo::exists(videoPath)) {
            qCritical() << "Video not found:" << videoPath;
            continue;
        }
        
        VideoParam param = SimplifiedTestHelpers::scanVideoMetadata(videoPath, prefs);
        if (param.thumbnail.isEmpty()) {
            qCritical() << "Failed to process video:" << videoPath;
            continue;
        }
        
        QString metadataPath = referenceDir + "/" + video + ".txt";
        QString thumbPath = referenceDir + "/" + video + ".jpg";
        
        // Remove existing files if they exist
        if (QFileInfo::exists(metadataPath)) {
            qDebug() << "Removing existing metadata file:" << metadataPath;
            QFile::remove(metadataPath);
        }
        if (QFileInfo::exists(thumbPath)) {
            qDebug() << "Removing existing thumbnail:" << thumbPath;
            QFile::remove(thumbPath);
        }
        
        if (!SimplifiedTestHelpers::saveMetadataToFile(param, metadataPath, QDir(samplesDir))) {
            qCritical() << "Failed to save metadata:" << metadataPath;
            continue;
        }
        
        if (!SimplifiedTestHelpers::saveThumbnail(param.thumbnail, thumbPath)) {
            qCritical() << "Failed to save thumbnail:" << thumbPath;
            continue;
        }
        
        qDebug() << "Successfully generated reference data for:" << video;
        qDebug() << "  Metadata:" << metadataPath;
        qDebug() << "  Thumbnail:" << thumbPath;
        qDebug() << "  Video size:" << param.size << "bytes";
        qDebug() << "  Duration:" << param.duration << "ms";
        qDebug() << "  Dimensions:" << param.width << "x" << param.height;
        qDebug() << "  Codec:" << param.codec;
        qDebug() << "  Framerate:" << param.framerate;
    }
    
    qDebug() << "\nReference data generation complete!";
    return 0;
}

