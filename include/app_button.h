/**
 * @file  app_button.h
 * @brief Button handler
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

#ifndef INCLUDE_APP_BUTTON_H_
#define INCLUDE_APP_BUTTON_H_

/*************************************************************************************************
 * Type Definitions
 *************************************************************************************************/
/** Defines successful and unsuccessful return values for button_manager functions */
typedef enum
{
    BUTTON_MANAGER_OK                             = 0, /**< No error */
    BUTTON_MANAGER_ACTIVE_LEVEL_INVALID           = 1, /**< Error: Active level not 0 or 1 */
    BUTTON_MANAGER_GPIO_PIN_INVALID               = 2, /**< Error: GPIO pin(s) invalid */
    BUTTON_MANAGER_LENGTH_EXCEEDS_MAX             = 3, /**< Error: Length or timeout exceeds max */
    BUTTON_MANAGER_RELATIVE_LENGTHS_INVALID       = 4, /**< Error: Longer presses have shorter lengths */
    BUTTON_MANAGER_LENGTH_NOT_DIVISIBLE_BY_PERIOD = 5  /**< Error: Length(s) non-divis by timer period */
} Button_ManagerReturn_t;

/** Defines active button state values */
typedef enum
{
    BUTTON_ACTIVE_LOW       = 0, /**< Active button state is LOW (0) */
    BUTTON_ACTIVE_HIGH      = 1, /**< Active button state is HIGH (1) */
    BUTTON_ACTIVE_UNDEFINED = 2  /**< Active button state is UNDEFINED (> 1) */
} Button_ActiveState_t;

/** Struct of pointers for callback functions to be triggered upon each type of press event */
typedef struct
{
    void (*ShortPress)     (void); /**< Callback for short press */
    void (*MediumPress)    (void); /**< Callback for medium press */
    void (*LongPress)      (void); /**< Callback for long press */
    void (*SuperLongPress) (void); /**< Callback for super long press */
    void (*DoublePress)    (void); /**< Callback for double press */
    void (*TriplePress)    (void); /**< Callback for triple press */
} Button_Callbacks_t;

/**
 * Struct of pointers for indicator callback functions used to indicate what type of press the
 * button manager is currently perceiving the ongoing press as
 */
typedef struct
{
    void (*ShortIndicator)     (void); /**< Indicator callback for short press */
    void (*MediumIndicator)    (void); /**< Indicator callback for medium press */
    void (*LongIndicator)      (void); /**< Indicator callback for long press */
    void (*SuperLongIndicator) (void); /**< Indicator callback for super long press */
    void (*DoubleIndicator)    (void); /**< Indicator callback for double press */
    void (*TripleIndicator)    (void); /**< Indicator callback for triple press */
} Button_IndicatorCallbacks_t;

/**
 * Struct used for defining how long each press type is,
 * as well as the timeout for multiple press detection (all values in ms)
 */
typedef struct
{
    uint16_t ShortPressLength;     /**< Length of a short press */
    uint16_t MediumPressLength;    /**< Length of a medium press */
    uint16_t LongPressLength;      /**< Length of a long press */
    uint16_t SuperLongPressLength; /**< Length of a super long press */
} Button_Lengths_t;

/*************************************************************************************************
 * Macros
 *************************************************************************************************/
/* Definition of public function-like macros go here */

/* Button configurations */
#define BUTTON_GPIO_INDEX               0
#define BUTTON_DEBOUNCE_COUNT           255
#define BUTTON_USER_CLOCK_SOURCE        TIMER0
#define BUTTON_USER_GPIO_ACTIVE_LEVEL   ((Button_ActiveState_t) BUTTON_ACTIVE_LOW)

#define MS_TO_S                         1000

/* Button Timer Settings
 * Assumptions:
 *    - SYSCLK = 8 MHz
 *    - Slow Clock Prescale = 8
 *    - Timer0 Clock Source = SLOWCLK32
 *    - Timer0 Prescale = 8
 */
#define BUTTON_TIMER_FREQ_HZ 3906
#define BUTTON_TIMER_PERIOD_MS 25
#define BUTTON_TIMER_PERIOD_CYCLES ((uint32_t) (BUTTON_TIMER_FREQ_HZ * BUTTON_TIMER_PERIOD_MS / MS_TO_S))

#define BUTTON_TIMER_INTERRUPT_PRIORITY 7
#define BUTTON_GPIO_INTERRUPT_PRIORITY 7

