/**
 * @file  app_cem102.c
 * @brief CEM102 low power application initialization and operation file
 *        source file
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

#include <hw.h>
#include <app.h>
#include "app_cem102.h"
#include "app_utils.h"
#include "app_customss.h"

#include "app_max30123.h"

#if APP_MEASURE_ACROSS_IO_INPUTS
#include "math.h"
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

#define APP_EVENT_FROM_CEM102      (1U << 0)
#define APP_EVENT_THRESH_VIOLATION (1U << 1)

static uint8_t cem102_event = 0;
//============================================================================
// PUBLIC FUNCTION DEFINITIONS
//============================================================================
//#define LOG_FLOAT_MARKER "%c%d.%04d"

#define MAX_WEO_LVL_MV			 32767.0f
#define MIN_WEO_LVL_MV			-32768.0f

#define fA_T0_nA			1000.0f

#define DAC_HIGH_SETTING		1.5f				// 1500mV - 800mV = 700mV
#define DAC_LOW_SETTING			0.5f				// 500mV - 800mV = -300mV

/**
 * @brief Macro for dissecting a float number into two numbers (integer and residuum).
 */
#if 0
#define LOG_FLOAT(val) (val > 0) ?' ':'-', \
					   (int32_t)(val),     \
                       (int32_t)(((val > 0) ? (val) - (int32_t)(val)       \
                       : (int32_t)(val) - (val))*10000)
#endif


/* Globals for Chicopee measurements */
volatile MEASUREMENT_STATUS_FLAG measure_status;
uint16_t measure_buf[MEASUREMENT_LENGTH];
uint32_t calib_ref_measurement;
volatile bool cem102_low_vbat = false;

/* GPIO IRQ detected */
volatile uint32_t irq = 0;
volatile uint8_t irq_valid_flag;

volatile Calib_state_t calib_state;

volatile uint32_t vdda_err_counter = 0;
static volatile uint16_t irq_cfg = 0;

CEM102_RegSaveData saved_regs;
#if APP_MEASURE_ACROSS_IO_INPUTS
static int32_t measure_values[APP_MEASURE_VOLTAGE_TYPES];
static uint8_t voltage_id = APP_MEASURE_OFFSET_VOLTAGE;
static uint8_t voltage_measurement_age = APP_VOLTAGE_MEASUREMENT_EXPIRY;
static uint8_t attribute_under_measure = APP_MEASURING_VOLTAGE;
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

DRIVER_CEM102_t *cem102 = &Driver_CEM102;
CEM102_TrimStruct trims;

/* Calibration data to be saved as a persistent data */
CEM102_CalibrationData calib_data;



//static uint32_t interval_time_count = 0;


const CEM102_Device cem102_dut_desc =
{
    /* Resources */
    .spi        = 0,
    .dma[0]     = 0,
    .dma[1]     = 1,
    .irq_index  = 1,

    /* Pads */
    .spi_clk    = CEM102_SPI_CLK_GPIO,
    .spi_ctrl   = CEM102_SPI_CTRL_GPIO,
    .spi_io0    = CEM102_SPI_IO0_GPIO,
    .spi_io1    = CEM102_SPI_IO1_GPIO,
    .clk        = -1,
    .irq        = CEM102_IRQ_GPIO,
    .pwr_en     = CEM102_NRESET_GPIO,
    .pwr_cfg    = LOW_VOLTAGE,
    .vdda_gpio  = CEM102_VDDA_DUT_GPIO,
    .aout_gpio  = CEM102_AOUT_GPIO,
    .io0        = -1,
    .io0_type   = GPIO_MODE_INPUT,
    .io1        = -1,
    .io1_type   = GPIO_MODE_INPUT,

    /* Interface configurations */
    .gpio_cfg   = (GPIO_LPF_DISABLE | GPIO_WEAK_PULL_DOWN | GPIO_3X_DRIVE),
    .spi_prescale = SPI_PRESCALE_8,

    /* Call-back functions */
    .spi_callback = &SPI_CallBack,
    .gpio_callback = &GPIO_CallBack,
};

const CEM102_Device *cem102_dut = &cem102_dut_desc;

//Jerry
/*	add sodykim for dac reference */
typedef struct {
	uint16_t we1;
	uint16_t we2;
	uint16_t ref;

} dac_target_mv_t;

dac_target_mv_t	dac_target_mV;

volatile uint32_t cem102_wait_time = 0;

static uint16_t ch1_violation_count = 0;
static uint16_t ch2_violation_count = 0;

double we1_current = 1000.0f;
double we2_current = 1000.0f;


static int Config_SendIdleCommand(void)
{
    /* Ensure all errors and system status is cleared before starting the
     * measurement.
     */
    uint16_t idle_cmd = DIGITAL_RESET_CLR_CMD | VDDA_ERR_CLR_CMD |
                        DC_DAC_LOAD_CLR_CMD | CH1_COMPLETION_CLR_CMD |
                        CH1_VIOLATION_CLR_CMD | CH2_VIOLATION_CLR_CMD |
                        CH2_COMPLETION_CLR_CMD | BUFFER_RESET_CMD |
                        IDLE_CMD;
    return cem102->SystemCommand(cem102_dut, idle_cmd);
}

static bool CEM102_ValidStateToMeasure(uint16_t sys_status)
{
    bool status = false;

    sys_status &= SYS_STATUS_SYS_STATE_MASK;
    if ((sys_status == IDLE_STATE) || (sys_status == CONTINUOUS_STATE))
    {
        status = true;
    }
    return status;
}

static void APP_SetCEM102Event(uint8_t event)
{
    cem102_event |= event;
}

static void APP_ClearCEM102Event(uint8_t event)
{
    cem102_event &= ~event;
}

static bool APP_CEM102EventValid(uint8_t event)
{
    return (cem102_event & event) ? true : false;
}

#if APP_MEASURE_ACROSS_IO_INPUTS
static bool APP_MeasuringVoltage(void)
{
    return (attribute_under_measure == APP_MEASURING_VOLTAGE) ? true : false;
}
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

static bool APP_MeasuringCurrent(void)
{
#if APP_MEASURE_ACROSS_IO_INPUTS
    return (attribute_under_measure == APP_MEASURING_CURRENT) ? true : false;
#else
    return true;
#endif
}

static bool CEM102_ThreshViolationEnabled(void)
{
    bool status = false;

    if ((CEM102_THRESH_VIOL_ENABLED == cem102_threshold_violation) &&
        (calib_state == CALIB_DONE))
    {
        status = true;
    }
    return status;
}

/* Measurement can start after some delay, when SINGLE_DELAY_CMD is used. */
static void CEM102_SetMeasurementDelayTimer(DRIVER_CEM102_t *p_cem102,
                                            uint16_t msec)
{
    msec /= APP_LSAD_MEASURE_DELAY_STEP_SZ;
    if (msec != 0)
    {
        uint16_t val = msec <<
                       SYSCTRL_AUX_CFG_SINGLE_DELAY_TIMER_POS;
        uint16_t mask = SYSCTRL_AUX_CFG_SINGLE_DELAY_TIMER_MASK;
        p_cem102->RegisterReadModifyWrite(cem102_dut, SYSCTRL_AUX_CFG, val,
                                          mask);
    }
}

/* Until we reach measurement state, we use SINGLE_CMD */
static uint16_t CEM102_GetMeasurementCmd(DRIVER_CEM102_t *p_cem102)
{
    uint16_t cmd = CONTINUOUS_CMD;

    if (calib_state != CALIB_DONE ||
#if APP_MEASURE_ACROSS_IO_INPUTS
         APP_MeasuringVoltage() ||
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
        !CEM102_DiagnosticsDone(p_cem102))
    {
        cmd = SINGLE_DELAY_CMD;
    }
    return cmd;
}

static void APP_SystemCommand(DRIVER_CEM102_t *p_cem102)
{
    uint16_t cmd = CEM102_GetMeasurementCmd(cem102);
    cmd |= SYSCTRL_CMD_ALL_ERR_MASK;
    p_cem102->SystemCommand(cem102_dut, cmd);
}

static void VDDA_ERR_Handler(void)
{
#ifdef SWMTRACE_OUTPUT
    static uint32_t vdda_err_counter = 0;
#endif /* SWMTRACE_OUTPUT */

    /* Set VDDA_ERR alarm code to be sent to the central */
    cem102_err_status |= CS0_CEM102_ERR_VDDA_ERROR;

    /* Increase VDDA_ERR Counter and set VDDA_ERR_FLAG */
#ifdef SWMTRACE_OUTPUT
    vdda_err_counter++;
    swmLogInfo("ERROR: VDDA_ERR Count: %d\r\n", vdda_err_counter);
#endif /* SWMTRACE_OUTPUT */

    CUSTOMSS_Notify_Now();

#if (CEM102_VDDA_ERR_OPERATION == NO_RETENTION_SLEEP_MODE)

    int counter = 0;

    while (counter < VDDA_ERR_NO_RET_SLEEP_ENTER_DELAY_S)
    {
        BLE_Kernel_Process();
        Sys_Delay(SystemCoreClock);
        counter++;
    }

#ifdef SWMTRACE_OUTPUT
    swmLogInfo("Entering no-retention sleep\r\n");
#endif /* SWMTRACE_OUTPUT */

    /* Go to No Retention Sleep */
    Switch_NoRetSleep_Mode();
#else
    int result = ERRNO_NO_ERROR;

    if (irq_valid_flag == 0)
    {
        /* Save current IRQ configuration */
        result = cem102->RegisterRead(cem102_dut, SYSCTRL_IRQ_CFG, &irq_cfg);
        if (result == ERRNO_NO_ERROR)
        {
            irq_valid_flag = 1;

            /* Disable VDDA_ERR IRQ */
            result = cem102->IRQConfig(cem102_dut, irq_cfg &
                                       (~VDDA_ERR_IRQ_ENABLE));
        }
    }

    /* If we are in MEASURE_INITIALIZED state, it means we reached
     * here after the calibration or due to a call to CEM102_Startup.
     * Either way, we are not in measuring state.
     */
    if (measure_status != MEASURE_INITIALIZED)
    {
        measure_status = MEASURE_MEASURING;
    }

    /* Clear the VDDA_ERR and proceed to a new measurement */
    APP_SystemCommand(cem102);
#endif /* (CEM102_VDDA_ERR_OPERATION == NO_RETENTION_SLEEP_MODE) */
}

