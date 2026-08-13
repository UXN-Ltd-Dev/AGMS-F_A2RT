/**
 * @file  app_customss.h
 * @brief Application-specific Bluetooth custom service server header file
 *
 * @copyright @parblock
 * Copyright (c) 2025 Semiconductor Components Industries, LLC (d/b/a
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

#ifndef APP_CUSTOMSS_H
#define APP_CUSTOMSS_H

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif    /* ifdef __cplusplus */

/* ----------------------------------------------------------------------------
 * Include files
 * --------------------------------------------------------------------------*/
#include <gattc_task.h>
#include <app.h>


#define CUSS_ENABLE_TEMPERATURE_CHAR    (1)

/* ----------------------------------------------------------------------------
 * sodykim define 
 * --------------------------------------------------------------------------*/
#define CS0_TX_PERMISSIONS               (PERM(RD, ENABLE) \
                                         | PERM(WRITE_REQ, ENABLE) \
                                         | PERM(WRITE_COMMAND, ENABLE) \
                                         | PERM(RP, SEC_CON))

#define CS0_RX_PERMISSIONS               (PERM(RD, ENABLE) | PERM(NTF, ENABLE))

/* Custom service UUIDs and characteristics */

/* Custom service UUIDs */
#define CS_SVC_UUID                     { 0x24, 0xdc, 0x0e, 0x6e, 0x01, 0x40, \
                                          0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                          0xb5, 0xf3, 0x93, 0xe0 }
/**
 * @brief       The UUIDs for the characteristics of custom service 0
 * @details     These 128-bit UUIDs are used during Bluetooth Low Energy
 *              communications to identify the various characteristics and data
 *              that belong to custom service 0.
 *              Refer to the ReadMe for details about the functions of each
 *              characteristic.
 */
#define CS0_CHAR_TX_UUID                 { 0x24, 0xdc, 0x0e, 0x6e, 0x03, 0x40, \
                                          0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                          0xb5, 0xf3, 0x93, 0xe0 }

#define CS0_CHAR_RX_UUID                 { 0x24, 0xdc, 0x0e, 0x6e, 0x02, 0x40, \
                                          0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                          0xb5, 0xf3, 0x93, 0xe0 }
                                          
#define CS0_VALUE_MAX_LENGTH     		  (20)    /* Used by the TX and RX characteristics */

#define CS0_CHAR_TX_NAME                 "TX_VALUE"
#define CS0_CHAR_RX_NAME                 "RX_VALUE"



/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/
#define CEM102_OTP_DATA_SIZE                 (32)

/* Diagnostics functions variable data to be sent to a mobile device. */
typedef struct
{
    uint32_t cal_curr_ratio;                 /* Floating points value encoded in IEEE11073 format */
    uint32_t WE1_offset_curr;                /* Floating points value encoded in IEEE11073 format */
    uint32_t WE2_offset_curr;                /* Floating points value encoded in IEEE11073 format */
    int32_t WE1_DAC_volt;                    /* In mV, signed value */
    int32_t WE2_DAC_volt;                    /* In mV, signed value */
    int32_t RE_DAC_volt;                     /* In mV, signed value */
    uint32_t AT1_residual_curr;              /* Floating points value encoded in IEEE11073 format */
    uint32_t AT2_residual_curr;              /* Floating points value encoded in IEEE11073 format */
    uint32_t WE1_residual_curr;              /* Floating points value encoded in IEEE11073 format */
    uint32_t WE2_residual_curr;              /* Floating points value encoded in IEEE11073 format */
    uint32_t RE_CE_residual_curr;            /* Floating points value encoded in IEEE11073 format */
    uint32_t RE_residual_curr;               /* Floating points value encoded in IEEE11073 format */
    uint8_t otp_data[CEM102_OTP_DATA_SIZE];  /* OTP Data in binary value */
} CEM102_DIAG_FUNC_CHAR;

#define ATTR_NOT_FOUND                         (0)
#define ATTR_FOUND                             (1)
#define ATTR_ERROR                             (-1)

/* Custom service UUIDs and characteristics */

/* Custom service UUIDs */

#define CS0_SVC_UUID                           { 0x24, 0xdc, 0x0e, 0x6e, 0x01, 0x80, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }

#define CS0_CHAR_AGMS_SEND_UUID 	           { 0x24, 0xdc, 0x0e, 0x6e, 0x02, 0x80, \
												 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
												 0xb5, 0xf3, 0x93, 0xe0 }

