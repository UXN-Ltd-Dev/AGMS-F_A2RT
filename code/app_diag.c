/**
 * @file  app_diag.c
 * @brief CEM102 diagnostic functionality related source file.
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

#if CEM102_DIAG_FUNCTIONS
typedef struct
{
    bool regs_saved;
    bool setup_changes_states;
    uint16_t diagreq;
    uint16_t diagreq_saved;
    CEM102_LSAD_STATE lsad_state;
    uint16_t apireq;
    CEM102_RegSaveData saved_data;
    CEM102_RegSaveData *p_saved_data;
    uint16_t sample_cnt[MAX_WE_CHANNEL_NUM];
}CEM102_DiagData;

static CEM102_DiagData diagData;

static int CEM102_StartMeasurement(DRIVER_CEM102_t *p_cem102, uint16_t cmd)
{
    CEM102_SetLSADState(CEM102_LSAD_STATE_MEAS_STARTED);

    cmd |= SYSCTRL_CMD_ALL_ERR_MASK;
    return p_cem102->StartMeasurement(cem102_dut, cmd);
}

static int CEM102_Enable_DAC_LOAD_IRQ(DRIVER_CEM102_t *p_cem102, bool enable)
{
    uint16_t irq_cfg = DC_DAC_LOAD_IRQ_DISABLE;

    if (enable)
    {
        irq_cfg = DC_DAC_LOAD_IRQ_ENABLE;
    }
    uint16_t irq_mask = SYSCTRL_IRQ_CFG_DC_DAC_LOAD_IRQ_CFG_MASK;
    int ret = p_cem102->RegisterReadModifyWrite(cem102_dut, SYSCTRL_IRQ_CFG,
                                                irq_cfg, irq_mask);
    return ret;
}

static int CEM102_SendCommand(DRIVER_CEM102_t *p_cem102, uint16_t cmd)
{
    return p_cem102->SystemCommand(cem102_dut,
                                SYSCTRL_CMD_ALL_ERR_MASK | cmd);
}

static void CEM102_DisableREDAC(DRIVER_CEM102_t *p_cem102,
                                const CEM102_Device *p_dev,
                                uint16_t sw_cfg, uint16_t buf_cfg)
{
    uint16_t val = 0;

    /* We want to retain the DAC settings, when disabling */
    p_cem102->RegisterRead(p_dev, DC_DAC_RE_CTRL, &val);

    val &= DC_DAC_RE_CTRL_DAC_RE_VALUE_MASK;
    val |= DAC_RE_DISABLE;

    /* Disable RE channel, SR switches, and HP mode, RE_BUFFER */
    p_cem102->REDACConfig(p_dev, val, sw_cfg, buf_cfg);
}

static CEM102_DAC_TYPE CEM102_GetChanNumber(CEM102_LSAD_READ_TYPE lsad_req)
{
    CEM102_DAC_TYPE chan = WE1;

    if (CEM102_LSAD_READ_WE2 == lsad_req)
    {
        chan = WE2;
    }
    else if (CEM102_LSAD_READ_RE == lsad_req)
    {
        chan = RE;
    }
    return chan;
}

static void CEM102_AdjustADCGain(DRIVER_CEM102_t *p_cem102,
                                 CEM102_CurrentData *p_data)
{
    int32_t chan = WE1;
    int8_t  reg = CHCFG_CH1_ACCUM;

    if (p_data->lsad_req == CEM102_LSAD_READ_WE2)
    {
        reg = CHCFG_CH2_ACCUM;
        chan = WE2;
    }

    uint16_t reg_val = 0;
    int result = p_cem102->RegisterRead(cem102_dut, reg, &reg_val);
    if (result == ERRNO_NO_ERROR)
    {
        reg_val &= CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_MASK;
        p_cem102->CalculateADCGain(chan, reg_val+1);
    }
}

static uint16_t CEM102_GetClearCommand(CEM102_LSAD_READ_TYPE lsad_req)
{
    uint16_t clr_cmd = CH1_COMPLETION_CLR_CMD | CH2_COMPLETION_CLR_CMD;
    switch (lsad_req)
    {
        case CEM102_LSAD_READ_WE1:
        {
            clr_cmd &= ~(CH2_COMPLETION_CLR_CMD);
            break;
        }
        case CEM102_LSAD_READ_WE2:
        {
            clr_cmd &= ~(CH1_COMPLETION_CLR_CMD);
            break;
        }
        default: /* CEM102_LSAD_READ_WE1_WE2 */
        {
            break;
        }
    }
    return clr_cmd;
}
static uint16_t CEM102_GetSysStatusMask(CEM102_LSAD_READ_TYPE lsad_req)
{
    uint16_t mask = CH1_COMPLETION | CH2_COMPLETION;
    switch (lsad_req)
    {
        case CEM102_LSAD_READ_WE1:
        case CEM102_LSAD_READ_RE:   /* LSAD-1 is used for RE DAC */
        {
            mask &= ~(CH2_COMPLETION);
            break;
        }
        case CEM102_LSAD_READ_WE2:
        {
            mask &= ~(CH1_COMPLETION);
            break;
        }
        default: /* CEM102_LSAD_READ_WE1_WE2 */
        {
            break;
        }
    }
    return mask;
}

