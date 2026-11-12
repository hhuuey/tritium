// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace fast_livo::io::detail
{

inline void require(bool condition, const char *message)
{
  if (!condition) throw std::runtime_error(message);
}

class Encoder
{
public:
  std::vector<std::uint8_t> bytes;

  template<class T> void scalar(T value)
  {
    static_assert(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>);
    if constexpr (std::is_floating_point_v<T>)
    {
      static_assert(std::numeric_limits<T>::is_iec559);
      using U = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
      U bits;
      std::memcpy(&bits, &value, sizeof(T));
      scalar(bits);
    }
    else
    {
      using U = std::make_unsigned_t<T>;
      const auto bits = static_cast<U>(value);
      for (std::size_t i = 0; i < sizeof(T); ++i) bytes.push_back((bits >> (8 * i)) & 255);
    }
  }

  void raw(const void *data, std::size_t size)
  {
    if (!size) return;
    const auto *begin = static_cast<const std::uint8_t *>(data);
    bytes.insert(bytes.end(), begin, begin + size);
  }

  void text(const std::string &value)
  {
    require(value.size() <= 1048576, "字符串过长");
    scalar<std::uint32_t>(value.size());
    raw(value.data(), value.size());
  }
};

class Decoder
{
public:
  explicit Decoder(const std::vector<std::uint8_t> &bytes) : bytes_(bytes) {}
  std::size_t remaining() const { return bytes_.size() - position_; }

  template<class T> T scalar()
  {
    static_assert(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>);
    if constexpr (std::is_floating_point_v<T>)
    {
      using U = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
      U bits = scalar<U>();
      T value;
      std::memcpy(&value, &bits, sizeof(T));
      return value;
    }
    else
    {
      require(remaining() >= sizeof(T), "二进制载荷截断");
      using U = std::make_unsigned_t<T>;
      U bits = 0;
      for (std::size_t i = 0; i < sizeof(T); ++i)
        bits |= static_cast<U>(bytes_[position_++]) << (8 * i);
      T value;
      std::memcpy(&value, &bits, sizeof(T));
      return value;
    }
  }

  void raw(void *target, std::size_t size)
  {
    require(size <= remaining(), "二进制载荷长度无效");
    if (size) std::memcpy(target, bytes_.data() + position_, size);
    position_ += size;
  }

  std::string text()
  {
    const auto size = scalar<std::uint32_t>();
    require(size <= 1048576 && size <= remaining(), "字符串长度无效");
    std::string value(size, '\0');
    raw(value.data(), size);
    return value;
  }

  bool boolean()
  {
    const auto value = scalar<std::uint8_t>();
    require(value <= 1, "布尔字段无效");
    return value != 0;
  }
private:
  const std::vector<std::uint8_t> &bytes_;
  std::size_t position_ = 0;
};

}
