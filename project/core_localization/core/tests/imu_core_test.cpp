#include "IMU_Processing.h"
#include "fast_livo/sensor_types.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

int main()
{
  const auto timestamp = fast_livo::Timestamp::from_parts(1763437339, 879452003);
  if (timestamp.nanoseconds != 1763437339879452003LL ||
      timestamp.seconds() != 1763437339.0 + 1e-9 * 879452003.0)
  {
    std::cerr << "纳秒转换必须保持旧 ROS1 的浮点秒计算顺序\n";
    return 1;
  }
  bool rejected = false;
  try
  {
    fast_livo::Timestamp::from_parts(9223372036LL, 854775808);
  }
  catch (const std::invalid_argument &)
  {
    rejected = true;
  }
  if (!rejected)
  {
    std::cerr << "必须拒绝 int64 纳秒溢出\n";
    return 1;
  }

  ImuProcess processor;
  processor.set_imu_init_frame_num(1);
  processor.disable_gravity_est();
  LidarMeasureGroup group;
  group.lio_vio_flg = LIO;
  group.lidar_frame_beg_time = 1.0;
  MeasureGroup measure;
  measure.lio_time = 1.1;
  for (int index = 0; index < 3; ++index)
  {
    auto sample = std::make_shared<fast_livo::ImuSample>();
    sample->timestamp = fast_livo::Timestamp::from_parts(1, index * 10000000);
    sample->linear_acceleration.z = -9.81;
    sample->angular_velocity.x = 0.02;
    measure.imu.push_back(sample);
  }
  group.measures.push_back(measure);
  StatesGroup state;
  processor.Process2(group, state, PointCloudXYZI::Ptr(new PointCloudXYZI()), Sophus::SE3());
  if (processor.imu_need_init || state.bias_g.norm() > 1e-12 ||
      std::abs(state.gravity.z() - 9.81) > 1e-12 ||
      std::abs(processor.IMU_mean_acc_norm - 9.81) > 1e-12)
  {
    std::cerr << "中立 IMU 数据必须保留已有初始化结果\n";
    return 1;
  }
  return 0;
}
