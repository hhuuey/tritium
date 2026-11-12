/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
For commercial use, please contact the developer or Prof. Fu Zhang <fuzhang@hku.hk>.
This file is subject to the terms and conditions in the accompanying LICENSE file.
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <gtsam/navigation/GPSFactor.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/slam/BetweenFactor.h>
#include <gtsam/slam/PriorFactor.h>
#include <stdexcept>

namespace fast_livo
{
void Backend::Impl::buildBatchGraph()
{
  gtSAMgraph.resize(0);
  const double rtk_cov = config.rtk_covariance;
  const double livo2_RPY_cov = config.livo_rotation_covariance;
  const double livo2_XYZ_cov = config.livo_position_covariance;
  auto priorNoise = gtsam::noiseModel::Diagonal::Variances(
      (gtsam::Vector(6) << 1e-6, 1e-6, 1e-6, 1e-6, 1e-6, 1e-6).finished());
  auto odometryNoise = gtsam::noiseModel::Diagonal::Variances(
      (gtsam::Vector(6) << livo2_RPY_cov, livo2_RPY_cov, livo2_RPY_cov,
                           livo2_XYZ_cov, livo2_XYZ_cov, livo2_XYZ_cov).finished());
  auto gps_noise = gtsam::noiseModel::Diagonal::Variances(
      (gtsam::Vector(3) << rtk_cov, rtk_cov, 1).finished());
  if (gpsQueue_B.size() != initialEstimate.size())
    throw std::runtime_error("RTK 与关键帧数量不一致");
  for (size_t i = 0; i < initialEstimate.size(); ++i)
  {
    checkCancelled();
    if (i == 0)
    {
      gtsam::Pose3 priorPose_T_wa = initialEstimate.at<gtsam::Pose3>(0);
      gtSAMgraph.add(gtsam::PriorFactor<gtsam::Pose3>(0, priorPose_T_wa, priorNoise));
    }
    else
    {
      gtsam::Pose3 poseFrom_T_wa = initialEstimate.at<gtsam::Pose3>(i - 1);
      gtsam::Pose3 poseTo_T_wa = initialEstimate.at<gtsam::Pose3>(i);
      gtsam::Pose3 T_rel_ant_from_svd = poseFrom_T_wa.between(poseTo_T_wa);
      gtSAMgraph.add(gtsam::BetweenFactor<gtsam::Pose3>(i - 1, i, T_rel_ant_from_svd, odometryNoise));
    }
    const auto& it = gpsQueue_B[i];
    gtsam::Point3 gps_point(it.position.x, it.position.y, it.position.z);
    // 与旧实现一致，采样协方差被保存但 GPSFactor 使用固定配置方差。
    gtSAMgraph.add(gtsam::GPSFactor(i, gps_point, gps_noise));
  }
}

void Backend::Impl::optimize()
{
  checkCancelled();
  if (keyFrames.size() < 3 || gpsQueue.size() < 2)
    throw std::runtime_error("优化至少需要三个已配对关键帧和两个 GNSS 采样");
  initialAlign();
  writeTumTrajectory(livo_tum_before_output_path_);
  saveOptimizedGlobalMap();
  gtsam::LevenbergMarquardtParams params;
  params.orderingType = gtsam::Ordering::METIS;
  params.setLinearSolverType("MULTIFRONTAL_CHOLESKY");
  params.setMaxIterations(500);
  params.setRelativeErrorTol(1e-9);
  params.setAbsoluteErrorTol(1e-9);
  buildBatchGraph();
  checkCancelled();
  // 上游构造器未传上面的 params；保持其实际默认 LM 行为，而不借重构改变求解。
  // 另一个默认参数对象只增加取消 hook，不改变排序、容差或迭代上限。
  gtsam::LevenbergMarquardtParams effective_params;
  effective_params.iterationHook = [this](size_t, double, double) { checkCancelled(); };
  gtsam::LevenbergMarquardtOptimizer optimizer(gtSAMgraph, initialEstimate, effective_params);
  gtsam::Values result = optimizer.optimize();
  if (result.size() != keyFrames.size() || result.empty())
    throw std::runtime_error("图优化未产生完整位姿结果");
  initialEstimate = result;
  is_optimized = true;
  const int numPoses = initialEstimate.size();
  for (int i = 0; i < numPoses; ++i)
  {
    gtsam::Pose3 rtk_optimizedPose = initialEstimate.at<gtsam::Pose3>(i);
    if (!rtk_optimizedPose.matrix().allFinite())
      throw std::runtime_error("图优化产生非有限位姿");
    keyFrames[i].pose = rtk_optimizedPose;
  }
  writeTumTrajectory(opt_tum_output_path_);
  writeRtkTumTrajectory();
  // 此时 keyFrames 仍是刚写入 TUM 的优化天线位姿；在旧杆臂转换前只读提取。
  captureResult();
  for (int i = 0; i < numPoses; ++i)
  {
    gtsam::Pose3 rtk_optimizedPose = initialEstimate.at<gtsam::Pose3>(i);
    gtsam::Pose3 imu_optimizedPose = rtk_optimizedPose.compose(T_imu_rtk.inverse());
    keyFrames[i].pose = imu_optimizedPose;
  }
  // 与上游一致：TUM 保存天线轨迹，最终地图使用去除杆臂后的 IMU 位姿。
  saveOptimizedGlobalMap();
}
}
