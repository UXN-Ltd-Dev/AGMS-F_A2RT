/**
 * @file  scheduler.c
 * @brief Scheduler source file
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
#include "scheduler.h"

static scheduler_task scheduler_task_queue[SCHEDULER_TASK_MAX];     /**< Task queue. */
static uint8_t total_scheduled_tasks = 0;                           /**< Number of the registered tasks. */

uint32_t calc_sleep_duration = 0;    /**< Calculated sleep duration in number of cycles */
uint32_t pre_sleep_duration = 0;    /**< Previous sleep duration in number of cycles */
uint32_t prog_sleep_duration = 0;    /**< Programmed sleep duration in number of cycles */

conn_param_info_t conn_param_info[APP_MAX_NB_CON];

/**
 * @brief Read current RTC timer counter
 * @return current RTC timer counter value
 */
static inline uint32_t RTC_Timer_Counter_Read(void)
{
    uint32_t rtc_timer_count;

    /* Read current rtc timer counter value */
    rtc_timer_count = ACS->RTC_COUNT;

    /* Read back and if different read back again
     * (data can be corrupted as rtc_clock and sysclk are asynchronous) */
    if (rtc_timer_count != ACS->RTC_COUNT)
    {
        rtc_timer_count = ACS->RTC_COUNT;
    }
    return (rtc_timer_count);
}

void conn_param_update_task(void)
{
    bool is_param_update_required = false;

    for (int i = 0; i < APP_MAX_NB_CON; i++)
    {
        /* If there is a connection that has not reached maximum connection update request,
         * and still has the default connection interval, send the connection parameter
         * update request.
         */
        if ((conn_param_info[i].update_attempt < MAX_CON_PARAM_UPDATE_ATTEMPT) &&
            ((conn_param_info[i].con_interval >= MASTER_DEF_MIN_CONN_INT) &&
            (conn_param_info[i].con_interval < MASTER_DEF_MAX_CONN_INT)) &&
            GAPC_IsConnectionActive(i))
        {
            GAPC_ParamUpdateCmd(i, MIN_CONNECTION_INTERVAL, MAX_CONNECTION_INTERVAL,
                                        SLAVE_LATENCY, CONNECTION_SUPERVISION_TIMEOUT,
                                        MIN_CONNECTION_EVENT_DURATION, MAX_CONNECTION_EVENT_DURATION);

            is_param_update_required = true;
            conn_param_info[i].update_attempt++;
        }
    }

    /* If there is no more connection that requires update, suspend the task */
    if (!is_param_update_required)
    {
        Scheduler_Set_TaskState(APP_CONN_PARAM_UDPATE_TASK_NUM, TASK_SUSPENDED);
    }
}

void enable_BASS_Tasks(void)
{
    /* Set BASS Tasks to BLOCKED state and resets the count cycles */
    Scheduler_Set_TaskState(APP_BASS_MEASURE_TASK_NUM, TASK_BLOCKED);
    Scheduler_Set_TaskState(APP_BASS_NTF_TASK_NUM, TASK_BLOCKED);
}

void disable_BASS_Tasks(void)
{
    Scheduler_Set_TaskState(APP_BASS_MEASURE_TASK_NUM, TASK_SUSPENDED);
    Scheduler_Set_TaskState(APP_BASS_NTF_TASK_NUM, TASK_SUSPENDED);
}

Task_Creation_t Scheduler_Create_NewTask(p_schedular_task_t task, uint32_t arrival_cycles)
{
    Task_Creation_t task_status = TASK_CREATE_ERR_NONE;

    if ((SCHEDULER_MIN_ARRIVAL_TIME > arrival_cycles) ||
        (SCHEDULER_MAX_ARRIVAL_TIME < arrival_cycles))
    {
        task_status = TASK_CREATE_ERR_TIME_LIMIT;
    }
    else if (SCHEDULER_TASK_MAX <= total_scheduled_tasks)
    {
        task_status = TASK_CREATE_ERR_COUNT_LIMIT;
    }
    else if (task == NULL)
    {
        task_status = TASK_CREATE_ERR_NULL_PTR;
    }
    else
    {
        scheduler_task_queue[total_scheduled_tasks].task_function = task;
        scheduler_task_queue[total_scheduled_tasks].arrival_cycles = arrival_cycles;
        scheduler_task_queue[total_scheduled_tasks].task_state = TASK_BLOCKED;
        scheduler_task_queue[total_scheduled_tasks].count_cycles = 0;
        total_scheduled_tasks++;
    }

    return task_status;
}