static void CEM102_ResultDACVoltage(DRIVER_CEM102_t *p_cem102,
                                    CEM102_CurrentData *p_data,
                                    uint16_t diagmask)
{
    uint32_t chan = WE1;  /* WE1 for RE */
    CEM102_DAC_TYPE dac = CEM102_GetChanNumber(p_data->lsad_req);

    if (WE2 == dac)
    {
        chan = WE2;
    }
    int32_t lsad_val = 0;

    const CEM102_Device *p_dev = cem102_dut;
    p_cem102->RE_ATBusSwitchConfig(p_dev, RE,  0);

    /* Only one LSAD is enabled at any given point of time.
     * As a result, Pass WE1 to this function all the time.
     */
    CEM102_GetValueFromBuffer(WE1, (uint32_t *)&lsad_val);

    int32_t v_measured = 0;
    p_cem102->CalculateVoltage(chan, lsad_val, &v_measured);

    if (v_measured < 0)
    {
        v_measured *= -1;
    }

    int32_t v_dac = APP_VOLTAGE_MV_TO_UV * p_data->target_mv;
    int32_t v_diff = 0;
    if (v_measured <= v_dac)
    {
        v_diff = v_dac - v_measured;
    }
    else
    {
        v_diff = v_measured - v_dac;
    }

    /* Now, v_diff has a difference, not the actual measurement. */
    if (WE2 == p_data->target_dac)
    {
        cem102_diag_func_char.WE2_DAC_volt = v_diff;
    } else if (RE == p_data->target_dac)
    {
        cem102_diag_func_char.RE_DAC_volt = v_diff;
    }
    else /* WE1 */
    {
        cem102_diag_func_char.WE1_DAC_volt = v_diff;
    }

    CEM102_DiagResult result;
    result.dac = p_data->target_dac;
    result.voltage = v_measured;
    result.diagmask = diagmask;
    result.in_diag = (diagData.apireq == 0) ? true : false;
    APP_NotifyDiagResult(p_cem102, p_dev, &result);
}

static void CEM102_PrintCurrent(CEM102_CurrentData *p_data, const char *str,
                                CEM102_DAC_TYPE dac, float curr)
{
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("\r%s LSAD%d (%s):, %f\r\n", str, dac,
               (p_data->div_factor == APP_FEMTO_PICO_COV_FACTOR) ? "pA" :
               ((p_data->div_factor == APP_FEMTO_NANO_COV_FACTOR) ?
               "nA" : "fA"), curr);
#endif /* SWMTRACE_OUTPUT */
}

/* This function returns value in pA */
static void CEM102_ResultCalculateCurrent(DRIVER_CEM102_t *p_cem102,
                                         CEM102_CurrentData *p_data,
                                         const char *str, float *p_fvalue,
                                         uint16_t diagmask)
{
    uint32_t chan = WE1;
    CEM102_DAC_TYPE dac = CEM102_GetChanNumber(p_data->lsad_req);

    if (WE2 == dac)
    {
        chan = WE2;
    }

    int32_t lsad_val = 0;

    /* Only one LSAD is enabled at any given point of time.
     * As a result, Pass WE1 to this function all the time.
     */
    CEM102_GetValueFromBuffer(WE1, (uint32_t *)&lsad_val);

    p_cem102->CalculateCurrent(chan, lsad_val, p_fvalue);

    CEM102_DiagResult result;
    result.curr = *p_fvalue;
    result.diagmask = diagmask;
    result.dac = chan;
    result.in_diag = true;
    APP_NotifyDiagResult(p_cem102, cem102_dut, &result);

    *p_fvalue /= p_data->div_factor;
    CEM102_PrintCurrent(p_data, str, dac, *p_fvalue);
}

/* Adjusts the reference current for sample count 8. Assumption is that
 * lsad_val is based on setting of sample count 16.
 */
static int CEM102_CalculateRefCurrent(uint32_t chan, uint32_t lsad_val,
                                      float *p_val)
{
    float cur_val = 0.0;

    *p_val = 0.0;

    int result = cem102->CalculateCurrent(chan, (int32_t)lsad_val, &cur_val);
    if (result == ERRNO_NO_ERROR && p_val != NULL)
    {
        uint16_t otp_ver = CEM102_OTP_GetVersion();
        float acc_setting = 0.0;

        if (otp_ver == CEM102_OTP_VERSION_1)
        {
            acc_setting = (float)CEM102_NACC_REF_R1;
        }
        else
        {
            acc_setting = (float)CEM102_NACC_REF;
        }
        float sample_cnt = (float)(cem102_dut->accum_sample_calib + 1);
        *p_val = cur_val / (acc_setting / sample_cnt);
    }
    return result;
}

static void CEM102_ResultCalibCurrent(DRIVER_CEM102_t *p_cem102,
                                      CEM102_CurrentData *p_data,
                                      uint16_t diagmask)
{
    /* When thermistor measurement is disabled, and apireq has at least
     * one valid request, calibration current is measured.
     */
    if ((diagData.apireq) && (calib_ref_measurement == 0))
    {
        APP_SetMeasureTimerEvent(cem102_dut, MEAS_TIMER_OK_TO_RUN_DIAGAPI);
    }
    CEM102_GetValueFromBuffer(CEM102_REF_MEASUREMENT_CHAN,
                             &calib_ref_measurement);

    float curr = 0.0;
    CEM102_CalculateRefCurrent(CEM102_REF_MEASUREMENT_CHAN,
                                            calib_ref_measurement,
                                            &curr);

    CEM102_DiagResult result;
    result.curr = curr;
    result.diagmask = diagmask;
    result.dac = CEM102_REF_MEASUREMENT_CHAN;
    result.in_diag = true;
    APP_NotifyDiagResult(p_cem102, cem102_dut, &result);

#ifdef SWMTRACE_OUTPUT
    curr /= p_data->div_factor;
    CEM102_PrintCurrent(p_data, "Measured calibration current", CEM102_REF_MEASUREMENT_CHAN,
                        curr);
#endif /* SWMTRACE_OUTPUT */
}

static void CEM102_ResultOffsetCurrent(DRIVER_CEM102_t *p_cem102,
                                      CEM102_CurrentData *p_data,
                                      uint16_t diagmask)
{
    float curr = 0.0;
    CEM102_ResultCalculateCurrent(p_cem102, p_data, "OffsetCur", &curr,
                                  diagmask);

    uint32_t val = App_Utils_ConvertFloatToUInt32(curr);
    if (CEM102_LSAD_READ_WE2 == p_data->lsad_req)
    {
        cem102_diag_func_char.WE2_offset_curr = val;
    }
    else
    {
        cem102_diag_func_char.WE1_offset_curr = val;
    }
}

