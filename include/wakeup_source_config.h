/**
 * @file  wakeup_source_config.h
 * @brief Wakeup source configuration header file
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

#ifndef WAKEUP_SOURCE_CONFIG_H_
#define WAKEUP_SOURCE_CONFIG_H_

/* ----------------------------------------------------------------------------
 * Include files
 * --------------------------------------------------------------------------*/
#include "hw.h"
#include "app.h"

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

/* Clear all sticky wakeup flags */
#define WAKEUP_FLAGS_CLEAR()                 Sys_ACS_WriteRegister(&ACS->WAKEUP_CTRL, (uint32_t)(0xFFFF))

/* Clock source for RTC
 * Possible options:
 *   - RTC_CLK_SRC_STANDBY_CLK: standby clock that could be XTAL32k or RC32k
 *                         (XTAL32k or RC32k needs to be enabled accordingly)
 *   - RTC_CLK_SRC_GPIO0: external oscillator on GPIO0
 *   - RTC_CLK_SRC_GPIO1: external oscillator on GPIO1
 *   - RTC_CLK_SRC_GPIO2: external oscillator on GPIO2
 *   - RTC_CLK_SRC_GPIO3: external oscillator on GPIO3
 *   - RTC_CLK_SRC_NO_CLOCK */
#define RTC_CLK_SRC                          RTC_CLK_SRC_STANDBY_CLK

/* Maximum RTC Counter Value */
#define RTC_COUNTER_MAX                 0xFFFFFFFF

/* RTC sleep duration(milliseconds) used for power mode
 * Possible options:
 *  - Any value between 5 to 300000 inclusive
 */
#define RTC_SLEEP_TIME_1MS              ((uint32_t)(1))
#define RTC_SLEEP_TIME_2MS              ((uint32_t)(2))
#define RTC_SLEEP_TIME_5MS              ((uint32_t)(5))
#define RTC_SLEEP_TIME_10MS             ((uint32_t)(10))
#define RTC_SLEEP_TIME_20MS             ((uint32_t)(20))
#define RTC_SLEEP_TIME_30MS             ((uint32_t)(30))
#define RTC_SLEEP_TIME_100MS            ((uint32_t)(100))
#define RTC_SLEEP_TIME_1S               ((uint32_t)(1000))
#define RTC_SLEEP_TIME_S(S)             ((uint32_t)(S * 1000))
#define RTC_SLEEP_TIME_60S              ((uint32_t)(60000))
#define RTC_SLEEP_TIME_M(M)             ((uint32_t)(M * 60000))

/* Clock source for sensor
 * Possible options:
 *   - SENSOR_CLK_RTC: RTC clock
 *   - SENSOR_CLK_SLOWCLK: slow clock */
#define SENSOR_CLK_SRC                       SENSOR_CLK_RTC

/* The number of ADC samples stored before waking up the core
 * Possible values:
 *   - SENSOR_FIFO_SIZE1: SIZE = 1
 *   - SENSOR_FIFO_SIZE2: SIZE = 2
 *   - etc. */
#define FIFO_SIZE_VALUE                      SENSOR_FIFO_SIZE1

/* The number of samples
 * Possible options:
 *   - 0x0: 1 sample used
 *   - 0x1: 2 sample used
 *   - 0x2: 3 samples used
 *   - 0x3: 4 samples used
 *   - 0x9: 10 samples used
 *   - 0xF: 16 samples used */
#define NBR_SAMPLES_VALUE                   SENSOR_NBR_SAMPLES_16

/* Sensor Threshold
 * Possible options:
 *   - SENSOR_THRESHOLD_MIN: Threshold min comparator is enabled
 *   - SENSOR_THRESHOLD_MAX: Threshold max comparator is enabled */
#define TEST_SENSOR_THRESHOLD_MIN           0
#define TEST_SENSOR_THRESHOLD_MAX           1

#define ADC_THRESHOLD_VALUE                 TEST_SENSOR_THRESHOLD_MAX

