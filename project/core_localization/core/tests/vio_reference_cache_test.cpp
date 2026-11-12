#include "vio.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

int main()
{
  try
  {
    auto point = std::make_unique<VisualPoint>(V3D(0, 0, 3));
    point->normal_ = V3D(0, 0, 1);
    point->is_normal_initialized_ = true;
    for (int id = 0; id < 6; ++id)
    {
      auto *patch = new float[4]{float(id * 10), float(id * 10 + 1),
                                float(id * 10 + 2), float(id * 10 + 3)};
      auto *feature = new Feature(point.get(), patch, V2D::Zero(), V3D(0, 0, 1), SE3(), 0);
      feature->id_ = id;
      feature->inv_expo_time_ = 1;
      point->addFrameRef(feature);
    }
    VIOManager manager;
    manager.visual_submap = new SubSparseMap;
    manager.visual_submap->voxel_points.push_back(point.get());
    manager.total_points = 1;
    manager.patch_size_total = 4;
    manager.update_flag = {1};
    const std::unordered_map<VOXEL_LOCATION, VoxelOctoTree *> planes;
    for (int call = 0; call < 3; ++call)
    {
      manager.updateReferencePatch(planes);
      for (const auto *feature : point->obs_)
      {
        if (!std::isfinite(feature->score_) || std::abs(feature->score_ - 2.0f) > 1e-6f)
          throw std::runtime_error("同形偏移图像块的 NCC 必须为1，缓存均值后不能改变参考评分");
        if (feature->mean_ != feature->id_ * 10 + 1.5f)
          throw std::runtime_error("参考图像块均值缓存无效");
      }
    }
    return 0;
  }
  catch (const std::exception &error)
  {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
