/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
For commercial use, please contact the developer or Prof. Fu Zhang <fuzhang@hku.hk>.
This file is subject to the terms and conditions in the accompanying LICENSE file.
*/
#pragma once

#include "fast_livo/measurements.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fast_livo
{

struct BackendConfig
{
  // 与旧默认一致为空；构造时必须显式指定，禁止隐式写入根目录。
  std::string output_directory;
  // 保留适配器元数据；与旧 gpsHandler 一样，push_gnss 不根据 gps_enabled 丢弃样本。
  bool gps_enabled = false;
  std::string gps_topic = "/ublox_driver/receiver_lla";
  double gps_offset = 0.0;
  // 必须显式提供三个有限值；零杆臂使用 {0, 0, 0}，不能用空向量代替。
  std::vector<double> gps_extrinsic;
  double rtk_covariance = 0.02;
  double livo_rotation_covariance = 1e-4;
  double livo_position_covariance = 1e-4;
  bool debug_mode = false;
  std::string debug_load_directory;
  // 空路径禁用绘图；显式提供而不存在或执行失败时，wait() 报错。
  std::string plot_script;
  double idle_seconds = 3.0;
};

enum class BackendStatus { Collecting, WaitingForIdle, Optimizing, Completed, Cancelled, Failed };

class BackendFrozen : public std::logic_error
{
public:
  using std::logic_error::logic_error;
};

struct BackendTrajectoryPoint
{
  Timestamp timestamp;
  Vector3 position;
  Quaternion orientation;
};

struct BackendResult
{
  // 与 opt_trajectory_after.txt 相同的 RTK 天线轨迹；不伪造优化后的速度。
  // 地图仍保持旧行为，使用去除杆臂后的 IMU 位姿。
  std::vector<BackendTrajectoryPoint> trajectory;
  std::string before_map_file, after_map_file;
  std::size_t keyframe_count = 0;
};

// 实例拥有独立 GNSS 原点。push_* 可并发调用，冻结后抛 BackendFrozen；
// 失败时重抛原异常，取消时抛取消异常，输入校验失败不会被当成冻结。
// request_optimization() 为一次性请求；wait() 等待完成并重新抛出失败。
// shutdown() 可重复调用，取消并 join；析构不会抛异常。
class Backend
{
public:
  explicit Backend(const BackendConfig& config);
  ~Backend();
  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;
  void push_gnss(const GnssSample& sample);
  void push_pose(const PoseSample& sample);
  void push_cloud(const CloudSample& sample);
  void request_optimization();
  void wait();
  void shutdown() noexcept;
  BackendStatus status() const;
  std::string failure() const;
  std::size_t matched_frame_count() const;
  // 仅 Completed 可读取，返回轨迹副本和文件引用，不复制全局点云。
  BackendResult result() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}

// 仅实现与白盒配对测试可见，公共 API 不暴露 GTSAM 或 GeographicLib。
#ifdef FAST_LIVO_BACKEND_INTERNAL
#include <GeographicLib/LocalCartesian.hpp>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <thread>

namespace fast_livo
{
namespace backend_detail
{
bool allFinite(std::initializer_list<double>);
// ROS1 ApproximateTime(10) 的双输入特化：默认 age_penalty=0.1，
// lower_bound=0，不重排输入、不添加 slop，不强行 flush 未证明最优的候选。
class Pairing
{
public:
  using Callback = std::function<void(const PoseSample&, const CloudSample&)>;
  explicit Pairing(Callback callback);
  void add(const PoseSample& pose);
  void add(const CloudSample& cloud);

private:
  struct Entry
  {
    std::int64_t time = 0;
    std::shared_ptr<const PoseSample> pose;
    std::shared_ptr<const CloudSample> cloud;
  };
  void add(int stream, Entry entry);
  void process();
  void move(int stream);
  void recover(int stream, std::size_t count);
  void publish();
  std::array<std::int64_t, 2> times(bool virtual_search) const;
  Callback callback_;
  std::array<std::deque<Entry>, 2> queues_;
  std::array<std::vector<Entry>, 2> past_;
  std::array<bool, 2> dropped_{{false, false}};
  std::array<Entry, 2> candidate_;
  int pivot_ = -1;
  std::int64_t pivot_time_ = 0, candidate_start_ = 0, candidate_end_ = 0;
};

using Cloud = pcl::PointCloud<pcl::PointXYZRGB>;
struct KeyFrame
{
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  double time = 0;
  gtsam::Pose3 pose = gtsam::Pose3::Identity();
  Eigen::Vector3d velocity = Eigen::Vector3d::Zero();
  Cloud::Ptr cloud;
};
using KeyFrames = std::vector<KeyFrame, Eigen::aligned_allocator<KeyFrame>>;

struct VelocitySeries
{
  std::vector<double> times, values;
};
bool interpolateLinear(const VelocitySeries&, double, double&);
void smoothVelocitySeries(VelocitySeries&);
bool normalizeVelocitySeries(VelocitySeries&);
VelocitySeries buildVelocitySeries(const std::vector<std::vector<double>>&, double);
VelocitySeries buildVelocitySeriesFromSamples(const std::vector<std::vector<double>>&, double);
double scoreVelocityOffset(const VelocitySeries&, const VelocitySeries&, double, int&);
void writeVelocityAlignmentCsv(const std::string&, const std::vector<std::vector<double>>&,
                              const std::vector<std::vector<double>>&,
                              const std::vector<std::vector<double>>&,
                              const std::vector<std::vector<double>>&,
                              const std::string&, double, double);
gtsam::Pose3 poseFromSample(const PoseSample&, bool from_message = true);
gtsam::Pose3 computeSVD(const std::vector<Eigen::Vector3d>&, const std::vector<Eigen::Vector3d>&);
std::ofstream openOutput(const std::string&);
void checkOutput(std::ofstream&, const std::string&);
void writeGlobalMapBinary(const KeyFrames&, const std::string&, const std::function<void()>&);
std::int64_t durationNanoseconds(double);
Timestamp shiftedTimestamp(Timestamp, std::int64_t);
}

struct Backend::Impl
{
  explicit Impl(const BackendConfig&);
  void ensureCollecting() const;
  void matched(const PoseSample&, const CloudSample&);
  void saveKeyFrameAndFactor(const gtsam::Pose3&, double, const Eigen::Vector3d&,
                            const backend_detail::Cloud::Ptr&);
  void run() noexcept;
  void optimize();
  void captureResult();
  void checkCancelled() const;
  void initialAlign();
  void prepareTimeAlignment();
  double calculateDtwTimeOffset(const std::vector<std::vector<double>>&,
                               const std::vector<std::vector<double>>&);
  double estimateVelocityTimeOffset(const std::vector<std::vector<double>>&,
                                   const std::vector<std::vector<double>>&,
                                   const std::vector<std::vector<double>>&,
                                   const std::vector<std::vector<double>>&,
                                   double, double&, double&, int&, std::string&);
  void buildBatchGraph();
  void saveOptimizedGlobalMap();
  void savekeyframescan();
  void writeTumTrajectory(const std::string&);
  void writeRtkTumTrajectory();
  void loadData(const std::string&);
  BackendConfig config;
  mutable std::mutex mutex;
  std::mutex join_mutex;
  std::condition_variable changed;
  std::thread worker;
  std::atomic_bool cancelled{false};
  BackendStatus state = BackendStatus::Collecting;
  std::exception_ptr error;
  std::string error_message;
  std::atomic_size_t frame_count{0};
  std::chrono::steady_clock::time_point last_keyframe_time = std::chrono::steady_clock::now();
  backend_detail::Pairing pairing;
  backend_detail::KeyFrames keyFrames;
  std::vector<BackendTrajectoryPoint> completed_trajectory;
  std::deque<PoseSample> gpsQueue, gpsQueue_B;
  GeographicLib::LocalCartesian gps_trans_;
  bool first_gps = true;
  gtsam::NonlinearFactorGraph gtSAMgraph;
  gtsam::Values initialEstimate;
  gtsam::Pose3 T_imu_rtk;
  bool is_optimized = false;
  std::string debug_optdata_path_, opt_tum_output_path_, livo_tum_before_output_path_;
  std::string rtk_tum_output_path_, opt_vel_output_path_, gps_vel_output_path_;
  std::string global_map_pcd_path_, keyframe_scan_pcd_path_;
};
}
#endif