/* Sensor data threshold min and max for wakeup
 * Possible options:
 *   - 0x00000: Increments counter if sample is <= 0x00000
 *   - 0x00001: Increments counter if sample is <= 0x00001
 *   - 0x3FFFF: Increments counter if sample is <= 0x3FFFF */
#define SENSOR_THRESHOLD_MIN                ((uint32_t)(0x3U << SENSOR_THRESHOLD_MIN_THRESHOLD_MIN_Pos)

/* Sensor data threshold min and max for wakeup
 * Possible options:
 *   - 0x00000: Increments counter if sample is >= 0x00000
 *   - 0x00001: Increments counter if sample is >= 0x00001
 *   - 0x3FFFF: Increments counter if sample is >= 0x3FFFF */
#define SENSOR_THRESHOLD_MAX                ((uint32_t)(0x3U << SENSOR_THRESHOLD_MAX_THRESHOLD_MAX_Pos)

#define SAR_CAL_WEIGHT_BIT9                  9
#define SAR_CAL_WEIGHT_BIT14                 14

/* Conversion factor ADC 14-bit for 3V range
 * The 14-bit ADC has 214 levels or 16384.
 * we use only half of its range, that is the positive half  (0:3V) or
 * (8192 : 16383) which corresponds to 16384/2 = 8192 levels
 * This assumes INN = VSSA, INP values between VSSA and 3V
 * Voltage = (digital val - 8192) * (voltage per LSB).
 * scaling factor  k= 8192/3 = 2730.66 and then:
 * V = (SAR_DATA - 8192)/k. */
#define SAR_ADC_OUTPUT_RANGE                 8192
#define ADC_SCALE                            2730.66
#define SAR_ADC_VBAT_SUPPLY                  3000

typedef enum
{
    RTC_ALARM_SLEEP_OK = 0,                           /**< RTC Alarm is okay to go to sleep. */
    RTC_ALARM_SLEEP_NOT_ALLOWED,                      /**< RTC Alarm is Not okay to go to sleep. */
    RTC_ALARM_SLEEP_FIRED,                            /**< RTC Alarm is already fired. */
    RTC_ALARM_DISABLED                                /**< RTC Alarm is disabled. */
} RTC_Alarm_Sleep_t;

/* ---------------------------------------------------------------------------
* Function prototype definitions
* --------------------------------------------------------------------------*/
/**
 * @brief      Configure selected wake up source and enable associated interrupts
 */
void Wakeup_Source_Config(void);

/**
 * @brief       Configure and enable RTC ALARM event
 * @param[in]   alarm_duration To define RTC ALARM event duration
 * @assumptions The following are pre-defined;
 *              RTC_CLK_SRC: RTC clock source
 *              RTC_ALARM_DURATION: RTC counter threshold
 */
void RTC_ALARM_Init(uint32_t alarm_duration);

/**
 * @brief       Configure and enable clock source for RTC
 */
void RTC_ClockSource_Init(void);

/**
 * @brief       Configure GPIO inputs and interrupts
 */
void GPIO_Wakeup_Init(void);

/**
 * @brief       Re-configure start value for the RTC timer counter
 *              and calculate total elapsed RTC cycles
 * @param[in]   Timer_counter Threshold value(number of cycles)
 *              for RTC timer counter
 */
void RTC_ALARM_Set_Timeout(uint32_t timer_counter);

/**
 * @brief       Set relative timeout for the RTC timer counter
 *              and calculate total elapsed RTC cycles. Function will
 *              set relative timer_counter value to RTC_CFG.
 * @param[in]   Timer_counter Start value(number of cycles)
 *              for RTC timer counter
 * @param[in]   Pre_timer_counter previously programmed
 *              timer_counter start value
 * @return      Programmed value of RTC_CFG register
 */
uint32_t RTC_ALARM_Set_Relative_Timeout(uint32_t timer_counter, uint32_t pre_timer_counter);

/**
 * @brief       Check RTC scheduler's status
 * @return      Current status of RTC Scheduler
 */
RTC_Alarm_Sleep_t RTC_Alarm_Sleep_Check(void);

#if 1
void RTC_Clock_Config_Init(void);
#endif


/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif    /* WAKEUP_SOURCE_CONFIG_H_ */
