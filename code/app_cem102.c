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


#define APP_EVENT_FROM_CEM102      (1U << 0)
#define APP_EVENT_THRESH_VIOLATION (1U << 1)

#if CEM102_DIAG_FUNCTIONS
/* RE DAC voltage isn't measured by by default */
#define CEM102_MEAS_DAC_VOLTAGES    (CEM102_ENABLE_WE1_V_MEAS | \
                                     CEM102_ENABLE_WE2_V_MEAS)
#else /* CEM102_DIAG_FUNCTIONS */
#define CEM102_MEAS_DAC_VOLTAGES   (0)
#endif /* CEM102_DIAG_FUNCTIONS */

/* There is no CEM102_ENABLE_WE1 as it is always enabled */
#define APP_ENABLED_FEATURES       ( \
                                    CEM102_ENABLE_INTERRUPT | \
                                    CEM102_ENABLE_WE2 | \
                                    CEM102_ENABLE_RE | \
                                    CEM102_MEAS_DAC_VOLTAGES | \
                                    CEM102_CONNECT_RE_CE | \
									CEM102_NTC_TO_VDDA \
                                   )

static uint8_t meas_timer_ok_to_run;
static uint8_t cem102_event = 0;

/* Globals for Chicopee measurements */
volatile MEASUREMENT_STATUS_FLAG demo_measure_status;
uint16_t demo_measure_buf[MEASUREMENT_LENGTH];
uint32_t calib_ref_measurement;
volatile bool cem102_low_vbat = false;
static bool vdda_err_handled = false;
static bool next_measurement_iteration = false;

volatile uint8_t irq_valid_flag;

volatile Calib_state_t calib_state;

static volatile uint16_t irq_cfg = 0;

#if APP_MEASURE_ACROSS_IO_INPUTS || CEM102_DIAG_FUNCTIONS
static uint8_t attribute_under_measure = APP_MEASURING_VOLTAGE;
#if APP_MEASURE_ACROSS_IO_INPUTS
static CEM102_RegSaveData saved_regs;
static int32_t measure_values[APP_MEASURE_VOLTAGE_TYPES];
static uint8_t voltage_id = APP_MEASURE_VOLTAGE_TYPE_FIRST;
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
#endif /* APP_MEASURE_ACROSS_IO_INPUTS || CEM102_DIAG_FUNCTIONS */

DRIVER_CEM102_t *cem102 = &Driver_CEM102;
CEM102_TrimStruct trims;

/* Calibration data to be saved as a persistent data */
CEM102_CalibrationData calib_data;

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
    .gpio_cfg   = (GPIO_LPF_DISABLE | GPIO_WEAK_PULL_UP  | GPIO_3X_DRIVE),
    .spi_prescale = SPI_PRESCALE_8,

    /* Call-back functions */
    .spi_callback = &SPI_CallBack,
    .gpio_callback = &GPIO_CallBack,

    /* Turn on the default features. CEM102_NTC_TO_VDDA is off by default
     * and CEM102_DC_DAC_16K is off, to set DC DAC frequency to 8 KHz.
     * WE1 is always enabled. No need to mention here.
     */
    .feature = APP_ENABLED_FEATURES,

    /* Sample accumulation setting. Common for both LSADs */
    .accum_sample_calib = ACCUM_SAMPLE_CNT_8,   /* For calibration */
    .accum_sample_curr = ACCUM_SAMPLE_CNT_8,    /* For current measurement */
    .accum_sample_volt = CEM102_ACCUM_SAMPLE_CNT_2, /* For voltage measurement */

    .threshold_detection[CEM102_LSAD1_IDX] = CEM102_THRESH_VIOL_DISABLED,
    .threshold_detection[CEM102_LSAD2_IDX] = CEM102_THRESH_VIOL_DISABLED,

    .fb_resistance[CEM102_LSAD1_IDX] = CH1_FEEDBACK_RESISTANCE,
    .fb_resistance[CEM102_LSAD2_IDX] = CH2_FEEDBACK_RESISTANCE,
    .lpf_cutoff_c[CEM102_LSAD1_IDX] = CEM102_CH1_LPF_CUTOFF_500HZ,
    .lpf_cutoff_c[CEM102_LSAD2_IDX] = CEM102_CH2_LPF_CUTOFF_500HZ,
    .lpf_cutoff_v[CEM102_LSAD1_IDX] = CEM102_CH2_LPF_CUTOFF_10KHZ,
    .lpf_cutoff_v[CEM102_LSAD2_IDX] = CEM102_CH2_LPF_CUTOFF_10KHZ,
    .lsad_prescale_curr = LSAD_PRESCALE2,
    .lsad_prescale_volt = LSAD_PRESCALE1,

    .lsad_cfg_user[CEM102_LSAD1_IDX] = LSAD1_ORSD8192_RSD8 | LSAD1_SYS_CHOP_ENABLE |
                                       LSAD1_ITRIM_1p00 | CH1_LSAD_GAIN_SETTING,
    .lsad_cfg_user[CEM102_LSAD2_IDX] = LSAD2_ORSD8192_RSD8 | LSAD2_SYS_CHOP_ENABLE |
                                       LSAD2_ITRIM_1p00 | CH2_LSAD_GAIN_SETTING,
    .lsad_cfg_calib[CEM102_LSAD1_IDX] = APP_LSAD_NUM_INT_CYCLE | LSAD1_SYS_CHOP_ENABLE |
                                        LSAD1_ITRIM_1p00 | CH1_LSAD_GAIN_SETTING,
    .lsad_cfg_calib[CEM102_LSAD2_IDX] = APP_LSAD_NUM_INT_CYCLE | LSAD2_SYS_CHOP_ENABLE |
                                        LSAD2_ITRIM_1p00 | CH2_LSAD_GAIN_SETTING,
    .lsad_cfg_volt = LSAD2_ORSD1024_RSD10 | LSAD2_SYS_CHOP_ENABLE |
                     LSAD2_ITRIM_1p00 | LSAD2_GAIN_UNITY,

    /* With LSADx_ORSD8192_RSD8 setting, when both diagnostics and thermistor
     * application are enabled, measurements take little more than 10 seconds.
     *
     * Tweak according to total measurement time. Timer is disabled by default,
     * which means measurements are done continuously.
     */
    .meas_timer_interval = 0, /* Value in seconds */
};

const CEM102_Device *cem102_dut = &cem102_dut_desc;

static uint16_t ch1_violation_count = 0;
static uint16_t ch2_violation_count = 0;

static void Prepare_ThresholdCfg(const CEM102_DAC_TYPE dac,
                                 uint16_t *p_threshold, uint16_t sample_cnt);

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

bool CEM102_Sleep_Check(void)
{
    bool result = true;

    /* If calibration is done, and measurement is already ready to be read,
     * do not allow the system go to sleep.*/
    if (((wakeup_irq != 0) || (demo_measure_status == MEASURE_READING_RESULTS) ||
        (demo_measure_status == MEASURE_RESULTS_READY)) && (calib_state == CALIB_DONE))
    {
        result = false;
    }

    return result;
}

bool APP_MeasurementTimerEnabled(void)
{
    return (cem102_dut->meas_timer_interval == 0) ? false : true;
}

static bool APP_ThermistorEnabled(void)
{
#if APP_MEASURE_ACROSS_IO_INPUTS
    return true;
#else
    return false;
#endif
}

static void APP_ClearMeasureTimerEvent(const CEM102_Device *p_dev, uint8_t ev)
{
    ev &= (MEAS_TIMER_OK_TO_RUN_MASK | MEAS_TIMER_STARTED_RUNNING);
    meas_timer_ok_to_run &= ~ev;
}

static void APP_SetMeasureAttribute(uint8_t attr)
{
#if APP_MEASURE_ACROSS_IO_INPUTS || CEM102_DIAG_FUNCTIONS
    attribute_under_measure = attr;
#endif /* APP_MEASURE_ACROSS_IO_INPUTS || CEM102_DIAG_FUNCTIONS */
}

static bool APP_DiagMeasuresVoltage(const CEM102_Device *p_dev)
{
    bool status = false;

#if CEM102_DIAG_FUNCTIONS
    if ((p_dev != NULL) && (CEM102_MEAS_WE1_VOLTAGE(p_dev) ||
        CEM102_MEAS_WE2_VOLTAGE(p_dev) || CEM102_MEAS_WE2_VOLTAGE(p_dev)))
    {
        status = true;
    }
#endif /* CEM102_DIAG_FUNCTIONS */
    return status;
}

static bool APP_CurrentOnlyMeasurement(const CEM102_Device *p_dev)
{
    bool status = true;

    if ((APP_ThermistorEnabled() || APP_DiagMeasuresVoltage(p_dev)))
    {
        status = false;
    }
    return status;
}

static void APP_TIAConfig(DRIVER_CEM102_t *p_cem102,
                          const CEM102_Device *p_dev, uint32_t idx,
                          bool require_lsad2)
{
    CEM102_DAC_TYPE dac[CEM102_NUM_LSADS] = { WE1, WE2 };
    uint16_t en[CEM102_NUM_LSADS] = { CH1_TIA_ENABLE, CH2_TIA_ENABLE };
    uint16_t cfg[CEM102_NUM_LSADS] = { CH1_TIA_INT_FB_ENABLE |
                                       CH1_FEEDBACK_RESISTANCE,
                                       CH2_TIA_INT_FB_ENABLE |
                                       CH2_FEEDBACK_RESISTANCE
                                     };
    if ((!require_lsad2) && (!CEM102_WE2_ENABLED(p_dev)))
    {
        en[CEM102_WE2_INDEX] = CH2_TIA_DISABLE;
    }
    p_cem102->TIAConfig(p_dev, dac[idx], en[idx], cfg[idx]);
}

