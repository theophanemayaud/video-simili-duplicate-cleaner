#ifndef VISUALFINGERPRINT_H
#define VISUALFINGERPRINT_H

#include <QImage>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

enum class FingerprintRotation : int {
    none = 0,
    clockwise90 = 1,
    rotated180 = 2,
    counterClockwise90 = 3,
};

struct VisualFingerprint {
    uint64_t phash = 0;
    std::array<uint8_t, 16 * 16> ssimPixels{};
    bool usable = false;
};

enum class BlackBarAxis {
    none,
    horizontal,
    vertical,
};

struct FrameAnalysis {
    bool informative = false;
    BlackBarAxis barAxis = BlackBarAxis::none;
    int leadingBar = 0;
    int trailingBar = 0;
    int proxyWidth = 0;
    int proxyHeight = 0;
};

struct NormalizedCrop {
    BlackBarAxis axis = BlackBarAxis::none;
    double leading = 0.0;
    double trailing = 0.0;
};

namespace VisualFingerprintBuilder
{
FrameAnalysis analyzeFrame(const QImage& frame);
std::optional<NormalizedCrop> unanimousBlackBarCrop(const std::vector<FrameAnalysis>& analyses);
QImage transformTile(const QImage& tile, const std::optional<NormalizedCrop>& crop, FingerprintRotation rotation);
VisualFingerprint build(const QImage& matchingImage, bool usable);
} // namespace VisualFingerprintBuilder

#endif // VISUALFINGERPRINT_H
