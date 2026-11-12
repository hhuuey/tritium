#pragma once

#include "fast_livo/measurements.h"
#include "fast_livo/config.h"
#include "fast_livo/backend.h"
#include <memory>

namespace fast_livo
{

class Engine
{
public:
  Engine(const Config &config, Outputs outputs);
  ~Engine();
  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;
  void push_imu(ImuSample::ConstPtr sample);
  void push_cloud(CloudSample::ConstPtr sample);
  void push_livox(LivoxSample::ConstPtr sample);
  void push_image(ImageSample::ConstPtr sample);
  void push_gnss(GnssSample::ConstPtr sample);
  bool process_one();
  void propagate_imu();
  void request_optimization();
  void wait_for_optimization();
  BackendResult optimization_result() const;
  void finish();
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

Config load_config(const std::string &algorithm_file, const std::string &camera_file, const std::string &output_dir);

}
