/**
 * @file    app_temperature_sensor.h
 * @brief   Header file for the internal temperature sensor interface.
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

#ifndef APP_TEMPERATURE_SENSOR_H
#define APP_TEMPERATURE_SENSOR_H

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

/* Application headers */

/* ----------------------------------------------------------------------------
 * Symbolic Constants
 * ------------------------------------------------------------------------- */

#define LSAD_VBAT_CHANNEL				(LSAD_INPUT_CH1)

/**
 * @brief       The LSAD channel used for the temperature sensor
 * @details     This LSAD channel is used to retrieve LSAD measurements taken
 *              for the internal temperature sensor.
 */
#define LSAD_TEMP_CHANNEL      	(LSAD_INPUT_CH0)


/**
 * @brief       The default temperature gain factor
 * @details     This value is used as the default gain factor, for
 *              converting an LSAD measurement into a temperature, when the
 *              measured trim values are found to be invalid (i.e. equal to
 *              0xFFFF). This scaler is in units of LSB per degree Celsius.
 */
#define DEFAULT_TEMPERATURE_GAIN        (21.3f)

/**
 * @brief       The default temperature offset factor
 * @details     This value is used as the default offset factor, for
 *              converting an LSAD measurement into a temperature, when the
 *              measured trim values are found to be invalid (i.e. equal to
 *              0xFFFF). This offset in in units of degrees Celsius.
 */
#define DEFAULT_TEMPERATURE_OFFSET      (334.0f)

/* ----------------------------------------------------------------------------
 * Macros
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Type Definitions
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Global Variables
 * ------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * Function Prototypes
 * ------------------------------------------------------------------------- */

/**
 * @brief       Retrieve an internal temperature measurement
 * @details     Computes the temperature using the LSAD input and the
 *              conversion factors. The return value is encoded as an
 *              IEEE-11073 encoded 32-bit floating point value where the first
 *              8 bits are the exponent and the last 24 bits are the mantissa.
 * @param       N/A
 * @return      uint32_t
 *                  The temperature in degrees Celsius.
 */
uint32_t App_TempSensor_Measure(void);

/**
 * @brief       Initialize the temperature sensor
 * @details     Configures the LSAD channel for the internal temperature
 *              sensor, initialized the conversion offset and scaler, and
 *              enables the sensor.
 * @param       uint32_t trim_error
 *                  A code indicating if any trim errors occurred during
 *                  initialization.
 * @return      N/A
 */
void App_TempSensor_Init(uint32_t trim_error);


void LSAD_measure_sensor_level(void);

void Temperature_Cumulative_Initial(void);

void Temperature_Cumulative_Average(uint16_t now_Temp);


/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif /* ifdef __cplusplus */

#endif /* APP_TEMPERATURE_SENSOR_H */

