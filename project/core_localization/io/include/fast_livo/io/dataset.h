// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "fast_livo/measurements.h"
#include <filesystem>
#include <variant>

namespace fast_livo::io
{

using Sample = std::variant<ImuSample::ConstPtr, CloudSample::ConstPtr, LivoxSample::ConstPtr,
                            ImageSample::ConstPtr, GnssSample::ConstPtr>;

enum class Kind : std::uint8_t { imu = 1, cloud = 2, livox = 3, image = 4, gnss = 5 };

struct Event
{
  // sequence 是 rosbag::View 回放序号，允许因未选择的话题而有空洞；不是 header.seq。
  std::uint64_t sequence = 0;
  // 原 bag 中的物理记录序号；未知时为 UINT64_MAX，不用于决定消费顺序。
  std::uint64_t physical_sequence = UINT64_MAX;
  Timestamp record_time;
  bool has_header = false;
  Timestamp header_time;
  std::uint32_t header_sequence = 0;
  std::string topic;
  std::string source_type;
  Sample sample;
  Kind kind() const { return static_cast<Kind>(sample.index() + 1); }
};

struct Summary
{
  std::uint64_t events = 0;
  // 顺序依次为 IMU、PointCloud2、Livox、BGR8 图像、GNSS PVT。
  std::array<std::uint64_t, 5> counts{};
};

class Writer
{
public:
  // 只接受不存在或空的目录，不覆盖已有数据；finish 后才发布 manifest。
  explicit Writer(const std::filesystem::path &directory);
  ~Writer();
  Writer(const Writer &) = delete;
  Writer &operator=(const Writer &) = delete;
  void append(const Event &event);
  void finish();
  const Summary &summary() const;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class Reader
{
public:
  // 限制单事件载荷及索引大小，防止损坏长度造成无界分配。
  explicit Reader(const std::filesystem::path &directory, std::uint64_t max_event_bytes = 1073741824);
  ~Reader();
  Reader(const Reader &) = delete;
  Reader &operator=(const Reader &) = delete;
  bool next(Event &event);
  const Summary &summary() const;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}
