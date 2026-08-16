#include "visualfingerprint.h"

#include <QTransform>

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
constexpr int ANALYSIS_MAX_EDGE = 64;
constexpr int MATCHING_TILE_MAX_EDGE = 64;
constexpr int PHASH_SIZE = 32; // pHash is generated from a 32x32 image.
constexpr int SSIM_SIZE = 16;  // Larger SSIM grids slow comparisons without useful matching benefit.
constexpr int NEAR_BLACK = 20;
constexpr double BLACK_LINE_COVERAGE = 0.98;
constexpr double MIN_BAR_FRACTION = 0.04;
constexpr double MAX_BAR_FRACTION = 0.45;
constexpr double LOW_INFORMATION_STDDEV = 2.0;
constexpr int LOW_INFORMATION_RANGE = 12;

QImage grayscaleAnalysisImage(const QImage& frame)
{
    if (frame.isNull())
        return {};

    QImage analysisImage = frame.convertToFormat(QImage::Format_Grayscale8);
    const int longestEdge = qMax(analysisImage.width(), analysisImage.height());
    if (longestEdge > ANALYSIS_MAX_EDGE) {
        const double scale = static_cast<double>(ANALYSIS_MAX_EDGE) / longestEdge;
        // Cap the longest edge while retaining the frame shape. A fixed square
        // would distort bar thickness; later code converts it to a fraction of
        // this analysis image before cropping the full-size tile.
        analysisImage = analysisImage.scaled(qMax(1, qRound(analysisImage.width() * scale)),
                                             qMax(1, qRound(analysisImage.height() * scale)),
                                             Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    return analysisImage;
}

bool lineIsBlack(const QImage& analysisImage, BlackBarAxis axis, int position)
{
    int black = 0;
    const int samples = axis == BlackBarAxis::horizontal ? analysisImage.width() : analysisImage.height();
    for (int sample = 0; sample < samples; ++sample) {
        const int x = axis == BlackBarAxis::horizontal ? sample : position;
        const int y = axis == BlackBarAxis::horizontal ? position : sample;
        if (analysisImage.constScanLine(y)[x] <= NEAR_BLACK)
            ++black;
    }
    return static_cast<double>(black) / samples >= BLACK_LINE_COVERAGE;
}

std::pair<int, int> scanOpposingBars(const QImage& analysisImage, BlackBarAxis axis)
{
    const int extent = axis == BlackBarAxis::horizontal ? analysisImage.height() : analysisImage.width();
    if (!lineIsBlack(analysisImage, axis, 0) || !lineIsBlack(analysisImage, axis, extent - 1))
        return {0, 0};

    int leading = 0;
    while (leading < extent && lineIsBlack(analysisImage, axis, leading))
        ++leading;

    int trailing = 0;
    while (trailing < extent - leading && lineIsBlack(analysisImage, axis, extent - 1 - trailing))
        ++trailing;

    const int minimumBar = qMax(2, static_cast<int>(std::ceil(extent * MIN_BAR_FRACTION)));
    const int maximumBar = static_cast<int>(std::floor(extent * MAX_BAR_FRACTION));
    if (leading < minimumBar || trailing < minimumBar || leading > maximumBar || trailing > maximumBar
        || qAbs(leading - trailing) > 1)
        return {0, 0};

    return {leading, trailing};
}

uint64_t computePhash(const cv::Mat& input)
{
    cv::Mat resized;
    cv::Mat gray;
    cv::Mat grayFloat;
    cv::Mat dct;
    cv::resize(input, resized, cv::Size(PHASH_SIZE, PHASH_SIZE), 0, 0, cv::INTER_AREA);
    // Preserve the established matcher weighting. QImage stores RGB here, but
    // the legacy path intentionally reached production using BGR conversion.
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
    gray.convertTo(grayFloat, CV_32F);
    cv::dct(grayFloat, dct);

    const cv::Mat topLeft = dct(cv::Rect(0, 0, 8, 8));
    const float dc = topLeft.at<float>(0, 0);
    const float average = (static_cast<float>(cv::sum(topLeft)[0]) - dc) / 63.0F;

    uint64_t hash = 0;
    int bit = 0;
    for (int row = 0; row < topLeft.rows; ++row)
        for (int column = 0; column < topLeft.cols; ++column, ++bit)
            if (topLeft.at<float>(row, column) > average)
                hash |= 1ULL << bit;
    return hash;
}
} // namespace

FrameAnalysis VisualFingerprintBuilder::analyzeFrame(const QImage& frame)
{
    const QImage analysisImage = grayscaleAnalysisImage(frame);
    FrameAnalysis result;
    if (analysisImage.isNull())
        return result;

    result.analysisWidth = analysisImage.width();
    result.analysisHeight = analysisImage.height();

    double sum = 0.0;
    double sumSquares = 0.0;
    int minimum = 255;
    int maximum = 0;
    const int pixels = analysisImage.width() * analysisImage.height();
    for (int y = 0; y < analysisImage.height(); ++y) {
        const uchar* line = analysisImage.constScanLine(y);
        for (int x = 0; x < analysisImage.width(); ++x) {
            const int value = line[x];
            sum += value;
            sumSquares += value * value;
            minimum = qMin(minimum, value);
            maximum = qMax(maximum, value);
        }
    }
    const double mean = sum / pixels;
    const double variance = qMax(0.0, sumSquares / pixels - mean * mean);
    // A frame needs either overall contrast or a meaningful brightness range. This preserves dark title/detail frames.
    result.informative = std::sqrt(variance) >= LOW_INFORMATION_STDDEV || maximum - minimum > LOW_INFORMATION_RANGE;
    if (!result.informative)
        return result;

    const auto horizontal = scanOpposingBars(analysisImage, BlackBarAxis::horizontal);
    const auto vertical = scanOpposingBars(analysisImage, BlackBarAxis::vertical);
    const bool hasHorizontal = horizontal.first > 0;
    const bool hasVertical = vertical.first > 0;
    if (hasHorizontal == hasVertical)
        return result;

    if (hasHorizontal) {
        result.barAxis = BlackBarAxis::horizontal;
        result.leadingBar = horizontal.first;
        result.trailingBar = horizontal.second;
    }
    else {
        result.barAxis = BlackBarAxis::vertical;
        result.leadingBar = vertical.first;
        result.trailingBar = vertical.second;
    }
    return result;
}

std::optional<NormalizedCrop>
VisualFingerprintBuilder::unanimousBlackBarCrop(const std::vector<FrameAnalysis>& analyses)
{
    std::vector<const FrameAnalysis*> informative;
    for (const FrameAnalysis& analysis : analyses)
        if (analysis.informative)
            informative.push_back(&analysis);
    if (informative.empty())
        return std::nullopt;

    const BlackBarAxis axis = informative.front()->barAxis;
    if (axis == BlackBarAxis::none)
        return std::nullopt;

    const double firstLeading = static_cast<double>(informative.front()->leadingBar)
                                / (axis == BlackBarAxis::horizontal ? informative.front()->analysisHeight
                                                                   : informative.front()->analysisWidth);
    const double firstTrailing = static_cast<double>(informative.front()->trailingBar)
                                 / (axis == BlackBarAxis::horizontal ? informative.front()->analysisHeight
                                                                    : informative.front()->analysisWidth);
    std::vector<double> leading;
    std::vector<double> trailing;
    for (const FrameAnalysis* analysis : informative) {
        if (analysis->barAxis != axis)
            return std::nullopt;
        const int extent = axis == BlackBarAxis::horizontal ? analysis->analysisHeight : analysis->analysisWidth;
        const double normalizedLeading = static_cast<double>(analysis->leadingBar) / extent;
        const double normalizedTrailing = static_cast<double>(analysis->trailingBar) / extent;
        if (std::abs(normalizedLeading - firstLeading) * extent > 1.0
            || std::abs(normalizedTrailing - firstTrailing) * extent > 1.0)
            return std::nullopt;
        // An analysis-image boundary represents an interval in the source image. Its
        // center is a better source-space estimate than the outer edge,
        // especially for bars whose scaled thickness is fractional.
        leading.push_back((analysis->leadingBar + 0.5) / extent);
        trailing.push_back((analysis->trailingBar + 0.5) / extent);
    }

    std::sort(leading.begin(), leading.end());
    std::sort(trailing.begin(), trailing.end());
    return NormalizedCrop{axis, leading[leading.size() / 2], trailing[trailing.size() / 2]};
}

QImage VisualFingerprintBuilder::transformTile(const QImage& tile, const std::optional<NormalizedCrop>& crop,
                                               FingerprintRotation rotation)
{
    QImage result = tile;
    if (crop) {
        QRect active = result.rect();
        if (crop->axis == BlackBarAxis::horizontal) {
            const int leading = qRound(result.height() * crop->leading);
            const int trailing = qRound(result.height() * crop->trailing);
            active.setTop(leading);
            active.setBottom(result.height() - trailing - 1);
        }
        else {
            const int leading = qRound(result.width() * crop->leading);
            const int trailing = qRound(result.width() * crop->trailing);
            active.setLeft(leading);
            active.setRight(result.width() - trailing - 1);
        }
        if (active.isValid())
            result = result.copy(active);
    }

    // pHash and SSIM consume only 32x32 and 16x16 inputs. Shrink each tile
    // before rotating and assembling the scratch collage so the opt-in path
    // never rotates full-resolution decoded frames.
    const int longestEdge = qMax(result.width(), result.height());
    if (longestEdge > MATCHING_TILE_MAX_EDGE) {
        const double scale = static_cast<double>(MATCHING_TILE_MAX_EDGE) / longestEdge;
        result = result.scaled(qMax(1, qRound(result.width() * scale)), qMax(1, qRound(result.height() * scale)),
                               Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    double angle = 0.0;
    if (rotation == FingerprintRotation::clockwise90)
        angle = 90.0;
    else if (rotation == FingerprintRotation::rotated180)
        angle = 180.0;
    else if (rotation == FingerprintRotation::counterClockwise90)
        angle = 270.0;
    if (angle != 0.0)
        result = result.transformed(QTransform().rotate(angle));
    return result.convertToFormat(QImage::Format_RGB888);
}

VisualFingerprint VisualFingerprintBuilder::build(const QImage& matchingImage, bool usable)
{
    VisualFingerprint result;
    result.usable = usable && !matchingImage.isNull();
    if (!result.usable)
        return result;

    const QImage rgb = matchingImage.convertToFormat(QImage::Format_RGB888);
    const cv::Mat image(rgb.height(), rgb.width(), CV_8UC3, const_cast<uchar*>(rgb.constBits()),
                        static_cast<size_t>(rgb.bytesPerLine()));
    result.phash = computePhash(image);

    cv::Mat resized;
    cv::Mat gray;
    cv::resize(image, resized, cv::Size(SSIM_SIZE, SSIM_SIZE), 0, 0, cv::INTER_AREA);
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
    for (int row = 0; row < SSIM_SIZE; ++row)
        std::memcpy(result.ssimPixels.data() + row * SSIM_SIZE, gray.ptr(row), SSIM_SIZE);
    return result;
}
