/**
 * @file  app_bass.c
 * @brief BASS Application-specific source file
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

#include <app.h>
#include <app_bass.h>
#include <hw.h>
#include <swmTrace_api.h>
#include <ble_bass.h>
#include "app_customss.h"
/* Application headers */
#include "app_temperature_sensor.h"

/* Struct containing values needed for battery level calculation */
static struct app_batt_read_t app_batt_read;

/* Array containing LSAD gain and offset values from TRIM sector */
static struct F_LSAD_TRIM g_f_lsad_gain_offset[LSAD_CHANNEL_NUM];

/* Default trim value sector. */
static TRIM_Type *trims = TRIM;

volatile uint8_t rsl15_vbat_percent;
volatile uint8_t last_rsl15_vbat_percent;
volatile VBAT_Status_t vbat_status;

void LSAD_MONITOR_IRQHandler(void)
{
    vbat_status = VBAT_READ_READY;

    /* Check if data overrun has occurred */
    if (LSAD->MONITOR_STATUS & LSAD_OVERRUN_TRUE)
    {
        LSAD->MONITOR_STATUS = LSAD_OVERRUN_CLEAR;
    }

    /* Check if a LSAD monitor alarm occurred */
    if (LSAD->MONITOR_STATUS & MONITOR_ALARM_TRUE)
    {
        LSAD->MONITOR_STATUS = MONITOR_ALARM_CLEAR;
    }

    LSAD->MONITOR_STATUS = LSAD_READY_CLEAR;
}

void APP_BASS_NTF_Task(void)
{
    BASS_BattLevelUpdReq(rsl15_vbat_percent, 0);
    last_rsl15_vbat_percent = rsl15_vbat_percent;
}

void APP_BASS_Measure_Task(void)
{
#if DEBUG_SLEEP_GPIO == DEBUG_RTC_TASKS_MODE
    /* Toggle GPIO to indicate BASS RTC Task */
    Sys_GPIO_Set_High(BASS_RTC_TASK_GPIO);
    Sys_GPIO_Set_Low(BASS_RTC_TASK_GPIO);
#endif
    LSAD_ChannelInit();
}

uint8_t APP_BASS_ReadBattLevel(uint8_t bas_nb)
{
    uint32_t rsl15_vbat_mV_avg;
    uint32_t min_voltage, max_voltage;

    if (cem102_dut->pwr_cfg == LOW_VOLTAGE)
    {
        /* Set minimum and maximum voltage range for RSL15 VBAT
         * when CEM102's power configuration is LOW_VOLTAGE.
         */
        min_voltage = LOW_VOLTAGE_CFG_MIN_VOLTAGE_MV;
        max_voltage = LOW_VOLTAGE_CFG_MAX_VOLTAGE_MV;
    }
    else
    {
        /* Set minimum and maximum voltage range for RSL15 VBAT
         * when CEM102's power configuration is HIGH_VOLTAGE.
         */
        min_voltage = HIGH_VOLTAGE_CFG_MIN_VOLTAGE_MV;
        max_voltage = HIGH_VOLTAGE_CFG_MAX_VOLTAGE_MV;
    }

    if (app_batt_read.read_cnt == LSAD_READS_NUM)
    {
        /* Calculate average RSL15 VBAT */
        rsl15_vbat_mV_avg = (uint32_t)(app_batt_read.rsl15_vbat_mV_sum / LSAD_READS_NUM);

        /* If the average RSL15 VBAT is less than min_vol then
         * simply set RSL15 VBAT in percentage to 0%. */
        if (rsl15_vbat_mV_avg < min_voltage)
        {
            rsl15_vbat_percent = 0;
        }
        else
        {
            /* Calculate RSL15 VBAT in percentage from the average RSL15 VBAT measured by LSAD.
             * The voltage is scaled from [min_voltage, max_voltage] to [0, 100] */
            rsl15_vbat_percent = (uint8_t)(((rsl15_vbat_mV_avg - min_voltage) * 100) /
                                         (max_voltage - min_voltage));

            /* If the calculated RSL15 VBAT in percentage is more than 100%
             *  simply set it to 100% */
            rsl15_vbat_percent = (rsl15_vbat_percent <= 100) ? rsl15_vbat_percent : 100;
        }

        app_batt_read.rsl15_vbat_percent_prev = rsl15_vbat_percent;

        /* Reset all variables associated with LSAD measurements before
         * the next round of measurements. */
        app_batt_read.rsl15_vbat_mV_sum = 0;
        app_batt_read.read_cnt = 0;
    }
    else
    {
        /* Return the last battery level calculated if a set of LSAD readings has not been
         * completed since the last time the function was called */
        rsl15_vbat_percent = app_batt_read.rsl15_vbat_percent_prev;
    }

    /* Update Battery Level only if rsl15_vbat_percent has been changed */
    if (last_rsl15_vbat_percent != rsl15_vbat_percent)
    {
        BASS_BattLevelUpdReq(rsl15_vbat_percent, 0);
        last_rsl15_vbat_percent = rsl15_vbat_percent;
    }

    return rsl15_vbat_percent;
}

