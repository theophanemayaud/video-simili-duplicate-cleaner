#include "videodiscovery.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace
{
// Photos keeps a derivative and a thumbnail for every asset, so a real library holds hundreds of thousands of files
// under resources/ while only originals/ can hold a video worth reviewing: Video::internalProcess rejects anything
// outside originals/ as a derivative. Enumerating the rest froze the UI for around 25s on a 440k entry library and
// every one of those entries was then discarded, so the whole library is replaced by its originals/ folder.
// A library without originals/ holds nothing we would accept, hence an empty result rather than a full walk.
QStringList directoriesToScanFor(const QDir& dir)
{
    if (!dir.dirName().endsWith(QStringLiteral(".photoslibrary")))
        return {dir.path()};

    const QString originals = dir.filePath(QStringLiteral("originals"));
    if (!QFileInfo(originals).isDir())
        return {};
    return {originals};
}
} // namespace

VideoDiscoveryResult discoverVideos(const QStringList& directories, const QStringList& nameFilters,
                                    const VideoDiscoveryProgress& reportProgress)
{
    VideoDiscoveryResult result;
    QStringList remainingDirectories;

    for (QString directoryPath : directories) {
        directoryPath.remove(QStringLiteral("\""));
        if (directoryPath.trimmed().isEmpty())
            continue;
        const QDir directory(directoryPath);
        if (!directory.exists()) {
            result.missingDirectories.append(QDir::toNativeSeparators(directory.path()));
            continue;
        }
        remainingDirectories.append(directoriesToScanFor(directory));
    }

    QDir directory;
    directory.setNameFilters(nameFilters);
    // Sorting each folder would only cost time: results go into a set and processVideos sorts the final list.
    directory.setSorting(QDir::Unsorted);
    // AllDirs lets video globs still see every folder; Files keeps the globs on video names.
    directory.setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    while (!remainingDirectories.isEmpty()) {
        directory.setPath(remainingDirectories.takeLast());
        if (reportProgress && !reportProgress(result.videos.size(), directory.path())) {
            result.cancelled = true;
            return result;
        }

        // One pass per folder. Subdirectories is omitted so a .photoslibrary can be replaced by originals/ instead
        // of walking resources/. Symlinked folders are left alone, which also prevents recursion cycles.
        QDirIterator iter(directory);
        while (iter.hasNext()) {
            const QFileInfo entry = iter.nextFileInfo();
            if (entry.isDir()) {
                if (!entry.isSymLink())
                    remainingDirectories.append(directoriesToScanFor(QDir(entry.filePath())));
                continue;
            }

            const QString filePathName = entry.canonicalFilePath();
            result.videos.insert(filePathName);
            if (reportProgress && !reportProgress(result.videos.size(), filePathName)) {
                result.cancelled = true;
                return result;
            }
        }
    }

    return result;
}
