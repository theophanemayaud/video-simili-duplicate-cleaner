#include "videodiscovery.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

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

// extensions.ini holds plain extension globs that MainWindow::loadExtensions normalizes to "*.ext", so a suffix set
// carries the same meaning as handing those globs to QDirIterator. Matching them here instead is what lets the walk
// observe cancellation on every entry: QDirIterator applies name filters inside hasNext(), so a folder holding many
// non-video files would otherwise be skipped past inside one call that no Stop request can interrupt. It also replaces
// one wildcard match per filter per file, over forty of them, with a single hash lookup, and makes matching
// case-insensitive everywhere rather than only where the filesystem happens to be.
QSet<QString> videoSuffixesFrom(const QStringList& extensionFilters)
{
    QSet<QString> suffixes;
    for (const QString& filter : extensionFilters) {
        const QString suffix = filter.mid(filter.lastIndexOf(u'.') + 1).toLower();
        // An empty suffix would match every extensionless file, so a malformed filter drops out instead.
        if (!suffix.isEmpty())
            suffixes.insert(suffix);
    }
    return suffixes;
}
} // namespace

QStringList loadVideoExtensionFilters()
{
    QFile file(QStringLiteral(":/extensions.ini"));
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QStringList filters;
    QTextStream text(&file);
    while (!text.atEnd()) {
        QString line = text.readLine();
        if (line.startsWith(u';') || line.isEmpty())
            continue;
        filters.append(line.replace(QRegularExpression(QStringLiteral("\\*?\\.")), QStringLiteral("*."))
                           .split(u' ', Qt::SkipEmptyParts));
    }
    return filters;
}

VideoDiscoveryResult discoverVideos(const QStringList& directories, const QStringList& extensionFilters,
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

    const QSet<QString> videoSuffixes = videoSuffixesFrom(extensionFilters);

    while (!remainingDirectories.isEmpty()) {
        const QString folder = remainingDirectories.takeLast();
        // Reported before the folder is opened so an empty or unreadable folder still answers a Stop request.
        if (reportProgress && !reportProgress(result.videos.size(), folder)) {
            result.cancelled = true;
            return result;
        }

        // One pass per folder. Subdirectories is omitted so a .photoslibrary can be replaced by originals/ instead
        // of walking resources/. Symlinked folders are left alone, which also prevents recursion cycles.
        // Hidden entries are included, matching master’s default QDir::AllEntries walk, so hidden videos are found and
        // Stop is observable on hidden non-video entries too.
        QDirIterator iter(folder, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
        while (iter.hasNext()) {
            const QFileInfo entry = iter.nextFileInfo();
            const bool isDirectory = entry.isDir();
            const bool isVideo = !isDirectory && videoSuffixes.contains(entry.suffix().toLower());

            QString reportedPath = folder;
            if (isVideo) {
                reportedPath = entry.canonicalFilePath();
                result.videos.insert(reportedPath);
            }
            else if (isDirectory && !entry.isSymLink())
                remainingDirectories.append(directoriesToScanFor(QDir(entry.filePath())));

            // Every entry is reported, videos and skipped files alike, so a folder full of non-video files cannot
            // hold off a Stop request until its enumeration ends.
            if (reportProgress && !reportProgress(result.videos.size(), reportedPath)) {
                result.cancelled = true;
                return result;
            }
        }
    }

    return result;
}
