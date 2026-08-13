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
    RTC_ALARM_Init(RTC_COUNTER_MAX);

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

void RTC_ALARM_Init(uint32_t alarm_duration)
{
    /* Configure and enable clock source for RTC */
    RTC_ClockSource_Init();

    /* Disable RTC, RTC ALARM event, RTC CLOCK event */
    Sys_RTC_Config(RTC_CLK_SRC, (RTC_DISABLE | RTC_DISABLE_ALARM_EVENT |
                                 RTC_DISABLE_CLOCK_EVENT));

    /* Reset the RTC Timer (prescale and counter) */
    Sys_RTC_Config(RTC_CLK_SRC, RTC_RESET);

    /* Set RTC ALARM counter threshold */
    Sys_RTC_Count_Threshold(alarm_duration);

    /* Clear sticky wakeup RTC alarm flag */
    WAKEUP_RTC_ALARM_FLAG_CLEAR();
}

void RTC_ClockSource_Init(void)
{
    if (RTC_CLK_SRC == RTC_CLK_SRC_STANDBY_CLK)
    {
        /* Configure and initialize the system low-power clock */
        Sys_LPClock_Init();
    }
    else if (RTC_CLK_SRC == RTC_CLK_SRC_GPIO3)
    {
        /* Configure GPIO3 as an input pin */
        SYS_GPIO_CONFIG(GPIO3, GPIO_MODE_INPUT);
    }
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

void RTC_ALARM_Set_Timeout(uint32_t timer_counter)
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

    /* Set wanted IRQ only (GPIO3) */
    NVIC_EnableIRQ(GPIO3_IRQn);

    /* Configure GPIO2 as standby clock */
    SYS_GPIO_CONFIG(2, GPIO_2X_DRIVE | GPIO_LPF_DISABLE | GPIO_NO_PULL | NS_CANNOT_USE_GPIO | GPIO_MODE_STANDBYCLK);

    /* Configure GPIO3 interrupt line to rising edge of GPIO2(standby clock) */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_NONE | GPIO_SRC_GPIO_2,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_RISING_EDGE | GPIO_SRC_GPIO_2,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Wait for rising edge of RTC_CLOCK */
    __WFI();

    /* Disable RTC, RTC ALARM event, RTC CLOCK event */
    Sys_RTC_Config(RTC_CLK_SRC, RTC_DISABLE_ALARM_EVENT);

    /* Clear the pending GPIO3 IRQ */
    NVIC_ClearPendingIRQ(GPIO3_IRQn);

    /* Configure GPIO3 interrupt line to falling edge of GPIO2(standby clock) */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_FALLING_EDGE | GPIO_SRC_GPIO_2,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Wait for falling edge of RTC_CLOCK */
    __WFI();

    /* Reset the RTC Timer (prescale and counter) */
    Sys_RTC_Count_Threshold(timer_counter-2);

    /* Clear the pending GPIO3 IRQ */
    NVIC_ClearPendingIRQ(GPIO3_IRQn);

    /* Configure GPIO3 interrupt line to rising edge of GPIO2(standby clock) */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_RISING_EDGE | GPIO_SRC_GPIO_2,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Wait for rising edge of RTC_CLOCK */
    __WFI();

    Sys_RTC_Config(RTC_CLK_SRC, RTC_RESET);

    /* Clear the pending GPIO3 IRQ */
    NVIC_ClearPendingIRQ(GPIO3_IRQn);

    /* Configure GPIO3 interrupt line to falling edge of GPIO2(standby clock) */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_FALLING_EDGE | GPIO_SRC_GPIO_2,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);
    /* Wait for rising edge of RTC_CLOCK */
    __WFI();

    /* Enable RTC, enable RTC ALARM event, disable RTC CLOCK event */
    Sys_RTC_Config(RTC_CLK_SRC, RTC_ENABLE_ALARM_EVENT);

    /* Clear the pending GPIO3_IRQ */
    NVIC_ClearPendingIRQ(GPIO3_IRQn);

    /* Disable GPIO3 interrupt line */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_NONE,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Reset GPIO2 */
    SYS_GPIO_CONFIG(2, GPIO_2X_DRIVE | GPIO_LPF_DISABLE | GPIO_WEAK_PULL_UP | NS_CANNOT_USE_GPIO | GPIO_MODE_DISABLE);

    /* Restore NVIC set enable register */
    NVIC->ISER[0] = nvic_set_enable[0];
    NVIC->ISER[1] = nvic_set_enable[1];

    /* Clear sticky wakeup RTC alarm flag */
    WAKEUP_RTC_ALARM_FLAG_CLEAR();

    /* Unmask interrupt */
    __enable_irq();
}

