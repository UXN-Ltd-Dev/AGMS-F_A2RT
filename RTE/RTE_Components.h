/*
 * Auto generated Run-Time-Environment Component Configuration File
 *      *** Do not modify ! ***
 *
 * Project: max30123_demo_v151
 * RTE configuration: max30123_demo_v151.rteconfig
*/
#ifndef RTE_COMPONENTS_H
#define RTE_COMPONENTS_H

/*
 * Define the Device Header File:
*/
#define CMSIS_device_header "rsl15.h"

/* ONSemiconductor::CEM102.CEM102_Driver.CEM102_Driver.source */
#if SWM_DMA_SOURCE == 0
    #undef SWM_DMA_SOURCE
    #define SWM_DMA_SOURCE  3
#endif
/* ONSemiconductor::Device.Bluetooth Core.BLE Abstraction */
#define RTE_ADDED_BLE_ABSTRACTION
/* ONSemiconductor::Device.Bluetooth Profiles.BASS.release */
#define RTE_ADDED_PRF_BASS
/* ONSemiconductor::Device.Libraries.SwmTrace.swmTrace_UART_NB_Source */
#define SWM_TRACE_TYPE SWM_TRACE_UART_NON_BLOCKING

#endif /* RTE_COMPONENTS_H */
