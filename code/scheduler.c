/**
 * @file  scheduler.c
 * @brief Scheduler source file (free-running RTC / delta 기반)
 *
 * @copyright @parblock
 * Copyright (c) 2023 Semiconductor Components Industries, LLC (d/b/a
 * onsemi), All Rights Reserved
 * @endparblock
 *
 * -------------------------------------------------------------------------
 * 동작 요약
 *   - RTC 를 리셋/정지하지 않고 free-running 으로 둔다(ACS->RTC_SECONDS 연속).
 *   - 경과분은 '절대 카운터의 델타(now - prev_rtc)'로 계산한다.
 *   - 다음 알람은 '절대 목표(now + 남은시간)'로 설정한다.
 *   - prev_rtc 는 Scheduler_Main 과 Scheduler_Set_TaskState 가 공유한다.
 * -------------------------------------------------------------------------
 */
#include "scheduler.h"
#include "app_temper_sensor.h"

/* 온도 측정 주기(초). scheduler.h 로 옮겨도 무방. */
#ifndef TEMPERATURE_MEASURE_TIMEOUT_S
#define TEMPERATURE_MEASURE_TIMEOUT_S   (10u)
#endif

#define DAC_POWER_ON_TIMEOUT_S  		(10u*1u)
#define CHRONO_A_PROCESS_TIMEOUT_S 	 	(60u*1u)
#define APP_AUTHENTICATION_TIMEOUT_S 	(3u)



static scheduler_task scheduler_task_queue[SCHEDULER_TASK_MAX];     /**< Task queue. */
static uint8_t total_scheduled_tasks = 0;                           /**< Number of the registered tasks. */

uint32_t calc_sleep_duration = 0;    /**< Calculated sleep duration in number of cycles */
uint32_t pre_sleep_duration = 0;    /**< Previous sleep duration in number of cycles */
uint32_t prog_sleep_duration = 0;    /**< Programmed sleep duration in number of cycles */

conn_param_info_t conn_param_info[APP_MAX_NB_CON];

/* 직전 RTC 절대 카운터 스냅샷. Scheduler_Main / Scheduler_Set_TaskState 공유 */
static volatile uint32_t prev_rtc = 0;

uint32_t dac_power_time = 0;

/**************************************************************************************
 * @brief Read current RTC timer counter
 * @return current RTC timer counter value
 **************************************************************************************/
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

/**************************************************************************************
 * @brief 절대 카운터 두 값 사이의 경과 사이클(델타)을 wrap 안전하게 계산.
 * @param now  현재 카운터
 * @param prev 직전 카운터
 * @return 경과 사이클
 *
 * 같은 값이면 0(짧은 간격 중복 호출 대비). now < prev 이면 wrap 으로 간주.
 * '+ 1u' 는 [0, RTC_COUNT_MAX] 범위(모듈러 = RTC_COUNT_MAX + 1)의 보정.
 * (RTC_COUNT 가 full 32-bit 이면 RTC_COUNT_MAX = 0xFFFFFFFF 이고 이 식은
 *  uint32 래핑에 의해 (now - prev) 와 동일해져 그대로 정확하다.)
 **************************************************************************************/
static inline uint32_t rtc_delta(uint32_t now, uint32_t prev)
{
    if (now >= prev)    {
        return (now - prev);
    }

    return (uint32_t)(RTC_COUNT_MAX - prev) + now + 1u;
}

/**************************************************************************************
 * If there is a connection that has not reached maximum connection update request,
 * and still has the default connection interval, send the connection parameter
 * update request.
 **************************************************************************************/
