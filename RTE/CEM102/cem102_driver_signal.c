/**
 * @file cem102_driver_signal.c
 * @brief CEM102 Driver Signal Path implementation
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

/* ADC gain global variable. Units are uV/LSB */
static float adc_gain[3] = {
                               CEM102_ADC_GAIN_NOM,
                               CEM102_ADC_GAIN_NOM,
                               CEM102_ADC_GAIN_NOM,
                           };

/* The order of registers given below must match with bit definitions of
 * REGISTER_DATA_VALID. Number of registers in the array for ch1 and ch2 must also be same.
 * These definitions need not be exposed to outside this file.
 */
#define CH1_REGS_TO_SAVE   ( REG_ANA_CFG0_SAVE | REG_ANA_CFG1_SAVE | \
                             REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG3_SAVE | \
                             REG_CHCFG_CH1_LSAD_SAVE | REG_CHCFG_CH1_THRESHOLD_SAVE | \
                             REG_CHCFG_CH1_BUFFER_SAVE | REG_CHCFG_CH1_ACCUM_SAVE \
                           )

#define CH2_REGS_TO_SAVE   ( REG_ANA_CFG0_SAVE | REG_ANA_CFG1_SAVE | \
                             REG_ANA_SW_CFG1_SAVE | REG_ANA_SW_CFG3_SAVE | \
                             REG_CHCFG_CH2_LSAD_SAVE | REG_CHCFG_CH2_THRESHOLD_SAVE | \
                             REG_CHCFG_CH2_BUFFER_SAVE | REG_CHCFG_CH2_ACCUM_SAVE \
                           )

/* ----------------------------------------------------------------------------
 * Configuration Functions
 * --------------------------------------------------------------------------*/

static uint32_t CEM102_Signal_PackMeasData(uint16_t p_data[], uint8_t msb_index,
                                           uint8_t lsb_index)
{
    uint32_t val = 0;
    if ((msb_index < CEM102_MEASUREMENT_LENGTH) &&
        (lsb_index < CEM102_MEASUREMENT_LENGTH))
    {
        val = p_data[msb_index];
        val <<= CEM102_SHIFT_FOR_WORD;
        val |= p_data[lsb_index];
    }
    return val;
}

int CEM102_Signal_ClockConfig(const CEM102_Device *p_cfg, uint16_t prescale, uint16_t delay)
{
    int result = ERRNO_NO_ERROR;

    if (p_cfg == NULL)
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        result = CEM102_Register_Write(p_cfg, CHCFG_LSAD_CLK,
                                       (((delay << CHCFG_LSAD_CLK_LSAD_START_DLY_POS)
                                       & CHCFG_LSAD_CLK_LSAD_START_DLY_MASK) |
                                       LSAD1_START_DLY_ENABLE | LSAD2_START_DLY_ENABLE |
                                       LSAD1_OFFSET0 | LSAD2_OFFSET0 |
                                       (prescale & CHCFG_LSAD_CLK_LSAD_CLK_PRESCALE_MASK)));
    }
    return result;
}

int CEM102_Signal_TIAConfig(const CEM102_Device *p_cfg, int tia, uint16_t enable, uint16_t tia_cfg)
{
    uint16_t temp_ana0 = CEM102_INITIAL_VALUE;
    uint16_t temp_ana1 = CEM102_INITIAL_VALUE;
    int result = ERRNO_NO_ERROR;

    if (p_cfg == NULL)
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        result = CEM102_Register_Read(p_cfg, ANA_CFG0, &temp_ana0);
        if (result == ERRNO_NO_ERROR)
        {
            result = CEM102_Register_Read(p_cfg, ANA_CFG1, &temp_ana1);
        }

        if (result == ERRNO_NO_ERROR)
        {
            /* Apply the supplied configuration, leaving the remaining bits unmodified. */
            temp_ana0 &= ~(CEM102_TIA_MASK <<
                           ((tia - CEM102_INDEX_ADJ) * (ANA_CFG0_CH2_TIA_FB_RES_POS - ANA_CFG0_CH1_TIA_FB_RES_POS)));
            tia_cfg &= (CEM102_TIA_MASK <<
                        ((tia - CEM102_INDEX_ADJ) * (ANA_CFG0_CH2_TIA_FB_RES_POS - ANA_CFG0_CH1_TIA_FB_RES_POS)));
            result = CEM102_Register_Write(p_cfg, ANA_CFG0, temp_ana0 | tia_cfg);

            temp_ana1 &= ~((1 << ANA_CFG1_CH1_TIA_CFG_POS)
                           << ((tia - CEM102_INDEX_ADJ) * (ANA_CFG1_CH2_TIA_CFG_POS - ANA_CFG1_CH1_TIA_CFG_POS)));
            enable &= ((1 << ANA_CFG1_CH1_TIA_CFG_POS)
                       << ((tia - CEM102_INDEX_ADJ) * (ANA_CFG1_CH2_TIA_CFG_POS - ANA_CFG1_CH1_TIA_CFG_POS)));
            result |= CEM102_Register_Write(p_cfg, ANA_CFG1, temp_ana1 | enable);
        }
    }
    return result;
}

int CEM102_Signal_SwitchConfig(const CEM102_Device *p_cfg, int channel, uint16_t channel_cfg)
{
    uint16_t temp = CEM102_INITIAL_VALUE;
    int result = ERRNO_NO_ERROR;

    /* Mask the provided channel configuration to ensure this only affects the
     * channel switches, and to ensure that SW114 (WE1_DAC to AT1) is
     * disconnected.
     */
    channel_cfg &= (CEM102_SW_CFG_MASK - (0x1 << ANA_SW_CFG0_CH1_SC114_CFG_POS));

    if (p_cfg == NULL)
    {
        result = ERRNO_PARAM_ERROR;
    }
    else if (channel == WE1)
    {
        /* Since the channel 1 switches share a register with the RE switches,
         * we need to read and write back the RE switch configuration (clearing
         * out the 10 channel configuration bits this function should be writing).
         */
        result = CEM102_Register_Read(p_cfg, ANA_SW_CFG0, &temp);

        if (result == ERRNO_NO_ERROR)
        {
            temp &= ~CEM102_SW_CFG_MASK;
            result = CEM102_Register_Write(p_cfg, ANA_SW_CFG0, temp | channel_cfg);
        }
    }
    else if (channel == WE2)
    {
        result = CEM102_Register_Write(p_cfg, ANA_SW_CFG1, channel_cfg);
    }
    else
    {
        result = ERRNO_PARAM_ERROR;
    }
    return result;
}

int CEM102_Signal_AIOConfigDeinit(const CEM102_Device *p_cfg, uint32_t chan,
                                  CEM102_RegSaveData *p_aiocfg)
{
    int result = ERRNO_NO_ERROR;

    if ((p_cfg == NULL) || (p_aiocfg == NULL) || ((chan != WE1) &&
        (chan != WE2)))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        result = CEM102_Register_Restore(p_cfg, p_aiocfg);

        /* We may have restored CHCFG_CHx_ACCUM register. Calculate ADC gain. */
        uint8_t val_offset[2] = { REG_CHCFG_CH1_ACCUM_OFFSET,
                                  REG_CHCFG_CH2_ACCUM_OFFSET
                                };
        uint32_t bitfield[2] = { REG_CHCFG_CH1_ACCUM_SAVE,
                                 REG_CHCFG_CH2_ACCUM_SAVE
                               };
        uint32_t index = chan - CEM102_INDEX_ADJ;

        /* If this register is not saved, probably the value of the register
         * isn't changed. Therefore, no need to calculate the ADC gain.
         */
        if ((p_aiocfg->valid) & bitfield[index] & (p_aiocfg->saved))
        {
            uint8_t offset = val_offset[index];
            uint16_t sample_cnt = (p_aiocfg->value[offset] &
                                   CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_MASK) >>
                                   CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_POS;
            if (sample_cnt > 0)
            {
                result |= CEM102_Signal_CalculateADCGain(chan,
                                                         (uint32_t)sample_cnt);
            }
        }
    }
    return result;
}

