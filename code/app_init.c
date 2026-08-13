/**
 * @file  app_init.c
 * @brief Source file for application initialization
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

#include <app.h>
#include "ble_bass.h"
#include <lowpower_clock.h>
#include "app_customss.h"
#include "app_button.h"

uint32_t trim_error;

#ifdef SWMTRACE_OUTPUT
static uint32_t traceOptions[] = {
    SWM_LOG_LEVEL_INFO,             /* In all cases log info messages */
    SWM_UART_RX_PIN | UART_RX_GPIO, /* Set RX pin for cases when using UART */
    SWM_UART_TX_PIN | UART_TX_GPIO, /* Set TX pin for cases when using UART */
    SWM_UART_RX_ENABLE,             /* Enable the UART Rx Interrupts */
    SWM_UART_BAUD_RATE | UART_BAUD  /* Set Baud rate */
};
#endif

/**
 * @brief       Application BLE low-power configuration
 */
struct ble_device_parameter local_ble_dev_params =
{
    .low_pwr_clk_accuracy = LOW_POWER_CLOCK_ACCURACY,
    .twosc = TWOSC,
    .rf_tx_power_dbm = DEF_TX_POWER,
#if defined(LPCLOCK_SRC_XTAL32K) && defined(LPCLOCK_SRC_RC32K)
    .low_pwr_clk =
    {
        .low_pwr_clk_xtal32 = LPCLOCK_SRC_XTAL32K,
        .low_pwr_clk_rc32 = LPCLOCK_SRC_RC32K,
        .low_pwr_standby_clk_src = LPCLOCK_SRC
    }
#endif /* if defined(LPCLOCK_SRC_XTAL32K) && defined(LPCLOCK_SRC_RC32K) */
};

void BLE_SystemInit(void)
{
    /* Set radio output power of RF */
    int tx_power_status = Sys_RFFE_SetTXPower(
        DEF_TX_POWER,
        LSAD_TXPWR_DEF,
        VDDPA_EN
    );
    int8_t tx_power_level_dbm = Sys_RFFE_GetTXPower(LSAD_TXPWR_DEF);

    /* If the actual set TX power is not within the accepted range
     * (i.e., the desired value +/- 1 dBm), hold the application here and prevent
     * it from executing further. */
    if ((tx_power_level_dbm < DEF_TX_POWER - 1)
        || (tx_power_level_dbm > DEF_TX_POWER + 1))
    {
        while (1)
        {
            __WFI();
        }
    }

    /* Check if VCC was too low to achieve the target TX power */
    if (tx_power_status != ERRNO_NO_ERROR)
    {
        while (1)
        {
            /* Wait for watchdog reset! */
            __WFI();
        }
    }

    /* Configure Baseband Controller Interface */
    BBIF->CTRL = (BB_CLK_ENABLE | BBCLK_DIVIDER_8);

    /* Set BB timer not reset bit */
    Sys_ACS_WriteRegister(&ACS->BB_TIMER_CTRL, BB_TIMER_NRESET);

    /* Initialize BLE stack */
    BLEStackInit();
}