void conn_param_update_task(void)
{
    bool is_param_update_required = false;

    for (int i = 0; i < APP_MAX_NB_CON; i++) {
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
    if (!is_param_update_required)	{
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

/**********************************************************************************
 *	온도 태스크 활성/비활성 헬퍼
 **********************************************************************************/
void enable_Temperature_Task(void)
{
    Scheduler_Set_TaskState(APP_TEMPERATURE_TASK_NUM, TASK_BLOCKED);
}

void disable_Temperature_Task(void)
{
    Scheduler_Set_TaskState(APP_TEMPERATURE_TASK_NUM, TASK_SUSPENDED);
}

/**********************************************************************************
 *	AUTHENTICATION TASK 활성/비활성
 **********************************************************************************/
void enable_Authentication_Task(void)
{
    Scheduler_Set_TaskState(APP_AUTHENTICATION_TASK_NUM, TASK_BLOCKED);
}

void disable_Authentication_Task(void)
{
    Scheduler_Set_TaskState(APP_AUTHENTICATION_TASK_NUM, TASK_SUSPENDED);
}

/**********************************************************************************
 *	CHARONO A TASK 활성/비활성
 **********************************************************************************/
void enable_Chrono_A_Task(void)
{
    Scheduler_Set_TaskState(APP_CHRONO_A_TASK_NUM, TASK_BLOCKED);
}

void disable_Chrono_A_Task(void)
{
    Scheduler_Set_TaskState(APP_CHRONO_A_TASK_NUM, TASK_SUSPENDED);
}

/**********************************************************************************
 *	POWER ON & WAIT 15MIN...CEM102 RUN SUSPENSE
 **********************************************************************************/
void cem102_dac_power_on_init(void)
{
	device_info.cem102_dac_power = CEM102_DAC_POWER_OFF;
    Scheduler_Set_TaskState(APP_DAC_POWER_TASK_NUM, TASK_BLOCKED);

    dac_power_time = Sys_RTC_Value_Seconds();
}

void cem102_dac_power_on_task(void)
{
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("lcem102_dac_power_on_task:[%d][%d] --> [%d]\r\n", Sys_RTC_Value_Seconds(), dac_power_time, (Sys_RTC_Value_Seconds()-dac_power_time));
    Sys_Delay(SystemCoreClock / 1000);
#endif

    CUSTOMSS_StartTimer();								// custom timer event start

    enable_Chrono_A_Task();

	device_info.cem102_dac_power = CEM102_DAC_POWER_ON;
	Scheduler_Set_TaskState(APP_DAC_POWER_TASK_NUM, TASK_SUSPENDED);
}







Task_Creation_t Scheduler_Create_NewTask(p_schedular_task_t task, uint32_t arrival_cycles)
{
    Task_Creation_t task_status = TASK_CREATE_ERR_NONE;

    if ((SCHEDULER_MIN_ARRIVAL_TIME > arrival_cycles) ||
        (SCHEDULER_MAX_ARRIVAL_TIME < arrival_cycles))
    {
        /* arrival_cycles 는 0 이 될 수 없다(아래 modulo 나눗셈의 0 除算 방지). */
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
        scheduler_task_queue[total_scheduled_tasks].task_state = TASK_SUSPENDED;
        scheduler_task_queue[total_scheduled_tasks].count_cycles = 0;
        total_scheduled_tasks++;
    }

    return task_status;
}

void Scheduler_Update_CountCycle(uint32_t now)
{
    /* [FIX] 절대 카운터 델타 (같은 값이면 0, wrap 보정 포함) */
    uint32_t elapsed = rtc_delta(now, prev_rtc);

    for (uint8_t i = 0; i < total_scheduled_tasks; i++)
    {
        /* Ignore SUSPENDED tasks. */
        if (TASK_SUSPENDED != scheduler_task_queue[i].task_state)
        {
            uint64_t updated = (uint64_t)scheduler_task_queue[i].count_cycles + elapsed;

            /* If the task period has expired */
            if ((uint64_t)scheduler_task_queue[i].arrival_cycles <= updated)
            {
                /* 나머지를 보존해 누적 드리프트 방지 (arrival_cycles != 0 보장) */
                scheduler_task_queue[i].count_cycles =
                    (uint32_t)(updated % scheduler_task_queue[i].arrival_cycles);
                scheduler_task_queue[i].task_state = TASK_READY;
            }
            else
            {
                scheduler_task_queue[i].count_cycles = (uint32_t)updated;
            }
        }
    }

    /* Save the current cycle for the next wake up delta. */
    prev_rtc = now;
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

/***************************************************************************************
 * @brief 다음에 깨야 할 '상대' 사이클(가장 임박한 BLOCKED 태스크까지)을 반환.
 * @return 남은 사이클. 실행 대기 태스크가 없으면 RTC_COUNT_MAX(sentinel).
 *
 * [FIX] 절대 변환(+ prev_rtc)은 Scheduler_Main 으로 옮겨, "태스크 없음"을
 *       구분할 수 있게 상대값 + sentinel 을 반환한다.
 ***************************************************************************************/
uint32_t Scheduler_Calculate_SleepDuration(void)
{
    uint32_t next_wakeup_time = RTC_COUNT_MAX;   /* sentinel: 없음 */

    for (uint8_t i = 0; i < total_scheduled_tasks; i++)
    {
        if (TASK_BLOCKED == scheduler_task_queue[i].task_state)
        {
            uint32_t task_wakeup_time =
                scheduler_task_queue[i].arrival_cycles - scheduler_task_queue[i].count_cycles;

            if (task_wakeup_time < next_wakeup_time)	{
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
    if (task_state == TASK_SUSPENDED)	{
        scheduler_task_queue[scheduled_task_number].count_cycles = 0;
    }

/*************************************************************************************************
 * If changing the task_state from SUSPENDED to BLOCKED,
 * check if RTC alarm has been disabled. If so, re-arm it.
 * (RTC 카운터 자체는 멈추지 않으므로 RESET/ENABLE 은 하지 않는다.)
 *************************************************************************************************/
    if ((task_state == TASK_BLOCKED) &&
        (scheduler_task_queue[scheduled_task_number].task_state == TASK_SUSPENDED))
    {
        if (RTC_Alarm_Sleep_Check() == RTC_ALARM_DISABLED)
        {
            /* [FIX] free-running 카운터 기준으로 델타 기준점을 새로 잡는다.
             *       (유휴 동안의 시간을 새 태스크에 잘못 누적하지 않도록) */
            prev_rtc = RTC_Timer_Counter_Read();

            /* Clear sticky wakeup RTC alarm flag */
            WAKEUP_RTC_ALARM_FLAG_CLEAR();

            /* 유휴 때 껐던 알람 이벤트만 다시 켠다(카운터는 계속 돌고 있음). */
            Sys_RTC_Config(RTC_CLK_SRC, RTC_ENABLE_ALARM_EVENT);

            /* 첫 웨이크: 현재 + 2ms (절대 목표) */
            RTC_ALARM_Set_Timeout(CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_2MS) + prev_rtc);
        }
    }

    scheduler_task_queue[scheduled_task_number].task_state = task_state;

    /* If task is being suspended, reset the count cycle */
    if (task_state == TASK_SUSPENDED)
    {
        scheduler_task_queue[scheduled_task_number].count_cycles = 0;

        bool active_task_remain = false;
        for (int i = 0; i < APP_TASK_NUM_MAX; i++)
        {
            if (scheduler_task_queue[i].task_state == TASK_BLOCKED)
            {
                active_task_remain = true;
                break;
            }
        }

        if (!active_task_remain)
        {
            Sys_RTC_Config(RTC_CLK_SRC, RTC_DISABLE_ALARM_EVENT);
        }
    }
}

Scheduler_Task_State_t Scheduler_Get_TaskState(uint8_t scheduled_task_number)
{
    return scheduler_task_queue[scheduled_task_number].task_state;
}

void Scheduler_Create_Tasks(void)
{
    /* Create_NewTask 가 TASK_SUSPENDED 상태로 등록한다. */
    Scheduler_Create_NewTask(&APP_BASS_Measure_Task,
                             CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(BATT_MEASURE_TIMEOUT_S)));

    Scheduler_Create_NewTask(&APP_BASS_NTF_Task,
                             CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_M(BATT_UPDATE_TIMEOUT_M)));

    Scheduler_Create_NewTask(&conn_param_update_task,
                            CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(CONN_PARAM_UPDATE_TASK_INTERVAL_S)));

//--------------------------------------------------------------------------------------------------------------------------
    Scheduler_Create_NewTask(&lsad_temperature_measure_handler,											// add sodykim
                             CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(TEMPERATURE_MEASURE_TIMEOUT_S)));

    Scheduler_Create_NewTask(&cem102_dac_power_on_task,													// add sodykim
                             CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(DAC_POWER_ON_TIMEOUT_S)));

    Scheduler_Create_NewTask(&max30123_M1_chrono_A_Timeout_Task,													// add sodykim
                             CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(CHRONO_A_PROCESS_TIMEOUT_S)));

    Scheduler_Create_NewTask(&Authentication_Certify_Task,													// add sodykim
                             CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(APP_AUTHENTICATION_TIMEOUT_S)));


}