static void CEM102_ResultATResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                           CEM102_CurrentData *p_data,
                                           uint16_t diagmask)
{
    float curr = 0.0;
    CEM102_ResultCalculateCurrent(p_cem102, p_data, "ATResCurr", &curr,
                                  diagmask);

    uint32_t val = App_Utils_ConvertFloatToUInt32(curr);
    if (CEM102_LSAD_READ_WE2 == p_data->lsad_req)
    {
        cem102_diag_func_char.AT2_residual_curr = val;
    }
    else
    {
        cem102_diag_func_char.AT1_residual_curr = val;
    }
}

static void CEM102_ResultResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                         CEM102_CurrentData *p_data,
                                         uint16_t diagmask)
{
    float curr = 0.0;
    CEM102_ResultCalculateCurrent(p_cem102, p_data, "WEResCurr", &curr,
                                  diagmask);
    uint32_t val = App_Utils_ConvertFloatToUInt32(curr);
    if (CEM102_LSAD_READ_WE2 == p_data->lsad_req)
    {
        cem102_diag_func_char.WE2_residual_curr = val;
    }
    else
    {
        cem102_diag_func_char.WE1_residual_curr = val;
    }
}

static void CEM102_ResultREResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                           CEM102_CurrentData *p_data,
                                           uint16_t diagmask)
{
    float curr = 0.0;
    CEM102_ResultCalculateCurrent(p_cem102, p_data, "REResCurr", &curr,
                                  diagmask);

    uint32_t val = App_Utils_ConvertFloatToUInt32(curr);
    cem102_diag_func_char.RE_residual_curr = val;
}

static void CEM102_ResultRECEResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                             CEM102_CurrentData *p_data,
                                             uint16_t diagmask)
{
    float curr = 0.0;
    CEM102_ResultCalculateCurrent(p_cem102, p_data, "RCResCurr", &curr,
                                  diagmask);

    uint32_t val = App_Utils_ConvertFloatToUInt32(curr);
    cem102_diag_func_char.RE_CE_residual_curr = val;
}

static void CEM102_LSADRestoreState(DRIVER_CEM102_t *p_cem102,
                                    CEM102_CurrentData *p_data,
                                    uint16_t diagmask, uint16_t *p_req)
{
    if (diagData.regs_saved)
    {
        uint16_t cmd = CEM102_GetClearCommand(p_data->lsad_req);
        p_cem102->SystemCommand(cem102_dut, cmd | IDLE_CMD);

        if (p_data->result_callback != NULL)
        {
            p_data->result_callback(p_cem102, p_data, diagmask);
        }
        /* Restore switch settings. */
        p_cem102->RegisterRestore(cem102_dut, diagData.p_saved_data);
        diagData.regs_saved = false;
        CEM102_AdjustADCGain(p_cem102, p_data);
    }
    CEM102_SetLSADState(CEM102_LSAD_STATE_SETUP);
    (*p_req) &= ~diagmask;
}

static int CEM102_ConfigChannelSwitch(DRIVER_CEM102_t *p_cem102,
                                     CEM102_LSAD_READ_TYPE lsad_req,
                                     uint16_t sw_conn[])
{
    int chan = WE1;

    if (CEM102_LSAD_READ_WE2 == lsad_req)
    {
        chan = WE2;
    }

    int result = ERRNO_NO_ERROR;
    if (CEM102_LSAD_READ_WE1_WE2 == lsad_req)
    {
        result = p_cem102->SwitchConfig(cem102_dut, WE1, sw_conn[0]);
        result |= p_cem102->SwitchConfig(cem102_dut, WE2, sw_conn[1]);
    }
    else
    {
        result = p_cem102->SwitchConfig(cem102_dut, chan, sw_conn[chan - 1]);
    }
    return result;
}

static int CEM102_SwitchConfigResCurr(DRIVER_CEM102_t *p_cem102,
                                      CEM102_CurrentData *p_data)
{
    /* Configure the ADC to prepare for the ChannelOffsetCurrent */
    int result = ERRNO_PARAM_ERROR;

    if (p_data->lsad_req == CEM102_LSAD_READ_WE2)
    {
        result = p_cem102->SwitchConfig(cem102_dut, WE1,
                                        CH1_SC119_SC120_CONNECT);
    }
    else if (p_data->lsad_req == CEM102_LSAD_READ_WE1)
    {
        result = p_cem102->SwitchConfig(cem102_dut, WE2,
                                        CH2_SC219_SC220_CONNECT);
    }
    return result;
}

static int CEM102_SetupATResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                         CEM102_CurrentData *p_data)
{
    /* Disconnect all SAxx switches */
    int result = p_cem102->RegisterWrite(cem102_dut, ANA_SW_CFG2,
                                         ATBUS_SA1_DISCONNECT);

    /* Disable RE channel, SR switches, and HP mode, RE_BUFFER */
    CEM102_DisableREDAC(p_cem102, cem102_dut,
                        CH1_SC109_SC110_CONNECT, RE_BUF_DISABLE);

    /* Disable all ST switches */
    result |= p_cem102->RE_ATBusSwitchConfig(cem102_dut, RE,
                                           ATBUS_BUF_DISABLE);
    if (result == ERRNO_NO_ERROR)
    {
        uint16_t sw_conn[CEM102_NUM_LSADS] = { CH1_SC113_CONNECT | CH1_SC119_SC120_CONNECT,
                                               CH2_SC213_CONNECT | CH2_SC219_SC220_CONNECT
                                             };
        result = CEM102_ConfigChannelSwitch(p_cem102, p_data->lsad_req,
                                           sw_conn);
    }
    return result;
}

static int CEM102_SetupResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                       CEM102_CurrentData *p_data)
{
    int result = CEM102_SwitchConfigResCurr(p_cem102, p_data);

    if (result != ERRNO_NO_ERROR)
    {
        /* Disable RE channel, SR switches, and HP mode, RE_BUFFER */
        CEM102_DisableREDAC(p_cem102, cem102_dut,
                            CH1_SC109_SC110_CONNECT, RE_BUF_DISABLE);
    }
    return result;
}

