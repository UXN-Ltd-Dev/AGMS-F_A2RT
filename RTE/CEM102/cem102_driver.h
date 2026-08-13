/**
 * @file cem102_driver.h
 * @brief CEM102 Driver implementation
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

#ifndef CEM102_DRIVER_H
#define CEM102_DRIVER_H

#ifdef  __cplusplus
extern "C"
{
#endif

/* ----------------------------------------------------------------------------
 * Supporting include files
 * --------------------------------------------------------------------------*/

/* External include files supporting embedded software components on the host
 * system, including the SPI and GPIO hardware abstractions. */
#include <hw.h>

/* Include files supporting CEM102 */
#include "cem102_hw.h"
#include "Driver_CEM102.h"

/**
 * @defgroup CEM102_DRIVERg CEM102 Driver Reference
 * @brief CEM102 Driver Reference
 *
 * This reference chapter presents a detailed description of all the functions
 * supporting the CEM102 device driver. This reference includes calling
 * parameters, returned values, and assumptions.
 * @{
 */

/* ----------------------------------------------------------------------------
 * Definitions
 * --------------------------------------------------------------------------*/
#ifndef ERRNO_NO_ERROR
    #define ERRNO_NO_ERROR              (0x0000)      /**< No error */
#endif /* ERRNO_NO_ERROR */

#ifndef ERRNO_GENERAL_FAILURE
    #define ERRNO_GENERAL_FAILURE       (0x0001)      /**< General error */
#endif /* ERRNO_GENERAL_FAILURE */
#define ERRNO_PARAM_ERROR               (0x0002)      /**< Parameter error */
#define ERRNO_INTERFACE_BUSY            (0x0003)      /**< Interface is busy error */
#define ERRNO_TIMEOUT                   (0x0004)      /**< Timeout error */
#define ERRNO_MEASUREMENT_BUSY          (0x0005)      /**< Measurement is busy error */
#define ERRNO_STATE_ERROR               (0x0006)      /**< Invalid state error */
#define ERRNO_ECC_CORRECTED             (0x0007)      /**< ECC corrected warning */
#define ERRNO_VBAT_LOW                  (0x0008)      /**< VBAT low warning */
#define ERRNO_CALIBRATION_ERROR         (0x0009)      /**< Error in DAC calibration */
#define ERRNO_PERS_WRITE_ERROR          (0x000A)      /**< Error in writing to persistent memory */
#define ERRORNO_PERS_ERASE_ERROR        (0x000B)      /**< Error in erasing persistent memory */
#define ERRNO_VCC_LOW_ERROR             (0x000C)      /**< VCC low error */
#define ERRNO_NOT_IMPLEMENTED           (0x000D)      /**< Feature disabled */
#define ERRNO_DAC_RANGE_INVALID         (0x000E)      /**< Voltage range not recommended */

/* Call back events */
#define CEM102_EVENT_IDLE               (0x0001)      /**< Event state machine is idle */
#define CEM102_EVENT_TX_COMPLETE        (0x0002)      /**< Event is TX complete */
#define CEM102_EVENT_TRANSFER_COMPLETE  (0x0003)      /**< Event is transfer complete */
#define CEM102_EVENT_TX_FAIL            (0x0004)      /**< Event is a TX failure */
#define CEM102_EVENT_TRANSFER_FAIL      (0x0005)      /**< Event is a transfer failure */
#define CEM102_EVENT_RESET              (0x0006)      /**< Event is a reset */

/** CEM102 SPI command structures */
#define CEM102_ADDR_Pos                 (24)          /**< Offset of the CEM102 address in transfers */
#define CEM102_CMD_Pos                  (16)          /**< Offset of the CEM102 command in transfers */

#define CEM102_CMD_WRITE                (0xC0 << CEM102_CMD_Pos)    /**< CEM102 write command */
#define CEM102_CMD_READ                 (0x80 << CEM102_CMD_Pos)    /**< CEM102 read command */

#define CEM102_EXTENDED_TRANSFER_Msk    (0x3F << CEM102_CMD_Pos)    /**< CEM102 command mask for extended transfers */

#define CEM102_DATA_Pos                 (0)           /**< Offset of the CEM102 data in transfers */

/** Define a maximum transfer length (in bytes): the maximum transfer includes
 *  the address, command, and 64 16-bit register reads or writes.
 */
#define CEM102_MAX_TRANSFER_LENGTH      (130)

#define CEM102_OTP_VERSION_POS          (10)
#define CEM102_OTP_VERSION_5            (5)
#define CEM102_OTP_VERSION_3            (3)
#define CEM102_OTP_VERSION_1            (1)
#define CEM102_OTP_VERSION_MASK         (0xFC00)

/* Wait time of 50 msec delay when ADC is configured */
#define CEM102_SYS_DELAY_50MS           (SystemCoreClock / 20)
#define CEM102_SYS_DELAY_20MS           (SystemCoreClock / 50)
#define CEM102_SYS_DELAY_100US          (SystemCoreClock / 10000)
#define CEM102_SYS_DELAY_20US           (SystemCoreClock / 50000)

#define CEM102_VCC_OK_TIMEOUT           (120)
#define CEM102_DIG_RESET_TIMEOUT        (4000)
#define CEM102_VDDA_DIG_RESET_TIMEOUT   (4000)

/* Multiplication factor to convert mV to uV */
#define CEM102_VOLTAGE_MV_TO_UV         (1000U)
#define CEM102_LSAD_START_DELAY_6MS     (3)

/* Number of sample accumulation configuration of 2 */
#define CEM102_ACCUM_SAMPLE_CNT_2       (1)

/* Space for two register reads per channel, and two channels, WE1 and WE2. */
#define CEM102_MEASUREMENT_LENGTH       (2 * 2)

/** CEM102 SPI configuration */
#define CEM102_SPI_CFG                  (SPI_TX_DMA_ENABLE          |\
                                         SPI_RX_DMA_ENABLE          |\
                                         SPI_OVERRUN_INT_DISABLE    |\
                                         SPI_UNDERRUN_INT_DISABLE   |\
                                         SPI_WORD_SIZE_8            |\
                                         SPI_MODE_SPI               |\
                                         SPI_CLK_PHASE_RISING       |\
                                         SPI_CLK_POLARITY_NORMAL)

/** CEM102 SPI control configuration */
#define CEM102_SPI_CTRL                 (SPI_CS_1 | SPI_MODE_NOP | SPI_DISABLE)

/** CEM102 DMA TX configuration */
#define CEM102_DMA_TX_CFG               (DMA_LITTLE_ENDIAN          |\
                                         SRC_TRANS_LENGTH_SEL       |\
                                         DMA_PRIORITY_0             |\
                                         DMA_SRC_ALWAYS_ON          |\
                                         (DMA_DEST_SPI0 + (p_cfg->spi << DMA_CFG0_DEST_SELECT_Pos)) |\
                                         WORD_SIZE_8BITS_TO_8BITS   |\
                                         DMA_SRC_ADDR_INCR_1        |\
                                         DMA_DEST_ADDR_STATIC       |\
                                         DMA_COMPLETE_INT_ENABLE)

/** CEM102 DMA RX configuration */
#define CEM102_DMA_RX_CFG               (DMA_LITTLE_ENDIAN          |\
                                         DEST_TRANS_LENGTH_SEL      |\
                                         DMA_PRIORITY_0             |\
                                         (DMA_SRC_SPI0 + (p_cfg->spi << DMA_CFG0_SRC_SELECT_Pos)) |\
                                         DMA_DEST_ALWAYS_ON         |\
                                         WORD_SIZE_8BITS_TO_8BITS   |\
                                         DMA_SRC_ADDR_STATIC        |\
                                         DMA_DEST_ADDR_INCR_1       |\
                                         DMA_COMPLETE_INT_ENABLE)

#define CEM102_DAC_MIN                  (614)     /**< Minimum DAC output of 0.344 V */
#define CEM102_DAC_MAX                  (3393)    /**< Maximum DAC output of 1.9 V */

/** Mask for TIA configuration */
#define CEM102_TIA_MASK                 ((1U << ANA_CFG0_CH1_TIA_HP_MODE_CFG_POS) | \
                                         (1U << ANA_CFG0_CH1_TIA_INT_FB_CFG_POS)  | \
                                         ANA_CFG0_CH1_TIA_FB_RES_MASK)

