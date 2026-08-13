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
#include "app_temperature_sensor.h"

/* ----------------------------------------------------------------------------
 * Private Symbolic Constants
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Private Macros
 * ------------------------------------------------------------------------- */
#ifndef LSAD_CTRL_STRT_Msk
#define LSAD_CTRL_STRT_Msk      (0x00000001U)
#endif

#ifndef LSAD_STATUS_BUSY_Msk
#define LSAD_STATUS_BUSY_Msk    (0x00000100U)
#endif

/* ----------------------------------------------------------------------------
 * Private Type Definitions
 * ------------------------------------------------------------------------- */
//#define NUM_LSAD_SAMPLE_COUNT			8

#define ABSOLUTE_TEMPERATURE_OFFSET		3700

/* ----------------------------------------------------------------------------
 * Global Variables
 * ------------------------------------------------------------------------- */

/* Initialize a structure for LSAD gain and offset values from TRIM sector */
static struct F_LSAD_TRIM lsad_gain_offset = {0};

/* Temperature sensor scale */
static float temperature_scale = 0;

/* Temperature sensor offset */
static float temperature_offset = 0;


uint16_t encoded_temp_buff[16];
uint8_t  encoded_temp_rot = 0;
uint8_t  encoded_temp_pos = 0;

//uint32_t raw_LSAD_sum[8];
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
 * @detail      Configures the LSAD channel used to measure the internal
 *              temperature sensor, and configures the LSAD interrupt.
 * @param       N/A
 * @return      N/A
 */
static void App_TempSensor_InitLSAD(void);

float Convert_LSAD_To_Celsius(uint32_t lsad_value);
uint32_t App_TempSensor_Measure_IEEE(uint32_t lsad_value);

/* ----------------------------------------------------------------------------
 * Private Function Definitions
 * ------------------------------------------------------------------------- */

static void App_TempSensor_InitOffset(uint32_t trim_error)
{
    /* Initialize a pointer to the default trim value sector */
    TRIM_Type *trims = TRIM;

    /* Check for trim errors */
    if (!(trim_error & ERROR_TEMPERATURE_INVALID))
    {
        /* The temperature sensor gain and/or offset are invalid. Use
         * high-frequency trims since we are using a high-speed pre-scaler */
        Sys_LSAD_TempSensor_Gain_Offset(&(trims->temp_sensor), &lsad_gain_offset);
    }

    /* Check for invalid trim values */
    if (trims->measured.temp_sensor_high == 0xFFFF || trims->measured.temp_sensor_30C == 0xFFFF)
    {
        /* Set scaler and offset to user-define default values */
        temperature_scale = DEFAULT_TEMPERATURE_GAIN;
        temperature_offset = DEFAULT_TEMPERATURE_OFFSET;
    }
    else
    {
        /* Compute gain and offset from trim values */
        uint16_t temp_sensor_high = trims->measured.temp_sensor_high;
        uint16_t temp_sensor_30C = trims->measured.temp_sensor_30C;
#ifdef RSL15_CID
        temperature_scale = (float)(temp_sensor_high  - temp_sensor_30C) / 60.0f;
#else /* ifdef RSL15_CID */
        temperature_scale = (float)(temp_sensor_high  - temp_sensor_30C) / 20.0f;
#endif /* ifdef RSL15_CID */
        temperature_offset = (temp_sensor_30C / temperature_scale) - 30.0f;
    }
}



static void App_TempSensor_InitLSAD(void)
{

#if 0

    /* Configure the input channel in single ended mode with LSAD positive
     * connected to the temperature sensor and LSAD negative to ground */
    Sys_LSAD_InputConfig(
        LSAD_TEMP_CHANNEL,
        LSAD_TEMP_CFG,
        LSAD_GROUND_CFG
    );


    /*Configure the sampling mode and rate for the LSAD */
#if RSL15_CID == 202
    LSAD->CFG = LSAD_NORMAL             /* Normal mode, sample all 8 channels */
              | LSAD_PRESCALE_1280H;    /* Sample rate is SLOWCLK/1280 */
#else /* if RSL15_CID == 202 */
    LSAD->CFG = VBAT_DIV2_ENABLE        /* Enable VBAT voltage divider */
              | LSAD_NORMAL             /* Normal mode, sample all 8 channels */
              | LSAD_PRESCALE_1280H;    /* Sample rate is SLOWCLK/1280 */
#endif /* if RSL15_CID == 202 */
#endif


    /* Enable the internal temperature sensor */
    uint32_t sensor_cfg_register = LSAD_TEMP_SENS_DUTY | TEMP_SENS_ENABLE;
    Sys_ACS_WriteRegister(&ACS->TEMP_SENSOR_CFG, sensor_cfg_register);

}