/* Current is measured in WE2 only. Incoming channel number is ignored */
static int CEM102_ConfigREResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                                CEM102_CurrentData *p_data)
{
    /* WE2 DAC shall be set to the RE voltage for RE_CE residual and
     * RE residual
     */
    uint16_t dac_setting = 0;

    int result = p_cem102->CalculateDACAmplitude(cem102_dut, WE2,
                                                 p_data->target_mv,
                                                 &dac_setting);

    /* OTP v3 parts will cause CalculateDACAmplitude to return error. */
    if (result == ERRNO_GENERAL_FAILURE)
    {
        /* Use existing, calibrated value */
        dac_setting = calib_data.dac_setting[WE2 - INDEX_ADJ_HELPER];
        p_data->target_mv = RE_DAC_V_USER_SETTING;
    }
    result |= p_cem102->WEDACConfig(cem102_dut, WE2,
                                    ((DC_DAC_WE2_CTRL_DAC_WE2_VALUE_MASK &
                                    dac_setting) | DAC_WE2_ENABLE),
                                    STATE_ERR_IRQ_DISABLE);

    /* RE voltage will be set via AT1 to WE2 voltage (RE voltage while in
     * normal operation)
     */
    uint16_t sw_conn[CEM102_NUM_LSADS] = { CH1_SC119_SC120_CONNECT,
                                           CH2_SC219_SC220_CONNECT | CH2_SC213_CONNECT
                                         };
    result |= CEM102_ConfigChannelSwitch(p_cem102, CEM102_LSAD_READ_WE1_WE2,
                                        sw_conn);
    return result;
}

static int CEM102_SetupRECEResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                           CEM102_CurrentData *p_data)
{
    CEM102_DisableREDAC(p_cem102, cem102_dut,
                        CEM102_SW_SR1_POSITION(cem102_dut) |
                        RE_SR2_CONNECT, RE_BUF_DISABLE);
    return CEM102_ConfigREResidualCurrent(p_cem102, p_data);
}

static int CEM102_SetupREResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                         CEM102_CurrentData *p_data)
{
    CEM102_DisableREDAC(p_cem102, cem102_dut, RE_SR2_CONNECT,
                        RE_BUF_DISABLE);
    return CEM102_ConfigREResidualCurrent(p_cem102, p_data);
}

static int CEM102_SetupOffsetCurrent(DRIVER_CEM102_t *p_cem102,
                                     CEM102_CurrentData *p_data)
{
    /* Configure the ADC to prepare for the ChannelOffsetCurrent */
    uint16_t sw_conn[CEM102_NUM_LSADS] = { CH1_SC119_SC120_CONNECT,
                                           CH2_SC219_SC220_CONNECT
                                         };
    CEM102_EnableSingleChannel(p_cem102,
                               (p_data->lsad_req == CEM102_LSAD_READ_WE2) ?
                               WE2 : WE1);
    return CEM102_ConfigChannelSwitch(p_cem102, p_data->lsad_req, sw_conn);
}

static int CEM102_SetupDACVoltage(DRIVER_CEM102_t *p_cem102,
                                  CEM102_CurrentData *p_data)
{
    int result = p_cem102->SetupADCToMeasureDAC(cem102_dut, p_data->target_dac,
                                                CEM102_PRE_MEASURE_CONFIG);
    CEM102_AdjustADCGain(p_cem102, p_data);

    if (result == ERRNO_NO_ERROR)
    {
        uint8_t index = (uint8_t)p_data->target_dac - 1;
        uint16_t we_en = DAC_WE1_ENABLE; /* Value same for all DACS */
        uint16_t dac_setting = 0;
        uint16_t calib_v[CEM102_MAX_DACS] = {
            WE1_DAC_V_USER_SETTING,
            WE2_DAC_V_USER_SETTING,
            RE_DAC_V_USER_SETTING
        };

        /* This logic isn't for the diagnostics */
        if (diagData.diagreq_saved == 0)
        {
            we_en = CEM102_GetDACEnableBit(cem102_dut, p_data->target_dac);
        }

        result = p_cem102->CalculateDACAmplitude(cem102_dut, p_data->target_dac,
                                                 p_data->target_mv, &dac_setting);
        if (result == ERRNO_GENERAL_FAILURE)
        {
            float val = (float)calib_data.dac_setting[index];
            if (p_data->target_mv)
            {
                val *= (float)p_data->target_mv;
                val /= (float)calib_v[index];
            }
            dac_setting = (uint16_t)val;
        }

        uint16_t dac_v = 0;
        uint8_t dac_reg[CEM102_MAX_DACS] = { DC_DAC_WE1_CTRL,
                                             DC_DAC_WE2_CTRL,
                                             DC_DAC_RE_CTRL
                                           };
        uint16_t ctrl_mask[CEM102_MAX_DACS] = { DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK,
                                                DC_DAC_WE2_CTRL_DAC_WE2_VALUE_MASK,
                                                DC_DAC_RE_CTRL_DAC_RE_VALUE_MASK
                                              };
        result = p_cem102->RegisterRead(cem102_dut, dac_reg[index], &dac_v);
        if (result == ERRNO_NO_ERROR)
        {
            uint16_t irq_cfg = DC_DAC_LOAD_IRQ_DISABLE;

            dac_v &= ctrl_mask[index];

            /* If DAC is enabled, we can expect DAC LOAD IRQ interrupt */
            if ((dac_v != dac_setting) && (we_en != 0))
            {
                CEM102_SetLSADState(CEM102_LSAD_STATE_DAC_LOADED);
                CEM102_Enable_DAC_LOAD_IRQ(p_cem102, true);
                irq_cfg = DC_DAC_LOAD_IRQ_ENABLE;
            }
            if (p_data->target_dac == RE)
            {
                p_cem102->REDACConfig(cem102_dut, ((ctrl_mask[index] &
                                      dac_setting) | we_en),
                                      (CEM102_SW_SR1_POSITION(cem102_dut) |
                                      RE_SR2_CONNECT),
                                      (RE_BUF_ENABLE |
                                      RE_HP_MODE_DISABLE));
            }
            else
            {
                result |= p_cem102->WEDACConfig(cem102_dut,
                                                (int)p_data->target_dac,
                                                ((ctrl_mask[index] &
                                                dac_setting) | we_en),
                                                irq_cfg);
            }

            /* When DAC is disabled, we are not getting the DAC_READY event.
             * Start the measurement now.
             */
            if ((dac_v == dac_setting) || (we_en == 0))
            {
                CEM102_EnableSingleChannel(p_cem102,
                                           (p_data->target_dac == WE2) ?
                                           WE2 : WE1);

                /* We can measure without any delay */
                CEM102_StartMeasurement(p_cem102, SINGLE_CMD);
            }
        }
    }
    return result;
}

