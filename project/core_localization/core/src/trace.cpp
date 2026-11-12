#include "LIVMapper.h"
#include <cstring>

namespace
{
void append_byte(std::uint64_t &hash, std::uint8_t value)
{
  hash = (hash ^ value) * 1099511628211ULL;
}

std::uint64_t cloud_hash(const PointCloudXYZI::Ptr &cloud)
{
  std::uint64_t hash = 14695981039346656037ULL;
  if(!cloud) return hash;
  for(const auto &point : cloud->points)
  {
    const float fields[] = {point.x,point.y,point.z,point.intensity,point.curvature,
                            point.normal_x,point.normal_y,point.normal_z};
    for(float value : fields)
    {
      std::uint32_t bits;
      std::memcpy(&bits,&value,sizeof(bits));
      for(int byte=0;byte<4;++byte) append_byte(hash,(bits>>(byte*8))&255);
    }
  }
  return hash;
}

std::uint64_t image_hash(const cv::Mat &image)
{
  std::uint64_t hash = 14695981039346656037ULL;
  for(int row=0;row<image.rows;++row)
    for(std::size_t column=0;column<image.cols*image.elemSize();++column)
      append_byte(hash,image.ptr<std::uint8_t>(row)[column]);
  return hash;
}
}

void LIVMapper::trace_state(const std::string &stage)
{
  if(!outputs_.trace) return;
  fast_livo::Outputs::Trace trace;
  trace.stage=stage;
  trace.time=keyframe_time;
  for(int row=0;row<3;++row)
    for(int column=0;column<3;++column) trace.state.push_back(_state.rot_end(row,column));
  for(const auto *value : {&_state.pos_end,&_state.vel_end,&_state.bias_g,&_state.bias_a,&_state.gravity})
    for(int index=0;index<3;++index) trace.state.push_back((*value)[index]);
  trace.state.push_back(_state.inv_expo_time);
  for(int row=0;row<DIM_STATE;++row)
    for(int column=0;column<DIM_STATE;++column) trace.state.push_back(_state.cov(row,column));
  const PointCloudXYZI::Ptr clouds[] = {LidarMeasures.pcl_proc_cur,feats_undistort,feats_down_body,pcl_w_wait_pub};
  for(int index=0;index<4;++index)
  {
    trace.cloud_hashes[index]=cloud_hash(clouds[index]);
    trace.cloud_sizes[index]=clouds[index] ? clouds[index]->size() : 0;
  }
  if(!LidarMeasures.measures.empty()) trace.input_image_hash=image_hash(LidarMeasures.measures.back().img);
  if(vio_manager && vio_manager->new_frame_) trace.gray_image_hash=image_hash(vio_manager->new_frame_->img_);
  outputs_.trace(trace);
}
