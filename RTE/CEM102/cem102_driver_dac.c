/**
 * @file cem102_driver_dac.c
 * @brief CEM102 DAC/ADC related functions.
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

#include <string.h>
#include <cem102_driver.h>

#define CEM102_DAC_VALUE_FOR_56MV     (100)
#define CEM102_DAC_ERROR_THRESHOLD    (2500)  /* microvolts */

/* CEM102_Get_LSAD_Config function relies on the following facts.
 * 1. ORSD shift position is 0. So, no shifting need while handling this input.
 * 2. Mask and values are same for LSAD1 and LSAD2. We don't need to have
 *    separate mask/value pair for each channel.
 * 3. The table below shows the expected ADC_CHOP_CTRL and BUF_CHOP_CTRL settings
 *    for a given SFCR and SYS_CHOP setting.
 *
 *                +--------------------+------------------+---------------------+---------------------+
 *                | SYS_CHOP_DISABLE   | SYS_CHOP_ENABLE  |  SYS_CHOP_DISABLE   |   SYS_CHOP_ENABLE   |
 * +--------------+--------------------+------------------+---------------------+---------------------+
 * |    SFCR      |   ADC_CHOP_CTRL    |   ADC_CHOP_CTRL  |    BUF_CHOP_CTRL    |   BUF_CHOP_CTRL     |
 * |--------------|--------------------+------------------+---------------------+---------------------+
 * |ORSD1_RSD19   |ADC_CHOP_CLK_DISABLE|ADC_CHOP_CLK_DIV2 |BUF_CHOP_CLK_DISABLE |BUF_CHOP_CLK_DISABLE |
 * |ORSD2_RSD18   |ADC_CHOP_CLK_DIV2   |ADC_CHOP_CLK_DIV2 |BUF_CHOP_CLK_DISABLE |BUF_CHOP_CLK_DIV2    |
 * |ORSD4_RSD18   |ADC_CHOP_CLK_DIV2   |ADC_CHOP_CLK_DIV2 |BUF_CHOP_CLK_DIV2    |BUF_CHOP_CLK_DIV4    |
 * |ORSD8_RSD18   |ADC_CHOP_CLK_DIV4   |ADC_CHOP_CLK_DIV4 |BUF_CHOP_CLK_DIV4    |BUF_CHOP_CLK_DIV8    |
 * |ORSD16_RSD16  |ADC_CHOP_CLK_DIV8   |ADC_CHOP_CLK_DIV8 |BUF_CHOP_CLK_DIV8    |BUF_CHOP_CLK_DIV16   |
 * |ORSD32_RSD16  |ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV16   |BUF_CHOP_CLK_DIV32   |
 * |ORSD64_RSD14  |ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV32   |BUF_CHOP_CLK_DIV64   |
 * |ORSD128_RSD14 |ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV64   |BUF_CHOP_CLK_DIV128  |
 * |ORSD256_RSD12 |ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV128  |BUF_CHOP_CLK_DIV256  |
 * |ORSD512_RSD12 |ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV256  |BUF_CHOP_CLK_DIV512  |
 * |ORSD1024_RSD10|ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV512  |BUF_CHOP_CLK_DIV1024 |
 * |ORSD2048_RSD10|ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV1024 |BUF_CHOP_CLK_DIV2048 |
 * |ORSD4096_RSD8 |ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV2048 |BUF_CHOP_CLK_DIV4096 |
 * |ORSD8192_RSD8 |ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV4096 |BUF_CHOP_CLK_DIV8192 |
 * |ORSD16384_RSD6|ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV8192 |BUF_CHOP_CLK_DIV16384|
 * |ORSD32768_RSD6|ADC_CHOP_CLK_DIV16  |ADC_CHOP_CLK_DIV16|BUF_CHOP_CLK_DIV16384|BUF_CHOP_CLK_DIV32768|
 */
