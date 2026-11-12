#pragma once
#include <string>

namespace fast_livo
{
// 返回实际编译核心时采用的版本与数值执行特征，而非调用者头文件的版本。
std::string numerics_manifest();
}