void GPIO1_IRQHandler(void)
{
    /* NVIC may be active, when debugger is connected */
    irq = 1;
}

void DMA0_IRQHandler(void)
{
    cem102->TXHandler(cem102_dut);
}

void DMA1_IRQHandler(void)
{
    cem102->RXHandler(cem102_dut);
}

void GPIO_CallBack(uint32_t event)
{

}

void SPI_CallBack(uint32_t event)
{
    /* If we were reading results, copy the SPI data to the measurement buffer
     * and then mark them as ready for processing */
    if (measure_status == MEASURE_READING_RESULTS)
    {
        /* During calibration, RegisterMultipleRead API is used that already
         * reads the data off SPI rx buffer. Call to RegisterBufferRead is
         * needed only when QueueRead is used, that is when CEM102_Operation
         * is in use.
         */
        if (calib_state == CALIB_DONE)
        {
            cem102->RegisterBufferRead(cem102_dut, measure_buf,
                                       MEASUREMENT_LENGTH);
        }

        measure_status = MEASURE_RESULTS_READY;

        if (calib_state == CALIB_REF_MEASURING)
        {
            calib_state = CALIB_REF_READING;
        }
        else if (calib_state == CALIB_CH1_MEASURING)
        {
            calib_state = CALIB_CH1_READING;
        }
        else if (calib_state == CALIB_CH2_MEASURING)
        {
            calib_state = CALIB_CH2_READING;
        }
    }
#if CEM102_DIAG_FUNCTIONS
    if (CEM102_GetLSADState() == CEM102_LSAD_STATE_DATA_REQUESTED)
    {
        CEM102_SetLSADState(CEM102_LSAD_STATE_DATA_AVAILABLE);
        APP_SetCEM102Event(APP_EVENT_FROM_CEM102);
    }
#endif /* CEM102_DIAG_FUNCTIONS */
}

static void Prepare_ThresholdCfg(const CEM102_DAC_TYPE dac,
                                 uint16_t *p_threshold, uint16_t sample_cnt)
{
    uint32_t index = dac - 1;

    /* Channel gain must be non-zero when calibration is done */
    if (channel_gain[index] != 0.0)
    {
        float limit[2] = { CH1_TARGET_VIOLATION_CURRENT,
                           CH2_TARGET_VIOLATION_CURRENT
                         };
        uint16_t threshold = (int)((limit[index] / channel_gain[index]) /
                              (sample_cnt + 1))
                               >> CEM102_LSAD_THRESHOLD_PADDING;

        /* Value of mask and shift position are is same for WE1 and WE2 */
        p_threshold[index] = (threshold <<
                             CHCFG_CH1_THRESHOLD_CH1_THRESHOLD_POS) &
                             CHCFG_CH1_THRESHOLD_CH1_THRESHOLD_MASK;

        uint16_t enable[2] = { APP_LSAD_VIOLATION_MODE,
                               APP_LSAD_VIOLATION_MODE
                             };
        if (CEM102_ThreshViolationEnabled())
        {
            if (APP_MeasuringCurrent() && CEM102_DiagnosticsDone(cem102))
            {
                p_threshold[index] |= enable[index];
            }
        }
    }
    else
    {
        p_threshold[index] = 0;
    }
}

/* Sets CEM102 to idle mode, if it takes more than 100 msec to go IDLE */
static void APP_CEM102_WaitForIdle(void)
{
    uint8_t counter = 0;
    bool state_idle = false;

    do
    {
        uint16_t st = 0;

        cem102->RegisterRead(cem102_dut, SYS_STATUS, &st);
        st &= SYS_STATUS_SYS_STATE_MASK;

        /* Either in IDLE or not in a valid state to expect IDLE */
        if (st == IDLE_STATE || (st != SINGLE_STATE &&
            st != SINGLE_DELAY_STATE))
        {
            state_idle = true;
        }
        else
        {
            Sys_Delay(APP_SYS_DELAY_20MS);
            counter++;
        }
        /* Set the chip in IDLE mode, if we wait for more than 50 msec */
        if (counter == 5)
        {
            Config_SendIdleCommand();
        }
    } while (!state_idle);
#ifdef SWMTRACE_OUTPUT
    if (counter > 0)
    {
        swmLogInfo("Set to IDLE after waiting for %d msecs\r\n", counter * 20);
    }
#endif /* SWMTRACE_OUTPUT */
}

static void APP_ReadMeasurement(DRIVER_CEM102_t *p_cem102, CEM102_DAC_TYPE dac)
{
    unsigned int len[CEM102_MAX_DACS] =
    {
        MEASUREMENT_LENGTH,
        MEASUREMENT_LENGTH,
        APP_REFERENCE_LENGTH
    };
    uint16_t status = 0;
    uint16_t status_exp[CEM102_MAX_DACS] =
    {
        CH1_COMPLETION,
        CH2_COMPLETION,
        CH1_COMPLETION
    };

    uint8_t index = (uint8_t)dac - INDEX_ADJ_HELPER;

    p_cem102->RegisterRead(cem102_dut, SYS_STATUS, &status);

    /* If the measurement has completed, and in IDLE state,
     * time to queue it for reading */
    if (((status & status_exp[index]) != 0) &&
        ((status & SYS_STATUS_SYS_STATE_MASK) == IDLE_STATE))
    {
        measure_status = MEASURE_READING_RESULTS;
        p_cem102->RegisterMultipleRead(cem102_dut, SYS_BUFFER_WORD0,
                                       measure_buf, len[index]);
    }
}

static int DAC_GetCalibratedSetting(CEM102_DAC_TYPE dac, uint16_t target_mv,
                                    uint16_t *p_dac)
{
    int result = ERRNO_NO_ERROR;

    if ((dac != WE1) && (dac != WE2) && (dac != RE))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        uint16_t dac_setting = 0;
        result = cem102->CalculateDACAmplitude(cem102_dut, dac, target_mv,
                                               &dac_setting);

        /* OTP v3 parts will cause CalculateDACAmplitude to return error. */
        if (result == ERRNO_GENERAL_FAILURE)
        {
            result = cem102->CalculateDACAmplitudeADC(cem102_dut, dac,
                                                      target_mv, &dac_setting);
        }

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("DAC-%d: CalculateDACAmplitude %d mV returns 0x%x\r\n", dac,
                   target_mv, dac_setting);
#endif
        *p_dac = dac_setting;
        APP_CEM102_WaitForIdle();
    }
    return result;
}