void CEM102_Get_LSAD_Config(const CEM102_Device *p_cfg, uint16_t *p_lsad_cfg,
                            uint16_t *p_mask)
{
    int result = ERRNO_NO_ERROR;

    if ((p_lsad_cfg == NULL) || (p_cfg == NULL))
    {
        result = ERRNO_PARAM_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        uint16_t orsd = *p_lsad_cfg & CHCFG_CH1_LSAD_LSAD1_SFCR_MASK;
        uint16_t sys_chop_cfg = *p_lsad_cfg & CHCFG_CH1_LSAD_LSAD1_SYSTEM_CHOP_CFG_MASK;
        uint16_t adc_cfg[2][5] =
        {
            { LSAD1_ADC_CHOP_CLK_DISABLE, LSAD1_ADC_CHOP_CLK_DIV2,
              LSAD1_ADC_CHOP_CLK_DIV2, LSAD1_ADC_CHOP_CLK_DIV4,
              LSAD1_ADC_CHOP_CLK_DIV8
            },
            { LSAD1_ADC_CHOP_CLK_DIV2, LSAD1_ADC_CHOP_CLK_DIV2,
              LSAD1_ADC_CHOP_CLK_DIV2, LSAD1_ADC_CHOP_CLK_DIV4,
              LSAD1_ADC_CHOP_CLK_DIV8
            },
        };
        uint16_t buf_chop_clk[2][2] =
        {
            { LSAD1_BUF_CHOP_CLK_DISABLE, LSAD1_BUF_CHOP_CLK_DISABLE },
            { LSAD1_BUF_CHOP_CLK_DISABLE, LSAD1_BUF_CHOP_CLK_DIV2 }
        };

        /* Clear the fields we are going to set. Leave the rest untouched. */
        *p_lsad_cfg &= ~CHCFG_CH1_LSAD_LSAD1_ADC_CHOP_CTRL_MASK;
        *p_lsad_cfg &= ~CHCFG_CH1_LSAD_LSAD1_BUF_CHOP_CTRL_MASK;

        /* Value of index is from LSADx_SYS_CHOP_ENABLE/DISABLE setting */
        uint16_t index = sys_chop_cfg >> CHCFG_CH1_LSAD_LSAD1_SYSTEM_CHOP_CFG_POS;

        /* Fill ADC_CHOP_CLOCK fields */
        uint16_t val = orsd >> CHCFG_CH1_LSAD_LSAD1_SFCR_POS;
        if (orsd >= LSAD1_ORSD32_RSD16)
        {
            *p_lsad_cfg |= LSAD1_ADC_CHOP_CLK_DIV16;
        }
        else
        {
            *p_lsad_cfg |= adc_cfg[index][val];
        }

        /* Fill BUF_CHOP_CLOCK fields */
        if (orsd >= LSAD1_ORSD4_RSD18)
        {
            val -= 1;
            if (sys_chop_cfg == LSAD1_SYS_CHOP_DISABLE)
            {
                val -= 1;
            }
            *p_lsad_cfg |= (val << CHCFG_CH1_LSAD_LSAD1_BUF_CHOP_CTRL_POS);
        }
        else
        {
            *p_lsad_cfg |= buf_chop_clk[index][val];
        }

        if (p_mask != NULL)
        {
            *p_mask |= (CHCFG_CH1_LSAD_LSAD1_SFCR_MASK |
                        CHCFG_CH1_LSAD_LSAD1_BUF_CHOP_CTRL_MASK |
                        CHCFG_CH1_LSAD_LSAD1_ADC_CHOP_CTRL_MASK |
                        CHCFG_CH1_LSAD_LSAD1_SYSTEM_CHOP_CFG_MASK);
        }
    }
}

static int Config_ADC_PostDACCalibration(const CEM102_Device *p_cfg,
                                         CEM102_DAC_TYPE chan)
{
    int result = ERRNO_NO_ERROR;
    uint32_t index = chan - 1;

    /* Most API, chan is one of WE1, WE2, RE. Some API expects WE1 for RE */
    CEM102_DAC_TYPE new_chan = (chan == RE) ? WE1 : chan;

    if (chan == RE)
    {
        uint16_t dac_re_value = CEM102_GetDACValue(p_cfg, chan);

        /* Disconnect RE_SR swithches, Disable RE Buffer and RE buffer
         * high power mode. */
        result |= CEM102_Signal_REDACConfig(p_cfg, dac_re_value, 0, 0);
    }

    /* Disconnect all SC switches */
    uint16_t sw_disc[3] = { 0, 0, CH1_SC109_SC110_DISCONNECT };
    result |= CEM102_Signal_SwitchConfig(p_cfg, new_chan, sw_disc[index]);

    /* Disconnect all the ST switches */
    result |= CEM102_Signal_RE_ATBus_Config(p_cfg, RE, 0);
    return result;
}

