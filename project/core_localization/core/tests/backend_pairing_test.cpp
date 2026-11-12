/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
This file is subject to the accompanying LICENSE file.
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace
{
using Pair = std::pair<std::int64_t, std::int64_t>;
struct Fixture
{
  std::vector<Pair> pairs;
  fast_livo::backend_detail::Pairing pairing{[this](const auto& pose, const auto& cloud) {
    pairs.emplace_back(pose.timestamp.nanoseconds, cloud.timestamp.nanoseconds);
  }};
  void pose(std::int64_t time)
  {
    fast_livo::PoseSample sample;
    sample.timestamp.nanoseconds = time;
    pairing.add(sample);
  }
  void cloud(std::int64_t time)
  {
    fast_livo::CloudSample sample;
    sample.timestamp.nanoseconds = time;
    pairing.add(sample);
  }
  void expect(std::initializer_list<Pair> expected) const
  {
    if (pairs != std::vector<Pair>(expected))
      throw std::runtime_error("ApproximateTime 配对结果不符");
  }
};
}

int main()
{
  try
  {
    Fixture same;
    same.cloud(100);
    same.pose(100);
    same.pose(200);
    same.cloud(200);
    same.pose(200);
    same.cloud(200);
    same.expect({{100, 100}, {200, 200}, {200, 200}});

    Fixture future;
    future.pose(100);
    future.cloud(150);
    future.expect({});
    future.pose(150);
    future.expect({{150, 150}});

    Fixture closest;
    closest.pose(1000000000);
    closest.cloud(1100000000);
    closest.pose(1200000000);
    closest.expect({{1000000000, 1100000000}});

    Fixture no_slop;
    no_slop.pose(1000000000);
    no_slop.cloud(100000000000);
    no_slop.expect({});
    no_slop.pose(200000000000);
    no_slop.expect({{1000000000, 100000000000}});

    Fixture overflow;
    for (int i = 1; i <= 11; ++i)
      overflow.pose(i * 100);
    overflow.cloud(100);
    overflow.expect({});
    overflow.cloud(200);
    overflow.expect({{200, 200}});

    Fixture delayed;
    for (int i = 1; i <= 10; ++i)
      delayed.cloud(i * 100);
    for (int i = 1; i <= 10; ++i)
      delayed.pose(i * 100);
    if (delayed.pairs.size() != 10)
      throw std::runtime_error("队列容量 10 内的同时间戳消息不能丢失");
  }
  catch (const std::exception& exception)
  {
    std::cerr << exception.what() << "\n";
    return 1;
  }
  return 0;
}
