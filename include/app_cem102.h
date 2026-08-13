/**
 * @file  app_cem102.h
 * @brief CEM102 low power application initialization header file
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

#ifndef APP_CEM102_H_
#define APP_CEM102_H_

#include <hw.h>
/* Include the CEM102 support library */
#include "cem102_driver.h"

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif    /* ifdef __cplusplus */

/**
 * @defgroup DEMO_APPg Demo Application Reference
 * @brief Demo Appplication Reference
 *
 * This reference chapter presents description of the functions
 * supporting the demo application. This reference includes calling
 * parameters, returned values, and assumptions.
 * @{
 */

#define LOG_FLOAT_MARKER  "%c%ld.%04ld"
#define LOG_FLOAT_MARKER1 "%c%ld.%02ld"

#if 0
#define LOG_FLOAT(val) (int32_t)(val),                                     \
                       (int32_t)(((val > 0) ? (val) - (int32_t)(val)       \
                       : (int32_t)(val) - (val))*10000)
#else
#define LOG_FLOAT(val) (val > 0 || val <=-1) ?' ':'-',        \
                      (int32_t)(val),                                                         \
                       (int32_t)(((val > 0) ? (val) - (int32_t)(val)       \
                       : (int32_t)(val) - (val))*10000)
#endif

/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/
/* CEM102 GPIO definitions */

/* CEM102 SPI CLK GPIO */
#define CEM102_SPI_CLK_GPIO             (11)

/* CEM102 SPI CLK GPIO */
#define CEM102_SPI_CTRL_GPIO            (12)

/* CEM102 SPI CLK GPIO */
#define CEM102_SPI_IO0_GPIO             (6)

/* CEM102 SPI CLK GPIO */
#define CEM102_SPI_IO1_GPIO             (4)

/* CEM102 IRQ GPIO */
#define CEM102_IRQ_GPIO                 (1)

/* CEM102 NRESET GPIO */
#define CEM102_NRESET_GPIO              (14)

/* CEM102 VDDA DUT GPIO */
#define CEM102_VDDA_DUT_GPIO            (13)

/* CEM102 AOUT GPIO */
#define CEM102_AOUT_GPIO                (5)

#define NO_RETENTION_SLEEP_MODE         (0)
#define VDDA_ERR_RECOVERY_MODE          (1)

/* CEM102 VDDA_ERR Operations:
 * - NO_RETENTION_SLEEP_MODE
 * - VDDA_ERR_RECOVERY_MODE
 *
 * If NO_RETENTION_SLEEP_MODE is selected, it will enter
 * the no-retention sleep mode when VDDA_ERR happens.
 * If VDDA_ERR_RECOVERY_MODE is selected, the system will
 * attempt to recover from VDDA_ERR.
 */
#define CEM102_VDDA_ERR_OPERATION        NO_RETENTION_SLEEP_MODE


#define PART_IDENTIFIER             0xff

/**
 * @brief SPI status flags.
 */
typedef enum _MEASUREMENT_STATUS_FLAG
{
    MEASURE_IDLE            = 0,
    MEASURE_INITIALIZED     = 1,
    MEASURE_MEASURING       = 2,
    MEASURE_READING_RESULTS = 3,
    MEASURE_RESULTS_READY   = 4
} MEASUREMENT_STATUS_FLAG;

typedef enum
{
    CALIB_NOT_STARTED,
    CALIB_INITIALIZED,
    CALIB_REF_MEASURING,
    CALIB_REF_READING,
    CALIB_CH1_MEASURING,
    CALIB_CH1_READING,
    CALIB_CH2_MEASURING,
    CALIB_CH2_READING,
    CALIB_DONE
} Calib_state_t;

/* Length of array containing CEM102 measurement data.
 * Space for two register reads per channel, and two channels, WE1 and WE2.
 */
#define MEASUREMENT_LENGTH                  (2 * 2)
#define APP_REFERENCE_LENGTH                (2)

/* VCC Typical when .pwr_cfg = LOW_VOLTAGE */
#define VCC_TYPICAL                         (1250)

/* Length of time in ms to discharge VDDA to its trimmed value
 * of 2.3 V */
#define VDDA_DISCHARGE_TIME_MS              (200)

/* Minimum VBAT to VDDA threshold */
#define SENSOR_THRESHOLD_MIN_THRESHOLD_MIN  (75)

/* LSAD Start Delay definitions in millisecond. Actual delay is two times the
 * programmed value.
 */
#define LSAD_START_DELAY_50MS               (25)
#define LSAD_START_DELAY_6MS                (3)

