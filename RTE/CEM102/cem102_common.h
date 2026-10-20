/**
 * @file cem102_common.h
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

#ifndef CEM102_COMMON_H_
#define CEM102_COMMON_H_

#ifdef  __cplusplus
extern "C"
{
#endif

/* External include files supporting embedded software components on the host
 * system. */
#include <hw.h>

#define CEM102_SHIFT_FOR_BYTE                      (8U)
#define CEM102_SHIFT_FOR_WORD                      (16U)
#define CEM102_SHIFT_TO_MULTIPLY_BY_2              (1U)
#define CEM102_SHIFT_LIMIT_FOR_UINT                (31U)
#define CEM102_SHIFT_LIMIT_FOR_USHORT              (15U)
#define CEM102_SPI_BUF_ADDR_IDX                    (0)
#define CEM102_SPI_BUF_CMD_IDX                     (1)
#define CEM102_SPI_BUF_DATA_IDX                    (2)
#define CEM102_SPI_DATA_IDX_MS_BYTE                (2)
#define CEM102_SPI_DATA_IDX_LS_BYTE                (3)
#define CEM102_SPI_FRAME_MAX_DBYTES                (64)
#define CEM102_SPI_FRAME_HDR_SZ                    (2)
#define CEM102_SPI_DBYTE_MULT_FACTOR               (2)
#define CEM102_SPI_SINGLE_XFER_COUNT               (1)
#define CEM102_INITIAL_VALUE                       (0)
#define CEM102_8_BITS_MASK                         (0xFFU)
#define CEM102_16_BITS_MASK                        (0xFFFFU)
#define CEM102_INDEX_ADJ                           (1)
#define CEM102_WE1_UPPER_HALF_IDX                  (0)
#define CEM102_WE1_LOWER_HALF_IDX                  (1)
#define CEM102_WE2_UPPER_HALF_IDX                  (2)
#define CEM102_WE2_LOWER_HALF_IDX                  (3)
#define CEM102_MICRO_UNITS_CONV_FACTOR             (1000000)
#define CEM102_NUM_LSADS                           (2)
#define CEM102_LSAD1_IDX                           (0)
#define CEM102_LSAD2_IDX                           (1)

/** @addtogroup CEM102_DRIVERg
 *  @{
 */

/**
* @brief Mask definitions needed for the driver
*/

#define ANA_CFG2_SENSOR_CAL_CFG_MASK               (1U << ANA_CFG2_SENSOR_CAL_CFG_POS)

#define SYSCTRL_IRQ_CFG_VDDA_ERR_IRQ_CFG_MASK      (1U << SYSCTRL_IRQ_CFG_VDDA_ERR_IRQ_CFG_POS)
#define SYSCTRL_IRQ_CFG_STATE_ERR_IRQ_CFG_MASK     (1U << SYSCTRL_IRQ_CFG_STATE_ERR_IRQ_CFG_POS)
#define SYSCTRL_IRQ_CFG_DC_DAC_LOAD_IRQ_CFG_MASK   (1U << SYSCTRL_IRQ_CFG_DC_DAC_LOAD_IRQ_CFG_POS)
#define CHCFG_CH2_LSAD_LSAD2_SYSTEM_CHOP_CFG_MASK  (1U << CHCFG_CH2_LSAD_LSAD2_SYSTEM_CHOP_CFG_POS)
#define SYSCTRL_IRQ_CFG_CH2_VIOLATION_IRQ_CFG_MASK (1U << SYSCTRL_IRQ_CFG_CH2_VIOLATION_IRQ_CFG_POS)
#define SYSCTRL_IRQ_CFG_CH1_VIOLATION_IRQ_CFG_MASK (1U << SYSCTRL_IRQ_CFG_CH1_VIOLATION_IRQ_CFG_POS)
#define SYSCTRL_IRQ_CFG_DIGITAL_RESET_IRQ_CFG_MASK (1U << SYSCTRL_IRQ_CFG_DIGITAL_RESET_IRQ_CFG_POS)
#define CHCFG_CH1_LSAD_LSAD1_SYSTEM_CHOP_CFG_MASK  (1U << CHCFG_CH1_LSAD_LSAD1_SYSTEM_CHOP_CFG_POS)
#define CHCFG_CH1_ACCUM_CH1_MEASUREMENT_CFG_MASK   (1U << CHCFG_CH1_ACCUM_CH1_MEASUREMENT_CFG_POS)
#define DC_DAC_WE1_CTRL_DAC_WE1_CFG_MASK           (1U << DC_DAC_WE1_CTRL_DAC_WE1_CFG_POS)
#define DC_DAC_RE_CTRL_DAC_RE_CFG_MASK             (1U << DC_DAC_RE_CTRL_DAC_RE_CFG_POS)
#define SYSCTRL_AUX_CFG_DC_DAC_FREQ_CFG_MASK       (uint16_t)(1U << SYSCTRL_AUX_CFG_DC_DAC_FREQ_CFG_POS)

#define ANA_SW_CFG2_ATBUS_SA14_CFG_MASK            (1U << ANA_SW_CFG2_ATBUS_SA14_CFG_POS)
#define ANA_SW_CFG2_ATBUS_SA7_CFG_MASK             (1U << ANA_SW_CFG2_ATBUS_SA7_CFG_POS)

#define ANA_SW_CFG0_CH1_SC109_SC110_CFG_MASK       (1U << ANA_SW_CFG0_CH1_SC109_SC110_CFG_POS)
#define ANA_SW_CFG1_CH2_SC209_SC210_CFG_MASK       (1U << ANA_SW_CFG1_CH2_SC209_SC210_CFG_POS)

#define ANA_SW_CFG0_CH1_SC112_CFG_MASK             (1U << ANA_SW_CFG0_CH1_SC112_CFG_POS)
#define ANA_SW_CFG1_CH2_SC212_CFG_MASK             (1U << ANA_SW_CFG1_CH2_SC212_CFG_POS)

#define SYSCTRL_CMD_ALL_ERR_MASK                   (BUFFER_RESET_CMD | \
                                                    SPI_ERR_CLR_CMD | \
                                                    DIGITAL_RESET_CLR_CMD | \
                                                    VDDA_ERR_CLR_CMD | \
                                                    DC_DAC_LOAD_CLR_CMD | \
                                                    CH2_VIOLATION_CLR_CMD | \
                                                    CH1_VIOLATION_CLR_CMD | \
                                                    CH2_COMPLETION_CLR_CMD | \
                                                    CH1_COMPLETION_CLR_CMD | \
                                                    OTP_STATUS_CLR_CMD | \
                                                    STATE_ERR_CLR_CMD)

/** @} */ /* End of the CEM102_DRIVERg group */

#ifdef  __cplusplus
}
#endif

#endif /* CEM102_COMMON_H_ */

