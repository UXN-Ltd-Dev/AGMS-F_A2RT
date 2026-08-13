/**
 * @file  app_utils.c
 * @brief Source file for utilities
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

#include <math.h>
#include "app.h"
#include "app_utils.h"
#include "app_customss.h"

uint32_t App_Utils_ConvertFloatToUInt32(float data)
{
    uint32_t outputValue = 0;

    if (data > (float)INT32_MAX)
    {
        outputValue = IEEE11073_POSITIVE_INFINITY;
        return outputValue;
    }
    else if (data < (float)INT32_MIN)
    {
        outputValue = IEEE11073_NEGATIVE_INFINITY;
        return outputValue;
    }
    else if (data == 0)
    {
        return outputValue;
    }

    int8_t exponent = 0;
    double mantissa = fabs((double)data);
    int8_t sign = data > 0 ? +1 : -1;

    for (; mantissa < (1 << IEEE11073_MANTISSA_MAX); exponent--)
    {
        mantissa *= 10.0;
    }

    for (; mantissa > (1 << IEEE11073_MANTISSA_MAX); exponent++)
    {
        mantissa /= 10.0;
    }

    if (exponent > IEEE11073_EXPONENT_MAX)
    {
        outputValue = (uint32_t)(sign > 0 ? IEEE11073_POSITIVE_INFINITY : IEEE11073_NEGATIVE_INFINITY);
        return outputValue;
    }

    if (exponent < IEEE11073_EXPONENT_MIN)
    {
        return outputValue;
    }

    int32_t finalMantissa = (int32_t)round(sign * mantissa);
    outputValue = (uint32_t)(((exponent << 24) & 0xFF000000) | ((finalMantissa << 0) & 0x00FFFFFF));

    return outputValue;
}