/* Delay using Sys_Delay call */
#define APP_SYS_DELAY_20MS                  (SystemCoreClock / 50)
#define APP_SYS_DELAY_50MS                  (SystemCoreClock / 20)
#define APP_SYS_DELAY_100MS                 (SystemCoreClock / 10)
#define APP_SYS_DELAY_200MS                 (SystemCoreClock / 5)

/* This is the default settings. We have the option of changing this
 * to CH1_TIA_FB_RES_8M / LSAD1_GAIN_DOUBLE, if needed */
#define CH1_FEEDBACK_RESISTANCE             (CH1_TIA_FB_RES_4M)
#define CH1_LSAD_GAIN_SETTING               (LSAD1_GAIN_UNITY)

#define CH2_FEEDBACK_RESISTANCE             (CH2_TIA_FB_RES_4M)
#define CH2_LSAD_GAIN_SETTING               (LSAD2_GAIN_UNITY)

/* Wait time of 50 msec delay when ADC is configured */
#define APP_LSAD_CALIB_DELAY                (SystemCoreClock / 20)

/* With 8 msec/lsb, delay value must be multiples of 8 */
#define APP_LSAD_MEASURE_DELAY_48MS         (48)
#define APP_LSAD_MEASURE_DELAY_STEP_SZ      (8)

/* Number of sample accumulation configuration. */
#define ACCUM_SAMPLE_CNT_8                  (7)

/* For LSAD */
#define CH1_VIOLATION_LIMIT                 (7)
#define CH2_VIOLATION_LIMIT                 (7)

/* Number of WE channels */
#define MAX_WE_CHANNEL_NUM                  (2)

/* Array index adjustment */
#define INDEX_ADJ_HELPER                    (1)

/* Delay before entering no-retention sleep due to vdda error in seconds */
#define VDDA_ERR_NO_RET_SLEEP_ENTER_DELAY_S (10)

/* WE1 is used for measuring calibration current */
#define CEM102_REF_MEASUREMENT_CHAN         (WE1)

typedef enum
{
    CEM102_LSAD_STATE_SETUP,
    CEM102_LSAD_STATE_DAC_LOADED,
    CEM102_LSAD_STATE_DAC_READY,
    CEM102_LSAD_STATE_MEAS_STARTED,
    CEM102_LSAD_STATE_DATA_REQUESTED,
    CEM102_LSAD_STATE_DATA_AVAILABLE,
    CEM102_LSAD_STATE_REINIT,
} CEM102_LSAD_STATE;

enum
{
    CEM102_CALIB_CURRRENT      = (1U << 0),
    CEM102_DIAG_CAL_CURR_RATIO = (1U << 1),
    CEM102_DIAG_WE1_OFF_CURR   = (1U << 2),
    CEM102_DIAG_WE2_OFF_CURR   = (1U << 3),
    CEM102_DIAG_WE1_DAC_VOLT   = (1U << 4),
    CEM102_DIAG_WE2_DAC_VOLT   = (1U << 5),
    CEM102_DIAG_RE_DAC_VOLT    = (1U << 6),
    CEM102_DIAG_AT1_RES_CURR   = (1U << 7),
    CEM102_DIAG_AT2_RES_CURR   = (1U << 8),
    CEM102_DIAG_WE1_RES_CURR   = (1U << 9),
    CEM102_DIAG_WE2_RES_CURR   = (1U << 10),
    CEM102_DIAG_RE_RES_CURR    = (1U << 11),
    CEM102_DIAG_RE_CE_RES_CURR = (1U << 12),
};
#define CEM102_DIAG_NUM_REQUESTS        (13)

/* Following diagnostic functionalities are run by default */
#define CEM102_DIAG_REQUESTS            (CEM102_DIAG_CAL_CURR_RATIO | \
                                         CEM102_DIAG_WE1_OFF_CURR | \
                                         CEM102_DIAG_WE2_OFF_CURR | \
                                         CEM102_DIAG_WE1_DAC_VOLT | \
                                         CEM102_DIAG_WE2_DAC_VOLT | \
                                         CEM102_DIAG_RE_DAC_VOLT)

/* ---------------------------------------------------------------------------
* Function prototype definitions
* --------------------------------------------------------------------------*/
/**
 * @brief       GPIO Interrupt Handler 1
 */
extern void GPIO1_IRQHandler(void);

/**
 * @brief       DMA Channel 0 Interrupt Handler
 */
extern void DMA0_IRQHandler(void);

/**
 * @brief       DMA Channel 1 Interrupt Handler
 */
