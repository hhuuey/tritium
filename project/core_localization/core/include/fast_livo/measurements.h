#pragma once

#include "fast_livo/sensor_types.h"
#include <opencv2/core.hpp>
#include <pcl/PCLPointCloud2.h>
#include <functional>
#include <vector>

namespace fast_livo
{

struct Quaternion
{
  double x = 0, y = 0, z = 0, w = 0;
};

struct CloudSample
{
  using ConstPtr = std::shared_ptr<const CloudSample>;
  Timestamp timestamp;
  std::string frame_id;
  pcl::PCLPointCloud2 cloud;
};

struct LivoxPoint
{
  std::uint32_t offset_time = 0;
  float x = 0, y = 0, z = 0;
  std::uint8_t reflectivity = 0, tag = 0, line = 0;
};

struct LivoxSample
{
  using Ptr = std::shared_ptr<LivoxSample>;
  using ConstPtr = std::shared_ptr<const LivoxSample>;
  Timestamp timestamp;
  std::string frame_id;
  std::uint64_t timebase = 0;
  std::uint32_t point_num = 0;
  std::uint8_t lidar_id = 0;
  std::array<std::uint8_t, 3> rsvd{};
  std::vector<LivoxPoint> points;
};

struct ImageSample
{
  using Ptr = std::shared_ptr<ImageSample>;
  using ConstPtr = std::shared_ptr<const ImageSample>;
  Timestamp timestamp;
  std::string frame_id;
  cv::Mat image;
};

struct GnssSample
{
  using ConstPtr = std::shared_ptr<const GnssSample>;
  struct Time { std::uint32_t week = 0; double tow = 0; } time;
  std::uint8_t fix_type = 0, carr_soln = 0, num_sv = 0;
  bool valid_fix = false, diff_soln = false;
  double latitude = 0, longitude = 0, altitude = 0, height_msl = 0;
  double h_acc = 0, v_acc = 0, p_dop = 0;
  double vel_n = 0, vel_e = 0, vel_d = 0, vel_acc = 0;
};

struct PoseSample
{
  using ConstPtr = std::shared_ptr<const PoseSample>;
  Timestamp timestamp;
  std::string frame_id, child_frame_id;
  Vector3 position, velocity;
  Quaternion orientation;
  std::array<double, 36> covariance{};
};

struct PlaneMarker
{
  int id = 0;
  std::string frame_id, name;
  Timestamp timestamp;
  Vector3 position, scale;
  Quaternion orientation;
  std::array<float, 4> color{};
};

struct Outputs
{
  std::function<Timestamp()> now;
  std::function<void(const std::string &, const PoseSample &)> pose;
  std::function<void(const std::string &, const CloudSample &)> cloud;
  std::function<void(const std::string &, const ImageSample &)> image;
  std::function<void(const std::vector<PoseSample> &)> path;
  std::function<void(const std::vector<PlaneMarker> &)> planes;
  struct Trace
  {
    std::string stage;
    double time = 0;
    std::vector<double> state;
    std::array<std::uint64_t, 4> cloud_hashes{};
    std::array<std::size_t, 4> cloud_sizes{};
    std::uint64_t input_image_hash = 0, gray_image_hash = 0;
  };
  std::function<void(const Trace &)> trace;
};

}