#define CS0_CHAR_AGMS_READ_UUID 	           { 0x24, 0xdc, 0x0e, 0x6e, 0x03, 0x80, \
												 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
												 0xb5, 0xf3, 0x93, 0xe0 }




#define CS0_CHAR_BAT_CEM102_UUID               { 0x24, 0xdc, 0x0e, 0x6e, 0x02, 0x60, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS0_CHAR_WE1_CURRENT_UUID              { 0x24, 0xdc, 0x0e, 0x6e, 0x03, 0x60, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS0_CHAR_WE2_CURRENT_UUID              { 0x24, 0xdc, 0x0e, 0x6e, 0x04, 0x60, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS0_CHAR_RE_UUID                       { 0x24, 0xdc, 0x0e, 0x6e, 0x05, 0x60, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#if APP_MEASURE_ACROSS_IO_INPUTS
#define CS0_CHAR_TEMP_UUID                     { 0x24, 0xdc, 0x0e, 0x6e, 0x06, 0x60, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS0_CHAR_WE1_VOLTAGE_UUID              { 0x24, 0xdc, 0x0e, 0x6e, 0x07, 0x60, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS0_CHAR_WE2_VOLTAGE_UUID              { 0x24, 0xdc, 0x0e, 0x6e, 0x08, 0x60, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#endif
#define CS0_CHAR_CEM102_ERR_UUID               { 0x24, 0xdc, 0x0e, 0x6e, 0x09, 0x60, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#if CEM102_DIAG_FUNCTIONS
#define CS1_SVC_UUID                           { 0x24, 0xdc, 0x0e, 0x6e, 0x01, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_CURRENT_REF_RATIO_UUID        { 0x24, 0xdc, 0x0e, 0x6e, 0x02, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_WE1_CURRENT_OFFSET_UUID       { 0x24, 0xdc, 0x0e, 0x6e, 0x03, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_WE2_CURRENT_OFFSET_UUID       { 0x24, 0xdc, 0x0e, 0x6e, 0x04, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_WE1_VOLTAGE_REF_UUID          { 0x24, 0xdc, 0x0e, 0x6e, 0x05, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_WE2_VOLTAGE_REF_UUID          { 0x24, 0xdc, 0x0e, 0x6e, 0x06, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_RE_VOLTAGE_REF_UUID           { 0x24, 0xdc, 0x0e, 0x6e, 0x07, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_AT1_CURRENT_RESIDUAL_UUID     { 0x24, 0xdc, 0x0e, 0x6e, 0x08, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_AT2_CURRENT_RESIDUAL_UUID     { 0x24, 0xdc, 0x0e, 0x6e, 0x09, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_WE1_CURRENT_RESIDUAL_UUID     { 0x24, 0xdc, 0x0e, 0x6e, 0x0a, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_WE2_CURRENT_RESIDUAL_UUID     { 0x24, 0xdc, 0x0e, 0x6e, 0x0b, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_RE_CE_CURRENT_RESIDUAL_UUID   { 0x24, 0xdc, 0x0e, 0x6e, 0x0c, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_CHAR_RE_CURRENT_RESIDUAL_UUID      { 0x24, 0xdc, 0x0e, 0x6e, 0x0d, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }
#define CS1_OTP_DATA_UUID                      { 0x24, 0xdc, 0x0e, 0x6e, 0x0e, 0x70, \
                                                 0xca, 0x9e, 0xe5, 0xa9, 0xa3, 0x00, \
                                                 0xb5, 0xf3, 0x93, 0xe0 }

#endif /* CEM102_DIAG_FUNCTIONS */

#define CS0_AGMS_MAX_LENGTH                    260

#define CS0_VBAT_MAX_LENGTH                    2
#define CS0_WE1_CURRENT_MAX_LENGTH             4
#define CS0_WE2_CURRENT_MAX_LENGTH             4
#define CS0_CEM102_ERR_MAX_LENGTH              1

#if APP_MEASURE_ACROSS_IO_INPUTS
#define CS0_TEMP_MAX_LENGTH                    4
#define CS0_WE1_VOLTAGE_MAX_LENGTH             4
#define CS0_WE2_VOLTAGE_MAX_LENGTH             4
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