int CEM102_Signal_AIOConfigInit(const CEM102_Device *p_cfg, uint32_t chan,
                                CEM102_RegSaveData *p_aiocfg)
{
    int result = ERRNO_NO_ERROR;

    if ((p_cfg == NULL) || (p_aiocfg == NULL) || ((chan != WE1) &&
        (chan != WE2)))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        uint8_t index = chan - CEM102_INDEX_ADJ;
        uint32_t save_req[2] = { CH1_REGS_TO_SAVE, CH2_REGS_TO_SAVE };

        p_aiocfg->valid = save_req[index];
        result = CEM102_Register_Save(p_cfg, p_aiocfg);

        /* All the relevant registers saved. We could program them. */
        uint16_t buf_cfg[2] = { CH1_BUF_DISABLE | CH1_BUF_BYPASS_ENABLE,
                                CH2_BUF_DISABLE | CH2_BUF_BYPASS_ENABLE
                              };

        uint16_t chancfg = CEM102_BIAS_CURR_FOR_V(p_cfg, index);
        chancfg |= p_cfg->lpf_cutoff_v[index];
        result = CEM102_Signal_BufferConfig(p_cfg, chan,
                                            chancfg,
                                            buf_cfg[index]);

        uint16_t sw_cfg[2] = { CH1_SC109_SC110_DISCONNECT |
                               CH1_SC112_CONNECT |
                               CH1_SC115_CONNECT | CH1_SC116_CONNECT,
                               CH2_SC209_SC210_DISCONNECT |
                               CH2_SC215_CONNECT | CH2_SC216_CONNECT,
                             };
        if (CEM102_WE2_ENABLED(p_cfg))
        {
            sw_cfg[CEM102_LSAD2_IDX] |= CH2_SC212_CONNECT;
        }
        sw_cfg[CEM102_WE1_INDEX] |= CEM102_SW_SR1_POSITION(p_cfg);

        result |= CEM102_Signal_SwitchConfig(p_cfg, chan, sw_cfg[index]);

        /* If LSADx_SYS_CHOP_DISABLE is 0. No need to be in the list */
        uint16_t lsad_cfg[2] = { LSAD1_ORSD64_RSD14 | LSAD1_ITRIM_1p00,
                                 LSAD2_ORSD64_RSD14 | LSAD2_ITRIM_1p00
                               };
        uint16_t sample_cnt = p_cfg->accum_sample_volt;
        uint16_t accum_cfg[2] = { (CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_MASK &
                                   sample_cnt) |
                                   CH1_MEASUREMENT_ENABLE,
                                  (CHCFG_CH2_ACCUM_CH2_ACCUM_SAMPLE_CNT_MASK &
                                   sample_cnt) |
                                   CH2_MEASUREMENT_ENABLE
                                };
        uint16_t buff_cfg[2] = { CH1_DOUBLE, CH2_DOUBLE };

        /* Calculate ADC gain, after changing sample accumuation count */
        result |= CEM102_Signal_CalculateADCGain(chan, (uint32_t)sample_cnt);

        CEM102_Get_LSAD_Config(p_cfg, &lsad_cfg[index], NULL);
        result |= CEM102_Signal_ADCConfig(p_cfg, chan, lsad_cfg[index], 0,
                                          buff_cfg[index], accum_cfg[index]);
    }
    return result;
}

int CEM102_Signal_RE_ATBus_Config(const CEM102_Device *p_cfg, int channel, uint16_t config)
{
    uint16_t temp;
    int result = ERRNO_NO_ERROR;

    if (p_cfg == NULL)
    {
        result = ERRNO_PARAM_ERROR;
    }

    /* RE Switch Configuration */
    else if (channel == WE1)
    {
        result = CEM102_Register_Read(p_cfg, ANA_SW_CFG0, &temp);

        if (result == ERRNO_NO_ERROR)
        {
            temp &= CEM102_SW_CFG_MASK;
            result = CEM102_Register_Write(p_cfg, ANA_SW_CFG0, temp | config);
        }
    }

    /* SA Switch Configuration */
    else if (channel == WE2)
    {
        result = CEM102_Register_Write(p_cfg, ANA_SW_CFG2, config);
    }

    /* ST Switch Configuration */
    else if (channel == RE)
    {
        result = CEM102_Register_Read(p_cfg, ANA_SW_CFG3, &temp);

        if (result == ERRNO_NO_ERROR)
        {
            temp &= CEM102_SW_CFG3_BUF_MASK;
            result = CEM102_Register_Write(p_cfg, ANA_SW_CFG3, temp | config);
        }
    }
    else
    {
        result = ERRNO_PARAM_ERROR;
    }

    return result;
}

int CEM102_Signal_BufferConfig(const CEM102_Device *p_cfg, int channel, uint16_t channel_cfg,
                               uint16_t buf_cfg)
{
    uint16_t temp_ana0 = CEM102_INITIAL_VALUE;
    uint16_t temp_ana1 = CEM102_INITIAL_VALUE;
    int result = ERRNO_NO_ERROR;

    if ((p_cfg == NULL) || ((channel != WE1) && (channel != WE2)))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        result = CEM102_Register_Read(p_cfg, ANA_CFG0, &temp_ana0);
        result |= CEM102_Register_Read(p_cfg, ANA_CFG1, &temp_ana1);
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Apply the supplied configuration, leaving the remaining bits unmodified. */
        temp_ana0 &= ~((ANA_CFG0_CH1_BUF_IB_MASK | (1 << ANA_CFG0_CH1_LPF_BW_CFG_POS)) <<
                       ((channel - CEM102_INDEX_ADJ) * (ANA_CFG0_CH2_BUF_IB_POS - ANA_CFG0_CH1_BUF_IB_POS)));
        channel_cfg &=  ((ANA_CFG0_CH1_BUF_IB_MASK | (1 << ANA_CFG0_CH1_LPF_BW_CFG_POS)) <<
                         ((channel - CEM102_INDEX_ADJ) * (ANA_CFG0_CH2_BUF_IB_POS - ANA_CFG0_CH1_BUF_IB_POS)));
        result = CEM102_Register_Write(p_cfg, ANA_CFG0, temp_ana0 | channel_cfg);

        temp_ana1 &= ~(((0x1 << ANA_CFG1_CH1_BUF_CFG_POS) | (0x1 << ANA_CFG1_CH1_BUF_BYPASS_CFG_POS))
                       << ((channel - CEM102_INDEX_ADJ) * (ANA_CFG1_CH2_BUF_CFG_POS - ANA_CFG1_CH1_BUF_CFG_POS)));
        buf_cfg &= (((0x1 << ANA_CFG1_CH1_BUF_CFG_POS) | (0x1 << ANA_CFG1_CH1_BUF_BYPASS_CFG_POS))
                    << ((channel - CEM102_INDEX_ADJ) * (ANA_CFG1_CH2_BUF_CFG_POS - ANA_CFG1_CH1_BUF_CFG_POS)));
        result |= CEM102_Register_Write(p_cfg, ANA_CFG1, temp_ana1 | buf_cfg);
    }

    return result;
}

int CEM102_Signal_WEDACConfig(const CEM102_Device *p_cfg, int dac, uint16_t dac_cfg,
                            uint16_t interrupt_cfg)
{
    uint16_t temp = CEM102_INITIAL_VALUE;
    int result = ERRNO_NO_ERROR;

    /* Confirm the requested DAC value is between 0.4 V and 1.9 V if the DAC
     * will be enabled; load temp with the DAC value only (removing the enable\
     * if present) */
    if ((dac_cfg & DAC_WE1_ENABLE) != 0)
    {
        temp = dac_cfg & ~(0x1 << DC_DAC_WE1_CTRL_DAC_WE1_CFG_POS);
        if ((temp < CEM102_DAC_MIN) || (temp > CEM102_DAC_MAX))
        {
            result = ERRNO_DAC_RANGE_INVALID;
        }
    }

    /* Allow for either WE DAC to be selected */
    if ((p_cfg == NULL) || ((dac != WE1) && (dac != WE2)))
    {
        result = ERRNO_PARAM_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Write the DAC configuration */
        result = CEM102_Register_Write(p_cfg, (DC_DAC_WE1_CTRL + (dac - CEM102_INDEX_ADJ)), dac_cfg);
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Write the DAC interrupt configuration, maintaining all other
         * interrupt configurations.
         */
        result = CEM102_Register_Read(p_cfg, SYSCTRL_IRQ_CFG, &temp);
        temp &= ~SYSCTRL_IRQ_CFG_DC_DAC_LOAD_IRQ_CFG_MASK;
        interrupt_cfg &= SYSCTRL_IRQ_CFG_DC_DAC_LOAD_IRQ_CFG_MASK;
        if (result == ERRNO_NO_ERROR)
        {
            result = CEM102_System_IRQConfig(p_cfg, temp | interrupt_cfg);
        }
    }
    return result;
}