static int Config_ADC_PreDACCalibration(const CEM102_Device *p_cfg,
                                        CEM102_DAC_TYPE chan)
{
    uint32_t index = (uint32_t)chan - 1;

    /* Most API, chan is one of WE1, WE2, RE. Some API expects WE1 for RE */
    CEM102_DAC_TYPE new_chan = (chan == RE) ? WE1 : chan;

    /* Disconnect all switch configurations (SA1 to SA14) in analog test bus. */
    int result = CEM102_Register_Write(p_cfg, ANA_SW_CFG2, ATBUS_SA1_DISCONNECT);

    /* Calculate the gain of the ADC for the given channel. Value from the
     * sample (CEM102_ACCUM_SAMPLE_CNT_2) as programmed in CHCFG_CHx_ACCUM
     * register is one less than the value of the sample count.
     */
    uint16_t sample_cnt = CEM102_ACCUM_SAMPLE_CNT_2;
    result |= CEM102_Signal_CalculateADCGain(new_chan, sample_cnt + 1);
    if (chan == RE)
    {
        uint16_t dac_re_value = CEM102_GetDACValue(p_cfg, chan);

        /* DAC Calibration Configuration:
         * RE : SC119 & SC120=OFF, RE =AT1, AT2=VSSA, use LSAD1
         */
        result |= CEM102_Signal_REDACConfig(p_cfg, dac_re_value,
                                            (RE_SR1_CONNECT | RE_SR2_CONNECT),
                                            (RE_BUF_ENABLE | RE_HP_MODE_DISABLE));
    }
    else
    {
        uint16_t tia_enable[2] = { CH1_TIA_ENABLE, CH2_TIA_ENABLE };
        uint16_t tia_cfg[2] = { CH1_TIA_INT_FB_ENABLE | CH1_TIA_FB_RES_1M,
                                CH2_TIA_INT_FB_ENABLE | CH2_TIA_FB_RES_1M
                              };
        result |= CEM102_Signal_TIAConfig(p_cfg, chan, tia_enable[index],
                                          tia_cfg[index]);

    }
    uint16_t ch_cfg[3] = { CEM102_CH1_BIAS_PRESCALE1 |
                           CEM102_CH1_LPF_CUTOFF_10KHZ,
                           CEM102_CH2_BIAS_PRESCALE1 |
                           CEM102_CH2_LPF_CUTOFF_10KHZ,
                           CEM102_CH1_BIAS_PRESCALE1 |
                           CEM102_CH1_LPF_CUTOFF_10KHZ
                         };
    uint16_t buf_cfg[3] = { CH1_BUF_ENABLE | CH1_BUF_BYPASS_ENABLE,
                            CH2_BUF_ENABLE | CH2_BUF_BYPASS_ENABLE,
                            CH1_BUF_ENABLE | CH1_BUF_BYPASS_ENABLE
                          };
    result |= CEM102_Signal_BufferConfig(p_cfg, new_chan, ch_cfg[index],
                                         buf_cfg[index]);

    /* DAC Calibration Configuration:
     * WE1: SC119 & SC120=OFF, WE1=AT2, AT1=VSSA
     * WE2: SC219 & SC220=OFF, WE2=AT1, AT2=VSSA
     */
    uint16_t sw_conn[3] = { CH1_SC111_DISCONNECT | CH1_SC113_CONNECT |
                            CH1_SC115_CONNECT | CH1_SC116_CONNECT |
                            CH1_SC119_SC120_DISCONNECT,
                            CH2_SC212_DISCONNECT | CH2_SC213_CONNECT |
                            CH2_SC215_CONNECT | CH2_SC216_CONNECT |
                            CH2_SC219_SC220_DISCONNECT,
                            CH1_SC111_DISCONNECT | CH1_SC112_DISCONNECT |
                            CH1_SC113_DISCONNECT | CH1_SC114_DISCONNECT |
                            CH1_SC115_CONNECT | CH1_SC116_CONNECT |
                            CH1_SC117_SC118_DISCONNECT |
                            CH1_SC119_SC120_DISCONNECT
                          };
    result |= CEM102_Signal_SwitchConfig(p_cfg, new_chan, sw_conn[index]);
    uint16_t at_conn[3] = { ATBUS_ST14_CONNECT | ATBUS_ST12_CONNECT |
                            ATBUS_ST21_DISCONNECT | ATBUS_ST17_DISCONNECT,
                            ATBUS_ST16_CONNECT | ATBUS_ST22_CONNECT |
                            ATBUS_ST21_DISCONNECT | ATBUS_ST17_DISCONNECT,
                            ATBUS_ST16_CONNECT | ATBUS_ST17_DISCONNECT |
                            ATBUS_ST21_DISCONNECT | ATBUS_ST22_CONNECT
                          };
    result |= CEM102_Signal_RE_ATBus_Config(p_cfg, RE, at_conn[index]);
    result |= CEM102_Signal_ClockConfig(p_cfg, LSAD_PRESCALE1,
                                        CEM102_LSAD_START_DELAY_6MS);
    uint16_t lsad_cfg[3] = { LSAD1_GAIN_UNITY | LSAD1_SYS_CHOP_ENABLE |
                             LSAD1_ORSD64_RSD14 | LSAD1_ITRIM_1p00,
                             LSAD2_GAIN_UNITY | LSAD2_SYS_CHOP_ENABLE |
                             LSAD2_ORSD64_RSD14 | LSAD2_ITRIM_1p00,
                             LSAD1_GAIN_UNITY | LSAD1_SYS_CHOP_ENABLE |
                             LSAD1_ORSD64_RSD14 | LSAD1_ITRIM_1p00
                           };
    uint16_t thresh_cfg[3] = { CH1_VIOLATION_CHECK_DISABLE,
                               CH2_VIOLATION_CHECK_DISABLE,
                               CH1_VIOLATION_CHECK_DISABLE
                             };
    uint16_t buffer_cfg[3] = { CH1_DOUBLE | CH1_VIOLATION_ABS_DISABLE,
                               CH2_DOUBLE | CH2_VIOLATION_ABS_DISABLE,
                               CH1_DOUBLE | CH1_VIOLATION_ABS_DISABLE
                             };
    uint16_t accum_cfg[3] = { (CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_MASK &
                              sample_cnt) |
                              CH1_MEASUREMENT_ENABLE,
                              (CHCFG_CH2_ACCUM_CH2_ACCUM_SAMPLE_CNT_MASK &
                              sample_cnt) |
                              CH2_MEASUREMENT_ENABLE,
                              (CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_MASK &
                              sample_cnt) |
                              CH1_MEASUREMENT_ENABLE
                            };
    CEM102_Get_LSAD_Config(p_cfg, &lsad_cfg[index], NULL);
    result |= CEM102_Signal_ADCConfig(p_cfg, new_chan, lsad_cfg[index],
                                      thresh_cfg[index], buffer_cfg[index],
                                      accum_cfg[index]);

    /* Configure the given channel's switches and buffers for direct ADC
     * measurements
     */
    result |= CEM102_Signal_DirectADCConfig(p_cfg, new_chan);
    return result;
}

