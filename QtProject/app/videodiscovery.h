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

// extensionFilters holds the "*.ext" globs from extensions.ini; only the extension part is significant and it is
// matched case-insensitively. reportProgress is called for every entry seen and stops the walk when it returns false.
VideoDiscoveryResult discoverVideos(const QStringList& directories, const QStringList& extensionFilters,
                                    const VideoDiscoveryProgress& reportProgress = {});

#endif // VIDEODISCOVERY_H