void DeviceInit(void)
{
    /* Debug Catch Mode
     * If DEBUG_CATCH_GPIO is low, enter Debug Catch Mode which holds the program execution in a
     * while loop to make it easier to connect to the debugger.
     * We suggest retaining this feature during development.
     */

    SYS_GPIO_CONFIG(DEBUG_CATCH_GPIO, (GPIO_MODE_GPIO_IN | GPIO_LPF_DISABLE |
                                    GPIO_WEAK_PULL_UP  | GPIO_6X_DRIVE));

    while ((Sys_GPIO_Read(DEBUG_CATCH_GPIO)) == 0)
    {
        SYS_WATCHDOG_REFRESH();
    }

    /* Load default trim values. */
    trim_error = SYS_TRIM_LOAD_DEFAULT();

    if ((trim_error & ERROR_INVALID_CRC) ||
        (trim_error & ERROR_BG_INVALID))
    {
        while (1)
        {
            /* Critical trim errors detected, halt program, put
             * core in low power state to conserve power. */
            __WFI();
        }
    }


    /* Set the VCC voltage regulator to buck converter or LDO mode.
     * Incorporated the changes for SWMCHICO-391 */
    Sys_Power_SetVCCRegulator(VCC_BUCK_ENABLE);

#if VCC_BUCK_ENABLE
    /* Set the inductor charge current (ICH_TRIM) for optimum RF performance */
    Sys_ACS_WriteRegister(
        &ACS->VCC_CTRL,
        ((ACS->VCC_CTRL & (~ACS_VCC_CTRL_ICH_TRIM_Mask)) | VCC_ICHTRIM_DEFAULT)
    );
#endif /* if VCC_BUCK_ENABLE */

    /* Configure and initialize system clock */
    App_Clock_Config();

    /* Set all the GPIOs to a known state to minimize the leakage current from GPIO pins */
    for (uint8_t i = 0; i < GPIO_PAD_COUNT; i++)
    {
        SYS_GPIO_CONFIG(i, (GPIO_MODE_DISABLE | GPIO_WEAK_PULL_UP
                           | GPIO_LPF_DISABLE | GPIO_6X_DRIVE));
    }

    /* Configure GPIOs */
    App_GPIO_Config();

    /* Load custom trim values */
    uint32_t custom_trim_error __attribute__((unused)) = SYS_TRIM_LOAD_CUSTOM();


#if DEBUG_SLEEP_GPIO == DEBUG_SYSCLK_PWR_MODE
    /* Clear POWER_MODE_GPIO to indicate wakeup */
    Sys_GPIO_Set_Low(POWER_MODE_GPIO);
#endif

#ifdef VOLTAGES_CALIB_VERIFY

    /* Hold here to verify calibrated voltages */
    while (true)
    {
        SYS_WATCHDOG_REFRESH();
    }
#endif    /* ifdef VOLTAGES_CALIB_VERIFY */

#if (CHARGE_PUMP_CFG == CHARGE_PUMP_ENABLE)
    Sys_ACS_WriteRegister(&ACS->VDDCP_CTRL, VDDCP_CPCLK_4KHZ | VDDCP_PTRIM_4MA | VDDCP_READY | VDDCP_COMP_ENABLED);
    Sys_ACS_WriteRegister(&ACS->SLEEP_MODE_CFG, ACS->SLEEP_MODE_CFG & ~VDDCP_ENABLE_IN_SLEEP);
#endif
#if (CHARGE_PUMP_CFG == CHARGE_PUMP_ALWAYS_ENABLE)
    Sys_ACS_WriteRegister(&ACS->VDDCP_CTRL, VDDCP_CPCLK_8KHZ | VDDCP_PTRIM_16MA | VDDCP_READY | VDDCP_COMP_ENABLED);
    Sys_ACS_WriteRegister(&ACS->SLEEP_MODE_CFG, BG_ENABLE_IN_SLEEP | VCC_ENABLE_IN_SLEEP | VDDCP_ENABLE_IN_SLEEP);
#endif

    /* If applicable, power down the following blocks to lower the power    */
    /* consumption. For details, see the Power Reduction Defines in app.h.  */
#if SENSOR_POWER_DISABLE
    /* Care should be taken when disabling the sensor
     * interface. Disabling the sensor interface also
     * powers down the BB timer. */
    Sys_Sensor_Disable();
#endif
#if CC312AO_POWER_DISABLE
    Sys_Power_CC312_Disable();
#endif
#if POWER_DOWN_FPU
    Power_Down_FPU();
#endif

    /* Sleep Initialization for Power Mode */
    App_No_Ret_Sleep_Init();
    App_Mem_Ret_Sleep_Init();

    /* Enable the wakeup source configuration */
    Wakeup_Source_Config();

#if (BUTTON_MANAGER_ENABLED == 1)
    /* Configure the button interface */
    App_Button_Config();
#endif

    /* Clear reset flags */
    RESET->DIG_STATUS = (uint32_t) RESET_DIG_STATUS_CLEAR;
    Sys_ACS_WriteRegister(&ACS->RESET_STATUS, (uint32_t) ACS_RESET_STATUS_CLEAR);

    APP_BASS_ReadBattLevelInit(trim_error);

#ifdef SWMTRACE_OUTPUT
    Init_SWMTrace();
#endif /* SWMTRACE_OUTPUT */
}

