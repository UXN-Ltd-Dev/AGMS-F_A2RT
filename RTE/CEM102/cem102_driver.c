/**
 * @file cem102_driver.c
 * @brief CEM102 Initialization and Base Driver implementation
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

/* ----------------------------------------------------------------------------
 * Version support
 * --------------------------------------------------------------------------*/
#define CEM102_DRIVER_VER_MAJOR         0x00
#define CEM102_DRIVER_VER_MINOR         0x01
#define CEM102_DRIVER_VER_REVISION      0x00

#define CEM102_DRIVER_VER               ((CEM102_DRIVER_VER_MAJOR << 12)   | \
                                         (CEM102_DRIVER_VER_MINOR << 8)    | \
                                         (CEM102_DRIVER_VER_REVISION))

/* Loop 5 times before returning with TIMEOUT error. */
#define CEM102_SPI_TIMEOUT_LOOP         (5)

/* Size given in 16 bits sized words minus one word (2 bytes) for header */
#define CEM102_SPI_MAX_DATA_SZ          ((CEM102_MAX_TRANSFER_LENGTH / 2) - 1)


/* To retain compatibility with older RSL15 SDK */
#if !defined(SPI_SELECT_CONTROLLER) && defined(SPI_SELECT_MASTER)
#define SPI_SELECT_CONTROLLER  SPI_SELECT_MASTER
#endif

/** CEM102 Driver version */
const unsigned int CEM102_Driver_Version = CEM102_DRIVER_VER;

/* ----------------------------------------------------------------------------
 * Private variables
 * --------------------------------------------------------------------------*/

/* SPI transfer buffers */
volatile CEM102_SPI_FLAG cem102_spi_status = SPI_TRANSFER_IDLE;
static uint8_t spi_tx_buffer[CEM102_MAX_TRANSFER_LENGTH];
static uint8_t spi_rx_buffer[CEM102_MAX_TRANSFER_LENGTH];
static unsigned int spi_rx_len;

/* Data structure for revision 1 of the OTP */
CEM102_TrimStruct_v1 *trim_struct_v1;

/* Data structure for revision 2 of the OTP */
CEM102_TrimStruct *trim_struct;

/* Overall channel gain for each channel */
float channel_gain[2];

/* When enabled, threshold violation detection is enabled only during
 * measurement and kept disabled during calibration and diagnostics.
 *
 * Note:
 * Legacy definition, not used anymore. Use threshold_detection[CHx]
 * in CEM102_Device structure.
 */
uint8_t cem102_threshold_violation = CEM102_THRESH_VIOL_DISABLED;

/* Events needed from applications to server different scenarios */
uint32_t cem102_app_events = 0;

/* ----------------------------------------------------------------------------
 * Initialization
 * --------------------------------------------------------------------------*/

int CEM102_Initialize(const CEM102_Device *p_cfg)
{
    int result = ERRNO_NO_ERROR;

    if (p_cfg == NULL)
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        /* Configure the specified SPI interface. DMA support for the SPI interface
         * will be configured when used. */
        Sys_SPI_GPIOConfig(&SPI[p_cfg->spi], SPI_SELECT_CONTROLLER, p_cfg->gpio_cfg,
                           p_cfg->spi_clk, p_cfg->spi_ctrl, p_cfg->spi_io0, p_cfg->spi_io1);
        Sys_SPI_Config(&SPI[p_cfg->spi], CEM102_SPI_CFG | p_cfg->spi_prescale);
        Sys_SPI_TransferConfig(&SPI[p_cfg->spi], CEM102_SPI_CTRL);

        /* Configure the supporting GPIOs if specified, including the clock,
         * interrupt, and pwr_en pin configurations. */
        if (p_cfg->clk >= 0)
        {
            SYS_GPIO_CONFIG(p_cfg->clk, p_cfg->gpio_cfg | GPIO_MODE_STANDBYCLK);
        }
        if ((int32_t)p_cfg->irq >= 0)
        {
            SYS_GPIO_CONFIG(p_cfg->irq, p_cfg->gpio_cfg | GPIO_MODE_INPUT);
            Sys_GPIO_IntConfig(p_cfg->irq_index,
                               (GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_RISING_EDGE | p_cfg->irq),
                               GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);
            NVIC_EnableIRQ(GPIO0_IRQn + p_cfg->irq_index);
        }
        if ((int32_t)p_cfg->pwr_en >= 0)
        {
            /* Enable the pwr_en as an output, ensure the signal is inactive
             * before switching to it if the output can be controlled. */
            Sys_GPIO_Set_High(p_cfg->pwr_en);
            SYS_GPIO_CONFIG(p_cfg->pwr_en, p_cfg->gpio_cfg | GPIO_MODE_GPIO_OUT);
            Sys_GPIO_Set_High(p_cfg->pwr_en);
        }

        uint32_t vdda_gpio = p_cfg->vdda_gpio;
        if (vdda_gpio == GPIO9)
        {
            result = ERRNO_PARAM_ERROR;
        }
        else if (vdda_gpio < GPIO_PAD_COUNT)
        {
            SYS_GPIO_CONFIG(vdda_gpio,
                            GPIO->CFG[vdda_gpio] &
                            (~(GPIO_CFG_PULL_CTRL_Mask | GPIO_CFG_IO_MODE_Mask)));
        }
        else
        {
            /* No valid GPIO selected. Do not return an error since no
             * GPIO selected is a valid condition */
        }

        /* Disconnect pull settings from GPIO connected to VDDC */
        uint32_t aout_gpio = p_cfg->aout_gpio;
        if (p_cfg->pwr_cfg == HIGH_VOLTAGE)
        {
            if (vdda_gpio < GPIO_PAD_COUNT)
            {
                SYS_GPIO_CONFIG(aout_gpio, GPIO_MODE_DISABLE | GPIO_NO_PULL);
            }
            else
            {
                result = ERRNO_PARAM_ERROR;
            }
        }

        /* When configuring the IO0, IO1 pins eliminate the pull register configuration as
         * these analog signals don't need them.
         */
        if (p_cfg->io0 >= 0)
        {
            uint32_t io0 = (uint32_t)p_cfg->io0;
            SYS_GPIO_CONFIG(io0, (p_cfg->gpio_cfg | p_cfg->io0_type) & ~GPIO_CFG_PULL_CTRL_Mask);
        }

        if (p_cfg->io1 >= 0)
        {
            uint32_t io1 = (uint32_t)p_cfg->io1;
            SYS_GPIO_CONFIG(io1, (p_cfg->gpio_cfg | p_cfg->io1_type) & ~GPIO_CFG_PULL_CTRL_Mask);
        }
    }
    return result;
}

