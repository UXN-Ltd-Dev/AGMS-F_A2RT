/**
 * @file  app_utils.c
 * @brief Source file for utilities
 *
 * @copyright @parblock
 * Copyright (c) 2024 Semiconductor Components Industries, LLC (d/b/a
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

#include <math.h>
#include "app.h"
#include "app_utils.h"
#include "app_customss.h"

uint32_t App_Utils_ConvertFloatToUInt32(float data)
{
    uint32_t outputValue = 0;

    if (data > (float)INT32_MAX)
    {
        outputValue = IEEE11073_POSITIVE_INFINITY;
        return outputValue;
    }
    else if (data < (float)INT32_MIN)
    {
        outputValue = IEEE11073_NEGATIVE_INFINITY;
        return outputValue;
    }
    else if (data == 0)
    {
        return outputValue;
    }

    int8_t exponent = 0;
    double mantissa = fabs((double)data);
    int8_t sign = data > 0 ? +1 : -1;

    for (; mantissa < (1 << IEEE11073_MANTISSA_MAX); exponent--)
    {
        mantissa *= 10.0;
    }

    for (; mantissa > (1 << IEEE11073_MANTISSA_MAX); exponent++)
    {
        mantissa /= 10.0;
    }

    if (exponent > IEEE11073_EXPONENT_MAX)
    {
        outputValue = (uint32_t)(sign > 0 ? IEEE11073_POSITIVE_INFINITY : IEEE11073_NEGATIVE_INFINITY);
        return outputValue;
    }

    if (exponent < IEEE11073_EXPONENT_MIN)
    {
        return outputValue;
    }

    int32_t finalMantissa = (int32_t)round(sign * mantissa);
    outputValue = (uint32_t)(((exponent << 24) & 0xFF000000) | ((finalMantissa << 0) & 0x00FFFFFF));

    return outputValue;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
/* ----------------------------------------------------------------------------
 * Private Function Definitions
 * ------------------------------------------------------------------------- */
void system_delay_ms(int tmout)
{
	do {
		SYS_WATCHDOG_REFRESH();
		Sys_Delay(APP_SYS_DELAY_1MS);					//
	}while(tmout-->0);
}



/**************************************************************************************************
 *	UNIX TIME 2026년 01월 01일 00시 00분 00초를 기준으로 정리 한다.
 **************************************************************************************************/
void set_epoch_time(calendar_t *t, uint32_t epoch)
{
    t->base_epoch = epoch;
    t->base_second= Sys_RTC_Value_Seconds();

	swmLogInfo("set_epoch : %d, %d\r\n", t->base_epoch, t->base_second);
}

uint32_t get_epoch_time(calendar_t *t)
{
    uint32_t progressed_time;
    uint32_t now_second;

    now_second = Sys_RTC_Value_Seconds();

    if(now_second > t->base_second) {
    	progressed_time = now_second - t->base_second;
    	t->base_epoch+=progressed_time;
    }

    t->base_second = now_second;

    swmLogInfo("get_epoch 2 : %d, %d, %d\r\n", now_second, progressed_time, t->base_epoch);

    return t->base_epoch;
}


int is_leap_year(int year)
{
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

/*********************************************************************************
 *	EPOCH TIME --> converter date & time
 **********************************************************************************/
void epoch_time_to_calendar(calendar_t *t, uint32_t epoch_time)
{
	uint32_t epoch = 0;
	int year = 2026;
	int month = 1;
	int day = 1;

	int sec, min, hour;
	int days;

//	epoch = get_epoch_time(t);
	epoch = epoch_time;

    /* 초 → 시/분/초 */
    sec  = epoch % 60;
    epoch /= 60;

    min  = epoch % 60;
    epoch /= 60;

    hour = epoch % 24;
    days = epoch / 24;

    /* 날짜 계산 */
    while (1)	{
        int days_in_year = is_leap_year(year) ? 366 : 365;
		if (days >= days_in_year)	{
            days -= days_in_year;
            year++;
        }
        else	{
			break;
        }
    }

    int days_in_month[12] = {
        31,
        is_leap_year(year) ? 29 : 28,
        31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    while (days >= days_in_month[month - 1])    {
		days -= days_in_month[month - 1];
		month++;
    }

	day += days;

	t->year = year-2000;
	t->mon = month;
	t->day = day;
	t->hour = hour;
	t->min = min;
	t->sec = sec;
}


/************************************************************************************************************
 *	FLASH MEMORY INITIAL & READ & WRITE
 ************************************************************************************************************/
void App_Flash_Config_Init(void)
{
    /* Power up and initialize flash timing registers based on SystemClock */
    Flash_Initialize(0, FLASH_CLOCK_16MHZ);
}



FlashStatus_t App_Write_Flash_Buffer(uint32_t addr, uint32_t *result_data, size_t value_size)
{
    /* Perform a sector erase in the default endurance mode */
    FlashStatus_t result;

    result = Flash_EraseSector(addr, 0);
    if (result != FLASH_ERR_NONE)
    	return result;

    result = Flash_WriteBuffer(addr, value_size, result_data, 0);
    if (result != FLASH_ERR_NONE)
    	return result;

    return FLASH_ERR_NONE;
}


FlashStatus_t App_Read_Flash_Buffer(uint32_t addr, uint32_t *result_data, size_t value_size)
{
    FlashStatus_t result;

    /* Perform a re-verification of written data for illustration purposes */
    result = Flash_ReadBuffer(addr, (uint32_t)result_data, value_size);
    if (result != FLASH_ERR_NONE)
    	return result;

    return FLASH_ERR_NONE;
}

/*************************************************************************************************
 *	TEMPERATURE ABSOLUTE OFFSET
 *	Read_Temperature_Offset((int32_t*)nvr4_buffer);
 *************************************************************************************************/
int8_t read_nvr4_lsad_temp_info(int16_t *offset_data)
{
	uint32_t nvr4_buffer[4];

	App_Read_Flash_Buffer(FLASH0_NVR4_BASE, nvr4_buffer, 4);
	*offset_data = (int16_t)(nvr4_buffer[0]);

	return (int8_t)(nvr4_buffer[1]);
}

void write_nvr4_lsad_temp_info(uint32_t  offset_data, uint32_t  offset_status)
{
	uint32_t nvr4_buffer[4];

	memset(nvr4_buffer, 0, sizeof(nvr4_buffer));

	App_Read_Flash_Buffer(FLASH0_NVR4_BASE, nvr4_buffer, 4);

	nvr4_buffer[0] = offset_data;
	nvr4_buffer[1] = offset_status;
	nvr4_buffer[2] = 'G';

#ifdef SWMTRACE_OUTPUT
	swmLogInfo("lsad temperature : [%d] [%d] [%d] [%d]\r\n", nvr4_buffer[0], nvr4_buffer[1], nvr4_buffer[2], nvr4_buffer[3]);
#endif

	App_Write_Flash_Buffer(FLASH0_NVR4_BASE, nvr4_buffer, 4);
}