static void APP_UpdateChannelConfig(DRIVER_CEM102_t *p_cem102,
                                    const CEM102_Device *p_dev, uint32_t idx,
                                    bool require_lsad2, bool measures_voltage)
{
    if (idx < CEM102_NUM_LSADS)
    {
        APP_TIAConfig(p_cem102, p_dev, idx, require_lsad2);

        CEM102_DAC_TYPE dac[CEM102_NUM_LSADS] = { WE1, WE2 };
        uint16_t bufcfg[CEM102_NUM_LSADS] = { CH1_BUF_ENABLE, CH2_BUF_ENABLE };
        uint16_t bias_cur_setting = measures_voltage ?
                                    CEM102_BIAS_CURR_FOR_V(p_dev, idx) :
                                    CEM102_BIAS_CURR_FOR_C(p_dev, idx);
        uint16_t chancfg = measures_voltage ?  p_dev->lpf_cutoff_v[idx] :
                                               p_dev->lpf_cutoff_c[idx];
        chancfg |= bias_cur_setting;

        /* When it calibration state, we are not disabling WE2 */
        if ((!require_lsad2) && (!CEM102_WE2_ENABLED(p_dev)))
        {
            bufcfg[CEM102_WE2_INDEX] = CH2_BUF_DISABLE | CH2_BUF_BYPASS_ENABLE;
        }
        p_cem102->BufferConfig(p_dev, dac[idx], chancfg, bufcfg[idx]);
    }

    /* Prescale setting must match the bias setting in chan_cfg above */
    p_cem102->ClockConfig(p_dev,
                          measures_voltage ? p_dev->lsad_prescale_volt :
                          p_dev->lsad_prescale_curr,
                          LSAD_START_DELAY_50MS);
}

/* Program uses "in_calibration" to to tell the function whether this function
 * is being called from calibration module or not. This is needed to
 * to pass "require_lsad2" flag to APP_UpdateChannelConfig function.
 */
static void APP_ChannelConfig(DRIVER_CEM102_t *p_cem102,
                              const CEM102_Device *p_dev, uint32_t idx,
                              bool in_calibration, bool measures_voltage)
{
    /* During calibration, we require LSAD2 as we use both LSADS.
     * During measurement, configuration decides the state of LSAD2.
     * So, "in_calibration" is mapped to "require_lsad2" boolean flag here.
     */
    APP_UpdateChannelConfig(p_cem102, p_dev, idx, in_calibration,
                            measures_voltage);

    CEM102_DAC_TYPE dac[CEM102_NUM_LSADS] = { WE1, WE2 };
    uint16_t swcfg[CEM102_NUM_LSADS] =
             {
                CH1_SC119_SC120_CONNECT | CH1_SC112_CONNECT,
                CH2_SC219_SC220_CONNECT | CH2_SC212_CONNECT,
             };
    p_cem102->SwitchConfig(p_dev, dac[idx], swcfg[idx]);

    uint16_t sample_cnt = (in_calibration) ?
                           p_dev->accum_sample_calib :
                           p_dev->accum_sample_curr;
    uint16_t accum_cfg[CEM102_NUM_LSADS] =
                            { (CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_MASK &
                               sample_cnt) | CH1_MEASUREMENT_ENABLE,
                              (CHCFG_CH2_ACCUM_CH2_ACCUM_SAMPLE_CNT_MASK &
                               sample_cnt) | CH2_MEASUREMENT_ENABLE
                            };

    /* By default, we use CH1_TIA_FB_RES_4M and LSAD1_GAIN_UNITY. This can be
     * changed to use CH1_TIA_FB_RES_8M + LSAD1_GAIN_DOUBLE, if needed */
    uint16_t buff_cfg[CEM102_NUM_LSADS] =
                           { CH1_DOUBLE | (CH1_VIOLATION_LIMIT <<
                             CHCFG_CH1_BUFFER_CH1_VIOLATION_LIMIT_POS),
                             CH2_DOUBLE | (CH2_VIOLATION_LIMIT <<
                             CHCFG_CH2_BUFFER_CH2_VIOLATION_LIMIT_POS)
                           };
    uint16_t lsad_cfg = (in_calibration) ?
                         p_dev->lsad_cfg_calib[idx] :
                         p_dev->lsad_cfg_user[idx];
    p_cem102->GetLSADConfig(p_dev, &lsad_cfg, NULL);

    uint16_t thresh_cfg[CEM102_NUM_LSADS] = { 0, 0 };
    Prepare_ThresholdCfg(dac[idx], thresh_cfg, sample_cnt);

    p_cem102->ADCConfig(p_dev, dac[idx], lsad_cfg, thresh_cfg[idx],
                        buff_cfg[idx], accum_cfg[idx]);

    /* Recalculate ADC gain, after setting sample accumuation count */
    p_cem102->CalculateADCGain(dac[idx], (uint32_t)sample_cnt + 1);
}

#if APP_MEASURE_ACROSS_IO_INPUTS
static void APP_DisconnectIOInputs(DRIVER_CEM102_t *p_cem102,
                                   const CEM102_Device *p_dev)
{
    if ((p_cem102 != NULL) && (p_dev != NULL))
    {
        p_cem102->RE_ATBusSwitchConfig(p_dev, RE,  0);
        if (!CEM102_WE2_ENABLED(p_dev))
        {
            p_cem102->SwitchConfig(p_dev, WE2, 0);
        }
    }
}

static bool APP_MeasuringVoltage(void)
{
    return (attribute_under_measure == APP_MEASURING_VOLTAGE) ? true : false;
}

#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

static bool APP_MeasuringWithDiag(void)
{
    bool status = false;

#if CEM102_DIAG_FUNCTIONS
    status = (attribute_under_measure == APP_MEASURING_WITH_DIAG) ? true : false;
#endif
    return status;
}

/* Returns false, when the function has no action */
static bool APP_StartMeasurementWithDiag(DRIVER_CEM102_t *p_cem102,
                                         const CEM102_Device *p_dev)
{
    bool status = false;

#if CEM102_DIAG_FUNCTIONS
    if (APP_DiagMeasuresVoltage(p_dev))
    {
        APP_SetMeasureAttribute(APP_MEASURING_WITH_DIAG);
        CEM102_DiagnosticsAPIInit(p_cem102, p_dev);
        status = true;
    }
#endif /* CEM102_DIAG_FUNCTIONS */
    return status;
}

static void APP_StartTheMeasurementTimer(const CEM102_Device *p_dev)
{
    ke_timer_set(CEM102_MEASUREMENT_START_TIMEOUT, TASK_APP,
                 TIMER_SETTING_S(p_dev->meas_timer_interval));
}

static bool APP_CalibrationValid(DRIVER_CEM102_t *p_cem102)
{
    bool result = false;
    CEM102_CalibrationData *p_calib_data = CEM102_CALIB_DATA_ADDR;

    if ((p_calib_data->dac_v[CEM102_WE1_INDEX] == WE1_DAC_V_USER_SETTING) &&
        (p_calib_data->dac_v[CEM102_WE2_INDEX] == WE2_DAC_V_USER_SETTING) &&
        (p_calib_data->dac_v[CEM102_RE_INDEX] == RE_DAC_V_USER_SETTING) &&
        (p_cem102->CalibrationValid()))
    {
        result = true;
    }
    return result;
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
    return ((cem102_event & event) != 0) ? true : false;
}

static bool APP_MeasuringCurrent(void)
{
#if APP_MEASURE_ACROSS_IO_INPUTS || CEM102_DIAG_FUNCTIONS
    return (attribute_under_measure == APP_MEASURING_CURRENT) ? true : false;
#else
    return true;
#endif /* APP_MEASURE_ACROSS_IO_INPUTS || CEM102_DIAG_FUNCTIONS */
}

static bool CEM102_ThreshViolationEnabled(const CEM102_Device *p_dev,
                                          uint8_t idx)
{
    bool status = false;

    if ((CEM102_THRESH_VIOL_ENABLED == p_dev->threshold_detection[idx]) &&
        (calib_state == CALIB_DONE))
    {
        status = true;
    }
    return status;
}

static void APP_SetChanAccumReg(DRIVER_CEM102_t *p_cem102, CEM102_DAC_TYPE dac,
                                uint16_t val, uint16_t mask)
{
    p_cem102->RegisterReadModifyWrite(cem102_dut,
                                      (dac == WE2) ? CHCFG_CH2_ACCUM :
                                      CHCFG_CH1_ACCUM, val, mask);
}

/* Though CEM102_DAC_TYPE is passed, this function deals with LSAD */
void CEM102_ChannelCtrl(DRIVER_CEM102_t *p_cem102, CEM102_DAC_TYPE dac,
                        bool enable)
{
    if (((dac == WE1) || (dac == WE2)) && (p_cem102 != NULL))
    {
        APP_SetChanAccumReg(p_cem102, dac, enable ? CH1_MEASUREMENT_ENABLE :
                            CH1_MEASUREMENT_DISABLE,
                            CHCFG_CH1_ACCUM_CH1_MEASUREMENT_CFG_MASK);
    }
}

void CEM102_EnableSingleChannel(DRIVER_CEM102_t *p_cem102, CEM102_DAC_TYPE dac)
{
    if (p_cem102 != NULL)
    {
        CEM102_DAC_TYPE other_dac = (dac == WE2) ? WE1 : WE2;
        CEM102_ChannelCtrl(p_cem102, other_dac, false);
        CEM102_ChannelCtrl(p_cem102, dac, true);
    }
}

/* Disables the buffer and measurement if not used */
static void CEM102_ConfigureLSAD(DRIVER_CEM102_t *p_cem102,
                                 const CEM102_Device *p_dev,
                                 CEM102_DAC_TYPE dac, bool enable)
{
    /* Disable value is 0, same for both LSADs */
    uint16_t val = enable ? ((dac == WE2) ?
                             CH2_BUF_ENABLE : CH1_BUF_ENABLE) :
                             CH1_BUF_DISABLE;
    uint16_t mask = (dac == WE2) ?
                     (1U << ANA_CFG1_CH2_BUF_CFG_POS) :
                     (1U << ANA_CFG1_CH1_BUF_CFG_POS);
    p_cem102->RegisterReadModifyWrite(p_dev, ANA_CFG1, val, mask);
    CEM102_ChannelCtrl(p_cem102, dac, enable);
}

