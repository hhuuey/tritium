/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
For commercial use, please contact the developer or Prof. Fu Zhang <fuzhang@hku.hk>.
This file is subject to the terms and conditions in the accompanying LICENSE file.
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace fast_livo
{
Backend::Impl::Impl(const BackendConfig& options)
    : config(options), pairing([this](const PoseSample& pose, const CloudSample& cloud) { matched(pose, cloud); })
{
  if (config.output_directory.empty())
    throw std::invalid_argument("后端需要显式配置 output_directory");
  if (config.gps_extrinsic.size() != 3 ||
      !backend_detail::allFinite({config.gps_offset, config.idle_seconds, config.rtk_covariance,
                                 config.livo_rotation_covariance, config.livo_position_covariance}) ||
      config.idle_seconds < 0 || config.rtk_covariance <= 0 ||
      config.livo_rotation_covariance <= 0 || config.livo_position_covariance <= 0)
    throw std::invalid_argument("后端配置的外参、时间或协方差无效");
  const Eigen::Vector3d lever(config.gps_extrinsic[0], config.gps_extrinsic[1], config.gps_extrinsic[2]);
  if (!backend_detail::allFinite({lever.x(), lever.y(), lever.z()}))
    throw std::invalid_argument("GNSS 外参必须为有限值");
  backend_detail::durationNanoseconds(config.gps_offset);
  T_imu_rtk = gtsam::Pose3(gtsam::Rot3(), gtsam::Point3(lever));
  const std::string root = config.output_directory;
  debug_optdata_path_ = root + "/debug/";
  opt_tum_output_path_ = root + "/TUM/opt_trajectory_after.txt";
  livo_tum_before_output_path_ = root + "/TUM/livo_trajectory_before.txt";
  rtk_tum_output_path_ = root + "/TUM/rtk_trajectory_time_aligned.txt";
  opt_vel_output_path_ = root + "/vel/opt_vel.txt";
  gps_vel_output_path_ = root + "/vel/gps_vel.txt";
  global_map_pcd_path_ = root + "/global_pcd/";
  keyframe_scan_pcd_path_ = root + "/scan_pcd/";
  for (const auto& directory : {debug_optdata_path_ + "pcd/", root + "/TUM/", root + "/vel/",
                                global_map_pcd_path_, keyframe_scan_pcd_path_})
    std::filesystem::create_directories(directory);
  if (config.debug_mode)
    loadData(config.debug_load_directory.empty() ? debug_optdata_path_ : config.debug_load_directory);
}

Backend::Backend(const BackendConfig& config) : impl_(std::make_unique<Impl>(config)) {}
Backend::~Backend() { shutdown(); }

void Backend::Impl::ensureCollecting() const
{
  if (state == BackendStatus::Optimizing || state == BackendStatus::Completed)
    throw BackendFrozen("后端本轮已冻结，不再接收输入");
  if (state == BackendStatus::Failed)
  {
    if (error)
      std::rethrow_exception(error);
    throw std::runtime_error(error_message.empty() ? "后端优化失败，未记录原始异常" : error_message);
  }
  if (state == BackendStatus::Cancelled)
    throw std::runtime_error("后端优化已取消");
  checkCancelled();
}

void Backend::push_pose(const PoseSample& sample)
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->ensureCollecting();
  // 在进入配对队列前验证，避免异常破坏正在搜索的候选。
  backend_detail::poseFromSample(sample);
  if (sample.timestamp.nanoseconds < 0)
    throw std::invalid_argument("位姿时间戳必须非负");
  impl_->pairing.add(sample);
}

void Backend::push_cloud(const CloudSample& sample)
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->ensureCollecting();
  if (sample.timestamp.nanoseconds < 0)
    throw std::invalid_argument("点云时间戳必须非负");
  impl_->pairing.add(sample);
}

