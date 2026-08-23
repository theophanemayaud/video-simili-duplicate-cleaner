#ifndef VIDEODISCOVERY_H
#define VIDEODISCOVERY_H

#include <QSet>
#include <QStringList>

#include <functional>

struct VideoDiscoveryResult {
    QSet<QString> videos;
    QStringList missingDirectories;
    bool cancelled = false;
};

using VideoDiscoveryProgress = std::function<bool(int videoCount, const QString& path)>;

VideoDiscoveryResult discoverVideos(const QStringList& directories, const QStringList& nameFilters,
                                    const VideoDiscoveryProgress& reportProgress = {});

#endif // VIDEODISCOVERY_H
