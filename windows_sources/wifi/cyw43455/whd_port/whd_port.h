/*++
Module Name: whd_port.h
Abstract:    Windows kernel port layer for Infineon WHD (Wi-Fi Host Driver).
             Defines the Windows-backed cy_rtos_* OS-abstraction types and the
             prototypes of the WHD-required RTOS + cyhal SDIO surface, mapped to
             KMDF/WDM kernel primitives. Signatures mirror the upstream WHD
             headers (External/rtos/cyabs_rtos.h, External/hal/cyhal_sdio.h) so
             final integration with WHD on the include path is mechanical.

   WHD calls DOWN into two things we must provide:
     1. cy_rtos_*      (OS abstraction: thread/semaphore/mutex/timer/delay/time)
     2. cyhal_sdio_*   (SDIO HAL: send_cmd=CMD52, bulk_transfer=CMD53, irq)
   See README.md for the full integration map.
--*/
#pragma once

#ifdef CYW_SIM
#include "../sim/wifi_simshim.h"
typedef unsigned long long  UPTR_T;
#else
#include <ntddk.h>
typedef ULONG_PTR           UPTR_T;
#endif

/* ---- WHD/Cypress result + time conventions ---- */
typedef ULONG cy_rslt_t;
#define CY_RSLT_SUCCESS            ((cy_rslt_t)0)
#define CY_RTOS_GENERAL_ERROR      ((cy_rslt_t)0xA0010001u)
#define CY_RTOS_TIMEOUT            ((cy_rslt_t)0xA0010002u)
#define CY_RTOS_BAD_PARAM          ((cy_rslt_t)0xA0010003u)

typedef ULONG cy_time_t;                       /* milliseconds */
#define CY_RTOS_NEVER_TIMEOUT      ((cy_time_t)0xFFFFFFFFu)

typedef enum {
    CY_RTOS_PRIORITY_MIN = 0, CY_RTOS_PRIORITY_LOW, CY_RTOS_PRIORITY_BELOWNORMAL,
    CY_RTOS_PRIORITY_NORMAL, CY_RTOS_PRIORITY_ABOVENORMAL, CY_RTOS_PRIORITY_HIGH,
    CY_RTOS_PRIORITY_REALTIME, CY_RTOS_PRIORITY_MAX
} cy_thread_priority_t;

typedef UPTR_T cy_thread_arg_t;
typedef void (*cy_thread_entry_fn_t)(cy_thread_arg_t arg);

/* ---- Windows-backed OS object types (cyabs_rtos_impl.h equivalents) ---- */
#ifndef CYW_SIM
typedef struct _CY_THREAD {
    PETHREAD             ThreadObj;            /* referenced kernel thread object */
    cy_thread_entry_fn_t Entry;
    cy_thread_arg_t      Arg;
} cy_thread_t;

typedef struct _CY_SEMAPHORE { KSEMAPHORE Sem; } cy_semaphore_t;
typedef struct _CY_MUTEX     { KMUTEX     Mtx; } cy_mutex_t;

typedef void (*cy_timer_callback_t)(cy_thread_arg_t arg);
typedef struct _CY_TIMER {
    KTIMER              Timer;
    KDPC                Dpc;
    cy_timer_callback_t Cb;
    cy_thread_arg_t     Arg;
    ULONG               PeriodMs;              /* 0 = one-shot */
} cy_timer_t;
typedef enum { CY_TIMER_TYPE_ONCE = 0, CY_TIMER_TYPE_PERIODIC } cy_timer_trigger_type_t;
typedef cy_thread_arg_t cy_timer_callback_arg_t;
#endif

/* ---- cy_rtos_* subset that WHD actually calls (verified via grep on WHD/src) ---- */
cy_rslt_t cy_rtos_init_semaphore(cy_semaphore_t *s, ULONG maxcount, ULONG initcount);
cy_rslt_t cy_rtos_get_semaphore(cy_semaphore_t *s, cy_time_t timeout_ms, int in_isr);
cy_rslt_t cy_rtos_set_semaphore(cy_semaphore_t *s, int in_isr);
cy_rslt_t cy_rtos_deinit_semaphore(cy_semaphore_t *s);

cy_rslt_t cy_rtos_init_mutex(cy_mutex_t *m);
cy_rslt_t cy_rtos_get_mutex(cy_mutex_t *m, cy_time_t timeout_ms);
cy_rslt_t cy_rtos_set_mutex(cy_mutex_t *m);
cy_rslt_t cy_rtos_deinit_mutex(cy_mutex_t *m);

cy_rslt_t cy_rtos_delay_milliseconds(cy_time_t num_ms);
cy_rslt_t cy_rtos_get_time(cy_time_t *tval);

#ifndef CYW_SIM
cy_rslt_t cy_rtos_create_thread(cy_thread_t *t, cy_thread_entry_fn_t entry,
                                const char *name, void *stack, ULONG stack_sz,
                                cy_thread_priority_t prio, cy_thread_arg_t arg);
cy_rslt_t cy_rtos_join_thread(cy_thread_t *t);
cy_rslt_t cy_rtos_exit_thread(void);

cy_rslt_t cy_rtos_init_timer(cy_timer_t *tmr, cy_timer_trigger_type_t type,
                             cy_timer_callback_t cb, cy_timer_callback_arg_t arg);
cy_rslt_t cy_rtos_start_timer(cy_timer_t *tmr, cy_time_t num_ms);
cy_rslt_t cy_rtos_stop_timer(cy_timer_t *tmr);
cy_rslt_t cy_rtos_deinit_timer(cy_timer_t *tmr);
cy_rslt_t cy_rtos_is_running_timer(cy_timer_t *tmr, int *state);
#endif
