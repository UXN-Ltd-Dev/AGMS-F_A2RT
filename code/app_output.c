/**
 * @file  app_output.c
 * @brief CEM102 application's measurement output.
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

#include <hw.h>
#include <app.h>
#include "app_cem102.h"
#include "app_utils.h"
#include "app_customss.h"

#if APP_MEASURE_ACROSS_IO_INPUTS
#include "math.h"

void APP_NotifyThermistorData(DRIVER_CEM102_t *p_cem102,
                              const CEM102_Device *p_dev,
                              int32_t v_vdda, int32_t v_res)
{
    float v_vdda_f = (float)v_vdda;
    float v_res_f = (float)v_res;

#ifdef SWMTRACE_OUTPUT
    swmLogInfo("\r,,,,,,,,,,Measured resistor voltage(mV):, %d.%03d\r\n",
               v_res / APP_VOLTAGE_MV_TO_UV, v_res % APP_VOLTAGE_MV_TO_UV);
    swmLogInfo("\r,,,,,,,,,,Measured VDDA voltage(mV):, %d.%03d\r\n",
               v_vdda / APP_VOLTAGE_MV_TO_UV, v_vdda % APP_VOLTAGE_MV_TO_UV);
#endif /* SWMTRACE_OUTPUT */

    /* Add voltage across two switches, ST12 and ST22. */
    v_vdda_f += (2 * v_vdda_f * CEM102_ST_SWITCH_RESISTANCE) /
               (1000 * APP_RESISTOR_IMPEDENCE);
    if (v_res != 0)
    {
        /* In Fahrenheit scale */
        float temperature_measured = 0.0;
        float resistance = APP_RESISTOR_IMPEDENCE *
                            ((v_vdda_f / v_res_f) - 1);

        /* To calculate temperature from resistance, we use simplified
         * B-parameter equation, derived from Steinhart-Hart equation
         *
         *   1/T = (1/T0) + (1/B) * log(R/R0) simplified as
         *   T = (B * T0) / (B + (T0 * log(R/R0)))
         */
        temperature_measured = APP_THERMISTOR_B_CONSTANT * APP_T0_VALUE;
        temperature_measured /= (APP_THERMISTOR_B_CONSTANT + (APP_T0_VALUE *
                                 log(resistance / APP_RESISTANCE_AT_25_DEG_C)));
        temperature_measured -= APP_CELCIUS_CONVERSION_FACTOR;



#ifdef SWMTRACE_OUTPUT
        swmLogInfo("\r,,,,,,Resistance (Kilo ohms):, %f, Temperature:(Celsius), %f \r\n",
                   resistance, temperature_measured);
#endif

        /* Convert and store so that the mobile app can pick up. */
        temperature = App_Utils_ConvertFloatToUInt32(temperature_measured);

        device_info.celsius = (int16_t)(temperature_measured*100);			// add sodykim for agms

    }
}
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

#if CEM102_DIAG_FUNCTIONS
static void APP_NotifyDACVoltage(DRIVER_CEM102_t *p_cem102,
                                 const CEM102_Device *p_dev,
                                 CEM102_DAC_TYPE dac,
                                 int32_t v_measured, bool in_diag)
{
    /* RE DAC voltage is not reported to mobile app. That's why it is not
     * stored anywhere. But reported in UART console, when enabled.
     */
    if (!in_diag)
    {
        if (WE2 == dac)
        {
            WE2_voltage = v_measured;
        }
        else if (WE1 == dac)
        {
            WE1_voltage = v_measured;
        }
    }
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("\r,,,,,,,,,,Measured voltage %s (mV):, %d.%03d\r\n",
               CEM102_GetDACName(dac),
               v_measured / APP_VOLTAGE_MV_TO_UV,
               v_measured % APP_VOLTAGE_MV_TO_UV);
#endif /* SWMTRACE_OUTPUT */
}

/* At present, all the current related measurements from the diagnostics
 * are not handled here. Mainly because the caller of this function
 * already printed the value from those measurements in UART console.
 * Nothing else to do here.
 */
void APP_NotifyDiagResult(DRIVER_CEM102_t *p_cem102,
                          const CEM102_Device *p_dev,
                          CEM102_DiagResult *p_result)
{
    switch (p_result->diagmask)
    {
    case CEM102_DIAG_WE1_DAC_VOLT:
    case CEM102_DIAG_WE2_DAC_VOLT:
    case CEM102_DIAG_RE_DAC_VOLT:
        APP_NotifyDACVoltage(p_cem102, p_dev, p_result->dac,
                             p_result->voltage, p_result->in_diag);
        break;
    default:
        break;
    }
}

#endif /* CEM102_DIAG_FUNCTIONS */

void APP_NotifyCurrentData(DRIVER_CEM102_t *p_cem102,
                           const CEM102_Device *p_dev, float we1_curr,
                           float we2_curr)
{
    /* Convert float type of current values to IEEE11073 format integer */
    WE1_current = App_Utils_ConvertFloatToUInt32(we1_curr);
    WE2_current = App_Utils_ConvertFloatToUInt32(we2_curr);

    /* Calculate CEM102 VBAT using SAR ADC */
    p_cem102->CalculateVBAT(&cem102_vbat);

    device_info.we1_current = we1_curr / APP_FEMTO_NANO_COV_FACTOR;
    device_info.we2_current = we2_curr / APP_FEMTO_NANO_COV_FACTOR;
    device_info.vbat_lvl = cem102_vbat;


#ifdef SWMTRACE_OUTPUT
    swmLogInfo("\rMeasured current (nA):, WE1, %.6f, WE2, %.6f, VBAT (mV): %d -->[ %d] \r\n",
              we1_curr / APP_FEMTO_NANO_COV_FACTOR,
              we2_curr / APP_FEMTO_NANO_COV_FACTOR, cem102_vbat, Sys_RTC_Value_Seconds());
#endif /* SWMTRACE_OUTPUT */
}

/* Reports the LSAD channel violation counter values for each channel. */
void APP_NotifyThresholdViolation(DRIVER_CEM102_t *p_cem102,
                                  const CEM102_Device *p_dev, uint32_t ch1,
                                  uint32_t ch2)
{
#ifdef SWMTRACE_OUTPUT
    if ((ch1 + ch2) != 0)
    {
        swmLogInfo("Threshold violation count CH1 = %d, CH2 = %d\r\n",
                   ch1, ch2);
    }
#endif /* SWMTRACE_OUTPUT */
}

