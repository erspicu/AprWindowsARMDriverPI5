/*++
Module Name: cyhal_sdio_win.c
Abstract:    Windows SDIO HAL for WHD. WHD's generic SDIO bus (whd_sdio.c) calls
             cyhal_sdio_send_cmd (CMD52 / IO_RW_DIRECT) and cyhal_sdio_bulk_transfer
             (CMD53 / IO_RW_EXTENDED). This file decodes the raw SDIO command
             argument into (function, address, data/length) and forwards to the
             injected SDIO_OPS (sdio_core.h) — which over real hardware issues the
             request via Windows inbox sdbus.sys (wired in at the driver level on
             the Win11/ARM64 target; that part is the only piece needing HW).

   The cyhal_sdio_* types here mirror the relevant subset of
   sources/wifi-host-driver/.../External/hal/cyhal_sdio.h; final integration
   replaces them with that upstream header (see README.md).

   SDIO argument layout (SD spec):
     CMD52: [31]rw [30:28]func [27]raw [25:9]addr(17b) [7:0]data
     CMD53: [31]rw [30:28]func [27]block [26]incr [25:9]addr(17b) [8:0]count
--*/
#include "whd_port.h"
#include "../sdio_core.h"

typedef enum { CYHAL_READ = 0, CYHAL_WRITE = 1 } cyhal_transfer_t;

/* SDIO bus object: carries the injected transport (sdbus-backed at runtime) */
typedef struct _cyhal_sdio_t {
    const SDIO_OPS *Ops;
} cyhal_sdio_t;

#define SDARG_RW(a)    (((a) >> 31) & 0x1u)
#define SDARG_FUNC(a)  (((a) >> 28) & 0x7u)
#define SDARG_ADDR(a)  (((a) >> 9)  & 0x1FFFFu)
#define SDARG_DATA(a)  ((a) & 0xFFu)
#define SDARG_COUNT(a) ((a) & 0x1FFu)

/* CMD52 (single register R/W). 'command' selects the SDIO command; WHD uses this
   for IO_RW_DIRECT. response (if any) returns the read byte in its low 8 bits. */
cy_rslt_t cyhal_sdio_send_cmd(const cyhal_sdio_t *obj, cyhal_transfer_t direction,
                              ULONG command, ULONG argument, ULONG *response)
{
    UCHAR func, data;
    ULONG addr;
    int   write, rc;

    UNREFERENCED_PARAMETER(direction);   /* the R/W bit in the argument is authoritative */
    UNREFERENCED_PARAMETER(command);     /* only IO_RW_DIRECT (CMD52) is decoded here */
    if (obj == NULL || obj->Ops == NULL) return CY_RTOS_BAD_PARAM;

    func  = (UCHAR)SDARG_FUNC(argument);
    addr  = SDARG_ADDR(argument);
    write = (int)SDARG_RW(argument);
    data  = (UCHAR)SDARG_DATA(argument);

    rc = obj->Ops->Cmd52(obj->Ops->Ctx, func, addr, &data, write);
    if (rc != 0) return CY_RTOS_GENERAL_ERROR;

    if (response != NULL) {
        *response = write ? 0u : (ULONG)data;   /* read returns the byte */
    }
    return CY_RSLT_SUCCESS;
}

/* CMD53 (bulk/extended). data/length are explicit; func/addr come from argument. */
cy_rslt_t cyhal_sdio_bulk_transfer(cyhal_sdio_t *obj, cyhal_transfer_t direction,
                                   ULONG argument, ULONG *data, USHORT length,
                                   ULONG *response)
{
    UCHAR func;
    ULONG addr;
    int   write, rc;

    UNREFERENCED_PARAMETER(direction);
    if (obj == NULL || obj->Ops == NULL || data == NULL) return CY_RTOS_BAD_PARAM;

    func  = (UCHAR)SDARG_FUNC(argument);
    addr  = SDARG_ADDR(argument);
    write = (int)SDARG_RW(argument);

    rc = obj->Ops->Cmd53(obj->Ops->Ctx, func, addr, (PUCHAR)data, (ULONG)length, write);
    if (rc != 0) return CY_RTOS_GENERAL_ERROR;

    if (response != NULL) {
        *response = 0u;
    }
    return CY_RSLT_SUCCESS;
}

/*
 * cyhal_sdio_init / register_irq / irq_enable: on Windows the bus is owned by
 * inbox sdbus.sys, so "init" just binds the injected SDIO_OPS; the in-band SDIO
 * interrupt is delivered through the sdbus callback wired at the driver level.
 * (These are stubs here; the sdbus glue is the Win11/ARM64-only piece.)
 */
cy_rslt_t cyhal_sdio_bind_ops(cyhal_sdio_t *obj, const SDIO_OPS *ops)
{
    if (obj == NULL || ops == NULL) return CY_RTOS_BAD_PARAM;
    obj->Ops = ops;
    return CY_RSLT_SUCCESS;
}