static void CEM102_DataToSPIHeader(uint32_t cmd, uint8_t addr, uint32_t len)
{
    if (len != 0)
    {
        uint32_t val = (cmd >> CEM102_CMD_Pos) + (len - CEM102_INDEX_ADJ);

        spi_tx_buffer[CEM102_SPI_BUF_ADDR_IDX] = addr;
        spi_tx_buffer[CEM102_SPI_BUF_CMD_IDX] = val & CEM102_8_BITS_MASK;
    }
}

static void CEM102_DataToSPIBuffer(uint16_t data, uint8_t index)
{
    if (index < CEM102_SPI_FRAME_MAX_DBYTES)
    {
        uint8_t offset0 = (index * CEM102_SPI_DBYTE_MULT_FACTOR) +
                          CEM102_SPI_DATA_IDX_MS_BYTE;
        uint8_t offset1 = (index * CEM102_SPI_DBYTE_MULT_FACTOR) +
                          CEM102_SPI_DATA_IDX_LS_BYTE;
        spi_tx_buffer[offset0] = (data >> CEM102_SHIFT_FOR_BYTE) &
                                  CEM102_8_BITS_MASK;
        spi_tx_buffer[offset1] = data & CEM102_8_BITS_MASK;
    }
}

static uint16_t CEM102_DataFromSPIBuffer(uint8_t index)
{
    uint16_t data = CEM102_INITIAL_VALUE;

    if (index < CEM102_SPI_FRAME_MAX_DBYTES)
    {
        uint8_t offset0 = (index * CEM102_SPI_DBYTE_MULT_FACTOR) +
                          CEM102_SPI_DATA_IDX_MS_BYTE;
        uint8_t offset1 = (index * CEM102_SPI_DBYTE_MULT_FACTOR) +
                          CEM102_SPI_DATA_IDX_LS_BYTE;
        data = spi_rx_buffer[offset0];
        data <<= CEM102_SHIFT_FOR_BYTE;
        data |= spi_rx_buffer[offset1];
    }
    return data;
}

/* ----------------------------------------------------------------------------
 * Transfer support
 * --------------------------------------------------------------------------*/

/**
 * @brief       Initiate transfer using SPI interface (modified from SPI driver,
 *              selecting for DMA)
 * @param[in]   p_cfg    CEM102 device configuration
 * @param[in]   data_out Data to be sent out
 * @param[in]   data_in  Input buffer
 * @param[in]   num  Length of data, in bytes to be sent or received.
 * @return      Execution status
 */
static int CEM102_Transfer(const CEM102_Device *p_cfg, const void *data_out,
                           void *data_in, uint32_t num)
{
    int status = ERRNO_NO_ERROR;

    if ((num > CEM102_MAX_TRANSFER_LENGTH) || (p_cfg == NULL))
    {
        status = ERRNO_PARAM_ERROR;
    }
    else if ((data_out == NULL) && (data_in == NULL))
    {
        status = ERRNO_PARAM_ERROR;
    }
    else if (cem102_spi_status != SPI_TRANSFER_IDLE)
    {
        /* Previous transfer is not completed yet */
        status = ERRNO_INTERFACE_BUSY;
    }
    else
    {
        /* Activate the chip (source) select line */
        Sys_SPI_TransferConfig(&SPI[p_cfg->spi], SPI_CS_0);

        /* Configure SPI to RW operation and start transfer (master) */
        Sys_SPI_TransferConfig(&SPI[p_cfg->spi], SPI_ENABLE |
                               SPI_MODE_READ_WRITE | SPI_CS_0);

        /* Updates the transfer status */
        cem102_spi_status = SPI_TX_ACTIVE;

        /* If we have a receive portion of the transfer, configure the
         * specified DMA channel to support the RX portion of the specified
         * SPI transfer.
         */
        if (data_in != NULL)
        {
            /* Since we have an RX component as well, change the status to
             * indicate a bidirectional transfer */
            cem102_spi_status = SPI_TRANSFER_ACTIVE;
        }

        Sys_DMA_ChannelConfig(&DMA[p_cfg->dma[1]],
                              CEM102_DMA_RX_CFG,
                              num,
                              0,
                              (uint32_t)&SPI[p_cfg->spi].RX_DATA,
                              (uint32_t)spi_rx_buffer);

        /* Clear DMA status register, buffers, and counts. */
        DMA[p_cfg->dma[1]].STATUS = DMA_COMPLETE_INT_CLEAR;
        DMA[p_cfg->dma[1]].CTRL |= DMA_CLEAR_BUFFER | DMA_CLEAR_CNTS;

        /* Enable the interrupts, clearing any pending triggers. */
        NVIC_ClearPendingIRQ(DMA0_IRQn + p_cfg->dma[1]);
        NVIC_EnableIRQ(DMA0_IRQn + p_cfg->dma[1]);

        /* Configure the specified DMA channel to support the TX portion of the
         * specified SPI transfer */
        Sys_DMA_ChannelConfig(&DMA[p_cfg->dma[0]],
                              CEM102_DMA_TX_CFG,
                              num,
                              0,
                              (uint32_t)spi_tx_buffer,
                              (uint32_t)&SPI[p_cfg->spi].TX_DATA);

        /* Clear DMA status register, buffers, and counts. */
        DMA[p_cfg->dma[0]].STATUS = DMA_COMPLETE_INT_CLEAR;
        DMA[p_cfg->dma[0]].CTRL |= DMA_CLEAR_BUFFER | DMA_CLEAR_CNTS;

        /* Enable the interrupts, clearing any pending triggers. */
        NVIC_ClearPendingIRQ(DMA0_IRQn + p_cfg->dma[0]);
        NVIC_EnableIRQ(DMA0_IRQn + p_cfg->dma[0]);

        /* Start the DMA channels */
        DMA[p_cfg->dma[1]].CTRL = (DMA[p_cfg->dma[1]].CTRL &
                                   ~(DMA_CTRL_MODE_ENABLE_Mask |
                                   DMA_CTRL_TRIGGER_SOURCE_Mask)) | DMA_ENABLE;
        DMA[p_cfg->dma[0]].CTRL = (DMA[p_cfg->dma[0]].CTRL &
                                   ~(DMA_CTRL_MODE_ENABLE_Mask |
                                   DMA_CTRL_TRIGGER_SOURCE_Mask)) | DMA_ENABLE;
    }
    return status;
}

