#pragma once

#include <Eigen/Core>
#include <opencv2/core/mat.hpp>

namespace fast_livo::detail
{
bool valid_vio_patch(const Eigen::Vector2d &pixel, int scale, int margin, const cv::Mat &image);
}