static int CEM102_SetupCalibCurrent(DRIVER_CEM102_t *p_cem102,
                                     CEM102_CurrentData *p_data)
{
    int32_t chan = WE1;

    if (p_data->lsad_req == CEM102_LSAD_READ_WE2)
    {
        chan = WE2;
    }
    CEM102_SetLSADState(CEM102_LSAD_STATE_MEAS_STARTED);

    /* Disable the unused LSAD's measurement */
    CEM102_ChannelCtrl(p_cem102, (chan == WE2) ? WE1 : WE2, false);
    return p_cem102->MeasureReference(cem102_dut, chan);
}

static void CEM102_MeasureWithLSAD(DRIVER_CEM102_t *p_cem102,
                                   CEM102_CurrentData *p_data,
                                   uint16_t diagmask, uint16_t *p_req)
{
    int result = ERRNO_NO_ERROR;

    /* When request is to measure both WE1 and WE2, incoming channel number
     * is enforced to be WE1. */
    switch (diagData.lsad_state)
    {
        case CEM102_LSAD_STATE_SETUP:
        {
            diagData.regs_saved = false;
            memset(diagData.p_saved_data, 0, sizeof(*diagData.p_saved_data));
            diagData.p_saved_data->valid = p_data->valid_regs;
            result = p_cem102->RegisterSave(cem102_dut, diagData.p_saved_data);

            if (result == ERRNO_NO_ERROR)
            {
                diagData.regs_saved = true;

                if (p_data->setup_callback != NULL)
                {
                    result = p_data->setup_callback(p_cem102, p_data);
                }
            }
            if ((result == ERRNO_NO_ERROR) && (!diagData.setup_changes_states))
            {
                /* We expect LSAD to be idle after calibration */
                CEM102_EnableSingleChannel(p_cem102,
                                           (p_data->lsad_req == CEM102_LSAD_READ_WE2) ?
                                           WE2 : WE1);
                result = CEM102_StartMeasurement(p_cem102, SINGLE_DELAY_CMD);
            }
            if (result != ERRNO_NO_ERROR)
            {
                /* Error. Restore the registers if needed before bailing out */
                CEM102_LSADRestoreState(p_cem102, p_data, diagmask, p_req);
            }
            break;
        }
        case CEM102_LSAD_STATE_DAC_LOADED:
        {
            break;
        }
        case CEM102_LSAD_STATE_DAC_READY:
        {
            CEM102_Enable_DAC_LOAD_IRQ(p_cem102, false);
            CEM102_EnableSingleChannel(p_cem102,
                                       (p_data->lsad_req == CEM102_LSAD_READ_WE2) ?
                                       WE2 : WE1);

            /* DAC may be just loaded. Measuring with some delay */
            CEM102_StartMeasurement(p_cem102, SINGLE_DELAY_CMD);
            break;
        }
        case CEM102_LSAD_STATE_MEAS_STARTED:
        {
            uint16_t mask = CEM102_GetSysStatusMask(p_data->lsad_req);
            uint16_t sysstatus = 0;

            result = p_cem102->RegisterRead(cem102_dut, SYS_STATUS, &sysstatus);
            if (result == ERRNO_NO_ERROR)
            {
                uint16_t cem102_state = sysstatus & SYS_STATUS_SYS_STATE_MASK;
                if (cem102_state == IDLE_STATE)
                {
                    if ((sysstatus & mask) == mask)
                    {
                        CEM102_SetLSADState(CEM102_LSAD_STATE_DATA_REQUESTED);
                        memset(demo_measure_buf, 0, sizeof(demo_measure_buf));
                        result = p_cem102->QueueRead(cem102_dut,
                                                     SYS_BUFFER_WORD0,
                                                     MEASUREMENT_LENGTH);
                    }
                }
            }
            if (result != ERRNO_NO_ERROR)
            {
                /* Bail out on error */
                CEM102_LSADRestoreState(p_cem102, p_data, diagmask, p_req);
            }
            break;
        }
        case CEM102_LSAD_STATE_DATA_AVAILABLE:
        {
            p_cem102->RegisterBufferRead(cem102_dut, demo_measure_buf,
                                       MEASUREMENT_LENGTH);
            CEM102_LSADRestoreState(p_cem102, p_data, diagmask, p_req);
            break;
        }
        case CEM102_LSAD_STATE_REINIT:
        case CEM102_LSAD_STATE_DATA_REQUESTED:
        {
            /* Do nothing states. Either we are waiting for the measurement
             * data or some events interrupted the diagnostics in the middle
             * and needs to be reinitialized before restarted.
             */
            break;
        }
    }
}