void Backend::request_optimization()
{
  std::lock_guard<std::mutex> join_lock(impl_->join_mutex);
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->state != BackendStatus::Collecting)
    throw std::logic_error("每个后端实例只能请求一次优化");
  impl_->checkCancelled();
  impl_->state = BackendStatus::WaitingForIdle;
  try
  {
    impl_->worker = std::thread([this] { impl_->run(); });
  }
  catch (...)
  {
    impl_->state = BackendStatus::Collecting;
    throw;
  }
}

void Backend::wait()
{
  {
    std::lock_guard<std::mutex> join_lock(impl_->join_mutex);
    if (impl_->worker.joinable())
      impl_->worker.join();
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->error)
    std::rethrow_exception(impl_->error);
  if (impl_->state == BackendStatus::Cancelled)
    throw std::runtime_error("后端优化已取消");
  if (impl_->state != BackendStatus::Completed)
    throw std::logic_error("尚未请求后端优化");
}

void Backend::shutdown() noexcept
{
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->cancelled.store(true);
  }
  impl_->changed.notify_all();
  std::lock_guard<std::mutex> join_lock(impl_->join_mutex);
  if (impl_->worker.joinable())
    impl_->worker.join();
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->state == BackendStatus::Collecting)
  {
    impl_->state = BackendStatus::Cancelled;
    impl_->error_message = "后端优化已取消";
  }
}

BackendStatus Backend::status() const
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state;
}

std::string Backend::failure() const
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->error_message;
}

std::size_t Backend::matched_frame_count() const { return impl_->frame_count.load(); }

BackendResult Backend::result() const
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->state != BackendStatus::Completed)
    throw std::logic_error("后端优化尚未成功完成，无法读取结果");
  BackendResult result;
  result.trajectory = impl_->completed_trajectory;
  result.before_map_file = impl_->global_map_pcd_path_ + "before_optimization.pcd";
  result.after_map_file = impl_->global_map_pcd_path_ + "after_optimization.pcd";
  result.keyframe_count = result.trajectory.size();
  return result;
}

void Backend::Impl::captureResult()
{
  std::vector<BackendTrajectoryPoint> trajectory;
  trajectory.reserve(keyFrames.size());
  for (const auto& frame : keyFrames)
  {
    checkCancelled();
    const auto& position = frame.pose.translation();
    const auto orientation = frame.pose.rotation().toQuaternion();
    BackendTrajectoryPoint point;
    point.timestamp = Timestamp::from_seconds(frame.time);
    point.position = {position.x(), position.y(), position.z()};
    point.orientation = {orientation.x(), orientation.y(), orientation.z(), orientation.w()};
    trajectory.push_back(point);
  }
  std::lock_guard<std::mutex> lock(mutex);
  completed_trajectory = std::move(trajectory);
}

void Backend::Impl::checkCancelled() const
{
  if (cancelled.load())
    throw std::runtime_error("后端优化已取消");
}

void Backend::Impl::run() noexcept
{
  try
  {
    {
      std::unique_lock<std::mutex> lock(mutex);
      if (keyFrames.empty())
        throw std::runtime_error("没有已配对的关键帧，不能执行优化");
      while (true)
      {
        checkCancelled();
        const auto deadline = last_keyframe_time +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(config.idle_seconds));
        if (std::chrono::steady_clock::now() >= deadline)
          break;
        changed.wait_until(lock, deadline);
      }
      state = BackendStatus::Optimizing;
    }
    optimize();
    checkCancelled();
    std::lock_guard<std::mutex> lock(mutex);
    state = BackendStatus::Completed;
  }
  catch (const std::exception& exception)
  {
    std::lock_guard<std::mutex> lock(mutex);
    error = std::current_exception();
    error_message = exception.what();
    state = cancelled.load() ? BackendStatus::Cancelled : BackendStatus::Failed;
  }
  catch (...)
  {
    std::lock_guard<std::mutex> lock(mutex);
    error = std::current_exception();
    error_message = "后端发生未知异常";
    state = BackendStatus::Failed;
  }
  changed.notify_all();
}
}