static int Config_PostCalibration(void)
{
    int result = ERRNO_NO_ERROR;
    uint16_t dac_setting = 0;

    uint16_t tia_en[2] = { CH1_TIA_ENABLE, CH2_TIA_ENABLE };
    uint16_t tia_cfg[2] = { CH1_TIA_INT_FB_ENABLE | CH1_FEEDBACK_RESISTANCE,
                            CH2_TIA_INT_FB_ENABLE | CH2_FEEDBACK_RESISTANCE
                          };
    uint16_t dac_mask[2] = { DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK,
                             DC_DAC_WE2_CTRL_DAC_WE2_VALUE_MASK
                           };
    uint16_t dac_en[2] = { DAC_WE1_ENABLE, DAC_WE2_ENABLE };
    uint16_t chan_cfg[2] = { CEM102_CH1_BIAS_PRESCALE2 |
                             CEM102_CH1_LPF_CUTOFF_500HZ,
                             CEM102_CH2_BIAS_PRESCALE2 |
                             CEM102_CH2_LPF_CUTOFF_500HZ
                           };
    uint16_t buf_cfg[2] = { CH1_BUF_ENABLE, CH2_BUF_ENABLE };
    uint16_t sw_cfg[2] = { CH1_SC119_SC120_CONNECT | CH1_SC112_CONNECT,
                           CH2_SC219_SC220_CONNECT | CH2_SC212_CONNECT,
                         };
                

    /* By default, we use CH1_TIA_FB_RES_4M and LSAD1_GAIN_UNITY. This can be
     * changed to use CH1_TIA_FB_RES_8M + LSAD1_GAIN_DOUBLE, if needed */
    uint16_t lsad_cfg[2] = { CH1_LSAD_GAIN_SETTING | APP_ORSD_USER_SETTING | LSAD1_ITRIM_1p00,
                             CH2_LSAD_GAIN_SETTING | APP_ORSD_USER_SETTING | LSAD2_ITRIM_1p00
                           };
    uint16_t buff_cfg[2] = { CH1_DOUBLE | (CH1_VIOLATION_LIMIT << CHCFG_CH1_BUFFER_CH1_VIOLATION_LIMIT_POS),
                             CH2_DOUBLE | (CH2_VIOLATION_LIMIT << CHCFG_CH2_BUFFER_CH2_VIOLATION_LIMIT_POS)
                           };
    uint16_t sample_cnt = ACCUM_SAMPLE_CNT_8;
    uint16_t accum_cfg[2] = { (CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_MASK & sample_cnt) | CH1_MEASUREMENT_ENABLE,
                              (CHCFG_CH2_ACCUM_CH2_ACCUM_SAMPLE_CNT_MASK & sample_cnt) | CH2_MEASUREMENT_ENABLE
                            };

    for (uint32_t index = 0; index < MAX_WE_CHANNEL_NUM; index++) {
        int dac = index + 1;
        if (index == 0)  {
            dac_setting = calib_data.dac_setting[WE1 - INDEX_ADJ_HELPER];
        }
        else {
            dac_setting = calib_data.dac_setting[WE2 - INDEX_ADJ_HELPER];
        }

        result |= cem102->TIAConfig(cem102_dut, dac, tia_en[index], tia_cfg[index]);
        result |= cem102->WEDACConfig(cem102_dut, dac,
                                      ((dac_mask[index] & dac_setting) |
                                      dac_en[index]), DC_DAC_LOAD_IRQ_DISABLE);
        result |= cem102->BufferConfig(cem102_dut, dac, chan_cfg[index],
                                       buf_cfg[index]);
        result |= cem102->SwitchConfig(cem102_dut, dac, sw_cfg[index]);
        result |= cem102->ClockConfig(cem102_dut, LSAD_PRESCALE2,
                                      LSAD_START_DELAY_50MS);
        uint16_t thresh_cfg[2] = { 0, 0 };
        Prepare_ThresholdCfg(dac, thresh_cfg, sample_cnt);

        cem102->GetLSADConfig(cem102_dut, &lsad_cfg[index], NULL);
        result |= cem102->ADCConfig(cem102_dut, dac, lsad_cfg[index],
                                    thresh_cfg[index], buff_cfg[index],
                                    accum_cfg[index]);

         /* Recalculate ADC gain, after setting sample accumuation count */
         cem102->CalculateADCGain(dac, (uint32_t)sample_cnt + 1);
    }

    /* Restore ANA_CFG2 configuration. */
    result |= cem102->RegisterReadModifyWrite(cem102_dut, ANA_CFG2,
                                              SENSOR_CAL_DISABLE,
                                              ANA_CFG2_SENSOR_CAL_CFG_MASK);

    /* RE channel enable */
    uint16_t re_dac = (uint16_t)calib_data.dac_setting[RE - INDEX_ADJ_HELPER];
    result |= cem102->REDACConfig(cem102_dut,
                                  ((DC_DAC_RE_CTRL_DAC_RE_VALUE_MASK &
                                  re_dac) | DAC_RE_ENABLE),
								  RE_SR1_DISCONNECT, RE_BUF_ENABLE);			// RE_SR1_CONNECT --> RE_SR1_DISCONNECT로 변경..25.07.16

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("Config_PostCalibration : RE %d\r\n", re_dac);
#endif


    result |= Config_SendIdleCommand();
    return result;
}

static int Config_ForMeasuring(CEM102_DAC_TYPE dac, uint16_t target_mv)
{
    int result = ERRNO_NO_ERROR;

    if (dac != WE1 && dac != WE2)
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        /* DAC RE and RE buf disable */
        result = cem102->REDACConfig(cem102_dut, 0, 0, 0);

        uint32_t index = dac - 1;
        uint16_t dac_setting = 0;
        if (result == ERRNO_NO_ERROR)
        {
            result = DAC_GetCalibratedSetting(dac, target_mv,
                                              &dac_setting);
        }

        uint16_t tia_en[2] = { CH1_TIA_ENABLE, CH2_TIA_ENABLE };
        uint16_t tia_cfg[2] = { CH1_TIA_INT_FB_ENABLE | CH1_FEEDBACK_RESISTANCE,
                                CH2_TIA_INT_FB_ENABLE | CH2_FEEDBACK_RESISTANCE
                              };
        result |= cem102->TIAConfig(cem102_dut, dac, tia_en[index],
                                    tia_cfg[index]);

        /* Configure the CEM102 device for a basic measurement */
        uint16_t dac_mask[2] = { DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK,
                                 DC_DAC_WE2_CTRL_DAC_WE2_VALUE_MASK
                               };
        uint16_t dac_en[2] = { DAC_WE1_ENABLE, DAC_WE2_ENABLE };

        result |= cem102->WEDACConfig(cem102_dut, dac,
                                      ((dac_mask[index] & dac_setting) |
                                      dac_en[index]), 0);

        uint16_t chan_cfg[2] = { CEM102_CH1_BIAS_PRESCALE2 | CEM102_CH1_LPF_CUTOFF_500HZ,
                                 CEM102_CH2_BIAS_PRESCALE2 | CEM102_CH2_LPF_CUTOFF_500HZ
                               };
        uint16_t buf_cfg[2] = { CH1_BUF_ENABLE, CH2_BUF_ENABLE };
        result |= cem102->BufferConfig(cem102_dut, dac, chan_cfg[index],
                                       buf_cfg[index]);

        uint16_t conn_cfg[2] = { CH1_SC119_SC120_CONNECT | CH1_SC112_CONNECT,
                                 CH2_SC219_SC220_CONNECT | CH2_SC212_CONNECT
                               };
        result |= cem102->SwitchConfig(cem102_dut, dac, conn_cfg[index]);

        /* Prescale setting must match the bias setting in chan_cfg above */
        result |= cem102->ClockConfig(cem102_dut, LSAD_PRESCALE2,
                                      LSAD_START_DELAY_50MS);

        uint16_t lsad_cfg[2] = { CH1_LSAD_GAIN_SETTING | LSAD1_SYS_CHOP_ENABLE |
                                 APP_LSAD_NUM_INT_CYCLE |  LSAD1_ITRIM_1p00,
                                 CH2_LSAD_GAIN_SETTING | LSAD2_SYS_CHOP_ENABLE |
                                 APP_LSAD_NUM_INT_CYCLE | LSAD2_ITRIM_1p00
                               };
        uint16_t buff_cfg[2] = { CH1_DOUBLE | (CH1_VIOLATION_LIMIT <<
                                 CHCFG_CH1_BUFFER_CH1_VIOLATION_LIMIT_POS),
                                 CH2_DOUBLE | (CH2_VIOLATION_LIMIT <<
                                 CHCFG_CH2_BUFFER_CH2_VIOLATION_LIMIT_POS)
                               };
        uint16_t sample_cnt = ACCUM_SAMPLE_CNT_8;
        uint16_t accum_cfg[2] = { (CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_MASK &
                                   sample_cnt) | CH1_MEASUREMENT_ENABLE,
                                  (CHCFG_CH2_ACCUM_CH2_ACCUM_SAMPLE_CNT_MASK &
                                   sample_cnt) | CH2_MEASUREMENT_ENABLE
                                };
        uint16_t thresh_cfg[2] = { 0, 0 };
        Prepare_ThresholdCfg(dac, thresh_cfg, sample_cnt);

        /* Get the config for the ORSD, system chopper state we want */
        cem102->GetLSADConfig(cem102_dut, &lsad_cfg[index], NULL);
        result |= cem102->ADCConfig(cem102_dut, dac, lsad_cfg[index],
                                    thresh_cfg[index], buff_cfg[index],
                                    accum_cfg[index]);

        /* Recalculate ADC gain, after setting sample accumuation count */
        cem102->CalculateADCGain(dac, (uint32_t)sample_cnt + 1);


        /* RE channel enable */
        uint16_t re_dac = (uint16_t)calib_data.dac_setting[RE - INDEX_ADJ_HELPER];
        result |= cem102->REDACConfig(cem102_dut,
                                      ((DC_DAC_RE_CTRL_DAC_RE_VALUE_MASK &
                                      re_dac) | DAC_RE_ENABLE),
									  RE_SR1_DISCONNECT, RE_BUF_ENABLE);			// RE_SR1_CONNECT --> RE_SR1_DISCONNECT로 변경..25.07.16

        result |= Config_SendIdleCommand();
        /* Extra settling time before measuring */
        Sys_Delay(APP_SYS_DELAY_200MS);
    }
    return result;
}

static int Config_PostMeasurement(CEM102_DAC_TYPE dac)
{
    int result = ERRNO_NO_ERROR;

    if (dac != WE1 && dac != WE2)
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        /* Re-configure internal switches to connect the ADC to the channel. */
        uint32_t index = dac - 1;
        uint16_t sw_cfg[2] = { CH1_SC119_SC120_CONNECT | CH1_SC112_CONNECT,
                               CH2_SC219_SC220_CONNECT | CH2_SC212_CONNECT,
                             };
        result |= cem102->SwitchConfig(cem102_dut, dac, sw_cfg[index]);

        /* Restore ANA_CFG2 configuration. */
        result |= cem102->RegisterReadModifyWrite(cem102_dut, ANA_CFG2,
                                                  SENSOR_CAL_DISABLE,
												  SENSOR_CAL_ENABLE);	//ANA_CFG2_SENSOR_CAL_CFG_MASK);

        result |= Config_SendIdleCommand();
        Sys_Delay(APP_SYS_DELAY_20MS);
    }
    return result;
}

