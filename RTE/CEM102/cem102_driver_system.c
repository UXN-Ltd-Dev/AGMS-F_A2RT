/**
 * @file cem102_driver_system.c
 * @brief CEM102 Driver System Support implementation
 *
 * @copyright @parblock
 * Copyright (c) 2022 Semiconductor Components Industries, LLC (d/b/a
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
#include "flash_rom.h"

/* ----------------------------------------------------------------------------
 * Support Configuration Functions
 * --------------------------------------------------------------------------*/

int CEM102_System_IRQConfig(const CEM102_Device *p_cfg, uint16_t reg_val)
{
    int result = ERRNO_GENERAL_FAILURE;

    if (p_cfg->irq >= 0)
    {
        result = CEM102_Register_Write(p_cfg, SYSCTRL_IRQ_CFG, reg_val);
    }
    return result;
}

int CEM102_System_Command(const CEM102_Device *p_cfg, uint16_t cmd)
{

    return CEM102_Register_Write(p_cfg, SYSCTRL_CMD, cmd);

}

int CEM102_System_Status_Queue(const CEM102_Device *p_cfg)
{
    int result = ERRNO_NO_ERROR;

    if (cem102_spi_status != SPI_TRANSFER_IDLE)
    {
        result = ERRNO_INTERFACE_BUSY;
    }
    else
    {
        result = CEM102_Register_QueueRead(p_cfg, SYS_BUFFER_VIOL_CNT, 3);
    }

    return result;
}

int CEM102_Calibration_Erase(void)
{
    int result = ERRNO_NO_ERROR;

    /* Erase the calibration data sector */
    if (Flash_EraseSector(CEM102_PERS_MEM_ADDR, 0) != FLASH_ERR_NONE)
    {
        result = ERRORNO_PERS_ERASE_ERROR;
    }

    return result;
}

int CEM102_Calibration_Valid(void)
{
    int ret = 0; /* False */
    unsigned int sig = CEM102_PersData->sig_ver & CEM102_CALIB_SIG_MASK;
    unsigned int ver = CEM102_PersData->sig_ver & CEM102_CALIB_VERS_MASK;

    if ((sig == CEM102_CALIB_SIG) && (ver == CEM102_CALIB_VERSION) &&
        (CEM102_PersData->_data_end_ == 0))
    {
        ret = 1; /* True */
    }

    return ret;
}

int CEM102_Calibration_Save(CEM102_CalibrationData *p_data)
{
    int result = ERRNO_NO_ERROR;

    /* Prevent calibration data structure size from growing above the
     * persistent memory area minus header */
    if (CEM102_PERS_MAX_DATA_SZ < CEM102_CALIB_DATA_SZ)
    {
        result = ERRNO_GENERAL_FAILURE;
    }

    if (result == ERRNO_NO_ERROR)
    {
        /* Erase count would be set to 1, if structure integrity check fails.
         * It may be first time, the calibration data is being written.
         *
         * This is the first version. Version handling is not needed.
         */
        int count = 0;
        if (CEM102_Calibration_Valid())
        {
            count = CEM102_PersData->erase_count;
        }

        CEM102_PersistentMemory PersData;

        /* Make sure all the header fields are initialized here */
        PersData._data_end_ = 0;
        PersData.erase_count = count + 1;
        PersData.sig_ver = CEM102_CALIB_SIG | CEM102_CALIB_VERSION;

        /* Copy the incoming calibration data */
        memcpy(&PersData.data, p_data, CEM102_CALIB_DATA_SZ);

        Flash_EraseSector(CEM102_PERS_MEM_ADDR, 0);
        Flash_WriteBuffer(CEM102_PERS_MEM_ADDR,
                          CEM102_PERS_DATA_SZ/sizeof(uint32_t),
                          (uint32_t *)&PersData, 0);

        /* Check if data is written correctly */
        int differ = memcmp((const void *)CEM102_CALIB_DATA_ADDR,
                        (char *)p_data, CEM102_CALIB_DATA_SZ);
        if (differ)
        {
            result = ERRNO_PERS_WRITE_ERROR;
        }
    }
    return result;
}

bool CEM102_VCC_Calibration_Valid(void)
{
    bool result = true;

    unsigned int ver = CEM102_PersData->sig_ver & CEM102_CALIB_VERS_MASK;

    if ((CEM102_Calibration_Valid()  == 0) || (ver <= CEM102_CALIB_VERSION_1_4))
    {
        result = false;
    }

    return result;
}

int CEM102_System_Status_Process(uint16_t *data, uint16_t *p_violation_cnt)
{
    uint16_t Sys_Buffer_Viol_Cnt = data[0];
    uint16_t Sys_Buffer_Viol_Status __attribute__((unused)) = data[1];
    uint16_t Sys_Status = data[2];
    *p_violation_cnt = 0;
    uint16_t result = 0;

    if (Sys_Status & DIGITAL_RESET)
    {
        result = DIGITAL_RESET;
    }
    else if (Sys_Status & VDDA_ERR)
    {
        result = VDDA_ERR;
    }
    else if (Sys_Status & SPI_ERR)
    {
        result = SPI_ERR;
    }
    else if (Sys_Status & STATE_ERR)
    {
        result = STATE_ERR;
    }
    else if (Sys_Status & CH1_VIOLATION)
    {
        *p_violation_cnt = (Sys_Buffer_Viol_Cnt &
                            SYS_BUFFER_VIOL_CNT_CH1_VIOLATION_CNT_MASK);

        result = CH1_VIOLATION;
    }
    else if (Sys_Status & CH2_VIOLATION)
    {
        *p_violation_cnt = (Sys_Buffer_Viol_Cnt &
                            SYS_BUFFER_VIOL_CNT_CH2_VIOLATION_CNT_MASK)
                            >> SYS_BUFFER_VIOL_CNT_CH2_VIOLATION_CNT_POS;

        result = CH2_VIOLATION;
    }
    else
    {
        result = (Sys_Status & SYS_STATUS_SYS_STATE_MASK);
    }

    return result;
}

