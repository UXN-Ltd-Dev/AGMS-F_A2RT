/**
 * @file  app_customss.c
 * @brief Application-specific Bluetooth custom service server source file
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

#include <ble_abstraction.h>
#include <string.h>
#include <swmTrace_api.h>
#include <app_customss.h>
#include <stdio.h>
#include <app.h>

/* Global variable definition */
volatile uint16_t cem102_vbat = 0;
volatile uint32_t WE1_current = 0;
volatile uint32_t WE2_current = 0;
volatile uint32_t temperature = 0;
volatile int32_t WE1_voltage = 0;
volatile int32_t WE2_voltage = 0;
volatile uint8_t cem102_err_status = 0;

static struct app_env_tag_cs0 app_env_cs0;
#if CEM102_DIAG_FUNCTIONS
static struct app_env_tag_cs1 app_env_cs1;
CEM102_DIAG_FUNC_CHAR cem102_diag_func_char;
#endif /* CEM102_DIAG_FUNCTIONS */
static uint8_t conidx_saved=0xff;

uint8_t custom_irq = 0;


volatile uint32_t agme_update_time = 0;

extern int afe_wakeup_step;
extern int afe_wakeup_time;

extern uint8_t max30123_measure_step;
extern uint8_t max30123_chronoamperometry;
extern uint8_t max30123_chrono_A_wait_count;
extern uint32_t customss_NTF_tmout;


#define CHRONO_A_TIMEOUT			60

void CUSTOMSS_NTF_Event_Handle(ke_task_id_t const conidx);


static const struct att_db_desc att_db_cs_svc0[] =
{
    /**** Service 0 ****/
    CS_SERVICE_UUID_128(CS0_SERVICE, CS0_SVC_UUID),


	/* CEM102 agms send data to serve */
	CS_CHAR_UUID_128(CS0_AGMS_SEND_VALUE_CHAR0,
		CS0_AGMS_SEND_VALUE_VAL0,
		CS0_CHAR_AGMS_SEND_UUID,
		(PERM(RD, ENABLE) | PERM(NTF, ENABLE)),
		sizeof(app_env_cs0.agms_to_air_buffer),
		app_env_cs0.agms_to_air_buffer, NULL),

	CS_CHAR_CCC(CS0_AGMS_SEND_VALUE_CCC0,
		app_env_cs0.agms_to_air_buffer, NULL),

	CS_CHAR_USER_DESC(CS0_AGMS_SEND_VALUE_USR_DSCP0,
		sizeof(CS0_AGMS_SEND_CHAR_NAME) - 1,
		CS0_AGMS_SEND_CHAR_NAME, NULL),

		/* CEM102 agms read data to serve */
	CS_CHAR_UUID_128(CS0_AGMS_READ_VALUE_CHAR0,                  	/* Characteristic's ID (attidx_char) */
		CS0_AGMS_READ_VALUE_VAL0,                   										/* Characteristic's Value ID (attidx_val) */
		CS0_CHAR_AGMS_READ_UUID,                        									/* Characteristic's UUID */
		CS0_TX_PERMISSIONS,                      									/* Characteristic's Permissions */
		sizeof(app_env_cs0.agms_to_air_buffer),   							/* Characteristic's Data Length */
		app_env_cs0.agms_to_air_buffer,           									/* Pointer to Characteristic's Data */
		App_BLE_CUSSReadWriteRequestCallback    				/* Pointer to Callback Function */
	),

	CS_CHAR_CCC(CS0_AGMS_READ_VALUE_CCC0,                   				/* CCC's ID */
			app_env_cs0.agms_to_air_buffer, NULL),     	 						/* Pointer to CCC's Data */

	CS_CHAR_USER_DESC(CS0_AGMS_READ_VALUE_USR_DSCP0,   				     /* User Description's ID */
			sizeof(CS0_AGMS_READ_CHAR_NAME) - 1,            			/* User Description's Length */
			CS0_AGMS_READ_CHAR_NAME,  NULL),                      			/* Pointer to Description String */


#if 0
    /* CEM102 battery data to serve */
    CS_CHAR_UUID_128(CS0_VBAT_CEM102_VALUE_CHAR0,
                     CS0_VBAT_CEM102_VALUE_VAL0,
                     CS0_CHAR_BAT_CEM102_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs0.vbat_to_air_buffer),
                     app_env_cs0.vbat_to_air_buffer, NULL),
    CS_CHAR_CCC(CS0_VBAT_CEM102_VALUE_CCC0,
                app_env_cs0.vbat_to_air_cccd_value,
                CUSSTOMSS_CS0_Callback),
    CS_CHAR_USER_DESC(CS0_VBAT_CEM102_VALUE_USR_DSCP0,
                      sizeof(CS0_VBAT_CEM102_CHAR_NAME) - 1,
                      CS0_VBAT_CEM102_CHAR_NAME,
                      NULL),

    /* WE1 data to serve */
    CS_CHAR_UUID_128(CS0_WE1_CURRENT_VALUE_CHAR0,
                     CS0_WE1_CURRENT_VALUE_VAL0,
                     CS0_CHAR_WE1_CURRENT_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs0.we1_curr_to_air_buffer),
                     app_env_cs0.we1_curr_to_air_buffer, NULL),
    CS_CHAR_CCC(CS0_WE1_CURRENT_VALUE_CCC0,
                app_env_cs0.we1_curr_to_air_cccd_value,
                CUSSTOMSS_CS0_Callback),
    CS_CHAR_USER_DESC(CS0_WE1_CURRENT_VALUE_USR_DSCP0,
                      sizeof(CS0_WE1_CURRENT_CHAR_NAME) - 1,
                      CS0_WE1_CURRENT_CHAR_NAME,
                      NULL),

    /* WE2 data to serve */
    CS_CHAR_UUID_128(CS0_WE2_CURRENT_VALUE_CHAR0,
                     CS0_WE2_CURRENT_VALUE_VAL0,
                     CS0_CHAR_WE2_CURRENT_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs0.we2_curr_to_air_buffer),
                     app_env_cs0.we2_curr_to_air_buffer, NULL),
    CS_CHAR_CCC(CS0_WE2_CURRENT_VALUE_CCC0,
                app_env_cs0.we2_curr_to_air_cccd_value,
                CUSSTOMSS_CS0_Callback),
    CS_CHAR_USER_DESC(CS0_WE2_CURRENT_VALUE_USR_DSCP0,
                      sizeof(CS0_WE2_CURRENT_CHAR_NAME) - 1,
                      CS0_WE2_CURRENT_CHAR_NAME,
                      NULL),

    /* Notification on device and measurement error */
    CS_CHAR_UUID_128(CS0_CEM102_ERR_VALUE_CHAR0, CS0_CEM102_ERR_VALUE_VAL0,
                   CS0_CHAR_CEM102_ERR_UUID,
                   PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                   sizeof(app_env_cs0.cem102_err_to_air_buffer),
                   app_env_cs0.cem102_err_to_air_buffer, NULL),
    CS_CHAR_CCC(CS0_CEM102_ERR_VALUE_CCC0,
              app_env_cs0.cem102_err_to_air_cccd_value,
              CUSSTOMSS_CS0_Callback),
    CS_CHAR_USER_DESC(CS0_CEM102_ERR_VALUE_USR_DSCP0,
                    sizeof(CS0_CEM102_ERR_CHAR_NAME) - 1,
                    CS0_CEM102_ERR_CHAR_NAME,
                    NULL),