/** Mask for switch configuration */
#define CEM102_SW_CFG_MASK              (0x3FF)

/** Mask for buffer configuration */
#define CEM102_SW_CFG3_BUF_MASK         ((ANA_SW_CFG3_ATBUS_BUF_TRIM_MASK)         | \
                                         (1U << ANA_SW_CFG3_ATBUS_BUF_CHOP_CLK_POS) | \
                                         (1U << ANA_SW_CFG3_ATBUS_BUF_CFG_POS))

/* Meaningful names for the switch connect/disconnect bits. */
#define CEM102_ANALOG_IO_ATBUS_CONNECT  (ATBUS_ST22_CONNECT | ATBUS_ST12_CONNECT)
#define CEM102_ANALOG_IO1_VSSA_CONNECT  (ATBUS_ST14_CONNECT)
#define CEM102_ANALOG_IO2_VSSA_CONNECT  (ATBUS_ST16_CONNECT)
#define CEM102_ANALOG_IO1_VDDA_CONNECT  (ATBUS_ST15_CONNECT)

#define CEM102_CH1_LPF_CUTOFF_500HZ     (CH1_LPF_BW_CFG_DISABLE)
#define CEM102_CH1_LPF_CUTOFF_10KHZ     (CH1_LPF_BW_CFG_ENABLE)
#define CEM102_CH2_LPF_CUTOFF_500HZ     (CH2_LPF_BW_CFG_DISABLE)
#define CEM102_CH2_LPF_CUTOFF_10KHZ     (CH2_LPF_BW_CFG_ENABLE)

#define CEM102_ST_SWITCH_RESISTANCE     (100) /* in Ohms */

/** Total number of LSAD input channels in hardware. Fixed do not modify */
#define CEM102_LSAD_CHANNEL_NUM         (8U)

/** VBAT LSAD channel */
#define CEM102_VBAT_CHANNEL             (LSAD_INPUT_CH0)

/** VCC LSAD calibration channel */
#define CEM102_VCC_CHANNEL              (1)

/** RSL15 VCC mV/trim code */
#define VCC_MV_PER_LSB                  (10)

/** RSL15 VCC 50mV offset */
#define VCC_50MV_OFFSET                 (50 / VCC_MV_PER_LSB)

/** RSL15 VCC target during CEM102_Power_Reset function in mV
 *  VCC trim value is set to 1300 mV without calibration.
 *  This value is lower than 1300 mV to allow some margin. */
#define VCC_PWR_RESET_TARGET            (1275)

/** RSL15 VCC trim setting for earlier parts */
#define TARGET_VCC_1350                 (135)

/** Number of LSAD Samples to average */
#define CEM102_LSAD_NSAMPLES            (5)

/** Number of LSAD Samples to discard */
#define CEM102_LSAD_DSAMPLES            (3)

/* Settings to enable threshold violation detection during
 * calibration or measurement or during both phases.
 */
#define CEM102_THRESH_VIOL_DISABLED     (0)
#define CEM102_THRESH_VIOL_ENABLED      (1)

/* Number of bits padded for LSAD threshold. */
#define CEM102_LSAD_THRESHOLD_PADDING   (5)

/* Voltage rail to measure */
#define MEASURE_VBAT                    (0)       /**< Measure VBAT */
#define MEASURE_VCC                     (1)       /**< Measure VCC */

/* SAR measurement configuration defines */
#define CEM102_SAR_MAX_VALUE            (8191)    /**< Maximum Signed Digital Value of SAR */
#define CEM102_SAR_SUPPLY               (2500)    /**< SAR Positive Voltage Supply */
#define CEM102_VDDA_TYPICAL             (2300)    /**< Typical VDDA Voltage of CEM102 */
#define CEM102_SAR_SIGNED_BIT_MASK      (0x4000)  /**< Mask for Signed bit in SAR DATA */
#define CEM102_SAR_DATA_MASK            (0x3FF)   /**< Mask for Value in SAR DATA */

/* Device supply configuration defines */
#define HIGH_VOLTAGE                    (1)       /**< Indicate that VBAT Voltage is >2.5V */
#define LOW_VOLTAGE                     (0)       /**< Indicate that VBAT Voltage is <=2.5V */

/** CEM102 ADC gain calculation, OTP gain factor */
#define CEM102_ADC_OTP_GAIN             (24897981217.0f)

/** CEM102 ADC gain calculation, OTP offset factor */
#define CEM102_ADC_OTP_OFFSET           (-185682.5f)

/** CEM102 ADC gain nominal setting (nominal value
 *  that would be read from OTP */
#define CEM102_ADC_OTP_NOM              (32768.0f)

/** CEM102 ADC nominal gain */
#define CEM102_ADC_GAIN_NOM             ((CEM102_ADC_OTP_NOM - CEM102_ADC_OTP_OFFSET) / CEM102_ADC_OTP_GAIN)

/** CEM102 ADC gain calculation */
#define CEM102_ADC_GAIN_CALC(x)         (((x) - CEM102_ADC_OTP_OFFSET) / CEM102_ADC_OTP_GAIN)

/**
 * @brief       Converts an ADC code to a voltage, calculated as follows
 *              voltage = adc_code * (2 V * 1000 [mV]/1 V / 2^14 steps.)
 * @param[in]     x     the ADC code input
 * @return      The voltage output in mV
 * @assumptions Low frequency mode for the ADC is used, meaning that the
 *              resolution of the ADC is 14-bits. CONVERT provides voltage
 *              level as a milliVolt value based on the input ADC code.
 * @examplecode driver_examples.c CEM102_CONVERT_Example
 */
#define CEM102_CONVERT(x)                ((uint32_t)((x * 1000) >> 13))

/** Macro for setting DAC voltage, in mV. Resolution is 0.56 mV/LSB */
#define CEM102_DAC_MV(x)                 (((178087 * (x)) / 100000) << \
                                          DC_DAC_WE1_CTRL_DAC_WE1_VALUE_POS)

/** Macro to convert nano amps to femto amps */
#define NANO_AMPS_TO_FEMTO_AMPS(x)       ((x) * 1000 * 1000)
#define CEM102_CALIB_CURRENT_NANO_AMPS   (100)
#define CEM102_CAL_CURR_DELTA_BEFORE_V5  (125000)       /**< 0.125 nA, in fA */
#define CEM102_CAL_CURR_DELTA_AFTER_V4   (250000)       /**< 0.25 nA, in fA */

/* Conversion factors */
#define CEM102_V_TO_MV_F                 (1000.0f)      /**< Float factor converting between mV and V. */
#define CEM102_UV_TO_MV                  (1000)         /**< Integer factor converting between uV and mV. */
#define CEM102_DAC_VOLTAGE_500MV         (500)

/* Channel calibration defines */
#define CEM102_NACC_OTP_R1               (4)            /**< Number of accumulations for the R1 calibration measurement */
#define CEM102_NACC_REF_R1               (182)          /**< Reference scalar for the R1 calibration measurement */
#define CEM102_NACC_OTP                  (2)            /**< Number of accumulations for the calibration measurement */
#define CEM102_NACC_REF                  (16)           /**< Reference scalar for the calibration measurement */
#define CEM102_GAIN_OFFSET               (19216)        /**< Gain offset */
#define CEM102_OTP_DAC_VOLTAGE           (1200)         /**< OTP reference DAC voltage */

#define CEM102_DAC_OFFSET_O2             (127.5f)       /**< Offset value for DAC offset calculation */
#define CEM102_DAC_OFFSET_G2             (12750.0f)     /**< Gain value for DAC offset calculation */
#define CEM102_DAC_GAIN_O1               (-6247.5f)     /**< Offset value for DAC gain calculation */
#define CEM102_DAC_GAIN_G1               (11350271.74f) /**< Gain value for DAC gain calculation */


#define CEM102_OTP_GAIN_THRESHOLD        (1750.0f)      /**< OTP Gain threshold to differentiate parts with double current */

#define VDDA_ERR_CLR_TIMEOUT             (4000)         /**< Timeout to wait until the VDDA_ERROR bit clears. */

/* Calibration data size defines */
#define CEM102_PERS_DATA_SZ         (sizeof(CEM102_PersistentMemory))
#define CEM102_CALIB_DATA_SZ        (sizeof(CEM102_CalibrationData))