void Scheduler_Update_CountCycle(uint32_t wakeup_cycle_count)
{
    /* Update count cycle for all task/s which are not SUSPENDED */
    for (uint8_t i = 0; i < total_scheduled_tasks; i++)
    {
        /* Ignore SUSPENDED tasks. */
        if (TASK_SUSPENDED != scheduler_task_queue[i].task_state)
        {
            scheduler_task_queue[i].count_cycles += wakeup_cycle_count;
        }
    }

    /* Update task to READY */
    for (uint8_t i = 0; i < total_scheduled_tasks; i++)
    {
        /* Ignore SUSPENDED tasks. */
        if (TASK_SUSPENDED != scheduler_task_queue[i].task_state)
        {
            /* Put it into READY state. */
            if (scheduler_task_queue[i].arrival_cycles <= scheduler_task_queue[i].count_cycles)
            {
                scheduler_task_queue[i].count_cycles = 0;
                scheduler_task_queue[i].task_state  = TASK_READY;
            }
        }
    }
}

void Scheduler_Run_ReadyTask(void)
{
    for (uint8_t i = 0; i < total_scheduled_tasks; i++)
    {
        /* If it is ready, then call it.*/
        if (TASK_READY == scheduler_task_queue[i].task_state)
        {
            scheduler_task_queue[i].task_state = TASK_BLOCKED;

            if (scheduler_task_queue[i].task_function)
            {
                scheduler_task_queue[i].task_function();
            }
        }
    }
}

uint32_t Scheduler_Calculate_SleepDuration(void)
{
    /* Next wake up time should be burst_time - current cycle_count */
    uint32_t next_wakeup_time = SCHEDULER_MAX_ARRIVAL_TIME;
    uint32_t task_wakeup_time = SCHEDULER_MAX_ARRIVAL_TIME;

    for (uint8_t i = 0; i < total_scheduled_tasks; i++)
    {
        if (TASK_BLOCKED == scheduler_task_queue[i].task_state)
        {
            task_wakeup_time = scheduler_task_queue[i].arrival_cycles - scheduler_task_queue[i].count_cycles;

            if (task_wakeup_time <= next_wakeup_time)
            {
                next_wakeup_time = task_wakeup_time;
            }
        }
    }

    return next_wakeup_time;
}

void Scheduler_Set_ArrivalCycle(uint8_t scheduled_task_number, uint32_t arrival_cycle)
{
    scheduler_task_queue[scheduled_task_number].arrival_cycles = arrival_cycle;
}

void Scheduler_Set_TaskState(uint8_t scheduled_task_number, Scheduler_Task_State_t task_state)
{
    /* If task is being suspended, reset the count cycle */
    if (task_state == TASK_SUSPENDED)
    {
        scheduler_task_queue[scheduled_task_number].count_cycles = 0;
    }

    /* If changing the task_state from SUSPENDED to BLOCKED,
     * check if RTC has been stopped. If so, re-configure RTC.
     * Otherwise, just change the state. */
    if ((task_state == TASK_BLOCKED) &&
        (scheduler_task_queue[scheduled_task_number].task_state == TASK_SUSPENDED))
    {
        if (RTC_Alarm_Sleep_Check() == RTC_ALARM_DISABLED)
        {
            /* Clear sticky wakeup RTC alarm flag */
            WAKEUP_RTC_ALARM_FLAG_CLEAR();

            /* Re-enable RTC */
            Sys_RTC_Config(RTC_CLK_SRC, RTC_RESET);
            Sys_RTC_Config(RTC_CLK_SRC, RTC_ENABLE | RTC_ENABLE_ALARM_EVENT);

            /* Set the first timeout for to start RTC Scheduler */
            RTC_ALARM_Set_Timeout(CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_10MS));
        }
    }

    scheduler_task_queue[scheduled_task_number].task_state = task_state;

    /* If task is being suspended, reset the count cycle */
    if (task_state == TASK_SUSPENDED)
    {
        scheduler_task_queue[scheduled_task_number].count_cycles = 0;

        bool active_task_remain = false;
        for (int i=0; i< APP_TASK_NUM_MAX; i++)
        {
            if (scheduler_task_queue[i].task_state == TASK_BLOCKED)
            {
                active_task_remain = true;
                break;
            }
        }

        if (!active_task_remain)
        {

            /* Disable RTC, RTC ALARM event, RTC CLOCK event, and resets counter. */
            Sys_RTC_Config(RTC_CLK_SRC, (RTC_DISABLE_ALARM_EVENT |
                                         RTC_DISABLE_CLOCK_EVENT |
                                         RTC_DISABLE));
            Sys_RTC_Config(RTC_CLK_SRC, RTC_RESET);
        }
    }
}

