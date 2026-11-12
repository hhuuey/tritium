#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
struct Pose
{
  std::string timestamp;
  std::array<double, 3> position;
  std::array<double, 4> quaternion;
};

std::vector<Pose> trajectory(const std::string &path)
{
  std::ifstream file(path);
  if (!file) throw std::runtime_error("无法读取轨迹: " + path);
  std::vector<Pose> poses;
  std::string line;
  while (std::getline(file, line))
  {
    std::istringstream input(line);
    Pose pose;
    if (!(input >> pose.timestamp) || pose.timestamp.front() == '#') continue;
    for (auto &value : pose.position)
      if (!(input >> value) || !std::isfinite(value)) throw std::runtime_error("轨迹位置无效");
    for (auto &value : pose.quaternion)
      if (!(input >> value) || !std::isfinite(value)) throw std::runtime_error("轨迹四元数无效");
    poses.push_back(pose);
  }
  if (poses.empty()) throw std::runtime_error("轨迹为空");
  return poses;
}

void save(const std::string &path, const std::string &report)
{
  if (std::filesystem::exists(path)) throw std::runtime_error("拒绝覆盖报告: " + path);
  std::ofstream output(path);
  output << report << '\n';
  output.close();
  if (!output) throw std::runtime_error("报告写入失败: " + path);
  std::cout << report << '\n';
}

std::string compare(const std::string &reference_path, const std::string &actual_path)
{
  const auto reference = trajectory(reference_path), actual = trajectory(actual_path);
  if (reference.size() != actual.size()) throw std::runtime_error("轨迹帧数不相等，禁止插值/补帧比较");
  double sum_position = 0, max_position = 0, sum_angle = 0, max_angle = 0;
  for (std::size_t i = 0; i < reference.size(); ++i)
  {
    const auto &r = reference[i], &a = actual[i];
    if (r.timestamp != a.timestamp) throw std::runtime_error("轨迹时间轴不同，禁止按近邻伪匹配");
    double squared = 0, dot = 0, norm_r = 0, norm_a = 0;
    for (std::size_t j = 0; j < 3; ++j) squared += (r.position[j] - a.position[j]) * (r.position[j] - a.position[j]);
    for (std::size_t j = 0; j < 4; ++j)
    {
      dot += r.quaternion[j] * a.quaternion[j];
      norm_r += r.quaternion[j] * r.quaternion[j];
      norm_a += a.quaternion[j] * a.quaternion[j];
    }
    if (norm_r == 0 || norm_a == 0 || !std::isfinite(norm_r) || !std::isfinite(norm_a) || !std::isfinite(dot))
      throw std::runtime_error("轨迹四元数范数无效");
    // 比较仅处理四元数符号等价，不对轨迹做刚体对齐或拟合。
    const double angle = 2 * std::acos(std::clamp(std::abs(dot) / std::sqrt(norm_r * norm_a), 0.0, 1.0));
    sum_position += squared;
    sum_angle += angle * angle;
    max_position = std::max(max_position, std::sqrt(squared));
    max_angle = std::max(max_angle, angle);
  }
  if (!std::isfinite(sum_position) || !std::isfinite(sum_angle))
    throw std::runtime_error("轨迹误差数值溢出，拒绝输出伪有效指标");
  std::ostringstream result;
  result << std::setprecision(17) << "{\"count\":" << actual.size()
         << ",\"exact_timestamp_axis\":true,\"position_rmse_m\":" << std::sqrt(sum_position / actual.size())
         << ",\"position_max_m\":" << max_position << ",\"orientation_rmse_rad\":"
         << std::sqrt(sum_angle / actual.size()) << ",\"orientation_max_rad\":" << max_angle
         << ",\"alignment\":\"none\",\"interpolation\":false}";
  return result.str();
}

std::string map_report(const std::string &path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("地图不存在: " + path);
  std::vector<std::uint64_t> sizes, counts;
  std::uint64_t points = 0, width = 0, height = 0;
  std::string line, encoding;
  while (std::getline(input, line))
  {
    std::istringstream fields(line);
    std::string key;
    fields >> key;
    if (key == "POINTS") fields >> points;
    if (key == "WIDTH") fields >> width;
    if (key == "HEIGHT") fields >> height;
    if (key == "SIZE" || key == "COUNT")
    {
      auto &target = key == "SIZE" ? sizes : counts;
      std::uint64_t value;
      while (fields >> value) target.push_back(value);
    }
    if (key == "DATA") { fields >> encoding; break; }
  }
  if (encoding != "binary" || sizes.empty() || sizes.size() != counts.size() ||
      !points || points != width * height || input.tellg() < 0)
    throw std::runtime_error("PCD 地图头部无效或不是原生二进制地图");
  std::uint64_t step = 0;
  for (std::size_t i = 0; i < sizes.size(); ++i) step += sizes[i] * counts[i];
  const auto bytes = std::filesystem::file_size(path);
  const auto offset = static_cast<std::uint64_t>(input.tellg());
  if (bytes != offset + step * points) throw std::runtime_error("PCD 地图载荷长度不完整");
  std::ostringstream result;
  result << "{\"points\":" << points << ",\"point_bytes\":" << step << ",\"file_bytes\":" << bytes
         << ",\"complete_binary_payload\":true}";
  return result.str();
}
}

int main(int argc, char **argv)
{
  try
  {
    if (argc == 5 && std::string(argv[1]) == "trajectory") save(argv[4], compare(argv[2], argv[3]));
    else if (argc == 4 && std::string(argv[1]) == "map") save(argv[3], map_report(argv[2]));
    else throw std::invalid_argument("用法: fast_livo_compare trajectory REF ACTUAL NEW_JSON | map PCD NEW_JSON");
    return 0;
  }
  catch (const std::exception &error) { std::cerr << "数值/地图验收失败: " << error.what() << '\n'; return 1; }
}