/* NVR5 region is used to store calibration data */
#define CEM102_PERS_MEM_ADDR        (FLASH0_NVR5_BASE)
#define CEM102_PERS_MEM_SIZE        (FLASH0_NVR5_SIZE)
#define CEM102_PERS_DATA_HDR_SZ     (CEM102_PERS_DATA_SZ - \
                                     CEM102_CALIB_DATA_SZ)
#define CEM102_PERS_MAX_DATA_SZ     (FLASH0_NVR5_SIZE - CEM102_PERS_DATA_HDR_SZ)
#define CEM102_PersData             ((CEM102_PersistentMemory *)CEM102_PERS_MEM_ADDR)
#define CEM102_PERS_INVALID_DATA    (0xFFFFFFFF)

/* Increment this value, when CEM102_CalibrationData structure changes
 * New structure and extra handling would come, when that happens. */
#define CEM102_CALIB_VERSION        (2)
#define CEM102_CALIB_VERSION_1_4    (1)

#define CEM102_CALIB_DATA_ADDR      (&CEM102_PersData->data)

#define CEM102_CALIB_SIG_POS        (24)
#define CEM102_CALIB_SIG            (0xA5 << CEM102_CALIB_SIG_POS)
#define CEM102_CALIB_SIG_MASK       (0xFF << CEM102_CALIB_SIG_POS)

#define CEM102_CALIB_VERS_POS       (0)
#define CEM102_CALIB_VERS_MASK      (0xFF << CEM102_CALIB_VERS_POS)

#define CEM102_CALIB_RSVD_POS       (8)
#define CEM102_CALIB_RSVD_MASK      (0xFFFF << CEM102_CALIB_RSVD_POS)

/* BUF_IB0 is for PRESCALE3 and BUF_IB3 for PRESCALE0 */
#define CEM102_CH1_BIAS_PRESCALE1   (CH1_BUF_IB2)
#define CEM102_CH2_BIAS_PRESCALE1   (CH2_BUF_IB2)

#define CEM102_CH1_BIAS_PRESCALE2   (CH1_BUF_IB0)
#define CEM102_CH2_BIAS_PRESCALE2   (CH2_BUF_IB0)

#define CEM102_MAX_DACS             (3) /* WE1, WE2, and RE */

/* ----------------------------------------------------------------------------
 * Global Variables
 * --------------------------------------------------------------------------*/
/** SPI status flag */
extern volatile CEM102_SPI_FLAG cem102_spi_status;

/** Trim structure (default, verison 2 or higher) */
extern CEM102_TrimStruct * trim_struct;

/** Trim structure (version 1) */
extern CEM102_TrimStruct_v1 * trim_struct_v1;

/** Channel gain expressed in fA/LSB */
extern float channel_gain[2];

/** Threshold violation detection can be enabled during calibration,
 *  measurement or both. */
extern uint8_t cem102_threshold_violation;

/* ----------------------------------------------------------------------------
 * Initialization and Base Driver Function Prototypes
 * --------------------------------------------------------------------------*/

/**
 * @brief       Initialize the SPI, GPIO interfaces to communicate with the
 *              CEM102 device.
 * @param[in]   cfg     Pointer to the CEM102 device to be used; for proper system operation
 *                      this parameter must specify a pull resistor as part of
 *                      the gpio_cfg field (for optimal power consumption this
 *                      pull resistor needs to be set to a weak pull down)
 * @return      Returns ERRNO_NO_ERROR if the the interface is initialized
 *              successfully, ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Initialize_Example
 */
int CEM102_Initialize(const CEM102_Device *p_cfg);

/**
 * @brief       Queue a register write.
 * @param[in]   cfg     Pointer to the CEM102 device to be used.
 * @param[in]   addr    Address of the register to write.
 * @param[in]   len     Number of registers to write
 * @return      Returns ERRNO_INTERFACE_BUSY if the interface was busy,
 *              otherwise returns ERRNO_NO_ERROR.
 * @examplecode driver_examples.c CEM102_Register_QueueWrite_Example
 */
int CEM102_Register_QueueWrite(const CEM102_Device *p_cfg, uint8_t addr,
                               unsigned int len);

/**
 * @brief       Queue a set of register writes.
 * @param[in]   cfg     Pointer to the CEM102 device to be used.
 * @param[in]   addr    Address of the register to write.
 * @param[in]   data[]  Data to be written to the registers.
 * @param[in]   len     Number of registers to write (max 64)
 * @return      Returns ERRNO_NO_ERROR if no errors have occurred, the error
 *                      otherwise.
 * @examplecode driver_examples.c CEM102_Register_BufferWrite_Example
 */
int CEM102_Register_BufferWrite(const CEM102_Device *p_cfg, uint8_t addr,
                                uint16_t data[], unsigned int len);

/**
 * @brief       Write a register. Blocks with timeout, waiting for the write
 *              to complete.
 * @param[in]   cfg     Pointer to the CEM102 device to be used.
 * @param[in]   addr    Address of the register to write.
 * @param[in]   data    Data to be written to the register.
 * @return      Returns ERRNO_INTERFACE_BUSY if the interface was busy,
 *              returns ERRNO_TIMEOUT if write doesn't succeed within 500us,
 *              otherwise returns ERRNO_NO_ERROR.
 * @examplecode driver_examples.c CEM102_Register_Write_Example
 */
int CEM102_Register_Write(const CEM102_Device *p_cfg, uint8_t addr,
                          uint16_t data);

/**
 * @brief       Read multiple registers located sequentially in memory.
 *              Blocks with time out, waiting for the read to complete.
 * @param[in]   p_cfg   Pointer to the CEM102 device to be used.
 * @param[in]   addr    Address of the first register to read from.
 * @param[out]  p_data  Pointer to where the data read from the registers will
 *                      be stored.
 * @param[in]   len     Number of registers to read.
 * @return      Returns ERRNO_INTERFACE_BUSY if the SPI interface is busy,
 *              returns ERRNO_PARAM_ERROR if length is greater than 64,
 *              returns ERRNO_TIMEOUT if read doesn't succeed within 500us,
 *              otherwise returns ERRNO_NO_ERROR.
 */
int CEM102_Register_Multiple_Read(const CEM102_Device *p_cfg, uint8_t addr,
                                  uint16_t *p_data, unsigned int len);

/**
 * @brief       Write multiple register registers located sequentially in memory.
 *              Blocks with timeout, waiting for the write to complete.
 * @param[in]   p_cfg   Pointer to the CEM102 device to be used.
 * @param[in]   addr    Address of the first register to write from.
 * @param[in]   p_data  Pointer where data to be picked from.
 * @param[in]   len     Number of registers to write.
 * @return      Returns ERRNO_INTERFACE_BUSY if the SPI interface is busy,
 *              returns ERRNO_PARAM_ERROR if length is greater than 64,
 *              returns ERRNO_TIMEOUT if write doesn't succeed within 500us,
 *              otherwise returns ERRNO_NO_ERROR.
 */
int CEM102_Register_Multiple_Write(const CEM102_Device *p_cfg, uint8_t addr,
                                   uint16_t *p_data, unsigned int len);

/**
 * @brief       Write a register, and verify the written data is correct.
 * @param[in]   cfg     Pointer to the CEM102 device to be used.
 * @param[in]   addr    Address of the register to write.
 * @param[in]   data    Data to be written to the register.
 * @return      Returns ERRNO_NO_ERROR if the data written matches the data
 *                      read back, ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Register_VerifiedWrite_Example
 */
int CEM102_Register_VerifiedWrite(const CEM102_Device *p_cfg, uint8_t addr,
                                  uint16_t data);

/**
 * @brief       Queue a register read; results will be available when the
 *              CEM102 RX handler is triggered.
 * @param[in]   cfg     Pointer to the CEM102 device to be used.
 * @param[in]   addr    Address of the register to read.
 * @param[in]   len     Number of registers to read (max 64)
 * @return      Returns ERRNO_NO_ERROR if no errors have occurred, the error
 *                      otherwise.
 * @examplecode driver_examples.c CEM102_Register_QueueRead_Example
 */
int CEM102_Register_QueueRead(const CEM102_Device *p_cfg, uint8_t addr,
                              unsigned int len);