#if CEM102_DIAG_FUNCTIONS
#define CS1_CURRENT_REF_RATIO_MAX_LENGTH       4
#define CS1_WE1_CURRENT_OFFSET_MAX_LENGTH      4
#define CS1_WE2_CURRENT_OFFSET_MAX_LENGTH      4
#define CS1_WE1_VOLTAGE_REF_MAX_LENGTH         4
#define CS1_WE2_VOLTAGE_REF_MAX_LENGTH         4
#define CS1_RE_VOLTAGE_REF_MAX_LENGTH          4
#define CS1_AT1_CURRENT_RESIDUAL_MAX_LENGTH    4
#define CS1_AT2_CURRENT_RESIDUAL_MAX_LENGTH    4
#define CS1_WE1_CURRENT_RESIDUAL_MAX_LENGTH    4
#define CS1_WE2_CURRENT_RESIDUAL_MAX_LENGTH    4
#define CS1_RE_CE_CURRENT_RESIDUAL_MAX_LENGTH  4
#define CS1_RE_CURRENT_RESIDUAL_MAX_LENGTH     4
#define CS1_OTP_DATA_MAX_LENGTH               CEM102_OTP_DATA_SIZE
#endif /* CEM102_DIAG_FUNCTIONS */

#define CS0_AGMS_SEND_CHAR_NAME                 "AGMS_SEND_VALUE"
#define CS0_AGMS_READ_CHAR_NAME                 "AGMS_READ_VALUE"

#define CS0_VBAT_CEM102_CHAR_NAME              "CEM102 Battery (mV)"
#define CS0_WE1_CURRENT_CHAR_NAME              "WE1 Current (nA)"
#define CS0_WE2_CURRENT_CHAR_NAME              "WE2 Current (nA)"
#define CS0_CEM102_ERR_CHAR_NAME               "CEM102 Error Status"
#if 1		// add sodykim
#define CS0_RE_CHAR_NAME               		  	"Reference Electrode (nA)"
#endif



#if APP_MEASURE_ACROSS_IO_INPUTS
#define CS0_TEMP_CHAR_NAME                     "Temperature"
#define CS0_WE1_VOLT_CHAR_NAME                 "WE1 Voltage (uV)"
#define CS0_WE2_VOLT_CHAR_NAME                 "WE2 Voltage (uV)"
#endif /* CEM102_DIAG_FUNCTIONS */

#if CEM102_DIAG_FUNCTIONS
#define CS1_CURRENT_REF_RATIO_CHAR_NAME        "Current Ratio"
#define CS1_WE1_CURRENT_OFFSET_CHAR_NAME       "WE1 Current Offset (pA)"
#define CS1_WE2_CURRENT_OFFSET_CHAR_NAME       "WE2 Current Offset (pA)"
#define CS1_WE1_VOLTAGE_REF_CHAR_NAME          "WE1 Voltage Delta (uV)"
#define CS1_WE2_VOLTAGE_REF_CHAR_NAME          "WE2 Voltage Delta (uV)"
#define CS1_RE_VOLTAGE_REF_CHAR_NAME           "RE Voltage Delta (uV)"
#define CS1_AT1_CURRENT_RESIDUAL_CHAR_NAME     "AT1 Residual Current (pA)"
#define CS1_AT2_CURRENT_RESIDUAL_CHAR_NAME     "AT2 Residual Current (pA)"
#define CS1_WE1_CURRENT_RESIDUAL_CHAR_NAME     "WE1 Residual Current (pA)"
#define CS1_WE2_CURRENT_RESIDUAL_CHAR_NAME     "WE2 Residual Current (pA)"
#define CS1_RE_CE_CURRENT_RESIDUAL_CHAR_NAME   "RE-CE Residual Current (pA)"
#define CS1_RE_CURRENT_RESIDUAL_CHAR_NAME      "RE Residual Current (pA)"
#define CS1_OTP_DATA_CHAR_NAME                 "OTP Data"
#endif /* CEM102_DIAG_FUNCTIONS */

#define CS0_CEM102_ERR_VBAT_THRESHOLD_POS      0
#define CS0_CEM102_ERR_VDDA_ERROR_POS          1
#define CS0_CEM102_ERR_DIG_RST_ERROR_POS       2
#define CS0_CEM102_ERR_VCC_LOW_POS             3

#define CS0_CEM102_ERR_NO_ERROR                (uint8_t)(0x0)
#define CS0_CEM102_ERR_VBAT_THRESHOLD          (uint8_t)(1U << CS0_CEM102_ERR_VBAT_THRESHOLD_POS)
#define CS0_CEM102_ERR_VDDA_ERROR              (uint8_t)(1U << CS0_CEM102_ERR_VDDA_ERROR_POS)
#define CS0_CEM102_ERR_DIG_RST                 (uint8_t)(1U << CS0_CEM102_ERR_DIG_RST_ERROR_POS)
#define CS0_CEM102_ERR_VCC_LOW                 (uint8_t)(1U << CS0_CEM102_ERR_VCC_LOW_POS)

