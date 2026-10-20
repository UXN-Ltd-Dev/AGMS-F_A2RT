/**
 * @file    app_globals.h
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

#ifndef APP_GLOBALS_H
#define APP_GLOBALS_H

#define	_CUSTOM_DEBUG_ 				0
#define	_EIS_FIFO_INFO_DEBUG_ 		1
#define _IMPEDANCE_INFO_DEBUG_		0
#define _IMPEDANCE_RESULT_DEBUG_	0


#define UNIX_TIME_DISABLE	0
#define UNIX_TIME_ENABLE	1

#define UNIX_TIME_MODE 	UNIX_TIME_ENABLE

#define LOG_FLOAT_MARKER  "%c%ld.%04ld"
#define LOG_FLOAT_MARKER1 "%c%ld.%02ld"


#define LOG_FLOAT(val) (val > 0 || val <=-1) ?' ':'-',        \
                      (int32_t)(val),                                                         \
                       (int32_t)(((val > 0) ? (val) - (int32_t)(val)       \
                       : (int32_t)(val) - (val))*10000)


/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/
#define LowByte(w) ((uint8_t) ((w) & 0xff))
#define HighByte(w) ((uint8_t) ((w) >> 8))
#define makeWord(a, b)   ((uint16_t)(((uint8_t)(a)) | ((uint16_t)((uint8_t)(b))) << 8))
#define makeDWord(a, b, c, d) ((uint32_t)(((uint8_t)(a)) |               \
    						 (((uint32_t)((uint8_t)(b))) << 8) |         \
							 (((uint32_t)((uint8_t)(c))) << 16) |        \
							 (((uint32_t)((uint8_t)(d))) << 24)))

// 32비트 데이터에서 각 바이트를 추출하는 매크로
#define GET_BYTE_A(data) ((uint8_t)( (data)        & 0xFF)) /* LSB (가장 하위 바이트) */
#define GET_BYTE_B(data) ((uint8_t)(((data) >> 8)  & 0xFF))
#define GET_BYTE_C(data) ((uint8_t)(((data) >> 16) & 0xFF))
#define GET_BYTE_D(data) ((uint8_t)(((data) >> 24) & 0xFF)) /* MSB (가장 상위 바이트) */



/* ----------------------------------------------------------------------------
 * user defines
 * --------------------------------------------------------------------------*/
#define BLE_PARAM_UPDATE_TIME	 	2
#define CUSTOMSS_NOTIFY_ON_TIME		10

#define FIRMWARE_MAIN				10
#define FIRMWARE_SUB			    0


#define FIRMWARE_VERSION			250
#define LOW_BATTERY_LEVEL			1460

#define LSAD_APPEND_READY			0
#define LSAD_APPEND_SUCCESS			1

#define GPIO_LOW	0
#define GPIO_HIGH	1

#define CGMS_DISABLE			0
#define CGMS_ENABLE				1

#define CGMS_REF_MODE    CGMS_DISABLE
#define CGMS_AEO_MODE    CGMS_DISABLE

#define CGMS_WEP1_DEFAULT		10000
#define CGMS_WEP2_DEFAULT	 	 9500

#define CGMS_AEO_DEFAULT		65500
#define CGMS_AEP_DEFAULT		65500


#define ADV_SCAN_STOP	0
#define ADV_SCAN_RUN	1

#define AMR_WAKEUP_FAIL			0
#define AMR_WAKEUP_SUCCESS	1


#define NOT_WAKEUP_FROM_SLLEP_MODE	0x55				//																			Not wakeup from SLEEP mode
#define AMR_WAKEUP_FROM_SLLEP_MODE	0x77				//																			AMR wakeup from SLEEP mode

#define AMR_WAKEUP_SLEEP_MODE_FAIL			0
#define AMR_WAKEUP_SLEEP_MODE_SUCCESS	1


#if 0
#define LSAD_PACKET_SIZE	30
#define LSAD_BUFFER_SIZE 	600
#else
#define LSAD_PACKET_SIZE	 60						// 60 * 8 = 480 + 8 + 8 = 496 max size
#define LSAD_BUFFER_SIZE 	720						// 1min is 6count...1hour 360count * 2 = 720...2hour
#endif