#if APP_MEASURE_ACROSS_IO_INPUTS
static int APP_SetupATBusSwitch(uint8_t request)
{
    /* Default setting is for the APP_MEASURE_RESISTOR_VOLTAGE request */
    uint16_t cfg = CEM102_ANALOG_IO_ATBUS_CONNECT |
                   CEM102_ANALOG_IO2_VSSA_CONNECT;

    int result = cem102->BufferConfig(cem102_dut, WE2,
                                      CEM102_CH2_LPF_CUTOFF_10KHZ,
                                      CH2_BUF_ENABLE | CH2_BUF_BYPASS_ENABLE);
    result |= cem102->SwitchConfig(cem102_dut, WE2,
                                   CH2_SC209_SC210_DISCONNECT |
                                   CH2_SC217_SC218_CONNECT |
                                   CH2_SC212_CONNECT
                                  );
    uint16_t sample_cnt = CEM102_ACCUM_SAMPLE_CNT_2;
    uint16_t lsad_cfg = LSAD2_GAIN_UNITY | LSAD2_SYS_CHOP_DISABLE |
                        LSAD2_ORSD64_RSD14 | LSAD2_ITRIM_1p00;
    uint16_t accum_cfg = (CHCFG_CH2_ACCUM_CH2_ACCUM_SAMPLE_CNT_MASK &
                          sample_cnt) | CH2_MEASUREMENT_ENABLE;
    uint16_t buff_cfg = CH2_DOUBLE | (CH2_VIOLATION_LIMIT <<
                        CHCFG_CH2_BUFFER_CH2_VIOLATION_LIMIT_POS);

    uint16_t thresh_cfg[2] = { 0, 0 };

    Prepare_ThresholdCfg(WE2, thresh_cfg, sample_cnt);

    /* Get the config for the ORSD, system chopper state we want */
    cem102->GetLSADConfig(cem102_dut, &lsad_cfg, NULL);
    result |= cem102->ADCConfig(cem102_dut, WE2, lsad_cfg, thresh_cfg[1],
                                buff_cfg, accum_cfg);

    /* Recalculate ADC gain, after changing sample count */
    cem102->CalculateADCGain(WE2, (uint32_t)sample_cnt + 1);

    if (request == APP_MEASURE_VDDA_VOLTAGE)
    {
        cfg |= CEM102_ANALOG_IO1_VDDA_CONNECT;
    }
    else if (request == APP_MEASURE_OFFSET_VOLTAGE)
    {
        cfg |= CEM102_ANALOG_IO1_VSSA_CONNECT;
    }
    result |= cem102->RE_ATBusSwitchConfig(cem102_dut, RE, cfg);
    return result;
}

static void APP_CalculateTemperature(void)
{
    /* Subtract the offset voltage, to improve accuracy */
    float v_vdda = (float)measure_values[APP_MEASURE_VDDA_VOLTAGE] -
                   (float)measure_values[APP_MEASURE_OFFSET_VOLTAGE];
    float v_res = (float)measure_values[APP_MEASURE_RESISTOR_VOLTAGE] -
                   (float)measure_values[APP_MEASURE_OFFSET_VOLTAGE];

    /* Add voltage across two switches, ST12 and ST22. */
    v_vdda += (2 * v_vdda * CEM102_ST_SWITCH_RESISTANCE) /
               (1000 * APP_RESISTOR_IMPEDENCE);
    if (v_res != 0)
    {
        /* In Fahrenheit scale */
        float temperature_measured = 0.0;
        float resistance = APP_RESISTOR_IMPEDENCE *
                            ((v_vdda / v_res) - 1);

        /* To calculate temperature from resistance, we use simplified
         * B-parameter equation, derived from Steinhart-Hart equation
         *
         *   1/T = (1/T0) + (1/B) * log(R/R0) simplified as
         *   T = (B * T0) / (B + (T0 * log(R/R0)))
         */
        temperature_measured = APP_THERMISTOR_B_CONSTANT * APP_T0_VALUE;
        temperature_measured /= (APP_THERMISTOR_B_CONSTANT + (APP_T0_VALUE *
                                 log(resistance / APP_RESISTANCE_AT_25_DEG_C)));
        temperature_measured -= APP_CELCIUS_CONVERSION_FACTOR;
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("Resistance %f kilo ohms, Temperature = %f deg C\r\n",
                    resistance, temperature_measured);
#endif
        temperature = App_Utils_ConvertFloatToUInt32(temperature_measured);
    }
}
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

static void CEM102_SetThreshViolDetection(int ch1_enable, int ch2_enable)
{
    if (CEM102_ThreshViolationEnabled())
    {
        uint16_t ch1_val = CH1_VIOLATION_CHECK_DISABLE;
        uint16_t ch2_val = CH2_VIOLATION_CHECK_DISABLE;
        uint16_t ch1_mask = CHCFG_CH1_THRESHOLD_CH1_VIOLATION_CFG_MASK;
        uint16_t ch2_mask = CHCFG_CH2_THRESHOLD_CH2_VIOLATION_CFG_MASK;

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("CEM102_SetThreshViolDetection : %d  %d \r\n", ch1_enable, ch2_enable);
#endif


        if (ch1_enable)
        {
            ch1_val = APP_LSAD_VIOLATION_MODE;
        }
        if (ch2_enable)
        {
            ch2_val = APP_LSAD_VIOLATION_MODE;
        }
        cem102->RegisterReadModifyWrite(cem102_dut, CHCFG_CH1_THRESHOLD,
                                        ch1_val, ch1_mask);
        cem102->RegisterReadModifyWrite(cem102_dut, CHCFG_CH2_THRESHOLD,
                                        ch2_val, ch2_mask);
    }
}

static void CEM102_SetCalibDone(void)
{
    calib_state = CALIB_DONE;
    measure_status = MEASURE_INITIALIZED;
    CEM102_SetThreshViolDetection(false, false);
}

static void CEM102_Restart(DRIVER_CEM102_t *cem102, bool restart)
{
    /* Local variable definitions */
    int result = ERRNO_NO_ERROR;

#if 0
    uint16_t fam = 0;
#endif


#ifdef SWMTRACE_OUTPUT
//    uint16_t rev = 0;
    uint32_t repeat_count = 0;
#endif

    do
    {
        SYS_WATCHDOG_REFRESH();

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("Restart counter:%d\r\n", repeat_count);
        repeat_count++;
#endif /* SWMTRACE_OUTPUT */

        /* Wait 20ms after putting CEM102 to RESET */
        Sys_GPIO_Set_Low(cem102_dut->pwr_en);
        Sys_Delay(APP_SYS_DELAY_20MS);

        /* Initialize the CEM102 device */
        result = cem102->Initialize(cem102_dut);
        cem102->VDDCCharge(cem102_dut);
        result |= cem102->PowerReset(cem102_dut);

        if (result == ERRNO_VCC_LOW_ERROR)
        {
            cem102_err_status |= CS0_CEM102_ERR_VCC_LOW;
        }

        result |= cem102->PowerInit(cem102_dut);
#if 1
        // MAX30132 POWER On READY
        max30123_power_on_ready();

        swmLogInfo("max30123_reg_initial\r\n");
        max30123_reg_initial();

#else
        if (result == ERRNO_NO_ERROR)
        {
            uint16_t temp = 0;

            Config_SendIdleCommand();

            cem102->RegisterRead(cem102_dut, SYS_STATUS, &temp);
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("\rSYS_STATUS(%X)\r\n", temp);
#endif /* SWMTRACE_OUTPUT */

            /* Check for VDDA error or Digital Reset */
            if ((temp & (VDDA_ERR|DIGITAL_RESET)) != 0)
            {
                result = ERRNO_GENERAL_FAILURE;
            }
        }

        if (result == ERRNO_NO_ERROR)
        {
            /* Read the chip family information */
            result = cem102->RegisterRead(cem102_dut, CHIP_FAMILY_VERSION, &fam);

            if ((result != ERRNO_NO_ERROR) || (0 == fam))
            {
                result |= ERRNO_GENERAL_FAILURE;
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("\r\nFailed to read chip version\r\n");
#endif /* SWMTRACE_OUTPUT */
            }
            else
            {
#ifdef SWMTRACE_OUTPUT
                result |= cem102->RegisterRead(cem102_dut, CHIP_REVISION, &rev);
                swmLogInfo("- Chip Revision: %X (%X)\r\n", fam, rev);
#endif /* SWMTRACE_OUTPUT */
            }
        }

        if (result == ERRNO_NO_ERROR)
        {
            /* Read OTP data */
            result = cem102->OTPRead(cem102_dut, &trims, INTERNAL_RC_CLK_FREQUENCY,
                                     OTP_READ_MAX_ATTEMPTS);

            if (result == ERRNO_NO_ERROR)
            {
#if CEM102_DIAG_FUNCTIONS
                memcpy(cem102_diag_func_char.otp_data, &trims,
                       sizeof(cem102_diag_func_char.otp_data));
#endif /* CEM102_DIAG_FUNCTIONS */
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("- OTP Read Successful. Version %d\r\n",
                           CEM102_OTP_GetVersion());
#endif
            }
            else if (result == ERRNO_ECC_CORRECTED)
            {
#if CEM102_DIAG_FUNCTIONS
                memcpy(cem102_diag_func_char.otp_data, &trims,
                       sizeof(cem102_diag_func_char.otp_data));
#endif /* CEM102_DIAG_FUNCTIONS */
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("- OTP Read Corrected");
#endif
            }
            else if (result == ERRNO_TIMEOUT)
            {
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("- OTP Read timed out. Reinitializing CEM102\r\n");
#endif
            }
            else
            {
#ifdef SWMTRACE_OUTPUT
                /* As the OTP data is unavailable, load the default trim settings */
                swmLogInfo("- OTP Read Unsuccessful(%d). Reinitializing CEM102\r\n",
                           result);
#endif
            }
        }

#endif

        if (result == ERRNO_NO_ERROR)
        {
            /* Load Device Trim */
            result |= cem102->DeviceTrim(cem102_dut, &trims);
        }
    } while (result != ERRNO_NO_ERROR);


#ifdef SWMTRACE_OUTPUT
    swmLogInfo("Measurement has been configured; enabling calibration.\r\n");
#endif /* SWMTRACE_OUTPUT */

    /* Enable CH1 & CH2 Completion IRQ */
    uint16_t temp = 0;
    result = cem102->RegisterRead(cem102_dut, SYSCTRL_IRQ_CFG, &temp);
    if (result == ERRNO_NO_ERROR)
    {
        temp |= (CH1_COMPLETION_IRQ_ENABLE |
                 CH2_COMPLETION_IRQ_ENABLE);
        cem102->IRQConfig(cem102_dut, temp);
    }

    /* Start calibration */
    calib_state = CALIB_INITIALIZED;
    measure_status = MEASURE_INITIALIZED;

#if CEM102_DIAG_FUNCTIONS
    /* This will cause the diagnostics module to re-initialize */
    CEM102_SetLSADState(CEM102_LSAD_STATE_REINIT);
#endif /* CEM102_DIAG_FUNCTIONS */
}

