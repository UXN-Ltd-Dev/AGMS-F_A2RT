/**
 * @file  wakeup_source_config.c
 * @brief Wakeup source configuration file
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

#include "app.h"
#include "wakeup_source_config.h"
#include "rtc.h"
#include "sensor.h"
#include <lowpower_clock.h>
#include "app_customss.h"

volatile uint32_t elapsed_carryover = 0;

void Wakeup_Source_Config(void)
{
    RTC_ALARM_Init();

    /* Configure and enable GPIO wakeup source */
    GPIO_Wakeup_Init();

    /* Disable NVIC GPIO1 interrupt if the GPIO1 is used as wakeup source
     * GPIO1 NVIC IRQ is recommended to disable if GPIO1 is used as wakeup
     * source. WAKEUP_IRQn is used to capture GPIO1 wakeup event */
    NVIC_DisableIRQ(GPIO1_IRQn);
    NVIC_DisableIRQ(GPIO0_IRQn);

    /* Clear all wakeup flags */
    WAKEUP_FLAGS_CLEAR();

    /* Clear NVIC Wakeup interrupt IRQ */
    NVIC_ClearPendingIRQ(WAKEUP_IRQn);

    /* Enable the Wakeup interrupt */
    NVIC_EnableIRQ(WAKEUP_IRQn);
}

void RTC_ALARM_Init(void)
{
    /* Configure and initialize the system low-power clock */
    Sys_LPClock_Init();

    /* Disable RTC, RTC ALARM event, RTC CLOCK event */
    Sys_RTC_Config(RTC_CLK_SRC, (RTC_DISABLE | RTC_DISABLE_ALARM_EVENT |
                                 RTC_DISABLE_CLOCK_EVENT));

    /* Reset the RTC Timer (prescale and counter) */
    Sys_RTC_Config(RTC_CLK_SRC, RTC_RESET);

    /* Enable RTC Clock */
    Sys_RTC_Config(RTC_CLK_SRC, RTC_ENABLE);

    /* Clear sticky wakeup RTC alarm flag */
    WAKEUP_RTC_ALARM_FLAG_CLEAR();
}

void GPIO_Wakeup_Init(void)
{
    SYS_GPIO_CONFIG(GPIO_WAKEUP_PIN, (GPIO_MODE_DISABLE | GPIO_LPF_DISABLE |
                                      GPIO_WEAK_PULL_DOWN  | GPIO_6X_DRIVE));
#if (BUTTON_MANAGER_ENABLED == 0)
    SYS_GPIO_CONFIG(GPIO_NO_RET_SLEEP_PIN, (GPIO_MODE_DISABLE | GPIO_LPF_DISABLE |
                                            GPIO_WEAK_PULL_UP | GPIO_6X_DRIVE));
#endif
}

void RTC_ALARM_Set_Timeout(uint32_t new_threshold)
{
    /* NVIC set enable registers */
    uint32_t nvic_set_enable[2];

    /* Mask interrupts */
    __disable_irq();

    /* Backup NVIC set enable registers so that we can restore it later */
    nvic_set_enable[0] = NVIC->ISER[0];
    nvic_set_enable[1] = NVIC->ISER[1];

    /* Set NVIC clear enable registers to clear interrupts */
    NVIC->ICER[0] = 0xFFFFFFFFU;
    NVIC->ICER[1] = 0xFFFFFFFFU;

    Sys_ACS_WriteRegister(&ACS->RTC_CTRL, RTC_DISABLE_ALARM_EVENT);

    /* Clear sticky wakeup RTC alarm flag */
    WAKEUP_RTC_ALARM_FLAG_CLEAR();

    /* Reset the RTC Timer (prescale and counter) */
    Sys_RTC_Count_Threshold(new_threshold);

    Sys_ACS_WriteRegister(&ACS->RTC_CTRL, RTC_ENABLE_ALARM_EVENT);

    /* Restore NVIC set enable register */
    NVIC->ISER[0] = nvic_set_enable[0];
    NVIC->ISER[1] = nvic_set_enable[1];

    /* Unmask interrupt */
    __enable_irq();
}

RTC_Alarm_Sleep_t RTC_Alarm_Sleep_Check(void)
{
    RTC_Alarm_Sleep_t rtc_alarm_status = RTC_ALARM_SLEEP_OK;
    uint32_t next_sleep;

    /* Check if RTC Alarm is Disabled */
    if (((ACS->RTC_CTRL) & (RTC_ALARM_EVENT_IS_ENABLED)) == 0x0)
    {
        rtc_alarm_status = RTC_ALARM_DISABLED;
    }
    /* Check if RTC Count already reached the threshold */
    else if ((ACS->RTC_COUNT_THRES) <= (ACS->RTC_COUNT))
    {
        rtc_alarm_status = RTC_ALARM_SLEEP_FIRED;
        wakeup_due_to_RTC = 1;
    }
    else
    {
        next_sleep = (ACS->RTC_COUNT_THRES) - (ACS->RTC_COUNT);

        /* Check if RTC Alarm is about to fire in next 2 ms */
        if (next_sleep <= CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_2MS))
        {
            rtc_alarm_status = RTC_ALARM_SLEEP_NOT_ALLOWED;
        }
    }

    return rtc_alarm_status;
}

