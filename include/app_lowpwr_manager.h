/**
 * @file  app_lowpwr_manager.h
 * @brief Application's low-power manager header file
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

#ifndef APP_LOWPWR_MANAGER_H_
#define APP_LOWPWR_MANAGER_H_

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif    /* ifdef __cplusplus */
/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------
* Function prototype definitions
* --------------------------------------------------------------------------*/

/**
 * @brief       Save the states of peripheral registers
 */
void App_LowPower_SavePeripheralStates(void);

/**
 * @brief       Restore the states of peripheral registers
 */
void App_LowPower_RestorePeripheralStates(void);

/**
 * @brief       Enter a low-power sleep mode
 */
void SOC_Sleep(uint8_t power_mode);

/**
 * @brief Switches Sleep Mode to No Retention Sleep Mode
 */
void Switch_NoRetSleep_Mode(void);

/**
 * @brief GPIO0 wake-up Handler routine
 */
void GPIO0_Wakeup_Process_Handler(void);

/**
 * @brief GPIO1 wake-up Handler routine
 */
void GPIO1_Wakeup_Process_Handler(void);

/**
 * @brief RTC wake-up Handler routine
 */
void RTC_Alarm_Wakeup_Process_Handler(void);

/**
 * @brief Threshold wake-up Handler routine
 */
void Threshold_Wakeup_Process_Handler(void);

/**
 * @brief   Wake-up IRQ interrupt handler
 */
void WAKEUP_IRQHandler(void);

/* ----------------------------------------------------------------------------
 * Global variables and types
 * --------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif    /* INCLUDE_APP_LOWPWR_MANAGER_H_ */