void CEM102_Startup(DRIVER_CEM102_t *p_cem102)
{
    /* Put 100ms Delay for the voltage to settle*/
    Sys_Delay(APP_SYS_DELAY_100MS);

#ifdef SWMTRACE_OUTPUT
    swmLogInfo("\rCEM102_Startup\r\n");
#endif /* SWMTRACE_OUTPUT */

    CEM102_Restart(p_cem102, false);
}

int CEM102_Calibrate(DRIVER_CEM102_t *cem102)
{
    uint32_t cal_measurement = 0;
    int result = ERRNO_NO_ERROR;
//    uint16_t target_mv = APP_CALIB_DAC_TARGET_VOLTAGE;
    static uint16_t dac_setting_re = 0;

    uint16_t dac_setting[CEM102_MAX_DACS] = { 0 };
    bool vcc_valid_data = false;
    static uint16_t new_vcc_trim = 0;

//    dac_target_mV.we1 = WE1_DAC_TARGET_MV;
    dac_target_mV.we1 = REF_DAC_TARGET_MV;					// WE1은 전류가 흐르지 않게 한다...테스트 용...
    dac_target_mV.we2 = WE2_DAC_TARGET_MV;
    dac_target_mV.ref = REF_DAC_TARGET_MV;

    if (calib_state == CALIB_INITIALIZED)
    {
        CEM102_CalibrationData *p_data = CEM102_CALIB_DATA_ADDR;

        if (cem102_dut->pwr_cfg == LOW_VOLTAGE)
        {
            /* Read the current VCC Trim */
            uint32_t vcc_trim = (ACS->VCC_CTRL & ACS_VCC_CTRL_VTRIM_Mask);

            /* Discharge VDDA down to its trimmed value of 2.3 */
            cem102->VDDADischarge(cem102_dut, VDDA_DISCHARGE_TIME_MS);

            /* If there is a valid vcc calibration data stored in the flash,
             * load that data for channel configurations. If not, proceed
             * with the vcc calibration.
             *
             * Comment out the following line,
             *    'vcc_valid_data = CEM102_VCC_Calibration_Valid();'
             * if you don't want to use the stored values of the calibration data
             * and run calibration every time device is powered up/initialized.
             */
            vcc_valid_data = CEM102_VCC_Calibration_Valid();
            if (!vcc_valid_data)
            {
                /* Calibrate VCC to 1250 mV */
                cem102->CalibrateVCC(cem102_dut, vcc_trim, VCC_TYPICAL);

                new_vcc_trim = (ACS->VCC_CTRL & ACS_VCC_CTRL_VTRIM_Mask) >>
                                ACS_VCC_CTRL_VTRIM_Pos;

#ifdef SWMTRACE_OUTPUT
                swmLogInfo("VCC Calibrated to:%x\r\n", new_vcc_trim);
#endif /* SWMTRACE_OUTPUT */
            }
            else
            {
                uint8_t stored_vcc_trim = (uint8_t)(p_data->vcc_trim);
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("VCC Calibration trim found in the flash:%x\r\n",
                           stored_vcc_trim);
#endif /* SWMTRACE_OUTPUT */
                Sys_ACS_WriteRegister(&ACS->VCC_CTRL,
                                     (ACS->VCC_CTRL & ~ACS_VCC_CTRL_VTRIM_Mask)
                                     | (stored_vcc_trim <<
                                        ACS_VCC_CTRL_VTRIM_Pos));
            }

        }

        /* Enable the VBAT VDDA monitor after VCC is calibrated */
        cem102->VBAT_VDDAMonitorStart(cem102_dut, WE1,
                                      SENSOR_THRESHOLD_MIN_THRESHOLD_MIN);

        CEM102_SetMeasurementDelayTimer(cem102, APP_LSAD_MEASURE_DELAY_48MS);

        /* If there is a valid calibration data stored in the flash, load that
         * data for channel configurations. If not, proceed with the
         * calibration.
         *
         * Comment out the following section, if you don't want to use the stored
         * values of the calibration data and run calibration every time
         * device is powered up/initialized.
         */

#if 0	// 25.07.21
        if (cem102->CalibrationValid())
        {
            if (vcc_valid_data)
            {
                calib_data.dac_setting[WE1 - INDEX_ADJ_HELPER] =
                                        p_data->dac_setting[WE1 - INDEX_ADJ_HELPER];
                calib_data.dac_setting[WE2 - INDEX_ADJ_HELPER] =
                                        p_data->dac_setting[WE2 - INDEX_ADJ_HELPER];
                calib_data.dac_setting[RE - INDEX_ADJ_HELPER] =
                                        p_data->dac_setting[RE - INDEX_ADJ_HELPER];

                channel_gain[WE1 - INDEX_ADJ_HELPER] = p_data->gain[WE1 - INDEX_ADJ_HELPER];
                channel_gain[WE2 - INDEX_ADJ_HELPER] = p_data->gain[WE2 - INDEX_ADJ_HELPER];

#ifdef SWMTRACE_OUTPUT
                swmLogInfo("Valid Calibration data found in the flash.\r\n");
#endif /* SWMTRACE_OUTPUT */
            }
            else
            {
                /* Copy the incoming calibration data */
                memcpy(&calib_data, p_data, CEM102_CALIB_DATA_SZ);

                calib_data.vcc_trim = new_vcc_trim;

                /* Save calibration data to the flash */
                result |= cem102->CalibrationSave(&calib_data);
            }

            CEM102_SetCalibDone();

            /* Configure switches for the measurement */
            result |= Config_PostCalibration();

        }
        else
#endif
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("Start Calibration.\r\n");
#endif /* SWMTRACE_OUTPUT */

            measure_status = MEASURE_MEASURING;
            calib_state = CALIB_REF_MEASURING;
            cem102->MeasureReference(cem102_dut, CEM102_REF_MEASUREMENT_CHAN);
        }
    }

    else if (calib_state == CALIB_REF_MEASURING)
    {
        APP_ReadMeasurement(cem102, RE);			// APP_REFERENCE_LENGTH 를 위해 RE를 읽는다..
    }
    else if (calib_state == CALIB_REF_READING)
    {
        /* Save the reference measurement */
        calib_ref_measurement = measure_buf[APP_WE1_LOWER_HALF_WORD] +
                               (measure_buf[APP_WE1_UPPER_HALF_WORD] << 16);

#if 1	// add for RE dac value...first calculation...very important
        if (result == ERRNO_NO_ERROR)
        {
            result = DAC_GetCalibratedSetting(RE, dac_target_mV.ref, &dac_setting_re);		// sodykim
            calib_data.dac_setting[RE - INDEX_ADJ_HELPER] = dac_setting_re;
        }
#endif


#if 0
        result |= Config_ForMeasuring(WE1, target_mv);
#else
        result |= Config_ForMeasuring(WE1, dac_target_mV.we1);	// sodykim
#endif
        /* Now that we have our channel configuration, take a calibration
         * measurement
         */
        cem102->CalibrateChannel(cem102_dut, WE1);
        measure_status = MEASURE_MEASURING;
        calib_state = CALIB_CH1_MEASURING;
    }
    else if (calib_state == CALIB_CH1_MEASURING)
    {
        APP_ReadMeasurement(cem102, WE1);
    }
    else if (calib_state == CALIB_CH1_READING)
    {
    	cal_measurement = measure_buf[APP_WE1_LOWER_HALF_WORD] +
                         (measure_buf[APP_WE1_UPPER_HALF_WORD] << 16);

        cem102->CalculateChannelGain(cem102_dut, WE1, calib_ref_measurement, cal_measurement);
        calib_data.gain[WE1 - INDEX_ADJ_HELPER] = channel_gain[WE1 - INDEX_ADJ_HELPER];

        result |= Config_PostMeasurement(WE1);

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("Start Calibration:WE2\r\n");
#endif /* SWMTRACE_OUTPUT */

#if 0
        result |= Config_ForMeasuring(WE2, target_mv);
#else
        result |= Config_ForMeasuring(WE2, dac_target_mV.we2);		// sodykim
#endif
        /* Now that we have our channel configuration, take a calibration
         * measurement
         */
        cem102->CalibrateChannel(cem102_dut, WE2);
        measure_status = MEASURE_MEASURING;
        calib_state = CALIB_CH2_MEASURING;
    }
    else if (calib_state == CALIB_CH2_MEASURING)
    {
        APP_ReadMeasurement(cem102, WE2);
    }
    else if (calib_state == CALIB_CH2_READING)
    {

        cal_measurement = measure_buf[APP_WE2_LOWER_HALF_WORD] +
                         (measure_buf[APP_WE2_UPPER_HALF_WORD] << 16);


        cem102->CalculateChannelGain(cem102_dut, WE2, calib_ref_measurement,
                                     cal_measurement);
        calib_data.gain[WE2 - INDEX_ADJ_HELPER] = channel_gain[WE2 - INDEX_ADJ_HELPER];

        result |= Config_PostMeasurement(WE2);

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("ChannelGain_2: 0x%X, 0x%X, 0x%X\r\n",
                   (int32_t)channel_gain[1], calib_ref_measurement,
                   cal_measurement);
#endif /* SWMTRACE_OUTPUT */


        calib_data.vcc_trim = new_vcc_trim;

        /* Calibration over. Save dac setting for 500 mv target voltage */
        int dac_error = ERRNO_NO_ERROR;
        CEM102_DAC_TYPE dac = WE1;
        uint8_t index = 0;
        for (dac = WE1; dac <= RE; dac++)
        {
            index = (uint8_t)dac - INDEX_ADJ_HELPER;
#if 0
        	dac_error |= DAC_GetCalibratedSetting(dac, APP_LEGACY_CALIB_DAC_VOLTAGE,  &dac_setting[index]);
#else
            if(dac==RE)
            	dac_error |= DAC_GetCalibratedSetting(dac, dac_target_mV.ref,  &dac_setting[index]);
            else if(dac==WE2)
            	dac_error |= DAC_GetCalibratedSetting(dac, dac_target_mV.we2,  &dac_setting[index]);
            else
            	dac_error |= DAC_GetCalibratedSetting(dac, dac_target_mV.we1,  &dac_setting[index]);
#endif

            calib_data.dac_setting[index] = dac_setting[index];

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("dac_setting: %d, %d\r\n",dac_setting[index], index);
#endif /* SWMTRACE_OUTPUT */
        }


#if 0		// NONE SAVE CALIBRATION INFORMATION
        /* Save calibration data to the flash */
        if (dac_error == ERRNO_NO_ERROR)
        {
            result |= cem102->CalibrationSave(&calib_data);
        }
#endif

        CEM102_SetCalibDone();

        uint16_t mask = 0;
        uint16_t regval = 0;
        if (dac_error == ERRNO_NO_ERROR)
        {
            /* When USE_LEGACY_WE_SET_CALIBRATION is set to 0, calibration
             * sets the DAC to 1200 mV. Setting the DAC to user setting, which
             * is 500 mV using the label WE_DAC_TARGET_MV */

            uint8_t addr[3] = {
                DC_DAC_WE1_CTRL,
                DC_DAC_WE2_CTRL,
                DC_DAC_RE_CTRL,
            };

            index = (uint8_t)RE - INDEX_ADJ_HELPER;
            for (dac = WE1; dac <= RE; dac++)
            {
                index = (uint8_t)dac - INDEX_ADJ_HELPER;
#if 0
                dac_error = DAC_GetCalibratedSetting(dac, WE_DAC_TARGET_MV, &regval);
#else
                if(dac==RE)
                	dac_error |= DAC_GetCalibratedSetting(dac, dac_target_mV.ref,  &regval);
                else if(dac==WE2)
                	dac_error |= DAC_GetCalibratedSetting(dac, dac_target_mV.we2,  &regval);
                else
                	dac_error |= DAC_GetCalibratedSetting(dac, dac_target_mV.we1,  &regval);
#endif
                if (dac_error == ERRNO_NO_ERROR)
                {
                    /* Value of the mask is same for all DACs */
                    mask = DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK;
                    cem102->RegisterReadModifyWrite(cem102_dut,
                                                    addr[index], regval,
                                                    mask);
                }
            }
        }

        /* Calibration leaves these switches connected. Disconnecting
         * will improve the residual current measurements. */
        regval = ATBUS_SA14_DISCONNECT | ATBUS_SA7_DISCONNECT;
        mask = ANA_SW_CFG2_ATBUS_SA14_CFG_MASK |  ANA_SW_CFG2_ATBUS_SA7_CFG_MASK;
        result |= cem102->RegisterReadModifyWrite(cem102_dut, ANA_SW_CFG2,
                                                  regval, mask);

#if APP_CEM102_FAST_CALIBRATION
        for (uint8_t idx = 0; idx < MAX_WE_CHANNEL_NUM; idx++)
        {
            regval = APP_ORSD_USER_SETTING;
            mask = 0;
            cem102->GetLSADConfig(cem102_dut, &regval, &mask);
            uint8_t reg = CHCFG_CH1_LSAD + (idx * (CHCFG_CH2_THRESHOLD -
                                            CHCFG_CH1_THRESHOLD));
            cem102->RegisterReadModifyWrite(cem102_dut, reg, regval, mask);
        }
#endif /* APP_CEM102_FAST_CALIBRATION */
    }
    return result;
}

