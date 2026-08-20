#ifndef EXTERNAL_FIXTURE_PATHS_H
#define EXTERNAL_FIXTURE_PATHS_H

#include <QDir>

namespace ExternalFixturePaths
{
inline QString root()
{
    const QString overrideRoot = qEnvironmentVariable("VIDEO_SIMILI_EXTERNAL_FIXTURES");
    if (!overrideRoot.isEmpty())
        return QDir::cleanPath(overrideRoot);

    return QDir(QDir::homePath())
        .filePath(QStringLiteral("Dev/Videos across all formats with duplicates of all kinds"));
}

inline QDir videos()
{
    return QDir(QDir(root()).filePath(QStringLiteral("Videos")));
}

inline QDir mountedLargeVideos()
{
#ifdef Q_OS_MACOS
    return QDir(QStringLiteral(
        "/Volumes/4TBSSD/Video duplicates - just for checking later my video duplicate program still works/Videos/"));
#else
    return {};
#endif
}
} // namespace ExternalFixturePaths

#endif // EXTERNAL_FIXTURE_PATHS_H
