#pragma once
#include "fast_livo/config.h"
#include <vikit/abstract_camera.h>

vk::AbstractCamera *create_camera(const fast_livo::Config &config);
