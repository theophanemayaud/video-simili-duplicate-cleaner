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

// Loads the "*.ext" filters bundled in extensions.ini. Keeping parsing beside discovery gives the app and corpus
// tests one definition of which files are videos.
QStringList loadVideoExtensionFilters();

// extensionFilters holds the "*.ext" globs from extensions.ini; only the extension part is significant and it is
// matched case-insensitively. reportProgress is called for every entry seen and stops the walk when it returns false.
VideoDiscoveryResult discoverVideos(const QStringList& directories, const QStringList& extensionFilters,
                                    const VideoDiscoveryProgress& reportProgress = {});

#endif // VIDEODISCOVERY_H
