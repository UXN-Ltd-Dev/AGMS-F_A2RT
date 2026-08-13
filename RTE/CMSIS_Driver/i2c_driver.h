/**
 * @file i2c_driver.h
 * @brief I2C CMSIS Driver header
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

#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif /* ifdef __cplusplus */

/* ----------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------- */

/* Device and library headers */
#include <RTE_Device.h>

/* CMSIS I2C driver header */
#include "Driver_I2C.h"

/* ----------------------------------------------------------------------------
 * Symbolic Constants
 * ------------------------------------------------------------------------- */

/* Constants to improve readability when using the CMSIS defined ARM_I2C_STATUS
 * structure */
#define ARM_I2C_STATUS_MODE_MASTER      (1U)
#define ARM_I2C_STATUS_MODE_SLAVE       (0U)
#define ARM_I2C_STATUS_DIRECTION_TX     (0U)
#define ARM_I2C_STATUS_DIRECTION_RX     (1U)

/* ----------------------------------------------------------------------------
 * Driver Control Blocks
 * ------------------------------------------------------------------------- */

#if RTE_I2C0_ENABLED
/* Make the I2C0 Driver Control Block available externally */
extern ARM_DRIVER_I2C Driver_I2C0;
#endif    /* if RTE_I2C0_ENABLED */

#if RTE_I2C1_ENABLED
/* Make the I2C1 Driver Control Block available externally */
extern ARM_DRIVER_I2C Driver_I2C1;
#endif    /* if RTE_I2C1_ENABLED */

/* ----------------------------------------------------------------------------
 * IRQ Handler Prototypes
 * ------------------------------------------------------------------------- */

#if RTE_I2C0_ENABLED
/* I2C0 interrupt handler function */
void I2C0_IRQHandler(void);
#endif    /* if RTE_I2C0_ENABLED */

#if RTE_I2C1_ENABLED
/* I2C1 interrupt handler function */
void I2C1_IRQHandler(void);
#endif    /* if RTE_I2C1_ENABLED */

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif /* ifdef __cplusplus */

#endif    /* I2C_DRIVER_H */