#endif

#if APP_MEASURE_ACROSS_IO_INPUTS
    /* Temperature data to serve */
    CS_CHAR_UUID_128(CS0_TEMP_VALUE_CHAR0, CS0_TEMP_VALUE_VAL0,
                     CS0_CHAR_TEMP_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs0.temp_to_air_buffer),
                     app_env_cs0.temp_to_air_buffer, NULL),
    CS_CHAR_CCC(CS0_TEMP_VALUE_CCC0,
                app_env_cs0.temp_to_air_cccd_value,
                CUSSTOMSS_CS0_Callback),
    CS_CHAR_USER_DESC(CS0_TEMP_VALUE_USR_DSCP0,
                      sizeof(CS0_TEMP_CHAR_NAME) - 1,
                      CS0_TEMP_CHAR_NAME,
                      NULL),

    /* WE1 voltage data to serve */
    CS_CHAR_UUID_128(CS0_WE1_VOLTAGE_VALUE_CHAR0, CS0_WE1_VOLTAGE_VALUE_VAL0,
                     CS0_CHAR_WE1_VOLTAGE_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs0.we1_volt_to_air_buffer),
                     app_env_cs0.we1_volt_to_air_buffer, NULL),
    CS_CHAR_CCC(CS0_WE1_VOLTAGE_VALUE_CCC0,
                app_env_cs0.we1_volt_to_air_cccd_value,
                CUSSTOMSS_CS0_Callback),
    CS_CHAR_USER_DESC(CS0_WE1_VOLTAGE_VALUE_USR_DSCP0,
                      sizeof(CS0_WE1_VOLT_CHAR_NAME) - 1,
                      CS0_WE1_VOLT_CHAR_NAME,
                      NULL),

    /* WE2 voltage data to serve */
    CS_CHAR_UUID_128(CS0_WE2_VOLTAGE_VALUE_CHAR0, CS0_WE2_VOLTAGE_VALUE_VAL0,
                     CS0_CHAR_WE2_VOLTAGE_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs0.we2_volt_to_air_buffer),
                     app_env_cs0.we2_volt_to_air_buffer, NULL),
    CS_CHAR_CCC(CS0_WE2_VOLTAGE_VALUE_CCC0,
                app_env_cs0.we2_volt_to_air_cccd_value,
                CUSSTOMSS_CS0_Callback),
    CS_CHAR_USER_DESC(CS0_WE2_VOLTAGE_VALUE_USR_DSCP0,
                     sizeof(CS0_WE2_VOLT_CHAR_NAME) - 1,
                     CS0_WE2_VOLT_CHAR_NAME,
                     NULL),
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

};

