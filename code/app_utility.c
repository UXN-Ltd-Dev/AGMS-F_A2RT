/**
 * @file    app_init.c
 * @brief   Source file for application general initialization.
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

/* Application headers */
#include "app.h"

#include "app_init.h"
#include "app_utility.h"

#include "app_globals.h"

/* ----------------------------------------------------------------------------
 * Private Symbolic Constants
 * ------------------------------------------------------------------------- */
#define MAIN_VERSION		3
#define SUB1_VERSION		0
#define SUB2_VERSION		0


/* ----------------------------------------------------------------------------
 * Private Macros
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Private Type Definitions
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Global Variables
 * ------------------------------------------------------------------------- */
uint8_t end_of_day;
uint8_t month_day[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};



/* ----------------------------------------------------------------------------
 * Private Function Prototypes
 * ------------------------------------------------------------------------- */

/**
 * @brief       Initialize the system clocks
 * @details     Configures the system clock to use the XTAL oscillator and sets
 *              the RF clock as the system clock.
 * @param       N/A
 * @return      N/A
 */


/**
 * @brief       Initialize the standby (low-power) clocks
 * @details     Configures the standby clock to the XTAL32 or RC32 oscillators.
 * @param       N/A
 * @return      N/A
 */


/* ----------------------------------------------------------------------------
 * Private Function Definitions
 * ------------------------------------------------------------------------- */
void sys_delay_10ms(uint8_t ms)
{
	uint8_t tmout=0;

	tmout = ms;

	do {
        /* Refresh the watchdog timers */
		SYS_WATCHDOG_REFRESH();
		Sys_Delay((uint32_t)(0.01*SystemCoreClock));					// 10ms * ms =

	}while(tmout-->0);
}



//---------------------------------------------------------------------------------------------
//	COMPILE DATA & TIME
//---------------------------------------------------------------------------------------------
void set_date_time_default_clock(void)
{
	uint8_t sec;
	uint8_t min;
	uint8_t hour;
	uint8_t day;
	uint8_t month;
	uint16_t year;

	year = 0;
	year = (BUILD_YEAR_CH0-'0')*1000;
	year+= (BUILD_YEAR_CH1-'0')*100;
	year+= (BUILD_YEAR_CH2-'0')*10;
	year+= (BUILD_YEAR_CH3-'0');

	month = 0;
	month = (BUILD_MONTH_CH0-'0')*10;
	month+= (BUILD_MONTH_CH1-'0');

	day = 0;
	day = (BUILD_DAY_CH0-'0')*10;
	day+= (BUILD_DAY_CH1-'0');

	hour = 0;
	hour = (BUILD_HOUR_CH0-'0')*10;
	hour+= (BUILD_HOUR_CH1-'0');

	min = 0;
	min = (BUILD_MIN_CH0-'0')*10;
	min+= (BUILD_MIN_CH1-'0');

	sec = 0;
	sec = (BUILD_SEC_CH0-'0')*10;
	sec+= (BUILD_SEC_CH1-'0');

	set_current_date_time((year-2000), month, day, hour, min, sec);
}