/**********************************************************************************************************
 *	1. 델타로 각 태스크 count_cycle 갱신 (도래 시 READY)
 * 	2. READY 태스크 실행 후 BLOCKED 복귀
 * 	3. 다음 최단 웨이크(상대) 계산
 * 	4. 태스크가 있으면 '현재 + 상대'의 절대 목표로 알람 설정
 **********************************************************************************************************/
void Scheduler_Main(void)
{
    uint32_t now = RTC_Timer_Counter_Read();

    /* 델타 기반 count 갱신 (내부에서 prev_rtc 를 now 로 갱신) */
    Scheduler_Update_CountCycle(now);

    /* READY 태스크 실행 */
    Scheduler_Run_ReadyTask();

    /* 다음 웨이크까지 남은 상대 사이클 (없으면 RTC_COUNT_MAX) */
    calc_sleep_duration = Scheduler_Calculate_SleepDuration();

    /* [FIX] 실행 대기 태스크가 있을 때만 알람을 절대 목표로 설정 */
    if (calc_sleep_duration != RTC_COUNT_MAX)	{
        uint32_t target = now + calc_sleep_duration;   /* 절대 목표 */

        /* 부분 폭 카운터 wrap 보정 (full 32-bit 면 RTC_COUNT_MAX+1==0 이라 no-op) */
        if (target > RTC_COUNT_MAX)	{
            target -= (RTC_COUNT_MAX + 1u);
        }

        RTC_ALARM_Set_Timeout(target);
    }
}