static void CEM102_UpdateCalibCurrentRatio(DRIVER_CEM102_t *p_cem102, uint16_t *p_req)
{
    uint32_t ibp = 0;
    uint32_t ibn = 0;
    float curr_measured = 0.0;

    int result = p_cem102->GetCalibratedCurrent(&ibp, &ibn);
    if (result == ERRNO_NO_ERROR)
    {
        result = CEM102_CalculateRefCurrent(CEM102_REF_MEASUREMENT_CHAN,
                                            calib_ref_measurement,
                                            &curr_measured);
    }
    if (result == ERRNO_NO_ERROR)
    {
        float curr_from_otp = (float)ibn;
        cem102_diag_func_char.cal_curr_ratio =
                App_Utils_ConvertFloatToUInt32(curr_measured / curr_from_otp);
#if SWMTRACE_OUTPUT
        swmLogInfo("\rCalibration current ratio:,%f\r\n",
                   curr_measured / curr_from_otp);
#endif /* SWMTRACE_OUTPUT */
    }
    (*p_req) &= ~CEM102_DIAG_CAL_CURR_RATIO;
}

static void CEM102_InitRegsToSaveForDacVolt(CEM102_CurrentData *p_data)
{
    p_data->valid_regs = REG_ANA_CFG0_SAVE | REG_ANA_CFG1_SAVE |
                         REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG1_SAVE |
                         REG_ANA_SW_CFG2_SAVE | REG_ANA_SW_CFG3_SAVE |
                         REG_SYSCTRL_IRQ_CFG_SAVE | REG_CHCFG_LSAD_CLK_SAVE;
    switch (p_data->lsad_req)
    {
        case CEM102_LSAD_READ_WE2:
        {
            p_data->valid_regs |= (REG_CHCFG_CH2_LSAD_SAVE |
                                   REG_CHCFG_CH2_THRESHOLD_SAVE |
                                   REG_CHCFG_CH2_BUFFER_SAVE |
                                   REG_CHCFG_CH1_ACCUM_SAVE |
                                   REG_CHCFG_CH2_ACCUM_SAVE |
                                   REG_DC_DAC_WE2_CTRL_SAVE
                                  );
            break;
        }

        /* WE1 is used for RE-DAC voltage measurement. Refer
         * Config_ADC_PreDACCalibration function in the HAL layer.
         */
        default: /* CEM102_LSAD_READ_WE1 & CEM102_LSAD_READ_RE */
        {
            p_data->valid_regs |= (REG_CHCFG_CH1_LSAD_SAVE |
                                   REG_CHCFG_CH1_THRESHOLD_SAVE |
                                   REG_CHCFG_CH1_BUFFER_SAVE |
                                   REG_CHCFG_CH1_ACCUM_SAVE |
                                   REG_CHCFG_CH2_ACCUM_SAVE |
                                   REG_DC_DAC_WE1_CTRL_SAVE
                                  );
            break;
        }
    }
}

static void CEM102_SetElectrodeInputs(DRIVER_CEM102_t *p_cem102, bool connect)
{
    uint16_t sw_cfg0 = CH1_SC112_DISCONNECT | CH1_SC109_SC110_DISCONNECT;
    uint16_t sw_cfg1 = CH2_SC212_DISCONNECT | CH2_SC209_SC210_DISCONNECT;
    uint16_t mask0 = ANA_SW_CFG0_CH1_SC109_SC110_CFG_MASK |
                     ANA_SW_CFG0_CH1_SC112_CFG_MASK;
    uint16_t mask1 = ANA_SW_CFG1_CH2_SC209_SC210_CFG_MASK |
                     ANA_SW_CFG1_CH2_SC212_CFG_MASK;

    if (connect)
    {
        sw_cfg0 = CH1_SC112_CONNECT | CH1_SC109_SC110_CONNECT;
        sw_cfg1 = CH2_SC212_CONNECT | CH2_SC209_SC210_CONNECT;
    }
    p_cem102->RegisterReadModifyWrite(cem102_dut, ANA_SW_CFG0, sw_cfg0, mask0);
    p_cem102->RegisterReadModifyWrite(cem102_dut, ANA_SW_CFG1, sw_cfg1, mask1);
}

static void CEM102_DiagnosticsInit(DRIVER_CEM102_t *p_cem102)
{
    if (diagData.lsad_state == CEM102_LSAD_STATE_REINIT ||
        diagData.p_saved_data != &diagData.saved_data)
    {
        /* On STATE_REINIT, clear apireq which may or may not be 0 */
        diagData.apireq = 0;
        diagData.p_saved_data = &diagData.saved_data;
        CEM102_SetLSADState(CEM102_LSAD_STATE_SETUP);
        diagData.regs_saved = false;
        diagData.diagreq = CEM102_DIAG_REQUESTS;
        diagData.diagreq_saved = diagData.diagreq;

        if (diagData.diagreq != 0)
        {
            if (calib_ref_measurement == 0)
            {
                diagData.diagreq |= CEM102_CALIB_CURRENT;

                /* Since calibration current measured here, it
                 * won't/need not be measured while handling "apireq".
                 * Setting it here as completed.
                 */
                APP_SetMeasureTimerEvent(cem102_dut,
                                         MEAS_TIMER_OK_TO_RUN_DIAGAPI);
            }

            /* Disconnect WE1 & WE2 before starting diagnostics */
            CEM102_SetElectrodeInputs(p_cem102, false);

            /* Clear any existing events & go to IDLE mode in case LSAD is
             * already measuring.
             */
            CEM102_SendCommand(p_cem102, IDLE_CMD);
        }
        else
        {
            APP_SetMeasureTimerEvent(cem102_dut,
                                     MEAS_TIMER_OK_TO_RUN_DIAGNOSTICS);
        }
    }
}

void CEM102_SetLSADState(CEM102_LSAD_STATE lsad_state)
{
    diagData.lsad_state = lsad_state;
}

CEM102_LSAD_STATE CEM102_GetLSADState(void)
{
    return diagData.lsad_state;
}

