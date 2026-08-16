#include "videopairmatcher.h"

#include "ssim.h"

#include <bit>

namespace
{
// SSIM is substantially more expensive than comparing pHashes, so even in
// SSIM mode pHash acts as a cheap first-pass filter. A similarity of 44 means
// that at most 20 of the 64 pHash bits differ. Pairs below this are unlikely
// enough to match that we avoid paying the SSIM cost for them.
constexpr int MIN_PHASH_SIMILARITY_FOR_SSIM = 44;
constexpr int MIN_ROTATED_PHASH_SIMILARITY = 57;
constexpr double MIN_ROTATED_SSIM_SIMILARITY = 0.90;

int durationModifier(const Video& left, const Video& right, const VideoPairMatchConfig& config)
{
    return qAbs(left.duration - right.duration) <= 1000 ? config.sameDurationModifier
                                                       : -config.differentDurationModifier;
}

int rawPhashSimilarity(const VisualFingerprint& left, const VisualFingerprint& right)
{
    return 64 - std::popcount(left.phash ^ right.phash);
}

double ssimSimilarity(const VisualFingerprint& left, const VisualFingerprint& right, int blockSize)
{
    const cv::Mat leftBytes(16, 16, CV_8UC1, const_cast<uint8_t*>(left.ssimPixels.data()));
    const cv::Mat rightBytes(16, 16, CV_8UC1, const_cast<uint8_t*>(right.ssimPixels.data()));
    cv::Mat leftFloat;
    cv::Mat rightFloat;
    leftBytes.convertTo(leftFloat, CV_32F);
    rightBytes.convertTo(rightFloat, CV_32F);
    return Ssim::calculate(leftFloat, rightFloat, blockSize);
}

VideoPairMatchResult scoreFingerprints(const Video& leftVideo, const Video& rightVideo,
                                       const VisualFingerprint& left, const VisualFingerprint& right,
                                       const VideoPairMatchConfig& config, bool rotated)
{
    VideoPairMatchResult result;
    if (!left.usable || !right.usable)
        return result;

    const int rawPhash = rawPhashSimilarity(left, right);
    const int modifier = durationModifier(leftVideo, rightVideo, config);
    result.phashSimilarity = qMin(rawPhash + modifier, 64);

    if (rotated) {
        if (rawPhash < MIN_ROTATED_PHASH_SIMILARITY || result.phashSimilarity < config.thresholdPhash)
            return result;
        const double rawSsim = ssimSimilarity(left, right, config.ssimBlockSize);
        if (rawSsim < MIN_ROTATED_SSIM_SIMILARITY)
            return result;
        result.ssimSimilarity = rawSsim + modifier / 64.0;
        result.matches = config.comparisonMode != Prefs::_SSIM || result.ssimSimilarity > config.thresholdSSIM;
        return result;
    }

    if (config.comparisonMode == Prefs::_PHASH) {
        result.matches = result.phashSimilarity >= config.thresholdPhash;
    }
    else if (result.phashSimilarity >= qMax(config.thresholdPhash, MIN_PHASH_SIMILARITY_FOR_SSIM)) {
        const double rawSsim = ssimSimilarity(left, right, config.ssimBlockSize);
        result.ssimSimilarity = rawSsim + modifier / 64.0;
        result.matches = result.ssimSimilarity > config.thresholdSSIM;
    }
    return result;
}
} // namespace

VideoPairMatchConfig VideoPairMatcher::configFromPrefs(const Prefs& prefs)
{
    return {prefs.comparisonMode(), prefs.thumbnailsMode(),      prefs._thresholdPhash,
            prefs._thresholdSSIM,   prefs._ssimBlockSize,        prefs._sameDurationModifier,
            prefs._differentDurationModifier, prefs.detectRotatedCopies()};
}

VideoPairMatchResult VideoPairMatcher::match(const Video& left, const Video& right, const VideoPairMatchConfig& config)
{
    VideoPairMatchResult bestResult;
    const int hashes = config.thumbnailsMode == cutEnds ? 2 : 1;
    // In cutEnds mode, try each thumbnail/hash until one matches. A match from
    // either end is sufficient, so the other comparison can then be skipped.
    for (int hashIndex = 0; hashIndex < hashes; ++hashIndex) {
        const VideoPairMatchResult base = scoreFingerprints(left, right, left.fingerprint(hashIndex),
                                                            right.fingerprint(hashIndex), config, false);
        if (base.phashSimilarity > bestResult.phashSimilarity)
            bestResult = base;
        if (base.matches)
            return base;
    }

    if (!config.detectRotatedCopies)
        return bestResult;

    for (const FingerprintRotation rotation : allFingerprintRotations) {
        if (rotation == FingerprintRotation::none)
            continue;
        for (int hashIndex = 0; hashIndex < hashes; ++hashIndex) {
            VideoPairMatchResult rotated = scoreFingerprints(left, right, left.fingerprint(hashIndex),
                                                             right.fingerprint(hashIndex, rotation), config, true);
            if (rotated.phashSimilarity > bestResult.phashSimilarity)
                bestResult = rotated;
            if (rotated.matches)
                return rotated;
        }
    }
    return bestResult;
}
