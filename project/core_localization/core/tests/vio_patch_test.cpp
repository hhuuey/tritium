#include "vio_patch.h"
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
  try
  {
    const cv::Mat image(1024, 1224, CV_8UC1);
    using fast_livo::detail::valid_vio_patch;
    if (valid_vio_patch(Eigen::Vector2d(80, 200), 32, 5, image))
      throw std::runtime_error("高搜索层级的梯度窗口必须拒绝越界，即使中心在图像内");
    if (!valid_vio_patch(Eigen::Vector2d(80, 200), 1, 5, image) ||
        !valid_vio_patch(Eigen::Vector2d(160, 200), 32, 5, image))
      throw std::runtime_error("有效的精细层级或边界窗口不应被排除");
    if (valid_vio_patch(Eigen::Vector2d(160, 864), 32, 5, image))
      throw std::runtime_error("末像素之外的双线性邻域必须被排除");
    for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()})
      if (valid_vio_patch(Eigen::Vector2d(invalid, 200), 1, 5, image))
        throw std::runtime_error("非有限投影不能被转换为图像地址");
    return 0;
  }
  catch (const std::exception &error)
  {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