int CEM102_Register_QueueWrite(const CEM102_Device *p_cfg, uint8_t addr, unsigned int len)
{
    int status = ERRNO_NO_ERROR;

    if ((len == 0) || (len > CEM102_SPI_FRAME_MAX_DBYTES) || (p_cfg == NULL))
    {
        status = ERRNO_PARAM_ERROR;
    }
    else
    {
        CEM102_DataToSPIHeader(CEM102_CMD_WRITE, addr, len);

        /* Write the register */
        status = CEM102_Transfer(p_cfg, spi_tx_buffer, NULL,
                                  (len << CEM102_SHIFT_TO_MULTIPLY_BY_2) +
                                   CEM102_SPI_FRAME_HDR_SZ);
    }
    return status;
}

int CEM102_Register_BufferWrite(const CEM102_Device *p_cfg, uint8_t addr,
                                uint16_t *p_data, unsigned int len)
{
    int status = ERRNO_NO_ERROR;

    if ((len > CEM102_SPI_FRAME_MAX_DBYTES) || (p_cfg == NULL) ||
        (p_data == NULL))
    {
        status = ERRNO_PARAM_ERROR;
    }
    else
    {
        for (uint8_t i = 0; i < len; i++)
        {
            CEM102_DataToSPIBuffer(*(p_data + i), i);
        }

        /* Write the registers */
        status = CEM102_Register_QueueWrite(p_cfg, addr, len);
    }
    return status;
}

static int CEM102_Wait_SPI_Idle(int32_t loop, unsigned int delay)
{
    int result = ERRNO_NO_ERROR;

    /* Loop with a delay. Bail out at the end of the loop */
    while (cem102_spi_status != SPI_TRANSFER_IDLE)
    {
        if (loop-- > 0)
        {
            Sys_Delay(delay);
        }
        else
        {
            result = ERRNO_TIMEOUT;
            break;
        }
    }
    return result;
}

int CEM102_Register_Multiple_Read(const CEM102_Device *p_cfg, uint8_t addr,
                                  uint16_t *p_data, unsigned int len)
{
    int result = ERRNO_PARAM_ERROR;

    if ((len <= CEM102_SPI_FRAME_MAX_DBYTES) && (p_cfg != NULL) &&
        (p_data != NULL))
    {
        result = CEM102_Register_QueueRead(p_cfg, addr, len);
        if (ERRNO_NO_ERROR == result)
        {
            result = CEM102_Wait_SPI_Idle(CEM102_SPI_TIMEOUT_LOOP,
                                          CEM102_SYS_DELAY_100US);
        }
        if (ERRNO_NO_ERROR == result)
        {
            result = CEM102_Register_BufferRead(p_cfg, p_data, len);
        }
    }
    return result;
}

int CEM102_Register_Multiple_Write(const CEM102_Device *p_cfg, uint8_t addr,
                                   uint16_t *p_data, unsigned int len)
{
    int result = ERRNO_NO_ERROR;

    if ((len > CEM102_SPI_FRAME_MAX_DBYTES) || (p_cfg == NULL) ||
        (len == 0) || (p_data == NULL))
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        result = CEM102_Register_BufferWrite(p_cfg, addr, p_data, len);
        if (result == ERRNO_NO_ERROR)
        {
            result = CEM102_Wait_SPI_Idle(CEM102_SPI_TIMEOUT_LOOP,
                                          CEM102_SYS_DELAY_100US);
        }
    }
    return result;
}

int CEM102_Register_Write(const CEM102_Device *p_cfg, uint8_t addr, uint16_t data)
{
    int result = ERRNO_NO_ERROR;

    if (p_cfg == NULL)
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        CEM102_DataToSPIBuffer(data, 0);

        result = CEM102_Register_QueueWrite(p_cfg, addr, 1);
        if (result == ERRNO_NO_ERROR)
        {
            /* Block with timeout, while waiting for the write to complete. */
            result = CEM102_Wait_SPI_Idle(CEM102_SPI_TIMEOUT_LOOP,
                                          CEM102_SYS_DELAY_100US);
        }
    }
    return result;
}

int CEM102_Register_VerifiedWrite(const CEM102_Device *p_cfg, uint8_t addr, uint16_t data)
{
    int result = ERRNO_NO_ERROR;

    if (p_cfg == NULL)
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        result = CEM102_Register_Write(p_cfg, addr, data);

        uint16_t temp = CEM102_16_BITS_MASK;
        if (result == ERRNO_NO_ERROR)
        {
            result = CEM102_Register_Read(p_cfg, addr, &temp);
        }
        if (result == ERRNO_NO_ERROR)
        {
            if (temp != data)
            {
                result = ERRNO_GENERAL_FAILURE;
            }
        }
    }
    return result;
}