#if CEM102_DIAG_FUNCTIONS
static const struct att_db_desc att_db_cs_svc1[] =
{
    /**** Service 1 ****/
    CS_SERVICE_UUID_128(CS1_SERVICE, CS1_SVC_UUID),

    /* Diagnostic function WE1 current reference data to serve */
    CS_CHAR_UUID_128(CS1_CURRENT_REF_RATIO_CHAR0, CS1_CURRENT_REF_RATIO_VAL0,
                     CS1_CHAR_CURRENT_REF_RATIO_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.curr_ref_ratio_to_air_buffer),
                     app_env_cs1.curr_ref_ratio_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_CURRENT_REF_RATIO_CCC0,
                app_env_cs1.curr_ref_ratio_to_air_cccd_value,
                CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_CURRENT_REF_RATIO_USR_DSCP0,
                      sizeof(CS1_CURRENT_REF_RATIO_CHAR_NAME) - 1,
                      CS1_CURRENT_REF_RATIO_CHAR_NAME,
                      NULL),

    /* Diagnostic function WE1 current offset data to serve */
    CS_CHAR_UUID_128(CS1_WE1_CURRENT_OFFSET_CHAR0, CS1_WE1_CURRENT_OFFSET_VAL0,
                     CS1_CHAR_WE1_CURRENT_OFFSET_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.we1_curr_offset_to_air_buffer),
                     app_env_cs1.we1_curr_offset_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_WE1_CURRENT_OFFSET_CCC0,
                app_env_cs1.we1_curr_offset_to_air_cccd_value,
                CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_WE1_CURRENT_OFFSET_USR_DSCP0,
                      sizeof(CS1_WE1_CURRENT_OFFSET_CHAR_NAME) - 1,
                      CS1_WE1_CURRENT_OFFSET_CHAR_NAME,
                      NULL),

    /* Diagnostic function WE2 current offset data to serve */
    CS_CHAR_UUID_128(CS1_WE2_CURRENT_OFFSET_CHAR0, CS1_WE2_CURRENT_OFFSET_VAL0,
                     CS1_CHAR_WE2_CURRENT_OFFSET_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.we2_curr_offset_to_air_buffer),
                     app_env_cs1.we2_curr_offset_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_WE2_CURRENT_OFFSET_CCC0,
              app_env_cs1.we2_curr_offset_to_air_cccd_value,
              CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_WE2_CURRENT_OFFSET_USR_DSCP0,
                    sizeof(CS1_WE2_CURRENT_OFFSET_CHAR_NAME) - 1,
                    CS1_WE2_CURRENT_OFFSET_CHAR_NAME,
                    NULL),

    /* Diagnostic function WE1 voltage reference data to serve */
    CS_CHAR_UUID_128(CS1_WE1_VOLTAGE_REF_CHAR0, CS1_WE1_VOLTAGE_REF_VAL0,
                     CS1_CHAR_WE1_VOLTAGE_REF_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.we1_volt_ref_to_air_buffer),
                     app_env_cs1.we1_volt_ref_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_WE1_VOLTAGE_REF_CCC0,
                app_env_cs1.we1_volt_ref_to_air_cccd_value,
                CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_WE1_VOLTAGE_REF_USR_DSCP0,
                      sizeof(CS1_WE1_VOLTAGE_REF_CHAR_NAME) - 1,
                      CS1_WE1_VOLTAGE_REF_CHAR_NAME,
                      NULL),

    /* Diagnostic function WE2 voltage reference data to serve */
    CS_CHAR_UUID_128(CS1_WE2_VOLTAGE_REF_CHAR0, CS1_WE2_VOLTAGE_REF_VAL0,
                     CS1_CHAR_WE2_VOLTAGE_REF_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.we2_volt_ref_to_air_buffer),
                     app_env_cs1.we2_volt_ref_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_WE2_VOLTAGE_REF_CCC0,
              app_env_cs1.we2_volt_ref_to_air_cccd_value,
              CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_WE2_VOLTAGE_REF_USR_DSCP0,
                    sizeof(CS1_WE2_VOLTAGE_REF_CHAR_NAME) - 1,
                    CS1_WE2_VOLTAGE_REF_CHAR_NAME,
                    NULL),

    /* Diagnostic function RE voltage reference data to serve */
    CS_CHAR_UUID_128(CS1_RE_VOLTAGE_REF_CHAR0, CS1_RE_VOLTAGE_REF_VAL0,
                     CS1_CHAR_RE_VOLTAGE_REF_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.re_volt_ref_to_air_buffer),
                     app_env_cs1.re_volt_ref_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_RE_VOLTAGE_REF_CCC0,
                app_env_cs1.re_volt_ref_to_air_cccd_value,
                CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_RE_VOLTAGE_REF_USR_DSCP0,
                      sizeof(CS1_RE_VOLTAGE_REF_CHAR_NAME) - 1,
                      CS1_RE_VOLTAGE_REF_CHAR_NAME,
                      NULL),

    /* Diagnostic function WE1 current leakage data to serve */
    CS_CHAR_UUID_128(CS1_AT1_CURRENT_RESIDUAL_CHAR0, CS1_AT1_CURRENT_RESIDUAL_VAL0,
                     CS1_CHAR_AT1_CURRENT_RESIDUAL_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.at1_curr_residual_to_air_buffer),
                     app_env_cs1.at1_curr_residual_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_AT1_CURRENT_RESIDUAL_CCC0,
                app_env_cs1.at1_curr_residual_to_air_cccd_value,
                CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_AT1_CURRENT_RESIDUAL_USR_DSCP0,
                    sizeof(CS1_AT1_CURRENT_RESIDUAL_CHAR_NAME) - 1,
                    CS1_AT1_CURRENT_RESIDUAL_CHAR_NAME,
                    NULL),

    /* Diagnostic function WE2 current leakage data to serve */
    CS_CHAR_UUID_128(CS1_AT2_CURRENT_RESIDUAL_CHAR0, CS1_AT2_CURRENT_RESIDUAL_VAL0,
                     CS1_CHAR_AT2_CURRENT_RESIDUAL_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.at2_curr_residual_to_air_buffer),
                     app_env_cs1.at2_curr_residual_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_AT2_CURRENT_RESIDUAL_CCC0,
              app_env_cs1.at2_curr_residual_to_air_cccd_value,
              CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_AT2_CURRENT_RESIDUAL_USR_DSCP0,
                    sizeof(CS1_AT2_CURRENT_RESIDUAL_CHAR_NAME) - 1,
                    CS1_AT2_CURRENT_RESIDUAL_CHAR_NAME,
                    NULL),

    /* Diagnostic function WE1 residual current data to serve */
    CS_CHAR_UUID_128(CS1_WE1_CURRENT_RESIDUAL_CHAR0, CS1_WE1_CURRENT_RESIDUAL_VAL0,
                     CS1_CHAR_WE1_CURRENT_RESIDUAL_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.we1_curr_residual_to_air_buffer),
                     app_env_cs1.we1_curr_residual_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_WE1_CURRENT_RESIDUAL_CCC0,
              app_env_cs1.we1_curr_residual_to_air_cccd_value,
              CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_WE1_CURRENT_RESIDUAL_USR_DSCP0,
                    sizeof(CS1_WE1_CURRENT_RESIDUAL_CHAR_NAME) - 1,
                    CS1_WE1_CURRENT_RESIDUAL_CHAR_NAME,
                    NULL),

    /* Diagnostic function WE2 residual current data to serve */
    CS_CHAR_UUID_128(CS1_WE2_CURRENT_RESIDUAL_CHAR0, CS1_WE2_CURRENT_RESIDUAL_VAL0,
                     CS1_CHAR_WE2_CURRENT_RESIDUAL_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.we2_curr_residual_to_air_buffer),
                     app_env_cs1.we2_curr_residual_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_WE2_CURRENT_RESIDUAL_CCC0,
              app_env_cs1.we2_curr_residual_to_air_cccd_value,
              CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_WE2_CURRENT_RESIDUAL_USR_DSCP0,
                    sizeof(CS1_WE2_CURRENT_RESIDUAL_CHAR_NAME) - 1,
                    CS1_WE2_CURRENT_RESIDUAL_CHAR_NAME,
                    NULL),

    /* Diagnostic function RE_CE residual current data to serve */
    CS_CHAR_UUID_128(CS1_RE_CE_CURRENT_RESIDUAL_CHAR0, CS1_RE_CE_CURRENT_RESIDUAL_VAL0,
                     CS1_CHAR_RE_CE_CURRENT_RESIDUAL_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.re_ce_curr_residual_to_air_buffer),
                     app_env_cs1.re_ce_curr_residual_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_RE_CE_CURRENT_RESIDUAL_CCC0,
              app_env_cs1.re_ce_curr_residual_to_air_cccd_value,
              CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_RE_CE_CURRENT_RESIDUAL_USR_DSCP0,
                    sizeof(CS1_RE_CE_CURRENT_RESIDUAL_CHAR_NAME) - 1,
                    CS1_RE_CE_CURRENT_RESIDUAL_CHAR_NAME,
                    NULL),

    /* Diagnostic function RE residual current data to serve */
    CS_CHAR_UUID_128(CS1_RE_CURRENT_RESIDUAL_CHAR0, CS1_RE_CURRENT_RESIDUAL_VAL0,
                     CS1_CHAR_RE_CURRENT_RESIDUAL_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.re_curr_residual_to_air_buffer),
                     app_env_cs1.re_curr_residual_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_RE_CURRENT_RESIDUAL_CCC0,
              app_env_cs1.re_curr_residual_to_air_cccd_value,
              CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_RE_CURRENT_RESIDUAL_USR_DSCP0,
                    sizeof(CS1_RE_CURRENT_RESIDUAL_CHAR_NAME) - 1,
                    CS1_RE_CURRENT_RESIDUAL_CHAR_NAME,
                    NULL),

    /* Diagnostic function OTP data to serve */
    CS_CHAR_UUID_128(CS1_OTP_DATA_CHAR0, CS1_OTP_DATA_VAL0,
                     CS1_OTP_DATA_UUID,
                     PERM(RD, ENABLE) | PERM(NTF, ENABLE),
                     sizeof(app_env_cs1.otp_data_to_air_buffer),
                     app_env_cs1.otp_data_to_air_buffer, NULL),
    CS_CHAR_CCC(CS1_OTP_DATA_CCC0,
              app_env_cs1.otp_data_to_air_cccd_value,
              CUSSTOMSS_CS1_Callback),
    CS_CHAR_USER_DESC(CS1_OTP_DATA_USR_DSCP0,
                    sizeof(CS1_OTP_DATA_CHAR_NAME) - 1,
                    CS1_OTP_DATA_CHAR_NAME,
                    NULL),
};
#endif /* CEM102_DIAG_FUNCTIONS */

static uint32_t notifyOnTimeout;

const struct att_db_desc * CUSTOMSS_GetDatabaseDescription(uint8_t att_db_cs_svc_id)
{
#if CEM102_DIAG_FUNCTIONS
	if (CUST_SVC1 == att_db_cs_svc_id)
	{
		return att_db_cs_svc1;
	}
	else /* CUST_SVC0 */
#endif /* CEM102_DIAG_FUNCTIONS */
	{
		return att_db_cs_svc0;
	}
}

