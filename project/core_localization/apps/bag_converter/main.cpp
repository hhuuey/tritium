// SPDX-License-Identifier: GPL-2.0-only
#include "options.h"
#include "record_order.h"
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include <csignal>

namespace
{
volatile std::sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }

int run(const fast_livo::bag_converter::Options &options)
{
  using namespace fast_livo;
  using namespace fast_livo::bag_converter;
  rosbag::Bag bag(options.bag, rosbag::bagmode::Read);
  ReplayOrderView view(bag);
  std::set<Role> present;
  for (const auto *connection : view.getConnections())
  {
    const auto found = options.topics.find(connection->topic);
    if (found == options.topics.end()) continue;
    if (!supported(connection->datatype, found->second))
      throw std::runtime_error("选定话题类型不受支持: " + connection->topic + " " + connection->datatype);
    present.insert(found->second);
  }
  for (auto role : options.required)
    if (!present.count(role)) throw std::runtime_error("bag 缺少必需传感器话题，检查 --*-topic / --require");
  io::Writer writer(options.output);
  std::uint64_t selected = 0;
  std::map<Role, std::uint64_t> counts;
  for (std::size_t sequence = 0; sequence < view.record_count(); ++sequence)
  {
    if (stopped) throw std::runtime_error("转换已中断，未发布 manifest；请删除不完整输出后重试");
    const auto owned = view.record(sequence);
    const auto &message = *owned;
    const auto found = options.topics.find(message.getTopic());
    if (found != options.topics.end())
    {
      auto event = convert(message, sequence, found->second);
      event.physical_sequence = view.physical_sequence(sequence);
      writer.append(event);
      ++counts[found->second];
      if (++selected % 10000 == 0) std::cerr << "已转换 " << selected << " 条传感器消息\n";
    }
  }
  for (auto role : options.required)
    if (!counts[role]) throw std::runtime_error("必需传感器没有实际消息，拒绝发布数据集");
  if (!selected) throw std::runtime_error("没有转换任何传感器消息");
  writer.finish();
  std::cout << "转换完成: " << selected << " / " << view.record_count()
            << " 条消息，输出 " << options.output << '\n';
  return 0;
}
}

int main(int argc, char **argv)
{
  if (argc == 2 && std::string(argv[1]) == "--help") { fast_livo::bag_converter::usage(); return 0; }
  std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
  try { return run(fast_livo::bag_converter::parse(argc, argv)); }
  catch (const std::exception &error)
  {
    std::cerr << "转换失败: " << error.what() << '\n';
    return stopped ? 130 : 1;
  }
}
