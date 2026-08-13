/**
 * @file    app_init.h
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

#ifndef APP_LSAD_H
#define APP_LSAD_H

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif /* ifdef __cplusplus */

/* ----------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------- */

/* Device and library headers */
#include <RTE_Device.h>

/* Application headers */

#define LSAD_INPUT_FAIL 	         (0)
#define LSAD_INPUT_SUCCESS  	     (1)


#define LSAD_SAMPLE_DISABLE     0
#define LSAD_SAMPLE_ENABLE      1

#define LSAD_SAMPLE_MODE    LSAD_SAMPLE_ENABLE


/* ----------------------------------------------------------------------------
 * Symbolic Constants
 * ------------------------------------------------------------------------- */
#define NUM_LSAD_SAMPLE_COUNT          	(8U)
#define NUM_LSAD_CHANNELS_USED          (3U)



/* LSAD interrupt will be set to occur after sampling channel 2.
 * Of the channels used in this application, this one is last in the sampling order.
 */

#define LSAD_WEO_CHANNEL                (LSAD_INPUT_CH0)
#define LSAD_REF_CHANNEL                (LSAD_INPUT_CH1)

#define LSAD_IRQ_CHANNEL                (LSAD_INPUT_CH2)

/* LSAD interrupt will trigger at approximately 98 Hz assuming 1 MHz slow clock and
 * LSAD_MODE_NORMAL is used.
 */
#define LSAD_PRESCALE                   (LSAD_PS_1280H)

#define NUM_SAMPLES                     (98U)
#define ROUNDING_OFFSET                 (0.5f)

#define DUMMY_OFFSET1                 	(45)
#define DUMMY_OFFSET2                 	(60)
#define DUMMY_OFFSET3                 	(65)
#define DUMMY_OFFSET4                 	(70)

/* User input work out GPIO */
#define WEO_SENSOR_GPIO                 (LSAD_GPIO10_CFG)

/* User input reference GPIO */
#define REF_SENSOR_GPIO                 (LSAD_GPIO1_CFG)




uint16_t lsad_calc_crc16(uint16_t length);
void append_data_time_payload(uint16_t idx, uint8_t year, uint8_t mon, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);
void load_data_time_payload(uint16_t idx, uint8_t *year, uint8_t *mon, uint8_t *day, uint8_t *hour, uint8_t *min, uint8_t *sec);
void append_epoch_time(uint16_t idx, uint32_t *e);
void load_epoch_time(uint16_t idx, uint32_t *e);

uint16_t g_atoi16(uint8_t  *au8_buff, int i);
uint32_t g_atoi32(uint8_t *au8_buff,  int i);
void g_itoa16(uint16_t value, uint8_t *au8_buff);
void g_itoa32(uint32_t value, uint8_t *au8_buff);

void append_lsad_lvl_payload(void);
void append_we_current(uint16_t idx, float we1, float we2);


void agms_send_payload_packet(uint8_t conidx);
void agms_send_impedance_info_packet(uint8_t conidx, uint8_t command);

void agms_request_current_data_time(uint8_t conidx, uint8_t status);

void get_current_data_time_cmd(uint8_t conidx, uint8_t state);

void progress_receive_msg_event(uint8_t conidx, const uint8_t *dest_buff);


/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif /* ifdef __cplusplus */

#endif /* APP_INIT_H */