int CEM102_Register_QueueRead(const CEM102_Device *p_cfg, uint8_t addr, unsigned int len)
{
    int status = ERRNO_NO_ERROR;

    if ((len == 0) || (len > CEM102_SPI_FRAME_MAX_DBYTES) || (p_cfg == NULL))
    {
        status = ERRNO_PARAM_ERROR;
    }
    else
    {
        CEM102_DataToSPIHeader(CEM102_CMD_READ, addr, len);
        spi_rx_len = len << CEM102_SHIFT_TO_MULTIPLY_BY_2;

        /* Write the command and read the 16-bit value */
        status = CEM102_Transfer(p_cfg, spi_tx_buffer, spi_rx_buffer,
                                  CEM102_SPI_FRAME_HDR_SZ + spi_rx_len);
    }
    return status;
}

int CEM102_Register_BufferRead(const CEM102_Device *p_cfg, uint16_t *p_data,
                               unsigned int len)
{
    int status = ERRNO_NO_ERROR;

    if (!(cem102_spi_status == SPI_TRANSFER_IDLE) &&
        !(cem102_spi_status == SPI_TRANSFER_COMPLETE))
    {
        status = ERRNO_INTERFACE_BUSY;
    }
    else if ((len > CEM102_SPI_FRAME_MAX_DBYTES) || (p_cfg == NULL) ||
             (p_data == NULL))
    {
        status = ERRNO_PARAM_ERROR;
    }
    else
    {
        /* Copy the data read out of the register buffer */
        for (uint8_t i = 0; i < len; i++)
        {
            p_data[i] = CEM102_DataFromSPIBuffer(i);
        }
    }
    return status;
}

int CEM102_Register_Read(const CEM102_Device *p_cfg, uint8_t addr,
                         uint16_t *p_data)
{
    int result = ERRNO_NO_ERROR;

    if ((p_cfg == NULL) || (p_data == NULL))
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        *p_data = CEM102_INITIAL_VALUE;

        result = CEM102_Register_QueueRead(p_cfg, addr,
                                           CEM102_SPI_SINGLE_XFER_COUNT);
        if (result == ERRNO_NO_ERROR)
        {
            /* Block with timeout, while waiting for the read to complete. */
            result = CEM102_Wait_SPI_Idle(CEM102_SPI_TIMEOUT_LOOP,
                                          CEM102_SYS_DELAY_100US);
            if (result == ERRNO_NO_ERROR)
            {
                uint8_t idx = CEM102_SPI_SINGLE_XFER_COUNT - CEM102_INDEX_ADJ;
                *p_data = CEM102_DataFromSPIBuffer(idx);
            }
        }
    }
    return result;
}

int CEM102_Register_ReadModifyWrite(const CEM102_Device *p_cfg, uint8_t addr,
                                    uint16_t data, uint16_t mask)
{
    int result = ERRNO_NO_ERROR;

    if (p_cfg == NULL)
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        uint16_t reg_value = CEM102_INITIAL_VALUE;

        result = CEM102_Register_Read(p_cfg, addr, &reg_value);
        if (result == ERRNO_NO_ERROR)
        {
            /* Clear the bits so that we can "OR" with the data */
            reg_value &= ~mask;

            /* Keep the bits that we want to "OR" */
            data &= mask;

            /* This will set/clear the requested bits for the write operation */
            reg_value |= data;
            result = CEM102_Register_Write(p_cfg, addr, reg_value);
        }
    }
    return result;
}

int CEM102_Register_Restore(const CEM102_Device *p_cfg,
                            CEM102_RegSaveData *p_data)
{
    int result = ERRNO_NO_ERROR;

    if ((p_cfg == NULL) || (p_data == NULL))
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        uint8_t reg_array[MAX_REG_COUNT_FOR_SAVE] = REG_ADDR_LIST_FOR_SAVE;

        for (uint8_t i = 0; i < sizeof(reg_array) / sizeof(reg_array[0]); i++)
        {
            /* Bits in valid and saved, must be set for the restore action */
            if (((p_data->valid) & (1U << i) & (p_data->saved)) == 0)
            {
                continue;
            }
            result = CEM102_Register_Write(p_cfg, reg_array[i],
                                           p_data->value[i]);
            if (result != ERRNO_NO_ERROR)
            {
                break;
            }
        }
    }
    return result;
}

int CEM102_Register_Save(const CEM102_Device *p_cfg,
                         CEM102_RegSaveData *p_data)
{
    int result = ERRNO_NO_ERROR;

    if ((p_cfg == NULL) || (p_data == NULL))
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        uint8_t reg_array[MAX_REG_COUNT_FOR_SAVE] = REG_ADDR_LIST_FOR_SAVE;

        p_data->saved = CEM102_INITIAL_VALUE;
        for (uint8_t i = 0; i < sizeof(reg_array) / sizeof(reg_array[0]); i++)
        {
            /* Check is save request is made */
            if ((p_data->valid & (1U << i)) == 0)
            {
                continue;
            }
            result = CEM102_Register_Read(p_cfg, reg_array[i],
                                          &p_data->value[i]);
            if (result == ERRNO_NO_ERROR)
            {
                /* Mark the registers that are successfully read. */
                p_data->saved |= (1U << i);
            }
        }
    }
    return result;
}

/* ----------------------------------------------------------------------------
 * Call backs and interrupt handlers
 * --------------------------------------------------------------------------*/

void CEM102_TX_Handler(const CEM102_Device *p_cfg)
{
    if (p_cfg != NULL)
    {
        uint32_t event = CEM102_EVENT_IDLE;

        /* Check if the device is currently idle */
        if (cem102_spi_status != SPI_TRANSFER_IDLE)
        {
            /* Check if data was lost */
            if ((SPI[p_cfg->spi].STATUS & SPI_OVERRUN_TRUE) ||
                (SPI[p_cfg->spi].STATUS & SPI_UNDERRUN_TRUE))
            {
                SPI[p_cfg->spi].CTRL = CEM102_SPI_CTRL;
                SPI[p_cfg->spi].STATUS = SPI_UNDERRUN_CLEAR | SPI_OVERRUN_CLEAR;
                cem102_spi_status = SPI_TRANSFER_IDLE;
                event = CEM102_EVENT_TX_FAIL;
            }
        }

        /* If an event happened and the application registered a callback */
        if (event != CEM102_EVENT_IDLE)
        {
            p_cfg->spi_callback(event);
        }
    }
}

