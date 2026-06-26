/*
 * v3dv_wddm.h — Windows (D3DKMT) winsys backend for Mesa v3dv.
 *
 * Mirrors the v3d_simulator hook: all v3dv kernel calls funnel through
 * v3d_ioctl() (src/broadcom/common/v3d_util.h). The Windows port adds a third
 * branch -> v3d_wddm_ioctl(), translating each DRM_IOCTL_V3D_* into D3DKMT.
 * See MD/Note/20260626-0300-v3dv-umd-winsys-port-surface.md.
 *
 * Overlays into Mesa: src/broadcom/vulkan/v3dv_wddm.{c,h}. OS-independent enough
 * to compile-check the translation switch standalone (V3DV_WDDM_SIM).
 */
#ifndef V3DV_WDDM_H
#define V3DV_WDDM_H

#ifdef V3DV_WDDM_SIM
#include "sim/wddm_shim.h"
#else
#include <stdint.h>
#include <stddef.h>
#include "drm-uapi/v3d_drm.h"
#include <d3dkmthk.h>
#endif

/* One GPU buffer object == one D3DKMT allocation. */
typedef struct v3d_wddm_bo {
   uint32_t handle;       /* GEM-style handle handed back to v3dv         */
   uint32_t d3dkmt_alloc; /* D3DKMT allocation handle                     */
   uint64_t gpu_va;       /* GPU virtual address (GET_BO_OFFSET) — nonzero*/
   uint64_t size;
   void    *cpu_ptr;      /* CPU mapping (MMAP_BO/Lock), NULL until locked */
   int      in_use;
} v3d_wddm_bo;

#define V3D_WDDM_MAX_BO 4096

/* Per-command private data the UMD hands to the KMD on submit (binner/render CL
   GPU addresses). Mirrors the KMD's V3D_CMD_PRIVATE; the KMD maps it to CT0/CT1
   via V3dSubmitFromCl. */
typedef struct v3d_wddm_cmd {
   uint32_t bcl_start, bcl_end;   /* binner CL  */
   uint32_t rcl_start, rcl_end;   /* render CL  */
} v3d_wddm_cmd;

/* Per-device WDDM winsys state (replaces the Linux render_fd). */
typedef struct v3d_wddm_device {
   uint32_t   adapter;            /* D3DKMT adapter handle                 */
   uint32_t   device;             /* D3DKMT device handle                  */
   uint32_t   hwqueue;            /* D3DKMT hardware queue / context        */
   uint32_t   next_handle;        /* BO handle allocator                    */
   uint64_t   next_gpu_va;        /* bump allocator for skeleton GPU VAs    */
   v3d_wddm_bo bo[V3D_WDDM_MAX_BO];
   /* V3D identity, filled from our KMD (DxgkDdiQueryAdapterInfo) — answers
      DRM_V3D_PARAM_V3D_CORE0_IDENT0/1 etc. */
   uint32_t   core0_ident0, core0_ident1, core0_ident2;
   uint32_t   hub_ident1, hub_ident2, hub_ident3;
} v3d_wddm_device;

/* Open the V3D adapter via D3DKMT; returns a pseudo-fd (index) or -1. */
int  v3d_wddm_init(void);
/* The v3d_ioctl translation entry point (DRM_IOCTL_V3D_* -> D3DKMT). */
int  v3d_wddm_ioctl(int fd, unsigned long request, void *arg);
void v3d_wddm_fini(int fd);

#endif /* V3DV_WDDM_H */
