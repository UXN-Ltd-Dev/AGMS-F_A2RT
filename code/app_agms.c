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
#include "app_agms.h"
#include "app_utility.h"

/* ----------------------------------------------------------------------------
 * Private Symbolic Constants
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Private Macros
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Private Type Definitions
 * ------------------------------------------------------------------------- */
#define FREQ_1000Hz   1000
#define FREQ_1Hz          1


/* ----------------------------------------------------------------------------
 * Global Variables
 * ------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------------
 * Private Function Prototypes
 * ------------------------------------------------------------------------- */

#define MAX_LOOP_COUNT	60

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	agms handler
//
uint16_t lsad_calc_crc16(uint16_t length)
{
    uint16_t i, j;
    unsigned int temp, temp2, flag;

    temp = 0xFFFF;

    for (i = 0; i < length; i++)    {
        temp = temp ^ agms_payload_buff[i];

        for (j = 1; j <= 8; j++)
        {
            flag = temp & 0x0001;
            temp >>=1;

            if (flag)
                temp ^= 0xA001;
        }
    }
    // Reverse byte order.
    temp2 = temp >> 8;
    temp = (temp << 8) | temp2;
    temp &= 0xFFFF;
    // the returned value is already swapped
    // crcLo byte is first & crcHi byte is last
    return temp;
}


#if 0
void append_data_time_payload(uint16_t idx, uint8_t year, uint8_t mon, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
	agms_info->date_time[idx].b.year = year;
	agms_info->date_time[idx].b.mon = mon;
	agms_info->date_time[idx].b.day = day;
	agms_info->date_time[idx].b.hour = hour;
	agms_info->date_time[idx].b.min = min;
	agms_info->date_time[idx].b.sec = sec;
}

void load_data_time_payload(uint16_t idx, uint8_t *year, uint8_t *mon, uint8_t *day, uint8_t *hour, uint8_t *min, uint8_t *sec)
{
	*year =agms_info->date_time[idx].b.year;
	*mon = agms_info->date_time[idx].b.mon;
	*day = agms_info->date_time[idx].b.day;
	*hour =agms_info->date_time[idx].b.hour;
	*min = agms_info->date_time[idx].b.min;
	*sec = agms_info->date_time[idx].b.sec;
}
#endif

void append_epoch_time(uint16_t idx, uint32_t *e)
{
	uint32_t epoch;

	epoch = get_epoch_time(&epoch_time);
	agms_info->Epoch_Time[idx] = epoch;

	*e = epoch;
}

void load_epoch_time(uint16_t idx, uint32_t *e)
{
	uint32_t epoch;

	epoch = agms_info->Epoch_Time[idx];

	*e = epoch;
}
/**********************************************************************************************************************************************************************************************
  *	CEM102 CALIBRATION ...weo1 & weo2 must 1000&1000...보정 중이라는 표시 중복을 피하기 위해 10000일 경우 10001변경 한다.
  *	calibration 중에는 10000 & 10000을 전송 한 뒤 앱에서 보정 중이라고 표시 하게 한다..기존 프로토콜을 활용...
  **********************************************************************************************************************************************************************************************/
void append_we_current(uint16_t idx, float we1, float we2)
{
	unsigned short sign;
	unsigned short exponent;
	unsigned char mantissa;

	float we_current;

	if(we1<0) {
		sign=0x8000;
		we_current = - we1;
	}
	else {
		sign=0x0000;
		we_current = we1;
	}

	exponent = (unsigned short)we_current;
	mantissa = (unsigned char)((we_current - exponent) * 100);
	exponent|= sign;

	agms_info->we1_current[idx].exponent = exponent;
	agms_info->we1_current[idx].mantissa = mantissa;


	if(we2<0) {
		sign=0x8000;
		we_current = -1 * we2;
	}
	else {
		sign=0x0000;
		we_current = we2;
	}

	exponent = (unsigned short)we_current;
	mantissa = (unsigned char)((we_current - exponent) * 100);
	exponent|= sign;

	agms_info->we2_current[idx].exponent = exponent;
	agms_info->we2_current[idx].mantissa = mantissa;


	agms_info->Temperature[idx] = rsl15_info->temperature;						// 온도 정보 저장..
}


