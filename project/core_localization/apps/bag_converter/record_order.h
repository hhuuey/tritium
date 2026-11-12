// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <rosbag/view.h>
#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace fast_livo::bag_converter
{

// 消费顺序完全沿用 View 的原生迭代器（含同时间戳次序），与 rosbag play 一致。
// protected 索引只用于附带物理序号，不改变回放顺序；不缓存消息载荷。
class ReplayOrderView : public rosbag::View
{
public:
  explicit ReplayOrderView(const rosbag::Bag &bag) : rosbag::View(bag), bag_(bag)
  {
    std::map<const ros::M_string *, Cursor> cursors;
    for (const auto *range : ranges_)
    {
      if (!cursors.emplace(range->connection_info->header.get(),
                           Cursor{range->connection_info, range->begin, range->end}).second)
        throw std::runtime_error("无法唯一识别 rosbag connection");
      for (auto entry = range->begin; entry != range->end; ++entry)
        records_.push_back({range->connection_info, *entry, 0});
    }
    std::sort(records_.begin(), records_.end(), [](const auto &a, const auto &b) {
      return std::tie(a.index.chunk_pos, a.index.offset) < std::tie(b.index.chunk_pos, b.index.offset);
    });
    std::map<std::pair<std::uint64_t, std::uint32_t>, std::uint64_t> physical;
    for (std::size_t i = 0; i < records_.size(); ++i)
      physical.emplace(std::make_pair(records_[i].index.chunk_pos, records_[i].index.offset), i);
    records_.clear();
    for (auto it = rosbag::View::begin(); it != rosbag::View::end(); ++it)
    {
      auto &cursor = cursors.at(it->getConnectionHeader().get());
      if (cursor.next == cursor.end || cursor.next->time != it->getTime())
        throw std::runtime_error("rosbag View 与 connection 索引不一致");
      const auto &index = *cursor.next++;
      records_.push_back({cursor.connection, index, physical.at({index.chunk_pos, index.offset})});
    }
  }

  std::size_t record_count() const { return records_.size(); }
  std::uint64_t physical_sequence(std::size_t position) const { return records_.at(position).physical; }

  std::unique_ptr<rosbag::MessageInstance> record(std::size_t position)
  {
    const auto &entry = records_.at(position);
    return std::unique_ptr<rosbag::MessageInstance>(newMessageInstance(entry.connection, entry.index, bag_));
  }

private:
  struct Record
  {
    const rosbag::ConnectionInfo *connection;
    rosbag::IndexEntry index;
    std::uint64_t physical;
  };
  struct Cursor
  {
    const rosbag::ConnectionInfo *connection;
    std::multiset<rosbag::IndexEntry>::const_iterator next, end;
  };
  const rosbag::Bag &bag_;
  std::vector<Record> records_;
};
}
