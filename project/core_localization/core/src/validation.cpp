#include <cmath>
#include <initializer_list>

namespace fast_livo::backend_detail
{
bool allFinite(std::initializer_list<double> values)
{
  for (double value : values)
    if (!std::isfinite(value)) return false;
  return true;
}
}
