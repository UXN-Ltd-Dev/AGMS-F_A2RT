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
 * @file    system_rsl15.c
 * @brief   CMSIS Cortex-M33 Device Peripheral Access Layer Source File.
 *
 */

#include <stdint.h>
#include <hw.h>
#include "system_rsl15.h"

#include "flash_rom.h"
#if defined (__ARM_FEATURE_CMSE) &&  (__ARM_FEATURE_CMSE == 3U)
  #include "partition_ARMCM33.h"
#endif /* if defined (__ARM_FEATURE_CMSE) &&  (__ARM_FEATURE_CMSE == 3U) */

/* ----------------------------------------------------------------------------
 * Variables
 * ------------------------------------------------------------------------- */

/* Required system clock frequency variable (Cortex-M33 clock frequency) */
uint32_t SystemCoreClock __attribute__ ((section(".systemclock"))) = DEFAULT_FREQ;

/*----------------------------------------------------------------------------
 * Forward Declarations for helper functions
 *----------------------------------------------------------------------------*/
static uint32_t GetSystemClockfromRCCLK(void);

static uint32_t GetSystemClockfromSTANDBYCLK(void);

static uint32_t GetSystemClockfromRFCLK(void);

static uint32_t GetSystemClockfromSWCLK(void);

static uint32_t GetSystemClockfromUnknownCLK(void);

/*----------------------------------------------------------------------------
 * System initialization function
 *----------------------------------------------------------------------------*/
#ifndef NON_SECURE
void SystemInit(void)
{
    Sys_NVIC_DisableAllInt();    /* Disable all interrupts */
    Sys_NVIC_ClearAllPendingInt();    /* Clear all pending interrupts */

    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);
    __set_FAULTMASK(FAULTMASK_DISABLE_INTERRUPTS);

#if defined (__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
    SCB->VTOR = (unsigned int)(&ISR_Vector_Table);
#endif /* if defined (__VTOR_PRESENT) && (__VTOR_PRESENT == 1U) */

#if defined (__FPU_USED) && (__FPU_USED == 1U)
    SCB->CPACR |= ((3U << 10U * 2U) |           /* enable CP10 Full Access */
                   (3U << 11U * 2U));         /* enable CP11 Full Access */
#endif /* if defined (__FPU_USED) && (__FPU_USED == 1U) */

#ifdef UNALIGNED_SUPPORT_DISABLE
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;
#endif /* ifdef UNALIGNED_SUPPORT_DISABLE */

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
    TZ_SAU_Setup();
#endif /* if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U) */

    /* Update SystemCoreClock by reading system registers
     * also update the flash registers */
    SystemCoreClockUpdate();
}
#endif

#ifdef NON_SECURE
void SystemInit(void)
{
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);
    __set_FAULTMASK(FAULTMASK_DISABLE_INTERRUPTS);

#if defined (__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
    SCB->VTOR = (unsigned int) (&ISR_Vector_Table);
#endif

#if defined (__FPU_USED) && (__FPU_USED == 1U)
    SCB->CPACR |= ((3U << 10U*2U) |           /* enable CP10 Full Access */
                   (3U << 11U*2U));           /* enable CP11 Full Access */
#endif
}
#endif

/*----------------------------------------------------------------------------
 * System Core Clock update function
 *----------------------------------------------------------------------------*/

void SystemCoreClockUpdate(void)
{
    uint32_t clk_src_sel = (CLK->SYS_CFG & CLK_SYS_CFG_SYSCLK_SRC_SEL_Mask);

    switch (clk_src_sel)
    {
        case SYSCLK_CLKSRC_RCCLK:
        {
            SystemCoreClock = GetSystemClockfromRCCLK();
        }
        break;

        case SYSCLK_CLKSRC_STANDBYCLK:
        {
            SystemCoreClock = GetSystemClockfromSTANDBYCLK();
        }
        break;

        case SYSCLK_CLKSRC_RFCLK:
        {
            SystemCoreClock = GetSystemClockfromRFCLK();
        }
        break;

        case SYSCLK_CLKSRC_SWCLK:
        {
            SystemCoreClock = GetSystemClockfromSWCLK();
        }
        break;

        default:
        {
            SystemCoreClock = GetSystemClockfromUnknownCLK();
        }
        break;
    }

    /* Initialize the delay settings for the flash interface (if it is enabled)
     * to ensure proper operation when using the new clock setting. */
    if ((FLASH[0].IF_STATUS & (0x1U << FLASH_IF_STATUS_ISOLATE_STATUS_Pos)) !=
         FLASH_ISOLATE)
    {
         Flash_Initialize(0, (FlashClockFrequency_t)(SystemCoreClock));
    }
}

/* Get clock frequency from internal RC oscillator (3/12/24/48 MHz steps)  */
static uint32_t GetSystemClockfromRCCLK(void)
{
    uint32_t clk = DEFAULT_FREQ;
    uint32_t rc_fsel = ACS->RCOSC_CTRL & ACS_RCOSC_CTRL_RC_FSEL_Mask;
    if (rc_fsel == RC_OSC_3MHZ)
    {
        clk = 3000000;
    }
    else if (rc_fsel == RC_OSC_12MHZ)
    {
        clk = 12000000;
    }
    else if (rc_fsel == RC_OSC_24MHZ)
    {
        clk = 24000000;
    }
    else
    {
        /* Nothing to do here */
    }
    return clk;
}

/* Get clock frequency from low frequency 32 Khz internal oscillator */
static uint32_t GetSystemClockfromSTANDBYCLK(void)
{
    /* SYSCLK is source from the standby oscillator or 32 kHz crystal.
     * We assume 32768 is close enough in this case, since there isn't
     * significant variation for this value. */
    return STANDBYCLK_DEFAULT_FREQ;
}

static uint32_t GetSystemClockfromRFCLK(void)
{
    uint32_t clk = DEFAULT_FREQ;
    if (RF0_REG33->CK_DIV_1_6_CK_DIV_1_6_BYTE > 0)
    {
        clk = RFCLK_BASE_FREQ /
              (RF0_REG33->CK_DIV_1_6_CK_DIV_1_6_BYTE);
    }
    else
    {
        /* Nothing to do here. Clock is not generated from RFCLK */
    }
    return clk;
}

/*
 * Get the serial wire debug port clock (external) */
static uint32_t GetSystemClockfromSWCLK(void)
{
    /* @ref SWCLK_MAX_FREQ is assumed to be the serial wire or JTCK clock
     * update this definition for a different clock */
    return SWCLK_MAX_FREQ / (CLK_SYS_CFG->SWCLK_PRESCALE_BYTE + 1);
}

static uint32_t GetSystemClockfromUnknownCLK(void)
{
    return DEFAULT_FREQ;
}