/* ----------------------------------------------------------------------------
 * Public Function Definitions
 * ------------------------------------------------------------------------- */

void App_TempSensor_Init(uint32_t trim_error)
{
    /* Initialize the gain and offset used for measurement conversion */
    App_TempSensor_InitOffset(trim_error);

    /* Initialize the LSAD channel and interrupt */
    App_TempSensor_InitLSAD();


//----------------------------------------------------------------------------------
#if(TEMP_CALI_MODE == TEMP_CALI_ENABLE)
    Temp_calibration.cal_status = Read_Temperature_Offset(&Temp_calibration.absolute_offset);

    memset(APP_DEV_VER_BUFF, 0, sizeof(APP_DEV_VER_BUFF));
    memcpy(APP_DEV_VER_BUFF, APP_DEV_VER, 4);

    if(Temp_calibration.cal_status == TEMP_CALIB_ALLOW)
    	APP_DEV_VER_BUFF[4] = 'T';
    else
    	APP_DEV_VER_BUFF[4] = 'C';

#ifdef SWMTRACE_OUTPUT
	swmLogInfo("CAL STATUE : %d , OFFSET : [%d]\r\n", Temp_calibration.cal_status, Temp_calibration.absolute_offset);
#endif
#endif
}

uint32_t App_TempSensor_Measure(void)
{
    /* Retrieve the LSAD measurement */
    uint32_t lsad_value = LSAD->DATA_TRIM_CH[LSAD_TEMP_CHANNEL];

    /* Compute the temperature from the LSAD measurement. The value is scaled
     * up two decimal places to increase accuracy. */
    uint32_t mantissa = (uint32_t)(
        ((lsad_value / temperature_scale) - temperature_offset) * 100
    );

    /* Set the exponent value to -2 to compensate for the up-scaling */
    uint8_t exponent = 0xFE;

    /* Store the temperature encoded as an IEEE-11073 32-bit floating point */
    uint32_t temperature = (
        ((exponent << 24) & 0xFF000000) | ((mantissa << 0) & 0x00FFFFFF)
    );

    return temperature;
}



/************************************************************************************************************
 *	LSAD Initial & run & measure
 *	LSAD POLLING 방식은 10ms 간격으로 8번을 scan 해야 정상적인 데이터 값을 읽어 올 수 있다.
 *	LSAD POLLING은 LSAD_INPUT_CH0~CH7까지 순차적으로 변환이 되기 때문에 LSAD_INPUT_CH0 부터 값을 읽어 처리 한다.
 *	즉 LSAD_INPUT_CH0, CH1, CH2, CH3 이기 때문에 최소 40ms대기 해야 한다.
 *	모니터링 방식을 사용하면 되지 않는다...
 ************************************************************************************************************/