/**
 * @brief       Retrieve data that was previously read using a set of register
 *              reads.
 * @param[in]   cfg     Pointer to the CEM102 device to be used.
 * @param[out]  data[]  Pointer to where the data read from the SPI rx buffer
                        will be stored.
 * @param[in]   len     Number of registers to read (max 64)
 * @return      Returns ERRNO_NO_ERROR if no errors have occurred, the error
 *                      otherwise.
 * @examplecode driver_examples.c CEM102_Register_BufferRead_Example
 */
int CEM102_Register_BufferRead(const CEM102_Device *p_cfg, uint16_t data[],
                               unsigned int len);

/**
 * @brief       Read a register. Blocks with timeout, waiting for the read to
 *              complete.
 * @param[in]   cfg     Pointer to the CEM102 device to be used.
 * @param[in]   addr    Address of the register to read.
 * @param[out]  p_data  Pointer to where the data read from the register will
 *                      be stored.
 * @return      Returns ERRNO_INTERFACE_BUSY if the interface was busy,
 *              returns ERRNO_TIMEOUT if read doesn't succeed within 500us,
 *              otherwise returns ERRNO_NO_ERROR.
 * @examplecode driver_examples.c CEM102_Register_Read_Example
 */
int CEM102_Register_Read(const CEM102_Device *p_cfg, uint8_t addr,
                         uint16_t *p_data);

/**
 * @brief       Called by hardware DMA when data is transmitted.
 *              Check DMA transfer, manage flags and counters.
 *              Application is notified via callback function once the
 *              transmit is complete
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @examplecode driver_examples.c CEM102_TX_Handler_Example
 */
void CEM102_TX_Handler(const CEM102_Device *p_cfg);

/**
 * @brief       Called by hardware DMA when data is received.
 *              Check DMA transfer, manage flags and counters.
 *              Application is notified via callback function once the
 *              receive is complete
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @examplecode driver_examples.c CEM102_RX_Handler_Example
 */
void CEM102_RX_Handler(const CEM102_Device *p_cfg);

/**
 * @brief       Called by hardware GPIO handler when an interrupt is received.
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @examplecode driver_examples.c CEM102_GPIO_Handler_Example
 */
void CEM102_GPIO_Handler(const CEM102_Device *p_cfg);

/**
 * @brief       Returns the correct configuration for the given integration
 *              cycle settings and system chopper state.
 * @param[in]   p_cfg       Pointer to CEM102 device configuration
 * @param[out]  p_lsad_cfg  Pointer to return BUF_CHOP and ADC_CHOP
 *                          settings for the given parameters.
 * @param[out]  p_mask      Pointer to return mask value for the bits this API
 *                          would add. Mask bits for system chopper control
 *                          and ORSD would also be added because those two
 *                          parameters are input to this function. If mask 
 *                          is not needed, NULL can be passed.
 * @note                    Pointer p_lsad_cfg also an input to this
 *                          function to provide system chopper setting and
 *                          ORSD setting.
 * @examplecode driver_examples.c CEM102_Get_LSAD_Config_Example
 */
void CEM102_Get_LSAD_Config(const CEM102_Device *p_cfg, uint16_t *p_lsad_cfg,
                            uint16_t *p_mask);

/**
 * @brief       Read OTP and store OTP data in structure
 * @param[in]   cfg                Pointer to CEM102 device configuration
 * @param[in]   p_trim_struct      Pointer to the OTP Data Structure
 * @param[in]   cem102_sysclk_freq CEM102 System Clock Frequency in Hz
 * @param[in]   timeout            OTP read timeout value
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_OTP_Read_Example
 */
int CEM102_OTP_Read(const CEM102_Device *p_cfg,
                    CEM102_TrimStruct *p_trim_struct,
                    uint32_t cem102_sysclk_freq, uint32_t timeout);

/* ----------------------------------------------------------------------------
 * Power Function Prototypes
 * --------------------------------------------------------------------------*/
/**
 * @brief       Reset the CEM102 device's analog components
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @return      Returns ERRNO_NO_ERROR if the the device is reset,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Power_Reset_Example
 */
int CEM102_Power_Reset(const CEM102_Device *p_cfg);

/**
 * @brief       Initialize the power for the CEM102 device
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @return      Returns ERRNO_NO_ERROR if there is no error when initializing
 *              the power to the device, ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Power_Init_Example
 */
int CEM102_Power_Init(const CEM102_Device *p_cfg);

/**
 * @brief       Discharge the CEM102 VDDA for the specified amount of time
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @param[in]   time    Time in milliseconds to wait for VDDA to discharge
 * @return      Returns ERRNO_NO_ERROR if there is no error when initializing
 *              the power to the device, ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Power_VDDADischarge_Example
 */
int CEM102_Power_VDDADischarge(const CEM102_Device *p_cfg, unsigned int time);

/**
 * @brief       Trim the CEM102 device with data loaded from OTP or otherwise
 *              pre-calculated.
 * @param[in]   cfg            Pointer to CEM102 device configuration
 * @param[in]   p_trim_struct  Pointer to the OTP Data Structure
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Power_DeviceTrim_Example
 */
int CEM102_Power_DeviceTrim(const CEM102_Device *p_cfg,
                            CEM102_TrimStruct *p_trim_struct);

/**
 * @brief       Places VCC on AOUT and AOUT on the specified GPIO (aout_gpio)
 *              to use VCC to charge the VDDC of CEM102.
 * @param[in]   cfg            Pointer to CEM102 device configuration
 * @examplecode driver_examples.c CEM102_Power_VDDCCharge_Example
 */
void CEM102_Power_VDDCCharge(const CEM102_Device *p_cfg);

/**
 * @brief       Initialize SAR ADC
 * @param[in]   cfg        Pointer to CEM102 device configuration
 * @param[in]   sar_cfg    Config settings for SAR_CFG register
 * @param[in]   sar_ctrl   Config settings for SAR_CTRL resister
 * @param[in]   clk_cfg    Config settings for CLK_SEL_BYTE register
 * @examplecode driver_examples.c CEM102_SAR_Init_Example
 */
void CEM102_SAR_Init(const CEM102_Device *p_cfg, uint32_t sar_cfg,
                     uint32_t sar_ctrl, uint32_t clk_cfg);

/**
 * @brief       Start SAR ADC Conversion
 * @param[in]   conv_mode  SAR Conversion mode: use
 *                         SAR_START_SINGLE or SAR_START_CONTINUOUS
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR otherwise.
 * @examplecode driver_examples.c CEM102_SAR_StartConversion_Example
 */
int CEM102_SAR_StartConversion(uint32_t conv_mode);

/**
 * @brief       Measure VBAT and flag OK if CEM102 VBAT is above the required threshold.
 *              Blocks waiting for the LSAD read in complete.
 * @param[in]   trim_error  Default trim error
 * @param[in]   threshold   Threshold Voltage in mV
 * @param[out]  p_data      Pointer to where VBAT value is to be written
 * @return      Returns 1 if VBAT is above the threshold voltage, 0 otherwise
 * @examplecode driver_examples.c CEM102_VBAT_OK_Example
 */
int CEM102_VBAT_OK(uint32_t trim_error, uint32_t threshold, uint16_t *p_data);

/**
 * @brief       Measure VCC and flag OK if CEM102 VCC is above the required threshold.
 *              Blocks waiting for the LSAD read in complete.
 * @param[in]   trim_error  Default trim error
 * @param[in]   threshold   Threshold Voltage in mV
 * @param[out]  p_data      Pointer to where VCC value is to be written
 * @return      Returns 1 if VCC is above the threshold voltage, 0 otherwise
 * @examplecode driver_examples.c CEM102_VCC_OK_Example
 */
int CEM102_VCC_OK(uint32_t trim_error, uint32_t threshold, uint16_t *p_data);

/**
 * @brief       Measure a rail and flag OK if the voltage is above the required threshold.
 *              Blocks waiting for the LSAD read in complete.
 * @param[in]   trim_error  Default trim error
 * @param[in]   threshold   Threshold Voltage in mV
 * @param[out]  p_data      Pointer to where the voltage value is to be written
 * @param[in]   rail        0 for measuring VBAT, 1 for measuring VCC.
 * @return      Returns 1 if VBAT is above threshold voltage, 0 otherwise
 * @examplecode driver_examples.c CEM102_VOLTAGE_OK_Example
 */
