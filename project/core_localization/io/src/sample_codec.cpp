// SPDX-License-Identifier: GPL-2.0-only
#include "sample_codec.h"

namespace fast_livo::io::detail
{
namespace
{
template<class T> void header(Encoder &out, const T &s)
{
  out.scalar(s.timestamp.nanoseconds);
  out.text(s.frame_id);
}
template<class T> void header(Decoder &in, T &s)
{
  s.timestamp.nanoseconds = in.scalar<std::int64_t>();
  s.frame_id = in.text();
}
void vector(Encoder &out, const Vector3 &v)
{
  out.scalar(v.x); out.scalar(v.y); out.scalar(v.z);
}
void vector(Decoder &in, Vector3 &v)
{
  v.x = in.scalar<double>(); v.y = in.scalar<double>(); v.z = in.scalar<double>();
}
template<class T, std::size_t N> void array(Encoder &out, const std::array<T, N> &a)
{
  for (auto v : a) out.scalar(v);
}
template<class T, std::size_t N> void array(Decoder &in, std::array<T, N> &a)
{
  for (auto &v : a) v = in.scalar<T>();
}
void write(Encoder &out, const ImuSample &s)
{
  header(out, s);
  vector(out, s.linear_acceleration); vector(out, s.angular_velocity);
  array(out, s.orientation); array(out, s.orientation_covariance);
  array(out, s.angular_velocity_covariance); array(out, s.linear_acceleration_covariance);
}
void read(Decoder &in, ImuSample &s)
{
  header(in, s);
  vector(in, s.linear_acceleration); vector(in, s.angular_velocity);
  array(in, s.orientation); array(in, s.orientation_covariance);
  array(in, s.angular_velocity_covariance); array(in, s.linear_acceleration_covariance);
}
void cloud_shape(const pcl::PCLPointCloud2 &c)
{
  require(static_cast<std::uint64_t>(c.width) * c.point_step <= c.row_step, "点云行步长无效");
  require(static_cast<std::uint64_t>(c.height) * c.row_step == c.data.size(), "点云数据长度不匹配");
  require(c.is_bigendian <= 1 && c.is_dense <= 1, "点云布尔字段无效");
  constexpr std::uint8_t sizes[] = {0, 1, 1, 2, 2, 4, 4, 4, 8};
  for (const auto &f : c.fields)
  {
    require(f.datatype >= 1 && f.datatype <= 8, "不支持的 PointField 数据类型");
    require(f.offset + static_cast<std::uint64_t>(f.count) * sizes[f.datatype] <= c.point_step,
            "点云字段超出 point_step");
  }
}
void write(Encoder &out, const CloudSample &s)
{
  cloud_shape(s.cloud);
  header(out, s);
  const auto &c = s.cloud;
  out.scalar(c.header.seq); out.scalar(c.header.stamp); out.text(c.header.frame_id);
  out.scalar(c.height); out.scalar(c.width);
  require(c.fields.size() <= 65536, "点云字段过多");
  out.scalar<std::uint32_t>(c.fields.size());
  for (const auto &f : c.fields)
  {
    out.text(f.name); out.scalar(f.offset); out.scalar(f.datatype); out.scalar(f.count);
  }
  out.scalar<std::uint8_t>(c.is_bigendian); out.scalar(c.point_step); out.scalar(c.row_step);
  out.scalar<std::uint8_t>(c.is_dense);
  out.scalar<std::uint64_t>(c.data.size()); out.raw(c.data.data(), c.data.size());
}
void read(Decoder &in, CloudSample &s)
{
  header(in, s);
  auto &c = s.cloud;
  c.header.seq = in.scalar<std::uint32_t>(); c.header.stamp = in.scalar<std::uint64_t>();
  c.header.frame_id = in.text();
  c.height = in.scalar<std::uint32_t>(); c.width = in.scalar<std::uint32_t>();
  const auto fields = in.scalar<std::uint32_t>();
  require(fields <= 65536 && fields <= in.remaining() / 13, "点云字段数量无效");
  c.fields.resize(fields);
  for (auto &f : c.fields)
  {
    f.name = in.text(); f.offset = in.scalar<std::uint32_t>();
    f.datatype = in.scalar<std::uint8_t>(); f.count = in.scalar<std::uint32_t>();
  }
  c.is_bigendian = in.boolean(); c.point_step = in.scalar<std::uint32_t>();
  c.row_step = in.scalar<std::uint32_t>(); c.is_dense = in.boolean();
  const auto size = in.scalar<std::uint64_t>();
  require(size <= in.remaining(), "点云载荷截断");
  c.data.resize(size); in.raw(c.data.data(), size);
  cloud_shape(c);
}
void write(Encoder &out, const LivoxSample &s)
{
  require(s.point_num == s.points.size(), "Livox point_num 与点数不一致");
  header(out, s);
  out.scalar(s.timebase); out.scalar(s.point_num); out.scalar(s.lidar_id); array(out, s.rsvd);
  out.scalar<std::uint64_t>(s.points.size());
  for (const auto &p : s.points)
  {
    out.scalar(p.offset_time); out.scalar(p.x); out.scalar(p.y); out.scalar(p.z);
    out.scalar(p.reflectivity); out.scalar(p.tag); out.scalar(p.line);
  }
}
void read(Decoder &in, LivoxSample &s)
{
  header(in, s);
  s.timebase = in.scalar<std::uint64_t>(); s.point_num = in.scalar<std::uint32_t>();
  s.lidar_id = in.scalar<std::uint8_t>(); array(in, s.rsvd);
  const auto count = in.scalar<std::uint64_t>();
  require(count == s.point_num && count <= in.remaining() / 19, "Livox 点数无效");
  s.points.resize(count);
  for (auto &p : s.points)
  {
    p.offset_time = in.scalar<std::uint32_t>(); p.x = in.scalar<float>();
    p.y = in.scalar<float>(); p.z = in.scalar<float>();
    p.reflectivity = in.scalar<std::uint8_t>(); p.tag = in.scalar<std::uint8_t>();
    p.line = in.scalar<std::uint8_t>();
  }
}
void write(Encoder &out, const ImageSample &s)
{
  require(!s.image.empty() && s.image.type() == CV_8UC3 && s.image.dims == 2, "图像必须为非空 BGR8");
  header(out, s);
  out.scalar<std::uint32_t>(s.image.rows); out.scalar<std::uint32_t>(s.image.cols);
  for (int row = 0; row < s.image.rows; ++row)
    out.raw(s.image.ptr(row), static_cast<std::size_t>(s.image.cols) * 3);
}
void read(Decoder &in, ImageSample &s)
{
  header(in, s);
  const auto rows = in.scalar<std::uint32_t>(), cols = in.scalar<std::uint32_t>();
  require(rows && cols && rows <= INT32_MAX && cols <= INT32_MAX, "图像尺寸无效");
  require(static_cast<std::uint64_t>(rows) * cols <= in.remaining() / 3, "图像载荷截断");
  s.image.create(rows, cols, CV_8UC3);
  for (int row = 0; row < s.image.rows; ++row)
    in.raw(s.image.ptr(row), static_cast<std::size_t>(cols) * 3);
}
void write(Encoder &out, const GnssSample &s)
{
  out.scalar(s.time.week); out.scalar(s.time.tow);
  out.scalar(s.fix_type); out.scalar<std::uint8_t>(s.valid_fix); out.scalar<std::uint8_t>(s.diff_soln);
  out.scalar(s.carr_soln); out.scalar(s.num_sv);
  for (double v : {s.latitude, s.longitude, s.altitude, s.height_msl, s.h_acc, s.v_acc,
                   s.p_dop, s.vel_n, s.vel_e, s.vel_d, s.vel_acc}) out.scalar(v);
}
void read(Decoder &in, GnssSample &s)
{
  s.time.week = in.scalar<std::uint32_t>(); s.time.tow = in.scalar<double>();
  s.fix_type = in.scalar<std::uint8_t>(); s.valid_fix = in.boolean(); s.diff_soln = in.boolean();
  s.carr_soln = in.scalar<std::uint8_t>(); s.num_sv = in.scalar<std::uint8_t>();
  for (double *v : {&s.latitude, &s.longitude, &s.altitude, &s.height_msl, &s.h_acc, &s.v_acc,
                    &s.p_dop, &s.vel_n, &s.vel_e, &s.vel_d, &s.vel_acc}) *v = in.scalar<double>();
}
template<class T> Sample read_sample(Decoder &in)
{
  auto sample = std::make_shared<T>();
  read(in, *sample);
  return std::shared_ptr<const T>(sample);
}
}

std::vector<std::uint8_t> encode(const Sample &sample)
{
  Encoder out;
  std::visit([&](const auto &s) { require(bool(s), "空传感器样本"); write(out, *s); }, sample);
  return std::move(out.bytes);
}

Sample decode(Kind kind, const std::vector<std::uint8_t> &bytes)
{
  Decoder in(bytes);
  Sample sample;
  switch (kind)
  {
    case Kind::imu: sample = read_sample<ImuSample>(in); break;
    case Kind::cloud: sample = read_sample<CloudSample>(in); break;
    case Kind::livox: sample = read_sample<LivoxSample>(in); break;
    case Kind::image: sample = read_sample<ImageSample>(in); break;
    case Kind::gnss: sample = read_sample<GnssSample>(in); break;
    default: throw std::runtime_error("未知传感器种类");
  }
  require(!in.remaining(), "载荷包含未识别尾部数据");
  return sample;
}

void validate_header(const Event &event)
{
  require(event.record_time.nanoseconds >= 0 && !event.topic.empty() && !event.source_type.empty(),
          "事件缺少有效的时间、话题或源消息类型");
  require(event.header_time.nanoseconds >= 0, "header 时间为负");
  if (!event.has_header)
    require(event.header_time.nanoseconds == 0 && event.header_sequence == 0, "无 header 事件不能设置 header 字段");
  std::visit([&](const auto &s) {
    require(bool(s), "空传感器样本");
    using T = std::decay_t<decltype(*s)>;
    if constexpr (!std::is_same_v<T, GnssSample>)
      require(event.has_header && s->timestamp.nanoseconds == event.header_time.nanoseconds,
              "事件 header 与传感器时间不一致");
    else
      require(!event.has_header, "GNSS PVT 消息没有 ROS header");
  }, event.sample);
}
}