#define CS0_CEM102_ERR_VBAT_THRESHOLD_POS 0
#define CS0_CEM102_ERR_VDDA_ERROR_POS     1
#define CS0_CEM102_ERR_DIG_RST_ERROR_POS  2

/* Custom service ID */
/* Used in calculating attribute number for given custom service */
enum CUST_SVC_ID
{
    CUST_SVC0,
    CUST_SVC1,
};

enum CS0_att
{
    /* Service 0 */
    CS0_SERVICE,

	/* AGMS Send Characteristic in Service 0 */
    CS0_AGMS_SEND_VALUE_CHAR0,
    CS0_AGMS_SEND_VALUE_VAL0,
    CS0_AGMS_SEND_VALUE_CCC0,
    CS0_AGMS_SEND_VALUE_USR_DSCP0,

	/* AGMS Read Characteristic in Service 0 */
    CS0_AGMS_READ_VALUE_CHAR0,
    CS0_AGMS_READ_VALUE_VAL0,
    CS0_AGMS_READ_VALUE_CCC0,
    CS0_AGMS_READ_VALUE_USR_DSCP0,

    /* Reference electrode Characteristic in Service 0 */
    CS0_VBAT_CEM102_VALUE_CHAR0,
    CS0_VBAT_CEM102_VALUE_VAL0,
    CS0_VBAT_CEM102_VALUE_CCC0,
    CS0_VBAT_CEM102_VALUE_USR_DSCP0,

    /* Working electrode 1 Characteristic in Service 0 */
    CS0_WE1_CURRENT_VALUE_CHAR0,
    CS0_WE1_CURRENT_VALUE_VAL0,
    CS0_WE1_CURRENT_VALUE_CCC0,
    CS0_WE1_CURRENT_VALUE_USR_DSCP0,

    /* Working electrode 2 Characteristic in Service 0 */
    CS0_WE2_CURRENT_VALUE_CHAR0,
    CS0_WE2_CURRENT_VALUE_VAL0,
    CS0_WE2_CURRENT_VALUE_CCC0,
    CS0_WE2_CURRENT_VALUE_USR_DSCP0,

    /* CEM102 VBAT Error Characteristic in Service 1 */
    CS0_CEM102_ERR_VALUE_CHAR0,
    CS0_CEM102_ERR_VALUE_VAL0,
    CS0_CEM102_ERR_VALUE_CCC0,
    CS0_CEM102_ERR_VALUE_USR_DSCP0,

#if APP_MEASURE_ACROSS_IO_INPUTS
    /* Temperature Characteristic in Service 0 */
    CS0_TEMP_VALUE_CHAR0,
    CS0_TEMP_VALUE_VAL0,
    CS0_TEMP_VALUE_CCC0,
    CS0_TEMP_VALUE_USR_DSCP0,

    /* Working electrode 1 Voltage Characteristic in Service 0 */
    CS0_WE1_VOLTAGE_VALUE_CHAR0,
    CS0_WE1_VOLTAGE_VALUE_VAL0,
    CS0_WE1_VOLTAGE_VALUE_CCC0,
    CS0_WE1_VOLTAGE_VALUE_USR_DSCP0,

    /* Working electrode 2 Voltage Characteristic in Service 0 */
    CS0_WE2_VOLTAGE_VALUE_CHAR0,
    CS0_WE2_VOLTAGE_VALUE_VAL0,
    CS0_WE2_VOLTAGE_VALUE_CCC0,
    CS0_WE2_VOLTAGE_VALUE_USR_DSCP0,
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

    /* Max number of services and characteristics */
    CS0_NB,
};

#if CEM102_DIAG_FUNCTIONS

enum CS1_att
{
    /* Service 1 */
    CS1_SERVICE,

    /* CEM102 Diagnostic WE1 Current Reference Characteristic in Service 1 */
    CS1_CURRENT_REF_RATIO_CHAR0,
    CS1_CURRENT_REF_RATIO_VAL0,
    CS1_CURRENT_REF_RATIO_CCC0,
    CS1_CURRENT_REF_RATIO_USR_DSCP0,