#define LSAD_SEND_SIZE 		512
#define LSAD_READ_SIZE 		32

#define AGMS_INFO_SEND_STOP		0
#define AGMS_INFO_SEND_NEXT		1

#define PROTOCOL_HEAD						0xA0
#define PROTOCOL_MODE						0x81
#define PROTOCOL_COMPANY_ID1		0x62
#define PROTOCOL_COMPANY_ID2		0x03

#define AGMS_PACKET_SIZE_POS		4

#define AGMS_PACKET_SIZE_H_POS		4
#define AGMS_PACKET_SIZE_L_POS		5


#define AGMS_PACKET_CRC_H_POS		6
#define AGMS_PACKET_CRC_L_POS		7

#define LSAD_PACKET_SIZE_POS		5



#define RECEIVE_LSAD_STATE_ID	1
#define RECEIVE_REQUEST_RTC		2

#define RECEIVE_FOTA_READY			0x10
#define RECEIVE_ANALOG_POWER    	0x11
#define RECEIVE_NOTIFY_TIME     	0x12
#define RECEIVE_REQUEST_INFORM  	0x13

#define RECEIVE_POWER_OFF		0x20
#define RECEIVE_CALI_RESET		0x30

#define NO_ERROR					0

#define AGMS_POWER_ON				0
#define AGMS_POWER_OFF				1

#define API_FALSE					0
#define API_TRUE					1

#define DISABLE						0
#define ENABLE						1

#define UPDATE_WAIT					0
#define UPDATE_TRUE					1

#define STRIPPING_NONE				0
#define STRIPPING_RUN				1
#define STRIPPING_STOP				2

#define ADV_STATE_NONE				0
#define ADV_STATE_STOP				1
#define ADV_STATE_START				2


/* ----------------------------------------------------------------------------
 * defines
 * --------------------------------------------------------------------------*/

#define DATE_TIME_UPDATE_NONE		0
#define DATE_TIME_UPDATE_FAIL		1
#define DATE_TIME_UPDATE_SUCCESS	2

#define CALENDAR_UPDATE_NONE		0
#define CALENDAR_UPDATE_FAIL		1
#define CALENDAR_UPDATE_SUCCESS		2

#define REQUEST_RTC_COMMAND			0x41
#define TRANSFER_COMMAND			0x42
#define CONTROL_COMMAND				0x43
#define CALB_TEMP_COMMAND			0x44
#define EIS_INFO_COMMAND       		0x45
#define EIS_WAIT_COMMAND       		0x46
#define PULSE_START_COMMAND    		0x50

#define CHRONO_A_INFO_COMMAND  		0x45


#define READ_UNIX_TIME_COMMAND		0x11

#define REQUEST_RTC_FIRST_STATE		0x11
#define REQUEST_RTC_ERROR_STATE		0x12
#define REPLY_BASIC_INFORM			0x22
#define REPLY_RE_CONNECTION			0x23

#define REQUEST_BASIC_INFO_STATE	0x22
#define TRANSMITTER_POWER_OFF		0x24
#define TRANSMITTER_DFU_FOTA		0x25

#define CALB_TEMP_CLEAR				0x01
#define CALB_TEMP_SETUP				0x02



#define REPLY_YEAR					8
#define REPLY_MON						9
#define REPLY_DAY						10
#define REPLY_HOUR					11
#define REPLY_MIN						12
#define REPLY_SEC						13

enum {
	PROTOCOL_SEND_LSAD_ID=1,
	PROTOCOL_REQUEST_RTC,
	PROTOCOL_REQUEST_REBOOT
};

enum {
	RECEIVE_NO_ERROR=0,
	RECEIVE_CRC_ERROR,
	RECEIVE_DATA_LENGTH_ERROR,
	RECEIVE_RTC_TIME_ERROR,
	RECEIVE_DUMMY_ERROR,
	RECEIVE_UNKNOW_CMD_ERROR					// 5
};

enum {
	REQUEST_COMMAND_NONE_=0,
	REQUEST_DATE_TIME,
	REQUEST_NEXT_LSAD_DATA,
	RESPONSE_BASIC_INFORM,
	RESPONSE_NOTIFICATION,
	RESPONSE_POWER_OFF
};