static void CEM102_DiagnosticsInternal(DRIVER_CEM102_t *p_cem102, uint16_t *p_req)
{
    /* Initialize, if not done already. */
    CEM102_DiagnosticsInit(p_cem102);

    uint16_t diagmask = 0;
    for (uint16_t idx = 0; idx < CEM102_DIAG_NUM_REQUESTS; idx++)
    {
        if ((*p_req) & (1U << idx))
        {
            diagmask = (1U << idx);
            break;
        }
    }

    CEM102_CurrentData data;
    memset(&data, 0, sizeof(data));

    /* Only few setup callbacks handles the state machines. */
    diagData.setup_changes_states = false;
    data.div_factor = APP_FEMTO_PICO_COV_FACTOR;

    switch (diagmask)
    {
        case CEM102_CALIB_CURRENT:
        {

            data.valid_regs = REG_ANA_CFG0_SAVE | REG_ANA_CFG1_SAVE |
                              REG_ANA_CFG2_SAVE | REG_CHCFG_LSAD_CLK_SAVE |
                              REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG2_SAVE |
                              REG_SYSCTRL_IRQ_CFG_SAVE;

            if (data.lsad_req == CEM102_LSAD_READ_WE2)
            {
                data.valid_regs |= (REG_DC_DAC_WE2_CTRL_SAVE |
                                    REG_CHCFG_CH2_BUFFER_SAVE |
                                    REG_CHCFG_CH2_ACCUM_SAVE |
                                    REG_CHCFG_CH2_LSAD_SAVE
                                   );
            }
            else
            {
                data.valid_regs |= (REG_DC_DAC_WE1_CTRL_SAVE |
                                    REG_CHCFG_CH1_BUFFER_SAVE |
                                    REG_CHCFG_CH1_ACCUM_SAVE |
                                    REG_CHCFG_CH1_LSAD_SAVE
                                   );
            }

            /* Setup callback will change the state, when it kicks off
             * measurement */
            diagData.setup_changes_states = true;

            /* Print in femto amps */
            data.div_factor = APP_FEMTO_NANO_COV_FACTOR;
            data.lsad_req = CEM102_LSAD_READ_WE1;
            data.setup_callback = CEM102_SetupCalibCurrent;
            data.result_callback = CEM102_ResultCalibCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);
            break;
        }
        case CEM102_DIAG_CAL_CURR_RATIO:
        {
            CEM102_UpdateCalibCurrentRatio(p_cem102, p_req);
            break;
        }
        case CEM102_DIAG_WE1_OFF_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG1_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE1;
            data.setup_callback = CEM102_SetupOffsetCurrent;
            data.result_callback = CEM102_ResultOffsetCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);
            break;
        }
        case CEM102_DIAG_WE2_OFF_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG1_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE2;
            data.setup_callback = CEM102_SetupOffsetCurrent;
            data.result_callback = CEM102_ResultOffsetCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);
            break;
        }
        case CEM102_DIAG_WE1_DAC_VOLT:
        {
            /* To be consistent, voltage measurement uses LSAD2 */
            data.lsad_req = CEM102_LSAD_READ_WE1;
            data.target_dac = WE1;
            data.target_mv = WE1_DAC_V_USER_SETTING;
            diagData.setup_changes_states = true;
            CEM102_InitRegsToSaveForDacVolt(&data);
            data.setup_callback = CEM102_SetupDACVoltage;
            data.result_callback = CEM102_ResultDACVoltage;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);
            break;
        }
        case CEM102_DIAG_WE2_DAC_VOLT:
        {
            /* To be consistent, voltage measurement uses LSAD2 */
            data.lsad_req = CEM102_LSAD_READ_WE2;
            data.target_dac = WE2;
            data.target_mv = WE2_DAC_V_USER_SETTING;
            diagData.setup_changes_states = true;
            CEM102_InitRegsToSaveForDacVolt(&data);
            data.setup_callback = CEM102_SetupDACVoltage;
            data.result_callback = CEM102_ResultDACVoltage;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);
            break;
        }
        case CEM102_DIAG_RE_DAC_VOLT:
        {
            /* To be consistent, voltage measurement uses LSAD2 */
            data.lsad_req = CEM102_LSAD_READ_WE1;
            data.target_dac = RE;
            data.target_mv = RE_DAC_V_USER_SETTING;
            diagData.setup_changes_states = true;
            CEM102_InitRegsToSaveForDacVolt(&data);
            data.setup_callback = CEM102_SetupDACVoltage;
            data.result_callback = CEM102_ResultDACVoltage;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);
            break;
        }
        case CEM102_DIAG_AT1_RES_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE |
                              REG_ANA_SW_CFG1_SAVE |
                              REG_ANA_SW_CFG2_SAVE |
                              REG_ANA_SW_CFG3_SAVE |
                              REG_ANA_CFG0_SAVE |
                              REG_ANA_CFG1_SAVE |
                              REG_DC_DAC_RE_CTRL_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE1;
            data.setup_callback = CEM102_SetupATResidualCurrent;
            data.result_callback = CEM102_ResultATResidualCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);

            break;
        }
        case CEM102_DIAG_AT2_RES_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE |
                              REG_ANA_SW_CFG1_SAVE |
                              REG_ANA_SW_CFG2_SAVE |
                              REG_ANA_SW_CFG3_SAVE |
                              REG_ANA_CFG0_SAVE |
                              REG_ANA_CFG1_SAVE |
                              REG_DC_DAC_RE_CTRL_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE2;
            data.setup_callback = CEM102_SetupATResidualCurrent;
            data.result_callback = CEM102_ResultATResidualCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);
            break;
        }
        case CEM102_DIAG_WE1_RES_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG1_SAVE |
                              REG_ANA_CFG1_SAVE | REG_DC_DAC_RE_CTRL_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE1;
            data.setup_callback = CEM102_SetupResidualCurrent;
            data.result_callback = CEM102_ResultResidualCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);
            break;
        }
        case CEM102_DIAG_WE2_RES_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG1_SAVE |
                              REG_ANA_CFG1_SAVE | REG_DC_DAC_RE_CTRL_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE2;
            data.setup_callback = CEM102_SetupResidualCurrent;
            data.result_callback = CEM102_ResultResidualCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);
            break;
        }
        case CEM102_DIAG_RE_RES_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE |
                              REG_ANA_SW_CFG1_SAVE |
                              REG_ANA_SW_CFG3_SAVE |
                              REG_ANA_CFG0_SAVE |
                              REG_ANA_CFG1_SAVE |
                              REG_DC_DAC_RE_CTRL_SAVE |
                              REG_DC_DAC_WE2_CTRL_SAVE |
                              REG_SYSCTRL_IRQ_CFG_SAVE;

            /* Channel number is irrelevant. Code always use WE2 */
            data.lsad_req = CEM102_LSAD_READ_WE2;
            data.target_mv = RE_DAC_V_USER_SETTING;
            data.setup_callback = CEM102_SetupREResidualCurrent;
            data.result_callback = CEM102_ResultREResidualCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);

            break;
        }
        case CEM102_DIAG_RE_CE_RES_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE |
                              REG_ANA_SW_CFG1_SAVE |
                              REG_ANA_SW_CFG3_SAVE |
                              REG_ANA_CFG0_SAVE |
                              REG_ANA_CFG1_SAVE |
                              REG_DC_DAC_RE_CTRL_SAVE |
                              REG_DC_DAC_WE2_CTRL_SAVE |
                              REG_SYSCTRL_IRQ_CFG_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE2;
            data.target_mv = RE_DAC_V_USER_SETTING;
            data.setup_callback = CEM102_SetupRECEResidualCurrent;
            data.result_callback = CEM102_ResultRECEResidualCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask, p_req);
            break;
        }
        default:
        {
            break;
        }
    }
}
#endif /* CEM102_DIAG_FUNCTIONS */