static void CEM102_ControlNextIteration(DRIVER_CEM102_t *p_cem102,
                                         const CEM102_Device *p_dev)
{
    /* One set of measurements finished. If measurement timer is
     * running, turn off both LSADs.
     */
    if (p_dev->meas_timer_interval != 0)
    {
        CEM102_ConfigureLSAD(p_cem102, p_dev, WE1, false);

        if (CEM102_WE2_ENABLED(p_dev) || (!APP_CurrentOnlyMeasurement(p_dev)))
        {
            CEM102_ConfigureLSAD(p_cem102, p_dev, WE2, false);
        }
    }
    else
    {
        next_measurement_iteration = true;
    }
}

static void CEM102_SetupMeasurementCycle(DRIVER_CEM102_t *p_cem102,
                                         const CEM102_Device *p_dev)
{
    if (p_dev->meas_timer_interval != 0)
    {
        if (CEM102_WE2_ENABLED(p_dev) || (!APP_CurrentOnlyMeasurement(p_dev)))
        {
            CEM102_ConfigureLSAD(p_cem102, p_dev, WE2, true);
        }

        /* WE1 is always enabled */
        CEM102_ConfigureLSAD(p_cem102, p_dev, WE1, true);
    }
}

/* Measurement can start after some delay, when SINGLE_DELAY_CMD is used. */
static void CEM102_SetMeasurementDelayTimer(DRIVER_CEM102_t *p_cem102,
                                            uint16_t msec)
{
    uint16_t val = LSAD_MEAS_ENABLE;
    uint16_t mask = 1U << SYSCTRL_AUX_CFG_LSAD_ENABLE_CFG_POS;

    msec /= APP_LSAD_MEASURE_DELAY_STEP_SZ;
    if (msec != 0)
    {
        val |= msec <<
              SYSCTRL_AUX_CFG_SINGLE_DELAY_TIMER_POS;
        mask |= SYSCTRL_AUX_CFG_SINGLE_DELAY_TIMER_MASK;
    }
    p_cem102->RegisterReadModifyWrite(cem102_dut, SYSCTRL_AUX_CFG, val,
                                      mask);
}

static void APP_SystemCommand(DRIVER_CEM102_t *p_cem102)
{
    uint16_t cmd = SINGLE_DELAY_CMD;
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
    if (demo_measure_status != MEASURE_INITIALIZED)
    {
        demo_measure_status = MEASURE_MEASURING;
    }

    /* Clear the VDDA_ERR and proceed to a new measurement */
    APP_SystemCommand(cem102);
#endif /* (CEM102_VDDA_ERR_OPERATION == NO_RETENTION_SLEEP_MODE) */
}

void GPIO1_IRQHandler(void)
{
    /* NVIC may be active, when debugger is connected */
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
    if (demo_measure_status == MEASURE_READING_RESULTS)
    {
        /* During calibration, RegisterMultipleRead API is used that already
         * reads the data off SPI rx buffer. Call to RegisterBufferRead is
         * needed only when QueueRead is used, that is when CEM102_Operation
         * is in use.
         */
        if (calib_state == CALIB_DONE)
        {
            cem102->RegisterBufferRead(cem102_dut, demo_measure_buf,
                                       MEASUREMENT_LENGTH);
        }
        demo_measure_status = MEASURE_RESULTS_READY;

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
    uint32_t index = dac - INDEX_ADJ_HELPER;

    /* Channel gain must be non-zero when calibration is done */
    if (channel_gain[index] != 0.0)
    {
        float limit[CEM102_NUM_LSADS] = { CH1_TARGET_VIOLATION_CURRENT,
                                          CH2_TARGET_VIOLATION_CURRENT
                                        };
        uint16_t threshold = (int)((limit[index] / channel_gain[index]) /
                              (sample_cnt + 1))
                               >> CEM102_LSAD_THRESHOLD_PADDING;

        /* Value of mask and shift position are is same for WE1 and WE2 */
        p_threshold[index] = (threshold <<
                             CHCFG_CH1_THRESHOLD_CH1_THRESHOLD_POS) &
                             CHCFG_CH1_THRESHOLD_CH1_THRESHOLD_MASK;

        uint16_t enable[CEM102_NUM_LSADS] = { APP_LSAD1_VIOLATION_MODE,
                                              APP_LSAD2_VIOLATION_MODE
                                            };
        if (CEM102_ThreshViolationEnabled(cem102_dut, (uint8_t)index))
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
        demo_measure_status = MEASURE_READING_RESULTS;
        p_cem102->RegisterMultipleRead(cem102_dut, SYS_BUFFER_WORD0,
                                       demo_measure_buf, len[index]);
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
        uint16_t dac_setting = CEM102_INITIAL_VALUE;
        result = cem102->CalculateDACAmplitude(cem102_dut, dac, target_mv,
                                               &dac_setting);

        /* OTP v3 parts will cause CalculateDACAmplitude to return error. */
        if (result == ERRNO_GENERAL_FAILURE)
        {
            result = cem102->CalculateDACAmplitudeADC(cem102_dut, dac,
                                                      target_mv, &dac_setting);
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("DAC-%d calibrated using ADC\r\n", dac);
#endif /* SWMTRACE_OUTPUT */
        }
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("DAC-%d: CalculateDACAmplitude %d mV returns 0x%x\r\n", dac,
                   (int)target_mv, (uint32_t)dac_setting);
#endif /* SWMTRACE_OUTPUT */
        *p_dac = dac_setting;
        APP_CEM102_WaitForIdle();
    }
    return result;
}

static int Config_PostCalibration(void)
{
    int result = ERRNO_NO_ERROR;
    uint16_t dac_setting = CEM102_INITIAL_VALUE;
    uint16_t dac_mask[2] = { DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK,
                             DC_DAC_WE2_CTRL_DAC_WE2_VALUE_MASK
                           };
    uint16_t dac_en[2] = { DAC_WE1_ENABLE, DAC_WE2_ENABLE };

    dac_en[CEM102_WE2_INDEX] = CEM102_GetDACEnableBit(cem102_dut, WE2);

    /* Make sure that CEM102 is in IDLE mode, before configuring */
    Config_SendIdleCommand();

    for (uint32_t index = 0; index < MAX_WE_CHANNEL_NUM; index++)
    {
        int dac = index + 1;

        /* This is not part of calibration. Passing "false" here */
        APP_ChannelConfig(cem102, cem102_dut, index, false, true);

        dac_setting = calib_data.dac_setting[index];

        cem102->WEDACConfig(cem102_dut, dac,
                            ((dac_mask[index] & dac_setting) |
                            dac_en[index]), DC_DAC_LOAD_IRQ_DISABLE);
    }

    /* Restore ANA_CFG2 configuration. */
    cem102->RegisterReadModifyWrite(cem102_dut, ANA_CFG2,
                                    SENSOR_CAL_DISABLE,
                                    ANA_CFG2_SENSOR_CAL_CFG_MASK);

    /* RE channel enable */
    uint16_t re_dac = (uint16_t)calib_data.dac_setting[RE - INDEX_ADJ_HELPER];
    uint16_t enable = CEM102_RE_ENABLED(cem102_dut) ? DAC_RE_ENABLE :
                                        DAC_RE_DISABLE;
    result = cem102->REDACConfig(cem102_dut,
                                 ((DC_DAC_RE_CTRL_DAC_RE_VALUE_MASK &
                                 re_dac) | enable),
                                 CEM102_SW_SR1_POSITION(cem102_dut),
                                 RE_BUF_ENABLE);
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
        /* Set to idle mode before configuring */
        Config_SendIdleCommand();

        /* Let us retain, not clear the value of DAC settings */
        uint16_t val = 0;
        cem102->RegisterRead(cem102_dut, DC_DAC_RE_CTRL, &val);
        val &= DC_DAC_RE_CTRL_DAC_RE_VALUE_MASK;

        /* DAC RE and RE buf disable */
        result = cem102->REDACConfig(cem102_dut, val, 0, 0);

        /* Disable the measurement for the unused channel, while calibrating
         * a channel. This will expedite the results. Need to re-enable, once
         * the calibration is over.
         */
        CEM102_ChannelCtrl(cem102, (dac == WE2) ? WE1 : WE2, false);

        uint32_t index = dac - INDEX_ADJ_HELPER;
        uint16_t dac_setting = 0;
        if (result == ERRNO_NO_ERROR)
        {
            result = DAC_GetCalibratedSetting(dac, target_mv,
                                              &dac_setting);
        }

        /* During calibration, WE2 must be ON. We pass true */
        APP_ChannelConfig(cem102, cem102_dut, index, true, true);

        /* Configure the CEM102 device for a basic measurement */
        uint16_t dac_mask[2] = { DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK,
                                 DC_DAC_WE2_CTRL_DAC_WE2_VALUE_MASK
                               };
        uint16_t dac_en[2] = { DAC_WE1_ENABLE, DAC_WE2_ENABLE };

        result |= cem102->WEDACConfig(cem102_dut, dac,
                                      ((dac_mask[index] & dac_setting) |
                                      dac_en[index]), 0);

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
        uint32_t index = dac - INDEX_ADJ_HELPER;
        uint16_t sw_cfg[2] = { CH1_SC119_SC120_CONNECT | CH1_SC112_CONNECT,
                               CH2_SC219_SC220_CONNECT | CH2_SC212_CONNECT,
                             };
        result |= cem102->SwitchConfig(cem102_dut, dac, sw_cfg[index]);

        /* Restore ANA_CFG2 configuration. */
        result |= cem102->RegisterReadModifyWrite(cem102_dut, ANA_CFG2,
                                                  SENSOR_CAL_DISABLE,
                                                  ANA_CFG2_SENSOR_CAL_CFG_MASK);

        Config_SendIdleCommand();
        Sys_Delay(APP_SYS_DELAY_20MS);
    }
    return result;
}

