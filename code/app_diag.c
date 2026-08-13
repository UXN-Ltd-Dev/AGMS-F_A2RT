/**
 * @file  app_diag.c
 * @brief CEM102 diagnostic functionality related source file.
 *
 * @copyright @parblock
 * Copyright (c) 2024 Semiconductor Components Industries, LLC (d/b/a
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

/* When dac is passed with RE, it assumes WE1 */
static void CEM102_GetValueFromBuffer(uint32_t chan, int32_t *p_lsad)
{
    uint8_t lower = APP_WE1_LOWER_HALF_WORD;
    uint8_t upper = APP_WE1_UPPER_HALF_WORD;

    if (WE2 == chan)
    {
        lower = APP_WE2_LOWER_HALF_WORD;
        upper = APP_WE2_UPPER_HALF_WORD;
    }
    *p_lsad = (int32_t)(measure_buf[lower] + (measure_buf[upper] << 16));
}

static void CEM102_ResultDACVoltage(DRIVER_CEM102_t *p_cem102,
                                    CEM102_CurrentData *p_data)
{
    uint32_t chan = WE1;  /* WE1 for RE */
    CEM102_DAC_TYPE dac = CEM102_GetChanNumber(p_data->lsad_req);

    if (WE2 == dac)
    {
        chan = WE2;
    }
    int32_t lsad_val = 0;
    CEM102_GetValueFromBuffer(chan, &lsad_val);

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

    /* Now, v_measured has a difference, not the actual measurement. */
    if (WE2 == dac)
    {
        cem102_diag_func_char.WE2_DAC_volt = v_diff;
    } else if (RE == dac)
    {
        cem102_diag_func_char.RE_DAC_volt = v_diff;
    }
    else /* WE1 */
    {
        cem102_diag_func_char.WE1_DAC_volt = v_diff;
    }
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("DAC-%d: Voltage(uV): dac=%d, measured=%d, Delta %d\r\n", dac,
               v_dac, v_measured, v_diff);
#endif /* SWMTRACE_OUTPUT */
}

static void CEM102_PrintCurrent(CEM102_CurrentData *p_data, const char *str,
                                CEM102_DAC_TYPE dac, float curr)
{
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("%s: DAC-%d: %f %s\r\n", str, dac, curr,
               (p_data->div_factor == APP_FEMTO_PICO_COV_FACTOR) ? "pA" :
               ((p_data->div_factor == APP_FEMTO_NANO_COV_FACTOR) ? "nA" :
              "fA"));
#endif /* SWMTRACE_OUTPUT */
}

/* This function returns value in pA */
static void CEM102_ResultCalculateCurrent(DRIVER_CEM102_t *p_cem102,
                                         CEM102_CurrentData *p_data,
                                         const char *str,
                                         float *p_fvalue)
{
    uint8_t chan = WE1;
    CEM102_DAC_TYPE dac = CEM102_GetChanNumber(p_data->lsad_req);

    if (WE2 == dac)
    {
        chan = WE2;
    }

    int32_t lsad_val = 0;
    CEM102_GetValueFromBuffer(chan, &lsad_val);

    p_cem102->CalculateCurrent(chan, lsad_val, p_fvalue);
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
        *p_val = cur_val / (acc_setting / (float)(ACCUM_SAMPLE_CNT_8+1));
    }
    return result;
}

static void CEM102_ResultCalibCurrent(DRIVER_CEM102_t *p_cem102,
                                      CEM102_CurrentData *p_data)
{
    CEM102_GetValueFromBuffer(CEM102_REF_MEASUREMENT_CHAN,
                             (int32_t *)&calib_ref_measurement);
#ifdef SWMTRACE_OUTPUT
    float curr = 0.0;
    CEM102_CalculateRefCurrent(CEM102_REF_MEASUREMENT_CHAN,
                                            calib_ref_measurement,
                                            &curr);
    curr /= p_data->div_factor;
    CEM102_PrintCurrent(p_data, "calib curr", CEM102_REF_MEASUREMENT_CHAN,
                        curr);
#endif /* SWMTRACE_OUTPUT */
}

