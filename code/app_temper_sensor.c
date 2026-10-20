/**
 * @file    app_temperature_sensor.c
 * @brief   Source file for the internal temperature sensor interface.
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

/* ----------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------- */

/* Device and library headers */
#include <hw.h>
#include <app.h>

/* Application headers */
#include "app_temper_sensor.h"

/* ----------------------------------------------------------------------------
 * Private Symbolic Constants
 * ------------------------------------------------------------------------- */

/** Reference point of the one-point calibration: 37.00 degC, scaled by 100 */
#define ABSOLUTE_TEMPERATURE_OFFSET     (3700)

/** Representable range of the x100 scaled temperature (uint16_t storage) */
#define TEMPERATURE_X100_MIN            (0L)
#define TEMPERATURE_X100_MAX            (65535L)

/** IEEE-11073 exponent for a x100 scaled mantissa: 10^-2 */
#define IEEE11073_EXPONENT_MINUS_2      (0xFEU)

/* ----------------------------------------------------------------------------
 * Private Macros
 * ------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------------
 * Private Type Definitions
 * ------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------------
 * Global Variables
 * ------------------------------------------------------------------------- */

/* Initialize a structure for LSAD gain and offset values from TRIM sector.
 * NOTE: currently only populated for reference - the conversion below derives
 *       its own scale/offset from trims->measured. Kept so the trim-corrected
 *       gain can be wired in later without changing the init flow. */
static struct F_LSAD_TRIM lsad_gain_offset = {0};

/* Temperature sensor scale (LSB per degree Celsius) */
static float temperature_scale = 0;

/* Temperature sensor offset (degrees Celsius) */
static float temperature_offset = 0;

/* ----------------------------------------------------------------------------
 * Private Function Prototypes
 * ------------------------------------------------------------------------- */

/**
 * @brief       Initialize the temperature offset and scaler factors
 * @details     Computes an offset and scaler factor to be used to convert LSAD
 *              measurements into temperature readings. If there are any trim
 *              errors, the user-defined default values are used.
 * @param       uint32_t trim_error
 *                  A code indicating if any trim errors occurred during
 *                  initialization.
 * @return      N/A
 */
static void App_TempSensor_InitOffset(uint32_t trim_error);

/**
 * @brief       Initializes LSAD input channels used for the temperature sensor
 * @detail      Enables the internal temperature sensor block. The LSAD channel
 *              itself is configured on demand in LSAD_measure_sensor_level().
 * @param       N/A
 * @return      N/A
 */
static void App_TempSensor_InitLSAD(void);

/**
 * @brief       Clamp a x100 scaled temperature into the uint16_t storage range
 * @param       int32_t value_x100
 * @return      uint16_t
 */
static uint16_t Temp_ClampToU16(int32_t value_x100);

/**
 * @brief       Pack a x100 scaled temperature into an IEEE-11073 32-bit FLOAT
 * @param       int32_t mantissa_x100
 * @return      uint32_t
 */
static uint32_t Temp_PackIEEE11073(int32_t mantissa_x100);

/* ----------------------------------------------------------------------------
 * Private Function Definitions
 * ------------------------------------------------------------------------- */

static void App_TempSensor_InitOffset(uint32_t trim_error)
{
    /* Initialize a pointer to the default trim value sector */
    TRIM_Type *trims = TRIM;

    /* No trim error reported: the temperature sensor gain/offset trims are
     * valid, so load them. High-frequency trims are used because a high-speed
     * pre-scaler is in use. */
    if (!(trim_error & ERROR_TEMPERATURE_INVALID))
    {
        Sys_LSAD_TempSensor_Gain_Offset(&(trims->temp_sensor), &lsad_gain_offset);
    }

    /* Check for invalid trim values */
    if (trims->measured.temp_sensor_high == 0xFFFF ||
        trims->measured.temp_sensor_30C  == 0xFFFF)
    {
        /* Set scaler and offset to user-defined default values */
        temperature_scale  = DEFAULT_TEMPERATURE_GAIN;
        temperature_offset = DEFAULT_TEMPERATURE_OFFSET;
    }
    else
    {
        /* Compute gain and offset from trim values */
        uint16_t temp_sensor_high = trims->measured.temp_sensor_high;
        uint16_t temp_sensor_30C  = trims->measured.temp_sensor_30C;
#ifdef RSL15_CID
        temperature_scale = (float)(temp_sensor_high - temp_sensor_30C) / 60.0f;
#else /* ifdef RSL15_CID */
        temperature_scale = (float)(temp_sensor_high - temp_sensor_30C) / 20.0f;
#endif /* ifdef RSL15_CID */
        temperature_offset = (temp_sensor_30C / temperature_scale) - 30.0f;
    }
}

