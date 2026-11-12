#include "voxel_map.h"
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
  try
  {
    VoxelMapConfig config{};
    config.max_layer_ = 0;
    config.max_voxel_size_ = 1;
    config.sigma_num_ = 3;
    std::unordered_map<VOXEL_LOCATION, VoxelOctoTree *> map;
    VoxelMapManager manager(config, map);
    VoxelOctoTree tree(0, 0, 5, 50, 0.01f);
    auto &plane = *tree.plane_ptr_;
    plane.is_plane_ = true;
    plane.radius_ = 1;
    plane.center_ = V3D::Zero();
    plane.normal_ = V3D(0, 0, 1);
    plane.d_ = 0;
    plane.plane_var_.setZero();
    plane.plane_var_(5, 5) = std::numeric_limits<double>::infinity();
    pointWithVar point;
    point.point_b = V3D(0.2, 0, 0.1);
    point.point_w = point.point_b;
    PointToPlane residual{};
    residual.point_b_ = V3D(7, 8, 9);
    bool matched = false;
    double probability = 0;
    manager.build_single_residual(point, &tree, 0, matched, probability, residual);
    if (matched || probability != 0 || residual.point_b_ != V3D(7, 8, 9))
      throw std::runtime_error("未产生有效候选记录的点面匹配不能报告成功或改变输出");
    plane.plane_var_(5, 5) = 0.01;
    manager.build_single_residual(point, &tree, 0, matched, probability, residual);
    if (!matched || !(probability > 0) || residual.point_b_ != point.point_b)
      throw std::runtime_error("有效有限方差的点面匹配必须保留原结果");
    return 0;
  }
  catch (const std::exception &error)
  {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