void AppMsgHandlersInit(void)
{
    /* BLE Database setup handler */
    MsgHandler_Add(GAPM_CMP_EVT, BLE_ConfigHandler);
    MsgHandler_Add(GAPM_PROFILE_ADDED_IND, BLE_ConfigHandler);
    MsgHandler_Add(GATTM_ADD_SVC_RSP, BLE_ConfigHandler);

    /* BLE Activity handler (responsible for air operations) */
    MsgHandler_Add(GAPM_CMP_EVT, BLE_ActivityHandler);
    MsgHandler_Add(GAPM_ACTIVITY_CREATED_IND, BLE_ActivityHandler);
    MsgHandler_Add(GAPM_ACTIVITY_STOPPED_IND, BLE_ActivityHandler);

    /* Connection handler */
    MsgHandler_Add(GAPM_CMP_EVT, BLE_ConnectionHandler);
    MsgHandler_Add(GAPC_CMP_EVT, BLE_ConnectionHandler);
    MsgHandler_Add(GAPC_CONNECTION_REQ_IND, BLE_ConnectionHandler);
    MsgHandler_Add(GAPC_DISCONNECT_IND, BLE_ConnectionHandler);
    MsgHandler_Add(GAPM_ADDR_SOLVED_IND, BLE_ConnectionHandler);
    MsgHandler_Add(GAPC_GET_DEV_INFO_REQ_IND, BLE_ConnectionHandler);
    MsgHandler_Add(GAPC_PARAM_UPDATE_REQ_IND, BLE_ConnectionHandler);
    MsgHandler_Add(GAPC_PARAM_UPDATED_IND, BLE_ConnectionHandler);

    /* Pairing / bonding  handler */
    MsgHandler_Add(GAPC_BOND_REQ_IND, BLE_PairingHandler);
    MsgHandler_Add(GAPC_BOND_IND, BLE_PairingHandler);
    MsgHandler_Add(GAPC_ENCRYPT_REQ_IND, BLE_PairingHandler);
    MsgHandler_Add(GAPC_ENCRYPT_IND, BLE_PairingHandler);

    /* UXN   handler */
    MsgHandler_Add(APP_CUSS_PARAM_UPDATE_REQ, ParamUpdateHandler);

}

void BatteryServiceServerInit(void)
{
    /* Passing 1 as the number of battery instances since the
     * ble_peripheral_server sample code is only designed to work with 1
     * battery instance, and the APP_BASS_ReadBattLevel function as the
     * callback function that will be called in the BLE abstraction layer
     * to return the battery level.
     */
    BASS_Initialize(APP_BAS_NB, APP_BASS_ReadBattLevel);

    /* Set timeout to 0 so that the kernel timer does not fire */
//    BASS_NotifyOnBattLevelChange(0);
//    BASS_NotifyOnTimeout(0);
}

void CustomServiceServerInit(void)
{
    CUSTOMSS_Initialize();

}

void IRQPriorityInit(void)
{
    uint8_t interrupt;

    /* Iterate through all external interrupts excluding WAKEUP_IRQn */
    for (interrupt = WAKEUP_IRQn; interrupt <= NVIC_LAST_VECTOR; ++interrupt)
    {
        /* If the interrupt is non-BLE, set priority to 1 (lower than
         * the default priority of 0). This ensures BLE stability. */
        if (interrupt < BLE_SW_IRQn || interrupt > BLE_ERROR_IRQn)
        {
            /* Retrieve the current interrupt priority level */
            uint32_t priority = NVIC_GetPriority((IRQn_Type)interrupt);

            /* Lower the priority by 1 */
            NVIC_SetPriority((IRQn_Type)interrupt, (priority + 1));
        }
    }
}

void BLEStackInit(void)
{
    uint8_t param_ptr;

    /* Consider the user-defined device parameters for the BLE stack */
    Device_BLE_Param_Set(local_ble_dev_params);

    BLE_Initialize(&param_ptr);

    /* BLE_Initialize() initialized a number of trim registers
     * using default values from in the BLE stack,
     * SYS_TRIM_LOAD_CUSTOM() ensures custom trim values are used. */
    uint32_t custom_trim_error __attribute__((unused)) = SYS_TRIM_LOAD_CUSTOM();

    ke_task_create(TASK_APP, MsgHandler_GetTaskAppDesc());
    Device_BLE_Public_Address_Read((uint32_t)APP_BLE_PUBLIC_ADDR_LOC);

    IRQPriorityInit();
}

void DisableAppInterrupts(void)
{
    Sys_NVIC_DisableAllInt();
    Sys_NVIC_ClearAllPendingInt();
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);
    __set_FAULTMASK(FAULTMASK_DISABLE_INTERRUPTS);
}