int CEM102_Signal_REDACConfig(const CEM102_Device *p_cfg, uint16_t dac_cfg,
                              uint16_t sw_cfg, uint16_t buf_cfg)
{
    uint16_t temp = CEM102_INITIAL_VALUE;
    int result = ERRNO_NO_ERROR;

    if (p_cfg == NULL)
    {
        result = ERRNO_PARAM_ERROR;
    }

    /* Confirm the requested DAC value is between 0.4 V and 1.9 V if the DAC
     * will be enabled; load temp with the DAC value only (removing the enable
     * if present) */
    if ((dac_cfg & DAC_RE_ENABLE) != 0)
    {
        temp = dac_cfg & ~(0x1 << DC_DAC_WE1_CTRL_DAC_WE1_CFG_POS);
        if ((temp < CEM102_DAC_MIN) || (temp > CEM102_DAC_MAX))
        {
            result = ERRNO_DAC_RANGE_INVALID;
        }
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Write the DAC configuration */
        result = CEM102_Register_Write(p_cfg, DC_DAC_RE_CTRL, dac_cfg);
    }

    /* Write the switch configuration without modifying the ten CH1 switches */
    if (result == ERRNO_NO_ERROR)
    {
        result = CEM102_Register_Read(p_cfg, ANA_SW_CFG0, &temp);
        if (result == ERRNO_NO_ERROR)
        {
            temp &= CEM102_SW_CFG_MASK;
            sw_cfg &= ~CEM102_SW_CFG_MASK;
            result = CEM102_Register_Write(p_cfg, ANA_SW_CFG0, temp | sw_cfg);
        }
    }

    /* Write the buffer configuration without changing any of the other buffer
     * configurations */
    if (result == ERRNO_NO_ERROR)
    {
        result = CEM102_Register_Read(p_cfg, ANA_CFG1, &temp);
        if (result == ERRNO_NO_ERROR)
        {
            temp &= ~((0x1 << ANA_CFG1_RE_BUF_HP_MODE_CFG_POS) |
                      (0x1 << ANA_CFG1_RE_BUF_CFG_POS));
            buf_cfg &= ((0x1 << ANA_CFG1_RE_BUF_HP_MODE_CFG_POS) |
                        (0x1 << ANA_CFG1_RE_BUF_CFG_POS));
            result = CEM102_Register_Write(p_cfg, ANA_CFG1, temp | buf_cfg);
        }
    }
    return result;
}

int CEM102_Signal_ADCConfig(const CEM102_Device *p_cfg, int channel, uint16_t lsad_cfg,
                            uint16_t thresh_cfg, uint16_t buf_cfg, uint16_t accum_cfg)
{
    int result = ERRNO_NO_ERROR;
    int channel_offset;

    /* Sanity check that the parameters are okay. If the chopper is enabled,
     * the accumulator specified needs to be odd since we accumulate over
     * (n + 1) samples. */
    if ((lsad_cfg & LSAD1_SYS_CHOP_ENABLE) != 0)
    {
        if ((accum_cfg & (1 << CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_POS)) !=
             (1 << CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_POS))
        {
            result = ERRNO_PARAM_ERROR;
        }
    }

    if ((p_cfg == NULL) || ((channel != WE1) && (channel != WE2)))
    {
        result = ERRNO_PARAM_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        channel_offset = (channel - CEM102_INDEX_ADJ) * (CHCFG_CH2_THRESHOLD - CHCFG_CH1_THRESHOLD);

        /* Configure the LSAD */
        result = CEM102_Register_Write(p_cfg, CHCFG_CH1_LSAD + channel_offset, lsad_cfg);

        /* Configure the threshold */
        result |= CEM102_Register_Write(p_cfg, CHCFG_CH1_THRESHOLD + channel_offset, thresh_cfg);

        /* Configure the buffer and violation block */
        result |= CEM102_Register_Write(p_cfg, CHCFG_CH1_BUFFER + channel_offset, buf_cfg);

        /* Configure the accumulation */
        result |= CEM102_Register_Write(p_cfg, CHCFG_CH1_ACCUM + channel_offset, accum_cfg);
    }

    return result;
}

int CEM102_Signal_DirectADCConfig(const CEM102_Device *p_cfg, uint32_t channel)
{
    uint16_t ana_cfg0 = CEM102_INITIAL_VALUE;
    uint16_t ana_cfg1 = CEM102_INITIAL_VALUE;
    int result = ERRNO_NO_ERROR;

    if ((p_cfg == NULL) || ((channel != WE1) && (channel != WE2)))
    {
        result = ERRNO_PARAM_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Disable buffer and enable buffer bypass to connect to the ADC
         * directly */
        result = CEM102_Register_Read(p_cfg, ANA_CFG1, &ana_cfg1);

        /* Enable buffer bypass */
        ana_cfg1 |= (CH1_BUF_BYPASS_ENABLE <<
                    ((channel - CEM102_INDEX_ADJ) * ANA_CFG1_CH2_TIA_CFG_POS));

        /* Disable the buffer */
        ana_cfg1 &= ~(CH1_BUF_ENABLE <<
                    ((channel - CEM102_INDEX_ADJ) * ANA_CFG1_CH2_TIA_CFG_POS));

        /* Write back the ANA_CFG1 register */
        result |= CEM102_Register_Write(p_cfg, ANA_CFG1, ana_cfg1);

        /* Disable internal FB network, read modify write
         * ANA_CFG0 */
        result |= CEM102_Register_Read(p_cfg, ANA_CFG0, &ana_cfg0);

        /* Enable the internal feedback network */
        ana_cfg0 |= (CH1_TIA_INT_FB_ENABLE <<
                    ((channel - CEM102_INDEX_ADJ) * ANA_CFG0_CH2_BUF_IB_POS));

        /* Enable the high frequency (10 kHz) low pass filter setting */
        ana_cfg0 |= (CEM102_CH1_LPF_CUTOFF_10KHZ <<
                    ((channel - CEM102_INDEX_ADJ) * ANA_CFG0_CH2_BUF_IB_POS));

        /* Write back ANA_CFG0 register */
        result |= CEM102_Register_Write(p_cfg, ANA_CFG0, ana_cfg0);
    }

    return result;
}

/* ----------------------------------------------------------------------------
 * Measurement Functions
 * --------------------------------------------------------------------------*/

int CEM102_Signal_StartMeasurement(const CEM102_Device *p_cfg, uint16_t cmd)
{
    uint16_t temp = CEM102_INITIAL_VALUE;
    int result = ERRNO_NO_ERROR;

    if (p_cfg == NULL)
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        result = CEM102_Register_Read(p_cfg, SYS_STATUS, &temp);
        if (result == ERRNO_NO_ERROR)
        {
            if ((temp & SYS_STATUS_SYS_STATE_MASK) != IDLE_STATE)
            {
                return ERRNO_MEASUREMENT_BUSY;
            }
            if ((temp & (DIGITAL_RESET | VDDA_ERR)) != 0)
            {
                return ERRNO_STATE_ERROR;
            }
            result = CEM102_Register_Write(p_cfg, SYSCTRL_CMD, cmd);
        }
    }
    return result;
}

static void CEM102_Signal_ProcessADCCommon(int32_t data, uint16_t lsad_cfg,
                                    uint32_t sample_cnt, uint32_t shift,
                                    float *p_data)
{
    /* If the gain is double we need an extra shift */
    if ((lsad_cfg == LSAD1_GAIN_DOUBLE) && (shift < CEM102_SHIFT_LIMIT_FOR_UINT))
    {
        shift++;
    }

    if (sample_cnt < CEM102_SAMPLE_CNT_CEILING)
    {
        sample_cnt++;
    }
    sample_cnt <<= shift;
    if (sample_cnt > 0)
    {
        float div = (float)sample_cnt;
        *p_data = (float)data / div;
    }
}

void CEM102_Signal_ProcessADCSingle(const CEM102_Device *p_cfg, uint16_t data,
                                    uint16_t lsad_cfg, int sample_cnt, int shift,
                                    float *p_processed_data)
{
    uint32_t shift_count = (uint32_t)shift;

    /* If we shift more than 31, we will get 0 */
    if ((p_processed_data != NULL) && (shift_count < CEM102_SHIFT_LIMIT_FOR_UINT))
    {
        int16_t stemp = CEM102_INITIAL_VALUE;

        /* Run through this hoop to convert unsigned short to signed int */
        memcpy(&stemp, &data, sizeof(stemp));
        shift_count++;

        CEM102_Signal_ProcessADCCommon((int32_t)stemp, lsad_cfg,
                                       (uint32_t)sample_cnt,
                                       shift_count, p_processed_data);
    }
}

