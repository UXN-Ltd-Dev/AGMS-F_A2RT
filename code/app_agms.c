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

/* ----------------------------------------------------------------------------
 * Private Symbolic Constants
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Private Macros
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Private Type Definitions
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Global Variables
 * ------------------------------------------------------------------------- */
agms_buff_t		agms_info;
agms_batch_t	agms_batch = {
	.batch_n    = 0,
	.batch_size = AB_BATCH_DEFAULT,
};

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
uint16_t lsad_calc_crc16(uint8_t *payload_buff, uint16_t length)
{
    uint16_t i, j;
    unsigned int temp, temp2, flag;

    temp = 0xFFFF;

    for (i = 0; i < length; i++)    {
        temp = temp ^ payload_buff[i];

        for (j = 1; j <= 8; j++)  {
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

int8_t agms_buff_is_empty(const agms_buff_t *ab) {
	return (agms_buff_count(ab) == 0u);
}

int8_t agms_buff_is_full (const agms_buff_t *ab) {
	return (agms_buff_count(ab) >= AB_CAPACITY);
}

static inline uint16_t agms_buff_next(uint16_t idx)
{
    return (uint16_t)((idx + 1u) % AB_CAPACITY);
}

void agms_buff_init(agms_buff_t *a)
{
    if (a == NULL) return;
    memset(a, 0, sizeof(*a));
}

void agms_buff_reset(agms_buff_t *a)
{
    if (a == NULL) return;
    a->head = a->tail = a->count = a->pending = 0u;
}

int8_t agms_buff_push_item(agms_buff_t *a, const agms_item_t *item)
{
	int8_t ok = true;

    if (a == NULL || item == NULL) return false;

    if (a->count >= AB_CAPACITY) {
        /* 가득 참 -> 가장 오래된 1개 폐기 */
        a->tail = agms_buff_next(a->tail);
        a->count--;
        /* 폐기된 것이 전송 대기(in-flight) 중이었다면 pending도 보정 */
        if (a->pending > 0u) {
        	a->pending--;
        }
        ok = false;
    }

    a->buf[a->head] = *item;
    a->head = agms_buff_next(a->head);
    a->count++;

    return ok;
}

int8_t agms_buff_push(agms_buff_t *a, float we1, float we2, int16_t celsius, uint32_t epoch_time)
{
	agms_item_t it;

    it.we1  = we1;
    it.we2  = we2;
    it.celsius = celsius;
    it.epoch_time = epoch_time;

    return agms_buff_push_item(a, &it);
}

int8_t agms_buff_pop(agms_buff_t *a, agms_item_t *out)
{
	int8_t ok = false;

    if (a == NULL || out == NULL) return false;

    if (a->count > 0u) {
        *out = a->buf[a->tail];
        a->tail = agms_buff_next(a->tail);
        a->count--;
        if (a->pending > 0u) a->pending--;
        ok = true;
    }

    return ok;
}

uint16_t agms_buff_count(const agms_buff_t *a)
{
    uint16_t n;
    if (a == NULL) return 0u;
    n = a->count;
    return n;
}

uint16_t agms_buff_batch_clamp(uint16_t n)
{
	if (n < AB_BATCH_MIN) return AB_BATCH_MIN;
	if (n > AB_BATCH_MAX) return AB_BATCH_MAX;

	return n;
}

uint16_t agms_buff_peek(agms_buff_t *a, agms_item_t *out, uint16_t max_n)
{
    uint16_t i, idx, n;

    if (a == NULL || out == NULL || max_n == 0u) return 0u;

    max_n = agms_buff_batch_clamp(max_n);   /* 1 ~ 60 로 제한 */

    n   = (a->count < max_n) ? a->count : max_n;
    idx = a->tail;
    for (i = 0u; i < n; i++) {
        out[i] = a->buf[idx];
        idx = agms_buff_next(idx);
    }
    a->pending = n;   /* ACK 대기 개수 기록 */

    return n;
}

uint16_t agms_buff_commit(agms_buff_t *a, uint16_t n)
{
    uint16_t i, del;

    if (a == NULL || n == 0u) return 0u;

    del = (a->count < n) ? a->count : n;
    for (i = 0u; i < del; i++) {
        a->tail = agms_buff_next(a->tail);
    }
    a->count -= del;
    a->pending = 0u;

    return del;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	we1 & we2 & temperature & epoch 저장
//

/************************************************************************************************************
 *	WE1 & WE2 & TEMP & UNIX TIME 을 추가 한다...
 ************************************************************************************************************/
void append_Potentiostat_data(void)
{
	uint32_t epoch;

	epoch = get_epoch_time(&calendar);
	agms_buff_push(&agms_info, device_info.we1_current, device_info.we2_current, device_info.celsius, epoch);

	epoch_time_to_calendar(&calendar, epoch);

#ifdef SWMTRACE_DEBUG
	swmTrace_printf("Potentiostat count : ("LOG_FLOAT_MARKER")nA, ("LOG_FLOAT_MARKER")nA [%d][%d]/[%d] ==>%d-%d-%d %d:%d:%d   \r\n",
			LOG_FLOAT(device_info.we1_current), LOG_FLOAT(device_info.we2_current), device_info.celsius, epoch, agms_info.count,
			calendar.year, calendar.mon, calendar.day, calendar.hour, calendar.min, calendar.sec);
#endif

}


static void load_exponent_mantissa(float we, uint16_t *exponent, uint8_t *mantissa)
{
	unsigned short sign = 0;
	float we_current;

	we_current = we;
	if(we<0) {
		sign=0x8000;
		we_current = -we;
	}

	*exponent = (uint16_t)(we_current);
	*mantissa = (uint8_t)((we_current - *exponent) * 100);
	*exponent|= sign;
}



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
uint16_t agms_send_payload_packet(void)
{
	uint8_t i;

	uint32_t epoch;
	uint16_t celsius;
	uint16_t exponent;
	uint8_t mantissa;

	uint16_t send_idx;
	uint16_t packet_count;
	uint16_t crc16=0;

	agms_batch.batch_size = agms_buff_batch_clamp(AB_BATCH_DEFAULT);
	if (agms_buff_count(&agms_info) == 0u)				// 저장된 데이터가 없다면...
		return 0;

	agms_batch.batch_n = agms_buff_peek(&agms_info, agms_batch.batch, agms_batch.batch_size);

#ifdef SWMTRACE_DEBUG
    swmLogInfo("agms_buff_peek : [%d] / [%d]\r\n", agms_batch.batch_n, agms_batch.batch_size);
#endif

//-------------------------------------------------------------------------------------------------------------------------------
	memset(agms_payload_buff, 0, sizeof(agms_payload_buff));
	send_idx = 0;

//	BASIC PACKET
	agms_payload_buff[send_idx++] = PROTOCOL_HEAD;
	agms_payload_buff[send_idx++] = PROTOCOL_MODE;
	agms_payload_buff[send_idx++] = TRANSFER_COMMAND;
	agms_payload_buff[send_idx++] = agms_info.run_state;

	agms_payload_buff[send_idx++] = 0;											// length
	agms_payload_buff[send_idx++] = 0;											// reserve
	agms_payload_buff[send_idx++] = 0;											// crc16 H
	agms_payload_buff[send_idx++] = 0;											// crc16 L

//	PAYLOAD
	epoch = agms_batch.batch[0].epoch_time;
	agms_payload_buff[send_idx++] = GET_BYTE_A(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_B(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_C(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_D(epoch);

	agms_payload_buff[send_idx++] = HighByte(device_info.vbat_lvl);			// data length | Time | weo data | aeo data | .......
	agms_payload_buff[send_idx++] = LowByte(device_info.vbat_lvl);			// data length | Time | weo data | aeo data | .......

//	AGMS WE1 7 WE2 Current
	packet_count = 6;															// epoch time(4byte) + vbat_lvl_mV(2byte)

	for(i=0;i<agms_batch.batch_n;i++) {
		celsius = agms_batch.batch[i].celsius;
		agms_payload_buff[send_idx++] = HighByte(celsius);
		agms_payload_buff[send_idx++] = LowByte(celsius);

		load_exponent_mantissa(agms_batch.batch[i].we1, &exponent, &mantissa);
		agms_payload_buff[send_idx++] = HighByte(exponent);
		agms_payload_buff[send_idx++] = LowByte(exponent);
		agms_payload_buff[send_idx++] = mantissa;

		load_exponent_mantissa(agms_batch.batch[i].we2, &exponent, &mantissa);
		agms_payload_buff[send_idx++] = HighByte(exponent);
		agms_payload_buff[send_idx++] = LowByte(exponent);
		agms_payload_buff[send_idx++] = mantissa;

		packet_count+=8;
	}

	agms_payload_buff[AGMS_PACKET_SIZE_H_POS] = HighByte(packet_count);											// data length | Time | weo data | aeo data | .......
	agms_payload_buff[AGMS_PACKET_SIZE_L_POS] = LowByte(packet_count);											// data length | Time | weo data | aeo data | .......

	crc16 = lsad_calc_crc16(agms_payload_buff, send_idx);
	agms_payload_buff[AGMS_PACKET_CRC_H_POS] = HighByte(crc16);
	agms_payload_buff[AGMS_PACKET_CRC_L_POS] = LowByte(crc16);

#ifdef SWMTRACE_DEBUG
    swmLogInfo("send payload : [%d] [%d]\r\n", packet_count, send_idx);
#endif

	return send_idx;

}


/*************************************************************************************************************************
  *     EIS Info Phase & Magnitude Info
  *************************************************************************************************************************/
uint16_t agms_send_impedance_info_packet(uint8_t command)
{
    uint8_t i;
    uint8_t au8_buff[4];

	uint16_t send_idx;
	uint16_t packet_count;
	uint16_t crc16=0;

	uint32_t epoch;

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

	epoch = impedance_calendar.epoch;

	agms_payload_buff[send_idx++] = GET_BYTE_A(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_B(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_C(epoch);
	agms_payload_buff[send_idx++] = GET_BYTE_D(epoch);

	agms_payload_buff[send_idx++] = HighByte(device_info.vbat_lvl);			// data length | Time | weo data | aeo data | .......
	agms_payload_buff[send_idx++] = LowByte(device_info.vbat_lvl);			// data length | Time | weo data | aeo data | .......


	agms_payload_buff[send_idx++] = HighByte(device_info.celsius);
	agms_payload_buff[send_idx++] = LowByte(device_info.celsius);

	for(i=0;i<2;i++) {
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
	packet_count = send_idx - 6;

	agms_payload_buff[AGMS_PACKET_SIZE_H_POS] = HighByte(packet_count);											// data length | Time | weo data | aeo data | .......
	agms_payload_buff[AGMS_PACKET_SIZE_L_POS] = LowByte(packet_count);											// data length | Time | weo data | aeo data | .......

	crc16 = lsad_calc_crc16(agms_payload_buff, send_idx);
	agms_payload_buff[AGMS_PACKET_CRC_H_POS] = HighByte(crc16);
	agms_payload_buff[AGMS_PACKET_CRC_L_POS] = LowByte(crc16);

	return send_idx;
}

/**************************************************************************************************
 *	UNIX TIME REQUEST
 **************************************************************************************************/
uint16_t agms_read_current_calendar(uint8_t status)
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

	crc16 = lsad_calc_crc16(agms_payload_buff, send_idx);
	agms_payload_buff[AGMS_PACKET_CRC_H_POS] = HighByte(crc16);
	agms_payload_buff[AGMS_PACKET_CRC_L_POS] = LowByte(crc16);

	return send_idx;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	data receive event handler
//
void agms_callback_msg_event(uint8_t conidx, const uint8_t *rb_buff)
{
	uint8_t cmd_code;
	uint8_t cmd_state;
	uint16_t remain;
	uint32_t epoch;

    if((rb_buff[0] != PROTOCOL_HEAD) || (rb_buff[1] != PROTOCOL_MODE))
		return;

	cmd_code = rb_buff[2];
	cmd_state= rb_buff[3];

    switch(cmd_code) {
		case REQUEST_RTC_COMMAND:
			if(cmd_state != RECEIVE_NO_ERROR)
				return;

            if((calendar.update != CALENDAR_UPDATE_SUCCESS) || (calendar.resync != CALENDAR_UPDATE_SUCCESS)) {
                calendar.update = calendar.resync = CALENDAR_UPDATE_SUCCESS;

                if(device_info.authentication != AUTHENTICATION_SUCCESS) {
					epoch = makeDWord(rb_buff[8], rb_buff[9], rb_buff[10], rb_buff[11]);
					set_epoch_time(&calendar, epoch);
                }

                device_info.authentication = AUTHENTICATION_SUCCESS;									// 인증 성공...
				device_info.auth_count = 0;
				disable_Authentication_Task();


#ifdef SWMTRACE_DEBUG
				swmLogInfo("REQUEST_RTC_COMMAND Receive OK. : [%d][%d] \r\n", calendar.base_epoch, epoch);
#endif
            }
			break;

		case TRANSFER_COMMAND:
			if(cmd_state != RECEIVE_NO_ERROR)
				return;

			if(agms_info.pending == 0u) return;

			if(device_info.agms_mode == AGMS_MODE_CHRONO_A) {				// CHRONO A 일 경우 현재 상태만 정리 한다.
				 device_info.agms_mode = AGMS_MODE_PSTAT;
			}
			else {
				agms_buff_commit(&agms_info, agms_info.pending);
				agms_batch.batch_n = 0;

				remain = agms_buff_count(&agms_info);      					// 남은 데이터 개수
				agms_batch.batch_ramain = remain;

				if (remain > 0u) {
					app_env_cs0.cem102_agms_to_air_cccd_value[conidx] = ATT_CCC_START_NTF;
					cs0_attr_to_send = APP_ATTR_SEND_AGMS_CONTINUEOUS;
					CUSTOMSS_AGMS_Start_Timer(conidx, TIMER_SETTING_S(1));
				}
			}

#ifdef SWMTRACE_DEBUG
			 swmLogInfo("TRANSFER_COMMAND...No Error : [%d] \r\n", agms_batch.batch_ramain);
#endif
			break;

		case CALB_TEMP_COMMAND:
			if(cmd_state == CALB_TEMP_CLEAR) {				// TEMP CALIBRATION CLEAR
				lsad_temp.calib_status = TEMP_CALIB_CLEAR;
				lsad_temp.calib_step = TEMP_CALIB_WAIT_STEP;
				lsad_temp.calib_wait_time = 0;
				lsad_temp.calib_average = 0;

        		write_nvr4_lsad_temp_info((uint32_t)lsad_temp.calib_average, (uint32_t)lsad_temp.calib_status);
			}

			break;

		default:
			break;
    }
}



////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
//	RING BUFF 2026.07.28
//	고정 크기 링 버퍼(circular buffer)
//
/**********************************************************************
 * 버퍼 초기화(비우기)
 **********************************************************************/
void rb_init(ring_buff_t *rb)
{
	rb->head  = 0;
	rb->count = 0;
}

/**********************************************************************
 * 현재 실제 데이터 개수 조회
 **********************************************************************/
int16_t rb_count(const ring_buff_t *rb)
{
    return rb->count;
}

/**********************************************************************
 * 생산자(ISR 등)에서 호출: 새 데이터 추가.
 * 가득 차면 가장 오래된 데이터를 버린다(인덱스만 전진, 메모리 이동 없음).
 **********************************************************************/
void rb_push(ring_buff_t *rb, float fwe1, float fwe2)
{
    int tail;

    tail = (rb->head + rb->count) % CAP;
    rb->we1_buff[tail] = fwe1;
    rb->we2_buff[tail] = fwe2;

    if (rb->count < CAP)
        rb->count++;                       // 여유 있음
    else
        rb->head = (rb->head + 1) % CAP;   // 가득 참 -> 오래된 값 폐기

#if 0	//#ifdef SWMTRACE_OUTPUT
	swmLogInfo("count : %d --> ", rb->count);
    for(int i=0;i<rb->count;i++) {
    	swmLogInfo("WEO1 : ("LOG_FLOAT_MARKER")nA, WEO2 : ("LOG_FLOAT_MARKER")nA\r\n", LOG_FLOAT(rb->we1_buff[i]), LOG_FLOAT(rb->we2_buff[i]));
    	Sys_Delay(APP_SYS_DELAY_1MS);
    }
	swmLogInfo("\r\n");
#endif

}

/**********************************************************************
 * 최대·최소를 하나씩 제외한 나머지로 평균을 내고 버퍼를 초기화한다.
 * out_avg : 계산된 평균 저장.
 * 반환값  : 평균에 실제 사용한 개수(= n-2). 데이터 부족 시 0.
 * 개수 읽기 -> 합/최소/최대 계산 -> 초기화를 한 임계구역으로 묶어
 **********************************************************************/
int rb_flush_trimmed_average(ring_buff_t *rb, float *out_avg1, float *out_avg2)
{
    int  n, used;
    float s1, s2;
    float v1, v2;
    float vmin1 = 0, vmax1 = 0;
    float vmin2 = 0, vmax2 = 0;

    n = rb->count;

    switch(n) {
    	case 0:
        	*out_avg1 = 0;
        	*out_avg2 = 0;
        	used = 0;
    		break;

    	case 1:
        	*out_avg1 = rb->we1_buff[rb->head];
        	*out_avg2 = rb->we2_buff[rb->head];
        	used = 1;
    		break;

    	case 2:
        	*out_avg1 = (rb->we1_buff[rb->head] + rb->we1_buff[rb->head + 1])/2;
        	*out_avg2 = (rb->we2_buff[rb->head] + rb->we2_buff[rb->head + 1])/2;
        	used = 2;
    		break;

    	default:
            vmin1 = vmax1 = rb->we1_buff[rb->head];
            vmin2 = vmax2 = rb->we2_buff[rb->head];

            s1 = s2 = 0;
            for (int i = 0; i < n; i++) {
                v1 = rb->we1_buff[(rb->head + i) % CAP];
                v2 = rb->we2_buff[(rb->head + i) % CAP];

                s1 += v1;
                s2 += v2;

                if (v1 < vmin1) vmin1 = v1;
                if (v1 > vmax1) vmax1 = v1;
                if (v2 < vmin2) vmin2 = v2;
                if (v2 > vmax2) vmax2 = v2;
            }

            s1 -= vmin1;                   // 최대·최소 각 1개 제거
            s1 -= vmax1;

            s2 -= vmin2;                   // 최대·최소 각 1개 제거
            s2 -= vmax2;

            used = n - 2;
            *out_avg1 = s1 / (float)used;
            *out_avg2 = s2 / (float)used;

    		break;
    }


#ifdef SWMTRACE_DEBUG
	swmLogInfo("rb_flush count : %d, ("LOG_FLOAT_MARKER"), ("LOG_FLOAT_MARKER") \r\n", rb->count, LOG_FLOAT(*out_avg1), LOG_FLOAT(*out_avg2));
    Sys_Delay(APP_SYS_DELAY_1MS);
#endif

    rb->head  = 0;
    rb->count = 0;                 			// 초기화까지 원자적으로

    return used;
}