void EnableAppInterrupts(void)
{
    NVIC_ClearPendingIRQ(BLE_HSLOT_IRQn);
    NVIC_ClearPendingIRQ(BLE_SLP_IRQn);
    NVIC_ClearPendingIRQ(BLE_FIFO_IRQn);
    NVIC_ClearPendingIRQ(BLE_CRYPT_IRQn);
    NVIC_ClearPendingIRQ(BLE_ERROR_IRQn);
    NVIC_ClearPendingIRQ(BLE_TIMESTAMP_TGT1_IRQn);
    NVIC_ClearPendingIRQ(BLE_FINETGT_IRQn);
    NVIC_ClearPendingIRQ(BLE_TIMESTAMP_TGT2_IRQn);
    NVIC_ClearPendingIRQ(BLE_SW_IRQn);

    NVIC_EnableIRQ(BLE_HSLOT_IRQn);
    NVIC_EnableIRQ(BLE_SLP_IRQn);
    NVIC_EnableIRQ(BLE_FIFO_IRQn);
    NVIC_EnableIRQ(BLE_CRYPT_IRQn);
    NVIC_EnableIRQ(BLE_ERROR_IRQn);
    NVIC_EnableIRQ(BLE_TIMESTAMP_TGT1_IRQn);
    NVIC_EnableIRQ(BLE_FINETGT_IRQn);
    NVIC_EnableIRQ(BLE_TIMESTAMP_TGT2_IRQn);
    NVIC_EnableIRQ(BLE_SW_IRQn);

    __set_FAULTMASK(FAULTMASK_ENABLE_INTERRUPTS);
    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);
}

void App_Mem_Ret_Sleep_Init(void)
{
    /* Set the power mode used for the application */
    mem_retention_sleep_mode_cfg.power_mode = SLEEP_MODE;

    /* Set the retention level */
    mem_retention_sleep_mode_cfg.retention_type = MEMORY_RETENTION;
    mem_retention_sleep_mode_cfg.vddret_ctrl.vddc_ret_en = VDDC_RETENTION_DISABLE;

    /* Set save/restore peripheral routine */
    mem_retention_sleep_mode_cfg.p_save_peripherals = App_LowPower_SavePeripheralStates;
    mem_retention_sleep_mode_cfg.p_restore_peripherals = App_LowPower_RestorePeripheralStates;

    /* DMA Channel number for RF register transfer */
    mem_retention_sleep_mode_cfg.dma_channel_rf = SLEEP_REG_BACKUP_DMA;

    /* Wakeup Configuration */
    mem_retention_sleep_mode_cfg.wakeup_cfg = WAKEUP_DELAY_2                      |
											  WAKEUP_GPIO1_ENABLE                 |
                                              WAKEUP_GPIO1_RISING                 |
                                              WAKEUP_GPIO0_ENABLE                 |
#if (BUTTON_MANAGER_ENABLED == 1)
                                              WAKEUP_GPIO0_FALLING                |
#else
                                              WAKEUP_GPIO0_RISING                 |
#endif
                                              WAKEUP_DCDC_OVERLOAD_DISABLE;

    /* Clock Configuration for Run Mode */
    mem_retention_sleep_mode_cfg.clock_cfg.sensorclk_freq = SENSOR_CLK_HZ;

    mem_retention_sleep_mode_cfg.clock_cfg.systemclk_freq = SYSTEM_CLK_HZ;

    mem_retention_sleep_mode_cfg.clock_cfg.uartclk_freq = UART_CLK_HZ;

    mem_retention_sleep_mode_cfg.clock_cfg.userclk_freq = USER_CLK_HZ;

    /* VDD Retention Regulator Configuration */
    /*  By default the retention regulators register default TRIM values
     *  are set to 0x03. The user can set to 0x01 to further reduce power
     *  consumption but this will limit operational capabilities across
     *  an extended temperature range. To demonstrate low power
     *  consumption, the value set here is 0x1 */
    mem_retention_sleep_mode_cfg.vddret_ctrl.vddm_ret_trim = 0x1;

    mem_retention_sleep_mode_cfg.vddret_ctrl.vddc_ret_trim = 0x1;

    mem_retention_sleep_mode_cfg.vddret_ctrl.vddacs_ret_trim = 0x1;

    /* Enable VDDT Baseband Timer if BLE is Present */
    mem_retention_sleep_mode_cfg.vddret_ctrl.vddt_ret = VDDTRETENTION_ENABLE;

    /* Ble functionality is present in this application */
    mem_retention_sleep_mode_cfg.ble_present_flag = BLE_PRESENT;

    mem_retention_sleep_mode_cfg.boot_cfg = BOOT_CUSTOM                     |
                                            BOOT_PWR_CAL_BYPASS_ENABLE      |
                                            BOOT_ROT_BYPASS_ENABLE;

    /* Application will resume from this address after wakeup from BOOT_CUSTOM
     * with memory retention */
    mem_retention_sleep_mode_cfg.p_app_resume = Main_Loop;

    /* Initialize the wakeup configuration */
    Sys_PowerModes_SetWakeupConfig(mem_retention_sleep_mode_cfg.wakeup_cfg);
}