enum {
	BASE_INFO_NONE=0,
	BASE_INFO_INIT,
	BASE_INFO_RE_INIT,
	BASE_INFO_RESPONSE,
	BASE_INFO_RECONNECT
};


typedef struct {
	uint8_t year;
	uint8_t mon;
	uint8_t day;

	uint8_t hour;
	uint8_t min;
	uint8_t sec;

	uint8_t update;
	uint8_t resync;

    uint32_t base_epoch;
    uint32_t base_second;

} calendar_t;
extern calendar_t	calendar;

#define AGMS_MODE_PSTAT			0
#define AGMS_MODE_CHRONO_A		1

#define AUTHENTICATION_FAIL			0
#define AUTHENTICATION_SUCCESS		1

#define CEM102_DAC_POWER_OFF	0
#define CEM102_DAC_POWER_ON		1
typedef struct {
	uint8_t authentication;
	uint8_t auth_count;
	uint8_t cem102_dac_power;
	uint8_t agms_mode;

	int16_t vbat_lvl;
	int16_t celsius;

	float we1_current;
	float we2_current;

} device_info_t;

extern device_info_t	device_info;



#define TEMP_CALIB_CLEAR			0
#define TEMP_CALIB_ALLOW			'C'

#define TEMP_CALIB_WAIT_STEP		0
#define TEMP_CALIB_RUN_STEP			1
#define TEMP_CALIB_IDLE_STEP		2

#define TEMP_CALI_DISABLE			0
#define TEMP_CALI_ENABLE			1
#define TEMP_CALI_MODE				TEMP_CALI_ENABLE

typedef struct {
	int8_t calib_status;							// 저장된 상태를 확인 한다.
	int8_t calib_step;

	int16_t calib_wait_time;
	int16_t calib_run_count;
	int16_t calib_average;

	int32_t calib_sum;
} lsad_temp_t;

extern lsad_temp_t	lsad_temp;


//--------------------------------------------------------------------------------
#define AGMS_NORMAL_STATE		0
#define AGMS_POWER_STABILIZE	1
#define AGMS_SENSOR_STABILIZE	2
#define AGMS_SENSOR_RECOVERY	3
#define AGMS_SENSOR_CALIBRATION	5

typedef struct {
	uint32_t sec:6;
	uint32_t min:6;
	uint32_t hour:5;
	uint32_t day:5;
	uint32_t mon:4;
	uint32_t year:5;
} tyFLAGBITS;

typedef union {
	uint32_t bAll;
	tyFLAGBITS b;
} tyFLAG;

/**************************************************************************************
 *	STEP UP
 **************************************************************************************/
#define STEP_END_BUFFER   128	// total 120
#define STEP_PER_BUFFER   132   /* 30초 @ 1Hz */

typedef struct {
    float    we1;
    float    we2;

} sample_t;

typedef struct {
    sample_t data[STEP_PER_BUFFER];

    uint8_t ovf_state;  					// STEP DATA 전송 이후 응답이 오지 않을 경우
    uint8_t ovf_tmout;  					// 30초 동안 응답이 없으면...

    uint16_t count;  						// 저장되는 갯수
    uint16_t Qpos;   						// 데이터 보내는 위치

    uint16_t send_pos;
    uint16_t dummy1;


    uint32_t epoch;							// 처음 저장 하는 시간만 저장
} step_buffer_t;

extern step_buffer_t *step_store_buf;   /* 지금 데이터를 쌓는 버퍼 */


#define CAP 10   				// 버퍼 용량. 한 주기 최대 유입(9개)보다 크게 둔다.
typedef struct {
    float	we1_buff[CAP+4];
    float	we2_buff[CAP+4];
    volatile int16_t head;    	// 가장 오래된 데이터 위치
    volatile int16_t count;   	// 현재 저장된 실제 데이터 개수
} ring_buff_t;

extern ring_buff_t	ring_buff;



extern uint8_t agms_payload_buff[LSAD_SEND_SIZE];



/**
 * @brief       Structure Memory Location
 * @details
 */

extern void app_device_malloc_attach(void);

#endif /* APP_GLOBALS_H */
