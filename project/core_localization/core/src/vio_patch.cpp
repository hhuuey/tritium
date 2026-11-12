#include "vio_patch.h"
#include <cmath>
#include <cstdint>

namespace fast_livo::detail
{
bool valid_vio_patch(const Eigen::Vector2d &pixel, int scale, int margin, const cv::Mat &image)
{
  if (image.empty() || image.type() != CV_8UC1 || !image.isContinuous() || scale < 1 || margin < 0 ||
      !std::isfinite(pixel[0]) || !std::isfinite(pixel[1]) ||
      pixel[0] < 0 || pixel[1] < 0 || pixel[0] >= image.cols || pixel[1] >= image.rows)
    return false;
  const auto x = static_cast<std::int64_t>(std::floor(static_cast<float>(pixel[0] / scale)) * scale);
  const auto y = static_cast<std::int64_t>(std::floor(static_cast<float>(pixel[1] / scale)) * scale);
  const auto radius = static_cast<std::int64_t>(margin) * scale;
  return x - radius >= 0 && y - radius >= 0 && x + radius < image.cols && y + radius < image.rows;
}
}
