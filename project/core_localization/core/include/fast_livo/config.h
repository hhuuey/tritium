#pragma once

#include <string>
#include <vector>

namespace fast_livo
{

struct Config
{
#define FAST_LIVO_PARAMETER(type, field, key, initial) type field = initial;
#include "fast_livo/config_fields.inc"
#undef FAST_LIVO_PARAMETER
  std::string source_dir;
};

}
