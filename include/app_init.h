/**
 * @file  app_init.h
 * @brief Application initialization header
 *
 * @copyright @parblock
 * Copyright (c) 2023 Semiconductor Components Industries, LLC (d/b/a
 * onsemi), All Rights Reserved
 *
 * This code is the property of onsemi and may not be redistributed
 * in any form without prior written permission from onsemi.
 * The terms of use and warranty for this code are covered by contractual
 * agreements between onsemi and the licensee.
 *
 * This is Reusable Code.
 * @endparblock
 */

#ifndef APP_INIT_H
#define APP_INIT_H

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif    /* ifdef __cplusplus */

/* ----- Define general clocks used by the application --------------------- */

/**
 * @brief       Set the sensor clock target frequency (in Hertz)
 * @details     The SAR-ADC, pulse counter, and ultra-low-power (ULP) data
 *              acquisitions subsystems use this clock frequency.
 */
#define SENSOR_CLK_HZ                   (32768)

/**
 * @brief       Set the system clock target frequency (in Hertz)
 * @details     This clock frequency is used for the RSL15's primary clock and
 *              all other clocks are derived from it (except the standby clock).
 *              When using BLE, the system clock needs to be a multiple of 8MHz
 *              to ensure the BBCLK has a valid frequency.
 * @note        During the clock initialization, in App_ClockConfig, the system
 *              clock will be coerced into a multiple of 8MHz.
 */
#define SYSTEM_CLK_HZ                   (8000000)

/**
 * @brief       Set UART peripheral clock target frequency (in Hertz)
 * @details     This clock frequency is used to achieve the desired baud rate
 *              for UART communications.
 * @note        The UART clock is divided from the system clock and is used to
 *              clock the interface.
 */
#define UART_CLK_HZ                     (SYSTEM_CLK_HZ)

/**
 * @brief       Set user clock target frequency (in Hertz)
 * @details     This clock is not used internally by the RSL15 system. It can
 *              be used for external needs as a clock source for external
 *              components and pulse-code modulated (PCM) interfaces.
 * @note        If this value exceeds the system clock frequency, and the
 *              RF clock is available, the USER_CLK_HZ will be sourced from
 *              the RF clock.
 */
#define USER_CLK_HZ                     (1000000)

/**
 * @brief       Initialize the device configurations
 */
void DeviceInit(void);

/**
 * @brief       Initialize application related message handlers
 */
void AppMsgHandlersInit(void);

/**
 * @brief       Initialize the BLE battery service server
 */
void BatteryServiceServerInit(void);

/**
 * @brief       Initialize the BLE custom service server
 */
void CustomServiceServerInit(void);

/**
 * @brief       Set the priority of non-BLE interrupts to 1
 */
void IRQPriorityInit(void);

/**
 * @brief       Initialize the BLE stack
 */
void BLEStackInit(void);

/**
 * @brief       Initialize the BLE subsystem
 */
void BLE_SystemInit(void);

/**
 * @brief       Disable interrupts and exceptions
 */
void DisableAppInterrupts(void);

/**
 * @brief       Enable interrupts and exceptions
 */
void EnableAppInterrupts(void);

/**
 * @brief       Initialize the GPIOs
 */
void App_GPIO_Config(void);

/**
 * @brief       Initialize the system clocks
 */
void App_Clock_Config(void);

/**
 * @brief       Initialize Memory Retention Sleep Mode configuration used for wake-up
 */
void App_Mem_Ret_Sleep_Init(void);

/**
 * @brief       Initialize No Retention Sleep Mode configuration used for wake-up
 */
void App_No_Ret_Sleep_Init(void);

/**
 * @brief Power Down the FPU Unit
 * @return Returns FPU_Q_ACCEPTED if the FPU power down was successful.
 *                 FPU_Q_DENIED if the FPU power down failed.
 */
uint32_t Power_Down_FPU(void);

/**
 * @brief Initialize swmTrace after wakeup from sleep
 */
void Init_SWMTrace(void);

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif    /* APP_INIT_H */