void CEM102_RX_Handler(const CEM102_Device *p_cfg)
{
    if (p_cfg != NULL)
    {
        uint32_t event = CEM102_EVENT_IDLE;

        /* Check if the device is currently idle */
        if (cem102_spi_status != SPI_TRANSFER_IDLE)
        {
            /* Check if data was lost */
            if ((SPI[p_cfg->spi].STATUS & SPI_OVERRUN_TRUE) ||
                (SPI[p_cfg->spi].STATUS & SPI_UNDERRUN_TRUE))
            {
                SPI[p_cfg->spi].CTRL = CEM102_SPI_CTRL;
                SPI[p_cfg->spi].STATUS = SPI_UNDERRUN_CLEAR | SPI_OVERRUN_CLEAR;
                cem102_spi_status = SPI_TRANSFER_IDLE;
                event = CEM102_EVENT_TRANSFER_FAIL;
            }

            /* Check if DMA RX is complete */
            if ((DMA[p_cfg->dma[1]].STATUS & DMA_COMPLETE_INT_TRUE) != 0)
            {
                if ((cem102_spi_status == SPI_TRANSFER_ACTIVE) ||
                    (cem102_spi_status == SPI_TX_ACTIVE))
                {
                    /* Disable the SPI, DMA channel and set the transfer to idle. */
                    Sys_SPI_TransferConfig(&SPI[p_cfg->spi], CEM102_SPI_CTRL);
                    NVIC_DisableIRQ(DMA0_IRQn + p_cfg->dma[0]);
                    NVIC_ClearPendingIRQ(DMA0_IRQn + p_cfg->dma[0]);

                    DMA[p_cfg->dma[0]].STATUS = DMA_COMPLETE_INT_CLEAR;
                    DMA[p_cfg->dma[0]].CTRL |= DMA_CLEAR_BUFFER | DMA_CLEAR_CNTS;

                    NVIC_DisableIRQ(DMA0_IRQn + p_cfg->dma[1]);
                    NVIC_ClearPendingIRQ(DMA0_IRQn + p_cfg->dma[1]);

                    DMA[p_cfg->dma[1]].STATUS = DMA_COMPLETE_INT_CLEAR;
                    DMA[p_cfg->dma[1]].CTRL |= DMA_CLEAR_BUFFER | DMA_CLEAR_CNTS;

                    cem102_spi_status = SPI_TRANSFER_COMPLETE;
                    event = CEM102_EVENT_TRANSFER_COMPLETE;
                }
            }
        }

        /* If an event happened and the application registered a callback */
        if (event != CEM102_EVENT_IDLE)
        {
            p_cfg->spi_callback(event);
        }

        if (cem102_spi_status == SPI_TRANSFER_COMPLETE)
        {
            cem102_spi_status = SPI_TRANSFER_IDLE;
        }
    }
}

void CEM102_GPIO_Handler(const CEM102_Device *p_cfg)
{
    uint32_t event;
    if (cem102_spi_status == SPI_TRANSFER_RESET)
    {
        event = CEM102_EVENT_RESET;
    }
    else
    {
        event = CEM102_EVENT_IDLE;
    }
    if (p_cfg != NULL)
    {
        p_cfg->gpio_callback(event);
    }
}

/* Local definitions for number of relevant ECC bits and the length of the OTP data */
#define ECC_NUM_BITS                    10
#define ECC_DATA_LEN                    16
#define ECC_STRICT_PARITY_BIT           (1U << (ECC_NUM_BITS - CEM102_INDEX_ADJ))

/**
 * @brief       Check the OTP's ECC data is correct (correcting any one bit errors)
 * @param[in]   p_otp_data    Data read from the OTP to check that this function is
 *                            checking/correcting
 * @return      Returns ERRNO_NO_ERROR if the ECC parity bits were correct,
 *              ERRNO_ECC_CORRECTED if the ECC parity bits indicate one bit in error,
 *              ERRNO_GENERAL_FAILURE if the ECC detects more than one error.
 */