#if APP_MEASURE_ACROSS_IO_INPUTS
static int APP_SetupATBusSwitch(DRIVER_CEM102_t *p_cem102,
                                const CEM102_Device *p_dev,
                                uint8_t request)
{
    int result = ERRNO_NO_ERROR;

    if (request == APP_MEASURE_VOLTAGE_TYPE_FIRST)
    {
        CEM102_ChannelCtrl(p_cem102, WE1, false);

        uint16_t prescale = p_dev->lsad_prescale_volt;
        uint16_t mask =  CHCFG_LSAD_CLK_LSAD_CLK_PRESCALE_MASK;
        p_cem102->RegisterReadModifyWrite(p_dev, CHCFG_LSAD_CLK,
                                          prescale, mask);

        uint16_t chancfg = (prescale == LSAD_PRESCALE1) ?
                            CEM102_CH2_BIAS_PRESCALE1 :
                            CEM102_CH2_BIAS_PRESCALE2;
        chancfg |= p_dev->lpf_cutoff_v[CEM102_LSAD2_IDX];

        uint16_t bufcfg = CH2_BUF_DISABLE | CH2_BUF_BYPASS_ENABLE;

        result = p_cem102->BufferConfig(p_dev, CEM102_VOLTAGE_MEAS_CHAN,
                                        chancfg, bufcfg);
        uint16_t swcfg = (CH2_SC209_SC210_DISCONNECT | CH2_SC215_CONNECT |
                      CH2_SC216_CONNECT);
        swcfg |= CEM102_WE2_ENABLED(p_dev) ? CH2_SC212_CONNECT :
                                             CH2_SC212_DISCONNECT;
        p_cem102->SwitchConfig(p_dev, CEM102_VOLTAGE_MEAS_CHAN, swcfg);

        uint16_t sample_cnt = p_dev->accum_sample_volt;
        uint16_t lsad_cfg = p_dev->lsad_cfg_volt;
        uint16_t accum_cfg = (CHCFG_CH2_ACCUM_CH2_ACCUM_SAMPLE_CNT_MASK &
                              sample_cnt) | CH2_MEASUREMENT_ENABLE;
        uint16_t buff_cfg = CH2_DOUBLE | (CH2_VIOLATION_LIMIT <<
                            CHCFG_CH2_BUFFER_CH2_VIOLATION_LIMIT_POS);

        uint16_t thresh_cfg[CEM102_NUM_LSADS] = { 0, 0 };

        Prepare_ThresholdCfg(CEM102_VOLTAGE_MEAS_CHAN, thresh_cfg, sample_cnt);

        /* Get the config for the ORSD, system chopper state we want */
        p_cem102->GetLSADConfig(p_dev, &lsad_cfg, NULL);
        p_cem102->ADCConfig(p_dev, CEM102_VOLTAGE_MEAS_CHAN, lsad_cfg,
                            thresh_cfg[CEM102_WE2_INDEX], buff_cfg, accum_cfg);

        /* Recalculate ADC gain, after changing sample count */
        p_cem102->CalculateADCGain(CEM102_VOLTAGE_MEAS_CHAN,
                                   (uint32_t)sample_cnt + 1);
    }

    /* Default setting is for the APP_MEASURE_RESISTOR_VOLTAGE request */
    uint16_t cfg = CEM102_ANALOG_IO_ATBUS_CONNECT;

    if (CEM102_NTC_VDDA_CONNECTED(p_dev))
    {
        cfg |= CEM102_ANALOG_IO2_VSSA_CONNECT;
    }
    else
    {
        cfg |= CEM102_ANALOG_IO2_VDDA_CONNECT;
    }

    if (request == APP_MEASURE_VDDA_VOLTAGE)
    {
       if (CEM102_NTC_VDDA_CONNECTED(p_dev))
       {
           cfg |= CEM102_ANALOG_IO1_VDDA_CONNECT;
       }
       else
       {
           cfg |= CEM102_ANALOG_IO1_VSSA_CONNECT;
       }
    }
    if (result == ERRNO_NO_ERROR)
    {
        result = p_cem102->RE_ATBusSwitchConfig(p_dev, RE, cfg);
    }
    return result;
}

static void APP_CalculateTemperature(DRIVER_CEM102_t *p_cem102,
                                     const CEM102_Device *p_dev)
{
    APP_NotifyThermistorData(p_cem102, p_dev,
                             measure_values[APP_MEASURE_VDDA_VOLTAGE],
                             measure_values[APP_MEASURE_RESISTOR_VOLTAGE]);

    /* Thermistor part is over. Disconnect the IO1 & IO2 pins */
    APP_DisconnectIOInputs(p_cem102, p_dev);
}
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

static void CEM102_SetThreshViolDetection(bool ch1_enable, bool ch2_enable)
{
    bool flags[CEM102_NUM_LSADS];
    uint16_t mode[CEM102_NUM_LSADS] = { APP_LSAD1_VIOLATION_MODE,
                                        APP_LSAD2_VIOLATION_MODE
                                      };
    uint16_t mask[CEM102_NUM_LSADS] = { CHCFG_CH1_THRESHOLD_CH1_VIOLATION_CFG_MASK,
                                        CHCFG_CH2_THRESHOLD_CH2_VIOLATION_CFG_MASK
                                      };
    uint8_t reg[CEM102_NUM_LSADS] = { CHCFG_CH1_THRESHOLD,
                                      CHCFG_CH2_THRESHOLD
                                    };

    flags[CEM102_LSAD1_IDX] = ch1_enable;
    flags[CEM102_LSAD2_IDX] = ch2_enable;

    for (uint8_t idx = 0; idx < CEM102_NUM_LSADS; idx++)
    {
        if (CEM102_ThreshViolationEnabled(cem102_dut, idx))
        {
            /* Value for both channels are same */
            uint16_t val = flags[idx] ? mode[idx] : CH1_VIOLATION_CHECK_DISABLE;
            cem102->RegisterReadModifyWrite(cem102_dut, reg[idx], val, mask[idx]);
        }
    }
}

/* When chan is passed with RE, it is WE1 */
void CEM102_GetValueFromBuffer(uint32_t chan, uint32_t *p_data)
{
    if (p_data != NULL)
    {
        uint8_t lsb_index = APP_WE1_LOWER_HALF_WORD;
        uint8_t msb_index = APP_WE1_UPPER_HALF_WORD;

        if (WE2 == chan)
        {
            lsb_index = APP_WE2_LOWER_HALF_WORD;
            msb_index = APP_WE2_UPPER_HALF_WORD;
        }
        *p_data = (uint32_t)demo_measure_buf[msb_index];
        *p_data <<= CEM102_SHIFT_FOR_WORD;
        *p_data |= (uint32_t)demo_measure_buf[lsb_index];
    }
}

static void APP_GetThreshViolationState(const CEM102_Device *p_dev,
                                        bool flags[])
{
    for (uint8_t idx = 0; idx < CEM102_NUM_LSADS; idx++)
    {
        flags[idx] = false;
        if (CEM102_ThreshViolationEnabled(p_dev, idx))
        {
            flags[idx] = true;
        }
    }
}

static void APP_EnableThresholdViolation(DRIVER_CEM102_t *p_cem102,
                                         const CEM102_Device *p_dev)
{
    bool flags[CEM102_NUM_LSADS] = { false, false };

    APP_GetThreshViolationState(p_dev, flags);
    CEM102_SetThreshViolDetection(flags[CEM102_LSAD1_IDX],
                                  flags[CEM102_LSAD2_IDX]);
}

static void APP_ConfigCurrentThresholdViolation(const CEM102_Device *p_dev)
{
    /* In the right state to handle the interrupts. Enabling them. */
    uint16_t mask = SYSCTRL_IRQ_CFG_VDDA_ERR_IRQ_CFG_MASK |
                    SYSCTRL_IRQ_CFG_STATE_ERR_IRQ_CFG_MASK |
                    SYSCTRL_IRQ_CFG_DIGITAL_RESET_IRQ_CFG_MASK |
                    SYSCTRL_IRQ_CFG_CH1_VIOLATION_IRQ_CFG_MASK |
                    SYSCTRL_IRQ_CFG_CH2_VIOLATION_IRQ_CFG_MASK;
    uint16_t data = VDDA_ERR_IRQ_ENABLE | STATE_ERR_IRQ_ENABLE |
                    DIGITAL_RESET_IRQ_ENABLE;

    /* Enable the threshold violation detection, if enabled */
    bool flags[CEM102_NUM_LSADS] = { false, false };
    APP_GetThreshViolationState(p_dev, flags);

    data |= flags[CEM102_LSAD1_IDX] ? CH1_VIOLATION_IRQ_ENABLE :
                                      CH1_VIOLATION_IRQ_DISABLE;
    data |= flags[CEM102_LSAD2_IDX] ? CH2_VIOLATION_IRQ_ENABLE :
                                      CH2_VIOLATION_IRQ_DISABLE;

    APP_EnableThresholdViolation(cem102, p_dev);

    /* Ready to proess interrupts. Time to enable them. */
    cem102->RegisterReadModifyWrite(p_dev, SYSCTRL_IRQ_CFG,
                                    data, mask);
}

static void CEM102_SetCalibDone(void)
{
    calib_state = CALIB_DONE;
    demo_measure_status = MEASURE_INITIALIZED;

    if (APP_MeasuringCurrent())
    {
        APP_ConfigCurrentThresholdViolation(cem102_dut);
    }
    else
    {
        CEM102_SetThreshViolDetection(false, false);
    }
}