void App_No_Ret_Sleep_Init(void)
{
    no_retention_sleep_mode_cfg.power_mode = SLEEP_MODE;
    no_retention_sleep_mode_cfg.retention_type = NO_RETENTION;
    no_retention_sleep_mode_cfg.vddret_ctrl.vddc_ret_en = VDDC_RETENTION_DISABLE;

    no_retention_sleep_mode_cfg.p_restore_peripherals = App_GPIO_Config;

    /* Wakeup Configuration */
    no_retention_sleep_mode_cfg.wakeup_cfg = WAKEUP_DELAY_16                     |
                                             WAKEUP_GPIO0_ENABLE                 |
#if (BUTTON_MANAGER_ENABLED == 1)
                                             WAKEUP_GPIO0_FALLING                |
#else
                                             WAKEUP_GPIO0_RISING                 |
#endif
                                             WAKEUP_DCDC_OVERLOAD_DISABLE;

    /* Clock Configuration for Run Mode */
    no_retention_sleep_mode_cfg.clock_cfg.sensorclk_freq = SENSOR_CLK_HZ;

    no_retention_sleep_mode_cfg.clock_cfg.systemclk_freq = SYSTEM_CLK_HZ;

    no_retention_sleep_mode_cfg.clock_cfg.uartclk_freq = UART_CLK_HZ;

    no_retention_sleep_mode_cfg.clock_cfg.userclk_freq = USER_CLK_HZ;

    /* Upon wake-up, BLE Stack will re-initialized so
     *  there is no need to maintain BLE configuration */
    no_retention_sleep_mode_cfg.vddret_ctrl.vddt_ret = VDDTRETENTION_DISABLE;
    no_retention_sleep_mode_cfg.ble_present_flag = POWER_MODES_BLE_NOT_PRESENT;

    /* Boot Configuration */
    no_retention_sleep_mode_cfg.boot_cfg = BOOT_FLASH_XTAL_DEFAULT_TRIM    |
                                           BOOT_PWR_CAL_BYPASS_ENABLE      |
                                           BOOT_ROT_BYPASS_ENABLE;
}

void App_Clock_Config(void)
{
    /*
     * Configure the 48 MHz XTAL oscillator and set the RFCLK frequency to
     * the desired system clock frequency defined in SYSTEM_CLK_HZ.
     * Check if actual RFCLK frequency does not equal SYSTEM_CLK_HZ
     */
    if (Sys_Clocks_XTALFreqConfig(SYSTEM_CLK_HZ) != SYSTEM_CLK_HZ)
    {
        /* Wait for watchdog reset! */
        __WFI();
    }

    /* Switch to (divided 48 MHz) oscillator clock, and update the
     * SystemCoreClock global variable. */
    Sys_Clocks_SystemClkConfig(SYSCLK_CLKSRC_RFCLK);

    /* Configure clock dividers */
    Sys_Clocks_DividerConfig(UART_CLK_HZ, SENSOR_CLK_HZ, USER_CLK_HZ);

    /* Configure and initialize the system low-power clock */
    Sys_LPClock_Init();
}

