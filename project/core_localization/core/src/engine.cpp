#include "fast_livo/engine.h"
#include "fast_livo/backend.h"
#include "LIVMapper.h"
#include <stdexcept>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace fast_livo
{
namespace
{
BackendConfig backend_config(const Config &config)
{
  BackendConfig result;
  result.output_directory = config.laserMapping_outputfilepath;
  result.gps_enabled = config.gps_gps_en;
  result.gps_topic = config.gps_gps_topic;
  result.gps_offset = config.gps_gps_time_offset;
  result.gps_extrinsic = config.gps_extrinsic_T;
  result.rtk_covariance = config.opt_rtk_cov;
  result.livo_rotation_covariance = config.opt_livo2_RPY_cov;
  result.livo_position_covariance = config.opt_livo2_XYZ_cov;
  result.debug_mode = config.opt_debug_mode;
  if(result.debug_mode) result.debug_load_directory = result.output_directory + "/debug/";
  return result;
}

void validate(const Config &config)
{
  if(config.laserMapping_outputfilepath.empty())
    throw std::invalid_argument("算法结果目录不能为空");
  if(config.extrin_calib_extrinsic_T.size()!=3 || config.extrin_calib_extrinsic_R.size()!=9 ||
     config.extrin_calib_Pcl.size()!=3 || config.extrin_calib_Rcl.size()!=9 || config.gps_extrinsic_T.size()!=3)
    throw std::invalid_argument("外参维度不匹配");
  if(config.preprocess_point_filter_num < 1 || config.preprocess_scan_line < 1 ||
     config.preprocess_scan_line > 128 || config.publish_pub_scan_num < 1)
    throw std::invalid_argument("点云过滤、扫描线或发布间隔参数无效");
}
}

struct Engine::Impl
{
  Outputs external;
  std::unique_ptr<Backend> backend;
  std::unique_ptr<LIVMapper> mapper;
  bool finished = false;
  bool optimization_requested = false;
  bool backend_freeze_reported = false;
  std::ofstream trace_file;

  Impl(const Config &config, Outputs outputs) : external(std::move(outputs))
  {
    validate(config);
    if(!external.now) throw std::invalid_argument("运行入口必须提供时钟");
    backend = std::make_unique<Backend>(backend_config(config));
    auto routed = external;
    if(!config.debug_trace_file.empty())
    {
      trace_file.open(config.debug_trace_file);
      if(!trace_file) throw std::runtime_error("无法创建数值追踪文件");
      routed.trace = [this](const Outputs::Trace &trace) {
        trace_file << trace.stage << "," << std::setprecision(17) << trace.time;
        for(auto value : trace.cloud_sizes) trace_file << "," << value;
        for(auto value : trace.cloud_hashes) trace_file << "," << value;
        trace_file << "," << trace.input_image_hash << "," << trace.gray_image_hash;
        for(auto value : trace.state) trace_file << "," << value;
        trace_file << "\n";
        if(!trace_file) throw std::runtime_error("数值追踪写入失败");
        if(external.trace) external.trace(trace);
      };
    }
    routed.pose = [this](const std::string &topic, const PoseSample &sample) {
      if(topic == "/odometry/fast_livo2") forward_backend(sample,&Backend::push_pose);
      if(external.pose) external.pose(topic,sample);
    };
    routed.cloud = [this](const std::string &topic, const CloudSample &sample) {
      if(topic == "/synced_cloud") forward_backend(sample,&Backend::push_cloud);
      if(external.cloud) external.cloud(topic,sample);
    };
    mapper = std::make_unique<LIVMapper>(config,std::move(routed));
  }

  void ensure_running() const
  {
    if(finished) throw std::logic_error("算法实例已经结束");
  }

  template<typename Sample, typename Method>
  void forward_backend(const Sample &sample, Method method)
  {
    try
    {
      (backend.get()->*method)(sample);
    }
    catch(const BackendFrozen &)
    {
      if(!backend_freeze_reported)
      {
        std::cerr << "本轮后端输入已冻结，前端继续运行，后续测量不加入已请求的批优化。\n";
        backend_freeze_reported=true;
      }
    }
  }
};

Engine::Engine(const Config &config, Outputs outputs) : impl_(std::make_unique<Impl>(config,std::move(outputs))) {}
Engine::~Engine() = default;
void Engine::push_imu(ImuSample::ConstPtr sample)
{
  impl_->ensure_running();
  if(!sample) throw std::invalid_argument("IMU 输入不能为空");
  impl_->mapper->imu_cbk(sample);
}
void Engine::push_cloud(CloudSample::ConstPtr sample)
{
  impl_->ensure_running();
  if(!sample) throw std::invalid_argument("点云输入不能为空");
  impl_->mapper->standard_pcl_cbk(sample);
}
void Engine::push_livox(LivoxSample::ConstPtr sample)
{
  impl_->ensure_running();
  if(!sample || sample->point_num != sample->points.size())
    throw std::invalid_argument("Livox 输入或点数不合法");
  impl_->mapper->livox_pcl_cbk(sample);
}
void Engine::push_image(ImageSample::ConstPtr sample)
{
  impl_->ensure_running();
  if(!sample || sample->image.empty()) throw std::invalid_argument("图像输入不能为空");
  impl_->mapper->img_cbk(sample);
}
void Engine::push_gnss(GnssSample::ConstPtr sample)
{
  impl_->ensure_running();
  if(!sample) throw std::invalid_argument("GNSS 输入不能为空");
  impl_->mapper->rtk_cbk(sample);
  impl_->forward_backend(*sample,&Backend::push_gnss);
}
bool Engine::process_one() { impl_->ensure_running(); return impl_->mapper->process_one(); }
void Engine::propagate_imu() { impl_->ensure_running(); impl_->mapper->imu_prop_callback(); }
void Engine::request_optimization()
{
  impl_->ensure_running();
  if(impl_->optimization_requested) return;
  impl_->backend->request_optimization();
  impl_->optimization_requested = true;
}
void Engine::wait_for_optimization()
{
  if(impl_->optimization_requested) impl_->backend->wait();
}
BackendResult Engine::optimization_result() const
{
  return impl_->backend->result();
}
void Engine::finish()
{
  if(impl_->finished) return;
  impl_->mapper->finish();
  impl_->backend->shutdown();
  impl_->mapper.reset();
  impl_->finished=true;
  if(impl_->trace_file.is_open())
  {
    impl_->trace_file.flush();
    if(!impl_->trace_file) throw std::runtime_error("数值追踪刷新失败");
  }
}

}
