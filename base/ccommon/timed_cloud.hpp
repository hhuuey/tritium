#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <Eigen/Geometry>
#include <pcl/PCLPointCloud2.h>
#include <pcl/io/pcd_io.h>

// Offline calibration uses the same timestamp convention as localization. FLOAT64
// storage contains uint64 nanosecond bits, not a floating-point timestamp.
namespace slam_data {
struct TimedPoint {
  float x, y, z, yaw, intensity;
  std::uint16_t ring;
  std::uint64_t timestamp_ns;
};
inline const pcl::PCLPointField& requireField(const pcl::PCLPointCloud2& cloud,
    const char* name,std::uint32_t size,std::uint8_t type) {
  auto found=std::find_if(cloud.fields.begin(),cloud.fields.end(),
    [&](const auto& field){return field.name==name;});
  if(found==cloud.fields.end() || found->count!=1 || found->offset+size>cloud.point_step || (type && found->datatype!=type))
    throw std::runtime_error(std::string("Invalid or missing PCD field: ")+name);
  return *found;
}
template<typename T> inline T readRaw(const std::uint8_t* point,const pcl::PCLPointField& field) {
  T value; std::memcpy(&value,point+field.offset,sizeof(value)); return value;
}
inline std::vector<TimedPoint> readTimedCloud(const std::filesystem::path& path,bool auxiliary=true,bool float_timestamp_only=false) {
  pcl::PCLPointCloud2 cloud;
  if(pcl::io::loadPCDFile(path.string(),cloud)<0 || cloud.data.empty())
    throw std::runtime_error("Cannot load timed PCD file: "+path.string());
  if(cloud.is_bigendian) throw std::runtime_error("Big-endian timed PCD is unsupported");
  const auto& x=requireField(cloud,"x",4,pcl::PCLPointField::FLOAT32);
  const auto& y=requireField(cloud,"y",4,pcl::PCLPointField::FLOAT32);
  const auto& z=requireField(cloud,"z",4,pcl::PCLPointField::FLOAT32);
  const auto& intensity=requireField(cloud,"intensity",4,pcl::PCLPointField::FLOAT32);
  const auto& time=requireField(cloud,"timestamp",8,float_timestamp_only?pcl::PCLPointField::FLOAT64:0);
  const auto* yaw=auxiliary?&requireField(cloud,"yaw",4,pcl::PCLPointField::FLOAT32):nullptr;
  const auto* ring=auxiliary?&requireField(cloud,"ring",2,pcl::PCLPointField::UINT16):nullptr;
  if(cloud.row_step<std::size_t(cloud.width)*cloud.point_step || cloud.data.size()<std::size_t(cloud.height)*cloud.row_step)
    throw std::runtime_error("PCD payload is shorter than POINTS: "+path.string());
  std::vector<TimedPoint> points; points.reserve(std::size_t(cloud.width)*cloud.height);
  for(std::size_t row=0;row<cloud.height;++row) for(std::size_t col=0;col<cloud.width;++col) {
    auto* p=cloud.data.data()+row*cloud.row_step+col*cloud.point_step;
    points.push_back({readRaw<float>(p,x),readRaw<float>(p,y),readRaw<float>(p,z),
      yaw?readRaw<float>(p,*yaw):0.0f,readRaw<float>(p,intensity),
      ring?readRaw<std::uint16_t>(p,*ring):std::uint16_t(0),readRaw<std::uint64_t>(p,time)});
  }
  return points;
}
struct RigidPose { Eigen::Quaterniond q; Eigen::Vector3d t; };
inline RigidPose interpolateRigid(const RigidPose& left,const RigidPose& right,double alpha) {
  return {left.q.slerp(alpha,right.q).normalized(),(1-alpha)*left.t+alpha*right.t};
}
}