static void CEM102_ResultOffsetCurrent(DRIVER_CEM102_t *p_cem102,
                                      CEM102_CurrentData *p_data)
{
    float curr = 0.0;
    CEM102_ResultCalculateCurrent(p_cem102, p_data, "OffsetCur", &curr);

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
                                      CEM102_CurrentData *p_data)
{
    float curr = 0.0;
    CEM102_ResultCalculateCurrent(p_cem102, p_data, "ATResCurr", &curr);

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
                                      CEM102_CurrentData *p_data)
{
    float curr = 0.0;
    CEM102_ResultCalculateCurrent(p_cem102, p_data, "WEResCurr", &curr);

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
                                      CEM102_CurrentData *p_data)
{
    float curr = 0.0;
    CEM102_ResultCalculateCurrent(p_cem102, p_data, "REResCurr", &curr);

    uint32_t val = App_Utils_ConvertFloatToUInt32(curr);
    cem102_diag_func_char.RE_residual_curr = val;
}

static void CEM102_ResultRECEResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                      CEM102_CurrentData *p_data)
{
    float curr = 0.0;
    CEM102_ResultCalculateCurrent(p_cem102, p_data, "RCResCurr", &curr);

    uint32_t val = App_Utils_ConvertFloatToUInt32(curr);
    cem102_diag_func_char.RE_CE_residual_curr = val;
}

