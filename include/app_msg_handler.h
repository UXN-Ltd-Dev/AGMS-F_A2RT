/**
 * @file  app_msg_handler.h
 * @brief Application message handlers header
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

#ifndef APP_MSG_HANDLER_H
#define APP_MSG_HANDLER_H

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif    /* ifdef __cplusplus */

#include <ke_msg.h>

/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/
#define MAX_CON_PARAM_UPDATE_ATTEMPT  5

#if 0
typedef struct {
	uint16_t intv_min;
	uint16_t intv_max;
	uint16_t latency;
	uint16_t sv_tmout;
} connection_interval_time_t;
extern connection_interval_time_t	connection_interval_time;
#else
struct connection_interval_time{
	uint16_t intv_min;
	uint16_t intv_max;
	uint16_t latency;
	uint16_t sv_tmout;
};
#endif

extern char APP_DEV_VER_BUFF[8];
#define APP_DEV_VER                  "V110"

/* ----------------------------------------------------------------------------
 * Function Prototypes
 * ------------------------------------------------------------------------- */

/**
 * @brief       Callback handler for BLE configuration events.
 * @param[in]   msg_id     The kernel message ID number.
 * @param[in]   param      The message parameter.
 * @param[in]   dest_id    The destination task ID number.
 * @param[in]   src_id     The source task ID number.
 */
void BLE_ConfigHandler(ke_msg_id_t const msg_id, void const *param,
                            ke_task_id_t const dest_id,
                            ke_task_id_t const src_id);

/**
 * @brief       Callback handler for BLE activity events.
 * @param[in]   msg_id     The kernel message ID number.
 * @param[in]   param      The message parameter.
 * @param[in]   dest_id    The destination task ID number.
 * @param[in]   src_id     The source task ID number.
 */
void BLE_ActivityHandler(ke_msg_id_t const msg_id, void const *param,
                            ke_task_id_t const dest_id,
                            ke_task_id_t const src_id);

/**
 * @brief       Callback handler for BLE connection events.
 * @param[in]   msg_id     The kernel message ID number.
 * @param[in]   param      The message parameter.
 * @param[in]   dest_id    The destination task ID number.
 * @param[in]   src_id     The source task ID number.
 */

void BLE_ConnectionHandler(ke_msg_id_t const msg_id, void const *param,
                            ke_task_id_t const dest_id,
                            ke_task_id_t const src_id);

/**
 * @brief       Callback handler for BLE paring events.
 * @param[in]   msg_id     The kernel message ID number.
 * @param[in]   param      The message parameter.
 * @param[in]   dest_id    The destination task ID number.
 * @param[in]   src_id     The source task ID number.
 */
void BLE_PairingHandler(ke_msg_id_t const msg_id, void const *param,
                            ke_task_id_t const dest_id,
                            ke_task_id_t const src_id);

/**
 * @brief       Set up advertising and scan response data buffers
 */
void PrepareAdvScanData(void);

/**
 * @brief       Send connection confirmation
 */
void APP_SendConCfm(uint8_t conidx);

#if 1
void ParamUpdateHandler(ke_msg_id_t const msg_id, void const *param,
                    ke_task_id_t const dest_id, ke_task_id_t const src_id);

void App_BLE_Power_Handler(ke_msg_id_t const msg_id, void const *param,
        ke_task_id_t const dest_id, ke_task_id_t const src_id);


void  adv_activity_reset_process(void);
#endif
/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif    /* APP_MSG_HANDLER_H */
