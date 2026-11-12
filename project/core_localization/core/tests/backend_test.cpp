/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
This file is subject to the accompanying LICENSE file.
*/
#include "fast_livo/backend.h"
#include <GeographicLib/LocalCartesian.hpp>
#include <pcl/conversions.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace
{
using namespace fast_livo;
namespace fs = std::filesystem;
void require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}
template<typename Error = std::exception, typename Function> std::string mustThrow(Function function)
{
  try { function(); }
  catch (const Error& exception) { return exception.what(); }
  throw std::runtime_error("操作必须显式报告指定类型的失败");
}
struct Fixture
{
  fs::path directory;
  std::vector<PoseSample> poses;
  std::vector<GnssSample> gnss;
  std::vector<CloudSample> clouds;
  explicit Fixture(fs::path path) : directory(std::move(path))
  {
    GeographicLib::LocalCartesian local(22.0, 114.0, 10.0);
    for (int i = 0; i < 8; ++i)
    {
      GnssSample g;
      g.time.week = 2000;
      g.time.tow = 1000.0 + i;
      g.latitude = 22.0 + i * 0.00001;
      g.longitude = 114.0 + i * i * 0.000002;
      g.altitude = 10.0 + i * 0.1;
      g.h_acc = 0.2; g.v_acc = 0.3;
      gnss.push_back(g);
      PoseSample p;
      p.timestamp = Timestamp::from_seconds(2000.0 * 604800.0 + g.time.tow + 315964800.0 - 18.0);
      local.Forward(g.latitude, g.longitude, g.altitude, p.position.x, p.position.y, p.position.z);
      // 原始 IMU 轨迹相对 ENU 平移，杆臂为 [1,0,0]。
      p.position.x += 9.0;
      p.position.y -= 5.0;
      p.position.z += 2.0;
      p.orientation.w = 1.0;
      poses.push_back(p);
      pcl::PointCloud<pcl::PointXYZRGB> cloud;
      pcl::PointXYZRGB point;
      point.x = 1.0; point.y = 2.0; point.z = 3.0;
      point.r = 10; point.g = 20; point.b = 30;
      cloud.push_back(point);
      CloudSample sample;
      sample.timestamp = p.timestamp;
      pcl::toPCLPointCloud2(cloud, sample.cloud);
      clouds.push_back(sample);
    }
  }
  BackendConfig config(const std::string& name) const
  {
    BackendConfig c;
    c.output_directory = (directory / name).string();
    c.gps_enabled = true;
    c.gps_extrinsic = {1, 0, 0};
    c.idle_seconds = 0;
    return c;
  }
  void push(Backend& backend, bool with_gnss = true) const
  {
    for (std::size_t i = 0; i < poses.size(); ++i)
    {
      if (with_gnss) backend.push_gnss(gnss[i]);
      if (i % 2 == 0)
      {
        backend.push_pose(poses[i]); backend.push_cloud(clouds[i]);
      }
      else
      {
        backend.push_cloud(clouds[i]); backend.push_pose(poses[i]);
      }
    }
  }
  void saveDebug(const fs::path& path) const
  {
    fs::create_directories(path / "pcd");
    std::ofstream odom(path / "odom.txt"), cov(path / "cov.txt"), rtk(path / "rtk.txt");
    odom << std::setprecision(17); cov << std::setprecision(17); rtk << std::setprecision(17);
    for (std::size_t i = 0; i < poses.size(); ++i)
    {
      const auto& p = poses[i];
      odom << p.timestamp.seconds() << " " << p.position.x << " " << p.position.y << " "
           << p.position.z << " 0 0 0 1\n";
      cov << p.timestamp.seconds() << "\n";
      rtk << p.timestamp.seconds() << " " << p.position.x - 9 << " " << p.position.y + 5 << " "
          << p.position.z - 2 << " 0 0 0 0.2 0.3\n";
      std::ostringstream name;
      name << std::setw(6) << std::setfill('0') << i << ".pcd";
      require(pcl::io::savePCDFile((path / "pcd" / name.str()).string(), clouds[i].cloud) == 0,
              "无法写入调试测试点云");
    }
  }
};

