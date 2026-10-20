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
 * @file    syscalls.c
 * @brief   Stubs for Newlib system calls.
 *
 */

#ifdef __GNUC__
#include <sys/stat.h>
#include <sys/unistd.h>

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif    /* ifdef __cplusplus */

/* Function Prototypes */
int _link(char *old, char *new);
int _close(int file);
int _fstat(int file, struct stat *st);
int _isatty(int file);
int _lseek(int file, int ptr, int dir);
void _exit(int status);
int _kill(int pid, int sig);
int _getpid(void);
int _write(int file, char* ptr, int len);
int _read(int file, char* ptr, int len);

/**
 * @brief       Generic function to perform a semihosting call
 * @param[in]   reason specifies the particular semihosting operation to perform
 * @param[in]   arg  pointer pointing to an argument block that contains additional
 *                   information needed for that operation
 * @return      returns the value contained in the r0 register
 */
static inline int do_Semihosting (int reason, void * arg)
{
    int value;
    __asm volatile ("mov r0, %1; mov r1, %2; bkpt %a3; mov %0, r0"
           : "=r" (value) /* Outputs */
           : "r" (reason), "r" (arg), "i" (0xAB) /* Inputs */
           : "r0", "r1", "r2", "r3", "ip", "lr", "memory", "cc"
            /* Clobbers r0, r1, r2, r3, ip, lr, memory and cc if in supervisor mode */);
    /* Just to be on the safe side with clobbered registers,
     * we are doing the same thing as newlib, which has the following comment:
     *
     * Accordingly to page 13-77 of ARM DUI 0040D other registers
     * can also be clobbered.  Some memory positions may also be
     * changed by a system call, so they should not be kept in registers. */

    return value;
}

/**
 * @brief       Establish a new name for an existing file.
 * @param[in]   old Old name of the file
 * @param[in]   new New name of the file
 * @return      returns -1
 */
int __attribute__((weak)) _link(char *old, char *new)
{
    return -1;
}

/**
 * @brief       Close a file.
 * @param[in]   file File descriptor
 * @return      returns -1
 */
int __attribute__((weak)) _close(int file)
{
    return -1;
}

/**
 * @brief       Status of an open file.
 * @param[in]   file File descriptor
 * @param[in]   *st Pointer to the stat struct
 * @return      returns 0
 */
int __attribute__((weak)) _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;

    return 0;
}

/**
 * @brief       Query whether output stream is a terminal.
 * @param[in]   file File descriptor
 * @return      returns 1
 */
int __attribute__((weak)) _isatty(int file)
{
    return 1;
}

/**
 * @brief       Set position in a file.
 * @param[in]   file File descriptor
 * @param[in]   ptr Offset Value
 * @param[in]   dir Position used as a reference for the offset
 * @return      returns 0
 */
int __attribute__((weak)) _lseek(int file, int ptr, int dir)
{
    return 0;
}

/**
 * @brief       Exit a program without cleaning up files.
 * @param[in]   status Exit Status
 */
void __attribute__((weak)) _exit(int status)
{
    while (1) {}
}

/**
 * @brief       Send a kill signal.
 * @param[in]   pid Process id
 * @param[in]   sig Signal to send
 * @return      returns -1
 */
int __attribute__((weak)) _kill(int pid, int sig)
{
    return -1;
}

/**
 * @brief       Get the process id.
 * @return      returns -1
 */
int __attribute__((weak)) _getpid(void)
{
    return -1;
}

/**
 * @brief       Read from a file.
 * @param[in]   file File descriptor
 * @param[in]   ptr Pointer to a buffer where read data will be stored
 * @param[in]   len Number of bytes to read from file
 * @return      return the actual number of bytes read
 *              if no error occurs, otherwise return -1
 */
int __attribute__((weak)) _read (int file, char *ptr, int len)
{
    if (file != STDIN_FILENO)
    {
        return -1;
    }

    int block[3];
    block[0] = file;
    block[1] = (int)ptr;
    block[2] = len;

    int ret = do_Semihosting(0x06, block);

    if (ret < 0)
    {
        return -1;
    }

    return ret;
}

/**
 * @brief       Write to a file.
 * @param[in]   file File descriptor
 * @param[in]   ptr Pointer to a buffer that contains data to write
 * @param[in]   len Number of bytes that should be written
 * @return      return len if no error occurs, otherwise return -1
 */
int __attribute__((weak)) _write(int file, char *ptr, int len)
{
    if (file != STDOUT_FILENO && file != STDERR_FILENO)
    {
        return -1;
    }

    int block[3];
    block[0] = file;
    block[1] = (int)ptr;
    block[2] = len;

    int ret = do_Semihosting(0x05, block);

    if (ret < 0)
    {
        return -1;
    }

    return len;
}

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* __cplusplus */
#endif    /* ifdef __GNUC__ */
