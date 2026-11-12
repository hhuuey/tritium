// SPDX-License-Identifier: GPL-2.0-only
#include "fast_livo/engine.h"
#include "fast_livo/io/dataset.h"
#include <csignal>
#include <iostream>
#include <map>
#include <cmath>
#include <limits>
#include <poll.h>
#include <unistd.h>

namespace
{
volatile std::sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }

struct Options
{
  std::string config, camera, dataset, output, trace;
  bool optimize_on_eof = false;
  double duration = std::numeric_limits<double>::infinity();
};

Options parse(int argc, char **argv)
{
  Options options;
  std::map<std::string, std::string *> values{
      {"--config", &options.config}, {"--camera", &options.camera},
      {"--dataset", &options.dataset}, {"--output", &options.output}};
  for (int i = 1; i < argc; ++i)
  {
    const std::string key = argv[i];
    if(key=="--duration")
    {
      if(i+1==argc || std::isfinite(options.duration)) throw std::runtime_error("--duration 缺失或重复");
      const std::string value=argv[++i];
      std::size_t consumed=0;
      options.duration=std::stod(value,&consumed);
      if(consumed!=value.size()) throw std::runtime_error("--duration 必须是完整数值");
      if(!std::isfinite(options.duration) || options.duration<=0) throw std::runtime_error("--duration 必须为正有限秒数");
      continue;
    }
    if(key=="--trace")
    {
      if(i+1==argc || !options.trace.empty()) throw std::runtime_error("--trace 缺失或重复");
      options.trace=argv[++i];
      if(options.trace.empty()) throw std::runtime_error("--trace 路径不能为空");
      continue;
    }
    if (key == "--optimize-on-eof")
    {
      if (options.optimize_on_eof) throw std::runtime_error("重复 --optimize-on-eof");
      options.optimize_on_eof = true;
      continue;
    }
    const auto found = values.find(key);
    if (found == values.end() || i + 1 == argc) throw std::runtime_error("参数未知或缺值: " + key);
    if (!found->second->empty()) throw std::runtime_error("重复参数: " + key);
    *found->second = argv[++i];
  }
  for (const auto &item : values)
    if (item.second->empty()) throw std::runtime_error("必须提供参数: " + item.first);
  return options;
}

bool request_at_eof(bool automatic)
{
  if (stopped) return false;
  if (automatic) return true;
  if (!isatty(STDIN_FILENO))
  {
    std::cerr << "已到 EOF；未显式请求优化，直接保存收尾（可用 --optimize-on-eof）。\n";
    return false;
  }
  std::cerr << "已到 EOF。按 Enter 显式优化，输入其他内容后 Enter 跳过；Ctrl-C 保存退出。\n";
  while (!stopped)
  {
    pollfd input{STDIN_FILENO, POLLIN, 0};
    const int ready = poll(&input, 1, 200);
    if (ready < 0 && !stopped) throw std::runtime_error("等待终端输入失败");
    if (ready <= 0) continue;
    if (input.revents & (POLLERR | POLLHUP | POLLNVAL)) return false;
    if (input.revents & POLLIN)
    {
      std::string line;
      return bool(std::getline(std::cin, line)) && line.empty() && !stopped;
    }
  }
  return false;
}

int run(const Options &options)
{
  using namespace fast_livo;
  io::Reader reader(options.dataset);
  const auto dataset_path = std::filesystem::canonical(options.dataset);
  auto output_path = std::filesystem::weakly_canonical(options.output);
  auto relative = output_path.lexically_relative(dataset_path);
  if (relative.empty() || *relative.begin() != "..")
    throw std::runtime_error("--output 不能位于只读 dataset 目录内");
  Timestamp clock;
  Outputs outputs;
  outputs.now = [&clock] { return clock; };
  auto config=load_config(options.config,options.camera,options.output);
  config.debug_trace_file=options.trace;
  Engine engine(config, std::move(outputs));
  std::uint64_t processed = 0;
  try
  {
    io::Event event;
    std::int64_t first_time=-1;
    while (!stopped && reader.next(event))
    {
      if(first_time<0) first_time=event.record_time.nanoseconds;
      if(static_cast<double>(event.record_time.nanoseconds-first_time)*1e-9>=options.duration) break;
      clock = event.record_time;
      std::visit([&](const auto &sample) {
        using T = std::decay_t<decltype(*sample)>;
        if constexpr (std::is_same_v<T, ImuSample>) engine.push_imu(sample);
        else if constexpr (std::is_same_v<T, CloudSample>) engine.push_cloud(sample);
        else if constexpr (std::is_same_v<T, LivoxSample>) engine.push_livox(sample);
        else if constexpr (std::is_same_v<T, ImageSample>) engine.push_image(sample);
        else if constexpr (std::is_same_v<T, GnssSample>) engine.push_gnss(sample);
      }, event.sample);
      // 不按 header 时间重排；每个事件后把同步器驱动至当前不能继续处理。
      while (!stopped && engine.process_one()) {}
      engine.propagate_imu();
      if (++processed % 10000 == 0) std::cerr << "已回放 " << processed << " 条事件\n";
    }
    if (request_at_eof(options.optimize_on_eof))
    {
      engine.request_optimization();
      engine.wait_for_optimization();
    }
  }
  catch (...)
  {
    engine.finish();
    throw;
  }
  engine.finish();
  std::cout << "回放" << (stopped ? "中断" : "结束") << ": " << processed << " 条事件\n";
  return stopped ? 130 : 0;
}
}

int main(int argc, char **argv)
{
  if (argc == 2 && std::string(argv[1]) == "--help")
  {
    std::cout << "fast_livo_offline --config FILE --camera FILE --dataset DIRECTORY --output DIRECTORY\n"
                 "  [--optimize-on-eof] [--duration SECONDS] [--trace FILE]\n";
    return 0;
  }
  std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
  try { return run(parse(argc, argv)); }
  catch (const std::exception &error) { std::cerr << "离线运行失败: " << error.what() << '\n'; return 1; }
}