void App_GPIO_Config(void)
{
    /* Disable JTAG TDI, TDO, and TRST connections to GPIO 2, 3, and 4 */
    GPIO->JTAG_SW_PAD_CFG &= ~(CM33_JTAG_DATA_ENABLED | CM33_JTAG_TRST_ENABLED);

#if DEBUG_SLEEP_GPIO == DEBUG_SYSCLK_PWR_MODE
    /* Configure POWER_MODE_GPIO */
    SYS_GPIO_CONFIG(POWER_MODE_GPIO, (GPIO_6X_DRIVE | GPIO_LPF_DISABLE |
                                      GPIO_WEAK_PULL_UP | GPIO_MODE_GPIO_OUT));

    /* Clear POWER_MODE_GPIO to indicate Run Mode Configuration */
    Sys_GPIO_Set_Low(POWER_MODE_GPIO);

    /* Configure SYSCLK_GPIO to output system clock */
    SYS_GPIO_CONFIG(SYSCLK_GPIO, (GPIO_2X_DRIVE | GPIO_LPF_DISABLE |
                                  GPIO_WEAK_PULL_UP | GPIO_MODE_SYSCLK));

    /* Configure Green LED_GPIO */
    SYS_GPIO_CONFIG(GREEN_LED_GPIO, (GPIO_6X_DRIVE | GPIO_LPF_DISABLE |
                                      GPIO_WEAK_PULL_UP | GPIO_MODE_GPIO_OUT));

    /* Pull it low to avoid current leakage */
    Sys_GPIO_Set_Low(GREEN_LED_GPIO);

#elif  DEBUG_SLEEP_GPIO == DEBUG_RTC_TASKS_MODE
    /* Configure TASK1_RUN_ACTIVITY_GPIO to output RTC Task 1 */
    SYS_GPIO_CONFIG(BASS_RTC_TASK_GPIO, (GPIO_2X_DRIVE | GPIO_LPF_DISABLE |
                                  GPIO_WEAK_PULL_UP | GPIO_MODE_GPIO_OUT));

    Sys_GPIO_Set_Low(BASS_RTC_TASK_GPIO);
#endif

    GPIO->CFG[CEM102_VDDA_DUT_GPIO] = GPIO_MODE_DISABLE | GPIO_NO_PULL;

    GPIO->CFG[RSL15_VDDC_GPIO] = GPIO_NO_PULL;

    CEM102_Initialize(cem102_dut);
}

uint32_t Power_Down_FPU(void)
{
    uint32_t success;

   /* Request power down of FPU */
    SYSCTRL->FPU_PWR_CFG = FPU_WRITE_KEY | ((SYSCTRL->FPU_PWR_CFG &
                           ~(0x1U << SYSCTRL_FPU_PWR_CFG_FPU_Q_REQ_Pos)) |
                           FPU_Q_REQUEST);

    /* Wait until it is accepted or denied */
    while ((SYSCTRL->FPU_PWR_CFG & (0x1U << SYSCTRL_FPU_PWR_CFG_FPU_Q_ACCEPT_Pos)) !=
           FPU_Q_ACCEPTED &&
           (SYSCTRL->FPU_PWR_CFG & (0x1U << SYSCTRL_FPU_PWR_CFG_FPU_Q_DENY_Pos)) !=
           FPU_Q_DENIED);

    if ((SYSCTRL->FPU_PWR_CFG & (0x1U << SYSCTRL_FPU_PWR_CFG_FPU_Q_ACCEPT_Pos)) ==
        FPU_Q_ACCEPTED)
    {
        /* If it is accepted, isolate and power down FPU */
        SYSCTRL->FPU_PWR_CFG = FPU_WRITE_KEY | FPU_Q_REQUEST | FPU_ISOLATE |
                               FPU_PWR_TRICKLE_ENABLE  | FPU_PWR_HAMMER_ENABLE;
        SYSCTRL->FPU_PWR_CFG = FPU_WRITE_KEY | FPU_Q_REQUEST | FPU_ISOLATE |
                               FPU_PWR_TRICKLE_DISABLE | FPU_PWR_HAMMER_DISABLE;
        success = FPU_Q_ACCEPTED;
    }
    else
    {
        /* If it is denied, cancel request */
        SYSCTRL->FPU_PWR_CFG = FPU_WRITE_KEY | ((SYSCTRL->FPU_PWR_CFG &
                               ~(0x1U << SYSCTRL_FPU_PWR_CFG_FPU_Q_REQ_Pos)) |
                               FPU_Q_NOT_REQUEST);
        success = FPU_Q_DENIED;
    }

    return success;
}

void Init_SWMTrace(void)
{
#ifdef SWMTRACE_OUTPUT
    swmTrace_init(traceOptions, (sizeof(traceOptions) / sizeof(traceOptions[0])));
#endif
}

