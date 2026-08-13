/**
 * @file  app_button.c
 * @brief Source file for utilities
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

#include "app.h"
#include "app_button.h"

/** Macro for reading the value of a given button */
#define BUTTON_READ_GPIO_STATE(button_GPIO_pin) ((GPIO->INPUT_DATA >> button_GPIO_pin) & 0x1)

/** Macro for checking if a button press callback exists and then calling it if it does */
#define BUTTON_PRESS_CALLBACK(callback)\
if (p_button_cbs->callback != NULL)\
{\
    p_button_cbs->callback();\
}

/** Macro for checking if a button indicator callback exists and then calling it if it does */
#define BUTTON_INDICATOR_CALLBACK(callback)\
if (p_button_indicator_cbs->callback != NULL)\
{\
    p_button_indicator_cbs->callback();\
}

/* Callbacks to be registered for each button press type */
Button_Callbacks_t button_cbs =
{
    App_ButtonShortPress,
    App_ButtonMediumPress,
    App_ButtonLongPress,
    App_ButtonSuperLongPress,
};

/* Indicator callbacks to be registered for each button press type */
Button_IndicatorCallbacks_t button_indicator_cbs =
{
    App_ButtonShortIndicator,
    App_ButtonMediumIndicator,
    App_ButtonLongIndicator,
    App_ButtonSuperLongIndicator
};

/* Lengths to be assigned to each button press type */
Button_Lengths_t button_lengths =
{
    100,   /* Short press length = 100ms */
    500,  /* Medium press length = 500ms */
    1000, /* Long press length = 1000ms */
    5000  /* Super long press length = 5000ms */
};

/** Struct used for tracking all required information to correctly interpret each press type */
typedef struct
{
    uint32_t button_current_state    :  1; /**< Button_ActiveState_t */
    uint32_t ignoring_release_events :  1; /**< Button_IgnoreReleaseEvents_t */
    uint32_t active_event            :  1; /**< Button_PendingPressEvent_t */
    uint32_t button_period_counter   : 15; /**< Supports up to 32s press length */
    uint32_t reserved                : 14; /**< Reserved for future use */
} Button_Tracker_t;

/** Defines values for when the button manager is and is not ignoring button release events */
typedef enum
{
    BUTTON_NOT_IGNORING_RELEASE_EVENTS = 0, /**< Not currently processing a super long press */
    BUTTON_IGNORING_RELEASE_EVENTS     = 1  /**< Currently processing a super long press */
} Button_IgnoreReleaseEvents_t;

/** Defines values for the presence (or lack thereof) of new button events */
typedef enum
{
    BUTTON_NO_NEW_EVENT = 0, /**< No new button transition event is present */
    BUTTON_NEW_EVENT    = 1  /**< A new button transition event is present */
} Button_PendingEvent_t;

/** Defines states for whether or not the button manager has been initialized yet */
typedef enum
{
    BUTTON_MANAGER_UNINITIALIZED = 0, /**< The button_manager has not yet been initialized */
    BUTTON_MANAGER_INITIALIZED   = 1  /**< The button_manager has been initialized successfully */
} Button_ManagerInitializationState_t;

/**
 * Tracks whether or not the button manager has been (validly) configured by the user,
 * must be set to BUTTON_MANAGER_INITIALIZED for any button handling to be performed
 */
Button_ManagerInitializationState_t button_mgr_initialized = BUTTON_MANAGER_UNINITIALIZED;

/** Stores how many GPIOs the user has configured to be buttons */
uint8_t button_num_GPIOs = 0;

/** Stores the active button state as defined by the user */
Button_ActiveState_t button_active_state = BUTTON_ACTIVE_UNDEFINED;

/** Stores the interrupting period (in ms) of the timekeeping peripheral as defined by the user */
uint8_t button_timer_period_ms = 0;

/** Pointer to the array of Button_Callbacks_t structs defined by the user */
Button_Callbacks_t *p_button_cbs;

