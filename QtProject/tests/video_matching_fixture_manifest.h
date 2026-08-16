#ifndef VIDEO_MATCHING_FIXTURE_MANIFEST_H
#define VIDEO_MATCHING_FIXTURE_MANIFEST_H

#include <QList>
#include <QSet>
#include <QString>

struct MatchingFixtureRecord {
    QString file;
    QString contentGroup;
    bool expectedProcessing = true;
    int matchingOrientationDegrees = 0;
    QSet<QString> tags;
};

class MatchingFixtureManifest
{
  public:
    static bool load(const QString& path, QList<MatchingFixtureRecord>& records, QString& error);
};

#endif // VIDEO_MATCHING_FIXTURE_MANIFEST_H