void checkOutputs(const BackendConfig& config)
{
  fs::path root(config.output_directory);
  for (const auto& name : {"TUM/opt_trajectory_after.txt", "TUM/livo_trajectory_before.txt",
                           "TUM/rtk_trajectory_time_aligned.txt"})
  {
    std::ifstream file(root / name);
    std::string line;
    std::size_t count = 0;
    while (std::getline(file, line))
      if (!line.empty() && line[0] != '#') ++count;
    require(count == 8, "TUM 必须包含所有已匹配帧");
  }
  pcl::PointCloud<pcl::PointXYZRGB> before, after;
  require(pcl::io::loadPCDFile((root / "global_pcd/before_optimization.pcd").string(), before) == 0 &&
          pcl::io::loadPCDFile((root / "global_pcd/after_optimization.pcd").string(), after) == 0,
          "优化前后地图必须均存在");
  require(before.size() == 8 && after.size() == 8, "地图点数发生变化");
  require(std::abs(before[0].x - 1.0) < 1e-3 && std::abs(after[0].x) < 1e-3,
          "TUM 天线位姿与最终 IMU 地图必须保留原杆臂转换顺序");
}

void checkResult(const Backend& backend, const BackendConfig& config)
{
  auto result = backend.result();
  require(result.keyframe_count == 8 && result.trajectory.size() == 8, "结果关键帧数量错误");
  require(result.before_map_file == config.output_directory + "/global_pcd/before_optimization.pcd" &&
          result.after_map_file == config.output_directory + "/global_pcd/after_optimization.pcd" &&
          fs::exists(result.before_map_file) && fs::exists(result.after_map_file), "结果地图引用无效");
  std::ostringstream rendered;
  rendered << "# timestamp tx ty tz qx qy qz qw\n" << std::fixed << std::setprecision(6);
  for (const auto& point : result.trajectory)
    rendered << point.timestamp.seconds() << " " << point.position.x << " " << point.position.y << " "
             << point.position.z << " " << point.orientation.x << " " << point.orientation.y << " "
             << point.orientation.z << " " << point.orientation.w << "\n";
  std::ifstream tum(config.output_directory + "/TUM/opt_trajectory_after.txt");
  std::ostringstream original;
  original << tum.rdbuf();
  require(rendered.str() == original.str(), "结果 API 必须与原 TUM 轨迹逐字符一致");
  result.trajectory.front().position.x += 1000;
  require(backend.result().trajectory.front().position.x != result.trajectory.front().position.x,
          "结果必须为独立副本，不能通过返回值修改后端状态");
}
}