static void App_TempSensor_InitLSAD(void)
{
    /* Enable the internal temperature sensor.
     * The LSAD input mux and sample rate are configured in
     * LSAD_measure_sensor_level(), immediately before each measurement. */
    uint32_t sensor_cfg_register = LSAD_TEMP_SENS_DUTY | TEMP_SENS_ENABLE;

    Sys_ACS_WriteRegister(&ACS->TEMP_SENSOR_CFG, sensor_cfg_register);
}

static uint16_t Temp_ClampToU16(int32_t value_x100)
{
    if (value_x100 < TEMPERATURE_X100_MIN)    {
        return (uint16_t)TEMPERATURE_X100_MIN;
    }

    if (value_x100 > TEMPERATURE_X100_MAX)    {
        return (uint16_t)TEMPERATURE_X100_MAX;
    }

    return (uint16_t)value_x100;
}

static uint32_t Temp_PackIEEE11073(int32_t mantissa_x100)
{
    /* Exponent in bits [31:24], two's complement signed mantissa in [23:0].
     * The uint32_t casts avoid the signed-overflow UB of (0xFE << 24). */
    return ((((uint32_t)IEEE11073_EXPONENT_MINUS_2) << 24) & 0xFF000000U)
         | (((uint32_t)mantissa_x100) & 0x00FFFFFFU);
}

/* ----------------------------------------------------------------------------
 * Public Function Definitions
 * ------------------------------------------------------------------------- */

void App_TempSensor_Init(uint32_t trim_error)
{
    /* Initialize the gain and offset used for measurement conversion */
    App_TempSensor_InitOffset(trim_error);

    /* Enable the internal temperature sensor block */
    App_TempSensor_InitLSAD();

    enable_Temperature_Task();


#if (TEMP_CALI_MODE == TEMP_CALI_ENABLE)
    lsad_temp.calib_status = read_nvr4_lsad_temp_info(&lsad_temp.calib_average);

    memset(APP_DEV_VER_BUFF, 0, sizeof(APP_DEV_VER_BUFF));
    memcpy(APP_DEV_VER_BUFF, APP_DEV_VER, 4);

    /* 'T' = calibrated offset present, 'C' = still accumulating */
    if (lsad_temp.calib_status == TEMP_CALIB_ALLOW) {
        APP_DEV_VER_BUFF[4] = 'T';
    }
    else {
        APP_DEV_VER_BUFF[4] = 'C';
    }

#ifdef SWMTRACE_OUTPUT
    swmLogInfo("CAL STATUS : %d , OFFSET : [%d]\r\n",
    		lsad_temp.calib_status, lsad_temp.calib_average);
#endif /* SWMTRACE_OUTPUT */
#endif /* TEMP_CALI_MODE */
}

uint32_t App_TempSensor_Measure(void)
{
    /* Retrieve the LSAD measurement */
    uint32_t lsad_value = LSAD->DATA_TRIM_CH[LSAD_TEMP_CHANNEL];

    /* Compute the temperature from the LSAD measurement. The value is scaled
     * up two decimal places to increase accuracy. */
    int32_t mantissa = (int32_t)(Convert_LSAD_To_Celsius(lsad_value) * 100.0f);

    /* Store the temperature encoded as an IEEE-11073 32-bit floating point */
    return Temp_PackIEEE11073(mantissa);
}

