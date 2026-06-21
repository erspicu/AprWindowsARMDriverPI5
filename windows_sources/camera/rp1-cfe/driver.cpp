/*++
Module Name: driver.cpp
Abstract:    AVStream minidriver entry for the RP1 CFE camera. Compiled as C++
             (the AVStream convention - ks.h's GUID macros take the C++ path).
             DriverEntry hands a KSDEVICE_DESCRIPTOR to KsInitializeDriver.
--*/
#include "common.h"

NTSTATUS
CfeDeviceAdd(_In_ PKSDEVICE Device)
{
    UNREFERENCED_PARAMETER(Device);
    /* TODO: KsCreateFilterFactory for the capture filter (MIPI-CSI2 sensor ->
       PiSP ISP -> video output pin), set up DMA/buffer queue + sensor I2C. */
    return STATUS_SUCCESS;
}

/* AVStream device dispatch: only Add is provided; the rest use KS defaults. */
static const KSDEVICE_DISPATCH CfeDeviceDispatch = {
    CfeDeviceAdd,   /* Add               */
    NULL,           /* Start             */
    NULL,           /* PostStart         */
    NULL,           /* QueryStop         */
    NULL,           /* CancelStop        */
    NULL,           /* Stop              */
    NULL,           /* QueryRemove       */
    NULL,           /* CancelRemove      */
    NULL,           /* Remove            */
    NULL,           /* QueryCapabilities */
    NULL,           /* SurpriseRemoval   */
    NULL,           /* QueryPower        */
    NULL,           /* SetPower          */
    NULL            /* QueryInterface    */
};

/* No filter factories yet (FilterDescriptorsCount = 0): the camera capture
   filter is the large next stage. */
static const KSDEVICE_DESCRIPTOR CfeDeviceDescriptor = {
    &CfeDeviceDispatch,
    0,
    NULL,
    KSDEVICE_DESCRIPTOR_VERSION
};

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    return KsInitializeDriver(DriverObject, RegistryPath, &CfeDeviceDescriptor);
}