extern void DMA1_IRQHandler(void);

/**
 * @brief       Call back to handle GPIO events.
 * @param[in]   event   Event that triggered the call back.
 */
extern void GPIO_CallBack(uint32_t event);

/**
 * @brief       Call back to handle SPI events.
 * @param[in]   event   Event that triggered the call back.
 */
extern void SPI_CallBack(uint32_t event);

/*
 * @brief       Calibrate the CEM102.
 * @param[in]   cem102   Pointer to the active CEM102 driver instance.
 */
int CEM102_Calibrate(DRIVER_CEM102_t *cem102);

/**
 * @brief       Calls the sequence of functions required for initializing the CEM102
 * @param[in]   cem102   Pointer to the active CEM102 driver instance.
 */
void CEM102_Startup(DRIVER_CEM102_t *cem102);

/**
 * @brief       Checks the CEM102 status and reads CEM102 data, as well as start
 *              new measurements
 * @param[in]   cem102       Pointer to the active CEM102 driver instance.
 */
void CEM102_Operation(DRIVER_CEM102_t *cem102);

/**
 * @brief       Returns the state of the diagnostics functionality.
 * @param[in]   cem102       Pointer to the active CEM102 driver instance.
 * @return      Returns true when diagnostics is completed. Returns false
 *              otherwise. Always returns true when CEM102_DIAG_FUNCTIONS
 *              or CEM102_DIAG_REQUESTS is set to 0.
 */
bool CEM102_DiagnosticsDone(DRIVER_CEM102_t *cem102);

/**
 * @brief       When enabled, run diagnostics and saves the results. This API
 *              must be called  until it is returns true, at which state the
 *              diagnostics has completed.
 * @param[in]   cem102       Pointer to the active CEM102 driver instance.
 * @note        This function implements the core of the new, diagnostics
 *              functionality. Diagnostics is enabled when compiler option
 *              CEM102_DIAG_FUNCTIONS is set to 1. When set to 1,
 *              existing, older diagnostics API, MeasureCurrent will return
 *              an error code ERRNO_NOT_IMPLEMENTED.
 */
void CEM102_Diagnostics(DRIVER_CEM102_t *cem102);

/**
 * @brief       Returns the state of the state machine that handles LSAD events.
 * @return      Returns one of the states the state machine is in; valid states
 *              are CEM102_LSAD_STATE_SETUP, CEM102_LSAD_STATE_MEAS_STARTED,
 *              CEM102_LSAD_STATE_DATA_REQUESTED,
 *              CEM102_LSAD_STATE_DATA_AVAILABLE,
 *              CEM102_LSAD_STATE_REINIT.
 */
CEM102_LSAD_STATE CEM102_GetLSADState(void);

/**
 * @brief       Sets the state of the state machine that handles LSAD events.
 * @param[in]   state  State of the state machine that is to be set; valid
 *                     options are CEM102_LSAD_STATE_SETUP,
 *                     CEM102_LSAD_STATE_MEAS_STARTED,
 *                     CEM102_LSAD_STATE_DATA_REQUESTED,
 *                     CEM102_LSAD_STATE_DATA_AVAILABLE,
 *                     CEM102_LSAD_STATE_REINIT.
 */
void CEM102_SetLSADState(CEM102_LSAD_STATE state);


/* ----------------------------------------------------------------------------
 * Global variables and types
 * --------------------------------------------------------------------------*/
extern uint16_t measure_buf[MEASUREMENT_LENGTH];
extern const CEM102_Device *cem102_dut;
extern volatile MEASUREMENT_STATUS_FLAG measure_status;
extern DRIVER_CEM102_t Driver_CEM102;
extern DRIVER_CEM102_t *cem102;
extern volatile uint16_t cem102_vbat;
extern volatile uint32_t WE1_current;
extern volatile uint32_t WE2_current;
extern volatile int32_t WE1_voltage;
extern volatile int32_t WE2_voltage;
extern volatile uint32_t temperature;
extern volatile uint8_t cem102_err_status;
volatile extern bool cem102_low_vbat;
extern uint32_t trim_error;
extern volatile Calib_state_t calib_state;
extern CEM102_CalibrationData calib_data;
extern uint32_t calib_ref_measurement;
extern CEM102_RegSaveData saved_regs;


//Jerry
extern void CEM102_measure_update(DRIVER_CEM102_t * cem102);

extern void delay_measure_update_time(void);
extern void CEM102_calibration_update(DRIVER_CEM102_t * cem102);


/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif    /* INCLUDE_APP_CEM102_H_ */