    /* CEM102 Diagnostic WE1 Current Offset Characteristic in Service 1 */
    CS1_WE1_CURRENT_OFFSET_CHAR0,
    CS1_WE1_CURRENT_OFFSET_VAL0,
    CS1_WE1_CURRENT_OFFSET_CCC0,
    CS1_WE1_CURRENT_OFFSET_USR_DSCP0,

    /* CEM102 Diagnostic WE2 Current Offset Characteristic in Service 1 */
    CS1_WE2_CURRENT_OFFSET_CHAR0,
    CS1_WE2_CURRENT_OFFSET_VAL0,
    CS1_WE2_CURRENT_OFFSET_CCC0,
    CS1_WE2_CURRENT_OFFSET_USR_DSCP0,

    /* CEM102 Diagnostic WE1 Voltage Reference Characteristic in Service 1 */
    CS1_WE1_VOLTAGE_REF_CHAR0,
    CS1_WE1_VOLTAGE_REF_VAL0,
    CS1_WE1_VOLTAGE_REF_CCC0,
    CS1_WE1_VOLTAGE_REF_USR_DSCP0,

    /* CEM102 Diagnostic WE2 Voltage Reference Characteristic in Service 1 */
    CS1_WE2_VOLTAGE_REF_CHAR0,
    CS1_WE2_VOLTAGE_REF_VAL0,
    CS1_WE2_VOLTAGE_REF_CCC0,
    CS1_WE2_VOLTAGE_REF_USR_DSCP0,

    /* CEM102 Diagnostic RE Voltage Reference Characteristic in Service 1 */
    CS1_RE_VOLTAGE_REF_CHAR0,
    CS1_RE_VOLTAGE_REF_VAL0,
    CS1_RE_VOLTAGE_REF_CCC0,
    CS1_RE_VOLTAGE_REF_USR_DSCP0,

    /* CEM102 Diagnostic AT1 Residual Current Characteristic in Service 1 */
    CS1_AT1_CURRENT_RESIDUAL_CHAR0,
    CS1_AT1_CURRENT_RESIDUAL_VAL0,
    CS1_AT1_CURRENT_RESIDUAL_CCC0,
    CS1_AT1_CURRENT_RESIDUAL_USR_DSCP0,

    /* CEM102 Diagnostic WE2 Current Leakage Characteristic in Service 1 */
    CS1_AT2_CURRENT_RESIDUAL_CHAR0,
    CS1_AT2_CURRENT_RESIDUAL_VAL0,
    CS1_AT2_CURRENT_RESIDUAL_CCC0,
    CS1_AT2_CURRENT_RESIDUAL_USR_DSCP0,

    /* CEM102 Diagnostic WE1 Residual Current Characteristic in Service 1 */
    CS1_WE1_CURRENT_RESIDUAL_CHAR0,
    CS1_WE1_CURRENT_RESIDUAL_VAL0,
    CS1_WE1_CURRENT_RESIDUAL_CCC0,
    CS1_WE1_CURRENT_RESIDUAL_USR_DSCP0,

    /* CEM102 Diagnostic WE2 Residual Current Characteristic in Service 1 */
    CS1_WE2_CURRENT_RESIDUAL_CHAR0,
    CS1_WE2_CURRENT_RESIDUAL_VAL0,
    CS1_WE2_CURRENT_RESIDUAL_CCC0,
    CS1_WE2_CURRENT_RESIDUAL_USR_DSCP0,

    /* CEM102 Diagnostic RE_CE Residual Current Characteristic in Service 1 */
    CS1_RE_CE_CURRENT_RESIDUAL_CHAR0,
    CS1_RE_CE_CURRENT_RESIDUAL_VAL0,
    CS1_RE_CE_CURRENT_RESIDUAL_CCC0,
    CS1_RE_CE_CURRENT_RESIDUAL_USR_DSCP0,

    /* CEM102 Diagnostic RE Residual Current Characteristic in Service 1 */
    CS1_RE_CURRENT_RESIDUAL_CHAR0,
    CS1_RE_CURRENT_RESIDUAL_VAL0,
    CS1_RE_CURRENT_RESIDUAL_CCC0,
    CS1_RE_CURRENT_RESIDUAL_USR_DSCP0,

    /* CEM102 Diagnostic OTP Data Characteristic in Service 1 */
    CS1_OTP_DATA_CHAR0,
    CS1_OTP_DATA_VAL0,
    CS1_OTP_DATA_CCC0,
    CS1_OTP_DATA_USR_DSCP0,

    /* Max number of services and characteristics */
    CS1_NB,
};
#endif /* CEM102_DIAG_FUNCTIONS */

