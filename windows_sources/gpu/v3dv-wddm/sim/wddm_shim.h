/* Standalone shim to compile-check v3dv_wddm.c (V3DV_WDDM_SIM): minimal subset
 * of the drm-uapi v3d_drm.h structs + ioctl codes + param enum, matching field
 * names/order, plus errno/memset. Real build uses the actual drm-uapi header. */
#ifndef WDDM_SHIM_H
#define WDDM_SHIM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

/* drm_v3d uapi structs (subset; same fields as include/drm-uapi/v3d_drm.h) */
struct drm_v3d_get_param     { uint32_t param; uint32_t pad; uint64_t value; };
struct drm_v3d_create_bo     { uint32_t size; uint32_t flags; uint32_t handle; uint32_t offset; };
struct drm_v3d_mmap_bo       { uint32_t handle; uint32_t flags; uint64_t offset; };
struct drm_v3d_get_bo_offset { uint32_t handle; uint32_t offset; };
struct drm_v3d_wait_bo       { uint32_t handle; uint32_t pad; uint64_t timeout_ns; };
struct drm_gem_close         { uint32_t handle; uint32_t pad; };
struct drm_v3d_submit_cl {
   uint32_t bcl_start; uint32_t bcl_end; uint32_t rcl_start; uint32_t rcl_end;
   uint32_t in_sync_bcl; uint32_t in_sync_rcl; uint32_t out_sync;
};

/* ioctl request codes — only need to be distinct for the switch (real values
 * come from DRM_IOWR(...) in the uapi header). */
#define DRM_IOCTL_GEM_CLOSE         0x09
#define DRM_IOCTL_V3D_SUBMIT_CL     0x40
#define DRM_IOCTL_V3D_WAIT_BO       0x41
#define DRM_IOCTL_V3D_CREATE_BO     0x42
#define DRM_IOCTL_V3D_MMAP_BO       0x43
#define DRM_IOCTL_V3D_GET_PARAM     0x44
#define DRM_IOCTL_V3D_GET_BO_OFFSET 0x45
#define DRM_IOCTL_V3D_SUBMIT_TFU    0x46
#define DRM_IOCTL_V3D_SUBMIT_CSD    0x47

enum drm_v3d_param {
   DRM_V3D_PARAM_V3D_UIFCFG,
   DRM_V3D_PARAM_V3D_HUB_IDENT1,
   DRM_V3D_PARAM_V3D_HUB_IDENT2,
   DRM_V3D_PARAM_V3D_HUB_IDENT3,
   DRM_V3D_PARAM_V3D_CORE0_IDENT0,
   DRM_V3D_PARAM_V3D_CORE0_IDENT1,
   DRM_V3D_PARAM_V3D_CORE0_IDENT2,
   DRM_V3D_PARAM_SUPPORTS_TFU,
   DRM_V3D_PARAM_SUPPORTS_CSD,
   DRM_V3D_PARAM_SUPPORTS_CACHE_FLUSH,
   DRM_V3D_PARAM_SUPPORTS_PERFMON,
   DRM_V3D_PARAM_SUPPORTS_MULTISYNC_EXT,
   DRM_V3D_PARAM_SUPPORTS_CPU_QUEUE,
};

#endif /* WDDM_SHIM_H */