static void CEM102_Restart(DRIVER_CEM102_t *p_cem102, bool restart)
{
    /* Local variable definitions */
    int result = ERRNO_NO_ERROR;
    uint16_t fam = 0;

#ifdef SWMTRACE_OUTPUT
    uint16_t rev = 0;
    uint32_t repeat_count = 0;
#endif
    const CEM102_Device *p_dev = cem102_dut;

    if (restart)
    {
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("\rStopping the timer, if already running\r\n");
#endif /* SWMTRACE_OUTPUT */
        ke_timer_clear(CEM102_MEASUREMENT_START_TIMEOUT, TASK_APP);
        APP_ClearMeasureTimerEvent(p_dev, MEAS_TIMER_STARTED_RUNNING |
                                   MEAS_TIMER_OK_TO_RUN_INIT);

        /* We are here on error. Clear the state to mark that we are not
         * ready for measurement. */
        p_cem102->NotifyEvent(p_dev, false, CEM102_APP_EV_MEASURE_PHASE, NULL);
    }

    do
    {
        result = ERRNO_NO_ERROR;
        SYS_WATCHDOG_REFRESH();

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("Restart counter:%d\r\n", repeat_count);
        repeat_count++;
#endif /* SWMTRACE_OUTPUT */

        /* Wait 20ms after putting CEM102 to RESET */
        Sys_GPIO_Set_Low(cem102_dut->pwr_en);
        Sys_Delay(APP_SYS_DELAY_20MS);

        /* Initialize the CEM102 device */
        p_cem102->Initialize(cem102_dut);
        p_cem102->VDDCCharge(cem102_dut);
        result = p_cem102->PowerReset(cem102_dut);

        if (result == ERRNO_VCC_LOW_ERROR)
        {
            cem102_err_status |= CS0_CEM102_ERR_VCC_LOW;
        }
        else
        {
            result = p_cem102->PowerInit(cem102_dut);
        }

#if 1		// MAX30123 INITIAL
        max30123_power_on_ready();

        swmLogInfo("max30123_reg_initial\r\n");
        max30123_reg_initial();


#else

        if (result == ERRNO_NO_ERROR)
        {
            uint16_t temp = 0;

            Config_SendIdleCommand();

            p_cem102->RegisterRead(cem102_dut, SYS_STATUS, &temp);
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
            result = p_cem102->RegisterRead(cem102_dut, CHIP_FAMILY_VERSION, &fam);

            if ((result != ERRNO_NO_ERROR) || (0 == fam))
            {
                result = ERRNO_GENERAL_FAILURE;
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("\r\nFailed to read chip version\r\n");
#endif /* SWMTRACE_OUTPUT */
            }
            else
            {
#ifdef SWMTRACE_OUTPUT
                result = p_cem102->RegisterRead(cem102_dut, CHIP_REVISION, &rev);
                swmLogInfo("- Chip Revision: %X (%X)\r\n", fam, rev);
#endif /* SWMTRACE_OUTPUT */
            }
        }

        if (result == ERRNO_NO_ERROR)
        {
            /* Read OTP data */
            result = p_cem102->OTPRead(cem102_dut, &trims, INTERNAL_RC_CLK_FREQUENCY,
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
            result = p_cem102->DeviceTrim(cem102_dut, &trims);
        }
    } while (result != ERRNO_NO_ERROR);


#ifdef SWMTRACE_OUTPUT
    swmLogInfo("Measurement has been configured; enabling calibration.\r\n");
#endif /* SWMTRACE_OUTPUT */


    /* Enable CH1 & CH2 Completion IRQ */
    uint16_t temp = 0;
    result = p_cem102->RegisterRead(cem102_dut, SYSCTRL_IRQ_CFG, &temp);
    if (result == ERRNO_NO_ERROR)
    {
        temp |= (CH1_COMPLETION_IRQ_ENABLE |
                 CH2_COMPLETION_IRQ_ENABLE);
        p_cem102->IRQConfig(cem102_dut, temp);
    }

    /* Start calibration */
    calib_state = CALIB_INITIALIZED;
    demo_measure_status = MEASURE_INITIALIZED;

#if CEM102_DIAG_FUNCTIONS
    /* This will cause the diagnostics module to re-initialize */
    CEM102_SetLSADState(CEM102_LSAD_STATE_REINIT);
#endif /* CEM102_DIAG_FUNCTIONS */
}

/* Put 100ms Delay for the voltage to settle*/
static void CEM102_RestartWithDelay(DRIVER_CEM102_t *p_cem102, bool flag)
{
    Sys_Delay(APP_SYS_DELAY_100MS);
    CEM102_Restart(p_cem102, flag);
}

uint16_t CEM102_GetDACEnableBit(const CEM102_Device *p_dev, CEM102_DAC_TYPE dac)
{
    /* Value of ENABLE / DISBLE bit is same for all DACS. */
    uint16_t we_en = DAC_WE1_ENABLE;

    if (((dac == RE) && (!CEM102_RE_ENABLED(p_dev))) ||
        ((dac == WE2) && (!CEM102_WE2_ENABLED(p_dev))))
    {
        we_en = DAC_WE1_DISABLE;
    }
    return we_en;
}

void CEM102_Startup(DRIVER_CEM102_t *p_cem102)
{
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("\rCEM102_Startup\r\n");
#endif /* SWMTRACE_OUTPUT */
    CEM102_RestartWithDelay(p_cem102, false);
}

int CEM102_Calibrate(DRIVER_CEM102_t *p_cem102)
{
    uint32_t cal_measurement = 0;
    int result = ERRNO_NO_ERROR;
    uint16_t target_mv = APP_CALIB_DAC_TARGET_VOLTAGE;
    bool vcc_valid_data = false;
    const CEM102_Device *p_dev = cem102_dut;
    static uint32_t new_vcc_trim = 0;

    if (calib_state == CALIB_INITIALIZED)
    {
        CEM102_CalibrationData *p_data = CEM102_CALIB_DATA_ADDR;
        uint32_t stored_vcc_trim = 0;

        if (cem102_dut->pwr_cfg == LOW_VOLTAGE)
        {
            /* Discharge VDDA down to its trimmed value of 2.3 */
            p_cem102->VDDADischarge(cem102_dut, VDDA_DISCHARGE_TIME_MS);

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

            /* Empty flash will have 0xFF */
            if (p_data->vcc_trim != 0xFFFF)
            {
                stored_vcc_trim = p_data->vcc_trim &
                                        ACS_VCC_CTRL_VTRIM_Mask;
            }
            if ((!vcc_valid_data) || (stored_vcc_trim == 0))
            {
                /* Read the current VCC Trim */
                uint32_t vcc_trim = (ACS->VCC_CTRL & ACS_VCC_CTRL_VTRIM_Mask);

                /* Calibrate VCC to 1250 mV */
                p_cem102->CalibrateVCC(cem102_dut, vcc_trim, VCC_TYPICAL);

                new_vcc_trim = (ACS->VCC_CTRL & ACS_VCC_CTRL_VTRIM_Mask) >>
                                ACS_VCC_CTRL_VTRIM_Pos;

#ifdef SWMTRACE_OUTPUT
                swmLogInfo("VCC Calibrated to:%x\r\n", new_vcc_trim);
#endif /* SWMTRACE_OUTPUT */
            }
            else
            {
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("VCC Calibration trim found in the flash:%x\r\n",
                           stored_vcc_trim);
#endif /* SWMTRACE_OUTPUT */
                Sys_ACS_WriteRegister(&ACS->VCC_CTRL,
                                     (ACS->VCC_CTRL & ~ACS_VCC_CTRL_VTRIM_Mask)
                                     | stored_vcc_trim);
            }

        }

        /* Enable the VBAT VDDA monitor after VCC is calibrated */
        p_cem102->VBAT_VDDAMonitorStart(cem102_dut, WE1,
                                      SENSOR_THRESHOLD_MIN_THRESHOLD_MIN);

        CEM102_SetMeasurementDelayTimer(p_cem102, APP_LSAD_MEASURE_DELAY_48MS);

        /* If there is a valid calibration data stored in the flash, load that
         * data for channel configurations. If not, proceed with the
         * calibration.
         *
         * Comment out the following section, if you don't want to use the stored
         * values of the calibration data and run calibration every time
         * device is powered up/initialized.
         */
#if 0
        if (APP_CalibrationValid(p_cem102))
        {
            /* Copy the incoming calibration data */
            memcpy(&calib_data, p_data, CEM102_CALIB_DATA_SZ);

            channel_gain[CEM102_WE1_INDEX] = p_data->gain[CEM102_WE1_INDEX];
            channel_gain[CEM102_WE2_INDEX] = p_data->gain[CEM102_WE2_INDEX];
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("Valid Calibration data found in the flash.\r\n");
#endif /* SWMTRACE_OUTPUT */
            if ((!vcc_valid_data) || (stored_vcc_trim == 0))
            {
                calib_data.vcc_trim = new_vcc_trim;

                /* Save calibration data to the flash */
                result = p_cem102->CalibrationSave(&calib_data);
            }
            CEM102_SetCalibDone();

            /* Configure switches for the measurement */
            result = Config_PostCalibration();
        }
        else
#endif

        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("Start Calibration.\r\n");
#endif /* SWMTRACE_OUTPUT */
            demo_measure_status = MEASURE_MEASURING;
            calib_state = CALIB_REF_MEASURING;
            p_cem102->MeasureReference(cem102_dut, CEM102_REF_MEASUREMENT_CHAN);
        }
    }
    else if (calib_state == CALIB_REF_MEASURING)
    {
        APP_ReadMeasurement(p_cem102, RE);
    }
    else if (calib_state == CALIB_REF_READING)
    {
        /* Save the reference measurement */
        CEM102_GetValueFromBuffer(WE1, &calib_ref_measurement);
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("Start Calibration:WE1\r\n");
#endif /* SWMTRACE_OUTPUT */
        result = Config_ForMeasuring(WE1, target_mv);

        /* Now that we have our channel configuration, take a calibration
         * measurement
         */
        p_cem102->CalibrateChannel(cem102_dut, WE1);
        demo_measure_status = MEASURE_MEASURING;
        calib_state = CALIB_CH1_MEASURING;
    }
    else if (calib_state == CALIB_CH1_MEASURING)
    {
        APP_ReadMeasurement(p_cem102, WE1);
    }
    else if (calib_state == CALIB_CH1_READING)
    {
        CEM102_GetValueFromBuffer(WE1, &cal_measurement);

        p_cem102->CalculateChannelGain(cem102_dut, WE1, calib_ref_measurement,
                                     cal_measurement);
        calib_data.gain[WE1 - INDEX_ADJ_HELPER] = channel_gain[WE1 - INDEX_ADJ_HELPER];

        Config_PostMeasurement(WE1);

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("Start Calibration:WE2\r\n");
#endif /* SWMTRACE_OUTPUT */
        result = Config_ForMeasuring(WE2, target_mv);

        /* Now that we have our channel configuration, take a calibration
         * measurement
         */
        p_cem102->CalibrateChannel(cem102_dut, WE2);
        demo_measure_status = MEASURE_MEASURING;
        calib_state = CALIB_CH2_MEASURING;
    }
    else if (calib_state == CALIB_CH2_MEASURING)
    {
        APP_ReadMeasurement(p_cem102, WE2);
    }
    else if (calib_state == CALIB_CH2_READING)
    {
        /* During calibration, only one channel is enabled. Measurement
         * results from LSAD2 comes in first two words. Therefore the
         * function CEM102_GetValueFromBuffer is passed WE1.
         */
        CEM102_GetValueFromBuffer(WE1, &cal_measurement);

        p_cem102->CalculateChannelGain(cem102_dut, WE2, calib_ref_measurement,
                                     cal_measurement);
        calib_data.gain[WE2 - INDEX_ADJ_HELPER] = channel_gain[WE2 - INDEX_ADJ_HELPER];

        result = Config_PostMeasurement(WE2);
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("ChannelGain_2: 0x%X, 0x%X, 0x%X\r\n",
                   (int32_t)channel_gain[1], calib_ref_measurement,
                   cal_measurement);
#endif /* SWMTRACE_OUTPUT */

        calib_data.vcc_trim = new_vcc_trim;

        uint16_t dac_v[CEM102_MAX_DACS] =
        {
            WE1_DAC_V_USER_SETTING,
            WE2_DAC_V_USER_SETTING,
            RE_DAC_V_USER_SETTING,
        };

        /* Calibration over. Save dac setting for 500 mv target voltage */
        int dac_error = ERRNO_NO_ERROR;
        CEM102_DAC_TYPE dac = WE1;
        uint8_t index = 0;
        uint16_t regval = 0;

        for (dac = WE1; dac <= RE; dac++)
        {
            index = (uint8_t)dac - INDEX_ADJ_HELPER;
            dac_error = DAC_GetCalibratedSetting(dac,
                                                 dac_v[index],
                                                 &regval);
            if (dac_error != ERRNO_NO_ERROR)
            {
                break;
            }
            calib_data.dac_setting[index] = regval;
            calib_data.dac_v[index] = dac_v[index];
        }

        /* Save calibration data to the flash */
        if (dac_error == ERRNO_NO_ERROR)
        {
            result = cem102->CalibrationSave(&calib_data);
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("Calibration data saved in persistent memory\r\n");
#endif /* SWMTRACE_OUTPUT */
        }

        CEM102_SetCalibDone();

        uint16_t mask = 0;
        regval = 0;
        if (dac_error == ERRNO_NO_ERROR)
        {
            /* When USE_LEGACY_WE_SET_CALIBRATION is set to 0, calibration
             * sets the DAC to 1200 mV. Setting all the DACs to user setting.
             */
            for (dac = WE1; dac <= RE; dac++)
            {
                index = dac - INDEX_ADJ_HELPER;

                uint16_t we_en = CEM102_GetDACEnableBit(cem102_dut, dac);

                /* Value of the mask is same for all DACs */
                mask = DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK |
                       DC_DAC_WE1_CTRL_DAC_WE1_CFG_MASK;
                regval = calib_data.dac_setting[index] | we_en;

                uint8_t dac_ctrl[CEM102_MAX_DACS] =
                {
                    DC_DAC_WE1_CTRL,
                    DC_DAC_WE2_CTRL,
                    DC_DAC_RE_CTRL,
                };
                p_cem102->RegisterReadModifyWrite(cem102_dut,
                                                dac_ctrl[index], regval,
                                                mask);
            }
        }

        /* Calibration leaves these switches connected. Disconnecting
         * will improve the residual current measurements. */
        regval = ATBUS_SA14_DISCONNECT | ATBUS_SA7_DISCONNECT;
        mask = ANA_SW_CFG2_ATBUS_SA14_CFG_MASK |
                        ANA_SW_CFG2_ATBUS_SA7_CFG_MASK;
        p_cem102->RegisterReadModifyWrite(cem102_dut, ANA_SW_CFG2,
                                          regval, mask);
        mask = 1U << ANA_SW_CFG0_RE_SR1_CFG_POS;
        regval = CEM102_SW_SR1_POSITION(p_dev);
        p_cem102->RegisterReadModifyWrite(p_dev, ANA_SW_CFG0, regval, mask);

        /* After calibration, RE BUF is disabled */
        if (CEM102_RE_ENABLED(p_dev))
        {
            regval = RE_BUF_ENABLE;
            mask = 1U << ANA_CFG1_RE_BUF_CFG_POS;
            p_cem102->RegisterReadModifyWrite(p_dev, ANA_CFG1, regval, mask);
        }
#if APP_CEM102_FAST_CALIBRATION
        for (uint8_t idx = 0; idx < MAX_WE_CHANNEL_NUM; idx++)
        {
            regval = cem102_dut->lsad_cfg_user[idx];
            mask = 0;
            p_cem102->GetLSADConfig(cem102_dut, &regval, &mask);
            uint8_t reg = CHCFG_CH1_LSAD + (idx * (CHCFG_CH2_THRESHOLD -
                                            CHCFG_CH1_THRESHOLD));
            p_cem102->RegisterReadModifyWrite(cem102_dut, reg, regval, mask);
        }
#endif /* APP_CEM102_FAST_CALIBRATION */
        if (!APP_DiagMeasuresVoltage(cem102_dut))
        {
            APP_ConfigCurrentThresholdViolation(cem102_dut);
        }
    }
    return result;
}

