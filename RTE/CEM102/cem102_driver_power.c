/**
 * @file cem102_driver_power.c
 * @brief CEM102 Driver Power implementation
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

#include <string.h>
#include <cem102_driver.h>

/* RSL15 SDK 1.11 has this definition removed. It only contains
 * VCC_TRIM_DEFAULT_UNCALIBRATED. The values aren't the same.
 */
#ifndef VCC_TRIM_1P31V 
#define VCC_TRIM_1P31V  (0x1FU << ACS_VCC_CTRL_VTRIM_Pos)
#endif

int CEM102_Power_Reset(const CEM102_Device *p_cfg)
{
    int result = 0;
    uint16_t read_data = 0;
    uint16_t timeout;
    uint16_t vddc_status = 0;
    uint8_t vcc_trim = 0;

    if (p_cfg->pwr_en < 0)
    {
        /* Flag a general failures, since the pwr_en pad isn't connected. */
        result =  ERRNO_GENERAL_FAILURE;
    }
    else
    {

        /* Set the SPI status to idle */
        cem102_spi_status = SPI_TRANSFER_IDLE;

        /* Pull the power enable pin low, resetting CEM102 */
        Sys_GPIO_Set_Low(p_cfg->pwr_en);

        /* Load DCDC trim for 1.25 V. */
        if (Sys_Trim_LoadDCDC(TRIM, TARGET_DCDC_1250) == ERROR_NO_ERROR)
        {
            /* Save vcc_trim value for 1.25 V to restore it later */
            vcc_trim = (ACS->VCC_CTRL & ACS_VCC_CTRL_VTRIM_Mask);

            /* Adjust calibrated 1.25 V setting up to 1.3 V */
            Sys_ACS_WriteRegister(&ACS->VCC_CTRL,
                                  (ACS->VCC_CTRL + VCC_50MV_OFFSET));
        }
        else if (Sys_Trim_LoadDCDC(TRIM, TARGET_VCC_1350) == ERROR_NO_ERROR)
        {
            /* Save vcc_trim value for 1.25 V to restore it later */
            vcc_trim = (ACS->VCC_CTRL & ACS_VCC_CTRL_VTRIM_Mask) -
                        (VCC_50MV_OFFSET * 2);

            /* Adjust calibrated 1.35 V setting down to 1.3 V */
            Sys_ACS_WriteRegister(&ACS->VCC_CTRL,
                                  (ACS->VCC_CTRL - VCC_50MV_OFFSET));
        }
        else
        {
            /* Save vcc_trim value to restore it later */
            vcc_trim = ACS->VCC_CTRL & ACS_VCC_CTRL_VTRIM_Mask;

            /* Indicate an error, attempt to reset CEM102 with
             * user VCC */
            result = ERRNO_GENERAL_FAILURE;
        }

        /* Allow some time for the VCC rail to settle (1s / 50 = 20ms) */
        Sys_Delay(SystemCoreClock / 50);

        if (p_cfg->pwr_cfg == HIGH_VOLTAGE)
        {
            vddc_status = VDDC_ENABLE;

            /* Remove AOUT from aout_gpio */
            Sys_ACS_WriteRegister(&ACS->AOUT_CTRL,
                                  (ACS->AOUT_CTRL &
                                   ~(ACS_AOUT_CTRL_TEST_AOUT_Mask      |
                                     ACS_AOUT_CTRL_AOUT_TO_GPIO_Mask)) |
                                   AOUT_NOT_CONNECTED_TO_GPIO);
        }
        else
        {
            vddc_status = VDDC_DISABLE;
        }

        /* Pull the power enable pin high, enabling CEM102 */
        Sys_GPIO_Set_High(p_cfg->pwr_en);

        /* Monitor VCC until it has settled to the correct voltage */
        uint32_t trim_error = Sys_Trim_VerifyTrims((TRIM_Type *)TRIM);
        uint16_t lsad_data = 0;

        /* This will roughly takes 10 seconds to timeout */
        timeout = CEM102_VCC_OK_TIMEOUT;

        while ((!CEM102_VCC_OK(trim_error, VCC_PWR_RESET_TARGET, &lsad_data)) && timeout)
        {
            if (!(--timeout))
            {
                result = ERRNO_VCC_LOW_ERROR;
            }

            SYS_WATCHDOG_REFRESH();
        }

        if (result == ERRNO_NO_ERROR)
        {
            /* Clear all flags */
            result |= CEM102_System_Command(p_cfg, SYSCTRL_CMD_ALL_ERR_MASK);

            /* Check that the digital reset is complete and the status bit cleared */
            timeout = CEM102_DIG_RESET_TIMEOUT;
            result |= CEM102_Register_Read(p_cfg, SYS_STATUS, &read_data);
            while (((read_data & (DIGITAL_RESET)) == (DIGITAL_RESET)) && timeout)
            {
                result |= CEM102_System_Command(p_cfg, SYSCTRL_CMD_ALL_ERR_MASK);
                result |= CEM102_Register_Read(p_cfg, SYS_STATUS, &read_data);
                if (--timeout == 0)
                {
                    result = ERRNO_TIMEOUT;
                }
            }
        }

        if (result == ERRNO_NO_ERROR)
        {
            /* Drop the CEM102 bandgap voltage to its mid level,
             * based on the ATE report the minimum trim value is chosen
             * to avert digital reset */
            result |= CEM102_Register_Write(p_cfg, ANA_TRIM0, 0xD);

            /* Disable the VDDC regulator */
            result |= CEM102_Register_Write(p_cfg, ANA_CFG2, vddc_status | VDDA_ERR_ENABLE);

            /* Return VCC voltage to 1.25 V */
            Sys_ACS_WriteRegister(&ACS->VCC_CTRL, (ACS->VCC_CTRL & ~ACS_VDDC_CTRL_VTRIM_Mask)
                                  | vcc_trim);

            /* Clear VDDA error in order before checking VDDA error. We want to
             * establish that CEM102 VDDA is stable. */
            result |= CEM102_Register_Write(p_cfg, SYSCTRL_CMD, VDDA_ERR_CLR_CMD);

            /* Wait until DIGITAL_RESET_STATUS and VDDA_ERR_STATUS are clear */
            timeout = CEM102_VDDA_DIG_RESET_TIMEOUT;
            result |= CEM102_Register_Read(p_cfg, SYS_STATUS, &read_data);
            while (((read_data & (DIGITAL_RESET | VDDA_ERR)) != 0) && timeout)
            {
                result |= CEM102_Register_Write(p_cfg, SYSCTRL_CMD, VDDA_ERR_CLR_CMD);
                result |= CEM102_Register_Read(p_cfg, SYS_STATUS, &read_data);
                if (--timeout == 0)
                {
                    result = ERRNO_TIMEOUT;
                }
            }

            if (read_data != ERRNO_NO_ERROR)
            {
                result = ERRNO_GENERAL_FAILURE;
            }
        }
    }
    return result;
}


