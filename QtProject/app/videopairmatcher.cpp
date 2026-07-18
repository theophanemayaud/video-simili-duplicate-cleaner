#include "videopairmatcher.h"

#include "ssim.h"

namespace
{
// SSIM is substantially more expensive than comparing pHashes, so even in
// SSIM mode pHash acts as a cheap first-pass filter. A similarity of 44 means
// that at most 20 of the 64 pHash bits differ. Pairs below this are unlikely
// enough to match that we avoid paying the SSIM cost for them.
constexpr int MIN_PHASH_SIMILARITY_FOR_SSIM = 44;

int phashSimilarity(const Video& left, const Video& right, int hashIndex,
                    const VideoPairMatchConfig& config)
{
    if (left.hash[hashIndex] == 0 && right.hash[hashIndex] == 0)
        return 0;

    int similarity = 64;
    uint64_t differentBits = left.hash[hashIndex] ^ right.hash[hashIndex];
    while (differentBits) {
        differentBits &= differentBits - 1;
        similarity--;
    }

    similarity += qAbs(left.duration - right.duration) <= 1000 ? config.sameDurationModifier
                                                               : -config.differentDurationModifier;
    return qMin(similarity, 64);
}
}

VideoPairMatchConfig VideoPairMatcher::configFromPrefs(const Prefs& prefs)
{
    return {prefs.comparisonMode(), prefs.thumbnailsMode(),       prefs._thresholdPhash,
            prefs._thresholdSSIM,   prefs._ssimBlockSize,         prefs._sameDurationModifier,
            prefs._differentDurationModifier};
}

VideoPairMatchResult VideoPairMatcher::match(const Video& left, const Video& right,
                                             const VideoPairMatchConfig& config)
{
    VideoPairMatchResult result;
    const int hashes = config.thumbnailsMode == cutEnds ? 2 : 1;
    for (int hashIndex = 0; hashIndex < hashes; ++hashIndex) {
        result.phashSimilarity =
            qMax(result.phashSimilarity, phashSimilarity(left, right, hashIndex, config));

        if (config.comparisonMode == Prefs::_PHASH) {
            result.matches = result.phashSimilarity >= config.thresholdPhash;
        }
        // Keep a stricter user-selected pHash threshold when applicable, but
        // never run expensive SSIM below its minimum pHash prefilter.
        else if (result.phashSimilarity >= qMax(config.thresholdPhash, MIN_PHASH_SIMILARITY_FOR_SSIM)) {
            result.ssimSimilarity = Ssim::calculate(left.grayThumb[hashIndex], right.grayThumb[hashIndex],
                                                    config.ssimBlockSize);
            const int durationModifier = qAbs(left.duration - right.duration) <= 1000
                                             ? config.sameDurationModifier
                                             : -config.differentDurationModifier;
            result.ssimSimilarity += durationModifier / 64.0;
            result.matches = result.ssimSimilarity > config.thresholdSSIM;
        }

        if (result.matches)
            break;
    }
    return result;
}