static int OTP_ECC_Check(uint16_t *p_otp_data)
{
    const uint16_t weights[ECC_NUM_BITS][ECC_DATA_LEN] =
            {{0xAD5B, 0x56AA, 0x5555, 0xAB55, 0xAAAA, 0xAAAA, 0xAAAA, 0x55AA, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0xD5},
             {0x366D, 0x9B33, 0x9999, 0xCD99, 0xCCCC, 0xCCCC, 0xCCCC, 0x66CC, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x66},
             {0xC78E, 0xE3C3, 0xE1E1, 0xF1E1, 0xF0F0, 0xF0F0, 0xF0F0, 0x78F0, 0x7878, 0x7878, 0x7878, 0x7878, 0x7878, 0x7878, 0x7878, 0x78},
             {0x7F0, 0x3FC, 0x1FE, 0x1FE, 0xFF, 0xFF, 0xFF, 0x80FF, 0x807F, 0x807F, 0x807F, 0x807F, 0x807F, 0x807F, 0x807F, 0x7F},
             {0xF800, 0x3FF, 0xFE00, 0x1FF, 0xFF00, 0xFF, 0xFF00, 0xFF, 0xFF80, 0x7F, 0xFF80, 0x7F, 0xFF80, 0x7F, 0xFF80, 0x7F},
             {0x0, 0xFC00, 0xFFFF, 0x1FF, 0x0, 0xFF00, 0xFFFF, 0xFF, 0x0, 0xFF80, 0xFFFF, 0x7F, 0x0, 0xFF80, 0xFFFF, 0x7F},
             {0x0, 0x0, 0x0, 0xFE00, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF, 0x0, 0x0, 0x0, 0xFF80, 0xFFFF, 0xFFFF, 0xFFFF, 0x7F},
             {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF00, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x7F},
             {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80},
             {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF}};

    uint16_t p[ECC_NUM_BITS] = {[0 ... (ECC_NUM_BITS - CEM102_INDEX_ADJ)] = 0};

    uint32_t parityBitNr;
    uint32_t wordNr;
    uint32_t i;
    uint16_t temp;
    int ret_val = ERRNO_NO_ERROR;

    if (p_otp_data != NULL)
    {
        for (parityBitNr = 0; parityBitNr < ECC_NUM_BITS; parityBitNr++)
        {
            /* Exclude the existing parity bits from the OTP read */
            for (wordNr = 0; wordNr < (ECC_DATA_LEN - CEM102_INDEX_ADJ); wordNr++)
            {
                /* Add the weighted data for each input word */
                p[parityBitNr] = p[parityBitNr] ^ (p_otp_data[wordNr] &
                                 weights[parityBitNr][wordNr]);
            }

            /* Calculate the parity of the remaining 16 bit word (1-bit value) */
            temp = CEM102_INITIAL_VALUE;
            for (i = 0; i < ECC_DATA_LEN; i++)
            {
                temp = temp ^ (p[parityBitNr] / (1U << i));
            }
            p[parityBitNr] = temp % 2;
        }

        /* For the extra parity bit, add all of the other parity bits */
        for (parityBitNr = 0; parityBitNr < (ECC_NUM_BITS - CEM102_INDEX_ADJ); parityBitNr++)
        {
            p[ECC_NUM_BITS - CEM102_INDEX_ADJ] = p[ECC_NUM_BITS - CEM102_INDEX_ADJ] + p[parityBitNr];
        }
        p[ECC_NUM_BITS - CEM102_INDEX_ADJ] = p[ECC_NUM_BITS - CEM102_INDEX_ADJ] % 2;


        /* Collect the ECC parity bits into one word */
        temp = CEM102_INITIAL_VALUE;
        for (parityBitNr = 0; parityBitNr < ECC_NUM_BITS; parityBitNr++)
        {
            temp = temp + (p[parityBitNr] * (1U << parityBitNr));
        }

        /*
         * Check the parity bits; since we've XORed with the previous parity bits
         * to this calculation:
         *  - If all the bits are cleared, there are no errors detected.
         *  - If there is an even number of parity bits set, the overall parity
         *    indicates an even number of errors (which cannot be corrected)
         *  - If there is an odd number of parity bits set, we can use the
         *    other parity bits to identify which bit to correct.
         */
        temp = temp ^ (p_otp_data[ECC_DATA_LEN - CEM102_INDEX_ADJ] >>
                       (ECC_DATA_LEN - ECC_NUM_BITS));
        if (temp == 0)
        {
            ret_val = ERRNO_NO_ERROR;
        }
        else
        {
            parityBitNr = CEM102_INITIAL_VALUE;
            for (i = 0; i < ECC_NUM_BITS; i++)
            {
                if ((temp & (1U << i)) != 0)
                {
                    parityBitNr++;
                }
            }

            if (parityBitNr % 2 == 0)
            {
                ret_val = ERRNO_GENERAL_FAILURE;
            }
            else
            {
                if (temp == ECC_STRICT_PARITY_BIT)
                {
                    /* Error was on the strict parity bit */
                    p_otp_data[ECC_DATA_LEN - CEM102_INDEX_ADJ] ^= ECC_STRICT_PARITY_BIT;
                }
                else
                {
                    /* Error was on any other bit */
                    temp &= (ECC_STRICT_PARITY_BIT - CEM102_INDEX_ADJ);

                    /* Figure out which data bit needs to be corrected
                     * by removing the parity bits from the count */
                    uint32_t shift = ECC_NUM_BITS - 2;
                    while (shift <= (ECC_NUM_BITS - 2))
                    {
                        if (temp > (1U << shift)) temp--;
                        shift--;
                    }

                    /* Convert to a zero indexed bit number */
                    temp--;

                    /* Flip the specified bit */
                    p_otp_data[temp / ECC_DATA_LEN] ^= 1U << (temp % ECC_DATA_LEN);
                }
                ret_val = ERRNO_ECC_CORRECTED;
            }
        }
    }
    return ret_val;
}

uint16_t CEM102_OTP_GetVersion(void)
{
    uint16_t otp_ver;

    if (trim_struct_v1 != NULL)
    {
        otp_ver = trim_struct_v1->device_info[1];
    }
    else if (trim_struct != NULL)
    {
        otp_ver = trim_struct->device_info[1];
    }
    else
    {
        otp_ver = CEM102_INITIAL_VALUE;
    }
    otp_ver = (otp_ver & CEM102_OTP_VERSION_MASK) >>
               CEM102_OTP_VERSION_POS;
    return otp_ver;
}