int CEM102_Power_Init(const CEM102_Device *p_cfg)
{
    uint16_t temp;
    uint16_t rf_clk;
    int result;
    uint16_t vddc_status;
    uint16_t timeout;

    /* Enable all the needed supplies, error monitoring of VDDA, and
     * turn on the RC clock if a clock isn't provided externally.
     */
    rf_clk = p_cfg->clk >= 0 ? RC_CLK_DISABLE: RC_CLK_ENABLE;

    /* Check status of VDDC regulator */
    if (p_cfg->pwr_cfg == HIGH_VOLTAGE)
    {
        vddc_status = VDDC_ENABLE;
    }
    else
    {
        vddc_status = VDDC_DISABLE;
    }

    result = CEM102_Register_Write(p_cfg, ANA_CFG2,
                                   (vddc_status | VREF_ENABLE |
                                    SENSOR_CAL_DISABLE | IREF_ENABLE |
                                    VDDA_ENABLE | VDDA_ERR_ENABLE | rf_clk |
                                    BG_CHOP_CLK_ENABLE | BG_CHOP_CLK_16KHZ));

    if (result == ERRNO_NO_ERROR)
    {
        /* Select the correct clock source (between the internal oscillator or
         * external source */
        rf_clk = p_cfg->clk >= 0 ? EXT_CLK_ENABLE: OSC_CLK_ENABLE;

        result = CEM102_Register_Read(p_cfg, SYSCTRL_AUX_CFG, &temp);
        temp &= ~SYSCTRL_AUX_CFG_SYS_CLK_CFG_MASK;
        if (result == ERRNO_NO_ERROR)
        {
            result = CEM102_Register_Write(p_cfg, SYSCTRL_AUX_CFG, temp | rf_clk);
        }
    }

    if (result == ERRNO_NO_ERROR)
    {
        uint16_t data = VDDA_ERR_IRQ_DISABLE | DIGITAL_RESET_IRQ_DISABLE;
        uint16_t mask = SYSCTRL_IRQ_CFG_DIGITAL_RESET_IRQ_CFG_MASK |
                        SYSCTRL_IRQ_CFG_VDDA_ERR_IRQ_CFG_MASK;

        /* VDDA_ERR is disabled until initialization and calibration is over */
        result = CEM102_Register_ReadModifyWrite(p_cfg, SYSCTRL_IRQ_CFG, data,
                                                 mask);
    }

    /* Make sure that the VDDA_ERR bit is cleared */
    CEM102_Register_Read(p_cfg, SYS_STATUS, &temp);
    timeout = VDDA_ERR_CLR_TIMEOUT;
    while ((temp & VDDA_ERR) && (timeout > 0))
    {
        CEM102_Register_Write(p_cfg, SYSCTRL_CMD, VDDA_ERR_CLR_CMD);
        CEM102_Register_Read(p_cfg, SYS_STATUS, &temp);

        if (--timeout == 0)
        {
            result = ERRNO_TIMEOUT;
        }
    }

    return result;
}

