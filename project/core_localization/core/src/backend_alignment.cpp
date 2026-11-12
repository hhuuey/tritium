/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
For commercial use, please contact the developer or Prof. Fu Zhang <fuzhang@hku.hk>.
This file is subject to the terms and conditions in the accompanying LICENSE file.
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fast_livo
{
namespace backend_detail
{
gtsam::Pose3 computeSVD(const std::vector<Eigen::Vector3d>& target, const std::vector<Eigen::Vector3d>& source)
{
  if (target.empty() || target.size() != source.size())
    return gtsam::Pose3::Identity();
  Eigen::Vector3d target_center = Eigen::Vector3d::Zero(), source_center = Eigen::Vector3d::Zero();
  for (const auto& p : target) target_center += p;
  for (const auto& p : source) source_center += p;
  target_center /= target.size();
  source_center /= source.size();
  Eigen::Matrix3d W = Eigen::Matrix3d::Zero();
  for (size_t i = 0; i < source.size(); ++i)
    W += (target[i] - target_center) * (source[i] - source_center).transpose();
  Eigen::JacobiSVD<Eigen::Matrix3d> svd(W, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Matrix3d R = svd.matrixU() * svd.matrixV().transpose();
  if (R.determinant() < 0)
    R = svd.matrixU() * Eigen::DiagonalMatrix<double, 3>(1, 1, -1) * svd.matrixV().transpose();
  Eigen::Vector3d t = target_center - R * source_center;
  return gtsam::Pose3{gtsam::Rot3(R), gtsam::Point3(t)};
}
}

void Backend::Impl::initialAlign()
{
  gpsQueue_B.clear();
  if (keyFrames.empty() || gpsQueue.empty())
    throw std::runtime_error("关键帧或 GNSS 队列为空");
  prepareTimeAlignment();
  checkCancelled();
  struct Sample { double t; Eigen::Vector3d p; };
  std::vector<Sample> samp;
  samp.reserve(gpsQueue.size());
  for (const auto& g : gpsQueue)
    samp.push_back({g.timestamp.seconds(), Eigen::Vector3d(g.position.x, g.position.y, g.position.z)});
  std::sort(samp.begin(), samp.end(), [](const Sample& a, const Sample& b) { return a.t < b.t; });
  std::vector<double> T, X, Y, Z;
  T.reserve(samp.size()); X.reserve(samp.size()); Y.reserve(samp.size()); Z.reserve(samp.size());
  for (size_t i = 0; i < samp.size(); ++i)
  {
    if (i > 0 && std::fabs(samp[i].t - samp[i - 1].t) < 1e-12)
      continue;
    T.emplace_back(samp[i].t);
    X.emplace_back(samp[i].p.x());
    Y.emplace_back(samp[i].p.y());
    Z.emplace_back(samp[i].p.z());
  }
  if (T.size() < 2)
    throw std::runtime_error("RTK 样条至少需要两个不同时间的采样");
  auto buildSpline = [](const std::vector<double>& t, const std::vector<double>& y,
                        std::vector<double>& M) -> bool {
    const size_t n = t.size();
    M.assign(n, 0.0);
    if (n < 2) return false;
    if (n == 2) return true;
    std::vector<double> h(n - 1);
    for (size_t i = 0; i < n - 1; ++i)
    {
      h[i] = t[i + 1] - t[i];
      if (h[i] <= 0) return false;
    }
    const size_t m = n - 2;
    std::vector<double> a(m), b(m), c(m), d(m);
    for (size_t i = 0; i < m; ++i)
    {
      a[i] = h[i] / 6.0;
      b[i] = (h[i] + h[i + 1]) / 3.0;
      c[i] = h[i + 1] / 6.0;
      d[i] = (y[i + 2] - y[i + 1]) / h[i + 1] - (y[i + 1] - y[i]) / h[i];
    }
    for (size_t i = 1; i < m; ++i)
    {
      double w = a[i] / b[i - 1];
      b[i] -= w * c[i - 1];
      d[i] -= w * d[i - 1];
    }
    std::vector<double> Min(m, 0.0);
    if (m > 0)
    {
      Min[m - 1] = d[m - 1] / b[m - 1];
      for (int i = int(m) - 2; i >= 0; --i)
        Min[i] = (d[i] - c[i] * Min[i + 1]) / b[i];
    }
    for (size_t i = 0; i < m; ++i) M[i + 1] = Min[i];
    return true;
  };
  auto evalSpline = [](const std::vector<double>& t, const std::vector<double>& y,
                       const std::vector<double>& M, double tq) -> double {
    const size_t n = t.size();
    if (n == 0) return 0.0;
    if (n == 1) return y[0];
    auto derivLeft = [&]() {
      double h = t[1] - t[0];
      return (y[1] - y[0]) / h - (h / 6.0) * (2.0 * M[0] + M[1]);
    };
    auto derivRight = [&]() {
      size_t k = n - 2;
      double h = t[k + 1] - t[k];
      return (y[k + 1] - y[k]) / h + (h / 6.0) * (2.0 * M[k + 1] + M[k]);
    };
    if (tq <= t.front()) return y.front() + derivLeft() * (tq - t.front());
    if (tq >= t.back()) return y.back() + derivRight() * (tq - t.back());
    size_t k = std::upper_bound(t.begin(), t.end(), tq) - t.begin() - 1;
    double h = t[k + 1] - t[k];
    double A = (t[k + 1] - tq) / h, B = (tq - t[k]) / h;
    // 特意保留上游 h/6 项，不能在库化过程中修正数学实现。
    return M[k] * (A * A * A) * h / 6.0 + M[k + 1] * (B * B * B) * h / 6.0
         + (y[k] - M[k] * h * h / 6.0) * A + (y[k + 1] - M[k + 1] * h * h / 6.0) * B;
  };
  std::vector<double> Mx, My, Mz;
  if (!buildSpline(T, X, Mx) || !buildSpline(T, Y, My) || !buildSpline(T, Z, Mz))
    throw std::runtime_error("RTK 样条构造失败");
  auto find_nearest_gps = [&](double query) -> const PoseSample* {
    auto it = std::lower_bound(gpsQueue.begin(), gpsQueue.end(), query,
                              [](const PoseSample& g, double t) { return g.timestamp.seconds() < t; });
    if (it == gpsQueue.begin()) return &*it;
    if (it == gpsQueue.end()) return &*std::prev(it);
    const auto& after = *it;
    const auto& before = *std::prev(it);
    return std::abs(after.timestamp.seconds() - query) < std::abs(query - before.timestamp.seconds())
               ? &after : &before;
  };
  for (size_t i = 0; i < keyFrames.size(); ++i)
  {
    checkCancelled();
    const double tk = keyFrames[i].time;
    PoseSample gps;
    gps.timestamp = Timestamp::from_seconds(tk);
    gps.frame_id = "map";
    gps.position = {evalSpline(T, X, Mx, tk), evalSpline(T, Y, My, tk), evalSpline(T, Z, Mz, tk)};
    gps.orientation.w = 1.0;
    const auto* nearest = find_nearest_gps(tk);
    gps.covariance[0] = nearest->covariance[0];
    gps.covariance[7] = nearest->covariance[7];
    gps.covariance[14] = nearest->covariance[14];
    gpsQueue_B.push_back(gps);
  }
  std::vector<Eigen::Vector3d> A_slam_gps, B_enu_gps;
  const double t_start = keyFrames.front().time;
  const double t_end_svd = t_start + 50.0;
  for (size_t i = 0; i < keyFrames.size(); ++i)
  {
    const double tk = keyFrames[i].time;
    if (tk > t_end_svd) break;
    if (tk < T.front() || tk > T.back()) continue;
    Eigen::Vector3d p_enu_gps;
    p_enu_gps.x() = evalSpline(T, X, Mx, tk);
    p_enu_gps.y() = evalSpline(T, Y, My, tk);
    p_enu_gps.z() = evalSpline(T, Z, Mz, tk);
    B_enu_gps.push_back(p_enu_gps);
    gtsam::Pose3 T_slam_imu = initialEstimate.at<gtsam::Pose3>(i);
    gtsam::Pose3 T_slam_gps = T_slam_imu.compose(T_imu_rtk);
    A_slam_gps.push_back(T_slam_gps.translation());
  }
  if (A_slam_gps.size() < 3)
    throw std::runtime_error("样条与关键帧的重叠配对不足三组，不能执行 SVD 对齐");
  gtsam::Pose3 T_enu_slam = backend_detail::computeSVD(B_enu_gps, A_slam_gps);
  for (size_t i = 0; i < keyFrames.size(); ++i)
  {
    gtsam::Pose3 T_slam_imu = initialEstimate.at<gtsam::Pose3>(i);
    gtsam::Pose3 T_slam_gps = T_slam_imu.compose(T_imu_rtk);
    gtsam::Pose3 T_enu_gps = T_enu_slam.compose(T_slam_gps);
    initialEstimate.update(i, T_enu_gps);
    keyFrames[i].pose = T_enu_gps;
  }
}
}