static bool CEM102_ReadFromQueue(DRIVER_CEM102_t *cem102,
                                 uint16_t cem102_status,
                                 uint16_t ch_completion_mask)
{
    bool stat = false;

    if ((cem102_status & ch_completion_mask) == ch_completion_mask)
    {
        stat = true;
        measure_status = MEASURE_READING_RESULTS;
        int result = cem102->QueueRead(cem102_dut, SYS_BUFFER_WORD0,
                                   MEASUREMENT_LENGTH);
        if (result != ERRNO_NO_ERROR)
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("ERROR: Failed to read the queue.\r\n");
#endif /* SWMTRACE_OUTPUT */
        }
    }
    return stat;
}

static bool CEM1020_ReadMeasurementData(DRIVER_CEM102_t *cem102,
                                        uint16_t cem102_status)
{
    uint16_t ch_completion_mask = CH2_COMPLETION;

#if APP_MEASURE_ACROSS_IO_INPUTS
    /* If measuring voltage, only check that channel 2 is done.
     * Otherwise, check that both channels are done.
     */
    if (APP_MeasuringCurrent())
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
    {
        ch_completion_mask |= CH1_COMPLETION;
    }
    return CEM102_ReadFromQueue(cem102, cem102_status, ch_completion_mask);
}

#if APP_MEASURE_ACROSS_IO_INPUTS
static void CEM102_CalculateVoltage(int32_t *p_we1_voltage,
                                    int32_t *p_we2_voltage)
{
    /* Get WE1 Voltage and WE2 Voltage */
    cem102->CalculateVoltage(WE1, (int32_t)(measure_buf[APP_WE1_LOWER_HALF_WORD] +
                             (measure_buf[APP_WE1_UPPER_HALF_WORD] << 16)),
                             p_we1_voltage);
    cem102->CalculateVoltage(WE2, (int32_t)(measure_buf[APP_WE2_LOWER_HALF_WORD] +
                             (measure_buf[APP_WE2_UPPER_HALF_WORD] << 16)),
                             p_we2_voltage);
#ifdef SWMTRACE_OUTPUT
    char *voltage_type[] = { "Offset",
                             "Fixed resistor",
                             "VDDA",
                           };
    swmLogInfo("[%d]: %s WE2 voltage: %d\r\n",
               voltage_id, voltage_type[voltage_id], (int32_t)*p_we2_voltage);
#endif /* SWMTRACE_OUTPUT */
}

static void CEM102_NextVoltageMeasurement(void)
{
    /* Measured data is handled. Configure the switches to the
     * next measurement */
    voltage_id++;

    int status = APP_SetupATBusSwitch(voltage_id);
    if (status != ERRNO_NO_ERROR)
    {
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("ERROR: Failed to switch IO measurement.\r\n");
#endif /* SWMTRACE_OUTPUT */
    }
    measure_status = MEASURE_MEASURING;
    APP_SystemCommand(cem102);
}

static void CEM102_SwitchToVoltageMeasurement(DRIVER_CEM102_t *cem102)
{
    /* Reconfigure to measure voltage */
    attribute_under_measure = APP_MEASURING_VOLTAGE;
    voltage_id = APP_MEASURE_OFFSET_VOLTAGE;

    int status = Config_SendIdleCommand();
    status |= APP_SetupATBusSwitch(voltage_id);

    CEM102_SetThreshViolDetection(false, false);
    measure_status = MEASURE_MEASURING;
    APP_SystemCommand(cem102);

    if (status != ERRNO_NO_ERROR)
    {
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("ERROR: Failed to start IO measurements.\r\n");
#endif /* SWMTRACE_OUTPUT */
    }
}

