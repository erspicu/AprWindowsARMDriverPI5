/*
 * v3dv_wddm.c — D3DKMT translation of v3dv's DRM_IOCTL_V3D_* calls.
 * Skeleton: the request switch + drm_v3d_* <-> D3DKMT data mapping is in place;
 * the actual D3DKMT calls are stubbed (d3dkmt_* helpers, marked TODO) until run
 * against our KMD on the Pi5 target. See v3dv_wddm.h + the port-surface note.
 */
#include "v3dv_wddm.h"

/* Single device for the skeleton (v3dv opens one render node). */
static v3d_wddm_device g_dev;

/* ---- D3DKMT operation stubs (TODO: wire to gdi32 D3DKMT* + our KMD) ---- */

static int d3dkmt_open_adapter(v3d_wddm_device *d)
{
   /* TODO: D3DKMTOpenAdapterFromLuid(V3D LUID) + D3DKMTCreateDevice +
      D3DKMTCreateContextVirtual/HwQueue. */
   d->adapter = 1; d->device = 1; d->hwqueue = 1;
   d->next_handle = 1;
   d->next_gpu_va = 0x1000;          /* nonzero: HW treats GPU VA 0 specially */
   /* Real Pi5 V3D 7.1.10.16 IDENT (read-only probe 2026-06-26, v3d_regs debugfs);
      the KMD overrides these at runtime via D3DKMTQueryAdapterInfo(private). */
   d->core0_ident0 = 0x07443356;  d->core0_ident1 = 0x81001431;  d->core0_ident2 = 0xc0078101;
   d->hub_ident1   = 0x00081117;  d->hub_ident2   = 0x00001900;  d->hub_ident3   = 0x00020a10;
   return 0;
}

static int d3dkmt_create_allocation(v3d_wddm_device *d, v3d_wddm_bo *bo, uint64_t size)
{
   /* TODO: D3DKMTCreateAllocation2(size) -> alloc handle; assign GPU VA via
      D3DKMTMapGpuVirtualAddress (our KMD's BuildPagingBuffer writes the PTE). */
   bo->d3dkmt_alloc = d->next_handle;
   bo->gpu_va = d->next_gpu_va;
   d->next_gpu_va += (size + 0xfff) & ~0xfffull;   /* page-align bump          */
   bo->size = size;
   return 0;
}

static int d3dkmt_lock(v3d_wddm_bo *bo)
{
   /* TODO: D3DKMTLock2(alloc) -> CPU pointer. */
   (void)bo;
   return 0;
}

static int d3dkmt_destroy_allocation(v3d_wddm_bo *bo)
{
   /* TODO: D3DKMTDestroyAllocation(alloc). */
   (void)bo;
   return 0;
}

static int d3dkmt_submit(v3d_wddm_device *d, const void *cl, uint32_t size)
{
   /* TODO: D3DKMTSubmitCommandToHwQueue() with the CL DMA buffer; our KMD's
      SubmitCommandVirtual writes V3D CT0/CT1 CA/EA. Completion -> monitored
      fence (D3DKMTCreateSynchronizationObject2). */
   (void)d; (void)cl; (void)size;
   return 0;
}

/* ---- BO table helpers ---- */

static v3d_wddm_bo *bo_alloc(v3d_wddm_device *d)
{
   for (uint32_t i = 0; i < V3D_WDDM_MAX_BO; i++) {
      if (!d->bo[i].in_use) {
         d->bo[i].in_use = 1;
         d->bo[i].handle = d->next_handle++;
         return &d->bo[i];
      }
   }
   return NULL;
}

static v3d_wddm_bo *bo_find(v3d_wddm_device *d, uint32_t handle)
{
   for (uint32_t i = 0; i < V3D_WDDM_MAX_BO; i++)
      if (d->bo[i].in_use && d->bo[i].handle == handle)
         return &d->bo[i];
   return NULL;
}

/* ---- GET_PARAM: answer v3dv's capability probe (static + KMD identity) ---- */

