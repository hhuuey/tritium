// SPDX-License-Identifier: GPL-2.0-only
#include "../../apps/bag_converter/conversion.h"
#include "../../apps/bag_converter/record_order.h"
#include <cv_bridge/cv_bridge.h>
#include <gnss_comm/GnssPVTSolnMsg.h>
#include <livox_ros_driver/CustomMsg.h>
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/PointCloud2.h>
#include <std_msgs/String.h>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <chrono>

namespace
{
using namespace fast_livo;
using fast_livo::bag_converter::Role;

void check(bool condition, const char *message)
{
  if (!condition) throw std::runtime_error(message);
}
template<class T> void stamp(T &message, int index)
{
  message.header.seq = index * 13;
  message.header.stamp = ros::Time(900 - index, 123456789 + index);
  message.header.frame_id = "fixture_" + std::to_string(index);
}
template<class T> void compare_header(const io::Event &event, const T &message)
{
  check(event.has_header && event.header_time.nanoseconds == static_cast<std::int64_t>(message.header.stamp.toNSec()) &&
        event.header_sequence == message.header.seq, "bag header 元数据改变");
}

void fixture(const std::filesystem::path &root)
{
  rosbag::Bag bag((root / "fixture.bag").string(), rosbag::bagmode::Write);
  bag.setChunkThreshold(1024);
  bag.setCompression(rosbag::compression::LZ4);
  sensor_msgs::Imu imu;
  stamp(imu, 1);
  imu.orientation.x = 0.1; imu.orientation.y = 0.2; imu.orientation.z = 0.3; imu.orientation.w = 0.4;
  imu.linear_acceleration.x = 1; imu.linear_acceleration.y = 2; imu.linear_acceleration.z = -3;
  imu.angular_velocity.x = -4; imu.angular_velocity.y = 5; imu.angular_velocity.z = 6;
  for (int i = 0; i < 9; ++i)
  {
    imu.orientation_covariance[i] = i - 1.0; imu.angular_velocity_covariance[i] = i + 0.1;
    imu.linear_acceleration_covariance[i] = i + 0.2;
  }
  bag.write("/livox/imu", ros::Time(1000, 1), imu);
  std_msgs::String ignored; ignored.data = "不相关话题，不能成为成功占位事件";
  bag.write("/unrelated", ros::Time(1000, 2), ignored);
  sensor_msgs::PointCloud2 cloud;
  stamp(cloud, 2);
  cloud.height = 2; cloud.width = 2; cloud.point_step = 16; cloud.row_step = 35;
  cloud.is_bigendian = true; cloud.is_dense = false; cloud.fields.resize(2);
  cloud.fields[0].name = "x"; cloud.fields[0].offset = 0; cloud.fields[0].datatype = 7; cloud.fields[0].count = 1;
  cloud.fields[1].name = "custom"; cloud.fields[1].offset = 4;
  cloud.fields[1].datatype = 4; cloud.fields[1].count = 3;
  cloud.data.resize(70);
  for (std::size_t i = 0; i < cloud.data.size(); ++i) cloud.data[i] = i * 7;
  bag.write("/livox/lidar", ros::Time(999, 3), cloud);
  livox_ros_driver::CustomMsg livox;
  stamp(livox, 3);
  livox.timebase = 123456789012345ULL; livox.point_num = 1; livox.lidar_id = 8;
  livox.rsvd = {{1, 2, 255}}; livox.points.resize(1);
  auto &point = livox.points.front();
  point.offset_time = 12345678; point.x = 1.5; point.y = -2.25; point.z = 3.75;
  point.reflectivity = 199; point.tag = 33; point.line = 5;
  bag.write("/livox/custom", ros::Time(1000, 1), livox);
  sensor_msgs::Image raw;
  stamp(raw, 4);
  raw.encoding = "mono8"; raw.width = 3; raw.height = 2; raw.step = 4;
  raw.data = {1, 2, 3, 99, 4, 5, 6, 88};
  bag.write("/left_camera/image", ros::Time(1000, 5), raw);
  sensor_msgs::CompressedImage compressed;
  stamp(compressed, 5);
  compressed.header.stamp = ros::Time(0, 0);
  compressed.format = "rgb8; png compressed bgr8";
  cv::Mat color(2, 3, CV_8UC3, cv::Scalar(11, 222, 33));
  color.at<cv::Vec3b>(1, 2) = {254, 0, 127};
  check(cv::imencode(".png", color, compressed.data), "无法生成测试 PNG");
  bag.write("/left_camera/image/compressed", ros::TIME_MIN, compressed);
  gnss_comm::GnssPVTSolnMsg gnss;
  gnss.time.week = 2400; gnss.time.tow = 123456.123456789;
  gnss.fix_type = 3; gnss.valid_fix = true; gnss.diff_soln = true; gnss.carr_soln = 2; gnss.num_sv = 42;
  double value = -8.125;
  for (double *v : {&gnss.latitude, &gnss.longitude, &gnss.altitude, &gnss.height_msl, &gnss.h_acc,
                   &gnss.v_acc, &gnss.p_dop, &gnss.vel_n, &gnss.vel_e, &gnss.vel_d, &gnss.vel_acc})
  { *v = value; value += 1.25; }
  bag.write("/ublox_driver/receiver_pvt", ros::Time(998, 7), gnss);
  bag.close();
  rosbag::Bag wrong((root / "wrong.bag").string(), rosbag::bagmode::Write);
  wrong.write("/livox/imu", ros::Time(1000), ignored);
}

void compare(const rosbag::MessageInstance &message, const io::Event &event)
{
  check(event.topic == message.getTopic() && event.source_type == message.getDataType() &&
        event.record_time.nanoseconds == static_cast<std::int64_t>(message.getTime().toNSec()), "bag 事件元数据改变");
  if (message.isType<sensor_msgs::Imu>())
  {
    const auto m = message.instantiate<sensor_msgs::Imu>();
    const auto &s = *std::get<ImuSample::ConstPtr>(event.sample);
    compare_header(event, *m);
    check(s.frame_id == m->header.frame_id && s.orientation[0] == m->orientation.x &&
          s.orientation[1] == m->orientation.y && s.orientation[2] == m->orientation.z &&
          s.orientation[3] == m->orientation.w && s.linear_acceleration.x == m->linear_acceleration.x &&
          s.linear_acceleration.y == m->linear_acceleration.y && s.linear_acceleration.z == m->linear_acceleration.z &&
          s.angular_velocity.x == m->angular_velocity.x && s.angular_velocity.y == m->angular_velocity.y &&
          s.angular_velocity.z == m->angular_velocity.z &&
          std::equal(s.orientation_covariance.begin(), s.orientation_covariance.end(), m->orientation_covariance.begin()) &&
          std::equal(s.angular_velocity_covariance.begin(), s.angular_velocity_covariance.end(),
                     m->angular_velocity_covariance.begin()) &&
          std::equal(s.linear_acceleration_covariance.begin(), s.linear_acceleration_covariance.end(),
                     m->linear_acceleration_covariance.begin()), "IMU 转换字段丢失");
  }
  else if (message.isType<sensor_msgs::PointCloud2>())
  {
    const auto m = message.instantiate<sensor_msgs::PointCloud2>();
    const auto &c = std::get<CloudSample::ConstPtr>(event.sample)->cloud;
    compare_header(event, *m);
    check(c.header.seq == m->header.seq && c.header.stamp == m->header.stamp.toNSec() / 1000 &&
          c.header.frame_id == m->header.frame_id && c.height == m->height && c.width == m->width &&
          c.point_step == m->point_step && c.row_step == m->row_step && c.is_bigendian == m->is_bigendian &&
          c.is_dense == m->is_dense && c.data == m->data && c.fields.size() == m->fields.size(),
          "PointCloud2 转换布局或字节改变");
    for (std::size_t i = 0; i < c.fields.size(); ++i)
      check(c.fields[i].name == m->fields[i].name && c.fields[i].offset == m->fields[i].offset &&
            c.fields[i].datatype == m->fields[i].datatype && c.fields[i].count == m->fields[i].count,
            "PointCloud2 扩展字段丢失");
  }
  else if (message.isType<livox_ros_driver::CustomMsg>())
  {
    const auto m = message.instantiate<livox_ros_driver::CustomMsg>();
    const auto &s = *std::get<LivoxSample::ConstPtr>(event.sample);
    compare_header(event, *m);
    check(s.frame_id == m->header.frame_id && s.timebase == m->timebase && s.point_num == m->point_num &&
          s.lidar_id == m->lidar_id && std::equal(s.rsvd.begin(), s.rsvd.end(), m->rsvd.begin()) &&
          s.points.size() == m->points.size(), "Livox 元数据丢失");
    for (std::size_t i = 0; i < s.points.size(); ++i)
    {
      const auto &p = s.points[i]; const auto &q = m->points[i];
      check(p.offset_time == q.offset_time && p.x == q.x && p.y == q.y && p.z == q.z &&
            p.reflectivity == q.reflectivity && p.tag == q.tag && p.line == q.line, "Livox 点字段丢失");
    }
  }
  else if (message.isType<gnss_comm::GnssPVTSolnMsg>())
  {
    const auto m = message.instantiate<gnss_comm::GnssPVTSolnMsg>();
    const auto &s = *std::get<GnssSample::ConstPtr>(event.sample);
    check(!event.has_header && event.header_time.nanoseconds == 0 &&
          s.time.week == m->time.week && s.time.tow == m->time.tow && s.fix_type == m->fix_type &&
          s.valid_fix == m->valid_fix && s.diff_soln == m->diff_soln && s.carr_soln == m->carr_soln &&
          s.num_sv == m->num_sv && s.latitude == m->latitude && s.longitude == m->longitude &&
          s.altitude == m->altitude && s.height_msl == m->height_msl && s.h_acc == m->h_acc &&
          s.v_acc == m->v_acc && s.p_dop == m->p_dop && s.vel_n == m->vel_n &&
          s.vel_e == m->vel_e && s.vel_d == m->vel_d && s.vel_acc == m->vel_acc, "GNSS PVT 字段丢失");
  }
  else
  {
    cv_bridge::CvImagePtr decoded;
    if (message.isType<sensor_msgs::Image>())
    {
      const auto m = message.instantiate<sensor_msgs::Image>();
      compare_header(event, *m); decoded = cv_bridge::toCvCopy(m, "bgr8");
    }
    else
    {
      const auto m = message.instantiate<sensor_msgs::CompressedImage>();
      compare_header(event, *m); decoded = cv_bridge::toCvCopy(m, "bgr8");
    }
    const auto &s = *std::get<ImageSample::ConstPtr>(event.sample);
    check(s.frame_id == decoded->header.frame_id && s.image.size() == decoded->image.size() &&
          s.image.type() == CV_8UC3 && cv::norm(s.image, decoded->image, cv::NORM_INF) == 0,
          "图像与 cv_bridge bgr8 不等价");
  }
}

void run(const std::filesystem::path &root)
{
  check(!std::filesystem::exists(root), "测试目录已存在，拒绝覆盖");
  std::filesystem::create_directories(root);
  fixture(root);
  rosbag::Bag bag((root / "fixture.bag").string(), rosbag::bagmode::Read);
  bag_converter::ReplayOrderView view(bag);
  rosbag::View baseline(bag);
  std::vector<std::string> playback_topics;
  for (const auto &message : baseline) playback_topics.push_back(message.getTopic());
  io::Writer writer(root / "dataset");
  const std::vector<std::string> original_topics{
      "/livox/imu", "/unrelated", "/livox/lidar", "/livox/custom",
      "/left_camera/image", "/left_camera/image/compressed", "/ublox_driver/receiver_pvt"};
  check(view.record_count() == original_topics.size(), "记录遍历遗漏消息（包括最小记录时间）");
  check(playback_topics != original_topics, "测试必须区分物理写入序和 rosbag play 消费序");
  for (std::size_t sequence = 0; sequence < view.record_count(); ++sequence)
  {
    const auto owned = view.record(sequence);
    const auto &message = *owned;
    check(message.getTopic() == playback_topics[sequence], "未保持 rosbag play 消费顺序或同时间戳次序");
    const auto physical = std::find(original_topics.begin(), original_topics.end(), message.getTopic());
    check(view.physical_sequence(sequence) == static_cast<std::uint64_t>(physical - original_topics.begin()),
          "原物理记录序号丢失");
    Role role = Role::image;
    if (message.getTopic() == "/livox/imu") role = Role::imu;
    if (message.getTopic() == "/livox/lidar" || message.getTopic() == "/livox/custom") role = Role::lidar;
    if (message.getTopic() == "/ublox_driver/receiver_pvt") role = Role::gnss;
    if (message.getTopic() == "/unrelated")
    {
      bool rejected = false;
      try { bag_converter::convert(message, sequence, Role::imu); }
      catch (const std::exception &) { rejected = true; }
      check(rejected, "未知消息不能作为成功占位");
    }
    else
    {
      auto event = bag_converter::convert(message, sequence, role);
      event.physical_sequence = view.physical_sequence(sequence);
      writer.append(event);
    }
  }
  writer.finish();
  io::Reader reader(root / "dataset");
  io::Event event;
  for (std::size_t sequence = 0; sequence < view.record_count(); ++sequence)
  {
    const auto owned = view.record(sequence);
    const auto &message = *owned;
    if (message.getTopic() != "/unrelated")
    {
      check(reader.next(event) && event.sequence == sequence, "原 bag 事件序号或顺序改变");
      check(event.physical_sequence == view.physical_sequence(sequence), "物理记录元数据改变");
      compare(message, event);
    }
  }
  check(!reader.next(event) && reader.summary().events == 6, "转换事件计数错误");
}
}

int main(int argc, char **argv)
{
  if (argc > 2) { std::cerr << "用法: bag_converter_test [NEW_OUTPUT_DIRECTORY]\n"; return 2; }
  const auto root = argc == 2 ? std::filesystem::path(argv[1]) :
      std::filesystem::temp_directory_path() / ("fast-livo-converter-" +
          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try
  {
    run(root);
    if (argc == 1) std::filesystem::remove_all(root);
    std::cout << "真实 ROS1 bag 六类消息全字段转换及往返通过\n";
    return 0;
  }
  catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
