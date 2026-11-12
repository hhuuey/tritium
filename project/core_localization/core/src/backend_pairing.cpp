/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2009, Willow Garage, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * 改编自 ROS1 Noetic message_filters/sync_policies/approximate_time.h，
 * 仅特化双输入并移除 ROS 依赖，保留其队列、pivot 和虚拟搜索语义。
 */
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace fast_livo::backend_detail
{
namespace
{
// ROS Duration 的乘法以秒乘后 round 到纳秒；不能直接比较未取整的浮点数。
std::int64_t age(std::int64_t duration)
{
  const auto whole_seconds = duration / 1000000000 - (duration % 1000000000 < 0 ? 1 : 0);
  const auto nanoseconds = duration - whole_seconds * 1000000000;
  const double seconds = static_cast<double>(whole_seconds) + 1e-9 * static_cast<double>(nanoseconds);
  const double scaled = seconds * 1.1;
  const auto whole = static_cast<std::int64_t>(std::floor(scaled));
  return whole * 1000000000 + static_cast<std::int64_t>(std::round((scaled - whole) * 1e9));
}
}

Pairing::Pairing(Callback callback) : callback_(std::move(callback)) {}

void Pairing::add(const PoseSample& pose)
{
  add(0, {pose.timestamp.nanoseconds, std::make_shared<PoseSample>(pose), nullptr});
}

void Pairing::add(const CloudSample& cloud)
{
  add(1, {cloud.timestamp.nanoseconds, nullptr, std::make_shared<CloudSample>(cloud)});
}

void Pairing::add(int stream, Entry entry)
{
  auto& queue = queues_[stream];
  queue.push_back(std::move(entry));
  if (queue.size() == 1 && !queues_[1 - stream].empty())
    process();
  if (queue.size() + past_[stream].size() <= 10)
    return;
  recover(0, past_[0].size());
  recover(1, past_[1].size());
  queue.pop_front();
  dropped_[stream] = true;
  if (pivot_ != -1)
  {
    pivot_ = -1;
    candidate_ = {};
    process();
  }
}

void Pairing::move(int stream)
{
  past_[stream].push_back(queues_[stream].front());
  queues_[stream].pop_front();
}

void Pairing::recover(int stream, std::size_t count)
{
  auto& past = past_[stream];
  while (count-- > 0)
  {
    queues_[stream].push_front(past.back());
    past.pop_back();
  }
}

std::array<std::int64_t, 2> Pairing::times(bool virtual_search) const
{
  std::array<std::int64_t, 2> result;
  for (int i = 0; i < 2; ++i)
    result[i] = virtual_search && queues_[i].empty()
                    ? std::max(past_[i].back().time, pivot_time_)
                    : queues_[i].front().time;
  return result;
}

void Pairing::publish()
{
  callback_(*candidate_[0].pose, *candidate_[1].cloud);
  candidate_ = {};
  pivot_ = -1;
  for (int i = 0; i < 2; ++i)
  {
    recover(i, past_[i].size());
    queues_[i].pop_front();
  }
}

void Pairing::process()
{
  while (!queues_[0].empty() && !queues_[1].empty())
  {
    const auto t = times(false);
    // 同时间时 start 取第一个输入，end 取最后一个输入，与 ROS 的 XOR 分支一致。
    const int start = t[1] < t[0] ? 1 : 0;
    const int end = t[1] >= t[0] ? 1 : 0;
    dropped_[1 - end] = false;
    if (pivot_ == -1)
    {
      // ROS 默认 DURATION_MAX = int32 秒上界 + 999999999 纳秒。
      if (t[end] - t[start] > 2147483647999999999LL || dropped_[end])
      {
        queues_[start].pop_front();
        continue;
      }
      candidate_ = {queues_[0].front(), queues_[1].front()};
      past_[0].clear();
      past_[1].clear();
      candidate_start_ = t[start];
      candidate_end_ = t[end];
      pivot_ = end;
      pivot_time_ = t[end];
      move(start);
    }
    else
    {
      if (age(t[end] - candidate_end_) < t[start] - candidate_start_)
      {
        candidate_ = {queues_[0].front(), queues_[1].front()};
        past_[0].clear();
        past_[1].clear();
        candidate_start_ = t[start];
        candidate_end_ = t[end];
      }
      move(start);
    }
    if (start == pivot_ || age(t[end] - candidate_end_) >= pivot_time_ - candidate_start_)
    {
      publish();
      continue;
    }
    if (!queues_[0].empty() && !queues_[1].empty())
      continue;
    std::array<std::size_t, 2> virtual_moves{{0, 0}};
    while (true)
    {
      const auto v = times(true);
      const int vstart = v[1] < v[0] ? 1 : 0;
      const int vend = v[1] >= v[0] ? 1 : 0;
      if (age(v[vend] - candidate_end_) >= pivot_time_ - candidate_start_)
      {
        publish();
        break;
      }
      if (age(v[vend] - candidate_end_) < v[vstart] - candidate_start_)
      {
        recover(0, virtual_moves[0]);
        recover(1, virtual_moves[1]);
        break;
      }
      move(vstart);
      ++virtual_moves[vstart];
    }
  }
}
}