int CEM102_OTP_Read(const CEM102_Device *p_cfg,
                    CEM102_TrimStruct *p_trim_struct,
                    uint32_t cem102_sysclk_freq, uint32_t timeout)
{
    int result = ERRNO_NO_ERROR;

    if ((timeout > CEM102_MAX_OTP_READ_ATTEMPTS) || (p_cfg == NULL) ||
        (p_trim_struct == NULL))
    {
        result = ERRNO_PARAM_ERROR;
    }
    else
    {
        /* Configure to read OTP */
        result = CEM102_Register_Write(p_cfg, SYSCTRL_CMD, BUFFER_RESET_CMD);
    }

    if (result == ERRNO_NO_ERROR)
    {
        if (cem102_sysclk_freq > 64000)
        {
            result = CEM102_Register_Write(p_cfg, SYSCTRL_OTP_CFG,
                                            (OTP_PWR_ENABLE | OTP_OSC_DISABLE |
                                            OTP_READ_CONSTANT));
        }
        else
        {
            result = CEM102_Register_Write(p_cfg, SYSCTRL_OTP_CFG,
                                            (OTP_PWR_ENABLE | OTP_OSC_DISABLE |
                                            OTP_READ_SYS_CLK));
        }
    }

    if (result == ERRNO_NO_ERROR)
    {
        result = CEM102_Register_Write(p_cfg, SYSCTRL_CMD, OTP_READ_CMD);
    }

    uint16_t read_data = CEM102_INITIAL_VALUE;
    if (result == ERRNO_NO_ERROR)
    {
        /* Wait until the State Machine is not in OTP Read State */
        result = CEM102_Register_Read(p_cfg, SYS_STATUS, &read_data);
    }

    int32_t timeout_temp = (int32_t)timeout;
    while ((result == ERRNO_NO_ERROR) && ((read_data &
           SYS_STATUS_SYS_STATE_MASK) != OTP_READ_STATE) &&
           (timeout_temp > 0))
    {
        read_data = CEM102_INITIAL_VALUE;
        result = CEM102_Register_Read(p_cfg, SYS_STATUS, &read_data);

        if ((--timeout_temp) <= 0)
        {
            result = ERRNO_TIMEOUT;
            break;
        }
    }

    /* Wait until the OTP Controller finishes it's execution */
    if (result == ERRNO_NO_ERROR)
    {
        read_data = CEM102_INITIAL_VALUE;
        result = CEM102_Register_Read(p_cfg, SYS_STATUS, &read_data);
    }

    timeout_temp = (int32_t) timeout;
    while ((result == ERRNO_NO_ERROR) && ((read_data & (1U <<
           SYS_STATUS_OTP_STATUS_POS)) != OTP_LOAD) &&
           (timeout_temp > 0))
    {
        read_data = CEM102_INITIAL_VALUE;
        result = CEM102_Register_Read(p_cfg, SYS_STATUS, &read_data);

        if ((--timeout_temp) <= 0)
        {
            result = ERRNO_TIMEOUT;
            break;
        }
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Read data from the Buffer Registers */
        uint32_t expected_size = sizeof(uint16_t) * ((SYS_BUFFER_WORD15 -
                                        SYS_BUFFER_WORD0) + CEM102_INDEX_ADJ);
        if (sizeof(*p_trim_struct) >= expected_size)
        {
            uint16_t *buffer_data_p = (uint16_t *)p_trim_struct;
            for (uint16_t addr = SYS_BUFFER_WORD0; addr <= SYS_BUFFER_WORD15; addr++)
            {
                result = CEM102_Register_Read(p_cfg, addr, buffer_data_p++);
            }
        }
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Clear OTP Status Flag */
        result = CEM102_Register_Write(p_cfg, SYSCTRL_CMD, OTP_STATUS_CLR_CMD);
    }

    if (result == ERRNO_NO_ERROR)
    {
       /* Disable OTP */
       result = CEM102_Register_Write(p_cfg, SYSCTRL_OTP_CFG,
                                       (OTP_PWR_DISABLE | OTP_OSC_DISABLE));
    }

    /* Check the ECC, correcting any one bit errors encountered */
    if (result == ERRNO_NO_ERROR)
    {
        result = OTP_ECC_Check((uint16_t *)p_trim_struct);

        /* Set internal trim structure pointer equal to the external trim structure
         * passed in. These OTP values are used elsewhere in the driver.
         * Base structure type based on device_info. OTP rev starts at bit 10,
         * so 0x2 << 10 == 2000 */

        if ((result == ERRNO_NO_ERROR) || (result == ERRNO_ECC_CORRECTED))
        {
            if ((p_trim_struct->device_info[1] & 0xFC00) < 2000)
            {
                trim_struct_v1 = (CEM102_TrimStruct_v1 *)p_trim_struct;

                /* Gain information for channel 2 is not populated to save
                 * testing time for OTP-v1. Copying the gain information to
                 * channel 2 from channel 1, while maintaining the encoding
                 * of data for channel 2, as given in OTP V1 specification.
                 */
                trim_struct_v1->adc[2] = ((trim_struct_v1->adc[2] & 0xFF) |
                                    ((trim_struct_v1->adc[0] & 0xFF0) << 4));
                trim_struct_v1->adc[3] = ((trim_struct_v1->adc[3] & 0xFFF0) |
                                    ((trim_struct_v1->adc[0] & 0xF000) >> 12));
                trim_struct = NULL;
            }
            else
            {
                trim_struct = p_trim_struct;
                trim_struct_v1 = NULL;
            }
        }
    }
    return result;
}



/****************************************************************************************************************************************************
  * 	MAX30132 ??? ?????? ??? ????..24.10.25 sodykim
  ****************************************************************************************************************************************************/
int max30123_spidrv_read_buffer(const CEM102_Device *p_cfg, uint8_t addr, unsigned int len)
{
    if (cem102_spi_status != SPI_TRANSFER_IDLE)    {
        return ERRNO_INTERFACE_BUSY;
    }

    if (len > CEM102_SPI_MAX_DATA_SZ)    {
        return ERRNO_PARAM_ERROR;
    }

    spi_tx_buffer[0] = addr;
    spi_tx_buffer[1] = 0x80;

    spi_rx_len = len;

    /* Write the command and read the 8-bit value */
    return CEM102_Transfer(p_cfg, spi_tx_buffer, spi_rx_buffer, 2 + spi_rx_len);
}


int max30123_spidrv_read_action(const CEM102_Device *p_cfg, uint8_t *rbuffer, unsigned int rlen)
{
    if (!(cem102_spi_status == SPI_TRANSFER_IDLE) && !(cem102_spi_status == SPI_TRANSFER_COMPLETE))
    {
        return ERRNO_INTERFACE_BUSY;
    }

    if (rlen > CEM102_SPI_MAX_DATA_SZ)
    {
        return ERRNO_PARAM_ERROR;
    }

#if 1
    	memcpy(rbuffer, &spi_rx_buffer[2], rlen);

#else
    /* Copy the data read out of the register buffer */
    for (unsigned int i = 0; i < rlen; i++)    {
        data[i] = spi_rx_buffer[i  + 2];
    }
#endif
    return ERRNO_NO_ERROR;
}