int CEM102_Power_VDDADischarge(const CEM102_Device *p_cfg, unsigned int time)
{
    uint16_t temp;
    int result;

    if (SystemCoreClock < 1000)
    {
        /* Flag a general failures, since SystemCoreClock isn't set
         * correctly as it indicates the frequency is less than 1000 Hz. */
        return ERRNO_GENERAL_FAILURE;
    }

    /* Save the current value of the analog configuration 2 register */
    result = CEM102_Register_Read(p_cfg, ANA_CFG2, &temp);

    if (result == ERRNO_NO_ERROR)
    {
        /* Enable the VDDA discharge, wait the specified amount of time, then
         * restore the previous value to analog configuration 2 */
        CEM102_Register_Write(p_cfg, ANA_CFG2, temp | VDDA_DISCHARGE_ENABLE);
        Sys_Delay(time * SystemCoreClock / 1000);
        result = CEM102_Register_Write(p_cfg, ANA_CFG2, temp);
    }
    return result;
}

int CEM102_Power_DeviceTrim(const CEM102_Device *p_cfg, CEM102_TrimStruct *p_trim_struct)
{
    int result;

    result = CEM102_Register_Write(p_cfg, ANA_TRIM0, p_trim_struct->trim[0]);
    result |= CEM102_Register_Write(p_cfg, ANA_TRIM1, p_trim_struct->trim[1]);
    return result;
}