int CEM102_VOLTAGE_OK(uint32_t trim_error, uint32_t threshold,
                      uint16_t *p_data, uint32_t rail);

/**
 * @brief       Monitor the VBAT and VDDA differential voltage.
 *              Trigger a wake up when the VBAT-VDDA differential voltage
 *              is less than the specified voltage threshold.
 *              For accurate measurements when CEM102 is powered from VBAT,
 *              this threshold will typically be set to 75 mV.
 * @param[in]   cfg            Pointer to CEM102 device configuration
 * @param[in]   measurement_en Enable Measurement
 * @param[in]   VBAT_margin    Threshold Voltage in mV
 * @examplecode driver_examples.c CEM102_VBAT_VDDA_MonitorStart_Example
 */
void CEM102_VBAT_VDDA_MonitorStart(const CEM102_Device *p_cfg,
                                   uint8_t measurement_en,
                                   uint32_t VBAT_margin);

/**
 * @brief       Calibrate VCC to the target voltage
 * @param[in]   cfg           Pointer to CEM102 device configuration
 * @param[in]   vcc_min_trim  Minimum safe VCC trim value
 *                            (usually the VCC trim value at
 *                            the room temperature).
 * @param[in]   vcc_target    Target VCC Voltage in mV
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE if max or min TRIM values are set.
 * @examplecode driver_examples.c CEM102_Calibrate_VCC_Example
 */
int CEM102_Calibrate_VCC(const CEM102_Device *p_cfg, uint8_t vcc_min_trim,
                         const uint32_t vcc_target);

/**
 * @brief       Calculate CEM102 VBAT
 * @param[out]  p_voltage   Output pointer to return calculated VBAT measurement in mV
 * @assumptions CEM102_VBAT_VDDA_MonitorStart is already running
 * @examplecode driver_examples.c CEM102_Calculate_VBAT_Example
 */
void CEM102_Calculate_VBAT(uint16_t *p_voltage);

/* ----------------------------------------------------------------------------
 * System Support Function Prototypes
 * --------------------------------------------------------------------------*/
/**
 * @brief       Configure the IRQ behavior
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @param[in]   irq_cfg Configuration to be written to SYSCTRL_IRQ_CFG.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_System_IRQConfig_Example
 */
int CEM102_System_IRQConfig(const CEM102_Device *p_cfg, uint16_t irq_cfg);

/**
 * @brief       Issue the specified system command configuration
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @param[in]   cmd     Command configuration to write
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_System_Command_Example
 */
int CEM102_System_Command(const CEM102_Device *p_cfg, uint16_t cmd);

/**
 * @brief       Queue Status Registers (SYS_BUFFER_VIOL_CNT, SYS_BUFFER_VIOL_STATUS, & SYS_STATUS)
 * @param[in]   cfg  Pointer to CEM102 device configuration
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_INTERFACE_BUSY if the SPI interface is busy.
 * @examplecode driver_examples.c CEM102_System_Status_Queue_Example
 */
int CEM102_System_Status_Queue(const CEM102_Device *p_cfg);

/**
 * @brief       Process Status Registers results
 * @param[in]   data            Pointer to the arrays containing
 *                              (SYS_BUFFER_VIOL_CNT, SYS_BUFFER_VIOL_STATUS, & SYS_STATUS)
 *                              register values.
 * @param[out]  p_violation_cnt Pointer to store Violation Count if any
 * @return      Returns SYS_STATE setting or prioritized error
 * @examplecode driver_examples.c CEM102_System_Status_Process_Example
 */
int CEM102_System_Status_Process(uint16_t *p_data, uint16_t *p_violation_cnt);

/* ----------------------------------------------------------------------------
 * Signal Chain Configuration Function Prototypes
 * --------------------------------------------------------------------------*/
/**
 * @brief       Configure the LSAD clocks
 * @param[in]   p_cfg       Pointer to CEM102 device configuration
 * @param[in]   prescale    Prescale settings for the LSAD
 * @param[in]   delay       LSAD conversion start delay
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Signal_ClockConfig_Example
 */
int CEM102_Signal_ClockConfig(const CEM102_Device *p_cfg, uint16_t prescale,
                              uint16_t delay);

/**
 * @brief       Configure a trans-impedance amplifier (TIA)
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @param[in]   tia     TIA to configure; valid options are WE1 or WE2.
 * @param[in]   enable  CH*_TIA_ENABLE or CH*_TIA_DISABLE
 * @param[in]   tia_cfg The feedback resistor and power mode configuration for
 *                      the TIA; use CH*_HP_MODE_*, CH*_TIA_INT_FB_*, and
 *                      CH*_TIA_FB_RES_*.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Signal_TIAConfig_Example
 */
int CEM102_Signal_TIAConfig(const CEM102_Device *p_cfg, int tia,
                            uint16_t enable, uint16_t tia_cfg);

/**
 * @brief       Configure the switches for a channel.
 * @param[in]   p_cfg       Pointer to CEM102 device configuration
 * @param[in]   channel     Channel to configure switches for; valid options
 *                          are WE1 or WE2.
 * @param[in]   channel_cfg Switch configurations; use a set of
 *                          CH*_SC*_CONNECT, CH*_SC*_DISCONNECT definitions.
 * @note        Any passed in configuration for CH*_SC114 is ignored and this
 *              switch remains disconnected.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Signal_SwitchConfig_Example
 */
int CEM102_Signal_SwitchConfig(const CEM102_Device *p_cfg, int channel,
                               uint16_t channel_cfg);

/**
 * @brief       Configure the switches for RE or ATBus.
 * @param[in]   cfg       Pointer to CEM102 device configuration
 * @param[in]   channel   Channel to configure switches for; valid options
 *                        are WE1 (RE), WE2 (SA Switches), or RE (ST Switches).
 * @param[in]   config    Switch configurations; use a set of
 *                        CH*_SR*_CONNECT, CH*_SR*_DISCONNECT or
 *                        CH*_ST*_CONNECT, CH*_ST*_DISCONNECT or
 *                        CH*_SA*_CONNECT, CH*_SA*_DISCONNECT definitions.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Signal_RE_ATBus_Config_Example
 */
int CEM102_Signal_RE_ATBus_Config(const CEM102_Device *p_cfg, int channel,
                                  uint16_t config);

/**
 * @brief       Configure the buffer and low pass filtering for a channel.
 * @param[in]   p_cfg       Pointer to CEM102 device configuration
 * @param[in]   channel     Channel to configure the buffer for; valid options
 *                          are WE1 or WE2.
 * @param[in]   channel_cfg Buffer bias and low-pass filter configuration; use
 *                          CH*_BUF_IB* and CH*_LPF_BW_CFG_ENABLE, CH*_LPF_BW_CFG_DISABLE
 * @param[in]   buf_cfg     Buffer configuration; use CH*_BUF_ENABLE, CH*_BUF_DISABLE and
 *                          CH*_BUF_BYPASS_ENABLE, CH*_BUF_BYPASS_DISABLE
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Signal_BufferConfig_Example
 */
int CEM102_Signal_BufferConfig(const CEM102_Device *p_cfg, int channel,
                               uint16_t channel_cfg, uint16_t buf_cfg);


/**
 * @brief       Configure a working electrode DAC.
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @param[in]   dac     WE DAC to configure; valid options are WE1 or WE2.
 * @param[in]   dac_cfg WE DAC configurations; use DAC_WE*_ENABLE/DAC_WE*_DISABLE
 *                      and a DAC value between 0.4 V and 1.9 V.
 * @param[in]   interrupt_cfg DAC IRQ configuration; use DC_DAC_LOAD_IRQ_*
 * @return      Returns ERRNO_PARAM_ERROR if the DAC value is invalid,
 *              ERRNO_GENERAL_FAILURE if a register read/write fails,
 *              ERRNO_NO_ERROR otherwise.
 * @examplecode driver_examples.c CEM102_Signal_WEDACConfig_Example
 */
int CEM102_Signal_WEDACConfig(const CEM102_Device *p_cfg, int dac,
                              uint16_t dac_cfg, uint16_t interrupt_cfg);