static int CUSTOMSS_SendAttribute(uint8_t conidx, uint8_t attr_to_send)
{
    uint8_t *p_data = NULL;
    uint16_t attr_id = 0;
    uint16_t length = 0;
    uint16_t handle = 0;
    int result = ATTR_NOT_FOUND;

    switch (attr_to_send)
    {
    	case APP_ATTR_SEND_AGMS_VALUE:
    	{
            if (app_env_cs0.agms_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs0.agms_to_air_buffer[0] = 0xA0;
                app_env_cs0.agms_to_air_buffer[1] = 0xBF;

                length = CS0_AGMS_MAX_LENGTH;
                length = 4;
                p_data = app_env_cs0.agms_to_air_buffer;
            }

    	}
    	break;

        case APP_ATTR_SEND_VBAT_VALUE:
        {
            if (app_env_cs0.vbat_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs0.vbat_to_air_buffer[0] = (uint8_t)((cem102_vbat >> 0) & 0xFF);
                app_env_cs0.vbat_to_air_buffer[1] = (uint8_t)((cem102_vbat >> 8) & 0xFF);

                attr_id = CS0_VBAT_CEM102_VALUE_VAL0;
                length = CS0_VBAT_MAX_LENGTH;
                p_data = app_env_cs0.vbat_to_air_buffer;
            }
        }
        break;

        case APP_ATTR_SEND_WE1_CURRENT:
        {
            if (app_env_cs0.we1_curr_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs0.we1_curr_to_air_buffer[0] = (uint8_t)((WE1_current >> 0) & 0xFF);
                app_env_cs0.we1_curr_to_air_buffer[1] = (uint8_t)((WE1_current >> 8) & 0xFF);
                app_env_cs0.we1_curr_to_air_buffer[2] = (uint8_t)((WE1_current >> 16) & 0xFF);
                app_env_cs0.we1_curr_to_air_buffer[3] = (uint8_t)((WE1_current >> 24) & 0xFF);

                attr_id = CS0_WE1_CURRENT_VALUE_VAL0;
                length = CS0_WE1_CURRENT_MAX_LENGTH;
                p_data = app_env_cs0.we1_curr_to_air_buffer;
            }
        }
        break;

        case APP_ATTR_SEND_WE2_CURRENT:
        {
            if (app_env_cs0.we2_curr_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs0.we2_curr_to_air_buffer[0] = (uint8_t)((WE2_current >> 0) & 0xFF);
                app_env_cs0.we2_curr_to_air_buffer[1] = (uint8_t)((WE2_current >> 8) & 0xFF);
                app_env_cs0.we2_curr_to_air_buffer[2] = (uint8_t)((WE2_current >> 16) & 0xFF);
                app_env_cs0.we2_curr_to_air_buffer[3] = (uint8_t)((WE2_current >> 24) & 0xFF);

                attr_id = CS0_WE2_CURRENT_VALUE_VAL0;
                length = CS0_WE2_CURRENT_MAX_LENGTH;
                p_data = app_env_cs0.we2_curr_to_air_buffer;
            }
        }
        break;

        case APP_ATTR_SEND_ERROR_CODE:
        {
            if (app_env_cs0.cem102_err_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs0.cem102_err_to_air_buffer[0] = cem102_err_status;

                attr_id = CS0_CEM102_ERR_VALUE_VAL0;
                length = CS0_CEM102_ERR_MAX_LENGTH;
                p_data = app_env_cs0.cem102_err_to_air_buffer;
            }
        }
        break;

#if APP_MEASURE_ACROSS_IO_INPUTS
        case APP_ATTR_SEND_TEMPERATURE:
        {
            if (app_env_cs0.temp_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs0.temp_to_air_buffer[0] = (uint8_t)((temperature >> 0) & 0xFF);
                app_env_cs0.temp_to_air_buffer[1] = (uint8_t)((temperature >> 8) & 0xFF);
                app_env_cs0.temp_to_air_buffer[2] = (uint8_t)((temperature >> 16) & 0xFF);
                app_env_cs0.temp_to_air_buffer[3] = (uint8_t)((temperature >> 24) & 0xFF);

                attr_id = CS0_TEMP_VALUE_VAL0;
                length = CS0_TEMP_MAX_LENGTH;
                p_data = app_env_cs0.temp_to_air_buffer;
            }
        }
        break;

        case APP_ATTR_SEND_WE1_VOLTAGE:
        {
            if (app_env_cs0.we1_volt_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs0.we1_volt_to_air_buffer[0] = (uint8_t)((WE1_voltage >> 0) & 0xFF);
                app_env_cs0.we1_volt_to_air_buffer[1] = (uint8_t)((WE1_voltage >> 8) & 0xFF);
                app_env_cs0.we1_volt_to_air_buffer[2] = (uint8_t)((WE1_voltage >> 16) & 0xFF);
                app_env_cs0.we1_volt_to_air_buffer[3] = (uint8_t)((WE1_voltage >> 24) & 0xFF);

                attr_id = CS0_WE1_VOLTAGE_VALUE_VAL0;
                length = CS0_WE1_VOLTAGE_MAX_LENGTH;
                p_data = app_env_cs0.we1_volt_to_air_buffer;
            }
        }
        break;

        case APP_ATTR_SEND_WE2_VOLTAGE:
        {
            if (app_env_cs0.we2_volt_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs0.we2_volt_to_air_buffer[0] = (uint8_t)((WE2_voltage >> 0) & 0xFF);
                app_env_cs0.we2_volt_to_air_buffer[1] = (uint8_t)((WE2_voltage >> 8) & 0xFF);
                app_env_cs0.we2_volt_to_air_buffer[2] = (uint8_t)((WE2_voltage >> 16) & 0xFF);
                app_env_cs0.we2_volt_to_air_buffer[3] = (uint8_t)((WE2_voltage >> 24) & 0xFF);

                attr_id = CS0_WE2_VOLTAGE_VALUE_VAL0;
                length = CS0_WE2_VOLTAGE_MAX_LENGTH;
                p_data = app_env_cs0.we2_volt_to_air_buffer;
            }
        }
        break;
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

#if CEM102_DIAG_FUNCTIONS
        case APP_ATTR_SEND_CURRENT_REF_RATIO:
        {
            if (app_env_cs1.curr_ref_ratio_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.curr_ref_ratio_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.cal_curr_ratio >> 0) & 0xFF);
                app_env_cs1.curr_ref_ratio_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.cal_curr_ratio >> 8) & 0xFF);
                app_env_cs1.curr_ref_ratio_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.cal_curr_ratio >> 16) & 0xFF);
                app_env_cs1.curr_ref_ratio_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.cal_curr_ratio >> 24) & 0xFF);

                attr_id = CS1_CURRENT_REF_RATIO_VAL0;
                length = CS1_CURRENT_REF_RATIO_MAX_LENGTH;
                p_data = app_env_cs1.curr_ref_ratio_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.curr_ref_ratio_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_WE1_CURRENT_OFFSET:
        {
            if (app_env_cs1.we1_curr_offset_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.we1_curr_offset_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.WE1_offset_curr >> 0) & 0xFF);
                app_env_cs1.we1_curr_offset_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.WE1_offset_curr >> 8) & 0xFF);
                app_env_cs1.we1_curr_offset_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.WE1_offset_curr >> 16) & 0xFF);
                app_env_cs1.we1_curr_offset_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.WE1_offset_curr >> 24) & 0xFF);

                attr_id = CS1_WE1_CURRENT_OFFSET_VAL0;
                length = CS1_WE1_CURRENT_OFFSET_MAX_LENGTH;
                p_data = app_env_cs1.we1_curr_offset_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.we1_curr_offset_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_WE2_CURRENT_OFFSET:
        {
            if (app_env_cs1.we2_curr_offset_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.we2_curr_offset_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.WE2_offset_curr >> 0) & 0xFF);
                app_env_cs1.we2_curr_offset_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.WE2_offset_curr >> 8) & 0xFF);
                app_env_cs1.we2_curr_offset_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.WE2_offset_curr >> 16) & 0xFF);
                app_env_cs1.we2_curr_offset_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.WE2_offset_curr >> 24) & 0xFF);

                attr_id = CS1_WE2_CURRENT_OFFSET_VAL0;
                length = CS1_WE2_CURRENT_OFFSET_MAX_LENGTH;
                p_data = app_env_cs1.we2_curr_offset_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.we2_curr_offset_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_WE1_VOLTAGE_REF:
        {
            if (app_env_cs1.we1_volt_ref_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.we1_volt_ref_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.WE1_DAC_volt >> 0) & 0xFF);
                app_env_cs1.we1_volt_ref_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.WE1_DAC_volt >> 8) & 0xFF);
                app_env_cs1.we1_volt_ref_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.WE1_DAC_volt >> 16) & 0xFF);
                app_env_cs1.we1_volt_ref_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.WE1_DAC_volt >> 24) & 0xFF);

                attr_id = CS1_WE1_VOLTAGE_REF_VAL0;
                length = CS1_WE1_VOLTAGE_REF_MAX_LENGTH;
                p_data = app_env_cs1.we1_volt_ref_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.we1_volt_ref_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_WE2_VOLTAGE_REF:
        {
            if (app_env_cs1.we2_volt_ref_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.we2_volt_ref_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.WE2_DAC_volt >> 0) & 0xFF);
                app_env_cs1.we2_volt_ref_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.WE2_DAC_volt >> 8) & 0xFF);
                app_env_cs1.we2_volt_ref_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.WE2_DAC_volt >> 16) & 0xFF);
                app_env_cs1.we2_volt_ref_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.WE2_DAC_volt >> 24) & 0xFF);

                attr_id = CS1_WE2_VOLTAGE_REF_VAL0;
                length = CS1_WE2_VOLTAGE_REF_MAX_LENGTH;
                p_data = app_env_cs1.we2_volt_ref_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.we2_volt_ref_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_RE_VOLTAGE_REF:
        {
            if (app_env_cs1.re_volt_ref_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.re_volt_ref_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.RE_DAC_volt >> 0) & 0xFF);
                app_env_cs1.re_volt_ref_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.RE_DAC_volt >> 8) & 0xFF);
                app_env_cs1.re_volt_ref_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.RE_DAC_volt >> 16) & 0xFF);
                app_env_cs1.re_volt_ref_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.RE_DAC_volt >> 24) & 0xFF);

                attr_id = CS1_RE_VOLTAGE_REF_VAL0;
                length = CS1_RE_VOLTAGE_REF_MAX_LENGTH;
                p_data = app_env_cs1.re_volt_ref_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.re_volt_ref_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_AT1_CURRENT_RESIDUAL:
        {
            if (app_env_cs1.at1_curr_residual_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.at1_curr_residual_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.AT1_residual_curr >> 0) & 0xFF);
                app_env_cs1.at1_curr_residual_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.AT1_residual_curr >> 8) & 0xFF);
                app_env_cs1.at1_curr_residual_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.AT1_residual_curr >> 16) & 0xFF);
                app_env_cs1.at1_curr_residual_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.AT1_residual_curr >> 24) & 0xFF);

                attr_id = CS1_AT1_CURRENT_RESIDUAL_VAL0;
                length = CS1_AT1_CURRENT_RESIDUAL_MAX_LENGTH;
                p_data = app_env_cs1.at1_curr_residual_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.at1_curr_residual_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_AT2_CURRENT_RESIDUAL:
        {
            if (app_env_cs1.at2_curr_residual_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.at2_curr_residual_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.AT2_residual_curr >> 0) & 0xFF);
                app_env_cs1.at2_curr_residual_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.AT2_residual_curr >> 8) & 0xFF);
                app_env_cs1.at2_curr_residual_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.AT2_residual_curr >> 16) & 0xFF);
                app_env_cs1.at2_curr_residual_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.AT2_residual_curr >> 24) & 0xFF);

                attr_id = CS1_AT2_CURRENT_RESIDUAL_VAL0;
                length = CS1_AT2_CURRENT_RESIDUAL_MAX_LENGTH;
                p_data = app_env_cs1.at2_curr_residual_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.at2_curr_residual_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_WE1_CURRENT_RESIDUAL:
        {
            if (app_env_cs1.we1_curr_residual_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.we1_curr_residual_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.WE1_residual_curr >> 0) & 0xFF);
                app_env_cs1.we1_curr_residual_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.WE1_residual_curr >> 8) & 0xFF);
                app_env_cs1.we1_curr_residual_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.WE1_residual_curr >> 16) & 0xFF);
                app_env_cs1.we1_curr_residual_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.WE1_residual_curr >> 24) & 0xFF);

                attr_id = CS1_WE1_CURRENT_RESIDUAL_VAL0;
                length = CS1_WE1_CURRENT_RESIDUAL_MAX_LENGTH;
                p_data = app_env_cs1.we1_curr_residual_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.we1_curr_residual_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_WE2_CURRENT_RESIDUAL:
        {
            if (app_env_cs1.we2_curr_residual_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.we2_curr_residual_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.WE2_residual_curr >> 0) & 0xFF);
                app_env_cs1.we2_curr_residual_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.WE2_residual_curr >> 8) & 0xFF);
                app_env_cs1.we2_curr_residual_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.WE2_residual_curr >> 16) & 0xFF);
                app_env_cs1.we2_curr_residual_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.WE2_residual_curr >> 24) & 0xFF);

                attr_id = CS1_WE2_CURRENT_RESIDUAL_VAL0;
                length = CS1_WE2_CURRENT_RESIDUAL_MAX_LENGTH;
                p_data = app_env_cs1.we2_curr_residual_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.we2_curr_residual_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_RE_CE_CURRENT_RESIDUAL:
        {
            if (app_env_cs1.re_ce_curr_residual_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.re_ce_curr_residual_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.RE_CE_residual_curr >> 0) & 0xFF);
                app_env_cs1.re_ce_curr_residual_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.RE_CE_residual_curr >> 8) & 0xFF);
                app_env_cs1.re_ce_curr_residual_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.RE_CE_residual_curr >> 16) & 0xFF);
                app_env_cs1.re_ce_curr_residual_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.RE_CE_residual_curr >> 24) & 0xFF);

                attr_id = CS1_RE_CE_CURRENT_RESIDUAL_VAL0;
                length = CS1_RE_CE_CURRENT_RESIDUAL_MAX_LENGTH;
                p_data = app_env_cs1.re_ce_curr_residual_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.re_ce_curr_residual_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_RE_CURRENT_RESIDUAL:
        {
            if (app_env_cs1.re_curr_residual_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                app_env_cs1.re_curr_residual_to_air_buffer[0] = (uint8_t)((cem102_diag_func_char.RE_residual_curr >> 0) & 0xFF);
                app_env_cs1.re_curr_residual_to_air_buffer[1] = (uint8_t)((cem102_diag_func_char.RE_residual_curr >> 8) & 0xFF);
                app_env_cs1.re_curr_residual_to_air_buffer[2] = (uint8_t)((cem102_diag_func_char.RE_residual_curr >> 16) & 0xFF);
                app_env_cs1.re_curr_residual_to_air_buffer[3] = (uint8_t)((cem102_diag_func_char.RE_residual_curr >> 24) & 0xFF);

                attr_id = CS1_RE_CURRENT_RESIDUAL_VAL0;
                length = CS1_RE_CURRENT_RESIDUAL_MAX_LENGTH;
                p_data = app_env_cs1.re_curr_residual_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.re_curr_residual_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;

        case APP_ATTR_SEND_OTP_DATA:
        {
            if (app_env_cs1.otp_data_to_air_cccd_value[conidx] == ATT_CCC_START_NTF)
            {
                memcpy(app_env_cs1.otp_data_to_air_buffer,
                       cem102_diag_func_char.otp_data,
                       sizeof(app_env_cs1.otp_data_to_air_buffer));

                attr_id = CS1_OTP_DATA_VAL0;
                length = CS1_OTP_DATA_MAX_LENGTH;
                p_data = app_env_cs1.otp_data_to_air_buffer;

                if (CEM102_DiagnosticsDone(cem102))
                {
                    app_env_cs1.otp_data_to_air_cccd_value[conidx] = ATT_CCC_STOP_NTFIND;
                }
            }
        }
        break;
#endif /* CEM102_DIAG_FUNCTIONS */

        default:
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("Invalid attribute (%d).\r\n", attr_to_send);
#endif /* SWMTRACE_OUTPUT */
            result = ATTR_ERROR;
        }
        break;
    }

    if (length)
    {
        /* If attribute ID is APP_ATTR_SEND_CURRENT_REF_RATIO or higher,
         * it belongs to the diagnostics group, which is in service 1.
         * Else, we get the handle for service 0. */
#if CEM102_DIAG_FUNCTIONS
        if (attr_to_send >= APP_ATTR_SEND_CURRENT_REF_RATIO)
        {
            handle = GATTM_GetHandle(CUST_SVC1, attr_id);
        }
        else
#endif /* CEM102_DIAG_FUNCTIONS */
        {
            handle = GATTM_GetHandle(CUST_SVC0, attr_id);
        }

        result = ATTR_FOUND;
    }

    /* Send notification to peer device, if everything checks out.
     * handle could be 0, if
     *    1) We didn't get ack for the previous SendEvtCmd
     *    2) All the attributes are not in ATT_CCC_START_NTF state.
     */
    if (handle && GAPC_IsConnectionActive(conidx))
    {
        GATTC_SendEvtCmd(conidx, GATTC_NOTIFY, attr_to_send,
                         handle, length, p_data);


        if (APP_ATTR_SEND_ERROR_CODE == attr_to_send)
        {
            /* Clear cem102 error status */
            cem102_err_status = CS0_CEM102_ERR_NO_ERROR;
        }
    }

    return result;
}