void CEM102_Power_VDDCCharge(const CEM102_Device *p_cfg)
{
    if (p_cfg->pwr_cfg == HIGH_VOLTAGE)
    {
        uint32_t aout_gpio = p_cfg->aout_gpio;
        SYS_GPIO_CONFIG(aout_gpio, GPIO_MODE_DISABLE | GPIO_NO_PULL);

        /* Bring out VCC on AOUT, put AOUT on p_cfg->aout_gpio */
        Sys_ACS_WriteRegister(&ACS->AOUT_CTRL,
                              (ACS->AOUT_CTRL &
                               ~(ACS_AOUT_CTRL_TEST_AOUT_Mask      |
                                 ACS_AOUT_CTRL_AOUT_TO_GPIO_Mask)) |
                              AOUT_VCC |
                              (aout_gpio << ACS_AOUT_CTRL_AOUT_TO_GPIO_Pos) |
                              SEL_AOUT_TO_GPIO);
    }
    else
    {
        /* We do not bring out VCC on AOUT in the case where
         * RSL15 supplies CEM102 */
    }
}

void CEM102_SAR_Init(const CEM102_Device *p_cfg, uint32_t sar_cfg, uint32_t sar_ctrl, uint32_t clk_cfg)
{
    static uint8_t sar_calibration_complete = 0;
    float gain_correction = 0;
    uint32_t sar_adc_offset = 0;
    uint32_t sar_supply = 0;

    /* Enable Sensor Interface */
    Sys_Sensor_Enable();

    if (sar_calibration_complete == 0)
    {
        /* Reset the Sensor interface (SAR and Pulse Counter) */
        Sys_Sensor_TimerReset();

        sar_supply = (sar_cfg & SAR_SUPPLY_BY_GPIO9) ? SAR_SUPPLY_BY_GPIO9 : SAR_SUPPLY_BY_VBAT;

        /* SAR Calibration */
        Sys_Calibrate_SARADC(CEM102_SAR_SUPPLY, sar_supply, &sar_adc_offset, &gain_correction);

        /* Calibration completed */
        sar_calibration_complete = 1;
    }

    /* Configure the SAR-ADC interface */
    Sys_Sensor_SARConfig(sar_cfg, sar_ctrl, clk_cfg);
}

int CEM102_SAR_StartConversion(uint32_t conv_mode)
{
    int result = ERRNO_NO_ERROR;

    if (conv_mode == SAR_START_SINGLE || conv_mode == SAR_START_CONTINUOUS)
    {
        /* Configure the SAR conversion mode */
        SENSOR->SAR_CTRL |= conv_mode;
    }
    else
    {
        result = ERRNO_PARAM_ERROR;
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Enable the sensor */
        Sys_Sensor_Enable();
    }

    return result;
}

int CEM102_VBAT_OK(uint32_t trim_error, uint32_t threshold, uint16_t *p_data)
{
    return CEM102_VOLTAGE_OK(trim_error, threshold, p_data, MEASURE_VBAT);
}

int CEM102_VCC_OK(uint32_t trim_error, uint32_t threshold, uint16_t *p_data)
{
    return CEM102_VOLTAGE_OK(trim_error, threshold, p_data, MEASURE_VCC);
}