/** Pointer to the Button_IndicatorCallbacks_t struct defined by the user */
Button_IndicatorCallbacks_t *p_button_indicator_cbs;

/** Pointer to the Button_Lengths_t struct defined by the user */
Button_Lengths_t *p_button_lengths;

/** Each struct stores tracking information required to process each button correctly */
Button_Tracker_t button_tracker;

/** Default values used for initializing and resetting the button_tracker */
Button_Tracker_t button_tracker_default_value = {0};

static inline void App_Button_ReleaseEventHandler(void);

static inline uint32_t App_Add_CurTimerVal(void);

static inline uint32_t App_Add_CurTimerVal(void)
{
    return (uint32_t) (TIMER0->VAL * MS_TO_S / BUTTON_TIMER_FREQ_HZ);
}

void TIMER0_IRQHandler(void)
{
    /* Call periodic event handlers */
    App_Button_PeriodicEventHandler();
}

void GPIO0_IRQHandler(void)
{
    App_Button_GPIOEventHandler();
}

void App_Button_GPIO_Timer_Init(void)
{
    /* Button GPIO and interrupt configuration */
    SYS_GPIO_CONFIG(BUTTON_GPIO,
                    (GPIO_MODE_GPIO_IN | GPIO_LPF_DISABLE | GPIO_WEAK_PULL_UP | GPIO_2X_DRIVE));

    Sys_GPIO_IntConfig(BUTTON_GPIO_INDEX, (GPIO_DEBOUNCE_ENABLE |
                                           (BUTTON_GPIO << GPIO_INT_CFG_SRC_Pos) |
                                           GPIO_EVENT_TRANSITION),
                                           GPIO_DEBOUNCE_SLOWCLK_DIV1024,
                                           BUTTON_DEBOUNCE_COUNT);

    /* TIMER0 configuration for button handling */
    Sys_Timer_Stop(BUTTON_USER_CLOCK_SOURCE);
    Sys_Timer_Config(BUTTON_USER_CLOCK_SOURCE,
                     TIMER_PRESCALE_8,
                     TIMER_FREE_RUN ,
                     BUTTON_TIMER_PERIOD_CYCLES);
    Sys_Timer_Start(BUTTON_USER_CLOCK_SOURCE);
}

