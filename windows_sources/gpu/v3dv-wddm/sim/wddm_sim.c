/* x64 compile+run check of the v3dv WDDM ioctl translation switch.
 * Build: cl /DV3DV_WDDM_SIM /I.. wddm_sim.c ..\v3dv_wddm.c */
#include <stdio.h>
#include "../v3dv_wddm.h"

static int g_pass, g_fail;
static void check(const char *w, int c)
{ if (c) { g_pass++; printf("  [PASS] %s\n", w); } else { g_fail++; printf("  [FAIL] %s\n", w); } }

int main(void)
{
   int fd;
   printf("== v3dv WDDM winsys ioctl-translation simulation ==\n");

   fd = v3d_wddm_init();
   check("init opens adapter (fd>=0)", fd >= 0);

   /* GET_PARAM: feature probe v3dv does at device create. */
   struct drm_v3d_get_param p = { .param = DRM_V3D_PARAM_SUPPORTS_CSD };
   check("GET_PARAM SUPPORTS_CSD ok", v3d_wddm_ioctl(fd, DRM_IOCTL_V3D_GET_PARAM, &p) == 0 && p.value == 1);
   p.param = DRM_V3D_PARAM_SUPPORTS_TFU;
   check("GET_PARAM SUPPORTS_TFU == 0 (Pi5 TFU:no)", v3d_wddm_ioctl(fd, DRM_IOCTL_V3D_GET_PARAM, &p) == 0 && p.value == 0);
   p.param = DRM_V3D_PARAM_V3D_CORE0_IDENT0;
   check("GET_PARAM CORE0_IDENT0 == 0x07443356", v3d_wddm_ioctl(fd, DRM_IOCTL_V3D_GET_PARAM, &p) == 0 && p.value == 0x07443356);
   p.param = DRM_V3D_PARAM_SUPPORTS_PERFMON;
   check("GET_PARAM PERFMON == 0 (deferred)", v3d_wddm_ioctl(fd, DRM_IOCTL_V3D_GET_PARAM, &p) == 0 && p.value == 0);

   /* CREATE_BO -> handle + nonzero GPU VA. */
   struct drm_v3d_create_bo c = { .size = 0x2000 };
   check("CREATE_BO ok", v3d_wddm_ioctl(fd, DRM_IOCTL_V3D_CREATE_BO, &c) == 0);
   check("CREATE_BO handle nonzero", c.handle != 0);
   check("CREATE_BO offset (GPU VA) nonzero", c.offset != 0);

   /* GET_BO_OFFSET round-trips the same VA. */
   struct drm_v3d_get_bo_offset o = { .handle = c.handle };
   check("GET_BO_OFFSET ok", v3d_wddm_ioctl(fd, DRM_IOCTL_V3D_GET_BO_OFFSET, &o) == 0);
   check("GET_BO_OFFSET == CREATE offset", o.offset == c.offset);

   /* second BO gets a distinct, page-advanced VA. */
   struct drm_v3d_create_bo c2 = { .size = 0x1000 };
   v3d_wddm_ioctl(fd, DRM_IOCTL_V3D_CREATE_BO, &c2);
   check("2nd BO distinct handle", c2.handle != c.handle);
   check("2nd BO VA advanced past 1st (0x2000)", c2.offset >= c.offset + 0x2000);

   /* MMAP_BO returns a token. */
   struct drm_v3d_mmap_bo m = { .handle = c.handle };
   check("MMAP_BO ok", v3d_wddm_ioctl(fd, DRM_IOCTL_V3D_MMAP_BO, &m) == 0);

   /* SUBMIT_CL extracts binner/render CL addresses for the KMD. */
   extern v3d_wddm_cmd g_v3d_wddm_last_cmd;
   struct drm_v3d_submit_cl cl = { .bcl_start = 0x1000, .bcl_end = 0x1800,
                                   .rcl_start = 0x2000, .rcl_end = 0x2400 };
   check("SUBMIT_CL routes ok", v3d_wddm_ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CL, &cl) == 0);
   check("SUBMIT_CL extracted BCL", g_v3d_wddm_last_cmd.bcl_start == 0x1000 && g_v3d_wddm_last_cmd.bcl_end == 0x1800);
   check("SUBMIT_CL extracted RCL", g_v3d_wddm_last_cmd.rcl_start == 0x2000 && g_v3d_wddm_last_cmd.rcl_end == 0x2400);

   /* GEM_CLOSE frees; subsequent GET_BO_OFFSET fails. */
   struct drm_gem_close gc = { .handle = c.handle };
   v3d_wddm_ioctl(fd, DRM_IOCTL_GEM_CLOSE, &gc);
   check("after close, GET_BO_OFFSET fails", v3d_wddm_ioctl(fd, DRM_IOCTL_V3D_GET_BO_OFFSET, &o) == -1);

   /* unknown ioctl -> ENOSYS. */
   check("unknown ioctl -> -1", v3d_wddm_ioctl(fd, 0xdead, NULL) == -1);

   printf("== %d passed, %d failed ==\n", g_pass, g_fail);
   return g_fail ? 1 : 0;
}
