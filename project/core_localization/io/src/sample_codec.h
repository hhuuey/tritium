// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "fast_livo/io/dataset.h"
#include "binary.h"

namespace fast_livo::io::detail
{
std::vector<std::uint8_t> encode(const Sample &sample);
Sample decode(Kind kind, const std::vector<std::uint8_t> &bytes);
void validate_header(const Event &event);
}
