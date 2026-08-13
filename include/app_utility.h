/**
 * @file    app_init.h
 * @brief   Header file for application general initialization.
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

#ifndef APP_UTILITY_H
#define APP_UTILITY_H

#include "app_max30123.h"


/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif /* ifdef __cplusplus */


/* ----------------------------------------------------------------------------
 * 	POWER OFF -> HW or SW Reset Check...goto Deep Sleep
 * ------------------------------------------------------------------------- */
#define DEEP_SLEEP_CODE_ENTER					0xf0b29681					// GOTO DEEP SLEEP
#define DEEP_SLEEP_CODE_WAKEUP				0x04825c05					// WAKEUP DEEP SLEEP
#define DEEP_SLEEP_CODE_CLEAR					0x00000000					//


//====================================================================================================
//
//====================================================================================================
#define BUILD_YEAR_CH0 (__DATE__[ 7])
#define BUILD_YEAR_CH1 (__DATE__[ 8])
#define BUILD_YEAR_CH2 (__DATE__[ 9])
#define BUILD_YEAR_CH3 (__DATE__[10])

#define BUILD_MONTH_IS_JAN (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_FEB (__DATE__[0] == 'F')
#define BUILD_MONTH_IS_MAR (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
#define BUILD_MONTH_IS_APR (__DATE__[0] == 'A' && __DATE__[1] == 'p')
#define BUILD_MONTH_IS_MAY (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
#define BUILD_MONTH_IS_JUN (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_JUL (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
#define BUILD_MONTH_IS_AUG (__DATE__[0] == 'A' && __DATE__[1] == 'u')
#define BUILD_MONTH_IS_SEP (__DATE__[0] == 'S')
#define BUILD_MONTH_IS_OCT (__DATE__[0] == 'O')
#define BUILD_MONTH_IS_NOV (__DATE__[0] == 'N')
#define BUILD_MONTH_IS_DEC (__DATE__[0] == 'D')


#define BUILD_MONTH_CH0 \
    ((BUILD_MONTH_IS_OCT || BUILD_MONTH_IS_NOV || BUILD_MONTH_IS_DEC) ? '1' : '0')

#define BUILD_MONTH_CH1 \
    ( \
        (BUILD_MONTH_IS_JAN) ? '1' : \
        (BUILD_MONTH_IS_FEB) ? '2' : \
        (BUILD_MONTH_IS_MAR) ? '3' : \
        (BUILD_MONTH_IS_APR) ? '4' : \
        (BUILD_MONTH_IS_MAY) ? '5' : \
        (BUILD_MONTH_IS_JUN) ? '6' : \
        (BUILD_MONTH_IS_JUL) ? '7' : \
        (BUILD_MONTH_IS_AUG) ? '8' : \
        (BUILD_MONTH_IS_SEP) ? '9' : \
        (BUILD_MONTH_IS_OCT) ? '0' : \
        (BUILD_MONTH_IS_NOV) ? '1' : \
        (BUILD_MONTH_IS_DEC) ? '2' : \
        /* error default */    '?' \
    )

#define BUILD_DAY_CH0 ((__DATE__[4] >= '0') ? (__DATE__[4]) : '0')
#define BUILD_DAY_CH1 (__DATE__[ 5])



// Example of __TIME__ string: "21:06:19"
//                              01234567

#define BUILD_HOUR_CH0 (__TIME__[0])
#define BUILD_HOUR_CH1 (__TIME__[1])

#define BUILD_MIN_CH0 (__TIME__[3])
#define BUILD_MIN_CH1 (__TIME__[4])

#define BUILD_SEC_CH0 (__TIME__[6])
#define BUILD_SEC_CH1 (__TIME__[7])




extern void sys_delay_10ms(uint8_t ms);
extern void set_date_time_default_clock(void);
extern void Product_Information_Service(void);

uint32_t progress_time_condition(uint32_t taget_time, uint32_t check_time);
uint32_t progress_current_time(void);

void set_current_date_time(uint8_t year, uint8_t mon, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);
void get_current_date_time(void);

void read_calendar(impedance_calendar_t *tm);

void App_Flash_Config_Init(void);

uint32_t Read_Deep_Sleep_Code(void);
void Write_Deep_Sleep_Code(uint32_t  code_data);

FlashStatus_t App_Write_Flash_Buffer(uint32_t addr, uint32_t *result_data, size_t value_size);
FlashStatus_t App_Read_Flash_Buffer(uint32_t addr, uint32_t *result_data, size_t value_size);

int8_t Read_Temperature_Offset(int16_t *offset_data);
void Write_Temperature_Offset(uint32_t  offset_data, uint32_t  offset_status);

void App_RTC_Alarm_Init(uint32_t seconds);
void App_RTC_Alarm_Rearm(uint32_t seconds);

void set_epoch_time(epoch_time_t *t, uint32_t epoch);
uint32_t get_epoch_time(epoch_time_t *t);

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif /* ifdef __cplusplus */

#endif /* APP_INIT_H */
