#include "video_matching_test_helpers.h"

#include "../app/db.h"
#include "../app/video.h"

#include <QElapsedTimer>

namespace
{
QString pairKey(const QString& left, const QString& right)
{
    return left < right ? left + QStringLiteral(" <> ") + right : right + QStringLiteral(" <> ") + left;
}

QString formatSetDifference(const QString& label, const QSet<QString>& pairs)
{
    if (pairs.isEmpty())
        return {};
    QStringList sorted(pairs.begin(), pairs.end());
    sorted.sort();
    return QStringLiteral("%1 (%2):\n  %3").arg(label).arg(sorted.size()).arg(sorted.join(QStringLiteral("\n  ")));
}
} // namespace

namespace VideoMatchingTestHelpers
{
QList<MatchingFixtureRecord> recordsWithTag(const QList<MatchingFixtureRecord>& manifest, const QString& tag)
{
    QList<MatchingFixtureRecord> selected;
    for (const MatchingFixtureRecord& record : manifest)
        if (record.tags.contains(tag))
            selected.append(record);
    return selected;
}

VideoPairMatchConfig matchConfig(int thumbnailMode, int thresholdBits, bool detectRotatedCopies)
{
    VideoPairMatchConfig config;
    config.comparisonMode = Prefs::_PHASH;
    config.thumbnailsMode = thumbnailMode;
    config.thresholdPhash = thresholdBits;
    config.sameDurationModifier = 1;
    config.differentDurationModifier = 4;
    config.detectRotatedCopies = detectRotatedCopies;
    return config;
}

FixtureScan scanFixtures(const QString& fixtureRoot, const QList<MatchingFixtureRecord>& manifest,
                         Prefs::USE_CACHE_OPTION cacheMode, const QString& cachePath, int thumbnailMode,
                         int thresholdBits, bool detectRotatedCopies)
{
    Prefs prefs;
    prefs.resetSettings();
    prefs.thumbnailsMode(thumbnailMode);
    prefs.comparisonMode(Prefs::_PHASH);
    prefs.useCacheOption(cacheMode);
    prefs.cacheFilePathName(cachePath);
    prefs._thresholdPhash = thresholdBits;
    prefs._sameDurationModifier = 1;
    prefs._differentDurationModifier = 4;
    prefs.detectRotatedCopies(detectRotatedCopies);
    if (cacheMode != Prefs::NO_CACHE)
        Db::initDbAndCacheLocation(prefs);

    QElapsedTimer timer;
    timer.start();
    FixtureScan scan;
    for (const MatchingFixtureRecord& record : manifest) {
        ProcessedFixture fixture;
        fixture.record = record;
        fixture.video = std::make_shared<Video>(prefs, QDir(fixtureRoot).filePath(record.file));
        const Video::ProcessingResult result = fixture.video->process();
        if (!result.success)
            fixture.processingError = result.errorMsg;
        scan.fixtures.append(std::move(fixture));
    }
    scan.elapsedMs = timer.elapsed();
    return scan;
}

const ProcessedFixture* findFixture(const FixtureScan& scan, const QString& file)
{
    for (const ProcessedFixture& fixture : scan.fixtures)
        if (fixture.record.file == file)
            return &fixture;
    return nullptr;
}

QString validatePairMatrix(const FixtureScan& scan, bool rotatedMatchingEnabled, int thresholdBits)
{
    QStringList processingProblems;
    for (const ProcessedFixture& fixture : scan.fixtures) {
        const bool succeeded = fixture.processingError.isEmpty();
        if (succeeded != fixture.record.expectedProcessing)
            processingProblems.append(
                QStringLiteral("%1 expected %2 but got %3")
                    .arg(fixture.record.file,
                         fixture.record.expectedProcessing ? QStringLiteral("success") : QStringLiteral("failure"),
                         succeeded ? QStringLiteral("success") : fixture.processingError));
    }

    QSet<QString> expectedPairs;
    QSet<QString> allowedPairs;
    QSet<QString> actualPairs;
    const VideoPairMatchConfig config = matchConfig(cutEnds, thresholdBits, rotatedMatchingEnabled);
    for (qsizetype leftIndex = 0; leftIndex < scan.fixtures.size(); ++leftIndex) {
        const ProcessedFixture& left = scan.fixtures.at(leftIndex);
        for (qsizetype rightIndex = leftIndex + 1; rightIndex < scan.fixtures.size(); ++rightIndex) {
            const ProcessedFixture& right = scan.fixtures.at(rightIndex);
            const QString key = pairKey(left.record.file, right.record.file);
            const bool sameContent = left.record.contentGroup == right.record.contentGroup;
            const bool sameOrientation =
                left.record.matchingOrientationDegrees == right.record.matchingOrientationDegrees;
            if (sameContent && (rotatedMatchingEnabled || sameOrientation) && left.record.expectedProcessing
                && right.record.expectedProcessing) {
                allowedPairs.insert(key);
                // Partial clips and substantially different durations are outside this feature's recall contract.
                if (qAbs(left.video->duration - right.video->duration) <= 1000)
                    expectedPairs.insert(key);
            }
            if (left.processingError.isEmpty() && right.processingError.isEmpty()
                && VideoPairMatcher::match(*left.video, *right.video, config).matches)
                actualPairs.insert(key);
        }
    }

    QStringList problems;
    if (!processingProblems.isEmpty())
        problems.append(QStringLiteral("Processing mismatches (%1):\n  %2")
                            .arg(processingProblems.size())
                            .arg(processingProblems.join(QStringLiteral("\n  "))));
    const QString missingText = formatSetDifference(QStringLiteral("Missing pairs"), expectedPairs - actualPairs);
    const QString unexpectedText = formatSetDifference(QStringLiteral("Unexpected pairs"), actualPairs - allowedPairs);
    if (!missingText.isEmpty())
        problems.append(missingText);
    if (!unexpectedText.isEmpty())
        problems.append(unexpectedText);
    return problems.join(QStringLiteral("\n"));
}

QString validateBaseline(const FixtureScan& scan)
{
    QStringList processingProblems;
    QSet<QString> expectedEstablishedPairs;
    QSet<QString> actualPairs;
    QSet<QString> actualCrossGroupPairs;
    const VideoPairMatchConfig config = matchConfig(cutEnds, 64, false);
    for (const ProcessedFixture& fixture : scan.fixtures) {
        if (fixture.record.tags.contains(QStringLiteral("expected_recovery")))
            continue;
        const bool succeeded = fixture.processingError.isEmpty();
        if (succeeded != fixture.record.expectedProcessing)
            processingProblems.append(
                QStringLiteral("%1 expected %2 but got %3")
                    .arg(fixture.record.file,
                         fixture.record.expectedProcessing ? QStringLiteral("success") : QStringLiteral("failure"),
                         succeeded ? QStringLiteral("success") : fixture.processingError));
    }

    for (qsizetype leftIndex = 0; leftIndex < scan.fixtures.size(); ++leftIndex) {
        const ProcessedFixture& left = scan.fixtures.at(leftIndex);
        for (qsizetype rightIndex = leftIndex + 1; rightIndex < scan.fixtures.size(); ++rightIndex) {
            const ProcessedFixture& right = scan.fixtures.at(rightIndex);
            const QString key = pairKey(left.record.file, right.record.file);
            const bool sameGroup = left.record.contentGroup == right.record.contentGroup;
            const bool recoveryPair = left.record.tags.contains(QStringLiteral("expected_recovery"))
                                      || right.record.tags.contains(QStringLiteral("expected_recovery"));
            if (sameGroup && !recoveryPair && left.record.expectedProcessing && right.record.expectedProcessing)
                expectedEstablishedPairs.insert(key);
            if (left.processingError.isEmpty() && right.processingError.isEmpty()
                && VideoPairMatcher::match(*left.video, *right.video, config).matches)
            {
                actualPairs.insert(key);
                if (!sameGroup)
                    actualCrossGroupPairs.insert(key);
            }
        }
    }

    QStringList problems;
    if (!processingProblems.isEmpty())
        problems.append(QStringLiteral("Processing mismatches (%1):\n  %2")
                            .arg(processingProblems.size())
                            .arg(processingProblems.join(QStringLiteral("\n  "))));
    const QString missingText = formatSetDifference(QStringLiteral("Missing established baseline pairs"),
                                                    expectedEstablishedPairs - actualPairs);
    const QString unexpectedText =
        formatSetDifference(QStringLiteral("Unexpected cross-group pairs"), actualCrossGroupPairs);
    if (!missingText.isEmpty())
        problems.append(missingText);
    if (!unexpectedText.isEmpty())
        problems.append(unexpectedText);
    return problems.join(QStringLiteral("\n"));
}
} // namespace VideoMatchingTestHelpers