/* States for sending the notification over BLE */
enum
{
    APP_ATTR_SEND_FIRST = 0,
    APP_ATTR_SEND_AGMS_VALUE,
    APP_ATTR_SEND_VBAT_VALUE,
    APP_ATTR_SEND_WE1_CURRENT,
    APP_ATTR_SEND_WE2_CURRENT,
    APP_ATTR_SEND_ERROR_CODE,
#if APP_MEASURE_ACROSS_IO_INPUTS
    APP_ATTR_SEND_TEMPERATURE,
    APP_ATTR_SEND_WE1_VOLTAGE,
    APP_ATTR_SEND_WE2_VOLTAGE,
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
#if CEM102_DIAG_FUNCTIONS
    APP_ATTR_SEND_CURRENT_REF_RATIO,
    APP_ATTR_SEND_WE1_CURRENT_OFFSET,
    APP_ATTR_SEND_WE2_CURRENT_OFFSET,
    APP_ATTR_SEND_WE1_VOLTAGE_REF,
    APP_ATTR_SEND_WE2_VOLTAGE_REF,
    APP_ATTR_SEND_RE_VOLTAGE_REF,
    APP_ATTR_SEND_AT1_CURRENT_RESIDUAL,
    APP_ATTR_SEND_AT2_CURRENT_RESIDUAL,
    APP_ATTR_SEND_WE1_CURRENT_RESIDUAL,
    APP_ATTR_SEND_WE2_CURRENT_RESIDUAL,
    APP_ATTR_SEND_RE_CE_CURRENT_RESIDUAL,
    APP_ATTR_SEND_RE_CURRENT_RESIDUAL,
    APP_ATTR_SEND_OTP_DATA,
#endif /* CEM102_DIAG_FUNCTIONS */
    APP_ATTR_SEND_LAST,
};

struct app_env_tag_cs0
{
    /* To BLE agms transfer buffer */
    uint8_t agms_to_air_buffer[CS0_AGMS_MAX_LENGTH];
    uint16_t agms_to_air_cccd_value[APP_MAX_NB_CON];

    /* To BLE WE 1 transfer buffer */
    uint8_t vbat_to_air_buffer[CS0_VBAT_MAX_LENGTH];
    uint16_t vbat_to_air_cccd_value[APP_MAX_NB_CON];

    /* To BLE WE 1 transfer buffer */
    uint8_t we1_curr_to_air_buffer[CS0_WE1_CURRENT_MAX_LENGTH];
    uint16_t we1_curr_to_air_cccd_value[APP_MAX_NB_CON];

    /* To BLE WE 2 transfer buffer */
    uint8_t we2_curr_to_air_buffer[CS0_WE2_CURRENT_MAX_LENGTH];
    uint16_t we2_curr_to_air_cccd_value[APP_MAX_NB_CON];

    /* To BLE cem102 vbat error transfer buffer */
    uint8_t cem102_err_to_air_buffer[CS0_CEM102_ERR_MAX_LENGTH];
    uint16_t cem102_err_to_air_cccd_value[APP_MAX_NB_CON];

#if APP_MEASURE_ACROSS_IO_INPUTS
    /* To BLE Temperature transfer buffer */
    uint8_t temp_to_air_buffer[CS0_TEMP_MAX_LENGTH];
    uint16_t temp_to_air_cccd_value[APP_MAX_NB_CON];

    /* To BLE WE1 Voltage transfer buffer */
    uint8_t we1_volt_to_air_buffer[CS0_WE1_VOLTAGE_MAX_LENGTH];
    uint16_t we1_volt_to_air_cccd_value[APP_MAX_NB_CON];

    /* To BLE WE2 Voltage transfer buffer */
    uint8_t we2_volt_to_air_buffer[CS0_WE2_VOLTAGE_MAX_LENGTH];
    uint16_t we2_volt_to_air_cccd_value[APP_MAX_NB_CON];
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
};