void CEM102_Signal_ProcessADCDouble(const CEM102_Device *p_cfg, uint32_t data,
                                    uint16_t lsad_cfg, int sample_cnt, int shift,
                                    float *p_processed_data)
{
    uint32_t shift_count = (uint32_t) shift;

    /* If we shift more than 31, we will get 0 */
    if ((p_processed_data != NULL) && (shift_count < CEM102_SHIFT_LIMIT_FOR_USHORT))
    {
        /* Add in the default shift */
        shift_count += 17;
        CEM102_Signal_ProcessADCCommon((int32_t)data, lsad_cfg,
                                       (uint32_t)sample_cnt,
                                       shift_count, p_processed_data);
    }
}

int CEM102_Signal_MeasureReference(const CEM102_Device *p_cfg, uint32_t channel)
{
    int result = ERRNO_NO_ERROR;
    int channel_offset = ((channel - CEM102_INDEX_ADJ) * 4);
    uint16_t lsad_setting = LSAD1_SYS_CHOP_ENABLE;
    uint32_t acc_setting = CEM102_INITIAL_VALUE;
    uint16_t wedac_setting = CEM102_INITIAL_VALUE;
    double wedac_offset = CEM102_INITIAL_VALUE;
    double wedac_gain = CEM102_INITIAL_VALUE;

    if ((channel != WE1) && (channel != WE2))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Set LSAD settings based on device version */
        if (trim_struct_v1 != NULL)
        {
            lsad_setting |= LSAD1_ORSD64_RSD14;
            acc_setting = CEM102_NACC_REF_R1;

            if (channel == WE1)
            {
                wedac_setting = (trim_struct_v1->dac[0] & 0xFF00) >> CEM102_SHIFT_FOR_BYTE;
                wedac_setting += (trim_struct_v1->dac[1] & 0xF) << CEM102_SHIFT_FOR_BYTE;
            }
            else
            {
                wedac_setting = (trim_struct_v1->dac[1] & 0xFFF0) >> 4;
            }
            wedac_setting = CEM102_DAC_MV(CEM102_DAC_VOLTAGE_500MV) -
                             wedac_setting;
        }
        else if (trim_struct != NULL)
        {
            lsad_setting |= LSAD1_ORSD8192_RSD8;
            acc_setting = CEM102_NACC_REF;

            /* Extract DAC Offset and Gain Values from OTP */
            wedac_offset = (trim_struct->dac[channel - CEM102_INDEX_ADJ] & CEM102_8_BITS_MASK);
            wedac_gain = (trim_struct->dac[channel - CEM102_INDEX_ADJ] & 0xFF00) >> CEM102_SHIFT_FOR_BYTE;

            /* Calculate the DAC Offset for FW */
            wedac_offset = ((wedac_offset - CEM102_DAC_OFFSET_O2) / CEM102_DAC_OFFSET_G2) * 1000;

            /* Calculate DAC Gain for FW */
            wedac_gain = ((wedac_gain - (CEM102_DAC_GAIN_O1)) / CEM102_DAC_GAIN_G1) * 1000;

            /* Calculate DAC Code to set WE electrode at 1.2V */
            wedac_setting = (uint16_t)((CEM102_OTP_DAC_VOLTAGE - wedac_offset) / wedac_gain);
        }
        else
        {
            /* OTP has not been read from the device */
            result =  ERRNO_GENERAL_FAILURE;
        }
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Configure WEDAC voltage using value from OTP */
        result |= CEM102_Signal_WEDACConfig(p_cfg, channel,
                                            ((DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK &
                                            wedac_setting) | DAC_WE1_ENABLE),
                                            DC_DAC_LOAD_IRQ_DISABLE);

        /* Configure device to duplicate production test settings */
        result |= CEM102_Register_Write(p_cfg,
                                        ANA_CFG0,
                                        (CH1_TIA_INT_FB_ENABLE |
                                        CH1_TIA_FB_RES_4M |
                                        CH1_BUF_IB2) <<
                                        (8 * (channel - CEM102_INDEX_ADJ)));

        result |= CEM102_Register_Write(p_cfg,
                                        ANA_CFG1,
                                        (CH1_BUF_ENABLE | CH1_TIA_ENABLE) <<
                                        (3 * (channel - CEM102_INDEX_ADJ)));

        result |= CEM102_Register_Write(p_cfg,
                                        CHCFG_LSAD_CLK,
                                        ((LSAD1_OFFSET0 | LSAD1_START_DLY_ENABLE) <<
                                        (channel - CEM102_INDEX_ADJ)) |
                                        (0x19 << CHCFG_LSAD_CLK_LSAD_START_DLY_POS) |
                                        LSAD_PRESCALE1);

        result |= CEM102_Register_Write(p_cfg,
                                        (CHCFG_CH1_BUFFER + channel_offset),
                                        CH1_DOUBLE);

        /* Use many samples for high accuracy */
        result |= CEM102_Register_Write(p_cfg,
                                        CHCFG_CH1_ACCUM + channel_offset,
                                        (CHCFG_CH1_ACCUM_CH1_ACCUM_SAMPLE_CNT_MASK & (acc_setting - CEM102_INDEX_ADJ)) |
                                        (CH1_MEASUREMENT_ENABLE));

        CEM102_Get_LSAD_Config(p_cfg, &lsad_setting, NULL);
        result |= CEM102_Register_Write(p_cfg,
                                        CHCFG_CH1_LSAD + channel_offset,
                                        LSAD1_ITRIM_1p00 |
                                        LSAD1_GAIN_UNITY |
                                        lsad_setting
                                        );

        if (result == ERRNO_NO_ERROR)
        {
            result |= CEM102_Signal_CalibrateChannel(p_cfg, channel);
        }
    }
    return result;
}

int CEM102_Signal_CalibrateChannel(const CEM102_Device *p_cfg, uint32_t channel)
{
    int result = ERRNO_NO_ERROR;
    uint16_t at_sw = CEM102_INITIAL_VALUE;
    uint16_t ana_cfg2 = CEM102_INITIAL_VALUE;

    if ((channel != WE1) && (channel != WE2))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* This function assumes that the application has configured the device
         * measurement settings to align with the application setting. However,
         * we do ensure that the internal reference current is enabled. */
        result |= CEM102_Register_Read(p_cfg, ANA_CFG2, &ana_cfg2);

        result |= CEM102_Register_Write(p_cfg,
                                        ANA_CFG2,
                                        SENSOR_CAL_ENABLE | ana_cfg2);

        /* Configure switches to connect 100 nA reference current to the TIA */

        /* Connect analog test bus to the TIA. SC113 for channel 1,
         * SC213 for channel 2. Connect the TIA to the ADC via SC119/SC120
         * or SC219/SC220*/
        result |= CEM102_Register_Write(p_cfg,
                                        ANA_SW_CFG0 + (channel - CEM102_INDEX_ADJ),
                                        CH1_SC113_CONNECT |
                                        CH1_SC119_SC120_CONNECT);

        if (channel == WE1)
        {
            at_sw = ATBUS_SA7_CONNECT;
        }
        else
        {
            at_sw = ATBUS_SA14_CONNECT;
        }

        /* Connect IBN_100N (100 nA reference current) to analog test bus. */
        result |= CEM102_Register_Write(p_cfg,
                                        ANA_SW_CFG2,
                                        at_sw);

        /* We are ready to start the measurement, start a single measurement */
        if (result == ERRNO_NO_ERROR)
        {
            result |= CEM102_Signal_StartMeasurement(p_cfg, SINGLE_CMD);
        }

    }
    return result;
}

