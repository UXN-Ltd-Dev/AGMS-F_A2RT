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
#define	_EIS_FIFO_INFO_DEBUG_ 		0
#define _IMPEDANCE_INFO_DEBUG_		0
#define _IMPEDANCE_RESULT_DEBUG_	1


#define UNIX_TIME_DISABLE	0
#define UNIX_TIME_ENABLE	1

#define UNIX_TIME_MODE 	UNIX_TIME_ENABLE

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

#define FIRMWARE_MAIN			2
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

#define RECEIVE_FOTA_READY				0x10
#define RECEIVE_ANALOG_POWER    	0x11
#define RECEIVE_NOTIFY_TIME     			0x12
#define RECEIVE_REQUEST_INFORM  	0x13

#define RECEIVE_POWER_OFF		0x20
#define RECEIVE_CALI_RESET		0x30

#define NO_ERROR				0

#define AGMS_POWER_ON				0
#define AGMS_POWER_OFF			1

#define API_FALSE				0
#define API_TRUE					1

#define DISABLE				0
#define ENABLE					1

#define UPDATE_WAIT		0
#define UPDATE_TRUE		1

#define STRIPPING_NONE				0
#define STRIPPING_RUN				1
#define STRIPPING_STOP				2

#define ADV_STATE_NONE				0
#define ADV_STATE_STOP				1
#define ADV_STATE_START			2


/* ----------------------------------------------------------------------------
 * defines
 * --------------------------------------------------------------------------*/

#define DATE_TIME_UPDATE_NONE		0
#define DATE_TIME_UPDATE_FAIL		1
#define DATE_TIME_UPDATE_SUCCESS	2


#define REQUEST_RTC_COMMAND			0x41
#define TRANSFER_COMMAND			0x42
#define CONTROL_COMMAND				0x43
#define CALB_TEMP_COMMAND			0x44
#define EIS_INFO_COMMAND       		0x45
#define EIS_WAIT_COMMAND       		0x46
#define PULSE_START_COMMAND    		0x50



#define REQUEST_RTC_FIRST_STATE			0x11
#define REQUEST_RTC_ERROR_STATE		0x12
#define REPLY_BASIC_INFORM						0x22
#define REPLY_RE_CONNECTION					0x23

#define REQUEST_BASIC_INFO_STATE		0x22
#define TRANSMITTER_POWER_OFF			0x24
#define TRANSMITTER_DFU_FOTA				0x25


#define CALB_TEMP_CLEAR					0x01
#define CALB_TEMP_SETUP					0x02



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


typedef struct _date_time{
	uint8_t year;
	uint8_t mon;
	uint8_t day;

	uint8_t hour;
	uint8_t min;
	uint8_t sec;

	uint8_t update;
	uint8_t sync;

	uint32_t base_sec;

} current_date_time_t;

extern current_date_time_t	_current_date_time;
extern current_date_time_t	*current_date_time;

//--------------------------------------------------------------------------------

typedef struct {
	uint8_t notifying;
	uint8_t cuss_ntf_timeout;
	uint8_t date_time_update;
	uint8_t adv_scan_mode;

	uint8_t is_connect_count;
	uint8_t send_init;
	uint8_t send_count;
	uint8_t param_update_cmd;

	uint8_t nvr3_update_cmd;
	uint8_t nvr3_update_count;
	uint8_t adv_power_config;
	uint8_t power_off_cmd;

	uint8_t agms_power_mode;
	uint8_t agms_adv_start;
	uint8_t agms_adv_state;
	uint8_t device_code;

	uint16_t vbat_lvl_mV;			// LSAD VALUE
	uint16_t temperature;

	float 		vbat_level;			// LSAD VALUE
	float 		Temperature;


}rsl15_info_t;
extern rsl15_info_t _rsl15_info;
extern rsl15_info_t *rsl15_info;

typedef struct {
	uint8_t  stripping_dump;
	uint8_t  stripping_status;
//	uint8_t  measure_update;

	uint16_t weo1_lvl_mV;
	uint16_t weo2_lvl_mV;

	double we1_current;
	double we2_current;

	uint16_t wep1_lvl_mV;
	uint16_t wep2_lvl_mV;

	uint16_t ref_lvl_mV;
	uint16_t vbat_lvl_mV;			// CEM102 VALUE

	uint16_t stripping_time;			// stripping time...600sec
	uint16_t stripping_tmout;


} afe_102_t;
extern afe_102_t	_afe_102;
extern afe_102_t	*afe_102;


//--------------------------------------------------------------------------------


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

typedef struct {
	uint16_t exponent;
	uint8_t  mantissa;
} we_lvl_t;

typedef struct {
	uint8_t run_mode;
	uint8_t dummy1;

	uint16_t idx_head;
	uint16_t idx_tail;
	uint16_t send_pos;

//	tyFLAG	 date_time[LSAD_BUFFER_SIZE+4];			// DATE & TIME
	we_lvl_t we1_current[LSAD_BUFFER_SIZE+4];		// EXPONENT
	we_lvl_t we2_current[LSAD_BUFFER_SIZE+4];		// MANTISSA
	uint16_t Temperature[LSAD_BUFFER_SIZE+4];		// 온도 정보
	uint32_t Epoch_Time[LSAD_BUFFER_SIZE+4];			// EPOCH UNIX TIME
} agms_info_t;

extern agms_info_t	_agms_info;
extern agms_info_t	*agms_info;

extern uint8_t agms_payload_buff[LSAD_SEND_SIZE];


#define TEMP_CALIB_CLEAR			0
#define TEMP_CALIB_ALLOW			5

#define TEMP_CALIB_WAIT_STEP		0
#define TEMP_CALIB_CUMU_STEP		1
#define TEMP_CALIB_IDLE_STEP		2


typedef struct {
	int8_t cal_status;							// 저장된 상태를 확인 한다.
	int8_t cal_step;
	int8_t dummy1;
	int8_t dummy2;

	int16_t wait_count;							// 8초 간격으로 데이터...40 * 8 = 320sec 이후 계산
	int16_t cumulative_count;					// 8초 간격으로 데이터...40 * 8 = 320sec 동안 계산
	int16_t current_ave;
	int16_t absolute_offset;

} Temp_calibration_t;

extern Temp_calibration_t	Temp_calibration;


typedef struct
{
    uint32_t base_epoch;
    uint32_t base_second;

} epoch_time_t;
extern epoch_time_t	epoch_time;


/**
 * @brief       Structure Memory Location
 * @details
 */

extern void app_device_malloc_attach(void);

#endif /* APP_GLOBALS_H */
