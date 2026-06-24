/*++
Module Name: cy_rtos_win.c
Abstract:    Windows kernel implementation of the WHD cy_rtos_* OS abstraction.
             Maps Cypress RTOS primitives to WDM/kernel objects, all intended to
             run on the WHD worker thread at PASSIVE_LEVEL (except set_semaphore,
             which a timer DPC may call at DISPATCH_LEVEL). See whd_port.h / README.
             Compiles under the WDK /kernel toolchain (ARM64).
--*/
#include "whd_port.h"

#define WHDP_TAG  'pdhW'   /* 'Whdp' */

/* milliseconds -> relative (negative) 100ns LARGE_INTEGER for Ke* waits */
static VOID WhdpRelInterval(cy_time_t ms, PLARGE_INTEGER li)
{
    li->QuadPart = -((LONGLONG)ms * 10000);
}

/* ---------------- Semaphore ---------------- */
cy_rslt_t cy_rtos_init_semaphore(cy_semaphore_t *s, ULONG maxcount, ULONG initcount)
{
    LONG limit = (maxcount != 0) ? (LONG)maxcount : MAXLONG;
    if (s == NULL) return CY_RTOS_BAD_PARAM;
    KeInitializeSemaphore(&s->Sem, (LONG)initcount, limit);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_get_semaphore(cy_semaphore_t *s, cy_time_t timeout_ms, int in_isr)
{
    LARGE_INTEGER li;
    NTSTATUS st;
    UNREFERENCED_PARAMETER(in_isr);          /* WHD never waits from an ISR here */
    if (s == NULL) return CY_RTOS_BAD_PARAM;
    if (timeout_ms == CY_RTOS_NEVER_TIMEOUT) {
        st = KeWaitForSingleObject(&s->Sem, Executive, KernelMode, FALSE, NULL);
    } else {
        WhdpRelInterval(timeout_ms, &li);
        st = KeWaitForSingleObject(&s->Sem, Executive, KernelMode, FALSE, &li);
    }
    if (st == STATUS_TIMEOUT) return CY_RTOS_TIMEOUT;
    return (st == STATUS_SUCCESS) ? CY_RSLT_SUCCESS : CY_RTOS_GENERAL_ERROR;
}

cy_rslt_t cy_rtos_set_semaphore(cy_semaphore_t *s, int in_isr)
{
    UNREFERENCED_PARAMETER(in_isr);
    if (s == NULL) return CY_RTOS_BAD_PARAM;
    /* only release if below the limit, so a spurious double-set can't bugcheck */
    if (KeReadStateSemaphore(&s->Sem) == 0) {
        KeReleaseSemaphore(&s->Sem, IO_NO_INCREMENT, 1, FALSE);
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_deinit_semaphore(cy_semaphore_t *s)
{
    UNREFERENCED_PARAMETER(s);               /* KSEMAPHORE needs no teardown */
    return CY_RSLT_SUCCESS;
}

/* ---------------- Mutex ---------------- */
cy_rslt_t cy_rtos_init_mutex(cy_mutex_t *m)
{
    if (m == NULL) return CY_RTOS_BAD_PARAM;
    KeInitializeMutex(&m->Mtx, 0);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_get_mutex(cy_mutex_t *m, cy_time_t timeout_ms)
{
    LARGE_INTEGER li;
    NTSTATUS st;
    if (m == NULL) return CY_RTOS_BAD_PARAM;
    if (timeout_ms == CY_RTOS_NEVER_TIMEOUT) {
        st = KeWaitForSingleObject(&m->Mtx, Executive, KernelMode, FALSE, NULL);
    } else {
        WhdpRelInterval(timeout_ms, &li);
        st = KeWaitForSingleObject(&m->Mtx, Executive, KernelMode, FALSE, &li);
    }
    if (st == STATUS_TIMEOUT) return CY_RTOS_TIMEOUT;
    return (st == STATUS_SUCCESS) ? CY_RSLT_SUCCESS : CY_RTOS_GENERAL_ERROR;
}

cy_rslt_t cy_rtos_set_mutex(cy_mutex_t *m)
{
    if (m == NULL) return CY_RTOS_BAD_PARAM;
    KeReleaseMutex(&m->Mtx, FALSE);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_deinit_mutex(cy_mutex_t *m)
{
    UNREFERENCED_PARAMETER(m);
    return CY_RSLT_SUCCESS;
}

/* ---------------- Time / delay ---------------- */
cy_rslt_t cy_rtos_delay_milliseconds(cy_time_t num_ms)
{
    LARGE_INTEGER li;
    WhdpRelInterval(num_ms, &li);
    KeDelayExecutionThread(KernelMode, FALSE, &li);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_get_time(cy_time_t *tval)
{
    if (tval == NULL) return CY_RTOS_BAD_PARAM;
    /* interrupt time is in 100ns units; convert to a 32-bit ms tick */
    *tval = (cy_time_t)(KeQueryInterruptTime() / 10000ULL);
    return CY_RSLT_SUCCESS;
}

/* ---------------- Thread ---------------- */
static VOID WhdpThreadTrampoline(PVOID Context)
{
    cy_thread_t *t = (cy_thread_t *)Context;
    t->Entry(t->Arg);
    PsTerminateSystemThread(STATUS_SUCCESS);
}

cy_rslt_t cy_rtos_create_thread(cy_thread_t *t, cy_thread_entry_fn_t entry,
                                const char *name, void *stack, ULONG stack_sz,
                                cy_thread_priority_t prio, cy_thread_arg_t arg)
{
    HANDLE h = NULL;
    NTSTATUS st;
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(stack);           /* system threads use a kernel stack */
    UNREFERENCED_PARAMETER(stack_sz);
    UNREFERENCED_PARAMETER(prio);            /* optional: KeSetPriorityThread later */
    if (t == NULL || entry == NULL) return CY_RTOS_BAD_PARAM;

    t->Entry = entry;
    t->Arg   = arg;
    t->ThreadObj = NULL;

    st = PsCreateSystemThread(&h, THREAD_ALL_ACCESS, NULL, NULL, NULL,
                              WhdpThreadTrampoline, t);
    if (!NT_SUCCESS(st)) return CY_RTOS_GENERAL_ERROR;

    /* keep a referenced object so we can join, then drop the handle */
    st = ObReferenceObjectByHandle(h, THREAD_ALL_ACCESS, *PsThreadType, KernelMode,
                                   (PVOID *)&t->ThreadObj, NULL);
    ZwClose(h);
    return NT_SUCCESS(st) ? CY_RSLT_SUCCESS : CY_RTOS_GENERAL_ERROR;
}

cy_rslt_t cy_rtos_join_thread(cy_thread_t *t)
{
    if (t == NULL || t->ThreadObj == NULL) return CY_RTOS_BAD_PARAM;
    KeWaitForSingleObject(t->ThreadObj, Executive, KernelMode, FALSE, NULL);
    ObDereferenceObject(t->ThreadObj);
    t->ThreadObj = NULL;
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_exit_thread(void)
{
    PsTerminateSystemThread(STATUS_SUCCESS);
    return CY_RSLT_SUCCESS;                   /* not reached */
}

/* ---------------- Timer ---------------- */
static VOID WhdpTimerDpc(PKDPC Dpc, PVOID Ctx, PVOID A1, PVOID A2)
{
    cy_timer_t *tmr = (cy_timer_t *)Ctx;
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(A1);
    UNREFERENCED_PARAMETER(A2);
    if (tmr != NULL && tmr->Cb != NULL) {
        tmr->Cb(tmr->Arg);                    /* runs at DISPATCH_LEVEL */
    }
}

cy_rslt_t cy_rtos_init_timer(cy_timer_t *tmr, cy_timer_trigger_type_t type,
                             cy_timer_callback_t cb, cy_timer_callback_arg_t arg)
{
    if (tmr == NULL || cb == NULL) return CY_RTOS_BAD_PARAM;
    KeInitializeTimerEx(&tmr->Timer, NotificationTimer);
    KeInitializeDpc(&tmr->Dpc, WhdpTimerDpc, tmr);
    tmr->Cb       = cb;
    tmr->Arg      = arg;
    tmr->PeriodMs = (type == CY_TIMER_TYPE_PERIODIC) ? 1 : 0;  /* set on start */
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_start_timer(cy_timer_t *tmr, cy_time_t num_ms)
{
    LARGE_INTEGER due;
    LONG period = 0;
    if (tmr == NULL) return CY_RTOS_BAD_PARAM;
    WhdpRelInterval(num_ms, &due);
    if (tmr->PeriodMs != 0) {
        period = (LONG)num_ms;                /* periodic: repeat every num_ms */
    }
    KeSetTimerEx(&tmr->Timer, due, period, &tmr->Dpc);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_stop_timer(cy_timer_t *tmr)
{
    if (tmr == NULL) return CY_RTOS_BAD_PARAM;
    KeCancelTimer(&tmr->Timer);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_deinit_timer(cy_timer_t *tmr)
{
    if (tmr == NULL) return CY_RTOS_BAD_PARAM;
    KeCancelTimer(&tmr->Timer);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_rtos_is_running_timer(cy_timer_t *tmr, int *state)
{
    if (tmr == NULL || state == NULL) return CY_RTOS_BAD_PARAM;
    /* KeReadStateTimer: TRUE once expired/signaled -> "running" is the inverse */
    *state = KeReadStateTimer(&tmr->Timer) ? 0 : 1;
    return CY_RSLT_SUCCESS;
}