void LSAD_measure_sensor_level(void)
{
#if 1
	uint8_t i;
	int raw_sum = 0;
	int offset_temp;

    float final_celsius = 0.0f;

    /* Stop the LSAD sampling */
    LSAD->CFG = LSAD_DISABLE;

    Sys_LSAD_InputConfig(LSAD_TEMP_CHANNEL,
    						LSAD_TEMP_CFG,
							LSAD_GROUND_CFG);

    /* Configure the sampling mode and rate for the LSAD */
    LSAD->CFG = LSAD_NORMAL             /* Normal mode, sample all 8 channels */
              | LSAD_PRESCALE_1280H;    /* Sample rate is SLOWCLK/1280 */

	Sys_Delay(LSAD_SCAN_DELAY);					// 10ms * ms =

    for (i = 0; i < 8; i++)    {
    	LSAD->MONITOR_STATUS = LSAD_CTRL_STRT_Msk;						// 변환 시작 (CTRL 대신 MONITOR_STATUS 사용)
    	while ((LSAD->MONITOR_STATUS & LSAD_STATUS_BUSY_Msk) != 0);		// 변환 대기 (STATUS 대신 MONITOR_STATUS 사용)
        raw_sum += LSAD->DATA_TRIM_CH[LSAD_TEMP_CHANNEL];				// 데이터 누적
        Sys_Delay(SystemCoreClock / 100 ); 								// 1ms
    }

    uint32_t raw_avg = raw_sum / 8;
    final_celsius = Convert_LSAD_To_Celsius(raw_avg);
    rsl15_info->temperature = (uint16_t)(final_celsius*100);


#if(TEMP_CALI_MODE == TEMP_CALI_ENABLE)
	if(Temp_calibration.cal_status == TEMP_CALIB_ALLOW) {					// 보정된 값이 있으면 계산을 할 필요가 없다...
		offset_temp = ABSOLUTE_TEMPERATURE_OFFSET - Temp_calibration.absolute_offset;
		rsl15_info->temperature = rsl15_info->temperature + offset_temp;
	}
	else {
		Temperature_Cumulative_Average(rsl15_info->temperature);
	}
#endif

	swmLogInfo( "Temp:[%d][%d] -->[%d][%d] \r\n", raw_sum, rsl15_info->temperature, Temp_calibration.cal_status, Temp_calibration.cal_step);
    Sys_Delay(SystemCoreClock / 100 ); 								// 1ms


    /* LSAD 비활성화 (저전력) */
    LSAD->CFG = LSAD_DISABLE;

#else
	int i, j;
	int offset_temp;
	uint32_t encoded_temp = 0;

	uint32_t encoded_buff[8];
	double encoded_ave;


    /* Stop the LSAD sampling */
    LSAD->CFG = LSAD_DISABLE;

	Sys_LSAD_InputConfig(
			LSAD_TEMP_CHANNEL,
			LSAD_TEMP_CFG,
			LSAD_GROUND_CFG);

	    /* Configure the sampling mode and rate for the LSAD */
	#if RSL15_CID == 202
	    LSAD->CFG = LSAD_NORMAL             /* Normal mode, sample all 8 channels */
	              | LSAD_PRESCALE_1280H;    /* Sample rate is SLOWCLK/1280 */
	#else /* if RSL15_CID == 202 */
	    LSAD->CFG = VBAT_DIV2_ENABLE        /* Enable VBAT voltage divider */
	              | LSAD_NORMAL             /* Normal mode, sample all 8 channels */
	              | LSAD_PRESCALE_1280H;    /* Sample rate is SLOWCLK/1280 */
	#endif /* if RSL15_CID == 202 */

#if 0
	for(i = LSAD_INPUT_CH0; i <= (LSAD_INPUT_CH1+1); i++){
		SYS_WATCHDOG_REFRESH();
		Sys_Delay(LSAD_SCAN_DELAY);					// 10ms * 3 = = 30ms delay...
	}
#else
	for(i = 0; i < 8; i++){
#if 0
		for(j = LSAD_INPUT_CH0; j <= (LSAD_INPUT_CH1+1); j++){
			SYS_WATCHDOG_REFRESH();
			Sys_Delay(LSAD_SCAN_DELAY);					// 10ms * 3 = = 30ms delay...
		}
#endif
		SYS_WATCHDOG_REFRESH();
		Sys_Delay(LSAD_SCAN_DELAY);					// 10ms * 3 = = 30ms delay...

		encoded_temp = App_TempSensor_Measure();
		encoded_buff[i] = (uint16_t)encoded_temp;
	}

	encoded_temp = 0;
	for(i = 0; i < 8; i++){
		encoded_temp+=encoded_buff[i];
	}

	encoded_ave = (double)encoded_temp/8;
	encoded_temp = (uint16_t)encoded_ave;

	swmLogInfo( "Temp:[%d][%d][%d][%d][%d][%d][%d][%d] ave : [%d]\r\n", encoded_buff[0],encoded_buff[1],encoded_buff[2],encoded_buff[3],encoded_buff[4],
			encoded_buff[5],encoded_buff[6],encoded_buff[7],encoded_temp);

#endif

	encoded_temp = App_TempSensor_Measure();
	rsl15_info->temperature = (uint16_t)encoded_temp;		// x.xx 의 FACTOR 값을 올려 보정 한다.

#if(TEMP_CALI_MODE == TEMP_CALI_ENABLE)
	if(Temp_calibration.cal_status == TEMP_CALIB_ALLOW) {					// 보정된 값이 있으면 계산을 할 필요가 없다...
		offset_temp = Temp_calibration.absolute_offset - ABSOLUTE_TEMPERATURE_OFFSET;
		rsl15_info->temperature = rsl15_info->temperature + offset_temp;
	}
	else {
		Temperature_Cumulative_Average(rsl15_info->temperature);
	}
#endif

#if 0	//#ifdef SWMTRACE_DEBUG
	swmLogInfo( "Temp:[%d] [%d.%d]  Ave:[%d] Cal:[%d]\r\n", rsl15_info->temperature, rsl15_info->temperature/100, rsl15_info->temperature%100,
				 Temp_calibration.current_ave, (uint16_t)encoded_temp);
#endif /* SWMTRACE_DEBUG */

    /* Stop the LSAD sampling */
    LSAD->CFG = LSAD_DISABLE;

#endif


}


