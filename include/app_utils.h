/**
 * @file  app_utils.h
 * @brief Utilities header file
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

#ifndef APP_UTILS_H
#define APP_UTILS_H

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif    /* ifdef __cplusplus */

/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/
/* Numbers for IEE11073 conversion */
#define IEEE11073_POSITIVE_INFINITY         (0x007FFFFEUL)
#define IEEE11073_NEGATIVE_INFINITY         (0x00800002UL)
#define IEEE11073_EXPONENT_MAX              (127)
#define IEEE11073_EXPONENT_MIN              (-128)
#define IEEE11073_MANTISSA_MAX              (23)

/* ---------------------------------------------------------------------------
* Function prototype definitions
* --------------------------------------------------------------------------*/
/**
 * @brief       Convert float type number to uint32.
 */
uint32_t App_Utils_ConvertFloatToUInt32(float data);

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif    /* APP_UTILS_H */