uint8_t CUSSTOMSS_CS0_Callback(uint8_t conidx, uint16_t attidx,
    uint16_t handle, uint8_t *dest_buffer, const uint8_t *src_buffer,
    uint16_t length, uint16_t operation, uint8_t hl_status,
    uint8_t *cfm_msg_instr)
{
    /* Initialize a variable to hold the return value */
    uint8_t status = hl_status;

    /* Instruct the abstraction layer to send a confirmation message */
    *cfm_msg_instr = CFM_MSG_INSTR_TO_SEND;

    /* Check that there are no high-layer errors */
    if (hl_status == GAP_ERR_NO_ERROR)
    {
        /* Process the attribute that this read/write callback relates to */
        switch (attidx)
        {
			case CS0_AGMS_READ_VALUE_CCC0:
			{
				/* Copy the data into the destination buffer
  				 * CCCD variables are of type uint16_t */
				if (operation == GATTC_WRITE_REQ_IND)
				{
					app_env_cs0.agms_to_air_cccd_value[conidx] = *src_buffer;

#ifdef SWMTRACE_OUTPUT
            swmLogInfo("CUSSTOMSS_CS0_Callback (%d).[%02x]\r\n", attidx, src_buffer[0]);
#endif /* SWMTRACE_OUTPUT */

				}
				break;
			}

        	case CS0_VBAT_CEM102_VALUE_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs0.vbat_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS0_WE1_CURRENT_VALUE_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs0.we1_curr_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS0_WE2_CURRENT_VALUE_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs0.we2_curr_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS0_CEM102_ERR_VALUE_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs0.cem102_err_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }
#if APP_MEASURE_ACROSS_IO_INPUTS
            case CS0_TEMP_VALUE_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs0.temp_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS0_WE1_VOLTAGE_VALUE_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs0.we1_volt_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS0_WE2_VOLTAGE_VALUE_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs0.we2_volt_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
        }

        /* Update the return status */
        status = ATT_ERR_NO_ERROR;
    }

    return status;
}