static uint16_t DAC_Get_NominalValue(int32_t target_voltage_mv)
{
    double mvolts_f = target_voltage_mv;

    uint16_t dac_value = (uint16_t)((mvolts_f * 4096.0f) / 2300.0f);
    return dac_value;
}

static int32_t DAC_Get_ErrorValue(int32_t target, int32_t measured)
{
    int32_t error;

    if (target < measured)
    {
        error = measured - target;
    }
    else
    {
        error = target - measured;
    }
    return error;
}

/* Supports RE DAC. Support for WE1 and WE2 can be added, if needed. */
uint16_t CEM102_GetDACValue(const CEM102_Device *p_cfg, CEM102_DAC_TYPE dac)
{
    uint16_t dac_v = (CEM102_DAC_MIN + CEM102_DAC_MAX) / 2; /* Default value. */

    if (dac == RE)
    {
        CEM102_Register_Read(p_cfg, DC_DAC_RE_CTRL, &dac_v);
        dac_v &= DC_DAC_RE_CTRL_DAC_RE_VALUE_MASK;
        dac_v >>= DC_DAC_RE_CTRL_DAC_RE_VALUE_POS;
    }
    return dac_v;
}

int CEM102_DAC_ADC_Setup(const CEM102_Device *p_cfg, CEM102_DAC_TYPE dac, uint32_t flag)
{
    int result = ERRNO_PARAM_ERROR;

    if ((dac == WE1) || (dac == WE2) || (dac == RE))
    {
        result = ERRNO_NO_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        if (flag == CEM102_POST_MEASURE_CONFIG)
        {
            result = Config_ADC_PostDACCalibration(p_cfg, (uint32_t)dac);
        }
        else if (flag == CEM102_PRE_MEASURE_CONFIG)
        {
            result = Config_ADC_PreDACCalibration(p_cfg, (uint32_t)dac);
        }
    }
    return result;
}

