/**
 * @file swmTrace_options.h
 * @brief This provides definitions of the available configuration options.
 * @copyright @parblock
 * Copyright (c) 2023 Semiconductor Components Industries, LLC (d/b/a
 * onsemi), All Rights Reserved
 *
 * This software and/or documentation is licensed by onsemi under limited
 * terms and conditions. The terms and conditions pertaining to the software and/or
 * documentation are available at http://www.onsemi.com/site/pdf/ONSEMI_T&C.pdf
 * ("onsemi Standard Terms and Conditions of Sale, Section 8 Software").
 * Do not use this software and/or documentation unless you have carefully read and
 * you agree to the limited terms and conditions. By using this software and/or
 * documentation, you agree to the limited terms and conditions.
 * @endparblock
 */

#ifndef SWMTRACE_OPTIONS_H_
#define SWMTRACE_OPTIONS_H_

/** @brief Status used to determine if logs are printing */
#define SWMTRACE_ON    1

/** @brief Status used to determine if logs are not printing */
#define SWMTRACE_OFF   0

/** @brief Define the default logging state as printing */
#ifndef SWMTRACE_LOGGING
#define SWMTRACE_LOGGING SWMTRACE_ON
#endif

/**
 * @brief Define the UART source to use.
 * @note  To change the UART source, update this define and rebuild the library.
 */
#define SWM_UART_SOURCE 0

/** @brief Define the DMA source to use.
 *  @note  To change the DMA source, update this define and rebuild the library.
 */
#define SWM_DMA_SOURCE  0

/**
 * @brief Macro defining the base of the logging options.
 */
#define SWM_LOG_OPTIONS_BASE    0x10000000U


/**
 * @brief Macro defining the VERBOSE logging level.
 */
#define SWM_LOG_LEVEL_VERBOSE   (SWM_LOG_OPTIONS_BASE + 0x01000000U)

/**
 * @brief Macro defining the INFORMATION logging level.
 */
#define SWM_LOG_LEVEL_INFO      (SWM_LOG_OPTIONS_BASE + 0x01000001U)

/**
 * @brief Macro defining the WARNING logging level.
 */
#define SWM_LOG_LEVEL_WARNING   (SWM_LOG_OPTIONS_BASE + 0x01000002U)

/**
 * @brief Macro defining the ERROR logging level.
 */
#define SWM_LOG_LEVEL_ERROR     (SWM_LOG_OPTIONS_BASE + 0x01000003U)

/**
 * @brief Macro defining the FATAL ERROR logging level.
 */
#define SWM_LOG_LEVEL_FATAL     (SWM_LOG_OPTIONS_BASE + 0x01000004U)

/**
 * @brief Macro defining the TEST PASS logging level.
 */
#define SWM_LOG_TEST_PASS       (SWM_LOG_OPTIONS_BASE + 0x01000005U)

/**
 * @brief Macro defining the TEST FAIL logging level.
 */
#define SWM_LOG_TEST_FAIL       (SWM_LOG_OPTIONS_BASE + 0x01000006U)


/**
 * @brief Macro indicating time stamps should be excluded in the log messages.
 * @note This is a future feature and is currently not implemented.
 */
#define SWM_LOG_TIME_STAMP_OFF  (SWM_LOG_OPTIONS_BASE + 0x02000000U)

/**
 * @brief Macro indicating time stamps should be included in the log messages.
 * @note This is a future feature and is currently not implemented.
 */
#define SWM_LOG_TIME_STAMP_ON   (SWM_LOG_OPTIONS_BASE + 0x02000001U)


/**
 * @brief Macro defining the base of the UART options.
 */
#define SWM_UART_OPTIONS_BASE   0x20000000U

/**
 * @brief Macros allowing selection of specific UART options.
 */
#define SWM_UART_OPTIONS_MASK   0xFF000000U

/**
 * @brief Macro defining the selection of the UART RX pin.
 * @note
 * This is defined as an indicator and should be ORed with the actual GPIO pin
 * to be used.
 * e.g to select GPIO 1 for RX, the options should be constructed as:
 *      SWM_UART_RX_PIN | 1
 */
#define SWM_UART_RX_PIN         (SWM_UART_OPTIONS_BASE + 0x01000000U)

/**
 * @brief Macro defining the selection of the UART TX pin.
 * @note
 * This is defined as an indicator and should be ORed with the actual GPIO pin
 * to be used.
 * e.g to select GPIO 2 for TX, the options should be constructed as:
 *      SWM_UART_TX_PIN | 2
 */
#define SWM_UART_TX_PIN         (SWM_UART_OPTIONS_BASE + 0x02000000U)

/**
 * @brief Macro indicating the RX Interrupt will be enabled when using the
 * UART. This is required for bi-directional traffic. Not needed for just
 * logging trace messages.
 */
#define SWM_UART_RX_ENABLE      (SWM_UART_OPTIONS_BASE + 0x04000000U)

/**
 * @brief Macro indicating the Baud Rate selection for the UART.
 */
#define SWM_UART_BAUD_RATE      (SWM_UART_OPTIONS_BASE + 0x08000000U)

#endif /* SWMTRACE_OPTIONS_H_ */