struct app_env_tag_cs1
{
#if CEM102_DIAG_FUNCTIONS
    uint8_t curr_ref_ratio_to_air_buffer[CS1_CURRENT_REF_RATIO_MAX_LENGTH];
    uint16_t curr_ref_ratio_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t we1_curr_offset_to_air_buffer[CS1_WE1_CURRENT_OFFSET_MAX_LENGTH];
    uint16_t we1_curr_offset_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t we2_curr_offset_to_air_buffer[CS1_WE2_CURRENT_OFFSET_MAX_LENGTH];
    uint16_t we2_curr_offset_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t we1_volt_ref_to_air_buffer[CS1_WE1_VOLTAGE_REF_MAX_LENGTH];
    uint16_t we1_volt_ref_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t we2_volt_ref_to_air_buffer[CS1_WE2_VOLTAGE_REF_MAX_LENGTH];
    uint16_t we2_volt_ref_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t re_volt_ref_to_air_buffer[CS1_RE_VOLTAGE_REF_MAX_LENGTH];
    uint16_t re_volt_ref_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t at1_curr_residual_to_air_buffer[CS1_AT1_CURRENT_RESIDUAL_MAX_LENGTH];
    uint16_t at1_curr_residual_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t at2_curr_residual_to_air_buffer[CS1_AT2_CURRENT_RESIDUAL_MAX_LENGTH];
    uint16_t at2_curr_residual_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t we1_curr_residual_to_air_buffer[CS1_WE1_CURRENT_RESIDUAL_MAX_LENGTH];
    uint16_t we1_curr_residual_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t we2_curr_residual_to_air_buffer[CS1_WE2_CURRENT_RESIDUAL_MAX_LENGTH];
    uint16_t we2_curr_residual_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t re_ce_curr_residual_to_air_buffer[CS1_RE_CE_CURRENT_RESIDUAL_MAX_LENGTH];
    uint16_t re_ce_curr_residual_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t re_curr_residual_to_air_buffer[CS1_RE_CURRENT_RESIDUAL_MAX_LENGTH];
    uint16_t re_curr_residual_to_air_cccd_value[APP_MAX_NB_CON];

    uint8_t otp_data_to_air_buffer[CS1_OTP_DATA_MAX_LENGTH];
    uint16_t otp_data_to_air_cccd_value[APP_MAX_NB_CON];
#endif /* CEM102_DIAG_FUNCTIONS */
};

enum custom_app_msg_id
{
    CUSTOMSS_NTF_TIMEOUT = TASK_FIRST_MSG(TASK_ID_APP) + 60,
    APP_CUSS_AGMS_REQ_DATE_TIME,
    APP_CUSS_AGMS_NEXT_SEND_DATA,
	APP_CUSS_PARAM_UPDATE_REQ,
	APP_CUSS_CHRONO_A_TIMEOUT,
	APP_CUSS_AFE_WAKEUP_TIMEOUT
};

/* ----------------------------------------------------------------------------
 * Global variables and types
 * --------------------------------------------------------------------------*/

#if CEM102_DIAG_FUNCTIONS
extern CEM102_DIAG_FUNC_CHAR cem102_diag_func_char;
#endif

/* ----------------------------------------------------------------------------
 * Function prototype definitions
 * --------------------------------------------------------------------------*/

/**
 * @brief       Retrieve the custom service attribute database
 * @param[in]   att_db_cs_svc_id
 *                  Customer service ID to be retrieved.
 * @return      const struct att_db_desc*
 *                  The custom service attribute database.
 */
const struct att_db_desc * CUSTOMSS_GetDatabaseDescription(uint8_t att_db_cs_svc_id);

/**
 * @brief       Initialize custom service variables and environment
 */
void CUSTOMSS_Initialize(void);

/**
 * @brief       Start the timer. If it is already running, stop and restart it.
 */
void CUSTOMSS_StartTimer(void);


void CUSTOMSS_AFE_Wakeup_Timeout(void);

/**
 * @brief       Set the notification timer to expire immediately
 */
void CUSTOMSS_Notify_Now(void);


void Call_Custom_NTF_Timeout(uint32_t timeout);

/**
 * @brief       Configure custom service to send periodic notifications
 * @param[in]   timeout    Timeout to be expired in unites of 10ms.
 *                         If set to 0, periodic notifications are disabled.
 */
void CUSTOMSS_NotifyOnTimeout(uint32_t timeout);

/**
 * @brief       Callback handler for all events related to the custom service
 * @param[in]   msg_id    The kernel message ID number.
 * @param[in]   param     The message parameter.
 * @param[in]   dest_id   The destination task ID number.
 * @param[in]   src_id    The source task ID number
 */
void CUSTOMSS_MsgHandler(ke_msg_id_t const msg_id, void const *param,
                         ke_task_id_t const dest_id, ke_task_id_t const src_id);