/* Called when all three voltage measurements are done */
static void CEM102_SwitchToCurrMeasurement(DRIVER_CEM102_t *cem102)
{
    APP_CalculateTemperature();

    /* Reset the voltage ID and the age of the measurement */
    voltage_id = APP_MEASURE_OFFSET_VOLTAGE;
    voltage_measurement_age = 1;

    /* Reconfigure to measure current */
    attribute_under_measure = APP_MEASURING_CURRENT;
    int status = Config_SendIdleCommand();

    /* Reset the AT Bus switches to their disconnected state. */
    status |= cem102->RE_ATBusSwitchConfig(cem102_dut, RE, 0U);
    status |= cem102->AIOConfigDeinit(cem102_dut, WE2, &saved_regs);

    CEM102_SetThreshViolDetection(true, true);
    measure_status = MEASURE_MEASURING;
    APP_SystemCommand(cem102);
    if (status != ERRNO_NO_ERROR)
    {
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("ERROR: Failed to start WE measurements.\r\n");
#endif /* SWMTRACE_OUTPUT */
    }
}
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

static void CEM102_CalculateCurrent(bool display, float *p_we1_current,
                                    float *p_we2_current)
{
    /* Get WE1 Current and WE2 Current */
    cem102->CalculateCurrent(WE1, (int32_t)(measure_buf[APP_WE1_LOWER_HALF_WORD] +
                             (measure_buf[APP_WE1_UPPER_HALF_WORD] << 16)),
                             p_we1_current);

    cem102->CalculateCurrent(WE2, (int32_t)(measure_buf[APP_WE2_LOWER_HALF_WORD] +
                             (measure_buf[APP_WE2_UPPER_HALF_WORD] << 16)),
                             p_we2_current);

    if (display)
    {
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("Measured current (nA): %.6f, %.6f, cem102_vbat: %d millivolts\r\n",
                   *p_we1_current / APP_FEMTO_NANO_COV_FACTOR,
                   *p_we2_current / APP_FEMTO_NANO_COV_FACTOR, cem102_vbat);
#endif /* SWMTRACE_OUTPUT */
    }
}

static void CEM102_UpdateCurrentAndVBAT(void)
{
	float WE1_cur = 0;
	float WE2_cur = 0;

	CEM102_CalculateCurrent(false, &WE1_cur, &WE2_cur);

#if 0
	/* Convert float type of current values to IEEE11073 format integer */
	WE1_current = App_Utils_ConvertFloatToUInt32(WE1_cur);
	WE2_current = App_Utils_ConvertFloatToUInt32(WE2_cur);
#endif

	/* Calculate CEM102 VBAT using SAR ADC */
	cem102->CalculateVBAT(&cem102_vbat);

/********************************************************************************************************************************************************************
 * 		CHANGE UXN PROTOCOL FORMAT...(nA -> WEO & WEP)	(fA)FEMTO -> (nA)NANO
 ********************************************************************************************************************************************************************/
	afe_102->we1_current = WE1_cur/APP_FEMTO_NANO_COV_FACTOR;
	afe_102->we2_current = WE2_cur/APP_FEMTO_NANO_COV_FACTOR;
	afe_102->vbat_lvl_mV = cem102_vbat;

	rsl15_info->vbat_lvl_mV = afe_102->vbat_lvl_mV;

#if 1
	if(afe_102->we1_current >= MAX_WEO_LVL_MV)	afe_102->we1_current = MAX_WEO_LVL_MV;
	if(afe_102->we1_current <= MIN_WEO_LVL_MV)	afe_102->we1_current = MIN_WEO_LVL_MV;
	if(afe_102->we2_current >= MAX_WEO_LVL_MV)	afe_102->we2_current = MAX_WEO_LVL_MV;
	if(afe_102->we2_current <= MIN_WEO_LVL_MV)	afe_102->we2_current = MIN_WEO_LVL_MV;
#endif

	if((afe_102->we1_current == 0) && (afe_102->we2_current == 0)) {	// afe_102->we1_current = 0 afe_102->we2_current = 0 는 AFE 오류..이젠 값을 가져 온다.
		if((we1_current!=1000.0f)&&(we2_current!=1000.0f)) {
			afe_102->we1_current = we1_current;
			afe_102->we2_current = we2_current;
		}
	}
	else {
		we1_current = afe_102->we1_current;
		we2_current = afe_102->we2_current;
	}

	LSAD_measure_sensor_level();												// read to battery & temperature...rtc time sync

#ifdef SWMTRACE_OUTPUT
	swmLogInfo("WEO1 : ("LOG_FLOAT_MARKER")nA, WEO2 ("LOG_FLOAT_MARKER")nA, VBat : %d , Temp : [%d]\r\n",
			LOG_FLOAT(afe_102->we1_current),  LOG_FLOAT(afe_102->we2_current), afe_102->vbat_lvl_mV, rsl15_info->temperature
			);
#endif

}

static void CEM102_ProcessMeasurementData(DRIVER_CEM102_t *cem102)
{
#if APP_MEASURE_ACROSS_IO_INPUTS
    if (APP_MeasuringVoltage())
    {
        CEM102_CalculateVoltage(&WE1_voltage, &WE2_voltage);
    }
    else
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
    {
        CEM102_UpdateCurrentAndVBAT();
    }

#ifdef SWMTRACE_OUTPUT
    if (ch1_violation_count + ch2_violation_count)
    {
        swmLogInfo("Threshold violation count CH1 = %d, CH2 = %d\r\n",
                   ch1_violation_count, ch2_violation_count);
    }
#endif /* SWMTRACE_OUTPUT */
    ch1_violation_count = 0;
    ch2_violation_count = 0;
}

static void CEM102_PrepNextMeasurement(DRIVER_CEM102_t *cem102)
{
#if (CEM102_VDDA_ERR_OPERATION == VDDA_ERR_RECOVERY_MODE)
    /* If VDDA_ERR happened, clear the flag and re-enable VDDA_ERR IRQ */
    if (irq_valid_flag)
    {
        irq_valid_flag = 0;
        cem102->IRQConfig(cem102_dut, irq_cfg);
    }
#endif /* (CEM102_VDDA_ERR_OPERATION == VDDA_ERR_RECOVERY_MODE) */

    if ((SENSOR->THRESHOLD_MIN & SENSOR_THRESHOLD_MIN_ENABLED) == 0)
    {
        SENSOR->THRESHOLD_MIN |= SENSOR_THRESHOLD_MIN_ENABLED;
    }

    measure_status = MEASURE_MEASURING;

#if APP_MEASURE_ACROSS_IO_INPUTS
    /* Update the measurement configuration */
    if (APP_MeasuringVoltage())
    {
        measure_values[voltage_id] = WE2_voltage;

        /* If the last voltage has been measured, calculate the
        * temperature and switch to measuring current. */
        if (voltage_id == APP_MEASURE_VDDA_VOLTAGE)
        {
            CEM102_SwitchToCurrMeasurement(cem102);
        }
        else
        {
            CEM102_NextVoltageMeasurement();
        }
    }
    else
    {
        /* If the voltage measurement is too old (i.e. has expired),
        * switch to measuring voltage.
        * Otherwise, continue with current measurements.
        */
        if (voltage_measurement_age == APP_VOLTAGE_MEASUREMENT_EXPIRY)
        {
            CEM102_SwitchToVoltageMeasurement(cem102);
        }
        else
        {
            APP_SystemCommand(cem102);
            voltage_measurement_age++;

            /* Reenable, if it is disabled by the handler before */
            if (APP_CEM102EventValid(APP_EVENT_THRESH_VIOLATION))
            {
                APP_ClearCEM102Event(APP_EVENT_THRESH_VIOLATION);
                CEM102_SetThreshViolDetection(true, true);
            }
        }
    }
#else
    if (APP_CEM102EventValid(APP_EVENT_THRESH_VIOLATION))
    {
        APP_ClearCEM102Event(APP_EVENT_THRESH_VIOLATION);
        CEM102_SetThreshViolDetection(true, true);
    }
    APP_SystemCommand(cem102);
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
}

static void CEM102_ChanViolationHandler(DRIVER_CEM102_t *cem102)
{
    uint16_t v_count = 0;

    /* Read the violation count */
    cem102->RegisterRead(cem102_dut, SYS_BUFFER_VIOL_CNT, &v_count);

    /* If violation persists, interrupt will persisist too. MCU will
     * be busy servicing the interrupt. Disable threshold detection for
     * the channel that indicated the violation. */
    CEM102_SetThreshViolDetection(false, false);

    /* Clear the violation and restart the measurement */
    measure_status = MEASURE_MEASURING;
    APP_SystemCommand(cem102);

    ch1_violation_count += (v_count &
                           SYS_BUFFER_VIOL_CNT_CH1_VIOLATION_CNT_MASK) >>
                           SYS_BUFFER_VIOL_CNT_CH1_VIOLATION_CNT_POS;
    ch2_violation_count += (v_count &
                           SYS_BUFFER_VIOL_CNT_CH2_VIOLATION_CNT_MASK) >>
                           SYS_BUFFER_VIOL_CNT_CH2_VIOLATION_CNT_POS;
    APP_SetCEM102Event(APP_EVENT_THRESH_VIOLATION);
}

void CEM102_Operation(DRIVER_CEM102_t *cem102)
{
    static uint16_t saved_sys_status = 0;

    if (cem102_low_vbat)
    {
        cem102_low_vbat = false;

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("low_vbat recognized(%d). Trying reading chip version.\r\n",
                    (int)wakeup_irq);
#endif /* SWMTRACE_OUTPUT */

        uint16_t fam = 0;

        /* Read the chip family information. This is for debug purpose only to
         * see if cem102 is responsive. */
        int result = cem102->RegisterRead(cem102_dut, CHIP_FAMILY_VERSION,
                                          &fam);
        if (ERRNO_NO_ERROR != result)
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("\r\Error while reading chip version\r\n");
#endif /* SWMTRACE_OUTPUT */
        }
        else if (0 == fam)
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("\r\nreading chip version returned 0\r\n");
#endif /* SWMTRACE_OUTPUT */
        }

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("- CHIP_FAMILY_VERSION: %X.\r\n", fam);
        swmLogInfo("Restaring CEM102...\r\n");
