#ifndef VIDEO_CORPUS_TEST_HELPERS_H
#define VIDEO_CORPUS_TEST_HELPERS_H

#include "../app/prefs.h"

#include <QDir>

struct WholeAppScanExpectation {
    Prefs::USE_CACHE_OPTION cacheOption = Prefs::NO_CACHE;
    int expectedVideos = 0;
    int expectedValidVideos = 0;
    int expectedMatchingVideos = 0;
    qint64 maximumElapsedMs = 0;
    uint similarityThreshold = 100;
    bool detectRotatedCopies = false;
};

// Shared mechanics only: the /Dev and mounted-large corpus targets own their
// paths, expected counts, and Qt Test slots.
class VideoCorpusTestHelpers
{
  public:
    static void initialize();
    static void emptyDatabase();
    static void addReferenceVideoRows(const QDir& videoDir, int expectedVideoCount);
    static void verifyReferenceVideo(Prefs::USE_CACHE_OPTION cacheOption, bool acceptSmallDurationDiff);
    static void runWholeAppScan(const QDir& videoDir, const WholeAppScanExpectation& expectation);
};

#endif // VIDEO_CORPUS_TEST_HELPERS_H