void Product_Information_Service(void)
{
#ifdef SWMTRACE_OUTPUT
	swmLogInfo("*********************************************************************\r\n");
	swmLogInfo("*                                                                   *\r\n");
	swmLogInfo("* AGMS T10 VER %d.%d.%d | %s , %s has started.\r\n", MAIN_VERSION, SUB1_VERSION, SUB2_VERSION, __DATE__, __TIME__);
	swmLogInfo("*                                                                   *\r\n");
	swmLogInfo("*********************************************************************\r\n");
#endif
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	app_utils
//
uint32_t progress_time_condition(uint32_t taget_time, uint32_t check_time)
{
	uint32_t current_time_mm=0;
	uint32_t progress_time=0;

	current_time_mm = Sys_RTC_Value_Seconds();

	if(current_time_mm == taget_time)
		return 0;

	if(current_time_mm > taget_time)
		progress_time = current_time_mm -taget_time;
	else
		progress_time =(0xFFFFFFFF - taget_time) + current_time_mm;

	if(progress_time >= check_time)
	{
		if(current_time_mm == 0)
			current_time_mm++;
		return current_time_mm;
	}
	return 0;
}


uint32_t progress_current_time(void)
{
	uint32_t current_time_mm=0;
	uint32_t progress_time=0;

	current_time_mm = Sys_RTC_Value_Seconds();

	if(current_time_mm == current_date_time->base_sec)
		return 0;

	if(current_time_mm > current_date_time->base_sec)
		progress_time = current_time_mm -current_date_time->base_sec;
	else
		progress_time =(0xFFFFFFFF - current_date_time->base_sec) + current_time_mm;

	current_date_time->base_sec = current_time_mm;

	return progress_time;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	current data & time setting
//
void set_current_date_time(uint8_t year, uint8_t mon, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
	current_date_time->year = year;
	current_date_time->mon = mon ;
	current_date_time->day = day;
	current_date_time->hour = hour;
	current_date_time->min = min;
	current_date_time->sec = sec;

	current_date_time->base_sec = Sys_RTC_Value_Seconds();				// RTC CLOCK
}

void get_current_date_time(void)
{
	uint8_t second=0;
	uint8_t minute=0;
	uint32_t progress_time;

	progress_time = progress_current_time();

	if(progress_time>(CUSTOMSS_NOTIFY_ON_TIME*10)) {										// 10min * 60 = 600...error  -> jump rtc time 10min
		current_date_time->sec +=CUSTOMSS_NOTIFY_ON_TIME;		// default time...10sec
		current_date_time->update = false;							// RTC Time 을 다시 갱신 한다...
	}

	else {
		if(progress_time>60) {
			minute = (uint8_t)((float)progress_time/60);
			second = progress_time%60;

			current_date_time->min +=(uint8_t)minute;
			current_date_time->sec +=(uint8_t)second;
		}
		else {
			current_date_time->sec +=(uint8_t)progress_time;
		}
	}

//---------------------------------------------------------------------------------------------------------------------------------------------
//	CALCULATION RTC TIME
//---------------------------------------------------------------------------------------------------------------------------------------------
	if(current_date_time->sec>=60) {
		current_date_time->sec-=60;
		current_date_time->min++;
	}

	if(current_date_time->min>=60) {
		current_date_time->min-=60;
		current_date_time->hour++;
	}

	if(current_date_time->hour>=24) {
		current_date_time->hour-=24;
		current_date_time->day++;
	}

	if(current_date_time->mon==2) {
		if(((current_date_time->year%4==0) && (current_date_time->year%100 != 0)) || (current_date_time->year%400==0 ))		{
			month_day[1] = 29;
		}
		else {
			month_day[1] = 28;
		}
	}

	end_of_day = month_day[current_date_time->mon-1];
	if(current_date_time->day>end_of_day) {
		current_date_time->day = 1;
		current_date_time->mon++;
	}

	if(current_date_time->mon>12) {
		current_date_time->mon=1;
		current_date_time->year++;
	}

#if 0
	swmLogInfo("get_datetime : %4d/%02d/%02d %02d:%02d:%02d [%4d]\r\n ", current_date_time->year, current_date_time->mon, current_date_time->day,
			current_date_time->hour, current_date_time->min, current_date_time->sec, progress_time);
#endif

}



/***********************************************************************************************************************************
  *	IMPEDANCE 측정 시간을 가져 온다
  ***********************************************************************************************************************************/
void read_calendar(impedance_calendar_t *tm)
{
//	uint32_t epoch;

	get_current_date_time();

	tm->year = current_date_time->year;
	tm->mon  = current_date_time->mon;
	tm->day  = current_date_time->day;

	tm->hour = current_date_time->hour;
	tm->min  = current_date_time->min;
	tm->sec  = current_date_time->sec;

	tm->epoch = get_epoch_time(&epoch_time);

	tm->update = 1;
}


/***********************************************************************************************************************************
  *		FLASH MEMORY INITIAL & READ & WRITE
  ***********************************************************************************************************************************/
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


uint32_t Read_Deep_Sleep_Code(void)
{
	uint32_t nvr4_buffer[4];

	App_Read_Flash_Buffer(FLASH0_NVR4_BASE, nvr4_buffer, 4);

	return nvr4_buffer[0];

}


void Write_Deep_Sleep_Code(uint32_t  code_data)
{
	uint32_t nvr4_buffer[2];


	nvr4_buffer[0] = code_data;
	nvr4_buffer[1] = 0x00000000;

	App_Write_Flash_Buffer(FLASH0_NVR4_BASE, nvr4_buffer, 4);
}


/*************************************************************************************************
 *	TEMPERATURE ABSOLUTE OFFSET
 *	Read_Temperature_Offset((int32_t*)nvr4_buffer);
 *************************************************************************************************/
int8_t Read_Temperature_Offset(int16_t *offset_data)
{
	uint32_t nvr4_buffer[4];

	App_Read_Flash_Buffer(FLASH0_NVR4_BASE, nvr4_buffer, 4);

//	memcpy(offset_data, nvr4_buffer, sizeof(nvr4_buffer));
	*offset_data = (int16_t)(nvr4_buffer[0]);

	return (int8_t)(nvr4_buffer[1]);
}


void Write_Temperature_Offset(uint32_t  offset_data, uint32_t  offset_status)
{
	uint32_t nvr4_buffer[4];

	nvr4_buffer[0] = offset_data;
	nvr4_buffer[1] = offset_status;

	App_Write_Flash_Buffer(FLASH0_NVR4_BASE, nvr4_buffer, 4);
}


/**
 * @brief  RTC alarm을 처음 무장한다 (부팅 시 1회 호출).
 *         현재 counter 값 기준으로 N초 후에 발화하도록 threshold 설정 후
 *         RTC alarm event를 활성화한다.
 *
 * @param  seconds  알람 발화까지 대기 시간 (초 단위, 1Hz mode 기준)
 */
void App_RTC_Alarm_Init(uint32_t seconds)
{
    /* 이전 알람 sticky flag 클리어 */
    WAKEUP_RTC_ALARM_FLAG_CLEAR();

    /* 현재 RTC count 기준으로 N초 후를 threshold로 설정 */
    uint32_t next_thres = ACS->RTC_COUNT + seconds;
    Sys_RTC_Count_Threshold(next_thres);

    /* RTC를 1Hz mode 유지하면서 alarm event 활성화 */
    Sys_RTC_Config((RTC_CLK_SRC | RTC_CLOCK_1S),
                   (RTC_ENABLE | RTC_ENABLE_ALARM_EVENT | RTC_DISABLE_CLOCK_EVENT));
}

/**
 * @brief  RTC alarm을 다음 주기로 재무장한다 (알람 발화 후 호출).
 *         이전 threshold를 기준으로 누적하므로 처리 지연이 발생해도
 *         주기 drift가 누적되지 않는다.
 *
 * @param  seconds  다음 알람까지 대기 시간 (초 단위)
 */
void App_RTC_Alarm_Rearm(uint32_t seconds)
{
    /* 이전 threshold + 주기 → 정확한 주기 유지 (drift 없음) */
    uint32_t next_thres = ACS->RTC_COUNT_THRES + seconds;
    Sys_RTC_Count_Threshold(next_thres);

    /* 알람 flag 클리어 (다음 발화를 위해) */
    WAKEUP_RTC_ALARM_FLAG_CLEAR();
}



/**************************************************************************************************
 *	UNIX TIME 2026년 01월 01일 00시 00분 00초를 기준으로 정리 한다.
 **************************************************************************************************/
void set_epoch_time(epoch_time_t *t, uint32_t epoch)
{
    t->base_epoch = epoch;
    t->base_second= Sys_RTC_Value_Seconds();

	swmLogInfo("set_epoch : %d, %d\r\n", t->base_epoch, t->base_second);
}

uint32_t get_epoch_time(epoch_time_t *t)
{
    uint32_t progressed_time;
    uint32_t now_second;

    now_second = Sys_RTC_Value_Seconds();

    if(now_second > t->base_second) {
    	progressed_time = now_second - t->base_second;
    	t->base_epoch+=progressed_time;
    }
    else {
    	t->base_epoch+=10;								// custom time을 더 한다...
    }

    t->base_second = now_second;

    swmLogInfo("get_epoch : %d, %d, %d\r\n", now_second, progressed_time, t->base_epoch);

    return t->base_epoch;
//    return t->base_epoch + (now_second - t->base_second);
}