int CEM102_VOLTAGE_OK(uint32_t trim_error, uint32_t threshold, uint16_t *p_data, uint32_t rail)
{
    /* Array containing LSAD gain and offset values from TRIM sector */
    static struct F_LSAD_TRIM g_f_lsad_gain_offset[CEM102_LSAD_CHANNEL_NUM];

    /* Default trim value sector. */
    static TRIM_Type *trims = TRIM;

    uint32_t voltage;
    uint32_t LSAD_Data_avg = 0;
    uint32_t LSAD_Data[CEM102_LSAD_DSAMPLES + CEM102_LSAD_NSAMPLES];

    uint32_t lsad_input_sel;
    uint32_t lsad_pre_sel;
    uint32_t lsad_monitor_cfg;
    uint32_t lsad_cfg;
    uint32_t lsad_int_en;

    struct F_LSAD_TRIM *gain_offset = &g_f_lsad_gain_offset[CEM102_VBAT_CHANNEL];

    if (!(trim_error & ERROR_LSAD_INVALID))
    {
        /* Using low frequency trims, as we are at max pre-scale */
        Sys_LSAD_Gain_Offset(&(trims->lsad_trim),
                             &g_f_lsad_gain_offset[CEM102_VBAT_CHANNEL]);
    }

    /* Backup Registers */
    lsad_input_sel = LSAD->INPUT_SEL[CEM102_VBAT_CHANNEL];
    lsad_pre_sel = LSAD->PRE_SEL_INPUT;
    lsad_monitor_cfg = LSAD->MONITOR_CFG;
    lsad_int_en = LSAD->INT_ENABLE;
    lsad_cfg = LSAD->CFG;

    /* Disable LSAD */
    LSAD->CFG = LSAD_DISABLE;

    /* Clear Interrupt Flags */
    LSAD->MONITOR_STATUS = LSAD_READY_CLEAR;

    /* Configure CEM102_VBAT_CHANNEL for voltage rail measurement */
    if (rail == MEASURE_VBAT)
    {
        Sys_LSAD_InputConfig(CEM102_VBAT_CHANNEL, LSAD_VBAT_DIV2_CFG, LSAD_GROUND_CFG);
    }
    else
    {
        Sys_ACS_WriteRegister(&ACS->AOUT_CTRL,
                              (ACS->AOUT_CTRL & ~ACS_AOUT_CTRL_TEST_AOUT_Mask) |
                              (AOUT_VCC));
        Sys_LSAD_InputConfig(CEM102_VBAT_CHANNEL, LSAD_AOUT_CFG, LSAD_GROUND_CFG);
    }

    /* Monitor Channel 0 , No Alarm */
    LSAD->MONITOR_CFG = MONITOR_ALARM_NONE | MONITOR_CH0;

    /* LSAD Normal Mode with Prescale 1280 */
    LSAD->CFG = LSAD_NORMAL | LSAD_PRESCALE_1280H;

    /* Enable LSAD Interrupts to poll flag bit */
    LSAD->INT_ENABLE = LSAD_INT_CH0 | LSAD_INT_EN;

    for (int i = 0; i < (CEM102_LSAD_DSAMPLES + CEM102_LSAD_NSAMPLES); i++)
    {
        /* Wait until the conversion completes */
        while (!(LSAD->MONITOR_STATUS & (LSAD_READY_TRUE)));

        /* Read converted data */
        LSAD_Data[i] = (i >= CEM102_LSAD_DSAMPLES) ? LSAD->DATA_TRIM_CH[CEM102_VBAT_CHANNEL] : 0;

        /* Clear Interrupt Flags */
        LSAD->MONITOR_STATUS = LSAD_READY_CLEAR;
    }

    /* Restore backed up Registers */
    LSAD->CFG = LSAD_DISABLE;
    LSAD->INPUT_SEL[CEM102_VBAT_CHANNEL] = lsad_input_sel;
    LSAD->PRE_SEL_INPUT = lsad_pre_sel;
    LSAD->MONITOR_CFG = lsad_monitor_cfg;
    LSAD->INT_ENABLE = lsad_int_en;
    LSAD->CFG = lsad_cfg;

    /* Average out the LSAD Samples */
    for (int i = CEM102_LSAD_DSAMPLES; i < (CEM102_LSAD_DSAMPLES + CEM102_LSAD_NSAMPLES); i++)
    {
        LSAD_Data_avg = LSAD_Data[i] + LSAD_Data_avg;
    }

    LSAD_Data_avg /= CEM102_LSAD_NSAMPLES;

    /* Process LSAD Data */
    if (gain_offset->lf_gain != 0)
    {
        voltage = (uint32_t)(((CEM102_CONVERT(LSAD_Data_avg) / CEM102_V_TO_MV_F)
                              - gain_offset->lf_offset) /
                              gain_offset->lf_gain * V_TO_MV);
    }

    else
    {
        voltage = (uint32_t)((2000 * LSAD_Data_avg) / (0x4000));
    }

    if (rail == MEASURE_VBAT)
    {
        *p_data = voltage * 2;
    }
    else
    {
        *p_data = voltage;
    }
    return (*p_data) < threshold ? 0 : 1;
}