uint32_t RTC_ALARM_Set_Relative_Timeout(uint32_t timer_counter, uint32_t pre_timer_counter)
{
    /* NVIC set enable registers */
    uint32_t nvic_set_enable[2];
    uint32_t new_counter_thres, cur_timer_counter;

    /* Mask interrupts */
    __disable_irq();

    /* Backup NVIC set enable registers so that we can restore it later */
    nvic_set_enable[0] = NVIC->ISER[0];
    nvic_set_enable[1] = NVIC->ISER[1];

    /* Set NVIC clear enable registers to clear interrupts */
    NVIC->ICER[0] = 0xFFFFFFFFU;
    NVIC->ICER[1] = 0xFFFFFFFFU;

    /* Set wanted IRQ only (GPIO3) */
    NVIC_EnableIRQ(GPIO3_IRQn);

    /* Configure GPIO2 as standby clock */
    SYS_GPIO_CONFIG(2, GPIO_2X_DRIVE | GPIO_LPF_DISABLE | GPIO_NO_PULL | NS_CANNOT_USE_GPIO | GPIO_MODE_STANDBYCLK);

    /* Configure GPIO3 interrupt line to rising edge of GPIO2(standby clock) */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_NONE | GPIO_SRC_GPIO_2,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_RISING_EDGE | GPIO_SRC_GPIO_2,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Wait for rising edge of RTC_CLOCK */
    __WFI();

    /* Clear the pending GPIO3 IRQ */
    NVIC_ClearPendingIRQ(GPIO3_IRQn);

    cur_timer_counter = ACS->RTC_COUNT;

    /* Read back and if different read back again
     * (data can be corrupted as rtc_clock and sysclk are asynchronous) */
    if (cur_timer_counter != ACS->RTC_COUNT)
    {
        cur_timer_counter = ACS->RTC_COUNT;
    }

    /* Calculate new threshold with compensation of elasped time since entering the scheduler */
    if (timer_counter <= (cur_timer_counter - pre_timer_counter))
    {
        new_counter_thres = 0;
    }
    else
    {
        new_counter_thres = timer_counter - (cur_timer_counter - pre_timer_counter);
    }

    /* If next sleep cycle is less than 5ms, set it to the maximum */
    if (new_counter_thres < 164)
    {
        elapsed_carryover = cur_timer_counter - pre_timer_counter;

        new_counter_thres = SCHEDULER_MAX_ARRIVAL_TIME;
    }

    /* Disable RTC, RTC ALARM event, RTC CLOCK event */
    Sys_RTC_Config(RTC_CLK_SRC, RTC_DISABLE_ALARM_EVENT);

    /* Configure GPIO3 interrupt line to falling edge of GPIO2(standby clock) */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_FALLING_EDGE | GPIO_SRC_GPIO_2,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Wait for falling edge of RTC_CLOCK */
    __WFI();

    /* Clear the pending GPIO3 IRQ */
    NVIC_ClearPendingIRQ(GPIO3_IRQn);

    /* Reset the RTC Timer (prescale and counter) */
    Sys_RTC_Count_Threshold(new_counter_thres-2);

    /* Configure GPIO3 interrupt line to rising edge of GPIO2(standby clock) */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_RISING_EDGE | GPIO_SRC_GPIO_2,
            GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Wait for rising edge of RTC_CLOCK */
    __WFI();

    Sys_RTC_Config(RTC_CLK_SRC, RTC_RESET);

    /* Clear the pending GPIO3 IRQ */
    NVIC_ClearPendingIRQ(GPIO3_IRQn);

    /* Configure GPIO3 interrupt line to falling edge of GPIO2(standby clock) */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_FALLING_EDGE | GPIO_SRC_GPIO_2,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);
    /* Wait for rising edge of RTC_CLOCK */
    __WFI();

    /* Clear sticky wakeup RTC alarm flag */
    WAKEUP_RTC_ALARM_FLAG_CLEAR();

    /* Enable RTC, enable RTC ALARM event, disable RTC CLOCK event */
    Sys_RTC_Config(RTC_CLK_SRC, RTC_ENABLE_ALARM_EVENT);

    /* Clear the pending GPIO3_IRQ */
    NVIC_ClearPendingIRQ(GPIO3_IRQn);

    /* Disable GPIO3 interrupt line */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_NONE,
                           GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Reset GPIO2 */
    SYS_GPIO_CONFIG(2, GPIO_2X_DRIVE | GPIO_LPF_DISABLE | GPIO_WEAK_PULL_UP | NS_CANNOT_USE_GPIO | GPIO_MODE_DISABLE);

    /* Restore NVIC set enable register */
    NVIC->ISER[0] = nvic_set_enable[0];
    NVIC->ISER[1] = nvic_set_enable[1];

    /* Unmask interrupt */
    __enable_irq();

    return new_counter_thres;
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

void RTC_Clock_Config_Init()
{

    /* Configure and enable clock source for RTC */
    RTC_ClockSource_Init();

    /* Disable RTC, RTC ALARM event, RTC CLOCK event */
    Sys_RTC_Config(RTC_CLK_SRC, (RTC_DISABLE | RTC_DISABLE_ALARM_EVENT |
                                 RTC_DISABLE_CLOCK_EVENT));

    /* Reset the RTC Timer (prescale and counter) */
    Sys_RTC_Config(RTC_CLK_SRC, RTC_RESET);

    /* Set RTC ALARM counter threshold */
    Sys_RTC_Count_Threshold(RTC_COUNT_THRES_32767);

    /* Set RTC clock source
     * RTC_CLK_SRC_STANDBY_CLK, disable RTC ALARM event, disable RTC CLOCK event *
     * Enable RTC, disable RTC ALARM event, disable RTC CLOCK event */
    Sys_RTC_Config((RTC_CLK_SRC|RTC_CLOCK_1S), (RTC_ENABLE | RTC_DISABLE_ALARM_EVENT |
                                 RTC_DISABLE_CLOCK_EVENT));
}