int CEM102_Signal_CalibrateDACVoltage(const CEM102_Device *p_cfg,
                                      int16_t *p_offset_value,
                                      uint32_t channel,
                                      int32_t voltage_target_uv,
                                      int32_t measured_voltage_uv,
                                      CEM102_DAC_CAL_TYPE *p_cal_status)
{
    CEM102_DAC_CAL_TYPE ret_val = *p_cal_status;
    static int16_t prev_offset = CEM102_INITIAL_VALUE;
    static int16_t new_offset = CEM102_INITIAL_VALUE;
    static int32_t prev_voltage_diff = CEM102_INITIAL_VALUE;
    static int32_t voltage_diff = CEM102_INITIAL_VALUE;
    int result = ERRNO_NO_ERROR;

    if ((channel != WE1) && (channel != WE2))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
        ret_val = DAC_CAL_ERROR;
    }
    else
    {
        uint32_t ch_index = channel - CEM102_INDEX_ADJ;

        /* Calculate voltage differences */
        prev_voltage_diff = voltage_diff;
        voltage_diff = voltage_target_uv - measured_voltage_uv;

        if (voltage_diff == 0)
        {
            /* Exact voltage found, no need to continue measuring */
            ret_val = DAC_CAL_COMPLETE;
        }
        else if ((voltage_diff > 0 && prev_voltage_diff < 0) ||
                (voltage_diff < 0 && prev_voltage_diff > 0))
        {
            /* We found the zero crossing point in the error. Need to select
             * the offset to return. */
            ret_val = DAC_CAL_COMPLETE;
        }
        else
        {
            /* A calibration is in progress */
            ret_val = DAC_CAL_IN_PROGRESS;

            /* Retrieve the current offset value, then increment it */
            result |= CEM102_Register_Read(p_cfg, DC_DAC_WE1_CTRL + ch_index,
                                           (uint16_t *)&prev_offset);

            if (result == ERRNO_NO_ERROR)
            {
                /* Set the new offset based on the difference value */
                if (voltage_diff < 0)
                {
                    new_offset = prev_offset - CEM102_INDEX_ADJ;
                }
                else
                {
                    new_offset = prev_offset + 1;
                }

                result |= CEM102_Register_Write(p_cfg, DC_DAC_WE1_CTRL +
                                                ch_index, new_offset);

                if (result != ERRNO_NO_ERROR)
                {
                    ret_val = DAC_CAL_ERROR;
                }
                else
                {
                    /* We are ready to start the measurement, start a single
                     * measurement */
                    if (result == ERRNO_NO_ERROR)
                    {
                        result |= CEM102_Signal_StartMeasurement(p_cfg, SINGLE_CMD);
                    }
                }
            }
            else
            {
                ret_val = DAC_CAL_ERROR;
            }
        }

        if (ret_val == DAC_CAL_COMPLETE)
        {
            /* Return the most accurate offset value  */
            int32_t vdp = voltage_diff > 0 ? voltage_diff :
                          -1 * voltage_diff;
            int32_t pvd = prev_voltage_diff > 0 ?
                                 prev_voltage_diff : -1 * prev_voltage_diff;
            if (voltage_diff == 0 || vdp <= pvd)
            {
                *p_offset_value = new_offset & DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK;
            }
            else
            {
                *p_offset_value = prev_offset & DC_DAC_WE1_CTRL_DAC_WE1_VALUE_MASK;
            }

            /* Write the most accurate offset value to the DAC register. */
            result |= CEM102_Register_Write(p_cfg,
                                            DC_DAC_WE1_CTRL + ch_index,
                                            (*p_offset_value) | DAC_WE1_ENABLE);

            voltage_target_uv /= CEM102_UV_TO_MV;
            int16_t sub = CEM102_DAC_MV(voltage_target_uv);
            *p_offset_value -= sub;
        }
    }

    *p_cal_status = ret_val;

    return result;
}

int CEM102_Signal_CalculateChannelGain(const CEM102_Device *p_cfg,
                                       uint32_t channel, uint32_t ref_val,
                                       uint32_t cal_val)
{
    float otp_gain;
    int result = ERRNO_NO_ERROR;
    float nacc_cal;
    float nacc_otp;
    uint16_t otp_rev;

    if ((channel != WE1) && (channel != WE2))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        /* Set LSAD settings based on device version */
        if (trim_struct_v1 != NULL)
        {
            if (channel == WE1)
            {
                /* Bits 4-15 from OTP word 7 form the channel gain */
                otp_gain = (float)((trim_struct_v1->adc[0] & 0xFFF0) >> 4);
            }
            else
            {
                /* OTP word 9 bits 8-15 contains bits 0-7 of the otp_gain.
                 * OTP word 10 bits 0-3 contains bits 8-11 of the otp_gain. */
                otp_gain = (float)(((trim_struct_v1->adc[2] & 0xFF00) >>
                            CEM102_SHIFT_FOR_BYTE) |
                            ((trim_struct_v1->adc[3] & 0xF) <<
                            CEM102_SHIFT_FOR_BYTE));
            }

            if (otp_gain > CEM102_OTP_GAIN_THRESHOLD)
            {
                otp_gain /= 2.0f;
            }

            /* Set number of accumulations used */
            nacc_cal = CEM102_NACC_REF_R1;
            nacc_otp = CEM102_NACC_OTP_R1;
        }
        else if (trim_struct != NULL)
        {
            otp_gain = trim_struct->adc[0];
            otp_rev = CEM102_OTP_GetVersion();

            if (otp_rev >= CEM102_OTP_VERSION_3)
            {
                otp_gain = (float)(trim_struct->adc[0] + CEM102_GAIN_OFFSET);
                otp_gain = otp_gain / 16.0f;
            }

            /* Set number of accumulations used */
            nacc_cal = CEM102_NACC_REF;
            nacc_otp = CEM102_NACC_OTP;
        }
        else
        {
            /* OTP has not been read from the device */
            result = ERRNO_GENERAL_FAILURE;
        }
        if (result == ERRNO_NO_ERROR)
        {
            uint32_t idx = channel - CEM102_INDEX_ADJ;
            /* Now we calculate the channel gain given the calibration
             * measurements, otp_gain and configuration settings. */
            channel_gain[idx] = ref_val * nacc_otp * otp_gain;
            channel_gain[idx] = channel_gain[idx] / (nacc_cal * cal_val);
        }
    }
    return result;
}

int CEM102_Signal_CalculateADCGain(uint32_t channel, uint32_t num_accum)
{
    int result = ERRNO_NO_ERROR;
    float accum = (float)num_accum;

    if ((channel != WE1) && (channel != WE2))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        if (trim_struct_v1 != NULL)
        {
            /* ADC gain is not implemented in OTP rev 1.
             * Use default values */
            adc_gain[channel - CEM102_INDEX_ADJ] /= (accum / 2);
        }
        else if (trim_struct != NULL)
        {
            float adc_trim = (float)trim_struct->adc[channel];

            /* Use the value obtained from OTP to calculate
             * the gain value. */
            adc_gain[channel - CEM102_INDEX_ADJ] = CEM102_ADC_GAIN_CALC(adc_trim) /
                                                         (accum / 2);
        }
        else
        {
            /* OTP has not been read from the device */
            result = ERRNO_GENERAL_FAILURE;
        }
    }
    return result;
}

int CEM102_Signal_GetCalibratedCurrent(uint32_t *p_ibp, uint32_t *p_ibn)
{
    int8_t ibp_value = CEM102_INITIAL_VALUE;
    int8_t ibn_value = CEM102_INITIAL_VALUE;
    int result = ERRNO_NO_ERROR;

    if (trim_struct_v1 != NULL)
    {
        ibp_value = ((trim_struct_v1->current_bp_delta[0] & 0xF000) >> 12) |
                 ((trim_struct_v1->current_bp_delta[1] & 0x0003) << 4);
        ibn_value = ((trim_struct_v1->current_bp_delta[1] & 0x00FC) >> 2);
    }
    else if (trim_struct != NULL)
    {
        ibp_value = trim_struct->current_bp_delta & CEM102_8_BITS_MASK;
        ibn_value = (trim_struct->current_bp_delta & 0xFF00) >> CEM102_SHIFT_FOR_BYTE;
    }
    else
    {
        *p_ibp = CEM102_INITIAL_VALUE;
        *p_ibn = CEM102_INITIAL_VALUE;

        /* OTP has not been read from the device */
        result = ERRNO_GENERAL_FAILURE;
    }

    if (result == ERRNO_NO_ERROR)
    {
        int32_t cal_curr_delta_femto = CEM102_INITIAL_VALUE;
        uint16_t otp_rev = CEM102_OTP_GetVersion();

        /* Step size changed since OTP version 5 */
        if (otp_rev >= CEM102_OTP_VERSION_5)
        {
            cal_curr_delta_femto = CEM102_CAL_CURR_DELTA_AFTER_V4;
        }
        else
        {
            cal_curr_delta_femto = CEM102_CAL_CURR_DELTA_BEFORE_V5;
        }

        *p_ibp = (uint32_t)(((int32_t)ibp_value * cal_curr_delta_femto) +
                  NANO_AMPS_TO_FEMTO_AMPS(CEM102_CALIB_CURRENT_NANO_AMPS));
        *p_ibn = (uint32_t)((int32_t)ibn_value * cal_curr_delta_femto) +
                  NANO_AMPS_TO_FEMTO_AMPS(CEM102_CALIB_CURRENT_NANO_AMPS);
    }
    return result;
}