void App_Button_Config(void)
{
    App_Button_GPIO_Timer_Init();

    /* Interrupt enabling and priority configuration */
    NVIC_SetPriority(TIMER0_IRQn, BUTTON_TIMER_INTERRUPT_PRIORITY);
    NVIC_SetPriority(GPIO0_IRQn, BUTTON_GPIO_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(TIMER0_IRQn);
    NVIC_EnableIRQ(GPIO0_IRQn);

    /* button_manager initialization */
    App_Button_Mng_Initialize(BUTTON_USER_GPIO_ACTIVE_LEVEL,
                              BUTTON_TIMER_PERIOD_MS,
                              BUTTON_GPIO,
                              &button_cbs,
                              &button_indicator_cbs,
                              &button_lengths);
}

void App_Button_GPIOEventHandler(void)
{
    /* No processing will take place if the button manager is uninitialized */
    if (button_mgr_initialized == BUTTON_MANAGER_INITIALIZED)
    {
        /* Upon a GPIO transition event, set the active_event flag for the periodic event handler */
        button_tracker.active_event = BUTTON_NEW_EVENT;

        /* If this is releasing event, process the handler right away */
        if (BUTTON_READ_GPIO_STATE(BUTTON_GPIO_INDEX) != button_active_state)
        {

            /* If a super long press event has been handled already, ignore this release */
            if (button_tracker.ignoring_release_events == BUTTON_IGNORING_RELEASE_EVENTS)
            {
                /* Clear the flag to avoid accidentally ignoring valid events */
                button_tracker.ignoring_release_events = BUTTON_NOT_IGNORING_RELEASE_EVENTS;
            }
            /* Process the release as normal */
            else
            {
                /* Iterate the counter by the amount of time elapsed */
                button_tracker.button_period_counter += App_Add_CurTimerVal();
                /* Call a helper function to process the release event */
                App_Button_ReleaseEventHandler();
            }

            /* Reset the counter that tracks how long the button has been pressed */
            button_tracker.button_period_counter = 0;
        }
    }
}

static inline void App_Button_SinglePressIndicatorHandler(void)
{
    /*
     * If the press hits the minimum length of a super long press,
     * immediately trigger the event without waiting for a user release
     */
    if (button_tracker.button_period_counter ==
        p_button_lengths->SuperLongPressLength)
    {
        /* Call the super long press indicator (if it exists) */
        BUTTON_INDICATOR_CALLBACK(SuperLongIndicator);

        /* Call the super long press callback (if it exists) */
        BUTTON_PRESS_CALLBACK(SuperLongPress);

        /*
         * Set flag to ignore the next release event, since the super
         * long press has already been serviced
         */
        button_tracker.ignoring_release_events = BUTTON_IGNORING_RELEASE_EVENTS;
    }
    /* Check if this press has reached the minimum length of a long press */
    else if (button_tracker.button_period_counter ==
             p_button_lengths->LongPressLength)
    {
        /* Call the long press indicator (if it exists) */
        BUTTON_INDICATOR_CALLBACK(LongIndicator);
    }
    /* Check if this press has reached the minimum length of a medium press */
    else if (button_tracker.button_period_counter ==
             p_button_lengths->MediumPressLength)
    {
        /* Call the medium press indicator (if it exists) */
        BUTTON_INDICATOR_CALLBACK(MediumIndicator);
    }
    /* Check if this press has reached the minimum length of a short press */
    else if (button_tracker.button_period_counter ==
             p_button_lengths->ShortPressLength)
    {
        /* Call the short press indicator (if it exists) */
        BUTTON_INDICATOR_CALLBACK(ShortIndicator);
    }
    /* Executed if the press is currently too short to trigger any events */
    else
    {
        /* Do nothing */
    }
}


static inline void App_Button_ReleaseEventHandler(void)
{
    /* Check if this press has reached the minimum length of a long press */
    if (button_tracker.button_period_counter >=
        p_button_lengths->LongPressLength)
    {
        /* Activate the long press callback (if it exists) */
        BUTTON_PRESS_CALLBACK(LongPress);
    }
    /* Check if this press has reached the minimum length of a medium press */
    else if (button_tracker.button_period_counter >=
             p_button_lengths->MediumPressLength)
    {
        /* Activate the medium press callback (if it exists) */
        BUTTON_PRESS_CALLBACK(MediumPress);
    }
    /* Check if this press has reached the minimum length of a short press */
    else if (button_tracker.button_period_counter >=
             p_button_lengths->ShortPressLength)
    {
        BUTTON_PRESS_CALLBACK(ShortPress);
    }
    /* Executed if the press was too short to trigger any events */
    else
    {
        /* Do nothing */
    }
}

Button_ManagerReturn_t App_Button_ConfigsValid(void)
{
    Button_ManagerReturn_t result = BUTTON_MANAGER_OK;

    /* Check that no lengths exceed their maximum values */
    if ((p_button_lengths->ShortPressLength     > BUTTON_MAX_PRESS_LENGTH_MS) ||
        (p_button_lengths->MediumPressLength    > BUTTON_MAX_PRESS_LENGTH_MS) ||
        (p_button_lengths->LongPressLength      > BUTTON_MAX_PRESS_LENGTH_MS) ||
        (p_button_lengths->SuperLongPressLength > BUTTON_MAX_PRESS_LENGTH_MS))
    {
        result = BUTTON_MANAGER_LENGTH_EXCEEDS_MAX;
    }

    /* Check that the button press lengths make sense relative to each other */
    if ((p_button_lengths->ShortPressLength  >= p_button_lengths->MediumPressLength) ||
        (p_button_lengths->MediumPressLength >= p_button_lengths->LongPressLength)   ||
        (p_button_lengths->LongPressLength   >= p_button_lengths->SuperLongPressLength))
    {
        result = BUTTON_MANAGER_RELATIVE_LENGTHS_INVALID;
    }

    /* Check that each length is divisible by the timekeeping interrupting period */
    if (((p_button_lengths->ShortPressLength     % button_timer_period_ms) != 0) ||
        ((p_button_lengths->MediumPressLength    % button_timer_period_ms) != 0) ||
        ((p_button_lengths->LongPressLength      % button_timer_period_ms) != 0) ||
        ((p_button_lengths->SuperLongPressLength % button_timer_period_ms) != 0))
    {
        result = BUTTON_MANAGER_LENGTH_NOT_DIVISIBLE_BY_PERIOD;
    }

    /* If all buttons pass every check, the config will be considered valid */
    return result;
}

Button_ManagerReturn_t App_Button_UpdateConfigs(Button_Callbacks_t* p_user_button_cbs,
                                            Button_IndicatorCallbacks_t* p_user_button_indicator_cbs,
                                            Button_Lengths_t* p_user_button_lengths)
{
    /* Disable the button manager while changing parameters */
    button_mgr_initialized = BUTTON_MANAGER_UNINITIALIZED;

    /* For any non-NULL parameters, overwrite the existing configuration */
    if (p_user_button_cbs != NULL)
    {
        p_button_cbs = p_user_button_cbs;
    }
    if (p_user_button_indicator_cbs != NULL)
    {
        p_button_indicator_cbs = p_user_button_indicator_cbs;
    }
    if (p_user_button_lengths != NULL)
    {
        p_button_lengths = p_user_button_lengths;
    }

    /* Reset the tracking information for each button before re-enabling the manager */
    for (uint8_t i = 0; i < button_num_GPIOs; i++)
    {
        button_tracker = button_tracker_default_value;
    }

    /* Re-enable the button manager if the new configurations are valid */
    Button_ManagerReturn_t config_status = App_Button_ConfigsValid();
    if (config_status == BUTTON_MANAGER_OK)
    {
        button_mgr_initialized = BUTTON_MANAGER_INITIALIZED;
    }

    /* If all buttons pass every check, the config will be considered valid */
    return config_status;
}

Button_ManagerReturn_t App_Button_Mng_Initialize(const uint8_t user_button_active_state,
                                                 const uint8_t user_button_timer_period_ms,
                                                 const uint8_t user_button_gpio,
                                                 Button_Callbacks_t *p_user_button_cbs,
                                                 Button_IndicatorCallbacks_t *p_user_button_indicator_cbs,
                                                 Button_Lengths_t *p_user_button_lengths)
{
    /* Disable the button manager while setting parameters */
    button_mgr_initialized = BUTTON_MANAGER_UNINITIALIZED;

    /* Prevent users from not defining a valid active button state */
    if(user_button_active_state > BUTTON_ACTIVE_HIGH)
    {
        return BUTTON_MANAGER_ACTIVE_LEVEL_INVALID;
    }

    /* Initialize button_tracker_default_values to a clean, released state */
    button_tracker_default_value.button_current_state = !user_button_active_state;

    /* Copy user params to global storage */
    button_active_state = user_button_active_state;
    button_timer_period_ms = user_button_timer_period_ms;

    /* Store pointers to user params passed by reference */
    p_button_cbs = p_user_button_cbs;
    p_button_indicator_cbs = p_user_button_indicator_cbs;
    p_button_lengths = p_user_button_lengths;

    /* Store button GPIO pins and initialize trackers to default values */
    button_tracker = button_tracker_default_value;

    /* Verify that the set configurations are valid so that the button manager can start to work */
    Button_ManagerReturn_t config_status = App_Button_ConfigsValid();
    if (config_status == BUTTON_MANAGER_OK)
    {
        button_mgr_initialized = BUTTON_MANAGER_INITIALIZED;
    }

    /* Return if initialization was successful or not */
    return (Button_ManagerReturn_t) config_status;
}

void App_Button_PeriodicEventHandler(void)
{
    /* No processing will take place if the button manager is uninitialized */
    if (button_mgr_initialized == BUTTON_MANAGER_UNINITIALIZED)
    {
        return;
    }


    /* Executed if there is a new transition event registered on the button's GPIO pin */
    if (button_tracker.active_event == BUTTON_NEW_EVENT)
    {
        /* Read the button's current GPIO state */
        button_tracker.button_current_state = BUTTON_READ_GPIO_STATE(BUTTON_GPIO_INDEX);

        /* Executed if this is a new press event */
        if (button_tracker.button_current_state == button_active_state)
        {
            /* Reset the counter that tracks how long the button has been pressed */
            button_tracker.button_period_counter = 0;
        }
        /* Executed if this is a new release event */
        else
        {
            /* If a super long press event has been handled already, ignore this release */
            if (button_tracker.ignoring_release_events == BUTTON_IGNORING_RELEASE_EVENTS)
            {
                /* Clear the flag to avoid accidentally ignoring valid events */
                button_tracker.ignoring_release_events = BUTTON_NOT_IGNORING_RELEASE_EVENTS;
            }
            /* Process the release as normal */
            else
            {
                /* Call a helper function to process the release event */
                App_Button_ReleaseEventHandler();
            }
        }

        /* Clear the active_event flag after processing */
        button_tracker.active_event = BUTTON_NO_NEW_EVENT;
    }
    /* No GPIO interrupt has been triggered, so the button state has not changed yet */
    else
    {
        /* Executed if the button is still being pressed from the last time */
        if (button_tracker.button_current_state == button_active_state)
        {
            /* If a super long press has already been processed, do nothing for now */
            if (button_tracker.ignoring_release_events ==
                BUTTON_IGNORING_RELEASE_EVENTS)
            {
                return;
            }

            /* Iterate the counter by the amount of time elapsed */
            button_tracker.button_period_counter += button_timer_period_ms;

            /* Call a helper function to process single press indicators appropriately */
            App_Button_SinglePressIndicatorHandler();
        }
    }
}

bool App_ButtonIsPressed(void)
{
    /* Return true if button is pressed, false otherwise */
    return BUTTON_USER_GPIO_ACTIVE_LEVEL == ((GPIO->INPUT_DATA >> (uint16_t)BUTTON_GPIO_INDEX) & 0x1);
}

/* ----------------------------------------------------------------------------
 * Button Presses and Indicators
 * --------------------------------------------------------------------------*/
void App_ButtonShortPress(void)
{
    if (sleep_mode == SLEEP_WKUP_RAM_MODE)
    {
        Switch_NoRetSleep_Mode();
    }
}

void App_ButtonMediumPress(void)
{
    if (sleep_mode == SLEEP_WKUP_RAM_MODE)
    {
        Switch_NoRetSleep_Mode();
    }
}

void App_ButtonLongPress(void)
{
    if (sleep_mode == SLEEP_WKUP_RAM_MODE)
    {
        Switch_NoRetSleep_Mode();
    }
}

void App_ButtonSuperLongPress(void)
{
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("Erasing the calibration data and the bond list.\r\n");
#endif /* SWMTRACE_OUTPUT */
    CEM102_Calibration_Erase();
    BondList_RemoveAll();
}

void App_ButtonShortIndicator(void)
{
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("Short Press Elapsed.\r\n");
#endif /* SWMTRACE_OUTPUT */
}

void App_ButtonMediumIndicator(void)
{
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("Medium Press Elapsed.\r\n");
#endif /* SWMTRACE_OUTPUT */
}

void App_ButtonLongIndicator(void)
{
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("Long Press Elapsed.\r\n");
#endif /* SWMTRACE_OUTPUT */
}

void App_ButtonSuperLongIndicator(void)
{
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("Super Long Press Elapsed.\r\n");
#endif /* SWMTRACE_OUTPUT */
}

