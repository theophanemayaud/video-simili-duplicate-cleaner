#ifndef VIDEO_MATCHING_TEST_HELPERS_H
#define VIDEO_MATCHING_TEST_HELPERS_H

#include "../app/comparison/internal/videopairmatcher.h"
#include "../app/prefs.h"
#include "shared/video_matching_fixture_manifest.h"

#include <QList>
#include <QString>

#include <memory>

class Video;

namespace VideoMatchingTestHelpers
{
struct ProcessedFixture {
    MatchingFixtureRecord record;
    std::shared_ptr<Video> video;
    QString processingError;
};

struct FixtureScan {
    QList<ProcessedFixture> fixtures;
    qint64 elapsedMs = 0;
};

QList<MatchingFixtureRecord> recordsWithTag(const QList<MatchingFixtureRecord>& manifest, const QString& tag);
VideoPairMatchConfig matchConfig(int thumbnailMode, int thresholdBits, bool detectRotatedCopies = false);
FixtureScan scanFixtures(const QString& fixtureRoot, const QList<MatchingFixtureRecord>& manifest,
                         Prefs::USE_CACHE_OPTION cacheMode, const QString& cachePath, int thumbnailMode,
                         int thresholdBits, bool detectRotatedCopies = false);
const ProcessedFixture* findFixture(const FixtureScan& scan, const QString& file);
QString validatePairMatrix(const FixtureScan& scan, bool rotatedMatchingEnabled, int thresholdBits);
QString validateBaseline(const FixtureScan& scan);
} // namespace VideoMatchingTestHelpers

#endif // VIDEO_MATCHING_TEST_HELPERS_H