int max30123_read_reg_action(const CEM102_Device *p_cfg, uint8_t addr,	uint8_t *rbuffer, unsigned int rlen)
{
    int result = ERRNO_PARAM_ERROR;

    if (rlen <= CEM102_SPI_MAX_DATA_SZ)
    {
    	result = max30123_spidrv_read_buffer(p_cfg, addr, rlen);

        if (ERRNO_NO_ERROR == result)        {
            result = CEM102_Wait_SPI_Idle(CEM102_SPI_TIMEOUT_LOOP,    CEM102_SYS_DELAY_100US);
        }

        if (ERRNO_NO_ERROR == result)        {
            result = max30123_spidrv_read_action(p_cfg, rbuffer, rlen);
        }
    }
    return result;
}

/**************************************************************************************************************************************************
  *		max30123 spi drive write
  **************************************************************************************************************************************************/
int max30123_spidrv_write_buffer(const CEM102_Device *p_cfg, uint8_t addr, unsigned int wlen)
{
    if (cem102_spi_status != SPI_TRANSFER_IDLE)    {
        return ERRNO_INTERFACE_BUSY;
    }

    if (wlen > CEM102_SPI_MAX_DATA_SZ)    {
        return ERRNO_PARAM_ERROR;
    }

    spi_tx_buffer[0] = addr;
    spi_tx_buffer[1] = 0x00;
//    spi_tx_buffer[1] = (CEM102_CMD_WRITE >> CEM102_CMD_Pos) + (len - 1);

    /* Write the register */
    return CEM102_Transfer(p_cfg, spi_tx_buffer, NULL, wlen  + 2);
}

int max30123_spidrv_write_action(const CEM102_Device *p_cfg, uint8_t addr, uint8_t *wbuffer, unsigned int wlen)
{
    if (cem102_spi_status != SPI_TRANSFER_IDLE)    {
        return ERRNO_INTERFACE_BUSY;
    }

    if (wlen > CEM102_SPI_MAX_DATA_SZ)    {
        return ERRNO_PARAM_ERROR;
    }

    memcpy(&spi_tx_buffer[2], wbuffer, wlen);

#if 0
    for (unsigned int i = 0; i < len; i++)    {
        spi_tx_buffer[i * 2 + 2] = (wbuffer[i] >> 8) & 0xFF;
        spi_tx_buffer[i * 2 + 3] = wbuffer[i] & 0xFF;
    }
#endif

    /* Write the registers */
    return max30123_spidrv_write_buffer(p_cfg, addr, wlen);
}

int max30123_write_reg_action(const CEM102_Device *p_cfg, uint8_t addr,   uint8_t *wbuffer, unsigned int wlen)
{
    int result = max30123_spidrv_write_action(p_cfg, addr, wbuffer, wlen);

    if (result == ERRNO_NO_ERROR)    {
        result = CEM102_Wait_SPI_Idle(CEM102_SPI_TIMEOUT_LOOP,
                                      CEM102_SYS_DELAY_100US);
    }

    return result;
}



/* CEM102 Driver Control Block */
DRIVER_CEM102_t Driver_CEM102 =
{
    CEM102_Initialize,
    CEM102_Register_QueueWrite,
    CEM102_Register_BufferWrite,
    CEM102_Register_Write,
    CEM102_Register_VerifiedWrite,
    CEM102_Register_QueueRead,
    CEM102_Register_BufferRead,
    CEM102_Register_Read,
    CEM102_TX_Handler,
    CEM102_RX_Handler,
    CEM102_GPIO_Handler,
    CEM102_Get_LSAD_Config,
    CEM102_OTP_Read,
    CEM102_Power_Reset,
    CEM102_Power_Init,
    CEM102_Power_VDDADischarge,
    CEM102_Power_DeviceTrim,
    CEM102_Power_VDDCCharge,
    CEM102_SAR_Init,
    CEM102_SAR_StartConversion,
    CEM102_VBAT_OK,
    CEM102_VCC_OK,
    CEM102_VOLTAGE_OK,
    CEM102_VBAT_VDDA_MonitorStart,
    CEM102_Calibrate_VCC,
    CEM102_Calculate_VBAT,
    CEM102_System_IRQConfig,
    CEM102_System_Command,
    CEM102_System_Status_Queue,
    CEM102_System_Status_Process,
    CEM102_Signal_ClockConfig,
    CEM102_Signal_TIAConfig,
    CEM102_Signal_SwitchConfig,
    CEM102_Signal_RE_ATBus_Config,
    CEM102_Signal_BufferConfig,
    CEM102_Signal_WEDACConfig,
    CEM102_Signal_REDACConfig,
    CEM102_Signal_ADCConfig,
    CEM102_Signal_DirectADCConfig,
    CEM102_Signal_StartMeasurement,
    CEM102_Signal_ProcessADCSingle,
    CEM102_Signal_ProcessADCDouble,
    CEM102_Signal_MeasureReference,
    CEM102_Signal_CalibrateChannel,
    CEM102_Signal_CalibrateDACVoltage,
    CEM102_Signal_CalculateChannelGain,
    CEM102_Signal_CalculateADCGain,
    CEM102_Signal_CalculateCurrent,
    CEM102_Signal_CalculateVoltage,
    CEM102_Signal_CalculateDACOffset,
    CEM102_Signal_GetADCGain,
    CEM102_Signal_CalculateDACGain,
    CEM102_DAC_Calibrate,
    CEM102_DAC_CalibrateWithADC,
    CEM102_Register_Multiple_Read,
    CEM102_Register_Multiple_Write,
    CEM102_Signal_GetCalibratedCurrent,
    CEM102_Register_ReadModifyWrite,
    CEM102_Signal_AIOConfigInit,
    CEM102_Signal_AIOConfigDeinit,
    CEM102_Signal_MeasureCurrent,
    CEM102_Signal_MeasureVoltage,
    CEM102_Notify_Event,
    CEM102_Register_Save,
    CEM102_Register_Restore,
    CEM102_DAC_ADC_Setup,
    CEM102_Calibration_Erase,
    CEM102_Calibration_Save,
    CEM102_Calibration_Valid,
    CEM102_VCC_Calibration_Valid,
	max30123_read_reg_action,
	max30123_write_reg_action
};