/**
 * @brief       Configure the reference electrode DAC.
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @param[in]   dac_cfg RE DAC configurations; use DAC_RE_ENABLE/DAC_RE_DISABLE
 *                      and a DAC value between 0.4 V and 1.9 V.
 * @param[in]   sw_cfg  RE DAC switch configuration; use a set of RE_SR*_DISCONNECT,
 *                      RE_SR*_CONNECT defines.
 * @param[in]   buf_cfg RE buffer configuration; use RE_BUF_ENABLE/RE_BUF_DISABLE,
 *                      RE_HP_MODE_ENABLE/RE_HP_MODE_DISABLE
 * @return      Returns ERRNO_PARAM_ERROR if the DAC value is invalid,
 *              ERRNO_GENERAL_FAILURE if a register read/write fails,
 *              ERRNO_NO_ERROR otherwise.
 * @examplecode driver_examples.c CEM102_Signal_REDACConfig_Example
 */
int CEM102_Signal_REDACConfig(const CEM102_Device *p_cfg, uint16_t dac_cfg,
                              uint16_t sw_cfg, uint16_t buf_cfg);

/**
 * @brief       Configure the buffer and low pass filtering for a channel.
 * @param[in]   p_cfg       Pointer to CEM102 device configuration
 * @param[in]   channel     Channel to configure the buffer for; valid options
 *                          are WE1 or WE2.
 * @param[in]   lsad_cfg    LSAD configuration; use values for settings; use values
 *                          for setting all of CHCFG_CH*_LSAD
 * @param[in]   thresh_cfg  Threshold configuration; use CH*_VIOLATION_CHECK_* and a
 *                          threshold value.
 * @param[in]   buf_cfg     Buffer configuration; use values for setting all of
 *                          CHCFG_CH*_BUFFER
 * @param[in]   accum_cfg   Accumulator configuration; use values for setting all of
 *                          CHCFG_CH*_ACCUM
 * @note        The CHx_ACCUM_SAMPLE_CNT value in accum_cfg must be even if
 *              LSADx_SYSTEM_CHOP_CFG is enabled in lsad_cfg.
 * @return      Returns ERRNO_PARAM_ERROR if the CHx_ACCUM_SAMPLE_CNT value is invalid,
 *              ERRNO_GENERAL_FAILURE if a register read/write fails,
 *              ERRNO_NO_ERROR otherwise.
 * @examplecode driver_examples.c CEM102_Signal_ADCConfig_Example
 */
int CEM102_Signal_ADCConfig(const CEM102_Device *p_cfg, int channel,
                            uint16_t lsad_cfg, uint16_t thresh_cfg,
                            uint16_t buf_cfg, uint16_t accum_cfg);

/* ----------------------------------------------------------------------------
 * Signal Chain Measurement Function Prototypes
 * --------------------------------------------------------------------------*/
/**
 * @brief       Configures the switches and buffers for a direct ADC
 *              measurement.
 * @param[in]   p_cfg       Pointer to CEM102 device configuration
 * @param[in]   channel     Channel to configure switches for; valid options
 *                          are WE1 or WE2.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Signal_DirectADCConfig_Example
 */
int CEM102_Signal_DirectADCConfig(const CEM102_Device *p_cfg, uint32_t channel);

/**
 * @brief       Start a measurement by changing the command state
 * @param[in]   cfg     Pointer to CEM102 device configuration
 * @param[in]   cmd     State command configuration needed to start the
 *                      appropriate measurement.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Signal_StartMeasurement_Example
 */
int CEM102_Signal_StartMeasurement(const CEM102_Device *p_cfg, uint16_t cmd);

/**
 * @brief       Process an accumulated data sample
 * @param[in]   cfg              Pointer to CEM102 device configuration
 * @param[in]   data             Data read from the accumulator (16 bit)
 * @param[in]   lsad_cfg         LSAD configuration; use LSAD*_GAIN_UNITY, LSAD*_GAIN_DOUBLE
 * @param[in]   sample_cnt       Sample count
 * @param[in]   shift            Accumulator shift
 * @param[out]  p_processed_data Pointer to variable to return the processed value.
 * @return      Returns calculated voltage.
 * @examplecode driver_examples.c CEM102_Signal_ProcessADCSingle_Example
 */
void CEM102_Signal_ProcessADCSingle(const CEM102_Device *p_cfg, uint16_t data,
                                    uint16_t lsad_cfg, int sample_cnt,
                                    int shift, float *p_processed_data);

/**
 * @brief       Process an accumulated data sample stored with double precision
 * @param[in]   cfg              Pointer to CEM102 device configuration
 * @param[in]   data             Data read from the accumulator (16 bit)
 * @param[in]   lsad_cfg         LSAD configuration; use LSAD*_GAIN_UNITY, LSAD*_GAIN_DOUBLE
 * @param[in]   sample_cnt       Sample count
 * @param[in]   shift            Accumulator shift
 * @param[out]  p_processed_data Pointer to variable to return the processed value.
 * @return      Returns calculated voltage.
 * @examplecode driver_examples.c CEM102_Signal_ProcessADCDouble_Example
 */
void CEM102_Signal_ProcessADCDouble(const CEM102_Device *p_cfg, uint32_t data,
                                    uint16_t lsad_cfg, int sample_cnt,
                                    int shift, float *p_processed_data);

/**
 * @brief       Start a measurement of the 100 nA internal reference current,
 *              configuring the device settings to match those used for the
 *              OTP measurement.
 * @param[in]   p_cfg       Pointer to CEM102 device configuration
 * @param[in]   channel     Measurement channel to be used.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if the incorrect channel is selected,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @note        This function will connect the TIA to the internal reference
 *              current. The user must configure the internal switches to
 *              re-connect the electrode after calibration is complete,
 *              disconnecting the internal reference current.
 * @note        This function will start a measurement, but is non-blocking.
 *              The results must be obtained by the application when they are ready.
 * @examplecode driver_examples.c CEM102_Signal_MeasureReference_Example
 */
int CEM102_Signal_MeasureReference(const CEM102_Device *p_cfg,
                                   uint32_t channel);

/**
 * @brief       Start a measurement of the 100 nA internal reference current,
 *              using the device settings defined by the user application.
 * @param[in]   p_cfg       Pointer to CEM102 device configuration
 * @param[in]   channel     Measurement channel to be used.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if the incorrect channel is selected,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @note        This function will connect the TIA to the internal reference
 *              current. The user must configure the internal switches to
 *              re-connect the electrode after calibration is complete,
 *              disconnecting the internal reference current.
 * @examplecode driver_examples.c CEM102_Signal_CalibrateChannel_Example
 */
int CEM102_Signal_CalibrateChannel(const CEM102_Device *p_cfg,
                                   uint32_t channel);

/**
 * @brief       Calibrate the selected electrode to the desired voltage
 * @param[in]   cfg                 Pointer to CEM102 device configuration
 * @param[out]  p_offset_value      Pointer to store calibrated offset value once
 *                                  calibration is complete
 * @param[in]   electrode           Selecting the desired electrode using CEM102_DAC_TYPE
 * @param[in]   voltage_target_uv   The desired output voltage in uV
 * @param[in]   measured_voltage_uv The most recent measured voltage in uV
 * @param[in]   p_cal_status        Pointer to the status of the current calibration procedure
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @examplecode driver_examples.c CEM102_Signal_CalibrateDACVoltage_Example
 */
int CEM102_Signal_CalibrateDACVoltage(const CEM102_Device *p_cfg,
                                      int16_t *p_offset_value,
                                      uint32_t electrode,
                                      int32_t voltage_target_uv,
                                      int32_t measured_voltage_uv,
                                      CEM102_DAC_CAL_TYPE *p_cal_status);

/**
 * @brief       Calculates the gain (fA/LSB) of the entire channel using the
 *              provided measurements and sets the internal gain variable.
 * @param[in]   p_cfg       Pointer to CEM102 device configuration
 * @param[in]   channel     Measurement channel to be used.
 * @param[in]   ref_val     Reference measurement value, also the return value of
 *                          CEM102_Signal_MeasureReference.
 * @param[in]   cal_val     Calibration measurement value, also the return value of
 *                          CEM102_Signal_CalibrateChannel. *
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if the incorrect channel is selected,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @note        This function will connect the TIA to the internal reference
 *              current. The user must configure the internal switches to
 *              re-connect the electrode after calibration is complete,
 *              disconnecting the internal reference current.
 * @note        This function will start a measurement, but is non-blocking.
 *              The results must be obtained by the application when they are ready.
 * @note        This function takes as input arguments the measurements obtained
 *              from CEM102_Signal_MeasureReference() and CEM102_Signal_CalibrateChannel.
 * @examplecode driver_examples.c CEM102_Signal_CalculateChannelGain_Example
 */
