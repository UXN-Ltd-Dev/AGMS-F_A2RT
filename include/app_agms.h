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

#define AB_CAPACITY      720u   /* 최대 저장 개수                    */
#define AB_BATCH_MIN     1u     /* 1회 전송 최소 개수                 */
#define AB_BATCH_MAX     60u    /* 1회 전송 최대 개수                 */
#define AB_BATCH_DEFAULT 60u    /* 기본 배치 크기                    */


/****************************************************************************
 *	저장 단위
 ****************************************************************************/
typedef struct {
    uint32_t epoch_time;
    float we1;
    float we2;
    int16_t celsius;
    int16_t flags;
} agms_item_t;

typedef struct {
	agms_item_t buf[AB_CAPACITY];
	uint8_t run_mode;
	uint8_t run_state;

    uint16_t  head;      	/* 다음 쓰기 위치            */
    uint16_t  tail;      	/* 가장 오래된 데이터 위치    */
    uint16_t  count;     	/* 현재 저장 개수            */
    uint16_t  pending;   	/* 전송 후 ACK 대기 중인 개수 */
    uint16_t  remain;  		/* 전송 후 남아 있는 개수   */

} agms_buff_t;
extern agms_buff_t	agms_info;


typedef struct {
	agms_item_t  batch[AB_BATCH_MAX];  	/* 최대 60개 분량 확보 */
    uint16_t   batch_n;              	/* 이번에 실제 보낸 개수 */
    uint16_t   batch_size;           	/* 요청 배치 크기 1~60   */
    uint16_t   batch_ramain;           	/* 남아 있는 개수   */

} agms_batch_t;
extern agms_batch_t	agms_batch;

/* ----------------------------------------------------------------------------
 * Symbolic Constants
 * ------------------------------------------------------------------------- */



/* LSAD interrupt will be set to occur after sampling channel 2.
 * Of the channels used in this application, this one is last in the sampling order.
 */


/* LSAD interrupt will trigger at approximately 98 Hz assuming 1 MHz slow clock and
 * LSAD_MODE_NORMAL is used.
 */



uint16_t lsad_calc_crc16(uint8_t *payload_buff, uint16_t length);

int8_t agms_buff_is_empty(const agms_buff_t *ab);
int8_t agms_buff_is_full (const agms_buff_t *ab);

void agms_buff_init(agms_buff_t *a);
void agms_buff_reset(agms_buff_t *a);
int8_t agms_buff_push_item(agms_buff_t *a, const agms_item_t *item);
int8_t agms_buff_push(agms_buff_t *a, float we1, float we2, int16_t celsius, uint32_t epoch_time);
int8_t agms_buff_pop(agms_buff_t *a, agms_item_t *out);
uint16_t agms_buff_count(const agms_buff_t *a);
uint16_t agms_buff_batch_clamp(uint16_t n);
uint16_t agms_buff_peek(agms_buff_t *a, agms_item_t *out, uint16_t max_n);
uint16_t agms_buff_commit(agms_buff_t *a, uint16_t n);


void append_Potentiostat_data(void);

uint16_t g_atoi16(uint8_t  *au8_buff, int i);
uint32_t g_atoi32(uint8_t *au8_buff,  int i);
void g_itoa16(uint16_t value, uint8_t *au8_buff);
void g_itoa32(uint32_t value, uint8_t *au8_buff);

uint16_t agms_send_payload_packet(void);
uint16_t agms_send_impedance_info_packet(uint8_t command);
uint16_t agms_read_current_calendar(uint8_t status);

void agms_callback_msg_event(uint8_t conidx, const uint8_t *rb_buff);


void rb_init(ring_buff_t *rb);
int16_t rb_count(const ring_buff_t *rb);
void rb_push(ring_buff_t *rb, float fwe1, float fwe2);
int rb_flush_trimmed_average(ring_buff_t *rb, float *out_avg1, float *out_avg2);


/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif /* ifdef __cplusplus */

#endif /* APP_INIT_H */