int CEM102_Signal_CalculateCurrent(uint32_t channel, int32_t lsad_value,
                                   float *p_current)
{
    int error = ERRNO_NO_ERROR;

    if ((p_current == NULL) || ((channel != WE1) && (channel != WE2)))
    {
        /* Invalid channel selected */
        error = ERRNO_PARAM_ERROR;
    }
    else
    {
        *p_current = ((float)(lsad_value) * channel_gain[channel - CEM102_INDEX_ADJ]);
    }

    return error;
}

int CEM102_Signal_CalculateVoltage(uint32_t channel, int32_t lsad_value,
                                   int32_t *p_voltage)
{
    int error = ERRNO_NO_ERROR;
    float temp = 0.0f;

    if ((p_voltage == NULL) || ((channel != WE1) && (channel != WE2) &&
        (channel != RE)))
    {
        /* Invalid channel selected */
        error = ERRNO_PARAM_ERROR;
    }
    else
    {
        temp = ((float)(lsad_value) * adc_gain[channel - CEM102_INDEX_ADJ]);

        /* Convert volt to microvolts */
        temp *= CEM102_MICRO_UNITS_CONV_FACTOR;

        *p_voltage = (int32_t)temp;
    }

    return error;
}

int CEM102_Signal_CalculateDACOffset(CEM102_DAC_TYPE offset_type,
                                     int16_t *p_offset_value)
{
    int16_t value = CEM102_INITIAL_VALUE;
    int result = ERRNO_NO_ERROR;

    if (NULL == p_offset_value)
    {
        result = ERRNO_PARAM_ERROR;
    }
    else if (trim_struct_v1 == NULL && trim_struct == NULL)
    {
        *p_offset_value = CEM102_INITIAL_VALUE;
        result = ERRNO_GENERAL_FAILURE;
    }
    else
    {
        switch (offset_type)
        {
            case RE:
                if (trim_struct_v1 != NULL)
                {
                    value = trim_struct_v1->dac[2] & 0x0FFF;
                }
                else // trim_struct pointer must be valid.
                {
                    value = (trim_struct->dac[2] & CEM102_8_BITS_MASK);
                }
                break;
            case WE2:
                if (trim_struct_v1 != NULL)
                {
                    value = (trim_struct_v1->dac[1] & 0xFFF0) >> 4;
                }
                else // trim_struct pointer must be valid.
                {
                    value = (trim_struct->dac[1] & CEM102_8_BITS_MASK);
                }
                break;
            default: // Default is WE1.
                if (trim_struct_v1 != NULL)
                {
                    value = (trim_struct_v1->dac[0] & 0xFF00) >>
                             CEM102_SHIFT_FOR_BYTE;
                    value |= (trim_struct_v1->dac[1] & 0xF) <<
                              CEM102_SHIFT_FOR_BYTE;
                }
                else // trim_struct pointer must be valid.
                {
                    value = (trim_struct->dac[0] & CEM102_8_BITS_MASK);
                }
                break;
        }

        if (trim_struct_v1 != NULL)
        {
            *p_offset_value = (CEM102_DAC_MV(CEM102_OTP_DAC_VOLTAGE) - value);
        }
        else
        {
            *p_offset_value = value;
        }
    }
    return result;
}

int CEM102_Signal_GetADCGain(uint32_t channel, float *p_gain)
{
    int result = ERRNO_NO_ERROR;

    if ((p_gain == NULL) || ((channel != WE1) && (channel != WE2)
        && (channel != RE)))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        *p_gain = adc_gain[channel - CEM102_INDEX_ADJ];
    }

    return result;
}

int CEM102_Signal_CalculateDACGain(uint32_t channel, double *p_dac_gain)
{
    int result = ERRNO_NO_ERROR;
    double dac_gain;

    if ((channel != WE1) && (channel != WE2) && (channel != RE))
    {
        /* Invalid channel selected */
        result = ERRNO_PARAM_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        if (trim_struct_v1 != NULL)
        {
            /* OTP v1 has no gain information */
            result = ERRNO_GENERAL_FAILURE;
        }

        if ((trim_struct != NULL) && (result == ERRNO_NO_ERROR))
        {
            dac_gain = (trim_struct->dac[channel - CEM102_INDEX_ADJ] &
                        0xFF00) >> CEM102_SHIFT_FOR_BYTE;
            *p_dac_gain = ((dac_gain - (CEM102_DAC_GAIN_O1)) /
                           CEM102_DAC_GAIN_G1) * 1000;
        }
    }

    return result;
}

static int CEM102_Signal_GetADCSamples(const CEM102_Device *p_cfg,
                                       CEM102_LSAD_READ_TYPE req,
                                       uint16_t *p_data)
{
    uint16_t ch_clr[4] = { CH1_COMPLETION_CLR_CMD,
                           CH2_COMPLETION_CLR_CMD,
                           CH1_COMPLETION_CLR_CMD,
                           CH1_COMPLETION_CLR_CMD | CH2_COMPLETION_CLR_CMD,
                         };
    uint16_t completion_flag[4] = { CH1_COMPLETION,
                                    CH2_COMPLETION,
                                    CH1_COMPLETION,
                                    CH1_COMPLETION | CH2_COMPLETION,
                                  };
    int result = ERRNO_NO_ERROR;

    if ((req > CEM102_LSAD_READ_WE1_WE2) ||
        (req < CEM102_LSAD_READ_WE1))
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        uint16_t cmd = DIGITAL_RESET_CLR_CMD | VDDA_ERR_CLR_CMD |
                       DC_DAC_LOAD_CLR_CMD | BUFFER_RESET_CMD | IDLE_CMD;
        cmd |= ch_clr[req];

        /* Ensure all errors and system status is cleared before starting the
         * first measurement. */
        result = CEM102_System_Command(p_cfg, cmd);

        /* Give time before next SPI transaction, after IDLE_CMD */
        Sys_Delay(CEM102_SYS_DELAY_20US);

        if (result == ERRNO_NO_ERROR)
        {
            result = CEM102_Signal_StartMeasurement(p_cfg, SINGLE_CMD);

            if (result == ERRNO_NO_ERROR)
            {
                /* Wait for the reference measurement to finish, the system can
                 * perform other activities during this time.
                 */
                uint16_t status = CEM102_INITIAL_VALUE;
                Sys_Delay(CEM102_SYS_DELAY_50MS);
                do
                {
                    status = CEM102_INITIAL_VALUE;
                    CEM102_Register_Read(p_cfg, SYS_STATUS, &status);
                    if (((status & (completion_flag[req])) == 0) &&
                        CEM102_INTERRUPT_ENABLED(p_cfg))
                    {
                        __WFI();
                    }
                    SYS_WATCHDOG_REFRESH();
                } while ((status & completion_flag[req]) == 0);
            }

            /* After the measurement is finished, queue a read of the measurement
             * data */
            result |= CEM102_Register_Multiple_Read(p_cfg, SYS_BUFFER_WORD0,
                                                    p_data,
                                                    CEM102_MEASUREMENT_LENGTH);
            uint16_t we1_val = CEM102_INITIAL_VALUE;
            CEM102_Register_Read(p_cfg, CHCFG_CH1_ACCUM, &we1_val);

            uint16_t we2_val = CEM102_INITIAL_VALUE;
            CEM102_Register_Read(p_cfg, CHCFG_CH2_ACCUM, &we2_val);

            /* When one of the channels is disabled, the results are read into
             * offset of CEM102_WE1_LOWER/UPPER_HALF_IDX. If the code
             * looks for WE2 data at offset CEM102_WE2_LOWER/UPPER_HALF_IDX,
             * copying the data to the expected place would help.
             */
            if (((we1_val & CHCFG_CH1_ACCUM_CH1_MEASUREMENT_CFG_MASK) == 0) ||
                ((we2_val & CHCFG_CH1_ACCUM_CH1_MEASUREMENT_CFG_MASK) == 0))
            {
                p_data[CEM102_WE2_UPPER_HALF_IDX] = p_data[CEM102_WE1_UPPER_HALF_IDX];
                p_data[CEM102_WE2_LOWER_HALF_IDX] = p_data[CEM102_WE1_LOWER_HALF_IDX];
            }
        }
    }
    return result;
}