static int handle_get_param(v3d_wddm_device *d, struct drm_v3d_get_param *p)
{
   switch (p->param) {
   case DRM_V3D_PARAM_V3D_CORE0_IDENT0: p->value = d->core0_ident0; return 0;
   case DRM_V3D_PARAM_V3D_CORE0_IDENT1: p->value = d->core0_ident1; return 0;
   case DRM_V3D_PARAM_V3D_CORE0_IDENT2: p->value = d->core0_ident2; return 0;
   case DRM_V3D_PARAM_V3D_HUB_IDENT1:   p->value = d->hub_ident1;   return 0;
   case DRM_V3D_PARAM_V3D_HUB_IDENT2:   p->value = d->hub_ident2;   return 0;
   case DRM_V3D_PARAM_V3D_HUB_IDENT3:   p->value = d->hub_ident3;   return 0;
   /* V3D 7.1.10.16 feature bits — calibrated to the real Pi5 (debugfs v3d_ident).*/
   case DRM_V3D_PARAM_SUPPORTS_TFU:           p->value = 0; return 0; /* TFU: no  */
   case DRM_V3D_PARAM_SUPPORTS_CSD:           p->value = 1; return 0;
   case DRM_V3D_PARAM_SUPPORTS_CACHE_FLUSH:   p->value = 1; return 0;
   case DRM_V3D_PARAM_SUPPORTS_MULTISYNC_EXT: p->value = 1; return 0;
   case DRM_V3D_PARAM_SUPPORTS_PERFMON:       p->value = 0; return 0; /* later */
   case DRM_V3D_PARAM_SUPPORTS_CPU_QUEUE:     p->value = 0; return 0; /* later */
   default:                                   p->value = 0; return 0;
   }
}

/* ---- the v3d_ioctl translation entry point ---- */

int v3d_wddm_ioctl(int fd, unsigned long request, void *arg)
{
   v3d_wddm_device *d = &g_dev;
   (void)fd;   /* single device skeleton */

   switch (request) {
   case DRM_IOCTL_V3D_GET_PARAM:
      return handle_get_param(d, (struct drm_v3d_get_param *)arg);

   case DRM_IOCTL_V3D_CREATE_BO: {
      struct drm_v3d_create_bo *c = (struct drm_v3d_create_bo *)arg;
      v3d_wddm_bo *bo = bo_alloc(d);
      if (!bo) { errno = ENOMEM; return -1; }
      if (d3dkmt_create_allocation(d, bo, c->size) != 0) { bo->in_use = 0; errno = ENOMEM; return -1; }
      c->handle = bo->handle;
      c->offset = (uint32_t)bo->gpu_va;   /* low 32 bits of the GPU VA */
      return 0;
   }

   case DRM_IOCTL_V3D_GET_BO_OFFSET: {
      struct drm_v3d_get_bo_offset *o = (struct drm_v3d_get_bo_offset *)arg;
      v3d_wddm_bo *bo = bo_find(d, o->handle);
      if (!bo) { errno = ENOENT; return -1; }
      o->offset = (uint32_t)bo->gpu_va;
      return 0;
   }

   case DRM_IOCTL_V3D_MMAP_BO: {
      struct drm_v3d_mmap_bo *m = (struct drm_v3d_mmap_bo *)arg;
      v3d_wddm_bo *bo = bo_find(d, m->handle);
      if (!bo) { errno = ENOENT; return -1; }
      if (d3dkmt_lock(bo) != 0) { errno = EINVAL; return -1; }
      m->offset = bo->handle;   /* token for the subsequent CPU map */
      return 0;
   }

   case DRM_IOCTL_V3D_WAIT_BO:
      /* TODO: wait the BO's last fence (D3DKMTWaitForSynchronizationObject). */
      return 0;

   case DRM_IOCTL_GEM_CLOSE: {
      struct drm_gem_close *gc = (struct drm_gem_close *)arg;
      v3d_wddm_bo *bo = bo_find(d, gc->handle);
      if (bo) { d3dkmt_destroy_allocation(bo); bo->in_use = 0; }
      return 0;
   }

   case DRM_IOCTL_V3D_SUBMIT_CL:
   case DRM_IOCTL_V3D_SUBMIT_CSD:
   case DRM_IOCTL_V3D_SUBMIT_TFU:
      /* All map to a hardware-queue submit; the CL/CSD/TFU payload differs. */
      return d3dkmt_submit(d, arg, 0);

   default:
      errno = ENOSYS;   /* PERFMON_* / SUBMIT_CPU etc. not ported yet */
      return -1;
   }
}

int v3d_wddm_init(void)
{
   memset(&g_dev, 0, sizeof(g_dev));
   if (d3dkmt_open_adapter(&g_dev) != 0)
      return -1;
   return 0;   /* pseudo-fd 0 */
}

void v3d_wddm_fini(int fd)
{
   (void)fd;
   /* TODO: D3DKMTDestroyDevice + D3DKMTCloseAdapter. */
}