/**
 * @brief       Data access callback function for CS0 characteristics
 * @param       uint8_t conidx
 *                  Connection index.
 *              uint16_t attidx
 *                  Attribute index in the user defined database.
 *              uint16_t handle
 *                  Attribute handle allocated in the Bluetooth stack.
 *              uint8_t *dest_buffer
 *                  Pointer to the destination data buffer. When this callback
 *                  is caused by a write operation, this might be a
 *                  characteristic buffer in the attribute database.
 *              const uint8_t *src_buffer
 *                  Pointer to the source data buffer. When this callback is
 *                  caused by a read operation, this might be a characteristic
 *                  buffer in the attribute database.
 *              uint16_t length
 *                  Length of data to be copied into the dest_buffer.
 *              uint16_t operation
 *                  The operation type that caused the callback. This value is
 *                  set to GATT_ReadReqInd for read operations and
 *                  GATTC_WriteReqInd for write operations.
 *              uint8_t hl_status
 *                  An error status code indicating any Bluetooth Low Energy
 *                  High Layer errors.
 *              uint8_t *cfm_msg_instr
 *                  An instruction to send or withhold a confirmation message.
 *                  This pointer is used as a return value that is updated by
 *                  the callback. A value of CFM_MSG_INSTR_TO_SEND or
 *                  CFM_MSG_INSTR_NOT_TO_SEND is used to send or withhold the
 *                  message, respectively.
 * @return      uint8_t
 *                  Returns ATT_ERR_NO_ERROR if hl_status shows no errors
 *                  (i.e. GAP_ERR_NO_ERROR), otherwise returns hl_status.
 */
uint8_t CUSSTOMSS_CS0_Callback(uint8_t conidx, uint16_t attidx,
                               uint16_t handle, uint8_t *dest_buffer, const uint8_t *src_buffer,
                               uint16_t length, uint16_t operation, uint8_t hl_status,
                               uint8_t *cfm_msg_instr);



#if CEM102_DIAG_FUNCTIONS
/**
 * @brief       Data access callback function for CS1 characteristics
 * @param       uint8_t conidx
 *                  Connection index.
 *              uint16_t attidx
 *                  Attribute index in the user defined database.
 *              uint16_t handle
 *                  Attribute handle allocated in the Bluetooth stack.
 *              uint8_t *dest_buffer
 *                  Pointer to the destination data buffer. When this callback
 *                  is caused by a write operation, this might be a
 *                  characteristic buffer in the attribute database.
 *              const uint8_t *src_buffer
 *                  Pointer to the source data buffer. When this callback is
 *                  caused by a read operation, this might be a characteristic
 *                  buffer in the attribute database.
 *              uint16_t length
 *                  Length of data to be copied into the dest_buffer.
 *              uint16_t operation
 *                  The operation type that caused the callback. This value is
 *                  set to GATT_ReadReqInd for read operations and
 *                  GATTC_WriteReqInd for write operations.
 *              uint8_t hl_status
 *                  An error status code indicating any Bluetooth Low Energy
 *                  High Layer errors.
 *              uint8_t *cfm_msg_instr
 *                  An instruction to send or withhold a confirmation message.
 *                  This pointer is used as a return value that is updated by
 *                  the callback. A value of CFM_MSG_INSTR_TO_SEND or
 *                  CFM_MSG_INSTR_NOT_TO_SEND is used to send or withhold the
 *                  message, respectively.
 * @return      uint8_t
 *                  Returns ATT_ERR_NO_ERROR if hl_status shows no errors
 *                  (i.e. GAP_ERR_NO_ERROR), otherwise returns hl_status.
 */
uint8_t CUSSTOMSS_CS1_Callback(uint8_t conidx, uint16_t attidx,
                               uint16_t handle, uint8_t *dest_buffer, const uint8_t *src_buffer,
                               uint16_t length, uint16_t operation, uint8_t hl_status,
                               uint8_t *cfm_msg_instr);
#endif


// sodykim add for BLE rx callback
uint8_t App_BLE_CUSSReadWriteRequestCallback(uint8_t conidx, uint16_t attidx,
    uint16_t handle, uint8_t *dest_buffer, const uint8_t *src_buffer,
    uint16_t length, uint16_t operation, uint8_t hl_status,
    uint8_t *cfm_msg_instr);


void set_agms_base_info(uint8_t base_info);
uint8_t get_agms_base_info(void);

void set_agms_conidx(uint8_t conidx);
uint8_t get_agms_conidx(void);

uint8_t read_custom_irq(void);
void clear_custom_irq(void);


void app_custom_send_payload_packet(void);

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif    /* BLE_CUSTOMSS_H */