int CEM102_Signal_CalculateChannelGain(const CEM102_Device *p_cfg,
                                       uint32_t channel, uint32_t ref_val,
                                       uint32_t cal_val);

/**
 * @brief       Calculates the gain (uV/LSB) of the ADC using the
 *              provided input parameters and sets the internal gain variable.
 * @param[in]   channel     Measurement channel to be used; valid options
 *                          are WE1, WE2.
 * @param[in]   num_accum   Number of accumulations to be used in measurements.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if invalid channel selected,
 *              ERRNO_GENERAL_FAILURE if the OTP has not been read.
 * @assumptions The OTP has previously been read correctly.
 * @examplecode driver_examples.c CEM102_Signal_CalculateADCGain_Example
 */
int CEM102_Signal_CalculateADCGain(uint32_t channel, uint32_t num_accum);

/**
 * @brief       Read the calibrated current in femto amps for both source and
 *              sink. Returned values are actual values, not the deviation
 *              from the expected calibration current.
 * @param[out]  p_ibp  calibrated current for the source (ibp).
 * @param[out]  p_ibn  calibrated current for the sink (ibn).
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_GENERAL_FAILURE if OTP information is not found.
 * @examplecode driver_examples.c CEM102_Signal_GetCalibratedCurrent_Example
 */
int CEM102_Signal_GetCalibratedCurrent(uint32_t *p_ibp, uint32_t *p_ibn);

/**
 * @brief       Calculates the current measured by the electrode using the
 *              previously measured and calculated channel gain.
 * @param[in]   channel     Measurement channel to be used.
 * @param[in]   lsad_value  Raw sample data from the ADC.
 * @param[out]  p_current   Input pointer to return calculated current
 *                          measurement in fA
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if the incorrect channel is selected.
 * @assumptions The channel gain has been previously calculated by
 *              CEM102_Signal_CalculateChannelGain().
 * @examplecode driver_examples.c CEM102_Signal_CalculateCurrent_Example
 */
int CEM102_Signal_CalculateCurrent(uint32_t channel, int32_t lsad_value,
                                   float *p_current);

/**
 * @brief       Calculates the voltage measured by the electrode using the
 *              previously measured and calculated ADC gain.
 * @param[in]   channel     Measurement channel to be used; valid options
 *                          are WE1, WE2, or RE.
 * @param[in]   lsad_value  Raw sample data from the ADC.
 * @param[out]  p_voltage   Input pointer to return calculated voltage
 *                          measurement in uV
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if the incorrect channel is selected.
 * @assumptions The adc gain has been previously calculated by
 *              CEM102_Signal_CalculateADCGain().
 * @examplecode driver_examples.c CEM102_Signal_CalculateVoltage_Example
 */
int CEM102_Signal_CalculateVoltage(uint32_t channel, int32_t lsad_value,
                                   int32_t *p_voltage);

/**
 * @brief       Calculate DAC Offset values based on OTP version and data.
 * @param[in]   offset_type    Type of Offset to calculate
 *                              1: WE1_DC_DAC
 *                              2: WE2_DC_DAC
 *                              3: RE_DC_DAC
 * @param[out]  p_offset_value Address of the variable to store calculated value
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if wrong offset_type selected.
 * @examplecode driver_examples.c CEM102_Signal_CalculateDACOffset_Example
 */
int CEM102_Signal_CalculateDACOffset(CEM102_DAC_TYPE offset_type,
                                     int16_t *p_offset_value);

/**
 * @brief       Read the ADC Gain values
 * @param[in]   channel         Measurement channel to be used.
 * @param[out]  adc_gain_value  Pointer to the variable to store adc_gain value.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if invalid channel selected.
 * @examplecode driver_examples.c CEM102_Signal_GetADCGain_Example
 */
int CEM102_Signal_GetADCGain(uint32_t channel, float *adc_gain_value);

/**
 * @brief       Calculate DAC Gain
 * @param[in]   channel       Measurement channel to be used; valid options
 *                            are WE1, WE2, or RE.
 * @param[in]   p_dac_gain    Pointer to the variable to store DAC gain value.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if invalid channel selected.
 *              ERRNO_GENERAL_FAILURE if OTP Rev1 found
 * @examplecode driver_examples.c CEM102_Signal_CalculateDACGain_Example
 */
int CEM102_Signal_CalculateDACGain(uint32_t channel, double *p_dac_gain);

/**
 * @brief       Gets the value from OTP for the DAC register, for the given voltage.
 * @param[in]   p_cfg           Pointer to CEM102 device configuration
 * @param[in]   channel         Channel to calibrate the DAC for; valid options
 *                              are WE1, WE2, or RE.
 * @param[in]   mvolts          Target voltage in millivolts.
 * @param[out]  p_dac_setting   The desired DAC setting for the given target
 *                              voltage.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if invalid channel selected,
 *              ERRNO_GENERAL_FAILURE if OTP Rev1 or Rev3 found.
 * @note        This API calibrates the DAC using calibrated OTP values.
 *              Refer to CEM102_DAC_CalibrateWithADC for an alternative method.
 * @examplecode driver_examples.c CEM102_DAC_Calibrate_Example
 */
int CEM102_DAC_Calibrate(const CEM102_Device *p_cfg, uint32_t channel,
                         uint16_t mvolts, uint16_t *p_dac_setting);

/**
 * @brief       Gets the value for the DAC register for the given voltage by
 *              calibrating DAC using calibrated ADC.
 * @param[in]   p_cfg           Pointer to CEM102 device configuration
 * @param[in]   channel         Channel to calibrate the DAC for; valid options
 *                              are WE1, WE2, or RE.
 * @param[in]   mvolts          Target voltage in millivolts.
 * @param[out]  p_dac_setting   The desired DAC setting for the given target
 *                              voltage.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if invalid channel selected,
 *              ERRNO_CALIBRATION_ERROR if an error encountered in calibration,
 *              ERRNO_GENERAL_FAILURE otherwise.
 * @note        This API calibrates the DAC using calibrated ADC. Refer to
 *              CEM102_DAC_Calibrate for an alternative method.
 * @examplecode driver_examples.c CEM102_DAC_CalibrateWithADC_Example
 */
int CEM102_DAC_CalibrateWithADC(const CEM102_Device *p_cfg, uint32_t channel,
                                uint16_t mvolts, uint16_t *p_dac_setting);

/**
 * @brief       Gets the version of the OTP data.
 * @return      Returns 0 on failure. Non-zero value would give
 *              the version of the OTP.
 * @note        It is important to call this after CEM102_OTP_Read
 *              is called by the application. Otherwise, version
 *              information won't be available and the function
 *              would return 0 until CEM102_OTP_Read is called.
 * @examplecode driver_examples.c CEM102_OTP_GetVersion_Example
 */
uint16_t CEM102_OTP_GetVersion(void);

/**
 * @brief       Reads the register, writes the given value, only for the the
 *              bits given in the mask, before writing it back. Blocks with
 *              timeout, waiting for the operation to complete.
 * @param[in]   cfg     Pointer to the CEM102 device to be used.
 * @param[in]   addr    Address of the register to read & modify.
 * @param[in]   data    Value, after applying the mask will be written to the
                        register.
 * @param[in]   mask    Bits that are set in the mask, will be modified in the
                        register.
 * @return      Returns ERRNO_INTERFACE_BUSY if the interface was busy,
 *              returns ERRNO_TIMEOUT if read doesn't succeed within 500us,
 *              otherwise returns ERRNO_NO_ERROR.
 * @examplecode driver_examples.c CEM102_Register_ReadModifyWrite_Example
 */
int CEM102_Register_ReadModifyWrite(const CEM102_Device *p_cfg, uint8_t addr,
                                    uint16_t data, uint16_t mask);

/**
 * @brief       Configure the device for analog IO measurements.
 * @param[in]   cfg       Pointer to CEM102 device configuration
 * @param[in]   channel   Channel to configure; valid options are WE1, WE2.
 * @param[out]  p_aiocfg  Pointer to save the original values for registers
 *                        that are about to be changed.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_TIMEOUT on timeout during register access.
 * @examplecode driver_examples.c CEM102_Signal_AIOConfigInit_Example
 */