#if CEM102_DIAG_FUNCTIONS
uint8_t CUSSTOMSS_CS1_Callback(uint8_t conidx, uint16_t attidx,
    uint16_t handle, uint8_t *dest_buffer, const uint8_t *src_buffer,
    uint16_t length, uint16_t operation, uint8_t hl_status,
    uint8_t *cfm_msg_instr)
{
    /* Initialize a variable to hold the return value */
    uint8_t status = hl_status;

    /* Instruct the abstraction layer to send a confirmation message */
    *cfm_msg_instr = CFM_MSG_INSTR_TO_SEND;

    /* Check that there are no high-layer errors */
    if (hl_status == GAP_ERR_NO_ERROR)
    {
        /* Process the attribute that this read/write callback relates to */
        switch (attidx)
        {
            case CS1_CURRENT_REF_RATIO_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.curr_ref_ratio_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_WE1_CURRENT_OFFSET_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.we1_curr_offset_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_WE2_CURRENT_OFFSET_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.we2_curr_offset_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_WE1_VOLTAGE_REF_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.we1_volt_ref_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_WE2_VOLTAGE_REF_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.we2_volt_ref_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_RE_VOLTAGE_REF_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.re_volt_ref_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_AT1_CURRENT_RESIDUAL_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.at1_curr_residual_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_AT2_CURRENT_RESIDUAL_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.at2_curr_residual_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_WE1_CURRENT_RESIDUAL_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.we1_curr_residual_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_WE2_CURRENT_RESIDUAL_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.we2_curr_residual_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_RE_CE_CURRENT_RESIDUAL_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.re_ce_curr_residual_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }

            case CS1_RE_CURRENT_RESIDUAL_CCC0:
            {
                /* Copy the data into the destination buffer
                 * CCCD variables are of type uint16_t */
                if (operation == GATTC_WRITE_REQ_IND)
                {
                    app_env_cs1.re_curr_residual_to_air_cccd_value[conidx] = *src_buffer;
                }
                break;
            }
        }

        /* Update the return status */
        status = ATT_ERR_NO_ERROR;
    }

    return status;
}
#endif