/*****************************************************************************************************************************
 *	CEM 102 대시 시간 동안 온도 센서 또한 대기를 한다..AFE Waiting Time 때문에 자동 지연이 된다...
 ****************************************************************************************************************************/
void  Temperature_Cumulative_Initial(void)
{
//	Temp_calibration.cal_step = TEMP_CALIB_CUMU_STEP;		// 8 sec * 150 = 1200sec = 1200sec / 60sec  = 20min
	Temp_calibration.cal_step = TEMP_CALIB_WAIT_STEP;		// 초기 시간 지연을 해야 한다...단 AFE_WAKEUP_WAIT_TIME시간에 따라 다르다..

	Temp_calibration.cumulative_count = 0;
	Temp_calibration.current_ave = 0;
}
/*****************************************************************************************************************************
 *	10분동안 온도 평균을 확인 하고 차수를 구한 뒤 메모리에 저장을 한다. 단 이미 저장된 값을 경우 평균을 구하지 않고 그 값을 활용 한다.
 ****************************************************************************************************************************/
void Temperature_Cumulative_Average(uint16_t now_Temp)
{
	double current_ave;

	switch(Temp_calibration.cal_step) {
		case TEMP_CALIB_WAIT_STEP:										// 온도 안정화 대기 시간..
			Temp_calibration.wait_count++;
			if(Temp_calibration.wait_count >= 90) {
				Temp_calibration.cal_step = TEMP_CALIB_CUMU_STEP;		// 10 sec * 90 = 900sec = 900sec / 60sec  = 15min

				Temp_calibration.wait_count = 0;
				Temp_calibration.cumulative_count = 0;
				Temp_calibration.current_ave = 0;
			}

#if _CUSTOM_DEBUG_
			swmLogInfo( "Temperature Wait Time :[%d]\r\n", Temp_calibration.wait_count);
	        Sys_Delay(SystemCoreClock / 100 ); 								// 1ms
#endif
			break;

		case TEMP_CALIB_CUMU_STEP:										// 안정화 된 온도 읽어 오기...평균 계산...
			Temp_calibration.cumulative_count++;
			current_ave = (double)Temp_calibration.current_ave;

			current_ave = current_ave + ((double)now_Temp - current_ave)/Temp_calibration.cumulative_count;
			Temp_calibration.current_ave = (uint16_t)current_ave;

			if(Temp_calibration.cumulative_count >= 30) {				// 30회 이상 평균을 낸 뒤...8sec * 30 = 240sec / 60sec = 4min
				Temp_calibration.cal_status = TEMP_CALIB_ALLOW;			// 저장을 한다..
				Temp_calibration.cal_step = TEMP_CALIB_IDLE_STEP;

				Temp_calibration.absolute_offset = Temp_calibration.current_ave;

				Write_Temperature_Offset(Temp_calibration.current_ave, TEMP_CALIB_ALLOW);
			}
			break;

		case TEMP_CALIB_IDLE_STEP:
			break;

		default:
			break;
	}

}


/************************************************************************************************************************
 *	NEW TEMPERATURE
 ************************************************************************************************************************/
/* ADC 원시 값을 섭씨(float)로 변환하는 함수 */
float Convert_LSAD_To_Celsius(uint32_t lsad_value)
{
    /* 공식: Temp = (ADC_Value / Scale) - Offset */
    return ((float)lsad_value / temperature_scale) - temperature_offset;
}

/* 업로드된 파일의 IEEE-11073 포맷 반환 함수 (참고용) */
uint32_t App_TempSensor_Measure_IEEE(uint32_t lsad_value)
{
    /* 섭씨 값을 구한 뒤 100을 곱해 정수부(mantissa) 생성 */
    float celsius = Convert_LSAD_To_Celsius(lsad_value);

    int32_t mantissa = (int32_t)(celsius * 100.0f);

    swmLogInfo( "mantissa:[%d] ==> ", mantissa);

    /* 지수(exponent)는 -2 (즉, 10^-2 = 0.01) */
    uint8_t exponent = 0xFE;

    /* IEEE-11073 32-bit float 포맷 패킹 */
    uint32_t encoded_temp = (
        ((exponent << 24) & 0xFF000000) | ((mantissa << 0) & 0x00FFFFFF)
    );

    return encoded_temp;
}
