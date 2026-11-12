// SPDX-License-Identifier: GPL-2.0-only
#include "conversion.h"
#include <cv_bridge/cv_bridge.h>
#include <gnss_comm/GnssPVTSolnMsg.h>
#include <livox_ros_driver/CustomMsg.h>
#include <sensor_msgs/CompressedImage.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/PointCloud2.h>
#include <algorithm>

namespace fast_livo::bag_converter
{
namespace
{
template<class T> void header(io::Event &event, T &sample, const std_msgs::Header &h)
{
  event.has_header = true;
  event.header_time = Timestamp::from_parts(h.stamp.sec, h.stamp.nsec);
  event.header_sequence = h.seq;
  sample.timestamp = event.header_time;
  sample.frame_id = h.frame_id;
}

template<class T> auto instantiate(const rosbag::MessageInstance &message)
{
  auto ptr = message.instantiate<T>();
  if (!ptr) throw std::runtime_error("消息 MD5/类型与选定传感器不匹配: " + message.getTopic());
  return ptr;
}

ImuSample::ConstPtr imu(const rosbag::MessageInstance &message, io::Event &event)
{
  const auto m = instantiate<sensor_msgs::Imu>(message);
  auto s = std::make_shared<ImuSample>();
  header(event, *s, m->header);
  s->linear_acceleration = {m->linear_acceleration.x, m->linear_acceleration.y, m->linear_acceleration.z};
  s->angular_velocity = {m->angular_velocity.x, m->angular_velocity.y, m->angular_velocity.z};
  s->orientation = {{m->orientation.x, m->orientation.y, m->orientation.z, m->orientation.w}};
  std::copy(m->orientation_covariance.begin(), m->orientation_covariance.end(), s->orientation_covariance.begin());
  std::copy(m->angular_velocity_covariance.begin(), m->angular_velocity_covariance.end(),
            s->angular_velocity_covariance.begin());
  std::copy(m->linear_acceleration_covariance.begin(), m->linear_acceleration_covariance.end(),
            s->linear_acceleration_covariance.begin());
  return s;
}

CloudSample::ConstPtr cloud(const rosbag::MessageInstance &message, io::Event &event)
{
  const auto m = instantiate<sensor_msgs::PointCloud2>(message);
  auto s = std::make_shared<CloudSample>();
  header(event, *s, m->header);
  auto &c = s->cloud;
  c.header.seq = m->header.seq;
  c.header.stamp = m->header.stamp.toNSec() / 1000;
  c.header.frame_id = m->header.frame_id;
  c.height = m->height; c.width = m->width;
  c.fields.resize(m->fields.size());
  for (std::size_t i = 0; i < c.fields.size(); ++i)
  {
    c.fields[i].name = m->fields[i].name; c.fields[i].offset = m->fields[i].offset;
    c.fields[i].datatype = m->fields[i].datatype; c.fields[i].count = m->fields[i].count;
  }
  c.is_bigendian = m->is_bigendian; c.point_step = m->point_step; c.row_step = m->row_step;
  c.data.assign(m->data.begin(), m->data.end()); c.is_dense = m->is_dense;
  return s;
}

LivoxSample::ConstPtr livox(const rosbag::MessageInstance &message, io::Event &event)
{
  const auto m = instantiate<livox_ros_driver::CustomMsg>(message);
  auto s = std::make_shared<LivoxSample>();
  header(event, *s, m->header);
  s->timebase = m->timebase; s->point_num = m->point_num; s->lidar_id = m->lidar_id;
  std::copy(m->rsvd.begin(), m->rsvd.end(), s->rsvd.begin());
  s->points.reserve(m->points.size());
  for (const auto &p : m->points)
    s->points.push_back({p.offset_time, p.x, p.y, p.z, p.reflectivity, p.tag, p.line});
  return s;
}

ImageSample::ConstPtr image(const rosbag::MessageInstance &message, io::Event &event)
{
  auto s = std::make_shared<ImageSample>();
  if (message.isType<sensor_msgs::Image>())
  {
    const auto m = instantiate<sensor_msgs::Image>(message);
    header(event, *s, m->header);
    s->image = cv_bridge::toCvCopy(m, "bgr8")->image;
  }
  else
  {
    const auto m = instantiate<sensor_msgs::CompressedImage>(message);
    header(event, *s, m->header);
    s->image = cv_bridge::toCvCopy(m, "bgr8")->image;
  }
  if (s->image.empty()) throw std::runtime_error("图像解码为空: " + message.getTopic());
  return s;
}

GnssSample::ConstPtr gnss(const rosbag::MessageInstance &message)
{
  const auto m = instantiate<gnss_comm::GnssPVTSolnMsg>(message);
  auto s = std::make_shared<GnssSample>();
  s->time.week = m->time.week; s->time.tow = m->time.tow;
  s->fix_type = m->fix_type; s->valid_fix = m->valid_fix; s->diff_soln = m->diff_soln;
  s->carr_soln = m->carr_soln; s->num_sv = m->num_sv;
  s->latitude = m->latitude; s->longitude = m->longitude; s->altitude = m->altitude;
  s->height_msl = m->height_msl; s->h_acc = m->h_acc; s->v_acc = m->v_acc; s->p_dop = m->p_dop;
  s->vel_n = m->vel_n; s->vel_e = m->vel_e; s->vel_d = m->vel_d; s->vel_acc = m->vel_acc;
  return s;
}
}

bool supported(const std::string &type, Role role)
{
  switch (role)
  {
    case Role::imu: return type == "sensor_msgs/Imu";
    case Role::lidar: return type == "sensor_msgs/PointCloud2" || type == "livox_ros_driver/CustomMsg";
    case Role::image: return type == "sensor_msgs/Image" || type == "sensor_msgs/CompressedImage";
    case Role::gnss: return type == "gnss_comm/GnssPVTSolnMsg";
  }
  return false;
}

io::Event convert(const rosbag::MessageInstance &message, std::uint64_t sequence, Role role)
{
  if (!supported(message.getDataType(), role))
    throw std::runtime_error("选定话题存在不支持的消息类型: " + message.getTopic() + " " + message.getDataType());
  io::Event event;
  event.sequence = sequence;
  event.record_time = Timestamp::from_parts(message.getTime().sec, message.getTime().nsec);
  event.topic = message.getTopic(); event.source_type = message.getDataType();
  switch (role)
  {
    case Role::imu: event.sample = imu(message, event); break;
    case Role::lidar:
      if (message.isType<sensor_msgs::PointCloud2>()) event.sample = cloud(message, event);
      else event.sample = livox(message, event);
      break;
    case Role::image: event.sample = image(message, event); break;
    case Role::gnss: event.sample = gnss(message); break;
  }
  return event;
}
}
