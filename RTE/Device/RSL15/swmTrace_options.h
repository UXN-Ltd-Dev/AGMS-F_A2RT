/**
 * @copyright @parblock
 * Semiconductor Components Industries, LLC dba "onsemi" is providing this "Technology" (including,
 * but not limited to, block diagrams, reference design kit, development product, prototype, sample,
 * any other non-production product, software, design tools, article, forum responses, Reusable
 * Code, etc.) "AS IS" and does not assume any liability arising from its use; nor does onsemi
 * convey any license to its or any third party's intellectual property rights. This Technology is
 * provided only to assist onsemi's customers in evaluating the Technology and the recipient assumes
 * all liability and risk associated with its use, including, but not limited to, compliance with
 * all laws and regulatory standards. onsemi reserves the right to make changes without further
 * notice to any of the Technology.
 *
 * The Technology, including recipient's input or feedback, is and shall remain the property of
 * onsemi. Nothing shall operate or be construed to grant any license, by implication, estoppel or
 * otherwise, under any of onsemi's intellectual property rights. Any source code is onsemi
 * proprietary and confidential information and recipient shall protect such source code to the
 * fullest extent required under law.
 * 
 * The Technology is not a finished product and is not available for resale to recipient's
 * consumers. Unless agreed otherwise in a separate agreement, the Technology is only intended for
 * research, development, demonstration and evaluation purposes and should only be used in
 * laboratory or development areas by persons with technical training and familiarity with the risks
 * associated with handling electrical/mechanical components, systems and subsystems. The recipient
 * assumes full responsibility and liability for proper and safe handling. Any other use, resale or
 * redistribution for any other purpose is strictly prohibited.
 *
 * THE TECHNOLOGY IS NOT WARRANTED AND PROVIDED ON AN "AS IS" BASIS ONLY. ANY WARRANTIES, EXPRESS,
 * IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 * A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE HEREBY EXPRESSLY DISCLAIMED.
 * 
 * TO THE FULLEST EXTENT PERMITTED BY LAW, IN NO EVENT SHALL ONSEMI BE LIABLE TO RECIPIENT OR ANY
 * OTHER PARTY. IN NO EVENT SHALL ONSEMI BE LIABLE FOR SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES
 * OF ANY NATURE WHATSOEVER (INCLUDING, BUT NOT LIMITED TO, LOSS OR DISGORGEMENT OF PROFITS, LOSS OF
 * USE AND LOSS OF GOODWILL), REGARDLESS OF WHETHER ONSEMI HAS BEEN GIVEN NOTICE OF ANY SUCH ALLEGED
 * DAMAGES, AND REGARDLESS OF WHETHER SUCH ALLEGED DAMAGES ARE SOUGHT UNDER CONTRACT, TORT OR OTHER
 * THEORIES OF LAW.
 *
 * This is Reusable Code.
 *
 * Copyright (c) 2025 Semiconductor Components Industries, LLC ("onsemi"), All Rights Reserved.
 *
 * Do not use this Technology unless the recipient has carefully read and agree to these limited
 * terms and conditions. By using this Technology, the recipient expressly agrees to these limited
 * terms and conditions.
 * @endparblock
 * 
 * @file    swmTrace_options.h
 * @brief   This provides definitions of the available configuration options.
 *
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
