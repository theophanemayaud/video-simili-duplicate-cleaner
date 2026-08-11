#ifndef VIDEOPAIRMATCHER_H
#define VIDEOPAIRMATCHER_H

#include "prefs.h"
#include "video.h"
#include "videopairspace.h"

// Immutable scan-time settings needed to decide whether two videos
// intrinsically match. The complete pair space grows quadratically with the
// number of videos, while actual visual matches are assumed to be sparse. The
// scan therefore applies these relatively expensive, strongly selective
// checks once to produce a much smaller candidate list. Background workers
// receive a snapshot because changing any of these values can change that list
// and therefore requires a new scan.
//
// This deliberately excludes filters such as file existence/trashed state,
// ignored-pair database state, and filename containment. Those values are
// mutable and cheap enough to check when consuming the already sparse list of
// discovered matches. This balances scan time against navigation time: enough
// work is done during discovery to make runtime filtering short, while keeping
// mutable filters out lets their changes take effect immediately without
// rescanning every possible video pair.
struct VideoPairMatchConfig {
    Prefs::VisualComparisonModes comparisonMode = Prefs::_PHASH;
    int thumbnailsMode = thumb4;
    int thresholdPhash = 57;
    double thresholdSSIM = Prefs::DEFAULT_SSIM_THRESHOLD;
    int ssimBlockSize = 16;
    int sameDurationModifier = 1;
    int differentDurationModifier = 4;
    bool detectRotatedCopies = false;
};

struct VideoPairMatchResult {
    bool matches = false;
    int phashSimilarity = 0;
    double ssimSimilarity = 0.0;
    FingerprintRotation relativeRotation = FingerprintRotation::none;
};

struct MatchedVideoPair {
    int left = 0;
    int right = 0;
    int64_t position = 0;
    int phashSimilarity = 0;
    double ssimSimilarity = 0.0;
};

namespace VideoPairMatcher
{
VideoPairMatchConfig configFromPrefs(const Prefs& prefs);
VideoPairMatchResult match(const Video& left, const Video& right, const VideoPairMatchConfig& config);
} // namespace VideoPairMatcher

#endif // VIDEOPAIRMATCHER_H