void LSAD_ReadVBAT(void)
{
    struct F_LSAD_TRIM *gain_offset = &g_f_lsad_gain_offset[RSL15_VBAT_CHANNEL];
    float lsad_rsl15_result = 0.0f;
    uint32_t rsl15_vbat_mV;

    NVIC_DisableIRQ(LSAD_MONITOR_IRQn);

    if (app_batt_read.read_cnt < LSAD_READS_NUM)
    {
        /* Calculating voltage using gain and offset and multiplying by 2 since
         * VBAT is divided by 2 when measured by the LSAD. */
        if (gain_offset->hf_gain != 0)
        {
            /* Computing LSAD result using the LSAD full scale of 2.0V and LSAD resolution
             * of 14 bits */
            lsad_rsl15_result = CONVERT(LSAD->DATA_TRIM_CH[RSL15_VBAT_CHANNEL]) / V_TO_MV_F;

            /* Applying offset and gain compensation to calculated voltage value */
            rsl15_vbat_mV = (uint32_t)(((lsad_rsl15_result - gain_offset->hf_offset) /
                    gain_offset->hf_gain * V_TO_MV) * LSAD_VBAT_FACTOR);
        }

        /* Calculating voltage using only reference voltage and LSAD data if gain is 0 */
        else
        {
            rsl15_vbat_mV = ((LSAD_VOLTAGE_RANGE_MV * LSAD_VBAT_FACTOR *
                              LSAD->DATA_TRIM_CH[RSL15_VBAT_CHANNEL]) / (LSAD_MAX_SIZE));
        }

        app_batt_read.rsl15_vbat_mV_sum += rsl15_vbat_mV;

        app_batt_read.read_cnt++;
    }

    if (app_batt_read.read_cnt == LSAD_READS_NUM)
    {
        APP_BASS_ReadBattLevel(0);
    }

    vbat_status = VBAT_DONE;
}

void APP_BASS_ReadBattLevelInit(uint32_t trim_error)
{
    app_batt_read.rsl15_vbat_mV_sum = 0;
    app_batt_read.rsl15_vbat_percent_prev = 0;
    app_batt_read.read_cnt = 0;

    if (!(trim_error & ERROR_LSAD_INVALID))
    {
        /* Using low frequency trims, as we are at max pre-scale */
        Sys_LSAD_Gain_Offset(&(trims->lsad_trim), &g_f_lsad_gain_offset[RSL15_VBAT_CHANNEL]);
    }
}

void LSAD_ChannelInit(void)
{
    LSAD->CFG = LSAD_DISABLE;

    /* Configure the assigned VBAT input channel in single ended configuration by connecting
     * the LSAD negative input to ground, connect the positive input to VBAT. */
    Sys_LSAD_InputConfig(RSL15_VBAT_CHANNEL,
                         LSAD_VBAT_DIV2_CFG, LSAD_GROUND_CFG);

    /* Configure the assigned compensation channel on both ends to VBAT,
     * to disable automatic offset compensation. */
    Sys_LSAD_InputConfig(LSAD_COMPENSATION_CHANNEL,
                         LSAD_VBAT_DIV2_CFG, LSAD_VBAT_DIV2_CFG);

    /* Monitor Channel 3 , No Alarm */
    LSAD->MONITOR_CFG = MONITOR_ALARM_NONE | MONITOR_CH3;

    LSAD->CFG = LSAD_NORMAL               /* Normal mode, all 8 channels sampled */
                | LSAD_PRESCALE_1280H;    /* Sample rate is SLOWCLK/1280 */

    /* Clear Interrupt Flags */
    LSAD->MONITOR_STATUS = LSAD_READY_CLEAR;

    /* Enable LSAD interrupt */
    Sys_LSAD_InterruptEnable(LSAD_INPUT_CH3);

    vbat_status = VBAT_MEASURING;
}


uint32_t App_VBAT_Measure(void)
{
    struct F_LSAD_TRIM *gain_offset = &g_f_lsad_gain_offset[RSL15_VBAT_CHANNEL];
    float lsad_rsl15_result = 0.0f;
    uint32_t rsl15_vbat_mV;


	/* Calculating voltage using gain and offset and multiplying by 2 since
	 * VBAT is divided by 2 when measured by the LSAD. */
	if (gain_offset->hf_gain != 0)
	{
		/* Computing LSAD result using the LSAD full scale of 2.0V and LSAD resolution
		 * of 14 bits */
		lsad_rsl15_result = CONVERT(LSAD->DATA_TRIM_CH[RSL15_VBAT_CHANNEL]) / V_TO_MV_F;

		/* Applying offset and gain compensation to calculated voltage value */
		rsl15_vbat_mV = (uint32_t)(((lsad_rsl15_result - gain_offset->hf_offset) /
				gain_offset->hf_gain * V_TO_MV) * LSAD_VBAT_FACTOR);
	}

	/* Calculating voltage using only reference voltage and LSAD data if gain is 0 */
	else
	{
		rsl15_vbat_mV = ((LSAD_VOLTAGE_RANGE_MV * LSAD_VBAT_FACTOR *
						  LSAD->DATA_TRIM_CH[RSL15_VBAT_CHANNEL]) / (LSAD_MAX_SIZE));
	}

    vbat_status = VBAT_DONE;

    return (uint32_t)(rsl15_vbat_mV/10);
}