int CEM102_DAC_CalibrateWithADC(const CEM102_Device *p_cfg, uint32_t chan,
                                uint16_t mvolts, uint16_t *p_dac)
{
    int result = ERRNO_NO_ERROR;
    uint16_t min = 1;
    int32_t error = 0;
    uint16_t expected = 0;
    uint16_t current_setting = 0;
    uint16_t previous_setting = 0;
    int32_t current_voltage = 0;

    /* Mask is common for both channels. */
    uint16_t max = DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK;
    uint32_t target = (uint32_t)mvolts * CEM102_VOLTAGE_MV_TO_UV;
    uint32_t index = chan - 1;
    uint16_t ctrl_mask[3] = { DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK,
                              DC_DAC_WE2_CTRL_DAC_WE2_VALUE_MASK,
                              DC_DAC_RE_CTRL_DAC_RE_VALUE_MASK
                            };
    uint16_t we_en[3] = { DAC_WE1_ENABLE, DAC_WE2_ENABLE, DAC_RE_ENABLE };

    if ((chan != WE1) && (chan != WE2) && (chan != RE))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }

    bool saved = false;
    CEM102_RegSaveData saved_regs;
    memset(&saved_regs, 0, sizeof(saved_regs));

    if (result == ERRNO_NO_ERROR)
    {
        saved_regs.valid = REG_ANA_CFG0_SAVE | REG_ANA_CFG1_SAVE |
                           REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG2_SAVE |
                           REG_CHCFG_LSAD_CLK_SAVE | REG_CHCFG_CH1_LSAD_SAVE |
                           REG_CHCFG_CH2_LSAD_SAVE | REG_CHCFG_CH1_ACCUM_SAVE |
                           REG_CHCFG_CH2_ACCUM_SAVE | REG_DC_DAC_WE2_CTRL_SAVE|
                           REG_DC_DAC_WE1_CTRL_SAVE | REG_ANA_SW_CFG1_SAVE |
                           REG_DC_DAC_RE_CTRL_SAVE;


        int ret = CEM102_Register_Save(p_cfg, &saved_regs);
        if (ret == ERRNO_NO_ERROR)
        {
            saved = true;
        }

        /* Configure the ADC to prepare for the DAC calibration */
        result = Config_ADC_PreDACCalibration(p_cfg, chan);

        /* Time for ADC to settle down with new settings */
        Sys_Delay(CEM102_SYS_DELAY_50MS);

        expected = DAC_Get_NominalValue(mvolts);

        /* Set the range to search through. Assumption is that 56 mV on either
         * side of the theoretical value should be OK.
         *
         * Note: DAC has 0.56 mV resolution per step.
         */
        if (expected > CEM102_DAC_VALUE_FOR_56MV)
        {
            min = expected - CEM102_DAC_VALUE_FOR_56MV;
        }
        if (expected < (max - CEM102_DAC_VALUE_FOR_56MV))
        {
            max = expected + CEM102_DAC_VALUE_FOR_56MV;
        }
        previous_setting = min;

        do
        {
            current_setting = ((max - min) / 2) + min;
            if ((current_setting == (max - 1)) &&
                (current_setting == previous_setting))
            {
                current_setting = max;
            }

            if (chan == RE)
            {
                result |= CEM102_Signal_REDACConfig(p_cfg, ((ctrl_mask[index] &
                                                    current_setting) | we_en[index]),
                                                    (RE_SR1_CONNECT | RE_SR2_CONNECT),
                                                    (RE_BUF_ENABLE | RE_HP_MODE_DISABLE));
            }
            else
            {
                result |= CEM102_Signal_WEDACConfig(p_cfg, chan,
                                                    ((ctrl_mask[index] &
                                                    current_setting) | we_en[index]),
                                                    DC_DAC_LOAD_IRQ_DISABLE);
            }
            result |= CEM102_Signal_MeasureVoltage(p_cfg, chan, &current_voltage, NULL);

            if (target == current_voltage)
            {
                /* Right setting is found */
                error = 0;
                break;
            }
            else
            {
                error = DAC_Get_ErrorValue(target, current_voltage);
            }

            if (previous_setting == current_setting)
            {
                /* Calibration not successful */
                break;
            }
            if (current_voltage < target)
            {
                min = current_setting;
            }
            else if (current_voltage > target)
            {
                max = current_setting;
            }
            previous_setting = current_setting;

            if ((max == min) ||
                ((max > min) && ((max - min) < 2)) ||
                ((max < min) && ((min - max) < 2)))
            {
                break;
            }
        } while (result == ERRNO_NO_ERROR);
    }

    if (result == ERRNO_NO_ERROR)
    {
        int32_t new_voltage = 0;
        int32_t new_error = 0;
        uint16_t new_setting = 0;

        if ((target != current_voltage) && (max != min))
        {
            if (max == current_setting)
            {
                current_setting = new_setting = min;
            }
            else
            {
                current_setting = new_setting = max;
            }

            if (chan == RE)
            {
                result |= CEM102_Signal_REDACConfig(p_cfg, ((ctrl_mask[index] &
                                                    new_setting) | we_en[index]),
                                                    (RE_SR1_CONNECT | RE_SR2_CONNECT),
                                                    (RE_BUF_ENABLE | RE_HP_MODE_DISABLE));
            }
            else
            {
                result |= CEM102_Signal_WEDACConfig(p_cfg, chan,
                                                    ((ctrl_mask[index] &
                                                    new_setting) | we_en[index]),
                                                    DC_DAC_LOAD_IRQ_DISABLE);
            }
            result |= CEM102_Signal_MeasureVoltage(p_cfg, chan, &new_voltage, NULL);
            new_error = DAC_Get_ErrorValue(target, new_voltage);
            if (new_error < error)
            {
                error = new_error;
                current_setting = new_setting;
                current_voltage = new_voltage;
            }
        }
    }
    *p_dac = current_setting;

    /* Post calibration clean-up. Turn the switches that are used, to off position */
    result |= Config_ADC_PostDACCalibration(p_cfg, chan);

    if (saved)
    {
        /* Restore user configuration */
        CEM102_Register_Restore(p_cfg, &saved_regs);
    }

    /* Due to some reason, if error deviates more than certain threshold, 2.5 mV,
     * an error code is returned to indicate this anomaly.
     */
    if (error > CEM102_DAC_ERROR_THRESHOLD)
    {
        result = ERRNO_CALIBRATION_ERROR;
    }
    return result;
}