void CEM102_VBAT_VDDA_MonitorStart(const CEM102_Device *p_cfg, uint8_t measurement_en, uint32_t VBAT_margin)
{
    uint32_t sar_cfg = 0;
    uint32_t sar_ctrl = 0;
    uint32_t clk_cfg = 0;
    uint32_t sar_input = 0;

    /* Selection of GPIO to input signal */
    GPIO->CFG[9] = GPIO_MODE_DISABLE | GPIO_NO_PULL;

    sar_input = (p_cfg->vdda_gpio % 2) ? (((p_cfg->vdda_gpio - 1) / 2) << SENSOR_SAR_CFG_SAR_PRE_SEL_ODD_Pos) :
                                    ((p_cfg->vdda_gpio / 2) << SENSOR_SAR_CFG_SAR_PRE_SEL_EVEN_Pos);

    /* SAR Configuration */
    sar_cfg = sar_input                       |
              SAR_IN_P_SRC_SAR_SUPPLY         |
              SAR_IN_N_SRC_PRE_SEL_ODD        |
              SAR_DATA_OUT_RX_DMA_DISABLED    |
              SAR_DATA_OUT_UPDATE_AUTO        |
              SAR_SUPPLY_AUTO                 |
              SAR_SUPPLY_BY_GPIO9;

    sar_ctrl = SAR_STOP_CONTINUOUS         |
               SAR_CONV_14BIT              |
               SAR_NSAMPLING_CYCLE_1       |
               SAR_SEL_GATED_SIGNED_COMPENSATED;

    /* Set sensor interface clock source */
    clk_cfg = SENSOR_CLK_STANDBY_BYTE;

    CEM102_SAR_Init(p_cfg, sar_cfg, sar_ctrl, clk_cfg);

    /* Calculate Threshold Min. register values */
    VBAT_margin = (uint32_t)((CEM102_SAR_MAX_VALUE * VBAT_margin)/(VBAT_margin + CEM102_VDDA_TYPICAL));

    VBAT_margin = (measurement_en ? SENSOR_THRESHOLD_MIN_ENABLED : SENSOR_THRESHOLD_MIN_DISABLED) |
                    ((uint32_t)(VBAT_margin << SENSOR_THRESHOLD_MIN_THRESHOLD_MIN_Pos));

    /* Configure data storage settings */
    Sys_Sensor_StorageConfig(SENSOR_SUMMATION_DISABLED,
                             ((uint32_t)(0x3U << SENSOR_PROCESSING_NBR_SAMPLES_Pos)),
                             VBAT_margin,
                             SENSOR_THRESHOLD_MAX_DISABLED,
                             SENSOR_FIFO_STORE_DISABLED,
                             SENSOR_FIFO_SIZE16,
                             (FIFO_RX_DMA_DISABLED | FIFO_RX_INT_DISABLED));

    /* Clear sticky wake up THRESHOLD FULL flag */
    Sys_ACS_WriteRegister(&ACS->WAKEUP_CTRL, ACS->WAKEUP_CTRL | THRESHOLD_FULL_EVENT_CLEAR);

    /* Start SAR Conversion */
    CEM102_SAR_StartConversion(SAR_START_CONTINUOUS);
}