static void CUSTOMSS_NTF_Timer(void)
{
    uint8_t conidx = 0;
    int result = ATTR_NOT_FOUND;

    int index = APP_ATTR_SEND_FIRST + 1;

    /* Search for the first attritute with the notification enabled
     *  and send the notification */
    while ((result == ATTR_NOT_FOUND) && (conidx < APP_MAX_NB_CON))
    {
        result = CUSTOMSS_SendAttribute(conidx, index++);
        if (index >= APP_ATTR_SEND_LAST)
        {
            conidx++;
            index = APP_ATTR_SEND_FIRST + 1;
        }
    }

    CUSTOMSS_StartTimer();
}

static void CUSTOMSS_Init_Service0(void)
{
    memset(&app_env_cs0, '\0', sizeof(struct app_env_tag_cs0));

    /* Initialize the client characteristic configurations to start
     * notifications. This is done for attributes that are transmitted to the
     * client devices. */
    for (uint8_t i = 0; i < APP_MAX_NB_CON; i++)
    {
        app_env_cs0.agms_to_air_cccd_value[i] = ATT_CCC_START_NTF;

        app_env_cs0.vbat_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs0.we1_curr_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs0.we2_curr_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs0.cem102_err_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;


#if APP_MEASURE_ACROSS_IO_INPUTS
        app_env_cs0.temp_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs0.we1_volt_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs0.we2_volt_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
    }
}

#if CEM102_DIAG_FUNCTIONS
static void CUSTOMSS_Init_Service1(void)
{
    memset(&app_env_cs1, '\0', sizeof(struct app_env_tag_cs1));

    /* Initialize the client characteristic configurations to start
     * notifications. This is done for attributes that are transmitted to the
     * client devices. */
    for (uint8_t i = 0; i < APP_MAX_NB_CON; i++)
    {
        app_env_cs1.curr_ref_ratio_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.we1_curr_offset_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.we2_curr_offset_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.we1_volt_ref_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.we2_volt_ref_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.re_volt_ref_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.at1_curr_residual_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.at2_curr_residual_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.we1_curr_residual_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.we2_curr_residual_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.re_ce_curr_residual_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.re_curr_residual_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
        app_env_cs1.otp_data_to_air_cccd_value[i] = ATT_CCC_STOP_NTFIND;
    }
}
#endif /* CEM102_DIAG_FUNCTIONS */

void CUSTOMSS_Initialize(void)
{
#if CEM102_DIAG_FUNCTIONS
    /* Initialize diagnostic function characteristics values */
    memset(&cem102_diag_func_char, 0, sizeof(CEM102_DIAG_FUNC_CHAR));
#endif /* CEM102_DIAG_FUNCTIONS */

    CUSTOMSS_Init_Service0();
#if CEM102_DIAG_FUNCTIONS
    CUSTOMSS_Init_Service1();
#endif /* CEM102_DIAG_FUNCTIONS */

    CUSTOMSS_NotifyOnTimeout(TIMER_SETTING_S(10));		// sodykim

    MsgHandler_Add(CUSTOMSS_NTF_TIMEOUT, CUSTOMSS_MsgHandler);
    MsgHandler_Add(GATTC_CMP_EVT, CUSTOMSS_MsgHandler);
    MsgHandler_Add(GAPC_DISCONNECT_IND, CUSTOMSS_MsgHandler);
    
//---------------------------------------------------------------------------------------------
//	ADD to CUSTOMSS_MsgHandler
//---------------------------------------------------------------------------------------------
	MsgHandler_Add(APP_CUSS_AGMS_REQ_DATE_TIME, CUSTOMSS_MsgHandler);
	MsgHandler_Add(APP_CUSS_AGMS_NEXT_SEND_DATA, CUSTOMSS_MsgHandler);

	MsgHandler_Add(APP_CUSS_CHRONO_A_TIMEOUT, CUSTOMSS_MsgHandler);

//	MsgHandler_Add(APP_CUSS_AFE_WAKEUP_TIMEOUT, CUSTOMSS_MsgHandler);
}

#if 0
void CUSTOMSS_AFE_Wakeup_Timeout(void)
{
    ke_timer_set(APP_CUSS_AFE_WAKEUP_TIMEOUT, TASK_APP, TIMER_SETTING_S(10));

}
#endif

void CUSTOMSS_Notify_Now(void)
{
    for (uint8_t i = 0; i < APP_MAX_NB_CON; i++)
    {
        if (GAPC_IsConnectionActive(i))
        {
            /* Change the notification timer to expire 1ms later */
            ke_timer_set(CUSTOMSS_NTF_TIMEOUT, TASK_APP, 1);
        }
    }
}

void Call_Custom_NTF_Timeout(uint32_t timeout)
{
	ke_timer_set(CUSTOMSS_NTF_TIMEOUT, TASK_APP, TIMER_SETTING_S(timeout));
}


/* CUSTOM Service Server specific, but generic function so that other modules
 * can use. That's why, timeout and CUSTOMSS_NTF_TIMEOUT are not exposed to
 * the caller.
 */
void CUSTOMSS_StartTimer(void)
{
    if (notifyOnTimeout)
    {
        ke_timer_set(CUSTOMSS_NTF_TIMEOUT, TASK_APP,
                     notifyOnTimeout);
    }
}

void CUSTOMSS_NotifyOnTimeout(uint32_t timeout)
{
    notifyOnTimeout = timeout;
}