void append_lsad_lvl_payload(void)
{
	uint32_t epoch;

#if 0
	get_current_date_time();
	append_data_time_payload(agms_info->idx_head, current_date_time->year, current_date_time->mon, current_date_time->day,
			current_date_time->hour, current_date_time->min, current_date_time->sec);
#else
	append_epoch_time(agms_info->idx_head, &epoch);
#endif

	append_we_current(agms_info->idx_head, afe_102->we1_current, afe_102->we2_current);

	agms_info->idx_head++;
	agms_info->idx_head%=LSAD_BUFFER_SIZE;																	// max buff size....jump 0 point
	if(agms_info->idx_head==agms_info->idx_tail) {																// if 1 cycle head point, tail point moving
		agms_info->idx_tail++;
		agms_info->idx_tail%=LSAD_BUFFER_SIZE;
	}

#ifdef SWMTRACE_DEBUG
	swmLogInfo("append payload : [%d] [%d] -> H:[%d] / T:[%d]\r\n", epoch, rsl15_info->temperature, agms_info->idx_head, agms_info->idx_tail);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//



/**************************************************************************************
  *   utility
  **************************************************************************************/
uint16_t g_atoi16(uint8_t  *au8_buff, int i)
{
    uint16_t retVal;

    retVal=(uint16_t)(au8_buff[i]&0xFF);
    retVal=retVal<<8;
    retVal|=(uint16_t)(au8_buff[i+1]&0xFF);

    return retVal;
}

uint32_t g_atoi32(uint8_t *au8_buff,  int i)
{
    uint32_t retVal;

    retVal=(uint32_t)(au8_buff[i]&0xFF);
    retVal=retVal<<8;
    retVal|=(uint32_t)(au8_buff[i+1]&0xFF);
    retVal=retVal<<8;
    retVal|=(uint32_t)(au8_buff[i+2]&0xFF);
    retVal=retVal<<8;
    retVal|=(uint32_t)(au8_buff[i+3]&0xFF);

    return retVal;
}

void g_itoa16(uint16_t value, uint8_t *au8_buff)
{
    au8_buff[0] = (uint8_t)((value>>8)&0xFF);
    au8_buff[1] = (uint8_t)(value&0xFF);
}

void g_itoa32(uint32_t value, uint8_t *au8_buff)
{
    au8_buff[0] = (uint8_t)((value>>24)&0xFF);
    au8_buff[1] = (uint8_t)((value>>16)&0xFF);
    au8_buff[2] = (uint8_t)((value>>8)&0xFF);
    au8_buff[3] = (uint8_t)(value&0xFF);
}


/**************************************************************************************************************
 * payload packet data send..MAX247 BLE STACK..HEAD 3byte ...247-3 = 244 max size
 * if max size is over payload, BLE send message fail
 * Header area size is 8byte...main data max size = 244-8 = 236
 * real data count is agms_info->idx_tail....data load count agms_info->bak_tail
 *  HEAD(8byte) + calendar (6byte) + Battery(2byte) + 8byte * N = 512byte...512-16 = 496 / 8 = 62...max 60개 데이터 전송 가능
 **************************************************************************************************************/
void agms_send_payload_packet(uint8_t conidx)
{
#if (UNIX_TIME_MODE == UNIX_TIME_DISABLE)
	uint8_t year, mon, day;
	uint8_t hour, min, sec;
#endif
	uint16_t temperature;
	uint16_t exponent;
	uint8_t mantissa;

	uint16_t send_idx;
	uint16_t send_packet;
	uint16_t packet_count;
	uint16_t crc16=0;

#if (UNIX_TIME_MODE == UNIX_TIME_ENABLE)
	uint32_t epoch;
#endif

	int8_t loop = 0;

	agms_info->send_pos = agms_info->idx_tail;									// 응답을 받지 못하면 다시 재 전송을 하도록 하기 위해 응답 받은 경우에만

	memset(agms_payload_buff, 0, sizeof(agms_payload_buff));
	send_idx = 0;

//	BASIC PACKET
	agms_payload_buff[send_idx++] = PROTOCOL_HEAD;
	agms_payload_buff[send_idx++] = PROTOCOL_MODE;
	agms_payload_buff[send_idx++] = TRANSFER_COMMAND;

	if (calib_state == CALIB_DONE)		{
		agms_payload_buff[send_idx++] = 0;											// status...NORMAL
	}
	else {
		agms_payload_buff[send_idx++] = 0x04;										// status...CEM 102 Calibration State
	}

	agms_payload_buff[send_idx++] = 0;												// length
	agms_payload_buff[send_idx++] = 0;												// reserve
	agms_payload_buff[send_idx++] = 0;												// crc16 H
	agms_payload_buff[send_idx++] = 0;												// crc16 L

//	PAYLOAD
#if (UNIX_TIME_MODE == UNIX_TIME_DISABLE)
	load_data_time_payload(agms_info->send_pos, &year, &mon, &day, &hour, &min, &sec);
	agms_payload_buff[send_idx++] = year;
	agms_payload_buff[send_idx++] = mon;
	agms_payload_buff[send_idx++] = day;
	agms_payload_buff[send_idx++] = hour;
	agms_payload_buff[send_idx++] = min;
	agms_payload_buff[send_idx++] = sec;
#else
	load_epoch_time(agms_info->send_pos, &epoch);

	agms_payload_buff[send_idx++] = GET_BYTE_A(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_B(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_C(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_D(epoch);
#endif

	agms_payload_buff[send_idx++] = HighByte(rsl15_info->vbat_lvl_mV);											// data length | Time | weo data | aeo data | .......
	agms_payload_buff[send_idx++] = LowByte(rsl15_info->vbat_lvl_mV);											// data length | Time | weo data | aeo data | .......

//	AGMS WE1 7 WE2 Current
#if (UNIX_TIME_MODE == UNIX_TIME_DISABLE)
	packet_count = 8;																			// date(3byte) + Time(3byte) + Battery(2byte)
#else
	packet_count = 6;
#endif

	send_packet = 0;
	loop = MAX_LOOP_COUNT;																		// just only loop time..60
	do {
		temperature = agms_info->Temperature[agms_info->send_pos];
		agms_payload_buff[send_idx++] = HighByte(temperature);
		agms_payload_buff[send_idx++] = LowByte(temperature);

		exponent = agms_info->we1_current[agms_info->send_pos].exponent;
		mantissa  = agms_info->we1_current[agms_info->send_pos].mantissa;
		agms_payload_buff[send_idx++] = HighByte(exponent);
		agms_payload_buff[send_idx++] = LowByte(exponent);
		agms_payload_buff[send_idx++] = mantissa;

		exponent = agms_info->we2_current[agms_info->send_pos].exponent;
		mantissa  = agms_info->we2_current[agms_info->send_pos].mantissa;
		agms_payload_buff[send_idx++] = HighByte(exponent);
		agms_payload_buff[send_idx++] = LowByte(exponent);
		agms_payload_buff[send_idx++] = mantissa;

		packet_count+=8;

		agms_info->send_pos++;
		agms_info->send_pos%=LSAD_BUFFER_SIZE;

		if(agms_info->send_pos==agms_info->idx_head) {												// end of buffer
			agms_info->run_mode = AGMS_INFO_SEND_STOP;											// stop
			break;
		}

		send_packet++;																											// MAX PACKET SIZE ...30(WE1 & WE2)
		if(send_packet>=LSAD_PACKET_SIZE) {																// max packet size...30
			agms_info->run_mode = AGMS_INFO_SEND_NEXT;											// rest data send next time again
			break;
		}
	} while(loop-->0);																										// max loop 60

	agms_payload_buff[AGMS_PACKET_SIZE_H_POS] = HighByte(packet_count);											// data length | Time | weo data | aeo data | .......
	agms_payload_buff[AGMS_PACKET_SIZE_L_POS] = LowByte(packet_count);											// data length | Time | weo data | aeo data | .......

	crc16 = lsad_calc_crc16(send_idx);
	agms_payload_buff[AGMS_PACKET_CRC_H_POS] = HighByte(crc16);
	agms_payload_buff[AGMS_PACKET_CRC_L_POS] = LowByte(crc16);



//----------------------------------------------------------------------------------------------------------------------------------------------------------------
//	data send..
//----------------------------------------------------------------------------------------------------------------------------------------------------------------
	/* Send notification to peer device */
    GATTC_SendEvtCmd(
        conidx,
        GATTC_NOTIFY,
        0,
        GATTM_GetHandle(CUST_SVC0, CS0_AGMS_SEND_VALUE_VAL0),
		send_idx,
		agms_payload_buff
    );


#ifdef SWMTRACE_DEBUG
    swmLogInfo("send payload : [%d] -> ", packet_count);
    swmLogInfo("%02d/%02d/%02d %02d:%02d:%02d ", agms_payload_buff[6], agms_payload_buff[7], agms_payload_buff[8],
    		agms_payload_buff[9],agms_payload_buff[10],agms_payload_buff[11]);

    swmLogInfo("weo:[%02x][%02x][%02x][%02x]  send size : [%d]\r\n",
    		agms_payload_buff[12], agms_payload_buff[13], agms_payload_buff[14], agms_payload_buff[15],send_idx);
#endif


}

/*************************************************************************************************************************
  *     EIS Info Phase & Magnitude Info
  *************************************************************************************************************************/
void agms_send_impedance_info_packet(uint8_t conidx, uint8_t command)
{
    uint8_t i;
    uint8_t au8_buff[4];

	uint16_t send_idx;
	uint16_t packet_count;
	uint16_t crc16=0;

#if (UNIX_TIME_MODE == UNIX_TIME_ENABLE)
	uint32_t epoch;
#endif
	memset(agms_payload_buff, 0, sizeof(agms_payload_buff));
	send_idx = 0;

//	BASIC PACKET
	agms_payload_buff[send_idx++] = PROTOCOL_HEAD;
	agms_payload_buff[send_idx++] = PROTOCOL_MODE;
	agms_payload_buff[send_idx++] = command;
    agms_payload_buff[send_idx++] = 0;                        // status...NORMAL

    agms_payload_buff[send_idx++] = 0;                        // length
    agms_payload_buff[send_idx++] = 0;                        // reserve
    agms_payload_buff[send_idx++] = 0;                        // crc16 H
    agms_payload_buff[send_idx++] = 0;                        // crc16 L

#if (UNIX_TIME_MODE == UNIX_TIME_DISABLE)
    agms_payload_buff[send_idx++] = impedance_calendar.year;
	agms_payload_buff[send_idx++] = impedance_calendar.mon;
	agms_payload_buff[send_idx++] = impedance_calendar.day;
	agms_payload_buff[send_idx++] = impedance_calendar.hour;
	agms_payload_buff[send_idx++] = impedance_calendar.min;
	agms_payload_buff[send_idx++] = impedance_calendar.sec;
#else
	epoch = impedance_calendar.epoch;

	agms_payload_buff[send_idx++] = GET_BYTE_A(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_B(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_C(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_D(epoch);
#endif

	agms_payload_buff[send_idx++] = HighByte(rsl15_info->vbat_lvl_mV);
	agms_payload_buff[send_idx++] = LowByte(rsl15_info->vbat_lvl_mV);

	agms_payload_buff[send_idx++] = HighByte(rsl15_info->temperature);
	agms_payload_buff[send_idx++] = LowByte(rsl15_info->temperature);

	for(i=CHRONO_FREQ_1HZ;i<CHRONO_FREQ_COUNT;i++) {
		g_itoa32((int32_t)impedance_result[i].z_real, au8_buff);
		agms_payload_buff[send_idx++] = au8_buff[0];
		agms_payload_buff[send_idx++] = au8_buff[1];
		agms_payload_buff[send_idx++] = au8_buff[2];
		agms_payload_buff[send_idx++] = au8_buff[3];

//		swmLogInfo("real : [%02x][%02x][%02x][%02x]\r\n", au8_buff[0], au8_buff[1], au8_buff[2], au8_buff[3]);

		g_itoa32((int32_t)impedance_result[i].z_imag, au8_buff);
		agms_payload_buff[send_idx++] = au8_buff[0];
		agms_payload_buff[send_idx++] = au8_buff[1];
		agms_payload_buff[send_idx++] = au8_buff[2];
		agms_payload_buff[send_idx++] = au8_buff[3];
//		swmLogInfo("img : [%02x][%02x][%02x][%02x]\r\n", au8_buff[0], au8_buff[1], au8_buff[2], au8_buff[3]);

		g_itoa32((int32_t)impedance_result[i].z_mag, au8_buff);
		agms_payload_buff[send_idx++] = au8_buff[0];
		agms_payload_buff[send_idx++] = au8_buff[1];
		agms_payload_buff[send_idx++] = au8_buff[2];
		agms_payload_buff[send_idx++] = au8_buff[3];

//		swmLogInfo("mag : [%02x][%02x][%02x][%02x]\r\n", au8_buff[0], au8_buff[1], au8_buff[2], au8_buff[3]);

		g_itoa32((int32_t)impedance_result[i].z_phase_deg, au8_buff);
		agms_payload_buff[send_idx++] = au8_buff[0];
		agms_payload_buff[send_idx++] = au8_buff[1];
		agms_payload_buff[send_idx++] = au8_buff[2];
		agms_payload_buff[send_idx++] = au8_buff[3];

//		swmLogInfo("phase : [%02x][%02x][%02x][%02x]\r\n", au8_buff[0], au8_buff[1], au8_buff[2], au8_buff[3]);
	}

#if (UNIX_TIME_MODE == UNIX_TIME_DISABLE)
	packet_count = send_idx - 8;
#else
	packet_count = send_idx - 6;
#endif
	agms_payload_buff[AGMS_PACKET_SIZE_H_POS] = HighByte(packet_count);											// data length | Time | weo data | aeo data | .......
	agms_payload_buff[AGMS_PACKET_SIZE_L_POS] = LowByte(packet_count);											// data length | Time | weo data | aeo data | .......

	crc16 = lsad_calc_crc16(send_idx);
	agms_payload_buff[AGMS_PACKET_CRC_H_POS] = HighByte(crc16);
	agms_payload_buff[AGMS_PACKET_CRC_L_POS] = LowByte(crc16);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
//	data send.. Send notification to peer device
//----------------------------------------------------------------------------------------------------------------------------------------------------------------
	GATTC_SendEvtCmd(conidx,
			GATTC_NOTIFY,
			0,
			GATTM_GetHandle(CUST_SVC0, CS0_AGMS_SEND_VALUE_VAL0),
			send_idx,
			agms_payload_buff);
}



//==================================================================================================================================
//	payload packet data send
//==================================================================================================================================
void agms_request_current_data_time(uint8_t conidx, uint8_t status)
{
	uint16_t send_idx;
	uint16_t crc16=0;
	uint16_t packet_count;

	memset(agms_payload_buff, 0, sizeof(agms_payload_buff));
	send_idx = 0;
	packet_count = 4;

	//	BASIC PACKET
		agms_payload_buff[send_idx++] = PROTOCOL_HEAD;
		agms_payload_buff[send_idx++] = PROTOCOL_MODE;
		agms_payload_buff[send_idx++] = REQUEST_RTC_COMMAND;
		agms_payload_buff[send_idx++] = status;									// status...NORMAL

		agms_payload_buff[send_idx++] = HighByte(packet_count);						// length
		agms_payload_buff[send_idx++] = LowByte(packet_count);												// reserve
		agms_payload_buff[send_idx++] = 0;												// crc16 H
		agms_payload_buff[send_idx++] = 0;												// crc16 L

	//	PAYLOAD

		agms_payload_buff[send_idx++] = FIRMWARE_MAIN;
		agms_payload_buff[send_idx++] = FIRMWARE_SUB;

		agms_payload_buff[send_idx++] = CUSTOMSS_NOTIFY_ON_TIME;
		agms_payload_buff[send_idx++] = 0;												// reserve

		crc16 = lsad_calc_crc16(send_idx);
		agms_payload_buff[AGMS_PACKET_CRC_H_POS] = HighByte(crc16);
		agms_payload_buff[AGMS_PACKET_CRC_L_POS] = LowByte(crc16);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
//	data send..
//----------------------------------------------------------------------------------------------------------------------------------------------------------------
		/* Send notification to peer device */
		GATTC_SendEvtCmd(
			conidx,
			GATTC_NOTIFY,
			0,
			GATTM_GetHandle(CUST_SVC0, CS0_AGMS_SEND_VALUE_VAL0),
			send_idx,
			agms_payload_buff
		);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	data receive event handler
//
void progress_receive_msg_event(uint8_t conidx, const uint8_t *dest_buff)
{
	uint8_t cmd_code;
	uint8_t cmd_state;
	uint32_t epoch;
	
    if((dest_buff[0] != PROTOCOL_HEAD) || (dest_buff[1] != PROTOCOL_MODE) || (dest_buff[3] != RECEIVE_NO_ERROR))
		return;

	cmd_code = dest_buff[2];
	cmd_state= dest_buff[3];
	
    switch(cmd_code) {
		case REQUEST_RTC_COMMAND:
			if(rsl15_info->date_time_update != DATE_TIME_UPDATE_SUCCESS) {		// 현재 시간을 업데이트 한다..
				rsl15_info->date_time_update = DATE_TIME_UPDATE_SUCCESS;

#if (UNIX_TIME_MODE == UNIX_TIME_DISABLE)
				set_current_date_time(dest_buff[8],dest_buff[9],dest_buff[10], dest_buff[11],dest_buff[12],dest_buff[13]);
#else
				epoch = makeDWord(dest_buff[8], dest_buff[9], dest_buff[10], dest_buff[11]);
				set_epoch_time(&epoch_time, epoch);
#endif

#ifdef SWMTRACE_DEBUG
				swmLogInfo("REQUEST_RTC_COMMAND Receive OK. \r\n");
#endif
			}
			break;
	
		case TRANSFER_COMMAND:
			agms_info->idx_tail = agms_info->send_pos;										// receive is ok...tail position move

			if(agms_info->run_mode == AGMS_INFO_SEND_NEXT) {				// IF NEXT DATA...
				 ke_timer_set(APP_CUSS_AGMS_NEXT_SEND_DATA, KE_BUILD_ID(TASK_APP, conidx),TIMER_SETTING_S(1));	// after 1sec, data send
			}

#ifdef SWMTRACE_DEBUG
			 swmLogInfo("TRANSFER_COMMAND...No Error \r\n");
#endif
			break;
	

		case EIS_INFO_COMMAND:													// IMPEDANCE 데이터에 대한 응답이 들어 오면...
			impedance_calendar.update = 0;

#ifdef SWMTRACE_DEBUG
			 swmLogInfo("IMPEDANCE_COMMAND...No Error \r\n");
#endif
			break;

		case CONTROL_COMMAND:													// CONTROL COMMAND에 대한 응답
			break;

		case CALB_TEMP_COMMAND:
			if(cmd_state == CALB_TEMP_CLEAR) {				// TEMP CALIBRATION CLEAR
				Temp_calibration.cal_step = TEMP_CALIB_WAIT_STEP;
				Temp_calibration.wait_count = 0;
				Temp_calibration.cumulative_count = 0;
				Temp_calibration.absolute_offset = 0;
				Temp_calibration.current_ave = 0;

				Write_Temperature_Offset(Temp_calibration.current_ave, TEMP_CALIB_CLEAR);
			}

			else if(cmd_state == CALB_TEMP_SETUP) {			// TEMP CALIBRATION SETUP...ABSOLUTE

			}
			break;


		default:
			break;
    }
}

