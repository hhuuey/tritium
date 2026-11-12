#pragma once

#include "fast_livo/measurements.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fast_livo_ros2
{
class SensorStatistics
{
public:
  explicit SensorStatistics(bool enabled = true) : enabled_(enabled) {}
  bool enabled() const { return enabled_; }
  void record(const fast_livo::ImuSample &sample) { mark(0, sample.timestamp); }
  void record(const fast_livo::CloudSample &sample) { mark(1, sample.timestamp); }
  void record(const fast_livo::LivoxSample &sample) { mark(2, sample.timestamp); }
  void record(const fast_livo::ImageSample &sample) { mark(3, sample.timestamp); }
  void record(const fast_livo::GnssSample &sample)
  {
    if (!enabled_) return;
    ++counts_[4];
    week_ = sample.time.week;
    tow_ = sample.time.tow;
  }
  std::string json() const
  {
    std::ostringstream result;
    result << "{\"kinds\":[\"imu\",\"cloud\",\"livox\",\"image\",\"gnss\"],\"counts\":[";
    for (std::size_t i = 0; i < counts_.size(); ++i)
      result << (i ? "," : "") << counts_[i];
    result << "],\"last_stamp_ns\":[";
    for (std::size_t i = 0; i < counts_.size(); ++i)
    {
      if (i) result << ',';
      if (counts_[i] && i != 4) result << last_[i];
      else result << "null";
    }
    result << "],\"gnss_week\":" << week_ << ",\"gnss_tow\":" << std::setprecision(17) << tow_ << '}';
    return result.str();
  }
  void save(const std::string &path) const
  {
    if (path.empty()) return;
    if (std::filesystem::exists(path) || std::filesystem::exists(path + ".pending"))
      throw std::runtime_error("拒绝覆盖统计文件: " + path);
    std::ofstream file(path + ".pending");
    file << json() << '\n';
    file.close();
    if (!file) throw std::runtime_error("无法写入统计文件: " + path);
    std::filesystem::rename(path + ".pending", path);
  }
private:
  void mark(std::size_t kind, fast_livo::Timestamp time)
  {
    if (!enabled_) return;
    ++counts_[kind];
    last_[kind] = time.nanoseconds;
  }
  bool enabled_;
  std::array<std::uint64_t, 5> counts_{};
  std::array<std::int64_t, 5> last_{};
  std::uint32_t week_ = 0;
  double tow_ = 0;
};
}
