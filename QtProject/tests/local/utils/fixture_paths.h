#ifndef LOCAL_FIXTURE_PATHS_H
#define LOCAL_FIXTURE_PATHS_H

#include <QDir>

namespace LocalFixturePaths
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
} // namespace LocalFixturePaths

#endif // LOCAL_FIXTURE_PATHS_H
