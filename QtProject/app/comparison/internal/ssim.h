#ifndef SSIM_H
#define SSIM_H

#include <opencv2/core/mat.hpp>

namespace Ssim
{
double calculate(const cv::Mat& left, const cv::Mat& right, int blockSize);
}

#endif // SSIM_H
