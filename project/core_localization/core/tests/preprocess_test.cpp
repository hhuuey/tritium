#include "preprocess.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition,const char *message)
{
  if(!condition) throw std::runtime_error(message);
}

Preprocess configured(int type)
{
  Preprocess processor;
  processor.set(false,type,0.01,1);
  processor.blind_sqr=0.0001;
  processor.N_SCANS=6;
  return processor;
}

template<typename Point,typename Initialize>
PointCloudXYZI::Ptr standard(int type,Initialize initialize)
{
  pcl::PointCloud<Point> input;
  for(int index=0;index<12;++index)
  {
    Point point{};
    point.x=2.0f+index*0.1f;point.y=1.0f;point.z=0.3f;
    initialize(point,index);
    input.push_back(point);
  }
  auto packet=std::make_shared<fast_livo::CloudSample>();
  packet->timestamp=fast_livo::Timestamp::from_parts(100,0);
  pcl::toPCLPointCloud2(input,packet->cloud);
  auto processor=configured(type);
  PointCloudXYZI::Ptr output(new PointCloudXYZI);
  processor.process(packet,output);
  require(output->size()==12,"标准点云转换后点数不一致");
  return output;
}

void check_time(const PointCloudXYZI::Ptr &cloud)
{
  for(std::size_t index=0;index<cloud->size();++index)
  {
    require(std::abs((*cloud)[index].x-(2.0f+index*0.1f))<1e-6f,"点序或位置被改变");
    require(std::abs((*cloud)[index].curvature-index)<1e-4,"点时间单位或时间偏移错误");
  }
}

void run()
{
  auto livox=std::make_shared<fast_livo::LivoxSample>();
  livox->timestamp=fast_livo::Timestamp::from_parts(100,0);
  for(int index=0;index<12;++index)
    livox->points.push_back({static_cast<std::uint32_t>(index*10000),2.0f+index*0.1f,
                            1.0f,0.3f,50,0x10,static_cast<std::uint8_t>(index%6)});
  livox->point_num=livox->points.size();
  auto avia=configured(AVIA);
  PointCloudXYZI::Ptr cloud(new PointCloudXYZI);
  avia.process(livox,cloud);
  require(cloud->size()==12,"AVIA 非特征模式必须保留首点");
  require(cloud->front().curvature==0 &&
          std::abs(cloud->back().curvature-0.11f)<1e-6f,"Livox 纳秒到毫秒转换错误");
  require(cloud->front().intensity==50,"Livox 反射率被丢弃");
  livox->points.back().offset_time=5000000;
  avia.process(livox,cloud);
  require(std::abs(cloud->back().curvature-(0.1f+0.004166667f))<1e-6f,
          "AVIA 原有大时间跳变修正规则被改变");

  check_time(standard<ouster_ros::Point>(OUST64,[](auto &point,int index) {
    point.intensity=50;point.t=index*1000000;point.ring=index%6;
  }));
  check_time(standard<velodyne_ros::Point>(VELO16,[](auto &point,int index) {
    point.intensity=50;point.time=index*1000.0f;point.ring=index%6;
  }));
  check_time(standard<xt32_ros::Point>(XT32,[](auto &point,int index) {
    point.intensity=50;point.timestamp=100.0+index*0.001;point.ring=index%6;
  }));
  auto pandar=standard<Pandar128_ros::Point>(PANDAR128,[](auto &point,int index) {
    point.intensity=128;point.timestamp=100.0+index*0.001;point.ring=index%6;
  });
  check_time(pandar);
  require(std::abs(pandar->front().intensity-128.0f/255.0f)<1e-6f,"Pandar 强度归一化被改变");
  check_time(standard<robosense_ros::Point>(ROBOSENSE,[](auto &point,int index) {
    point.intensity=50;point.timestamp=100.0+index*0.001;point.ring=index%6;
  }));
  auto rgb=standard<pcl::PointXYZRGB>(L515,[](auto &point,int index) {
    point.r=10+index;point.g=20+index;point.b=30+index;
  });
  require(rgb->front().normal_x==10 && rgb->front().normal_y==20 && rgb->front().normal_z==30,
          "L515 原有颜色字段映射被改变");
  require(rgb->back().curvature==0,"L515 不应伪造扫描内时间");
}
}

int main()
{
  try { run(); return 0; }
  catch(const std::exception &error) { std::cerr << error.what() << "\n"; return 1; }
}