static int CEM102_Signal_GetLSADValues(const CEM102_Device *p_cfg, uint32_t chan,
                                       CEM102_LSAD_READ_TYPE req,
                                       int32_t *p_lsad1, int32_t *p_lsad2)
{
    uint16_t cem102_measure_buf[CEM102_MEASUREMENT_LENGTH];

    int result = CEM102_Signal_GetADCSamples(p_cfg, req, cem102_measure_buf);
    if (result == ERRNO_NO_ERROR)
    {
        uint32_t index = chan - CEM102_INDEX_ADJ;
        uint32_t lower_half = (index * CEM102_SPI_DBYTE_MULT_FACTOR) +
                               CEM102_WE1_LOWER_HALF_IDX;
        uint32_t upper_half = (index * CEM102_SPI_DBYTE_MULT_FACTOR) +
                               CEM102_WE1_UPPER_HALF_IDX;
        if (chan == RE)
        {
            lower_half = CEM102_WE1_LOWER_HALF_IDX;
            upper_half = CEM102_WE1_UPPER_HALF_IDX;
        }

        *p_lsad1 = (int32_t)CEM102_Signal_PackMeasData(cem102_measure_buf,
                                                       upper_half, lower_half);
        if ((CEM102_LSAD_READ_WE1_WE2 == req) && (p_lsad2 != NULL))
        {
            *p_lsad2 = (int32_t)CEM102_Signal_PackMeasData(cem102_measure_buf,
                                                           CEM102_WE2_UPPER_HALF_IDX,
                                                           CEM102_WE2_LOWER_HALF_IDX);
        }
    }
    return result;
}

#if !CEM102_DIAG_FUNCTIONS
static int CEM102_Signal_GetCurrentValues(const CEM102_Device *p_cfg,
                                          uint32_t dac,
                                          float *p_cur1, float *p_cur2)
{
    int result = ERRNO_NO_ERROR;

    if ((p_cfg == NULL) || (p_cur1 == NULL) || ((dac != WE1) &&
        (dac != WE2) && (dac != RE)))
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        CEM102_LSAD_READ_TYPE req = dac - CEM102_INDEX_ADJ;

        int32_t lsad1 = CEM102_INITIAL_VALUE;
        int32_t lsad2 = CEM102_INITIAL_VALUE;
        int32_t *p_lsad2 = NULL;

        /* If p_cur2 is not NULL, that means request is to read LSAD for
         * both WE1 and WE2. Incoming channel number is ignored
         * and enforced to be WE1. */
        if (NULL != p_cur2)
        {
            dac = WE1;
            req = CEM102_LSAD_READ_WE1_WE2;
            p_lsad2 = &lsad2;
        }

        result = CEM102_Signal_GetLSADValues(p_cfg, dac, req, &lsad1, p_lsad2);
        if ((p_cur2 != NULL) && (result == ERRNO_NO_ERROR))
        {
            result = CEM102_Signal_CalculateCurrent(WE2, lsad2, p_cur2);
        }
        if (result == ERRNO_NO_ERROR)
        {
            result = CEM102_Signal_CalculateCurrent(dac, lsad1, p_cur1);
        }
    }
    return result;
}

static int CEM102_Signal_MeasureOffsetCurrent(const CEM102_Device *p_cfg,
                                              CEM102_CURR_MEASURE_TYPE req,
                                              CEM102_CurrentData *p_data)
{
    int result = ERRNO_NO_ERROR;
    CEM102_RegSaveData saved_data;
    float *p_cur1 = &p_data->current_we;

    /* When request is to measure both WE1 and WE2, incoming channel number
     * is enforced to be WE1. */
    float *p_cur2 = (req == CEM102_OFFSET_CURRENT_WE1_WE2) ?
                     &p_data->current_we2 : NULL;
    uint32_t chan = (req == CEM102_OFFSET_CURRENT_WE2) ? WE2 : WE1;

    /* Store user configuration */
    bool saved = false;
    memset(&saved_data, 0, sizeof(saved_data));
    saved_data.valid = REG_ANA_SW_CFG0_SAVE | REG_ANA_SW_CFG1_SAVE;
    result = CEM102_Register_Save(p_cfg, &saved_data);

    if (result == ERRNO_NO_ERROR)
    {
        saved = true;

        /* Configure the ADC to prepare for the ChannelOffsetCurrent */
        uint16_t sw_conn[2] = { CH1_SC119_SC120_CONNECT,
                                CH2_SC219_SC220_CONNECT
                              };
        if (req == CEM102_OFFSET_CURRENT_WE1_WE2)
        {
            result |= CEM102_Signal_SwitchConfig(p_cfg, WE1, sw_conn[0]);
            result |= CEM102_Signal_SwitchConfig(p_cfg, WE2, sw_conn[1]);
        }
        else
        {
            result = CEM102_Signal_SwitchConfig(p_cfg, chan,
                                                sw_conn[chan - CEM102_INDEX_ADJ]);
        }
    }

    if (result == ERRNO_NO_ERROR)
    {
        result = CEM102_Signal_GetCurrentValues(p_cfg, chan, p_cur1, p_cur2);
    }

    if (saved)
    {
        /* Restore user configuration */
        result = CEM102_Register_Restore(p_cfg, &saved_data);
    }
    return result;
}

static int CEM102_Signal_MeasureResidualCurrent(const CEM102_Device *p_cfg,
                                                CEM102_CURR_MEASURE_TYPE req,
                                                CEM102_CurrentData *p_data)
{
    uint32_t chan = (CEM102_CH_RES_CURRENT_WE2 == req) ? WE2 :  WE1;
    int result = ERRNO_NO_ERROR;
    CEM102_RegSaveData saved_data;
    bool saved = false;

    /* Store user configuration */
    memset(&saved_data, 0, sizeof(saved_data));
    saved_data.valid = ( REG_ANA_SW_CFG0_SAVE |
                         REG_ANA_SW_CFG1_SAVE |
                         REG_ANA_CFG1_SAVE |
                         REG_DC_DAC_RE_CTRL_SAVE
                       );
    result = CEM102_Register_Save(p_cfg, &saved_data);

    if (result == ERRNO_NO_ERROR)
    {
        saved = true;

        /* Configure the ADC to prepare for the ChannelResidual */
        uint16_t sw_conn[2] = { CH1_SC119_SC120_CONNECT,
                                CH2_SC219_SC220_CONNECT
                              };
        if (req == CEM102_CH_RES_CURRENT_WE1)
        {
            result = CEM102_Signal_SwitchConfig(p_cfg, WE2, sw_conn[1]);

        }
        else /* CEM102_CH_RES_CURRENT_WE2 */
        {
            /* Configure the ADC to prepare for the ChannelResidual */
            result = CEM102_Signal_SwitchConfig(p_cfg, WE1, sw_conn[0]);
        }

        /* Disable RE channel, SR switches, and HP mode, RE_BUFFER */
        uint16_t dac_re_value = CEM102_GetDACValue(p_cfg, RE);
        result |= CEM102_Signal_REDACConfig(p_cfg, dac_re_value, 0, 0);
    }

    if (result == ERRNO_NO_ERROR)
    {
        result = CEM102_Signal_GetCurrentValues(p_cfg, chan, &p_data->current_we,
                                                NULL);
    }

    if (saved)
    {
        /* Restore user configuration */
        result = CEM102_Register_Restore(p_cfg, &saved_data);
    }
    return result;
}