int CEM102_Calibrate_VCC(const CEM102_Device *p_cfg, uint8_t vcc_min_trim, const uint32_t vcc_target)
{
    uint32_t vcc = 0;
    uint8_t vcc_trim_new = vcc_min_trim;
    uint8_t max_setting = (uint8_t)VCC_TRIM_1P31V;
    uint8_t min_setting = vcc_min_trim;
    uint32_t sar_data_previous = 0;
    uint32_t sar_data_new;
    uint8_t adjust_vcc = 0;
    uint8_t tolerance = 5;
    uint16_t result = ERRNO_NO_ERROR;

    /* Configure SAR ADC for SAR Measurements */
    uint32_t sar_cfg = 0;
    uint32_t sar_ctrl = 0;
    uint32_t clk_cfg = 0;
    uint32_t sar_input = 0;

    GPIO->CFG[9] = GPIO_MODE_DISABLE | GPIO_NO_PULL;

    sar_input = (p_cfg->vdda_gpio % 2) ? (((p_cfg->vdda_gpio - 1) / 2) << SENSOR_SAR_CFG_SAR_PRE_SEL_ODD_Pos) :
                ((p_cfg->vdda_gpio / 2) << SENSOR_SAR_CFG_SAR_PRE_SEL_EVEN_Pos);

    /* Selection of GPIO to input signal */
    sar_cfg = sar_input                       |
              SAR_IN_P_SRC_SAR_SUPPLY         |
              SAR_IN_N_SRC_PRE_SEL_ODD        |
              SAR_DATA_OUT_RX_DMA_DISABLED    |
              SAR_DATA_OUT_UPDATE_AUTO        |
              SAR_SUPPLY_AUTO                 |
              SAR_SUPPLY_BY_GPIO9;

    /* Run conversion */
    sar_ctrl = SAR_STOP_CONTINUOUS         |
               SAR_CONV_14BIT              |
               SAR_NSAMPLING_CYCLE_1       |
               SAR_SEL_GATED_SIGNED_COMPENSATED;

    /* Set sensor interface clock source */
    clk_cfg = SENSOR_CLK_STANDBY_BYTE;

    CEM102_SAR_Init(p_cfg, sar_cfg, sar_ctrl, clk_cfg);

    CEM102_SAR_StartConversion(SAR_START_CONTINUOUS);

    do
    {
        sar_data_new = SENSOR->SAR_DATA;

        if (sar_data_new != sar_data_previous)
        {
            sar_data_previous = sar_data_new;

            /* VCC = RSL15_VDDA/2 */
            vcc = ((CEM102_SAR_MAX_VALUE * CEM102_VDDA_TYPICAL) / (CEM102_SAR_MAX_VALUE - sar_data_new)) / 2;

            if (vcc <= (vcc_target - tolerance))
            {
                vcc_trim_new += 1;
                adjust_vcc = 1;
            }
            else if (vcc > (vcc_target + tolerance))
            {
                vcc_trim_new -= 1;
                adjust_vcc = 1;
            }
            else
            {
                adjust_vcc = 0;
            }

            if (adjust_vcc)
            {
                if (vcc_trim_new < min_setting)
                {
                    vcc_trim_new = min_setting;
                    result = ERRNO_GENERAL_FAILURE;
                }

                if (vcc_trim_new > max_setting)
                {
                    vcc_trim_new = max_setting;
                    result = ERRNO_GENERAL_FAILURE;
                }

                Sys_ACS_WriteRegister(&ACS->VCC_CTRL, (ACS->VCC_CTRL & ~ACS_VCC_CTRL_VTRIM_Mask)
                                                      | (vcc_trim_new << ACS_VCC_CTRL_VTRIM_Pos));
                adjust_vcc = 0;
            }
        }
    }
    while (((vcc <= (vcc_target - tolerance)) || (vcc > (vcc_target +
             tolerance))) && (result == ERRNO_NO_ERROR));

    /* Disable Sensor Interface */
    Sys_Sensor_Disable();

    return result;
}

void CEM102_Calculate_VBAT(uint16_t *p_voltage)
{
    *p_voltage = (uint16_t)(((float)(CEM102_VDDA_TYPICAL))
                          / (1.0f - ((float)(SENSOR->SAR_DATA)) / ((float)(CEM102_SAR_MAX_VALUE))));
}