Scheduler_Task_State_t Scheduler_Get_TaskState(uint8_t scheduled_task_number)
{
    return scheduler_task_queue[scheduled_task_number].task_state;
}

void Scheduler_Create_Tasks(void)
{
    Scheduler_Create_NewTask(&APP_BASS_Measure_Task,
                             CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(BATT_MEASURE_TIMEOUT_S)));
    Scheduler_Set_TaskState(APP_BASS_MEASURE_TASK_NUM, TASK_SUSPENDED);

    Scheduler_Create_NewTask(&APP_BASS_NTF_Task,
                             CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_M(BATT_UPDATE_TIMEOUT_M)));
    Scheduler_Set_TaskState(APP_BASS_NTF_TASK_NUM, TASK_SUSPENDED);

    Scheduler_Create_NewTask(&conn_param_update_task,
                            CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(CONN_PARAM_UPDATE_TASK_INTERVAL_S)));
    Scheduler_Set_TaskState(APP_CONN_PARAM_UDPATE_TASK_NUM, TASK_SUSPENDED);
}

void Scheduler_Main(void)
{
    /*
     * 1. Update count_cycle (check if task_period <= count_cycle, change it to READY
     * 2. Run ready state and set it back to blocked
     * 3. Calculate the next smallest wakeup time
     * 4. Check if we are allowed to go to sleep. If not, serve BLE process and
     *    calculates the wakeup time again.
     */

    uint32_t elapsed_duration;

    elapsed_duration = RTC_Timer_Counter_Read();

    /* Update count cycle of each task */
    Scheduler_Update_CountCycle(elapsed_duration);

    /* Run tasks which are in READY state */
    Scheduler_Run_ReadyTask();

    /* Calculate sleep time for next task need to execute.
     * If there is no task to be scheduled, calc_sleep_duration will be
     * configured as SCHEDULER_MAX_ARRIVAL_TIME */
    calc_sleep_duration = Scheduler_Calculate_SleepDuration();

    /* If calc_sleep_duration is not SCHEDULER_MAX_ARRIVAL_TIME,
     * proceed to the calculation of next wake-up.
     * Otherwise, stop the RTC Timer */
    if (calc_sleep_duration != SCHEDULER_MAX_ARRIVAL_TIME)
    {
        /* Re-configure RTC wakeup time before entering sleep */
        /* If new wakeup time is smaller than 5 ms, it will be set to MAX */
        prog_sleep_duration = RTC_ALARM_Set_Relative_Timeout(calc_sleep_duration, elapsed_duration);

        /* Check if RTC have enough time to sleep */
        while (prog_sleep_duration == SCHEDULER_MAX_ARRIVAL_TIME)
        {
            if (BLE_Baseband_Is_Awake())
            {
                BLE_Kernel_Process();
            }

            /* Update count cycles with elapsed RTC cycles since last read */
            elapsed_duration = RTC_Timer_Counter_Read();
            Scheduler_Update_CountCycle(elapsed_duration + elapsed_carryover);
            elapsed_carryover = 0;

            /* Run tasks which are in READY state */
            Scheduler_Run_ReadyTask();

            /* Calculate sleep time for next task need to execute  */
            calc_sleep_duration = Scheduler_Calculate_SleepDuration();

            /* Re-configure RTC wakeup time before entering sleep */
            prog_sleep_duration = RTC_ALARM_Set_Relative_Timeout(calc_sleep_duration, elapsed_duration);
        }
    }
    else
    {
        /* Disable RTC, RTC ALARM event, RTC CLOCK event */
        Sys_RTC_Config(RTC_CLK_SRC, (RTC_DISABLE_ALARM_EVENT |
                                     RTC_DISABLE_CLOCK_EVENT |
                                     RTC_DISABLE));
    }
}