static int CEM102_Signal_MeasureREResidual(const CEM102_Device *p_cfg,
                                           CEM102_CURR_MEASURE_TYPE req,
                                           CEM102_CurrentData *p_data)
{
    int result = ERRNO_NO_ERROR;
    CEM102_RegSaveData saved_data;
    bool saved = false;

    /* Store user configuration */
    memset(&saved_data, 0, sizeof(saved_data));
    saved_data.valid = ( REG_ANA_SW_CFG0_SAVE |
                         REG_ANA_SW_CFG1_SAVE |
                         REG_ANA_SW_CFG3_SAVE |
                         REG_ANA_CFG0_SAVE |
                         REG_ANA_CFG1_SAVE |
                         REG_DC_DAC_RE_CTRL_SAVE |
                         REG_DC_DAC_WE2_CTRL_SAVE |
                         REG_SYSCTRL_IRQ_CFG_SAVE
                       );
    result = CEM102_Register_Save(p_cfg, &saved_data);

    if (result == ERRNO_NO_ERROR)
    {
        saved = true;
        uint16_t dac_setting = CEM102_GetDACValue(p_cfg, RE);

        /* Configure the ADC to prepare for the RE_CE residual and RE residual */
        if (req == CEM102_RE_CE_RES_CURRENT)
        {
            /* RE channel disable and SR1=ON & SR2=ON*/
            result |= CEM102_Signal_REDACConfig(p_cfg, dac_setting,
                                                (CEM102_SW_SR1_POSITION(p_cfg) |
                                                RE_SR2_CONNECT), 0);

        }
        else if (req == CEM102_RE_RES_CURRENT)
        {
            /* RE channel disable and SR1=OFF & SR2=ON*/
            result |= CEM102_Signal_REDACConfig(p_cfg, dac_setting, RE_SR2_CONNECT, 0);
        }

        /* WE2 DAC shall be set to the RE voltage for RE_CE residual and RE
         * residual */
        int ret = CEM102_DAC_Calibrate(p_cfg, WE2, p_data->target_mv,
                                       &dac_setting);

        /* OTP v3 parts will cause CalculateDACAmplitude to return error. */
        if (ret == ERRNO_GENERAL_FAILURE)
        {
            dac_setting = (DC_DAC_RE_CTRL_DAC_RE_VALUE_MASK &
                           saved_data.value[REG_DC_DAC_RE_CTRL_OFFSET]);
        }

        result |= CEM102_Signal_WEDACConfig(p_cfg, WE2,
                                            ((DC_DAC_WE2_CTRL_DAC_WE2_VALUE_MASK &
                                            dac_setting) | DAC_WE2_ENABLE), 0);

        /* RE voltage will be set via AT1 to WE2 voltage(=RE voltage while
         * in normal operation) */
        uint16_t sw_conn[2] = { CH1_SC119_SC120_CONNECT,
                                CH2_SC213_CONNECT | CH2_SC219_SC220_CONNECT
                              };
        result |= CEM102_Signal_SwitchConfig(p_cfg, WE1, sw_conn[0]);
        result |= CEM102_Signal_SwitchConfig(p_cfg, WE2, sw_conn[1]);
    }

    if (result == ERRNO_NO_ERROR)
    {
        result = CEM102_Signal_GetCurrentValues(p_cfg, WE2, &p_data->current_we,
                                                NULL);
    }
    if (saved)
    {
        /* Restore user configuration */
        result = CEM102_Register_Restore(p_cfg, &saved_data);
    }
    return result;
}

static int CEM102_Signal_MeasureATResidual(const CEM102_Device *p_cfg,
                                           CEM102_CURR_MEASURE_TYPE req,
                                           CEM102_CurrentData *p_data)
{
    int result = ERRNO_NO_ERROR;
    CEM102_RegSaveData saved_data;

    /* Restore user configuration */
    memset(&saved_data, 0, sizeof(saved_data));
    saved_data.valid = ( REG_ANA_SW_CFG0_SAVE |
                         REG_ANA_SW_CFG1_SAVE |
                         REG_ANA_SW_CFG2_SAVE |
                         REG_ANA_SW_CFG3_SAVE |
                         REG_ANA_CFG0_SAVE |
                         REG_ANA_CFG1_SAVE |
                         REG_DC_DAC_RE_CTRL_SAVE
                       );
    result = CEM102_Register_Save(p_cfg, &saved_data);

    bool saved = false;
    if (result == ERRNO_NO_ERROR)
    {
        saved = true;

        /* Disconnect all SAxx switches */
        result = CEM102_Register_Write(p_cfg, ANA_SW_CFG2, 0);

        /* Disable RE channel, SR switches, and HP mode, RE_BUFFER */
        uint16_t dac_re_value = CEM102_GetDACValue(p_cfg, RE);
        result |= CEM102_Signal_REDACConfig(p_cfg, dac_re_value, 0, 0);

        /* Disable all ST switches */
        result |= CEM102_Signal_RE_ATBus_Config(p_cfg, RE, 0);
    }

    float *p_cur = &p_data->current_we;
    float *p_cur2 = (CEM102_AT_RES_CURRENT_WE1_WE2 == req) ?
                     &p_data->current_we2 : NULL;
    if (result == ERRNO_NO_ERROR)
    {
        uint16_t sw_conn[2] = { CH1_SC113_CONNECT | CH1_SC119_SC120_CONNECT,
                                CH2_SC213_CONNECT | CH2_SC219_SC220_CONNECT
                              };
        int chan = (CEM102_AT_RES_CURRENT_WE2 == req) ? WE2 : WE1;

        if (req == CEM102_AT_RES_CURRENT_WE1_WE2)
        {
            result |= CEM102_Signal_SwitchConfig(p_cfg, WE1, sw_conn[0]);
            result |= CEM102_Signal_SwitchConfig(p_cfg, WE2, sw_conn[1]);
        }
        else
        {
            result = CEM102_Signal_SwitchConfig(p_cfg, chan,
                                                sw_conn[chan - CEM102_INDEX_ADJ]);
        }
    }

    if (result == ERRNO_NO_ERROR)
    {
        result = CEM102_Signal_GetCurrentValues(p_cfg, WE1, p_cur, p_cur2);
    }

    if (saved)
    {
        /* Restore user configuration */
        result = CEM102_Register_Restore(p_cfg, &saved_data);
    }
    return result;
}
#endif /* CEM102_DIAG_FUNCTIONS */

int CEM102_Signal_MeasureCurrent(const CEM102_Device *p_cfg,
                                 CEM102_CURR_MEASURE_TYPE req,
                                 CEM102_CurrentData *p_data)
{
#if CEM102_DIAG_FUNCTIONS
    int result = ERRNO_NOT_IMPLEMENTED;
#else
    int result = ERRNO_NO_ERROR;

    switch (req)
    {
    case CEM102_CH_RES_CURRENT_WE1:
    case CEM102_CH_RES_CURRENT_WE2:
        result = CEM102_Signal_MeasureResidualCurrent(p_cfg, req, p_data);
        break;
    case CEM102_RE_CE_RES_CURRENT:
    case CEM102_RE_RES_CURRENT:
        result = CEM102_Signal_MeasureREResidual(p_cfg, req, p_data);
        break;
    case CEM102_AT_RES_CURRENT_WE1:
    case CEM102_AT_RES_CURRENT_WE2:
    case CEM102_AT_RES_CURRENT_WE1_WE2:
        result = CEM102_Signal_MeasureATResidual(p_cfg, req, p_data);
        break;
    case CEM102_OFFSET_CURRENT_WE1:
    case CEM102_OFFSET_CURRENT_WE2:
    case CEM102_OFFSET_CURRENT_WE1_WE2:
        result = CEM102_Signal_MeasureOffsetCurrent(p_cfg, req, p_data);
        break;
    default:
        result = ERRNO_PARAM_ERROR;
        break;

    }
#endif /* CEM102_DIAG_FUNCTIONS */
    return result;
}

int CEM102_Signal_MeasureVoltage(const CEM102_Device *p_cfg, int chan,
                                 int32_t *p_voltage1, int32_t *p_voltage2)

{
    int result = ERRNO_NO_ERROR;

    if ((p_cfg == NULL) || ((chan != WE1) && (chan != WE2) && (chan != RE)) ||
        (p_voltage1 == NULL))
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        uint32_t dac = (uint32_t)chan;
        CEM102_LSAD_READ_TYPE req = dac - CEM102_INDEX_ADJ;
        int32_t lsad2 = CEM102_INITIAL_VALUE;
        int32_t *p_lsad2 = NULL;

        /* If p_voltage2 is not NULL, that means request is to calculate the
         * voltages of both WE1 and WE2. Incoming channel number is ignored
         * and enforced to be WE1. */
        if (NULL != p_voltage2)
        {
            dac = WE1;
            req = CEM102_LSAD_READ_WE1_WE2;
            p_lsad2 = &lsad2;
        }

        /* Parameter chan can be one of WE1, WE2, RE. Some APIs expect
         * WE1 for RE. */
        int32_t lsad1 = CEM102_INITIAL_VALUE;
        result = CEM102_Signal_GetLSADValues(p_cfg, dac, req, &lsad1, p_lsad2);

        if (result == ERRNO_NO_ERROR)
        {
            /* AT1 & AT2 are configured to be inverse to ADC output */
            if (dac == WE1)
            {
                lsad1 *= (-1);
            }

            /* Time to calculate the actual voltage measured on the working
             * electrode of the given channel, given the raw uncompensated
             * measurement.
             */
            if (NULL != p_voltage2)
            {
                result = CEM102_Signal_CalculateVoltage(WE2, lsad2, p_voltage2);
            }

            /* Most API, chan is one of WE1, WE2, RE. Some API expects WE1
             * for RE */
            uint32_t new_chan = (dac == RE) ? WE1 : dac;

            /* Calculate the actual voltage measured on the working electrode of
             * the given channel, given the raw uncompensated measurement.
             */
            if (result == ERRNO_NO_ERROR)
            {
                result = CEM102_Signal_CalculateVoltage(new_chan, lsad1,
                                                        p_voltage1);
            }
        }
    }
    return result;
}