int main()
{
  const fs::path root = "backend-unit-test-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  try
  {
    Fixture fixture(root);
    auto config = fixture.config("success");
    auto invalid_config = fixture.config("invalid-extrinsic");
    invalid_config.gps_extrinsic.clear();
    mustThrow([&] { Backend invalid(invalid_config); });
    invalid_config.gps_extrinsic = {0, 0};
    mustThrow([&] { Backend invalid(invalid_config); });
    invalid_config.gps_extrinsic = {0, 0, 0};
    Backend zero_lever(invalid_config);
    require(zero_lever.status() == BackendStatus::Collecting, "显式零杆臂必须有效");
    Backend backend(config);
    require(backend.status() == BackendStatus::Collecting, "构造不得启动优化");
    for (double invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
      auto invalid_options = fixture.config("nonfinite");
      invalid_options.gps_offset = invalid;
      mustThrow<std::invalid_argument>([&] { Backend rejected(invalid_options); });
      invalid_options.gps_offset = 0;
      invalid_options.gps_extrinsic[0] = invalid;
      mustThrow<std::invalid_argument>([&] { Backend rejected(invalid_options); });
      auto invalid_pose = fixture.poses.front();
      invalid_pose.position.x = invalid;
      mustThrow<std::invalid_argument>([&] { backend.push_pose(invalid_pose); });
      invalid_pose = fixture.poses.front();
      invalid_pose.orientation.x = invalid;
      mustThrow<std::invalid_argument>([&] { backend.push_pose(invalid_pose); });
      auto invalid_gnss = fixture.gnss.front();
      invalid_gnss.vel_e = invalid;
      mustThrow<std::invalid_argument>([&] { backend.push_gnss(invalid_gnss); });
    }
    mustThrow<std::invalid_argument>([&] { backend.push_pose(PoseSample{}); });
    mustThrow([&] { backend.wait(); });
    mustThrow([&] { backend.result(); });
    fixture.push(backend);
    require(backend.matched_frame_count() == 8, "相同时间戳必须全部匹配");
    backend.request_optimization();
    backend.wait();
    require(backend.status() == BackendStatus::Completed && backend.failure().empty(), "优化未完成");
    checkOutputs(config);
    checkResult(backend, config);
    mustThrow<BackendFrozen>([&] { backend.push_pose(fixture.poses[0]); });
    mustThrow<BackendFrozen>([&] { backend.push_cloud(fixture.clouds[0]); });
    mustThrow<BackendFrozen>([&] { backend.push_gnss(fixture.gnss[0]); });
    mustThrow([&] { backend.request_optimization(); });
    backend.shutdown(); backend.shutdown();

    Backend missing(fixture.config("missing"));
    fixture.push(missing, false);
    missing.request_optimization();
    mustThrow([&] { missing.wait(); });
    require(missing.status() == BackendStatus::Failed && !missing.failure().empty(), "失败不可静默");
    require(mustThrow<std::runtime_error>([&] { missing.push_pose(PoseSample{}); }) == missing.failure(),
            "失败后的输入必须重抛原始错误，不能改成冻结或输入校验异常");
    mustThrow([&] { missing.result(); });

    Backend unmatched(fixture.config("unmatched"));
    unmatched.push_pose(fixture.poses.front());
    unmatched.request_optimization();
    mustThrow([&] { unmatched.wait(); });
    require(unmatched.matched_frame_count() == 0, "不得把未配对 pose 当成关键帧");

    auto cancel_config = fixture.config("cancelled");
    cancel_config.idle_seconds = 60;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
      Backend cancelled(cancel_config);
      fixture.push(cancelled);
      cancelled.request_optimization();
      mustThrow<std::invalid_argument>([&] { cancelled.push_pose(PoseSample{}); });
      mustThrow([&] { cancelled.result(); });
      const auto start = std::chrono::steady_clock::now();
      cancelled.shutdown();
      require(std::chrono::steady_clock::now() - start < std::chrono::seconds(2), "取消必须唤醒 idle 等待");
      mustThrow([&] { cancelled.wait(); });
      require(cancelled.status() == BackendStatus::Cancelled, "取消状态错误");
      require(mustThrow<std::runtime_error>([&] { cancelled.push_pose(PoseSample{}); }).find("取消") != std::string::npos,
              "取消后的输入必须显式报告取消，不能改成冻结或输入校验异常");
      mustThrow([&] { cancelled.result(); });
    }

    auto plot_config = fixture.config("bad-plot");
    plot_config.plot_script = (root / "missing-plot.py").string();
    Backend bad_plot(plot_config);
    fixture.push(bad_plot);
    bad_plot.request_optimization();
    mustThrow([&] { bad_plot.wait(); });
    require(bad_plot.status() == BackendStatus::Failed, "指定不存在绘图脚本不可默认为成功");

    auto hanging_config = fixture.config("cancel-plot");
    hanging_config.plot_script = (root / "hanging-plot.py").string();
    {
      std::ofstream script(hanging_config.plot_script);
      script << "import pathlib, sys, time\n"
                "pathlib.Path(sys.argv[2], 'plot.started').write_text('ready')\n"
                "time.sleep(60)\n";
    }
    Backend hanging(hanging_config);
    fixture.push(hanging);
    hanging.request_optimization();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!fs::exists(fs::path(hanging_config.output_directory) / "debug/plot.started") &&
           std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(fs::exists(fs::path(hanging_config.output_directory) / "debug/plot.started"), "绘图子进程未启动");
    mustThrow<BackendFrozen>([&] { hanging.push_pose(fixture.poses[0]); });
    const auto cancel_start = std::chrono::steady_clock::now();
    hanging.shutdown();
    require(std::chrono::steady_clock::now() - cancel_start < std::chrono::seconds(2),
            "取消必须终止显式绘图子进程并 join");
    mustThrow([&] { hanging.wait(); });

    auto output_config = fixture.config("bad-output");
    Backend bad_output(output_config);
    fixture.push(bad_output);
    fs::remove_all(fs::path(output_config.output_directory) / "TUM");
    std::ofstream(fs::path(output_config.output_directory) / "TUM") << "blocked";
    bad_output.request_optimization();
    mustThrow([&] { bad_output.wait(); });
    require(bad_output.status() == BackendStatus::Failed, "输出失败不得静默成功");

    auto debug_config = fixture.config("debug-output");
    debug_config.debug_mode = true;
    debug_config.debug_load_directory = (root / "debug-input").string();
    fixture.saveDebug(debug_config.debug_load_directory);
    Backend debug(debug_config);
    require(debug.matched_frame_count() == 8, "调试载入必须恢复所有关键帧");
    debug.request_optimization();
    debug.wait();
    checkOutputs(debug_config);
    checkResult(debug, debug_config);
    fs::remove(fs::path(debug_config.debug_load_directory) / "pcd/000000.pcd");
    mustThrow([&] { Backend invalid(debug_config); });
    fs::remove_all(root);
  }
  catch (const std::exception& exception)
  {
    std::cerr << exception.what() << "\n";
    fs::remove_all(root);
    return 1;
  }
  return 0;
}
