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
#include "app_agms.h"

//#include "app_utility.h"


calendar_t		calendar;
device_info_t	device_info;

lsad_temp_t		lsad_temp;

ring_buff_t		ring_buff;
step_buffer_t 	*step_store_buf;

uint8_t agms_payload_buff[LSAD_SEND_SIZE];

//====================================================================================================
//	app defines initial
//====================================================================================================
void app_device_malloc_attach(void)
{
	memset(&device_info, 0, sizeof(device_info_t));
	memset(&calendar, 0, sizeof(calendar_t));
	memset(&lsad_temp, 0, sizeof(lsad_temp_t));
	memset(&ring_buff, 0, sizeof(ring_buff_t));

	step_store_buf = (step_buffer_t *)malloc(sizeof(step_buffer_t));
	memset(step_store_buf, 0, sizeof(step_buffer_t));

	agms_buff_init(&agms_info);
    rb_init(&ring_buff);
}
