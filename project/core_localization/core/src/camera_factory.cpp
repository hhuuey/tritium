#include "camera_factory.h"
#include <vikit/pinhole_camera.h>
#include <vikit/omni_camera.h>
#include <vikit/equidistant_camera.h>
#include <vikit/polynomial_camera.h>
#include <vikit/atan_camera.h>
#include <stdexcept>

vk::AbstractCamera *create_camera(const fast_livo::Config &c)
{
  if(c.cam_model == "Ocam") return new vk::OmniCamera(c.cam_calib_file);
  if(c.cam_model == "Pinhole")
    return new vk::PinholeCamera(c.cam_width,c.cam_height,c.scale,c.cam_fx,c.cam_fy,c.cam_cx,c.cam_cy,
                               c.cam_d0,c.cam_d1,c.cam_d2,c.cam_d3);
  if(c.cam_model == "EquidistantCamera")
    return new vk::EquidistantCamera(c.cam_width,c.cam_height,c.scale,c.cam_fx,c.cam_fy,c.cam_cx,c.cam_cy,
                                    c.k1,c.k2,c.k3,c.k4);
  if(c.cam_model == "PolynomialCamera")
    return new vk::PolynomialCamera(c.cam_width,c.cam_height,c.cam_fx,c.cam_fy,c.cam_cx,c.cam_cy,
                                   c.cam_skew,c.k2,c.k3,c.k4,c.k5,c.k6,c.k7);
  if(c.cam_model == "ATAN")
    return new vk::ATANCamera(c.cam_width,c.cam_height,c.cam_fx,c.cam_fy,c.cam_cx,c.cam_cy,c.cam_d0);
  throw std::invalid_argument("不支持的相机模型：" + c.cam_model);
}
