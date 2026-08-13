/**
 * @file  app_bass.h
 * @brief Bluetooth battery service header
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

#ifndef APP_BASS_H
#define APP_BASS_H

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif    /* ifdef __cplusplus */

#include <stdint.h>
#include <ke_msg.h>

struct app_batt_read_t
{
    uint32_t rsl15_vbat_mV_sum;                    /* Sum of RSL15 VBAT measured by LSAD */
    uint8_t rsl15_vbat_percent_prev;
    uint8_t read_cnt;
};

typedef enum _VBAT_Status_t
{
    VBAT_IDLE,              /* LSAD is in Idle state */
    VBAT_MEASURING,         /* LSAD is configured and started measurement */
    VBAT_READ_READY,        /* LSAD result is ready to be read */
    VBAT_DONE               /* The result value is stored and RSL15 is allowed to go to sleep */
} VBAT_Status_t;

/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/

/* Note:
 *   The ble_peripheral_server sample code is only designed to work with APP_BAS_NB
 *   set to 1. */
#define APP_BAS_NB                       1    /* Number of batteries */
#define BATT_LEVEL_LOW_THRESHOLD_PERCENT 15    /* Battery level low at 15% of 1.1V to 1.4V range */

/* LSAD, VBAT and BATMON alarm configuration */
#define BATMON_ALARM_COUNT_CFG           1U

/* Maximum and minimum voltage values used as a reference in order to calculate
 * battery level percent for LOW_VOLTAGE and HIGH_VOLTAGE power configurations
 * defined in cem102_driver.h */
#define LOW_VOLTAGE_CFG_MAX_VOLTAGE_MV   1550
#define LOW_VOLTAGE_CFG_MIN_VOLTAGE_MV   1270
#define HIGH_VOLTAGE_CFG_MAX_VOLTAGE_MV  3300
#define HIGH_VOLTAGE_CFG_MIN_VOLTAGE_MV  2200

/* RSL15 TEMP LSAD Channel */
#define RSL15_TEMP_CHANNEL               0

/* RSL15 VBAT LSAD Channel */
#define RSL15_VBAT_CHANNEL               1


/* Factor that VBAT is divided by */
#define LSAD_VBAT_FACTOR                 2

/* Compensation LSAD Channel */
#define LSAD_COMPENSATION_CHANNEL        7

/* Since the LSAD output code is 14 bits, the LSAD output maximum size is 2^14 */
#define LSAD_MAX_SIZE                    0x4000

#define LSAD_VOLTAGE_RANGE_MV            2000

#define LSAD_READS_NUM                   8

#define LSAD_CHANNEL_NUM                 8

/* Interval for timer that calls BattLevelReadHandler the first time */
#define BEGIN_TIMER_S     1

/* Time interval to measure the battery level */
#define BATT_MEASURE_TIMEOUT_S           20

/* Time interval to notify battery level. BASS_NotifyOnTimeout uses this value
 * to set a kernel timer for the BASS_BATT_LEVEL_NTF_TIMEOUT event, which
 * sends a battery level update request regardless of whether the previous
 * battery level was the same.
 * Should be greater than BATT_CHANGE_TIMEOUT_S */
#define BATT_UPDATE_TIMEOUT_M            3

/* ----------------------------------------------------------------------------
 * Global variables and types
 * --------------------------------------------------------------------------*/
extern volatile VBAT_Status_t vbat_status;

/* ----------------------------------------------------------------------------
 * Function prototype definitions
 * --------------------------------------------------------------------------*/

extern void LSAD_MONITOR_IRQHandler(void);

/**
 * @brief       Notify the central the latest battery level.
 */
void APP_BASS_NTF_Task(void);

/**
 * @brief       Start the LSAD measurement of the battery level.
 */
void APP_BASS_Measure_Task(void);

/**
 * @brief       Calculate the current battery level by mapping the measured
 *              voltage from [MIN_VOLTAGE_MV, MAX_VOLTAGE_MV] to [0, 100]
 * @param[in]   bas_nb    Battery instance used in BLE abstraction layer.
 * @return      Returns the voltage in [0,100] percentile.
 */
uint8_t APP_BASS_ReadBattLevel(uint8_t bas_nb);

/**
 * @brief       Read the LSAD result and calculate the voltage in mV.
 */
void LSAD_ReadVBAT(void);

/**
 * @brief       Initializes app_batt_read and g_f_lsad_gain_offset struct.
 * @param[in]   trim_error    Indicate if any trim error has occurred.
 */
void APP_BASS_ReadBattLevelInit(uint32_t trim_error);

/**
 * @brief       Initializes the LSAD configurations.
 */
void LSAD_ChannelInit(void);


uint32_t App_VBAT_Measure(void);

#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif    /* APP_H */