/* Should be called only when CEM102_DiagnosticsDone returns false.  */
void CEM102_Diagnostics(DRIVER_CEM102_t *p_cem102)
{
#if CEM102_DIAG_FUNCTIONS
    do
    {
        CEM102_DiagnosticsInternal(p_cem102, &diagData.diagreq);
        if (CEM102_DiagnosticsDone(p_cem102))
        {
            /* Clear all the events return in a clean state, in IDLE mode, the same
             * way we entered the diagnostics mode.
             */
            CEM102_SendCommand(p_cem102, IDLE_CMD);
        }
    } while ((!CEM102_DiagnosticsDone(p_cem102)) &&
           (CEM102_GetLSADState() == CEM102_LSAD_STATE_SETUP));
#endif /* CEM102_DIAG_FUNCTIONS */
}

bool CEM102_DiagnosticsDone(DRIVER_CEM102_t *p_cem102)
{
    bool result = true;

#if CEM102_DIAG_FUNCTIONS
    CEM102_DiagnosticsInit(p_cem102);
    if (diagData.diagreq)
    {
        result = false;
    }
    else
    {
        if (diagData.diagreq_saved)
        {
            diagData.diagreq_saved = 0;

            /* Connect WE1 & WE2 back as diagnostics is over. */
            CEM102_SetElectrodeInputs(p_cem102, true);
            APP_SetMeasureTimerEvent(cem102_dut,
                                     MEAS_TIMER_OK_TO_RUN_DIAGNOSTICS);
        }
    }
#endif /* CEM102_DIAG_FUNCTIONS */
    return result;
}

const char *CEM102_GetDACName(CEM102_DAC_TYPE dac)
{
    const char *p;
    if (dac == WE2)
    {
        p = "WE2";
    }
    else if (dac == RE)
    {
        p = "RE";
    }
    else
    {
        p = "WE1";
    }
    return p;
}

void CEM102_DiagnosticsAPIInit(DRIVER_CEM102_t *p_cem102, const CEM102_Device *p_dev)
{
#if CEM102_DIAG_FUNCTIONS
    diagData.p_saved_data = &diagData.saved_data;
    CEM102_SetLSADState(CEM102_LSAD_STATE_SETUP);
    diagData.regs_saved = false;
    diagData.apireq = 0;
    if (CEM102_MEAS_WE1_VOLTAGE(p_dev))
    {
       diagData.apireq |= CEM102_DIAG_WE1_DAC_VOLT;
    }
    if (CEM102_MEAS_WE2_VOLTAGE(p_dev))
    {
       diagData.apireq |= CEM102_DIAG_WE2_DAC_VOLT;
    }
    if (CEM102_MEAS_RE_VOLTAGE(p_dev))
    {
       diagData.apireq |= CEM102_DIAG_RE_DAC_VOLT;
    }

    if (diagData.apireq != 0)
    {
        if (calib_ref_measurement == 0)
        {
            diagData.apireq |= CEM102_CALIB_CURRENT;
        }

        /* Clear any existing events & go to IDLE mode in case LSAD is
         * already measuring.
         */
        CEM102_SendCommand(p_cem102, IDLE_CMD);
    }
    else
    {
        APP_SetMeasureTimerEvent(p_dev, MEAS_TIMER_OK_TO_RUN_DIAGAPI);
    }

    /* Call once to kick start the measurement */
    CEM102_DiagnosticsAPI(p_cem102, p_dev);
#endif /* CEM102_DIAG_FUNCTIONS */
}

bool CEM102_DiagnosticsAPIDone(DRIVER_CEM102_t *p_cem102, const CEM102_Device *p_dev)
{
    bool result = true;

#if CEM102_DIAG_FUNCTIONS
    if (diagData.apireq)
    {
        result = false;
    }
#endif /* CEM102_DIAG_FUNCTIONS */
    return result;
}

void CEM102_DiagnosticsAPI(DRIVER_CEM102_t *p_cem102, const CEM102_Device *p_dev)
{
#if CEM102_DIAG_FUNCTIONS
    bool done = false;
    do
    {
        CEM102_DiagnosticsInternal(p_cem102, &diagData.apireq);
        done = CEM102_DiagnosticsAPIDone(p_cem102, p_dev);
        if (done)
        {
            /* Clear all the events return in a clean state, in IDLE mode, the same
             * way we entered the diagnostics mode.
             */
            CEM102_SendCommand(p_cem102, IDLE_CMD);
        }
    } while ((!done) && (CEM102_GetLSADState() == CEM102_LSAD_STATE_SETUP));
#endif /* CEM102_DIAG_FUNCTIONS */
}

