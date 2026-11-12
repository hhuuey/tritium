#include "vio_reduction.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

int main()
{
  try
  {
    using fast_livo::detail::VioErrorPartial;
    const std::array<VioErrorPartial, 4> frozen{{
        {327134.34375f, 1216}, {347242.75f, 1152},
        {392816.6875f, 1152}, {173856.453125f, 1152}}};
    std::array<int, 4> completion{{0, 1, 2, 3}};
    do
    {
      std::array<VioErrorPartial, 4> partials{};
      for (int thread : completion) partials[thread] = frozen[thread];
      const auto combined = fast_livo::detail::combine_vio_errors(partials);
      volatile int measurements = combined.measurements;
      const float mean = combined.error / measurements;
      if (combined.measurements != 4672 || mean != 265.6357421875f ||
          !(mean <= 265.6357421875f))
        throw std::runtime_error("真实 VIO 临界样本因线程完成顺序或 fast-math 改变了接受决策");
    } while (std::next_permutation(completion.begin(), completion.end()));
    return 0;
  }
  catch (const std::exception &error)
  {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