int CEM102_DAC_Calibrate(const CEM102_Device *p_cfg, uint32_t dac,
                         uint16_t mvolts, uint16_t *p_setting)
{
    int result = ERRNO_NO_ERROR;

    if ((dac != WE1) && (dac != WE2) && (dac != RE))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }

    uint16_t otp_ver = CEM102_OTP_GetVersion();
    if ((otp_ver == CEM102_OTP_VERSION_3) || (otp_ver == CEM102_OTP_VERSION_1))
    {
        result = ERRNO_GENERAL_FAILURE;
    }

    if (result == ERRNO_NO_ERROR)
    {
        int16_t we_offset = 0;
        uint16_t dac_setting = 0;

        result |= CEM102_Signal_CalculateDACOffset(dac, &we_offset);
        if (trim_struct_v1 != NULL)
        {
            uint32_t v_value = (uint32_t)mvolts;
            dac_setting = (uint16_t)(CEM102_DAC_MV(v_value) - (uint32_t)we_offset);
        }
        else if (trim_struct != NULL)
        {
            double dac_gain = 0;

            result |= CEM102_Signal_CalculateDACGain(dac, &dac_gain);
            if (result == ERRNO_NO_ERROR)
            {
                double dac_offset = (((float)we_offset - CEM102_DAC_OFFSET_O2) /
                                     CEM102_DAC_OFFSET_G2) * 1000;
                dac_setting = (uint16_t)(((double)mvolts - dac_offset) /
                               dac_gain);
            }
        }
        *p_setting = dac_setting;
    }
    return result;
}