/** The maximum usable length for a press length */
/* Limited to 15 bits in Button_Tracker_t */
#define BUTTON_MAX_PRESS_LENGTH_MS 32767

/*************************************************************************************************
 * Function Prototypes
 *************************************************************************************************/
extern void TIMER0_IRQHandler(void);
extern void GPIO0_IRQHandler(void);

/**
 * @brief Initialize GPIO and Timer used in the button
 */
void App_Button_GPIO_Timer_Init(void);

/**
 * @brief Sets the button configurations
 */
void App_Button_Config(void);
/**
 * @brief Callback used to handle GPIO interrupt events
 */
void App_Button_GPIOEventHandler(void);

/**
 * @brief Callback used to handle timekeeping interrupt events, performs all button processing
 */
void App_Button_PeriodicEventHandler(void);

/**
* @brief Verifies that the currently set button configuration parameters are valid
*
* @return BUTTON_MANAGER_OK if all specified configurations are valid,
*         BUTTON_MANAGER_<FAIL_CONDITION> otherwise
*/
Button_ManagerReturn_t App_Button_ConfigsValid(void);

/**
 * @brief Overwrites the existing button callback and length configurations
 *
 * @param[in] p_user_button_cbs An array of Button_Callbacks_t structs that the user can use to define
 *                              all callback functions for each button
 * @param[in] p_user_button_indicator_cbs A pointer to a Button_IndicatorCallbacks_t struct that the
 *                                        user can use to define all indicator callback functions
 * @param[in] p_user_button_lengths A pointer to a Button_Lengths_t struct that the user can use to
 *                                  define the lengths of each type of button press
 *
 * @return BUTTON_MANAGER_OK if all new configurations are valid and are set successfully,
 *         BUTTON_MANAGER_<FAIL_CONDITION> otherwise
 */
Button_ManagerReturn_t App_Button_UpdateConfigs(Button_Callbacks_t* p_user_button_cbs,
                                            Button_IndicatorCallbacks_t* p_user_button_indicator_cbs,
                                            Button_Lengths_t* p_user_button_lengths);

/**
 * @brief Initializes the button manager with the specified user config
 *
 * @param[in] user_button_active_state The active state of the buttons in the user HW config
 * @param[in] user_button_timer_period_ms The interrupting period of the timekeeping peripheral
 *                                        (TIMERx or SysTick) configured by the user
 * @param[in] p_user_button_gpio the GPIO pin used for the button in the user HW config
 * @param[in] p_user_button_cbs An array of Button_Callbacks_t structs that the user can use to define
 *                              all callback functions for each button
 * @param[in] p_user_button_indicator_cbs A pointer to a Button_IndicatorCallbacks_t struct that the
 *                                        user can use to define all indicator callback functions
 * @param[in] p_user_button_lengths A pointer to a Button_Lengths_t struct that the user can use to
 *                                  define the lengths of each type of button press
 *
 * @return BUTTON_MANAGER_OK if initialization has been successful,
 *         BUTTON_MANAGER_<FAIL_CONDITION> otherwise
 */
Button_ManagerReturn_t App_Button_Mng_Initialize(const uint8_t user_button_active_state,
                                                 const uint8_t user_button_timer_period_ms,
                                                 const uint8_t user_button_gpio,
                                                 Button_Callbacks_t *p_user_button_cbs,
                                                 Button_IndicatorCallbacks_t *p_user_button_indicator_cbs,
                                                 Button_Lengths_t *p_user_button_lengths);

/**
 * @brief Check if the button is being pressed
 *
 * @return TRUE if the button is pressed
 *         FALSE if the button is not pressed
 */
bool App_ButtonIsPressed(void);

/**
 * @brief Callback for short press
 */
void App_ButtonShortPress(void);

/**
 * @brief Callback for medium press
 */
void App_ButtonMediumPress(void);

/**
 * @brief Callback for long press
 */
void App_ButtonLongPress(void);

/**
 * @brief Callback for super long press
 */
void App_ButtonSuperLongPress(void);

/**
 * @brief Indicator for short press
 */
void App_ButtonShortIndicator(void);

/**
 * @brief Indicator for medium press
 */
void App_ButtonMediumIndicator(void);

/**
 * @brief Indicator for long press
 */
void App_ButtonLongIndicator(void);

/**
 * @brief Indicator for super long press
 */
void App_ButtonSuperLongIndicator(void);

#endif /* INCLUDE_APP_BUTTON_H_ */
