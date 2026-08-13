/**
 * @file app.c
 * @brief Main application source file
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

#include <app.h>
#include "app_customss.h"

struct ble_sleep_api_param_tag ble_sleep_api_param =
{
    .app_sleep_request = 1,
    .max_sleep_duration = MAX_SLEEP_DURATION,
    .min_sleep_duration = MIN_SLEEP_DURATION,
};
/* Count number of wake-ups from memory retention sleep */

#define AFE_WAKEUP_WAIT_TIME	2//900				// 900sec = 15min * 60sec

int afe_wakeup_step = 0;
int afe_wakeup_time = 0;
static uint32_t afe_start_time = 0;

uint32_t customss_NTF_tmout = 0;


volatile uint8_t sleep_mode;

/* RTC flags and counters */
volatile uint8_t wakeup_due_to_RTC = 0;

/* Count number of wake-ups from memory retention sleep */
int wakeup_cnt = 0;

/* Mask to get all reset flags from ACS_RESET_STATUS register */
#define ACS_RESET_STATUS_RESET_FLAGS_MASK         ((uint32_t)(0x03FF0000))

/* Mask to get ACS Reset flag from RESET_STATUS_DIG register */
#define RESET_DIG_STATUS_ACS_RESET_FLAGS_MASK     ((uint32_t)(0x00000001))

void BLE_Sleep_Process(void)
{
    if (BLE_Baseband_Is_Awake())
    {
        BLE_Kernel_Process();

        /* Checks for sleep have to be done with interrupt disabled */
        GLOBAL_INT_DISABLE();

        /* Check if processor clock can be gated */
        switch (BLE_Baseband_Sleep(&ble_sleep_api_param))
        {
            case RWIP_DEEP_SLEEP:
            {
                SOC_Sleep(sleep_mode);
                break;
            }

            case RWIP_CPU_SLEEP:
            {
                /* Wait for interrupt */
                __WFI();
                break;
            }

            case RWIP_ACTIVE:
            default:
            {
                /* Some activity is pending; system cannot enter
                 * a low-power mode.
                 */
                break;
            }
        }

        /* Checks for sleep have to be done with interrupt disabled */
        GLOBAL_INT_RESTORE();
    }
    else
    {
        /* Wait in Idle Mode until the baseband is awake.
         * Interrupts will still be processed as they become pending. */
        Sys_PowerModes_IdleUntilBBAwake();
    }
}

int main(void)
{
    /* Disable all existing interrupts, clearing all pending sources */
    DisableAppInterrupts();

    /* Check if reset due to wakeup from sleep mode:
     *   - All reset flags from ACS_RESET_STATUS register are clear, and
     *   - ACS Reset flag from RESET_STATUS_DIG register is set (regardless of
     *     all other flags) */
    if (((ACS->RESET_STATUS & ACS_RESET_STATUS_RESET_FLAGS_MASK) == 0x0) &&
        ((RESET->DIG_STATUS & RESET_DIG_STATUS_ACS_RESET_FLAGS_MASK) == 0x1))
    {
        App_No_Ret_Sleep_Init();

        /* Reinitialize the system after wakeup */
        Sys_PowerModes_WakeupWithReset(&no_retention_sleep_mode_cfg);
    }
    else /* Else: Not wakeup from SLEEP mode */
    {
        /* IMPORTANT: always have this to make sure DEBUG_CATCH_GPIO is not frozen
         * (in case sleep then wake up from FLASH, main() is executed and in here
         * pad retention could be enabled)*/
        /* Disable pad retention */
        ACS_BOOT_CFG->PADS_RETENTION_EN_BYTE = PADS_RETENTION_DISABLE_BYTE;
    }

    /* Change RSL15 Sleep mode Sleep with Memory Retention */
    sleep_mode = SLEEP_WKUP_RAM_MODE;
    measure_status = MEASURE_IDLE;
    calib_state = CALIB_NOT_STARTED;

    irq_valid_flag = 0;

    /* To global value memory allocation and initial */
    app_device_malloc_attach();

    /* To clear minimum threshold sticky bit */
    Sys_Sensor_Disable();

    DeviceInit();

    /* ★ 추가: BB Timer를 살리기 위해 sensor interface 다시 활성화 */
    Sys_Sensor_Enable();			// 26.05.11

    /* Configure RF parameters and initialize the BLE stack */
    BLE_SystemInit();

    /* Initialize Bluetooth Services */
    BatteryServiceServerInit();
    CustomServiceServerInit();

    /* Subscribe application callback handlers to BLE events */
    AppMsgHandlersInit();

    /* Prepare advertising and scan response data (device name + company ID) */
    //PrepareAdvScanData();			// del sodykim

    /* Send a message to the BLE stack requesting a reset.
     * The stack returns a GAPM_CMT_EVT / GAPM_RESET event upon completion.
     * See BLE_ConfigHandler follow what happens next. */
    GAPM_SoftwareReset(); /* Step 1 */

    EnableAppInterrupts();

    /* Initialize CEM102 structure */
    cem102 = &Driver_CEM102;
    CEM102_Startup(cem102);

    /* Initialize VBAT Status */
    vbat_status = VBAT_IDLE;

    /* Create RTC Scheduler tasks and enable RTC Alarm */
 //   Scheduler_Create_Tasks();		// del sodykim

	/*	add sodykim for data & time */
    RTC_Clock_Config_Init();
    afe_start_time = Sys_RTC_Value_Seconds();
    customss_NTF_tmout = afe_start_time;

#if CUSS_ENABLE_TEMPERATURE_CHAR
    /* Initialize the temperature sensor */
    App_TempSensor_Init(trim_error);
#endif /* CUSS_ENABLE_TEMPERATURE_CHAR */

//    afe_wakeup_step = 1;									// CEM102 Calibration Start..

    /* Execute main loop */
    Main_Loop();

    return 0;
}

void Main_Loop(void)
{
    while (1)
    {
        SYS_WATCHDOG_REFRESH();

/**********************************************************************
  *  CEM102 Measurement
  **********************************************************************/
        switch(afe_wakeup_step) {
        	case 0:
        	    if ((Sys_RTC_Value_Seconds() - afe_start_time) >= AFE_WAKEUP_WAIT_TIME)	{   // 15분 경과...
#ifdef SWMTRACE_OUTPUT
	swmLogInfo("15min elapsed, starting CEM102\r\n");
#endif
//					max30123_DAC_power_on();								// DAC POWER ON
					CUSTOMSS_StartTimer();

					Temperature_Cumulative_Initial();						// 15동안 대기를 한 뒤
        	        afe_wakeup_step = 1;									// CEM102 Calibration Start..
        	    }
        		break;

			case 1:
				max30123_measure_Process();
				break;

			default:
				break;

        }


/**********************************************************************
  *  BLE and SLEEP
  **********************************************************************/
        BLE_Sleep_Process();

    }
}
