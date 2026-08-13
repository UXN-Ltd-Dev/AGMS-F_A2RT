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


/* Application headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_globals.h"
//#include "app_utility.h"



current_date_time_t	 _current_date_time;
current_date_time_t	*current_date_time;

rsl15_info_t 	_rsl15_info;
rsl15_info_t 	*rsl15_info;

afe_102_t		_afe_102;
afe_102_t		*afe_102;

agms_info_t		_agms_info;
agms_info_t		*agms_info;

Temp_calibration_t	Temp_calibration;

epoch_time_t	epoch_time;

/*
 *	STX	Company ID	Command	Length	Date&Time WEO Level	AEO Level	WEP Level	AEP Level	REF Level	Battery	crc16
 *  2byte	2byte	1byte	 1byte	 6byte	   max 50*2	max 50*2	2 byte		2 byte			2			2		2	   = 200byte + 22 = 222 byte(max packet size)

 *	STX		Command		Status Length Reverse Checksum	Date&Time WEO1 Level	WEO2 Level
 *  2byte     	1byte	1	  1byte  1byte	 1byte	         2byte 	        6byte      (3byte      + 		3byte) * 40 =  10byte + 6byte + 240byte = 256

 */

uint8_t agms_payload_buff[LSAD_SEND_SIZE];


//====================================================================================================
//	app defines initial
//====================================================================================================
void app_device_malloc_attach(void)
{
#if 1

	current_date_time = &_current_date_time;
	memset(current_date_time, 0, sizeof(current_date_time_t));

	rsl15_info = &_rsl15_info;
	memset(rsl15_info, 0, sizeof(rsl15_info_t));

	afe_102 = &_afe_102;
	memset(afe_102, 0, sizeof(afe_102_t));

	agms_info = &_agms_info;
	memset(agms_info, 0, sizeof(agms_info_t));

	memset(&Temp_calibration, 0, sizeof(Temp_calibration_t));
	memset(&epoch_time, 0, sizeof(epoch_time_t));


#else
	rsl15_info = (rsl15_info_t *)malloc(sizeof(rsl15_info_t));
	memset(rsl15_info, 0, sizeof(rsl15_info_t));

	afe_102 = (afe_102_t *)malloc(sizeof(afe_102_t));
	memset(afe_102, 0, sizeof(afe_102_t));

	agms_info = (agms_info_t *)malloc(sizeof(agms_info_t));
	memset(agms_info, 0, sizeof(agms_info_t));

	current_date_time = (current_date_time_t *)malloc(sizeof(current_date_time_t));
	memset(current_date_time, 0, sizeof(current_date_time_t));

#endif
}