/************************************************************************************************************
 *  LSAD Initial & run & measure
 *
 *  LSAD는 NORMAL 모드에서 CH0~CH7을 순차적으로 자유 구동(free-running) 변환한다.
 *  소프트웨어 변환 시작/BUSY 핸드셰이크가 없으므로, 서로 다른 N개의 샘플을 얻으려면
 *  full scan 주기(LSAD_SCAN_DELAY)마다 한 번씩 읽는 방법밖에 없다.
 *  PRESCALE_1280H 기준 채널당 1280 SLOWCLK, 8채널 → 한 스캔 약 10ms.
 *
 *  첫 스캔은 입력 mux 변경 전에 시작되었을 수 있으므로 버린다.
 *  총 블로킹 시간 ≈ (LSAD_TEMP_SAMPLE_COUNT + 1) * LSAD_SCAN_DELAY.
 ************************************************************************************************************/
void lsad_temperature_measure_handler(void)
{
    uint32_t i;
    uint32_t raw_sum = 0;
    uint32_t raw_avg;
    float    final_celsius;
    int32_t  temp_x100;

    /* Stop the LSAD before re-configuring the input mux */
    LSAD->CFG = LSAD_DISABLE;

    /* Configure the input channel in single ended mode with LSAD positive
     * connected to the temperature sensor and LSAD negative to ground */
    Sys_LSAD_InputConfig(LSAD_TEMP_CHANNEL,
                         LSAD_TEMP_CFG,
                         LSAD_GROUND_CFG);

    /* Configure the sampling mode and rate for the LSAD */
#if (RSL15_CID == 202)
    LSAD->CFG = LSAD_NORMAL             /* Normal mode, sample all 8 channels */
              | LSAD_PRESCALE_1280H;    /* Sample rate is SLOWCLK/1280 */
#else /* if RSL15_CID == 202 */
    LSAD->CFG = VBAT_DIV2_ENABLE        /* Enable VBAT voltage divider */
              | LSAD_NORMAL             /* Normal mode, sample all 8 channels */
              | LSAD_PRESCALE_1280H;    /* Sample rate is SLOWCLK/1280 */
#endif /* if RSL15_CID == 202 */

    /* Discard the first scan - it may predate the input mux change */
    SYS_WATCHDOG_REFRESH();
    Sys_Delay(LSAD_SCAN_DELAY);
    (void)LSAD->DATA_TRIM_CH[LSAD_TEMP_CHANNEL];

    /* One sample per full scan */
    for (i = 0; i < LSAD_TEMP_SAMPLE_COUNT; i++)
    {
        SYS_WATCHDOG_REFRESH();
        Sys_Delay(LSAD_SCAN_DELAY);
        raw_sum += LSAD->DATA_TRIM_CH[LSAD_TEMP_CHANNEL];
    }

    raw_avg       = raw_sum / LSAD_TEMP_SAMPLE_COUNT;
    final_celsius = Convert_LSAD_To_Celsius(raw_avg);
    temp_x100     = (int32_t)(final_celsius * 100.0f);


#if (TEMP_CALI_MODE == TEMP_CALI_ENABLE)
    if (lsad_temp.calib_status == TEMP_CALIB_ALLOW)	{						// 보정이 된 상태라면
        temp_x100 += (ABSOLUTE_TEMPERATURE_OFFSET - (int32_t)lsad_temp.calib_average);
        device_info.celsius = Temp_ClampToU16(temp_x100);
    }

    else {
    	device_info.celsius = Temp_ClampToU16(temp_x100);
    	lsad_temperature_calibration(device_info.celsius);
    }

    swmLogInfo("Temp: raw[%d] avg[%d] -> [%d] cal[%d]\r\n",
               raw_sum, raw_avg, device_info.celsius, lsad_temp.calib_status);

#else /* if TEMP_CALI_MODE == TEMP_CALI_ENABLE */
    device_info.celsius = Temp_ClampToU16(temp_x100);

    swmLogInfo("Temp: raw[%d] avg[%d] -> [%d]\r\n",
               raw_sum, raw_avg, device_info.celsius);
#endif /* if TEMP_CALI_MODE == TEMP_CALI_ENABLE */

    /* LSAD 비활성화 (저전력) */
    LSAD->CFG = LSAD_DISABLE;
}