int CEM102_Signal_AIOConfigInit(const CEM102_Device *p_cfg, uint32_t channel,
                                CEM102_RegSaveData *p_aiocfg);

/**
 * @brief       Restore the registers that were modified for analog IO
 *              measurements, to its prior state.
 * @param[in]   cfg       Pointer to CEM102 device configuration
 * @param[in]   channel   Channel to configure; valid options are WE1, WE2.
 * @param[in]   p_aiocfg  Pointer to the location where the original values
 *                        were stored.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_TIMEOUT on timeout during register access.
 * @examplecode driver_examples.c CEM102_Signal_AIOConfigDeinit_Example
 */
int CEM102_Signal_AIOConfigDeinit(const CEM102_Device *p_cfg, uint32_t channel,
                                  CEM102_RegSaveData *p_aiocfg);

/**
 * @brief       Gets the value for the measured current.
 * @param[in]   p_cfg           Pointer to CEM102 device configuration
 * @param[in]   req             Current to measure; valid options are
 *                              CEM102_OFFSET_CURRENT_WE1,
 *                              CEM102_OFFSET_CURRENT_WE2,
 *                              CEM102_OFFSET_CURRENT_WE1_WE2,
 *                              CEM102_CH_RES_CURRENT_WE1,
 *                              CEM102_CH_RES_CURRENT_WE2,
 *                              CEM102_RE_RES_CURRENT,
 *                              CEM102_AT_RES_CURRENT_WE1,
 *                              CEM102_AT_RES_CURRENT_WE2,
 *                              CEM102_AT_RES_CURRENT_WE1_WE2,
 *                              CEM102_RE_CE_RES_CURRENT.
 * @param[out]  p_data          Pointer to CEM102_CurrentData to return measured
 *                              current value
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if an invalid request is selected,
 *              ERRNO_NOT_IMPLEMENTED when CEM102_DIAG_FUNCTIONS is set to 1.
 * @note        This API measures various residual currents and offset current
 *              based on the request. This is an old, obsolete implementation.
 *              To use the newer, robust implementation, set the compiler option
 *              CEM102_DIAG_FUNCTIONS to 1. For more information, refer the
 *              function CEM102_Diagnostics.
 * @examplecode driver_examples.c CEM102_Signal_MeasureCurrent_Example
 */
int CEM102_Signal_MeasureCurrent(const CEM102_Device *p_cfg,
                                 CEM102_CURR_MEASURE_TYPE req,
                                 CEM102_CurrentData *p_data);

/**
 * @brief       Measures the voltage using ADC the selected channel(s).
 * @param[in]   p_cfg           Pointer to CEM102 device configuration
 * @param[in]   chan            Channel to measure voltage; valid options are
 *                              WE1, WE2, and RE.
 * @param[out]  p_voltage1      Measured voltage for the given channel is
 *                              stored, when p_voltage2 is NULL. Otherwise,
 *                              voltage for WE1 is returned.
 * @param[out]  p_voltage2      Measured voltage is returned for WE2. When
 *                              this pointer is valid, API would return voltage
 *                              for both WE1 and WE2.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if an invalid request is selected.
 * @note        When p_voltage2 pointer is not NULL, the API will measure
 *              voltage for both WE1 and WE2. Channel parameter is ignored.
 * @examplecode driver_examples.c CEM102_Signal_MeasureVoltage_Example
 */
int CEM102_Signal_MeasureVoltage(const CEM102_Device *p_cfg, int chan,
                                 int32_t *p_voltage1, int32_t *p_voltage2);

/**
 * @brief       Saves the requested register set in the given data structure.
 * @param[in]   p_cfg           Pointer to CEM102 device configuration
 * @param[out]  p_data          Structure that contains the requested register
 *                              list that are expected to be saved. Successfully
 *                              saved registers are also marked for later use.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if an invalid request is selected.
 * @examplecode driver_examples.c CEM102_Register_Save_Example
 */
int CEM102_Register_Save(const CEM102_Device *p_cfg,
                         CEM102_RegSaveData *p_data);

/**
 * @brief       Restores the saved register values.
 * @param[in]   p_cfg           Pointer to CEM102 device configuration
 * @param[out]  p_data          Contains values of registers and information on
 *                              the register set that are expected to be restored.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if an invalid request is selected.
 * @examplecode driver_examples.c CEM102_Register_Restore_Example
 */
int CEM102_Register_Restore(const CEM102_Device *p_cfg,
                            CEM102_RegSaveData *p_data);

/**
 * @brief       Used for measuring the DAC voltage with LSAD. This is an
 *              internal API, with no generic functionality for the user
 *              applications.
 * @param[in]   p_cfg           Pointer to CEM102 device configuration
 * @param[in]   dac             Channel to measure voltage; valid options are
 *                              WE1, WE2, and RE.
 * @param[in]   flag            Flag to indicate settings for pre-measurement and post
 *                              measurement; valid options are CEM102_PRE_MEASURE_CONFIG
 *                              and CEM102_POST_MEASURE_CONFIG.
 * @note        This is an internal API meant for running diagnostics.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PARAM_ERROR if an invalid request is selected.
 * @examplecode driver_examples.c CEM102_DAC_ADC_Setup_Example
 */
int CEM102_DAC_ADC_Setup(const CEM102_Device *p_cfg, CEM102_DAC_TYPE dac,
                         uint32_t flag);

/**
 * @brief       Erase persistent storage with calibration data.
 * @return      Returns ERRNO_NO_ERROR, if there is no error.
 * @examplecode driver_examples.c CEM102_Calibration_Erase_Example
 */
int CEM102_Calibration_Erase(void);

/**
 * @brief       Checks if persistent storage has valid calibration data.
 * @return      Returns true, if valid calibration data available.
 *              Returns false otherwise.
 * @note        Users who prefer running the calibration routine at each
 *              device initialization can either modify the CalibrationValid
 *              API to return FALSE, or comment out the section of the code that
 *              runs, when this API returns true in demo sample application.
 * @examplecode driver_examples.c CEM102_Calibration_Valid_Example
 */
int CEM102_Calibration_Valid(void);

/**
 * @brief       Saves the given calibration data.
 * @param[in]   p_data          Pointer to calibration data that is to be saved.
 * @return      Returns ERRNO_NO_ERROR if there is no error,
 *              ERRNO_PERS_WRITE_ERROR if write fails,
 *              ERRNO_GENERAL_FAILURE if data size is too big to fit in.
 * @examplecode driver_examples.c CEM102_Calibration_Save_Example
 */
int CEM102_Calibration_Save(CEM102_CalibrationData *p_data);

/**
 * @brief       Checks if persistent storage has valid vcc calibration data.
 * @return      Returns true, if valid vcc calibration data available.
 *              Returns false otherwise.
 * @note        Users who prefer running the calibration routine at each
 *              device initialization can either modify the VCCCalibrationValid
 *              API to return FALSE, or comment out the section of the code that
 *              runs, when this API returns true in demo sample application.
 * @examplecode driver_examples.c CEM102_VCC_Calibration_Valid_Example
 */
bool CEM102_VCC_Calibration_Valid(void);

/** @} */ /* End of the CEM102_DRIVERg group */

/* Internal API, to get DAC_REF_VALUE field in DC_DAC_RE_CTRL register */
uint16_t CEM102_GetDACValue(const CEM102_Device *p_cfg, CEM102_DAC_TYPE dac);
int max30132_spidrv_read_buffer(const CEM102_Device *p_cfg, uint8_t addr, unsigned int len);
int max30132_spidrv_read_action(const CEM102_Device *p_cfg, uint8_t *rbuffer, unsigned int rlen);
int max30132_read_reg_action(const CEM102_Device *p_cfg, uint8_t addr,	uint8_t *rbuffer, unsigned int rlen);

int max30132_spidrv_write_buffer(const CEM102_Device *p_cfg, uint8_t addr, unsigned int wlen);
int max30132_spidrv_write_action(const CEM102_Device *p_cfg, uint8_t addr, uint8_t *wbuffer, unsigned int wlen);
int max30132_write_reg_action(const CEM102_Device *p_cfg, uint8_t addr,   uint8_t *wbuffer, unsigned int wlen);

#ifdef  __cplusplus
}
#endif

#endif    /* CEM102_DRIVER_H */