static void CEM102_LSADRestoreState(DRIVER_CEM102_t *p_cem102,
                                    CEM102_CurrentData *p_data,
                                    uint16_t diagmask)
{
    if (diagData.regs_saved)
    {
        uint16_t cmd = CEM102_GetClearCommand(p_data->lsad_req);
        p_cem102->SystemCommand(cem102_dut, cmd | IDLE_CMD);

        if (p_data->result_callback != NULL)
        {
            p_data->result_callback(p_cem102, p_data);
        }
        /* Restore switch settings. */
        p_cem102->RegisterRestore(cem102_dut, diagData.p_saved_data);
        diagData.regs_saved = false;
        CEM102_AdjustADCGain(p_cem102, p_data);
    }
    CEM102_SetLSADState(CEM102_LSAD_STATE_SETUP);
    diagData.diagreq &= ~diagmask;
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
    result |= p_cem102->REDACConfig(cem102_dut, DAC_RE_DISABLE,
                                    CH1_SC109_SC110_CONNECT, RE_BUF_DISABLE);

    /* Disable all ST switches */
    result |= p_cem102->RE_ATBusSwitchConfig(cem102_dut, RE,
                                           ATBUS_BUF_DISABLE);
    if (result == ERRNO_NO_ERROR)
    {
        uint16_t sw_conn[2] = { CH1_SC113_CONNECT | CH1_SC119_SC120_CONNECT,
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
        p_cem102->REDACConfig(cem102_dut, DAC_RE_DISABLE,
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
        p_data->target_mv = WE_DAC_TARGET_MV;
    }
    result |= p_cem102->WEDACConfig(cem102_dut, WE2,
                                    ((DC_DAC_WE2_CTRL_DAC_WE2_VALUE_MASK &
                                    dac_setting) | DAC_WE2_ENABLE),
                                    STATE_ERR_IRQ_DISABLE);

    /* RE voltage will be set via AT1 to WE2 voltage (RE voltage while in
     * normal operation)
     */
    uint16_t sw_conn[2] = { CH1_SC119_SC120_CONNECT,
                            CH2_SC219_SC220_CONNECT | CH2_SC213_CONNECT
                          };
    result |= CEM102_ConfigChannelSwitch(p_cem102, CEM102_LSAD_READ_WE1_WE2,
                                        sw_conn);
    return result;
}

static int CEM102_SetupRECEResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                           CEM102_CurrentData *p_data)
{
    int result = p_cem102->REDACConfig(cem102_dut, DAC_RE_DISABLE,
                                       RE_SR1_CONNECT | RE_SR2_CONNECT,
                                       RE_BUF_DISABLE);
    if (result != ERRNO_NO_ERROR)
    {
        result = CEM102_ConfigREResidualCurrent(p_cem102, p_data);
    }
    return result;
}

static int CEM102_SetupREResidualCurrent(DRIVER_CEM102_t *p_cem102,
                                         CEM102_CurrentData *p_data)
{
    int result = p_cem102->REDACConfig(cem102_dut, DAC_RE_DISABLE,
                                       RE_SR2_CONNECT, RE_BUF_DISABLE);
    if (result != ERRNO_NO_ERROR)
    {
        result = CEM102_ConfigREResidualCurrent(p_cem102, p_data);
    }
    return result;
}

static int CEM102_SetupOffsetCurrent(DRIVER_CEM102_t *p_cem102,
                                     CEM102_CurrentData *p_data)
{
    /* Configure the ADC to prepare for the ChannelOffsetCurrent */
    uint16_t sw_conn[2] = { CH1_SC119_SC120_CONNECT,
                            CH2_SC219_SC220_CONNECT
                          };
    return CEM102_ConfigChannelSwitch(p_cem102, p_data->lsad_req, sw_conn);
}

static int CEM102_SetupDACVoltage(DRIVER_CEM102_t *p_cem102,
                                  CEM102_CurrentData *p_data)
{
    CEM102_DAC_TYPE dac = CEM102_GetChanNumber(p_data->lsad_req);

    int result = p_cem102->SetupADCToMeasureDAC(cem102_dut, dac,
                                                CEM102_PRE_MEASURE_CONFIG);
    CEM102_AdjustADCGain(p_cem102, p_data);

    if (result == ERRNO_NO_ERROR)
    {
        uint8_t index = dac - 1;
        uint16_t we_en[3] = { DAC_WE1_ENABLE, DAC_WE2_ENABLE, DAC_RE_ENABLE };
        uint16_t dac_setting = 0;

        result = p_cem102->CalculateDACAmplitude(cem102_dut, dac,
                                                 p_data->target_mv, &dac_setting);
        if (result == ERRNO_GENERAL_FAILURE)
        {
            float val = (float)calib_data.dac_setting[index];
            if (p_data->target_mv)
            {
                val *= (float)p_data->target_mv;
                val /= (float)WE_DAC_TARGET_MV;
            }
            dac_setting = (uint16_t)val;
        }

        uint16_t dac_v = 0;
        uint8_t dac_reg[3] = { DC_DAC_WE1_CTRL,
                               DC_DAC_WE2_CTRL,
                               DC_DAC_RE_CTRL
                             };
        uint16_t ctrl_mask[3] = { DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK,
                                  DC_DAC_WE2_CTRL_DAC_WE2_VALUE_MASK,
                                  DC_DAC_RE_CTRL_DAC_RE_VALUE_MASK
                                };
        result = p_cem102->RegisterRead(cem102_dut, dac_reg[index], &dac_v);
        if (result == ERRNO_NO_ERROR)
        {
            uint16_t irq_cfg = DC_DAC_LOAD_IRQ_DISABLE;

            dac_v &= ctrl_mask[index];
            if (dac_v != dac_setting)
            {
                CEM102_SetLSADState(CEM102_LSAD_STATE_DAC_LOADED);
                CEM102_Enable_DAC_LOAD_IRQ(p_cem102, true);
                irq_cfg = DC_DAC_LOAD_IRQ_ENABLE;
            }
            if (dac == RE)
            {
                result |= p_cem102->REDACConfig(cem102_dut, ((ctrl_mask[index] &
                                                dac_setting) | we_en[index]),
                                                (RE_SR1_CONNECT | RE_SR2_CONNECT),
                                                (RE_BUF_ENABLE |
                                                RE_HP_MODE_DISABLE));
            }
            else
            {
                result |= p_cem102->WEDACConfig(cem102_dut, (int)dac,
                                                ((ctrl_mask[index] &
                                                dac_setting) | we_en[index]),
                                                irq_cfg);
            }
            if (dac_v == dac_setting)
            {
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
    return p_cem102->MeasureReference(cem102_dut, chan);
}

static void CEM102_MeasureWithLSAD(DRIVER_CEM102_t *p_cem102,
                                   CEM102_CurrentData *p_data,
                                   uint16_t diagmask)
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
                result = CEM102_StartMeasurement(p_cem102, SINGLE_DELAY_CMD);
            }
            if (result != ERRNO_NO_ERROR)
            {
                /* Error. Restore the registers if needed before bailing out */
                CEM102_LSADRestoreState(p_cem102, p_data, diagmask);
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
                        memset(measure_buf, 0, sizeof(measure_buf));
                        result = p_cem102->QueueRead(cem102_dut,
                                                     SYS_BUFFER_WORD0,
                                                     MEASUREMENT_LENGTH);
                    }
                }
            }
            if (result != ERRNO_NO_ERROR)
            {
                /* Bail out on error */
                CEM102_LSADRestoreState(p_cem102, p_data, diagmask);
            }
            break;
        }
        case CEM102_LSAD_STATE_DATA_AVAILABLE:
        {
            p_cem102->RegisterBufferRead(cem102_dut, measure_buf,
                                       MEASUREMENT_LENGTH);
            CEM102_LSADRestoreState(p_cem102, p_data, diagmask);
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

static void CEM102_UpdateCalibCurrentRatio(DRIVER_CEM102_t *p_cem102)
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
        swmLogInfo("CalibCurrRatio(%f/%f=%f)\r\n", curr_measured,
                   curr_from_otp, curr_measured / curr_from_otp);
#endif /* SWMTRACE_OUTPUT */
    }
    diagData.diagreq &= ~CEM102_DIAG_CAL_CURR_RATIO;
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
        diagData.p_saved_data != &saved_regs)
    {
        diagData.p_saved_data = &saved_regs;
        diagData.lsad_state = CEM102_LSAD_STATE_SETUP;
        diagData.regs_saved = false;
        diagData.diagreq = CEM102_DIAG_REQUESTS;
        diagData.diagreq_saved = diagData.diagreq;

        if (diagData.diagreq != 0)
        {
            if (calib_ref_measurement == 0)
            {
                diagData.diagreq |= CEM102_CALIB_CURRRENT;
            }

            /* Disconnect WE1 & WE2 before starting diagnostics */
            CEM102_SetElectrodeInputs(p_cem102, false);

            /* Clear any existing events & go to IDLE mode in case LSAD is
             * already measuring.
             */
            CEM102_SendCommand(p_cem102, IDLE_CMD);
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

#endif /* CEM102_DIAG_FUNCTIONS */

static void CEM102_DiagnosticsInternal(DRIVER_CEM102_t *p_cem102)
{
#if CEM102_DIAG_FUNCTIONS
    /* Initialize, if not done already. */
    CEM102_DiagnosticsInit(p_cem102);

    uint16_t diagmask = 0;
    for (uint16_t idx = 0; idx < CEM102_DIAG_NUM_REQUESTS; idx++)
    {
        if (diagData.diagreq & (1U << idx))
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
        case CEM102_CALIB_CURRRENT:
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
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);
            break;
        }
        case CEM102_DIAG_CAL_CURR_RATIO:
        {
            CEM102_UpdateCalibCurrentRatio(p_cem102);
            break;
        }
        case CEM102_DIAG_WE1_OFF_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG1_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE1;
            data.setup_callback = CEM102_SetupOffsetCurrent;
            data.result_callback = CEM102_ResultOffsetCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);
            break;
        }
        case CEM102_DIAG_WE2_OFF_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG1_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE2;
            data.setup_callback = CEM102_SetupOffsetCurrent;
            data.result_callback = CEM102_ResultOffsetCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);
            break;
        }
        case CEM102_DIAG_WE1_DAC_VOLT:
        {
            data.lsad_req = CEM102_LSAD_READ_WE1;
            data.target_mv = WE_DAC_TARGET_MV;
            diagData.setup_changes_states = true;
            CEM102_InitRegsToSaveForDacVolt(&data);
            data.setup_callback = CEM102_SetupDACVoltage;
            data.result_callback = CEM102_ResultDACVoltage;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);
            break;
        }
        case CEM102_DIAG_WE2_DAC_VOLT:
        {
            data.lsad_req = CEM102_LSAD_READ_WE2;
            data.target_mv = WE_DAC_TARGET_MV;
            diagData.setup_changes_states = true;
            CEM102_InitRegsToSaveForDacVolt(&data);
            data.setup_callback = CEM102_SetupDACVoltage;
            data.result_callback = CEM102_ResultDACVoltage;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);
            break;
        }
        case CEM102_DIAG_RE_DAC_VOLT:
        {
            data.lsad_req = CEM102_LSAD_READ_RE;
            data.target_mv = WE_DAC_TARGET_MV;
            diagData.setup_changes_states = true;
            CEM102_InitRegsToSaveForDacVolt(&data);
            data.setup_callback = CEM102_SetupDACVoltage;
            data.result_callback = CEM102_ResultDACVoltage;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);
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
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);

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
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);
            break;
        }
        case CEM102_DIAG_WE1_RES_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG1_SAVE |
                              REG_ANA_CFG1_SAVE | REG_DC_DAC_RE_CTRL_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE1;
            data.setup_callback = CEM102_SetupResidualCurrent;
            data.result_callback = CEM102_ResultResidualCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);
            break;
        }
        case CEM102_DIAG_WE2_RES_CURR:
        {
            data.valid_regs = REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG1_SAVE |
                              REG_ANA_CFG1_SAVE | REG_DC_DAC_RE_CTRL_SAVE;
            data.lsad_req = CEM102_LSAD_READ_WE2;
            data.setup_callback = CEM102_SetupResidualCurrent;
            data.result_callback = CEM102_ResultResidualCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);
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
            data.target_mv = WE_DAC_TARGET_MV;
            data.setup_callback = CEM102_SetupREResidualCurrent;
            data.result_callback = CEM102_ResultREResidualCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);

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
            data.target_mv = WE_DAC_TARGET_MV;
            data.setup_callback = CEM102_SetupRECEResidualCurrent;
            data.result_callback = CEM102_ResultRECEResidualCurrent;
            CEM102_MeasureWithLSAD(p_cem102, &data, diagmask);
            break;
        }
        default:
        {
            break;
        }
    }

    if (CEM102_DiagnosticsDone(p_cem102))
    {
        /* Clear all the events return in a clean state, in IDLE mode, the same
         * way we entered the diagnostics mode.
         */
        CEM102_SendCommand(p_cem102, IDLE_CMD);
    }
#endif /* CEM102_DIAG_FUNCTIONS */
}

/* Should be called only when CEM102_DiagnosticsDone returns false.  */
void CEM102_Diagnostics(DRIVER_CEM102_t *p_cem102)
{
#if SWMTRACE_OUTPUT
    uint8_t loop = 0;
#endif /* SWMTRACE_OUTPUT */

    do
    {
#if SWMTRACE_OUTPUT
        if (++loop > 1)
        {
            swmLogInfo("Calling again(%d)\r\n", loop);
        }
#endif /* SWMTRACE_OUTPUT */
        CEM102_DiagnosticsInternal(p_cem102);
    } while ((!CEM102_DiagnosticsDone(p_cem102)) &&
           (CEM102_GetLSADState() == CEM102_LSAD_STATE_SETUP));
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
        }
    }
#endif /* CEM102_DIAG_FUNCTIONS */
    return result;
}