/************************************************************************************************************************
 *  NEW TEMPERATURE
 ************************************************************************************************************************/

/* ADC 원시 값을 섭씨(float)로 변환하는 함수 */
float Convert_LSAD_To_Celsius(uint32_t lsad_value)
{
    /* 공식: Temp = (ADC_Value / Scale) - Offset */
    return ((float)lsad_value / temperature_scale) - temperature_offset;
}

/* IEEE-11073 포맷 반환 함수 (참고용) */
uint32_t App_TempSensor_Measure_IEEE(uint32_t lsad_value)
{
    /* 섭씨 값을 구한 뒤 100을 곱해 정수부(mantissa) 생성 */
    float   celsius  = Convert_LSAD_To_Celsius(lsad_value);
    int32_t mantissa = (int32_t)(celsius * 100.0f);

    swmLogInfo("mantissa:[%d] ==> ", mantissa);

    /* 지수(exponent)는 -2 (즉, 10^-2 = 0.01) */
    return Temp_PackIEEE11073(mantissa);
}


/************************************************************************************************************************
 *  NEW LSAD TEMPERATURE CALIBRATION
 ************************************************************************************************************************/
void lsad_temperature_run_mode(void)
{
	lsad_temp.calib_step = TEMP_CALIB_RUN_STEP;	// 보정 시작
	lsad_temp.calib_run_count = 0;				// 보정 시간 초기화
	lsad_temp.calib_average  = 0;				// 전체 평균 값 초기화...
	lsad_temp.calib_sum  = 0;					// 전체 총합 값 초기화...
}


/************************************************************************************************************************
 *  LSAD TEMPERATURE CALIBRATION 대기 및 시작
 ************************************************************************************************************************/
#define TEMP_CALIB_WAIT_TIME	90
#define TEMP_CALIB_RUN_COUNT	30						// 24 is 30 / 6 = 5 min
void lsad_temperature_calibration(uint16_t now_Temp)
{
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("lsad_temp.calib_status:[%d][%d]\r\n", lsad_temp.calib_step, now_Temp);
    Sys_Delay(SystemCoreClock / 1000);
#endif

    switch (lsad_temp.calib_step)	{								// 챔버 보정 시간 안정화...10분 = 10sec * 60 = 600sec is 10min
        case TEMP_CALIB_WAIT_STEP:
        	lsad_temp.calib_wait_time++;
        	lsad_temp.calib_wait_time%=TEMP_CALIB_WAIT_TIME;
        	if(lsad_temp.calib_wait_time==0) {
        		lsad_temperature_run_mode();
        	}
            break;

        case TEMP_CALIB_RUN_STEP:
        	lsad_temp.calib_run_count++;
        	lsad_temp.calib_sum+=now_Temp;
        	lsad_temp.calib_run_count%=TEMP_CALIB_RUN_COUNT;
        	if(lsad_temp.calib_run_count==0) {
        		lsad_temp.calib_average = (int16_t)((double)lsad_temp.calib_sum/TEMP_CALIB_RUN_COUNT);

        		lsad_temp.calib_status = TEMP_CALIB_ALLOW;
        		lsad_temp.calib_step = TEMP_CALIB_IDLE_STEP;

        		write_nvr4_lsad_temp_info((uint32_t)lsad_temp.calib_average, (uint32_t)lsad_temp.calib_status);
                swmLogInfo("TEMP_CALIB_CUMU_STEP:[%d]\r\n", lsad_temp.calib_average);
        	}
        	break;

        case TEMP_CALIB_IDLE_STEP:
            break;

        default:
            break;
    }
}
