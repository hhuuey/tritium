#include "fast_livo/diagnostics.h"
#include <Eigen/Core>
#include <boost/version.hpp>
#include <opencv2/core/version.hpp>
#include <pcl/pcl_config.h>
#include <flann/config.h>
#include <sstream>

namespace fast_livo
{
std::string numerics_manifest()
{
  std::ostringstream result;
  result << "{\"compiler\":\"" << __VERSION__ << "\",\"fp_model\":\"" << FAST_LIVO_FP_MODEL
         << "\",\"eigen\":\""
         << EIGEN_WORLD_VERSION << "." << EIGEN_MAJOR_VERSION << "." << EIGEN_MINOR_VERSION
         << "\",\"pcl\":\"" << PCL_VERSION_PRETTY << "\",\"opencv\":\"" << CV_VERSION
         << "\",\"flann\":\"" << FLANN_VERSION_ << "\",\"boost\":" << BOOST_VERSION
         << ",\"vio_reduction\":\"thread-index-float32\",\"vio_reference_mean_cache\":\"initialized-reuse\""
         << ",\"point_to_plane_init\":\"value-initialized\""
         << ",\"voxel_residual_success\":\"materialized-candidate\",\"vio_patch_windows\":\"checked-image-domain\"";
#ifdef EIGEN_VECTORIZE_NEON
  result << ",\"eigen_neon\":true";
#else
  result << ",\"eigen_neon\":false";
#endif
  result << ",\"eigen_max_align_bytes\":" << EIGEN_MAX_ALIGN_BYTES;
  result << ",\"cplusplus\":" << __cplusplus;
#ifdef __STRICT_ANSI__
  result << ",\"strict_ansi\":true";
#else
  result << ",\"strict_ansi\":false";
#endif
#ifdef EIGEN_HAS_CXX17_OVERALIGN
  result << ",\"eigen_cxx17_overalign\":" << EIGEN_HAS_CXX17_OVERALIGN;
#endif
#ifdef __FAST_MATH__
  result << ",\"fast_math\":true";
#else
  result << ",\"fast_math\":false";
#endif
  result << ",\"mp_proc_num\":" << MP_PROC_NUM << "}";
  return result.str();
}
}