static bool CEM102_ReadFromQueue(DRIVER_CEM102_t *p_cem102,
                                 uint16_t cem102_status,
                                 uint16_t ch_completion_mask)
{
    bool stat = false;

    if ((cem102_status & ch_completion_mask) == ch_completion_mask)
    {
        stat = true;
        demo_measure_status = MEASURE_READING_RESULTS;
        int result = p_cem102->QueueRead(cem102_dut, SYS_BUFFER_WORD0,
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

static bool CEM1020_ReadMeasurementData(DRIVER_CEM102_t *p_cem102,
                                        uint16_t cem102_status)
{
    uint16_t ch_completion_mask = CH2_COMPLETION;

    /* If measuring voltage, only check that channel 2 is done.
     * Otherwise, check that both channels are done.
     */
    if (APP_MeasuringCurrent())
    {
        if (!CEM102_WE2_ENABLED(cem102_dut))
        {
            ch_completion_mask &= ~CH2_COMPLETION;
        }
        ch_completion_mask |= CH1_COMPLETION;
    }
    return CEM102_ReadFromQueue(p_cem102, cem102_status, ch_completion_mask);
}

#if APP_MEASURE_ACROSS_IO_INPUTS
static void CEM102_CalculateVoltage(DRIVER_CEM102_t *p_cem102,
                                    int32_t *p_we2_voltage)
{
    int32_t we2_result = 0;

    /* During voltage measurement, one channel, LSAD2 is enabled.
     * Results from the measurement comes in first two words. Therefore the
     * function CEM102_GetValueFromBuffer is passed with WE1. CalculateVoltage
     * HAL API uses ADC gain. It requires correct channel ID, which is WE2.
     */
    CEM102_GetValueFromBuffer(WE1, (uint32_t *)&we2_result);
    if (we2_result < 0)
    {
        we2_result *= -1;
    }
    p_cem102->CalculateVoltage(CEM102_VOLTAGE_MEAS_CHAN, we2_result,
                               p_we2_voltage);
}

static void CEM102_NextVoltageMeasurement(void)
{
    /* Measured data is handled. Configure the switches to the
     * next measurement */
    voltage_id++;

    int status = APP_SetupATBusSwitch(cem102, cem102_dut, voltage_id);
    if (status != ERRNO_NO_ERROR)
    {
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("ERROR: Failed to switch IO measurement.\r\n");
#endif /* SWMTRACE_OUTPUT */
    }
    demo_measure_status = MEASURE_MEASURING;
    APP_SystemCommand(cem102);
}

static void CEM102_SwitchToVoltageMeasurement(DRIVER_CEM102_t *p_cem102)
{
    /* Reconfigure to measure voltage */
    APP_SetMeasureAttribute(APP_MEASURING_VOLTAGE);
    voltage_id = APP_MEASURE_VOLTAGE_TYPE_FIRST;

    int status = APP_SetupATBusSwitch(p_cem102, cem102_dut, voltage_id);

    CEM102_SetThreshViolDetection(false, false);
    demo_measure_status = MEASURE_MEASURING;

    if (status != ERRNO_NO_ERROR)
    {
#ifdef SWMTRACE_OUTPUT
        swmLogInfo("ERROR: Failed to start IO measurements.\r\n");
#endif /* SWMTRACE_OUTPUT */
    }
    APP_SystemCommand(p_cem102);
}
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

/* Called when all three voltage measurements are done */
static void CEM102_SwitchToCurrMeasurement(DRIVER_CEM102_t *p_cem102,
                                           const CEM102_Device *p_dev)
{
#if APP_MEASURE_ACROSS_IO_INPUTS || CEM102_DIAG_FUNCTIONS
    /* Reconfigure to measure current */
    APP_SetMeasureAttribute(APP_MEASURING_CURRENT);
    Config_SendIdleCommand();

    uint16_t reg = p_dev->lsad_prescale_curr;
    uint16_t mask =  CHCFG_LSAD_CLK_LSAD_CLK_PRESCALE_MASK;
    p_cem102->RegisterReadModifyWrite(p_dev, CHCFG_LSAD_CLK, reg, mask);

#if APP_MEASURE_ACROSS_IO_INPUTS

    /* Reset the voltage ID and the age of the measurement */
    voltage_id = APP_MEASURE_VOLTAGE_TYPE_FIRST;

    /* Reset the AT Bus switches to their disconnected state. */
    p_cem102->AIOConfigDeinit(p_dev, CEM102_VOLTAGE_MEAS_CHAN,
                              &saved_regs);

    demo_measure_status = MEASURE_MEASURING;
#else /* APP_MEASURE_ACROSS_IO_INPUTS */

    /* Diagnostics may have disabled LSAD. */
    CEM102_ChannelCtrl(p_cem102, WE2, true);
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
    if (APP_MeasuringCurrent())
    {
        APP_ConfigCurrentThresholdViolation(p_dev);
    }

    CEM102_ChannelCtrl(p_cem102, WE1, true);
    APP_SystemCommand(p_cem102);
#endif /* APP_MEASURE_ACROSS_IO_INPUTS || CEM102_DIAG_FUNCTIONS */
}

static void CEM102_CalculateCurrent(float *p_we1_current, float *p_we2_current)
{
    CEM102_DAC_TYPE dacs[MAX_WE_CHANNEL_NUM] = { WE1, WE2 };
    int32_t we_result[MAX_WE_CHANNEL_NUM] = { 0 };
    float curr[MAX_WE_CHANNEL_NUM] = { 0 };
    uint8_t count = (CEM102_WE1_WE2_ENABLED(cem102_dut)) ?
                     MAX_WE_CHANNEL_NUM : 1;

    for (uint8_t i = 0; i < count; i++)
    {
        CEM102_GetValueFromBuffer(dacs[i], (uint32_t *)&we_result[i]);
        cem102->CalculateCurrent(dacs[i], we_result[i], &curr[i]);
    }
    *p_we1_current = curr[CEM102_WE1_INDEX];
    *p_we2_current = curr[CEM102_WE2_INDEX];
}

static void CEM102_UpdateCurrentAndVBAT(void)
{
    float WE1_cur = 0;
    float WE2_cur = 0;

    CEM102_CalculateCurrent(&WE1_cur, &WE2_cur);

    APP_NotifyCurrentData(cem102, cem102_dut, WE1_cur, WE2_cur);
}

static void CEM102_ProcessMeasurementData(DRIVER_CEM102_t *p_cem102)
{
    const CEM102_Device *p_dev = cem102_dut;

#if APP_MEASURE_ACROSS_IO_INPUTS
    if (APP_MeasuringVoltage())
    {
        CEM102_CalculateVoltage(p_cem102, &measure_values[voltage_id]);
    }
    else
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
    if (APP_MeasuringCurrent())
    {
        /* What to do after current set of measurements are done */
        CEM102_ControlNextIteration(p_cem102, p_dev);

        CEM102_UpdateCurrentAndVBAT();

        /* With current measurement, which is last in the sequence, done,
         * current set of measurements is over. Measurement timer will
         * kickstart the next set of measurements.
         */
        demo_measure_status = MEASURE_IDLE;
    }

    /* About to recognize the threshold violation event. Clear it */
    if (APP_CEM102EventValid(APP_EVENT_THRESH_VIOLATION))
    {
        APP_ClearCEM102Event(APP_EVENT_THRESH_VIOLATION);
    }

    APP_NotifyThresholdViolation(p_cem102, p_dev,
                                 ch1_violation_count,
                                 ch2_violation_count);
    ch1_violation_count = 0;
    ch2_violation_count = 0;
}

static void CEM102_PrepNextMeasurement(DRIVER_CEM102_t *p_cem102)
{
#if (CEM102_VDDA_ERR_OPERATION == VDDA_ERR_RECOVERY_MODE)
    /* If VDDA_ERR happened, clear the flag and re-enable VDDA_ERR IRQ */
    if (irq_valid_flag)
    {
        irq_valid_flag = 0;
        p_cem102->IRQConfig(cem102_dut, irq_cfg);
    }
#endif /* (CEM102_VDDA_ERR_OPERATION == VDDA_ERR_RECOVERY_MODE) */

    if ((SENSOR->THRESHOLD_MIN & SENSOR_THRESHOLD_MIN_ENABLED) == 0)
    {
        SENSOR->THRESHOLD_MIN |= SENSOR_THRESHOLD_MIN_ENABLED;
    }

#if APP_MEASURE_ACROSS_IO_INPUTS
    demo_measure_status = MEASURE_MEASURING;

    /* Update the measurement configuration */
    if (APP_MeasuringVoltage())
    {
        /* If the last voltage for thermistor application has been measured,
         * calculate the temperature and switch to next measurement. */
        if (voltage_id == APP_MEASURE_VOLTAGE_TYPE_LAST)
        {
            APP_CalculateTemperature(p_cem102, cem102_dut);
            if (!APP_StartMeasurementWithDiag(p_cem102, cem102_dut))
            {
                CEM102_SwitchToCurrMeasurement(p_cem102, cem102_dut);
            }
        }
        else
        {
            CEM102_NextVoltageMeasurement();
        }
    }
    else
    {
        CEM102_SwitchToVoltageMeasurement(p_cem102);
    }
#else /* APP_MEASURE_ACROSS_IO_INPUTS */
    if (demo_measure_status == MEASURE_IDLE)
    {
        demo_measure_status = MEASURE_MEASURING;

        /* If there is no measurement done inside diag module,
         * kick start the measurement here.
         */
        if (!APP_StartMeasurementWithDiag(p_cem102, cem102_dut))
        {
            APP_ConfigCurrentThresholdViolation(cem102_dut);
            APP_SystemCommand(p_cem102);
        }
    }
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */
}

static void CEM102_ChanViolationHandler(DRIVER_CEM102_t *p_cem102)
{
    uint16_t v_count = 0;

    /* Read the violation count */
    p_cem102->RegisterRead(cem102_dut, SYS_BUFFER_VIOL_CNT, &v_count);

    /* If violation persists, interrupt will persisist too. MCU will
     * be busy servicing the interrupt. Disable threshold detection for
     * the channel that indicated the violation. */
    CEM102_SetThreshViolDetection(false, false);

    /* Clear the violation and restart the measurement */
    demo_measure_status = MEASURE_MEASURING;
    APP_SystemCommand(p_cem102);

    ch1_violation_count += (v_count &
                            SYS_BUFFER_VIOL_CNT_CH1_VIOLATION_CNT_MASK) >>
                            SYS_BUFFER_VIOL_CNT_CH1_VIOLATION_CNT_POS;
    ch2_violation_count += (v_count &
                            SYS_BUFFER_VIOL_CNT_CH2_VIOLATION_CNT_MASK) >>
                            SYS_BUFFER_VIOL_CNT_CH2_VIOLATION_CNT_POS;
    APP_SetCEM102Event(APP_EVENT_THRESH_VIOLATION);
}

/* Will run if meas_timer_interval is initialized with non-zero value */
void CEM102_MeasurementTimerHandler(void)
{
    /* Timer event received. Time to start next set of measurements */
    if (demo_measure_status == MEASURE_IDLE)
    {
        CEM102_SetupMeasurementCycle(cem102, cem102_dut);
        CEM102_PrepNextMeasurement(cem102);
    }
    APP_StartTheMeasurementTimer(cem102_dut);
}

static void APP_HandleDiagAPI(DRIVER_CEM102_t *p_cem102,
                              const CEM102_Device *p_dev)
{
    if (APP_CEM102EventValid(APP_EVENT_FROM_CEM102))
    {
        APP_ClearCEM102Event(APP_EVENT_FROM_CEM102);
        CEM102_DiagnosticsAPI(p_cem102, p_dev);
    }
    if (CEM102_DiagnosticsAPIDone(p_cem102, p_dev))
    {
        CEM102_SwitchToCurrMeasurement(p_cem102, p_dev);
    }
}

/* When all the timer related events are received, timer will be started,
 * if not running already.
 */
void APP_SetMeasureTimerEvent(const CEM102_Device *p_dev, uint8_t ev)
{
    ev &= MEAS_TIMER_OK_TO_RUN_MASK;
    meas_timer_ok_to_run |= ev;

    /* Some events are not needed on certain conditions */
    if (!APP_DiagMeasuresVoltage(p_dev))
    {
        meas_timer_ok_to_run |= MEAS_TIMER_OK_TO_RUN_DIAGAPI;
    }
#if !CEM102_DIAG_FUNCTIONS
    meas_timer_ok_to_run |= MEAS_TIMER_OK_TO_RUN_DIAGAPI;
    meas_timer_ok_to_run |= MEAS_TIMER_OK_TO_RUN_DIAGNOSTICS;
#endif
    uint8_t flag = (meas_timer_ok_to_run & MEAS_TIMER_OK_TO_RUN_MASK);
    if ((flag == MEAS_TIMER_OK_TO_RUN_MASK) &&
        ((meas_timer_ok_to_run & MEAS_TIMER_STARTED_RUNNING) == 0))
    {
        meas_timer_ok_to_run |= MEAS_TIMER_STARTED_RUNNING;
        APP_StartTheMeasurementTimer(p_dev);
    }
}

void CEM102_Operation(DRIVER_CEM102_t *p_cem102)
{
    static uint16_t saved_sys_status = 0;
    const CEM102_Device *p_dev = cem102_dut;

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
        int result = p_cem102->RegisterRead(cem102_dut, CHIP_FAMILY_VERSION,
                                            &fam);
        if (ERRNO_NO_ERROR != result)
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("\rError while reading chip version\r\n");
#endif /* SWMTRACE_OUTPUT */
        }
        else if (0 == fam)
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("\r\nreading chip version returned 0\r\n");
#endif /* SWMTRACE_OUTPUT */
        }

#ifdef SWMTRACE_OUTPUT
        swmLogInfo("Restaring CEM102...\r\n");
#endif /* SWMTRACE_OUTPUT */
        wakeup_irq = 0;

        /* Disable sensor so it does not trigger false VBAT threshold
         * during re-start up. Sensor will be reconfigured during
         * CEM102_Startup.
         */
        Sys_Sensor_Disable();

        demo_measure_status = MEASURE_IDLE;
        calib_state = CALIB_NOT_STARTED;

        /* Delay before restarting to allow the system to settle */
        CEM102_RestartWithDelay(p_cem102, true);
    }


    /* If the system has just completed initialization, start the first
     * measurement, after running the diagnostics, if enabled.
     */
    if (demo_measure_status == MEASURE_INITIALIZED &&
        (calib_state == CALIB_DONE) &&
        CEM102_DiagnosticsDone(p_cem102))
    {
        wakeup_irq = 0;

        int status = ERRNO_NO_ERROR;

        /* We are ready for measurement. Let the HAL layer know */
        p_cem102->NotifyEvent(p_dev, true, CEM102_APP_EV_MEASURE_PHASE, NULL);

        if (!CEM102_WE2_ENABLED(p_dev))
        {
            p_cem102->SwitchConfig(p_dev, WE2, 0);
        }
        for (uint8_t idx = 0; idx < CEM102_NUM_LSADS; idx++)
        {
            /* Initialize LSAD/BUFFER for current configuration, before
             * switching to voltage configuration */
            APP_UpdateChannelConfig(p_cem102, p_dev, idx, false, false);
        }
#if APP_MEASURE_ACROSS_IO_INPUTS
        /* Set the starting attribute to measure to be the voltage via IO */
        APP_SetMeasureAttribute(APP_MEASURING_VOLTAGE);

        /* Initialize the switches and ADC for the measurement.
         *
         * AIOConfigInit API uses CEM102_ACCUM_SAMPLE_CNT_2. ADC gain is
         * Calculated inside the AIOConfigInit
         *
         * For IO measurements, CEM102_VOLTAGE_MEAS_CHAN (WE2) channel is used.
         */
        p_cem102->AIOConfigInit(cem102_dut, CEM102_VOLTAGE_MEAS_CHAN,
                                &saved_regs);

        /* Setup the Test bus' switches correctly for the first measurement */
        voltage_id = APP_MEASURE_VOLTAGE_TYPE_FIRST;
        APP_SetupATBusSwitch(p_cem102, cem102_dut, voltage_id);
#endif
        if (!CEM102_WE2_ENABLED(cem102_dut))
        {
            /* Mask value is same for all DACS */
            p_cem102->RegisterReadModifyWrite(p_dev,
                                              DC_DAC_WE2_CTRL,
                                              DAC_WE2_DISABLE,
                                              DC_DAC_WE1_CTRL_DAC_WE1_CFG_MASK);

            /* Voltage measurements require LSAD2. Current measurement
             * can have LSAD2 enabled, if configuration allows
             */
            APP_UpdateChannelConfig(cem102, p_dev, CEM102_LSAD2_IDX,
                                    APP_CurrentOnlyMeasurement(p_dev) ?
                                    false : true,
                                    APP_CurrentOnlyMeasurement(p_dev) ?
                                    false : true);
            if (APP_CurrentOnlyMeasurement(cem102_dut))
            {
                CEM102_ConfigureLSAD(p_cem102, p_dev, WE2, false);
            }
        }
        if (!CEM102_RE_ENABLED(cem102_dut))
        {
            p_cem102->RegisterReadModifyWrite(p_dev,
                                              DC_DAC_RE_CTRL,
                                              DAC_RE_DISABLE,
                                              DC_DAC_RE_CTRL_DAC_RE_CFG_MASK);
        }
#if !APP_MEASURE_ACROSS_IO_INPUTS
        /* After calibration, LSAD2 is enabled, while LSAD1 is disabled */
        if (!APP_StartMeasurementWithDiag(p_cem102, p_dev))
        {
            /* By default, it is set to measure voltage */
            APP_SetMeasureAttribute(APP_MEASURING_CURRENT);

            /* At this stage, LSAD2 is enabled, LSAD1 may be disabled
             * due to diagnostics.
             */
            CEM102_ChannelCtrl(p_cem102, WE1, true);
        }
#endif /* NOT APP_MEASURE_ACROSS_IO_INPUTS */
        if (status == ERRNO_NO_ERROR)
        {
            demo_measure_status = MEASURE_MEASURING;

            APP_SetMeasureTimerEvent(cem102_dut, MEAS_TIMER_OK_TO_RUN_INIT);
            if (calib_ref_measurement != 0)
            {
                APP_SetMeasureTimerEvent(cem102_dut, MEAS_TIMER_OK_TO_RUN_DIAGAPI);
            }
            if (!APP_MeasuringWithDiag())
            {
                APP_SystemCommand(p_cem102);
            }
            p_cem102->RegisterRead(cem102_dut, SYS_STATUS,
                                 &saved_sys_status);
        }
#ifdef SWMTRACE_OUTPUT
        else
        {
            swmLogInfo("ERROR: Failed to start a measurement.\r\n");
        }
#endif /* SWMTRACE_OUTPUT */
    }
    else if (calib_state == CALIB_DONE)
    {
        int result = ERRNO_NO_ERROR;

        /* If a CEM102 IRQ occurred, read the CEM102 status */
        if (wakeup_irq)
        {
            uint16_t cem102_status = 0;
            wakeup_irq = 0;
            APP_SetCEM102Event(APP_EVENT_FROM_CEM102);
            result = p_cem102->RegisterRead(cem102_dut, SYS_STATUS,
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
            result = p_cem102->RegisterRead(cem102_dut, SYSCTRL_IRQ_CFG,
                                          &irq_cfg);
            if (result == ERRNO_NO_ERROR)
            {
                /* Disable Digital Reset IRQ */
                result = p_cem102->IRQConfig(cem102_dut, irq_cfg &
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

            demo_measure_status = MEASURE_IDLE;
            calib_state = CALIB_NOT_STARTED;

            /* Delay before restarting to allow the system to settle */
            CEM102_RestartWithDelay(p_cem102, true);
        }
        else if ((saved_sys_status & STATE_ERR) == STATE_ERR)
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("ERROR: State Error. Restarting CEM102...\r\n");
#endif /* SWMTRACE_OUTPUT */
            saved_sys_status = 0;
            demo_measure_status = MEASURE_IDLE;
            calib_state = CALIB_NOT_STARTED;

            /* State Error occurred, re-initialize the CEM102. */
            CEM102_Restart(p_cem102, true);
        }
        else if ((saved_sys_status & VDDA_ERR) == VDDA_ERR)
        {
            saved_sys_status &= ~VDDA_ERR;

            /* To avoid handler run again before previous call completes */
            if (!vdda_err_handled)
            {
                vdda_err_handled = true;
                VDDA_ERR_Handler();
                vdda_err_handled = false;
            }

            /* If VDDA_ERR_Handler returns, that means RECOVERY is defined */
            p_cem102->RegisterRead(cem102_dut, SYS_STATUS,
                                 &saved_sys_status);
        }
        else if (saved_sys_status & (CH1_VIOLATION|CH2_VIOLATION))
        {
            CEM102_ChanViolationHandler(p_cem102);
            p_cem102->RegisterRead(cem102_dut, SYS_STATUS,
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

            if (!CEM102_DiagnosticsDone(p_cem102))
            {
                if (APP_CEM102EventValid(APP_EVENT_FROM_CEM102))
                {
                    APP_ClearCEM102Event(APP_EVENT_FROM_CEM102);
                    CEM102_Diagnostics(p_cem102);
                }
            }
            else if (APP_MeasuringWithDiag())
            {
                APP_HandleDiagAPI(p_cem102, cem102_dut);
            }
            else
            {
                /* Check if the next results can be read */
                if ((demo_measure_status != MEASURE_READING_RESULTS) &&
                    (demo_measure_status != MEASURE_RESULTS_READY))
                {
                    if ((CEM102_ValidStateToMeasure(saved_sys_status)) &&
                        (APP_CEM102EventValid(APP_EVENT_FROM_CEM102)))
                    {
                        APP_ClearCEM102Event(APP_EVENT_FROM_CEM102);
                        if (CEM1020_ReadMeasurementData(p_cem102,
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
                 * variable, cem102_status may be 0, when demo_measure_status is
                 * set to MEASURE_RESULTS_READY. It is better to use the
                 * SYS STATE value that is saved during the previous
                 * interrupt.
                 */
                uint16_t cem102_state = saved_sys_status &
                                        SYS_STATUS_SYS_STATE_MASK;
                if (((cem102_state == CONTINUOUS_STATE) ||
                    (cem102_state == IDLE_STATE)) &&
                    (demo_measure_status == MEASURE_RESULTS_READY))
                {
                    SYS_WATCHDOG_REFRESH();

                    CEM102_ProcessMeasurementData(p_cem102);
                    if ((next_measurement_iteration) ||
                        (demo_measure_status != MEASURE_IDLE))
                    {
                        next_measurement_iteration = false;
                        CEM102_PrepNextMeasurement(p_cem102);
                    }
                }
            }
        }
    }
}