#endif /* SWMTRACE_OUTPUT */
        wakeup_irq = 0;

        /* Disable sensor so it does not trigger false VBAT threshold
         * during re-start up. Sensor will be reconfigured during
         * CEM102_Startup.
         */
        Sys_Sensor_Disable();

        measure_status = MEASURE_IDLE;
        calib_state = CALIB_NOT_STARTED;

        /* Put 100 ms Delay before restarting to allow the system to settle */
        Sys_Delay(APP_SYS_DELAY_100MS);
        CEM102_Restart(cem102, true);
    }


    /* If the system has just completed initialization, start the first
     * measurement, after running the diagnostics, if enabled.
     */
    if (measure_status == MEASURE_INITIALIZED &&
        CEM102_DiagnosticsDone(cem102))
    {
        wakeup_irq = 0;

        int status = ERRNO_NO_ERROR;
#if APP_MEASURE_ACROSS_IO_INPUTS
        /* Set the starting attribute to measure to be the voltage via IO */
        attribute_under_measure = APP_MEASURING_VOLTAGE;

        /* Initialize the switches and ADC for the measurement.
         *
         * AIOConfigInit API uses CEM102_ACCUM_SAMPLE_CNT_2. ADC gain is
         * Calculated inside the AIOConfigInit
         *
         * For IO measurements, WE2 channel is used.
         */
        cem102->AIOConfigInit(cem102_dut, WE2, &saved_regs);

        /* Setup the Test bus' switches correctly for the first measurement */
        voltage_id = APP_MEASURE_OFFSET_VOLTAGE;
        APP_SetupATBusSwitch(voltage_id);
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

        if (status == ERRNO_NO_ERROR)
        {
            swmLogInfo("measure_status : MEASURE_INITIALIZED.\r\n");

            measure_status = MEASURE_MEASURING;

            /* In the right state to handle the interrupts. Enabling them. */
            uint16_t mask = SYSCTRL_IRQ_CFG_VDDA_ERR_IRQ_CFG_MASK |
                            SYSCTRL_IRQ_CFG_STATE_ERR_IRQ_CFG_MASK |
                            SYSCTRL_IRQ_CFG_DIGITAL_RESET_IRQ_CFG_MASK;
            uint16_t data = VDDA_ERR_IRQ_ENABLE | STATE_ERR_IRQ_ENABLE |
                            DIGITAL_RESET_IRQ_ENABLE;

            /* Enable the threshold violation detection, if enabled */
            if (CEM102_ThreshViolationEnabled())		//         if ((CEM102_THRESH_VIOL_ENABLED == cem102_threshold_violation) && (calib_state == CALIB_DONE))
            {
                if (APP_MeasuringCurrent())
                {
                    CEM102_SetThreshViolDetection(true, true);
                }

                mask |= (SYSCTRL_IRQ_CFG_CH1_VIOLATION_IRQ_CFG_MASK |
                         SYSCTRL_IRQ_CFG_CH2_VIOLATION_IRQ_CFG_MASK);
                data |= (CH1_VIOLATION_IRQ_ENABLE | CH2_VIOLATION_IRQ_ENABLE);
            }

            /* Ready to proess interrupts. Time to enable them. */
            cem102->RegisterReadModifyWrite(cem102_dut, SYSCTRL_IRQ_CFG, data, mask);
            APP_SystemCommand(cem102);

            cem102->RegisterRead(cem102_dut, SYS_STATUS, &saved_sys_status);

        }
#ifdef SWMTRACE_OUTPUT
        else
        {
            swmLogInfo("ERROR: Failed to start a measurement.\r\n");
        }
#endif /* SWMTRACE_OUTPUT */
    }
    else
    {
        int result = ERRNO_NO_ERROR;

        /* If a CEM102 IRQ occurred, read the CEM102 status */
        if (wakeup_irq)
        {
            uint16_t cem102_status = 0;
            wakeup_irq = 0;

            APP_SetCEM102Event(APP_EVENT_FROM_CEM102);
            result = cem102->RegisterRead(cem102_dut, SYS_STATUS,
                                          &cem102_status);
            if (ERRNO_NO_ERROR == result)
            {
                saved_sys_status = cem102_status;
            }
            SYS_WATCHDOG_REFRESH();
        }

        if ((saved_sys_status & DIGITAL_RESET) == DIGITAL_RESET)
        {
            /* Set Digital Reset error bit to be sent to the central */
            cem102_err_status |= CS0_CEM102_ERR_DIG_RST;

            /* Clear the value as the cem102 will be reset */
            saved_sys_status = 0;

            /* Save current IRQ configuration */
            result = cem102->RegisterRead(cem102_dut, SYSCTRL_IRQ_CFG,
                                          &irq_cfg);
            if (result == ERRNO_NO_ERROR)
            {
                /* Disable Digital Reset IRQ */
                result = cem102->IRQConfig(cem102_dut, irq_cfg &
                                           (~DIGITAL_RESET_IRQ_ENABLE));
            }

#ifdef SWMTRACE_OUTPUT
            swmLogInfo("ERROR: Digital Reset. Restarting CEM102...\r\n");
#endif /* SWMTRACE_OUTPUT */

            /* Disable sensor so it does not trigger false VBAT threshold
             * during re-start up. Sensor will be reconfigured during
             * CEM102_Startup.
             */
            Sys_Sensor_Disable();

            measure_status = MEASURE_IDLE;
            calib_state = CALIB_NOT_STARTED;

            /* 50 ms delay before restarting to allow the system to settle */
            Sys_Delay(APP_SYS_DELAY_50MS);
            CEM102_Restart(cem102, true);
        }

        else if ((saved_sys_status & STATE_ERR) == STATE_ERR)
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("ERROR: State Error. Restarting CEM102...\r\n");
#endif /* SWMTRACE_OUTPUT */
            saved_sys_status = 0;
            measure_status = MEASURE_IDLE;
            calib_state = CALIB_NOT_STARTED;

            /* State Error occurred, re-initialize the CEM102. */
            CEM102_Startup(cem102);
        }
        else if ((saved_sys_status & VDDA_ERR) == VDDA_ERR)
        {
            saved_sys_status &= ~VDDA_ERR;
            VDDA_ERR_Handler();

            /* If VDDA_ERR_Handler returns, that means RECOVERY is defined */
            cem102->RegisterRead(cem102_dut, SYS_STATUS,
                                 &saved_sys_status);
        }
        else if (saved_sys_status & (CH1_VIOLATION|CH2_VIOLATION))
        {
            CEM102_ChanViolationHandler(cem102);
            cem102->RegisterRead(cem102_dut, SYS_STATUS,
                                 &saved_sys_status);
        }
        else
        {
            /* Checked for all the possible errors. Time to check whether
             * diagnostics is over before starting the measurement.
             */
            if ((saved_sys_status & DC_DAC_LOAD) != 0)
            {
                saved_sys_status &= ~DC_DAC_LOAD;
#if CEM102_DIAG_FUNCTIONS
                if (CEM102_GetLSADState() == CEM102_LSAD_STATE_DAC_LOADED)
                {
                    CEM102_SetLSADState(CEM102_LSAD_STATE_DAC_READY);
                }
#endif /* CEM102_DIAG_FUNCTIONS */
            }

            if (!CEM102_DiagnosticsDone(cem102))
            {
                if (APP_CEM102EventValid(APP_EVENT_FROM_CEM102))
                {
                    APP_ClearCEM102Event(APP_EVENT_FROM_CEM102);
                    CEM102_Diagnostics(cem102);
                }
            }
            else
            {
                /* Check if the next results can be read */
                if ((measure_status != MEASURE_READING_RESULTS) &&
                    (measure_status != MEASURE_RESULTS_READY))
                {
                    if ((CEM102_ValidStateToMeasure(saved_sys_status)) &&
                        (APP_CEM102EventValid(APP_EVENT_FROM_CEM102)))
                    {
                        APP_ClearCEM102Event(APP_EVENT_FROM_CEM102);
                        if (CEM1020_ReadMeasurementData(cem102,
                                                        saved_sys_status))
                        {
                            saved_sys_status &= ~(CH1_COMPLETION |
                                                  CH2_COMPLETION);
                        }
                    }
                }

                /* Make sure the device is in CONTINUOUS STATE before reading
                 * data.
                 *
                 * Measure_status changes on DMA irq. cem102_status gets
                 * updated on wakeup_irq. Both variables may not have valid
                 * values at the same time. On such cases, being a local
                 * variable, cem102_status may be 0, when measure_status is
                 * set to MEASURE_RESULTS_READY. It is better to use the
                 * SYS STATE value that is saved during the previous
                 * interrupt.
                 */
                uint16_t cem102_state = saved_sys_status &
                                        SYS_STATUS_SYS_STATE_MASK;
                if (((cem102_state == CONTINUOUS_STATE) ||
                    (cem102_state == IDLE_STATE)) &&
                    (measure_status == MEASURE_RESULTS_READY))
                {
                    SYS_WATCHDOG_REFRESH();

                    CEM102_ProcessMeasurementData(cem102);

                    CEM102_PrepNextMeasurement(cem102);
                }
            }
        }
    }
}