void CUSTOMSS_MsgHandler(ke_msg_id_t const msg_id, void const *param,
                         ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint8_t conidx = KE_IDX_GET(src_id);

    switch (msg_id)
    {
        case CUSTOMSS_NTF_TIMEOUT:
        {
#if 0
			CUSTOMSS_NTF_Timer();
#else
		    if (conidx_saved != conidx) {
		        conidx_saved = conidx;

				ke_timer_set(APP_CUSS_CHRONO_A_TIMEOUT, KE_BUILD_ID(TASK_APP, conidx),TIMER_SETTING_S(CHRONO_A_TIMEOUT));
		    }

#if 0
			if(rsl15_info->date_time_update == DATE_TIME_UPDATE_SUCCESS) {
//				LSAD_measure_sensor_level();												// read to battery & temperature...rtc time sync
				append_lsad_lvl_payload();												// add_Queue Buffer

				if ((app_env_cs0.agms_to_air_cccd_value[0] == ATT_CCC_START_NTF)
						&& GAPC_IsConnectionActive(conidx))
				{
					agms_send_payload_packet(conidx);
				}
			}
#endif

#if 0
			swmLogInfo("CUSTOMSS_NTF_TIMEOUT : %ld\r\n", (Sys_RTC_Value_Seconds() - customss_NTF_tmout));
			customss_NTF_tmout = Sys_RTC_Value_Seconds();
#endif


			custom_irq = 1;
			CUSTOMSS_StartTimer();

			LSAD_measure_sensor_level();				// 온도 측정을 위해...
#endif
        }
        break;

/**************************************************************************************************************
 * BELOW TO UXN...
 **************************************************************************************************************/
        case APP_CUSS_AGMS_REQ_DATE_TIME:

			/* Check if the peer device is connected and can be notified */
			if ((app_env_cs0.agms_to_air_cccd_value[0] == ATT_CCC_START_NTF)
					&& GAPC_IsConnectionActive(conidx))
			{

				if(rsl15_info->date_time_update!=DATE_TIME_UPDATE_SUCCESS) {
					agms_request_current_data_time(conidx, REQUEST_RTC_FIRST_STATE);
					ke_timer_set(APP_CUSS_AGMS_REQ_DATE_TIME, KE_BUILD_ID(TASK_APP, conidx),TIMER_SETTING_S(3));																							// 3sec interval

	#ifdef SWMTRACE_DEBUG
					swmLogInfo("REQUEST_DATE_TIME_UPDATE!!\r\n");
	#endif
				}
			}
			break;

		case APP_CUSS_AGMS_NEXT_SEND_DATA:

			/* Check if the peer device is connected and can be notified */
			if ((app_env_cs0.agms_to_air_cccd_value[0] == ATT_CCC_START_NTF)
					&& GAPC_IsConnectionActive(conidx))
			{
				agms_send_payload_packet(conidx);
			    CUSTOMSS_StartTimer();						// 연속으로 보내는 경우 CUSTOM TIME시간을 다시 초기화 한다.
			}
			break;

//	1분 마다 이벤트가 발생을 한다...1440min is 1day
		case APP_CUSS_CHRONO_A_TIMEOUT:						// 60초 간격으로 시간을 처리 한다..
			if((max30123_measure_step == MAX30123_STEP_DC_CURRENT) && (rsl15_info->date_time_update == DATE_TIME_UPDATE_SUCCESS))
			{
				max30123_chrono_A_wait_tmout++;
				if(max30123_chrono_A_wait_tmout > CHRONO_A_START_TIME) {
					max30123_measure_step = MAX30123_STEP_IMPEDANCE;
					max30123_chronoamperometry = 0;
					max30123_chrono_A_wait_tmout = 0;
				}
			}

			ke_timer_set(APP_CUSS_CHRONO_A_TIMEOUT, KE_BUILD_ID(TASK_APP, conidx),TIMER_SETTING_S(CHRONO_A_TIMEOUT));

			break;

			default:
				break;



#if 0		// del sodykim for
        case GATTC_CMP_EVT:
        {
            const struct gattc_cmp_evt *p = param;

            if (p->operation == GATTC_NOTIFY)
            {
                if (p->status == ATT_ERR_NO_ERROR)
                {
                    int index = p->seq_num + 1;
                    int result = ATTR_NOT_FOUND;

                    while ((result == ATTR_NOT_FOUND) && (conidx < APP_MAX_NB_CON)
                            && (index < APP_ATTR_SEND_LAST))
                    {
                        result = CUSTOMSS_SendAttribute(conidx, index++);

                        if (index >= APP_ATTR_SEND_LAST)
                        {
                            conidx++;
                            index = APP_ATTR_SEND_FIRST + 1;
                        }
                    }
                }
#ifdef SWMTRACE_OUTPUT
                else
                {
                    swmLogInfo("SendEvtCmd failed (conid=%d, seq=%d)\r\n",
                               conidx, p->seq_num);
                }
#endif /* SWMTRACE_OUTPUT */
            }
        }
        break;

        case GAPC_DISCONNECT_IND:
        {
            /* Stop the periodic timer for the inactive connection.
             * APP_SendConCfm will restart the timer. */
            ke_timer_clear(CUSTOMSS_NTF_TIMEOUT,
                           KE_BUILD_ID(TASK_APP, conidx));
        }
        break;
#endif
    }
}


/********************************************************************************
 * CUSTOM NTF MSG HANDLER
 ********************************************************************************/
void CUSTOMSS_NTF_Event_Handle(ke_task_id_t const conidx)
{
    if (conidx_saved != conidx) {
        conidx_saved = conidx;
    }

#ifdef SWMTRACE_OUTPUT
	swmLogInfo("CUSTOMSS_NTF_Event_Handle (%d).(%d)\r\n", conidx, GAPC_IsConnectionActive(conidx));
#endif /* SWMTRACE_OUTPUT */


    CUSTOMSS_StartTimer();
	LSAD_measure_sensor_level();																// read to battery & temperature...rtc time sync

	if(rsl15_info->date_time_update == DATE_TIME_UPDATE_SUCCESS) {

		append_lsad_lvl_payload();																// add_Queue Buffer

		if ((app_env_cs0.agms_to_air_cccd_value[0] == ATT_CCC_START_NTF)
				&& GAPC_IsConnectionActive(conidx))
		{
			agms_send_payload_packet(conidx);
		}
    }
}


/* ----------------------------------------------------------------------------
 * Public Function Definitions
 * ------------------------------------------------------------------------- */
uint8_t App_BLE_CUSSReadWriteRequestCallback(uint8_t conidx, uint16_t attidx,
    uint16_t handle, uint8_t *dest_buffer, const uint8_t *src_buffer,
    uint16_t length, uint16_t operation, uint8_t hl_status,
    uint8_t *cfm_msg_instr)
{

	/* Initialize a variable to hold the return value */
    uint8_t status = hl_status;

    /* Instruct the abstraction layer to send a confirmation message */
    *cfm_msg_instr = CFM_MSG_INSTR_TO_SEND;

    /* Check that there are no high-layer errors */
    if (hl_status == GAP_ERR_NO_ERROR)
    {
        /* Copy the data into the destination buffer */
        memcpy(dest_buffer, src_buffer, length);

        /* Process the attribute that this read/write callback relates to */
        switch (attidx)
        {
            /* Read or write request to TX characteristic */
            case CS0_AGMS_SEND_VALUE_VAL0:
            {
                /* Log the received data */
#ifdef SWMTRACE_DEBUG
                swmLogInfo("TX R/W Callback (%d):(%d) ", conidx, length);
#endif
                break;
            }

            case CS0_AGMS_READ_VALUE_VAL0:
            {
                /* Log the received data */
#ifdef SWMTRACE_DEBUG
                swmLogInfo("RX R/W Callback (%d):(%d) ", conidx, length);
#endif

#if 0
                for(int i =0;i<length;i++) {
                    swmLogInfo("[%02X] ", dest_buffer[i]);
                }
                swmLogInfo("\r\n");
#endif

                progress_receive_msg_event(conidx, dest_buffer);

                break;
            }


        }

        /* Update the return status */
        status = ATT_ERR_NO_ERROR;
    }
    else
    {
        /* Log the error code */
#ifdef SWMTRACE_DEBUG
        swmLogInfo(
            "R/W Request Callback (%d); attribute (%d); operation (%d); error (%d)\r\n",
            conidx, attidx, operation, hl_status
        );
#endif
    }

    return status;
}


void set_agms_conidx(uint8_t conidx)
{
	conidx_saved = conidx;
}

uint8_t get_agms_conidx(void)
{
	return conidx_saved;
}

uint8_t read_custom_irq(void)
{
	return custom_irq;
}

void clear_custom_irq(void)
{
	custom_irq = 0;
}



void app_custom_send_payload_packet(void)
{
	/* Check if the peer device is connected and can be notified */
	if ((app_env_cs0.agms_to_air_cccd_value[0] == ATT_CCC_START_NTF)
			&& GAPC_IsConnectionActive(conidx_saved))
	{
		if(impedance_calendar.update) {
	        swmLogInfo("send_impedance_info\r\n");
			agms_send_impedance_info_packet(conidx_saved, EIS_INFO_COMMAND);				// EIS INFO

		} else {
			if(max30123_chrono_A_wait_count==0) {
				swmLogInfo("send_payload_info\r\n");
				agms_send_payload_packet(conidx_saved);
			}
		}
	}
}
