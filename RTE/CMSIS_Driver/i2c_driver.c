/**
 * @file i2c_driver.c
 * @brief I2C CMSIS@briefDriver implementation
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

/* ----------------------------------------------------------------------------
 * DMA / CM3 Code Switches
 * ------------------------------------------------------------------------- */

#include <RTE_Device.h>

#define I2C_DMA_CODE_EN ((RTE_I2C0_DMA_EN_DEFAULT && RTE_I2C0_ENABLED) || \
                            (RTE_I2C1_DMA_EN_DEFAULT && RTE_I2C1_ENABLED))

#define I2C_CM33_CODE_EN (!I2C_DMA_CODE_EN)

/* ----------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------- */

/* Device and library headers */
#include <hw.h>
#include <i2c_driver.h>
#include <string.h>
#if I2C_DMA_CODE_EN
    #include <dma_driver.h>
#endif    /* if I2C_DMA_CODE_EN */

/* ----------------------------------------------------------------------------
 * Preprocessor Checks
 * ------------------------------------------------------------------------- */

#if !(RTE_I2C0_ENABLED || RTE_I2C1_ENABLED)
    #warning "No I2C instance enabled in RTE_Device.h!"
#endif    /* #if !(RTE_I2C0_ENABLED || RTE_I2C1_ENABLED) */

#if RTE_I2C0_ENABLED && RTE_I2C1_ENABLED
    #if !(RTE_I2C0_DMA_EN_DEFAULT == RTE_I2C1_DMA_EN_DEFAULT)
        #error "If using both I2C0 and I2C1, the DMA control for both must be the same!"
    #endif    /* #if !(RTE_I2C0_DMA_EN_DEFAULT == RTE_I2C1_DMA_EN_DEFAULT) */
    #if I2C_DMA_CODE_EN && (RTE_I2C0_DMA_CH_DEFAULT == RTE_I2C1_DMA_CH_DEFAULT)
        #error "If using DMA for both I2C0 and I2C1, they must use different DMA channels!"
    #endif    /* #if I2C_DMA_CODE_EN && (RTE_I2C0_DMA_CH_DEFAULT == RTE_I2C1_DMA_CH_DEFAULT) */
#endif    /* #if RTE_I2C0_ENABLED && RTE_I2C1_ENABLED */

/* ----------------------------------------------------------------------------
 * Private Symbolic Constants
 * ------------------------------------------------------------------------- */

/* Driver version */
#define ARM_I2C_DRV_VERSION             ARM_DRIVER_VERSION_MAJOR_MINOR(1, 1)

/* Minimum allowed ratio between SYSCLK and I2CCLK */
#define SYSCLOCK_I2C_MIN_RATIO          (2.5f)

/* Driver status flag definition */
#define I2C_INITIALIZED                 ((uint8_t)(1U))
#define I2C_POWERED                     ((uint8_t)(1U << 1))
#define I2C_CONFIGURED                  ((uint8_t)(1U << 2))

/* Default interrupt configuration for the I2C interface */
#define I2C_DEFAULT_INT_CFG             I2C_REPEATED_START_INT_ENABLE | \
                                        I2C_TX_INT_ENABLE | \
                                        I2C_RX_INT_ENABLE | \
                                        I2C_BUS_ERROR_INT_ENABLE | \
                                        I2C_OVERRUN_INT_DISABLE | \
                                        I2C_STOP_INT_ENABLE | \
                                        I2C_AUTO_ACK_DISABLE

/* This value can be written to the I2C_STATUS register to clear its sticky
 * bits */
#define I2C_STATUS_CLEAR_BITS           I2C_REPEATED_START_DETECTED_CLEAR | \
                                        I2C_STOP_DETECTED_CLEAR | \
                                        I2C_BUS_ERROR_CLEAR | \
                                        I2C_OVERRUN_CLEAR

/* I2C general call address */
#define I2C_GENERAL_CALL_ADDR           (0U)

/* ----------------------------------------------------------------------------
 * Private Macros
 * ------------------------------------------------------------------------- */

/**
 * @brief       SCL Master pre-scale to SYSCLK cycles conversion.
 * @param[in]   x (uint32_t)
 *                  The master prescale.
 */
#define I2C_M_PRESCALE_TO_SYSCLK_CYCLES(x)  (3U * ((x) + 1U))

/**
 * @brief       Check if a repeated start was detected.
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_REPEATED_START_DETECTED(sts) (((sts) & (1 << I2C_STATUS_REPEATED_START_DETECTED_Pos)) \
                                                    == I2C_REPEATED_START_DETECTED)

/**
 * @brief       Check if a bus error was detected.
 * @details     A bus error is detected when the bit driven onto the serial data
 *              line is different from the data read from the serial data line.
 *              As soon as a bus error is detected, the I2C interface stops
 *              driving the SCL and SDA lines.
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_BUS_ERROR_DETECTED(sts)  (((sts) & (1 << I2C_STATUS_BUS_ERROR_Pos)) \
                                                == I2C_BUS_ERROR)

/**
 * @brief       Check if the I2C interface is operating in master mode.
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_MASTER_ACTIVE(sts)       (((sts) & (1 << I2C_STATUS_MASTER_MODE_Pos)) \
                                                == I2C_MASTER_ACTIVE)

/**
 * @brief       Check if a stop condition was detected on the I2C bus.
 * @note        When the device generates a stop condition, this flag is also
 *              set (detects its own stop condition).
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_STOP_DETECTED(sts)       (((sts) & (1 << I2C_STATUS_STOP_DETECTED_Pos)) \
                                                == I2C_STOP_DETECTED)

/**
 * @brief       Check if I2C data is needed or available.
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_DATA_EVENT(sts)          (((sts) & (1 << I2C_STATUS_DATA_EVENT_Pos)) \
                                                == I2C_DATA_EVENT)

/**
 * @brief       Check if the I2C data register holds an address.
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_DATA_IS_ADDR(sts)        (((sts) & (1 << I2C_STATUS_ADDR_DATA_Pos)) \
                                                == I2C_DATA_IS_ADDR)

/**
 * @brief       Check if current transfer is a write.
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_IS_WRITE(sts)            (((sts) & (1 << I2C_STATUS_READ_WRITE_Pos)) \
                                                == I2C_IS_WRITE)

/**
 * @brief       Check if current transfer is a read.
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_IS_READ(sts)             (((sts) & (1 << I2C_STATUS_READ_WRITE_Pos)) \
                                                == I2C_IS_READ)

/**
 * @brief       Check if new I2C TX data can be written.
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_TX_REQ(sts)              (((sts) & (1 << I2C_STATUS_TX_REQ_Pos)) \
                                                == I2C_TX_REQ)

/**
 * @brief       Check if new I2C RX data is available.
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_RX_REQ(sts)              (((sts) & (1 << I2C_STATUS_RX_REQ_Pos)) \
                                                == I2C_RX_REQ)

/**
 * @brief       Check if the I2C interface is clock stretching.
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_CLK_STRETCHED(sts)       (((sts) & (1 << I2C_STATUS_CLK_STRETCH_Pos)) \
                                                == I2C_CLK_STRETCHED)

/**
 * @brief       Check if the last I2C byte was acknowledged (ACKed).
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_HAS_ACK(sts)             (((sts) & (1 << I2C_STATUS_ACK_Pos)) \
                                                == I2C_HAS_ACK)

/**
 * @brief       Check if the last I2C byte was not acknowledged (NACKed).
 * @param[in]   sts (uint32_t)
 *                  The I2C_STATUS register.
 */
#define I2C_STATUS_HAS_NACK(sts)            (((sts) & (1 << I2C_STATUS_ACK_Pos)) \
                                                == I2C_HAS_NACK)

/**
 * @brief       Check if stalled slave.
 * @param[in]   i2c (I2C_RESOURCES *)
 *                  Pointer to I2C resources.
 */
#define STALLED_SLAVE(i2c)                  (!I2C_STATUS_MASTER_ACTIVE((i2c)->reg->STATUS) \
                                                && I2C_STATUS_CLK_STRETCHED((i2c)->reg->STATUS))

/**
 * @brief       Check if stalled slave transmitter.
 * @param[in]   i2c (I2C_RESOURCES *)
 *                  Pointer to I2C resources.
 */
#define STALLED_SLAVE_TRANSMITTER(i2c)      (I2C_STATUS_IS_READ((i2c)->reg->STATUS) \
                                                && STALLED_SLAVE((i2c)))

/**
 * @brief       Check if stalled slave receiver.
 * @param[in]   i2c (I2C_RESOURCES *)
 *                  Pointer to I2C resources.
 */
#define STALLED_SLAVE_RECEIVER(i2c)         (I2C_STATUS_IS_WRITE((i2c)->reg->STATUS) \
                                                && STALLED_SLAVE((i2c)))

/**
 * @brief       Check if the current or most recent transaction was a general
 *              call.
 * @note        The I2C interface will not set the general call status flag if
 *              its slave address is 0 (the general call address), so the slave
 *              address also must be checked to determine if addressed as a
 *              general call.
 * @param[in]   i2c (I2C_RESOURCES *)
 *                  Pointer to I2C resources.
 */
#define IS_GENERAL_CALL(i2c)                (((i2c)->reg->STATUS & I2C_ADDR_GEN_CALL) || \
                                                ((i2c)->info.slave_addr == I2C_GENERAL_CALL_ADDR))

/**
 * @brief       Check if the direction of the current transfer matches what the
 *              driver is expecting.
 * @note        This is ONLY applicable to slave mode.
 * @param[in]   i2c (I2C_RESOURCES *)
 *                  Pointer to I2C resources.
 */
#define DIRECTION_MATCHES(i2c)              ((i2c->reg->STATUS & (1 << I2C_STATUS_READ_WRITE_Pos)) ^ \
                                                (i2c->info.arm_status.direction << I2C_STATUS_READ_WRITE_Pos))

#if I2C_DMA_CODE_EN
/**
 * @brief       Get the DMA transfer counter value.
 * @details     The DMA counter register is split into two fields. We only want
 *              the lower field, so a mask is applied. See documentation for the
 *              `DMA_CNTS` register for more details.
 * @param[in]   i2c (I2C_RESOURCES *)
 *                  Pointer to I2C resources.
 * @param[in]   dma (DRIVER_DMA_t *)
 *                  Pointer to the DMA driver.
 */
#define DMA_XFER_CNT(i2c, dma)              ((int32_t)((dma)->GetCounterValue((i2c)->cfg.dma_ch) \
                                                & DMA_CNTS_TRANSFER_WORD_CNT_Mask))
#endif

/* ----------------------------------------------------------------------------
 * Private Type Definitions
 * ------------------------------------------------------------------------- */

/**
 * @brief       I2C configuration structure.
 */
typedef const struct
{
    uint8_t             scl;        /**< SCL IO Pin number */
    uint8_t             sda;        /**< SDA IO Pin number */
    uint32_t            pin_cfg;    /**< Pin LPF, drive and pull configuration */
#if I2C_DMA_CODE_EN
    DMA_SEL_t           dma_ch;     /**< DMA channel */
    DMA_SignalEvent_t   dma_cb;     /**< DMA event handler function */
#endif    /* if I2C_DMA_CODE_EN */
} I2C_CONFIG_t;

/**
 * @brief       I2C transfer information (run-time) structure.
 */
typedef struct
{
    uint32_t    num;            /**< Total number of bytes to transfer */
    uint8_t    *data;           /**< Pointer to data buffer */
    bool        pending;        /**< If transfer is pending */
    bool        slv_wait;       /**< Slave waiting to be addressed by master */
    bool        data_xfer_actv; /**< Data transfer active */
#if I2C_CM33_CODE_EN
    int32_t     cnt;            /**< Number of bytes transferred (DMA disabled) */
#else    /* #if I2C_CM33_CODE_EN */
    int32_t     dma_cnt_cmpst;  /**< DMA counter compensation value */
#endif    /* #if I2C_CM33_CODE_EN */
} I2C_TRANSFER_INFO_t;

/**
 * @brief       I2C information (run-time) structure.
 */
typedef struct
{
    ARM_I2C_SignalEvent_t   cb_event;           /**< I2C event callback */
    ARM_I2C_STATUS          arm_status;         /**< ARM I2C status flags */
    uint8_t                 state;              /**< Current I2C power state */
    uint32_t                slave_prescale;     /**< I2C slave prescale */
    uint32_t                master_prescale;    /**< I2C master prescale */
    uint8_t                 slave_addr;         /**< Slave address */
    bool                    gen_call_en;        /**< If slave responds to general call */
#if I2C_DMA_CODE_EN
    uint32_t                dma_rx_cfg;         /**< DMA channel receiver configuration */
    uint32_t                dma_tx_cfg;         /**< DMA channel transmitter configuration */
#endif    /* if I2C_DMA_CODE_EN */
} I2C_INFO_t;

/**
 * @brief       I2C resources structure.
 */
typedef struct
{
    I2C_Type     *const reg;        /**< I2C interface pointer */
    const I2C_CONFIG_t  cfg;        /**< I2C configuration structure */
    const IRQn_Type     irqn;       /**< I2C IRQ number */
    I2C_INFO_t          info;       /**< I2C run-time information */
    I2C_TRANSFER_INFO_t xfer;       /**< I2C transfer run-time information */
#if I2C_DMA_CODE_EN
    const DMA_TRG_t     dma_trg;    /**< DMA target selection */
#endif    /* if I2C_DMA_CODE_EN */
} I2C_RESOURCES_t;

/* ----------------------------------------------------------------------------
 * Private Function Prototypes
 * ------------------------------------------------------------------------- */

/**
 * @brief       Configure the I2C interface. Should be used after a reset or
 *              whenever a reconfiguration of the I2C block is required.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 */
static void I2Cx_Configure(I2C_RESOURCES_t *i2c);

/**
 * @brief       Finishes a slave transfer.
 * @details     Generates event flags when an I2C slave transfer is done and
 *              sets the status of the driver to not busy.
 * @assumptions In slave mode and a transfer has finished.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      The event flags.
 */
static uint32_t I2Cx_SlaveFinishTransfer(I2C_RESOURCES_t *i2c,
                                         uint32_t i2c_status);

static uint32_t I2Cx_SlaveAddressed(I2C_RESOURCES_t *i2c, uint32_t i2c_status);

#if I2C_DMA_CODE_EN
/**
 * @brief       Start a DMA driven I2C data transfer.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 */
static inline void I2Cx_StartDMATransfer(I2C_RESOURCES_t *i2c);

/**
 * @brief       End a DMA driven I2C data transfer.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 */
static inline void I2Cx_EndDMATransfer(I2C_RESOURCES_t *i2c);

/**
 * @brief       Master DMA driver event handler.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 */
static void I2Cx_MasterDMAEventHandler(I2C_RESOURCES_t *i2c);

/**
 * @brief       Slave DMA driver event handler.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 */
static void I2Cx_SlaveDMAEventHandler(I2C_RESOURCES_t *i2c);
#endif    /* if I2C_DMA_CODE_EN */

/**
 * @brief       Called by hardware ISR when operating as a master.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      I2C events.
 */
static uint32_t I2Cx_MasterIRQHandler(I2C_RESOURCES_t *i2c);

/**
 * @brief       Called by hardware ISR when operating as a slave.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      I2C events.
 */
static uint32_t I2Cx_SlaveIRQHandler(I2C_RESOURCES_t *i2c);

/**
 * @brief       Get I2C driver version.
 * @return      ARM_DRIVER_VERSION
 */
static ARM_DRIVER_VERSION I2Cx_GetVersion(void);

/**
 * @brief       Get I2C driver capabilities.
 * @return      ARM_I2C_CAPABILITIES
 */
static ARM_I2C_CAPABILITIES I2Cx_GetCapabilities(void);

/**
 * @brief       Initialize I2C driver.
 * @details     Initializes the I2C GPIO pins, registers the callback handler,
 *              and configures the DMA driver if DMA is enabled.
 * @param[in]   cb_event (ARM_I2C_SignalEvent_t)
 *                  Pointer to the I2C event callback function. Pass NULL to
 *                  indicate that no callback is used.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      ARM_DRIVER_OK
 */
static int32_t I2Cx_Initialize(ARM_I2C_SignalEvent_t cb_event,
                              I2C_RESOURCES_t *i2c);

/**
 * @brief       Uninitialize I2C flags, GPIO pins and removes reference to
 *              callback function.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      ARM_DRIVER_OK
 */
static int32_t I2Cx_Uninitialize(I2C_RESOURCES_t *i2c);

/**
 * @brief       Control the I2C interface power.
 * @param[in]   state (ARM_POWER_STATE)
 *                  The desired power state. Can be ARM_POWER_FULL or
 *                  ARM_POWER_OFF (ARM_POWER_LOW is unsupported).
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      ARM_DRIVER_OK if the operation is successful.
 * @return      ARM_DRIVER_ERROR_UNSUPPORTED if argument is invalid.
 */
static int32_t I2Cx_PowerControl(ARM_POWER_STATE state, I2C_RESOURCES_t *i2c);

/**
 * @brief       Start transmitting data as I2C Master.
 * @param[in]   addr (uint32_t)
 *                  Slave address (7-bit).
 * @param[in]   data (const uint8_t *)
 *                  Pointer to data buffer to transmit to I2C Slave.
 * @param[in]   num (uint32_t)
 *                  Number of data bytes to transmit.
 * @param[in]   xfer_pending (bool)
 *                  Transfer operation is pending (stop condition will not be
 *                  generated when transfer is done). Note that the hardware
 *                  will automatically generate a stop condition if an address
 *                  or data byte is not acknowledged by the slave.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      Execution status
 */
static int32_t I2Cx_MasterTransmit(uint32_t addr, const uint8_t *data,
                                  uint32_t num, bool xfer_pending,
                                  I2C_RESOURCES_t *i2c);

/**
 * @brief       Start receiving data as I2C Master.
 * @param[in]   addr (uint32_t)
 *                  Slave address (7-bit).
 * @param[in]   data (uint8_t *)
 *                  Pointer to data buffer to store received data.
 * @param[in]   num (uint32_t)
 *                  Number of data bytes to receive.
 * @param[in]   xfer_pending (bool)
 *                  Transfer operation is pending (stop condition will not be
 *                  generated when transfer is done). Note that the hardware
 *                  will automatically generate a stop condition if the address
 *                  is not acknowledged by the slave.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      Execution status
 */
static int32_t I2Cx_MasterReceive(uint32_t addr, uint8_t *data,
                                 uint32_t num, bool xfer_pending,
                                 I2C_RESOURCES_t *i2c);

/**
 * @brief       Start transmitting data as I2C Slave.
 * @param[in]   data (const uint8_t *)
 *                  Pointer to data buffer to transmit.
 * @param[in]   num (uint32_t)
 *                  Number of data bytes to transmit.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      Execution status
 */
static int32_t I2Cx_SlaveTransmit(const uint8_t *data, uint32_t num,
                                 I2C_RESOURCES_t *i2c);

/**
 * @brief       Start receiving data as I2C Slave.
 * @param[in]   data (uint8_t *)
 *                  Pointer to data buffer to store received data.
 * @param[in]   num (uint32_t)
 *                  Number of data bytes to receive.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      Execution status
 */
static int32_t I2Cx_SlaveReceive(uint8_t *data, uint32_t num,
                                I2C_RESOURCES_t *i2c);

/**
 * @brief       Get transferred data count.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      The number of data bytes that have been transferred. The value
 *              -1 indicates that the slave has not been addressed by a master.
 */
static int32_t I2Cx_GetDataCount(I2C_RESOURCES_t *i2c);

/**
 * @brief       Control I2C Interface
 * @param[in]   control (uint32_t)
 *                  The control operation.
 * @param[in]   arg (uint32_t)
 *                  Argument for operation. Some control operations do not use
 *                  this.
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      Execution status
 */
static int32_t I2Cx_Control(uint32_t control, uint32_t arg,
                           I2C_RESOURCES_t *i2c);

/**
 * @brief       Get I2Cx status
 * @param[in]   i2c (I2C_RESOURCES_t *)
 *                  Pointer to I2C resources.
 * @return      Return I2Cx status as an ARM_I2C_STATUS structure
 */
static ARM_I2C_STATUS I2Cx_GetStatus(I2C_RESOURCES_t *i2c);

/* ----- I2C0 driver wrapper function prototypes -------------------------- */

#if RTE_I2C0_ENABLED

#if RTE_I2C0_DMA_EN_DEFAULT
/* I2C0 DMA event handler function */
static void I2C0_DMAEventHandler(uint32_t event);
#endif    /* if RTE_I2C0_DMA_EN_DEFAULT */

static int32_t I2C0_Initialize(ARM_I2C_SignalEvent_t cb_event);
static int32_t I2C0_Uninitialize(void);
static int32_t I2C0_PowerControl(ARM_POWER_STATE state);
static int32_t I2C0_MasterTransmit(uint32_t addr, const uint8_t *data,
                                   uint32_t num, bool xfer_pending);
static int32_t I2C0_MasterReceive(uint32_t addr, uint8_t *data, uint32_t num,
                                  bool xfer_pending);
static int32_t I2C0_SlaveTransmit(const uint8_t *data, uint32_t num);
static int32_t I2C0_SlaveReceive(uint8_t *data, uint32_t num);
static int32_t I2C0_GetDataCount(void);
static int32_t I2C0_Control(uint32_t control, uint32_t arg);
static ARM_I2C_STATUS I2C0_GetStatus(void);
#endif    /* if RTE_I2C0_ENABLED */

/* ----- I2C1 driver wrapper function prototypes -------------------------- */

#if RTE_I2C1_ENABLED

#if RTE_I2C1_DMA_EN_DEFAULT
/* I2C1 DMA event handler function */
static void I2C1_DMAEventHandler(uint32_t event);
#endif    /* if RTE_I2C1_DMA_EN_DEFAULT */

static int32_t I2C1_Initialize(ARM_I2C_SignalEvent_t cb_event);
static int32_t I2C1_Uninitialize(void);
static int32_t I2C1_PowerControl(ARM_POWER_STATE state);
static int32_t I2C1_MasterTransmit(uint32_t addr, const uint8_t *data,
                                   uint32_t num, bool xfer_pending);
static int32_t I2C1_MasterReceive(uint32_t addr, uint8_t *data, uint32_t num,
                                  bool xfer_pending);
static int32_t I2C1_SlaveTransmit(const uint8_t *data, uint32_t num);
static int32_t I2C1_SlaveReceive(uint8_t *data, uint32_t num);
static int32_t I2C1_GetDataCount(void);
static int32_t I2C1_Control(uint32_t control, uint32_t arg);
static ARM_I2C_STATUS I2C1_GetStatus(void);
#endif    /* if RTE_I2C1_ENABLED */

/* ----------------------------------------------------------------------------
 * Private Global Variables
 * ------------------------------------------------------------------------- */

/* Driver Version */
static const ARM_DRIVER_VERSION DriverVersion =
{
    ARM_I2C_API_VERSION,
    ARM_I2C_DRV_VERSION
};

/* Driver Capabilities */
static const ARM_I2C_CAPABILITIES DriverCapabilities =
{
    .address_10_bit = 0U,    /* 10-bit addresses not supported */
    .reserved = 0U
};

#if RTE_I2C0_ENABLED

/* I2C0 Resources */
static I2C_RESOURCES_t I2C0_Resources =
{
    .reg        = &I2C[0],
    .cfg        = {
        .scl        = RTE_I2C0_SCL_PIN,                     /* I2C0 default scl pin identifier */
        .sda        = RTE_I2C0_SDA_PIN,                     /* I2C0 default sda pin identifier */
        .pin_cfg    = (RTE_I2C0_GPIO_LPF | RTE_I2C0_GPIO_DRIVE | RTE_I2C0_GPIO_PULL),
#if RTE_I2C0_DMA_EN_DEFAULT
        .dma_ch     = (DMA_SEL_t)RTE_I2C0_DMA_CH_DEFAULT,   /* I2C0 default DMA channel */
        .dma_cb     = I2C0_DMAEventHandler                  /* I2C0 DMA event handler */
#endif    /* if RTE_I2C0_DMA_CH_DEFAULT */
    },
    .irqn       = I2C0_IRQn,
    .info       = {0},
    .xfer       = {0},
#if RTE_I2C0_DMA_EN_DEFAULT
    .dma_trg    = DMA_TRG_I2C0,    /* I2C0 default DMA target selection */
#endif    /* if RTE_I2C0_DMA_EN_DEFAULT */
};
#endif    /* if RTE_I2C0_ENABLED */

#if RTE_I2C1_ENABLED

/* I2C1 Resources */
static I2C_RESOURCES_t I2C1_Resources =
{
    .reg        = &I2C[1],
    .cfg        = {
        .scl        = RTE_I2C1_SCL_PIN,                     /* I2C1 default scl pin identifier */
        .sda        = RTE_I2C1_SDA_PIN,                     /* I2C1 default sda pin identifier */
        .pin_cfg    = (RTE_I2C1_GPIO_LPF | RTE_I2C1_GPIO_DRIVE | RTE_I2C1_GPIO_PULL),
#if RTE_I2C1_DMA_EN_DEFAULT
        .dma_ch     = (DMA_SEL_t)RTE_I2C1_DMA_CH_DEFAULT,   /* I2C1 default DMA channel */
        .dma_cb     = I2C1_DMAEventHandler,                 /* I2C1 DMA event handler */
#endif    /* if RTE_I2C1_DMA_EN_DEFAULT */
    },
    .irqn       = I2C1_IRQn,
    .info       = {0},
    .xfer       = {0},
#if RTE_I2C1_DMA_EN_DEFAULT
    .dma_trg    = DMA_TRG_I2C1,    /* I2C1 default DMA target selection */
#endif    /* if RTE_I2C1_DMA_EN_DEFAULT */
};
#endif    /* if RTE_I2C1_ENABLED */

#if I2C_DMA_CODE_EN
/* Import the DMA driver handle */
extern DRIVER_DMA_t Driver_DMA;

/* Prepare DMA driver pointer */
static DRIVER_DMA_t *dma = &Driver_DMA;
#endif    /* if I2C_DMA_CODE_EN */

/* ----------------------------------------------------------------------------
 * Private Function Definitions
 * ------------------------------------------------------------------------- */

static void I2Cx_Configure(I2C_RESOURCES_t *i2c)
{
    /* Reconfigure interrupts, slave address, master and slave prescale.  */
    i2c->reg->CFG =
    (
        I2C_DEFAULT_INT_CFG |
        ((i2c->info.slave_addr << I2C_CFG_SLAVE_ADDRESS_Pos) & I2C_CFG_SLAVE_ADDRESS_Mask) |
        ((i2c->info.slave_prescale << I2C_CFG_SLAVE_PRESCALE_Pos) & I2C_CFG_SLAVE_PRESCALE_Mask) |
        ((i2c->info.master_prescale << I2C_CFG_MASTER_PRESCALE_Pos) & I2C_CFG_MASTER_PRESCALE_Mask)
    );

    i2c->reg->CTRL = I2C_ENABLE;

    /* If slave mode was previously enabled, re-enable it */
    if (i2c->info.gen_call_en || i2c->info.slave_addr)
    {
        i2c->reg->CFG |= I2C_SLAVE_ENABLE;
    }

    SYS_WATCHDOG_REFRESH();
    while (!(i2c->reg->CTRL & I2C_STATUS_ENABLED))
    {

    }
}

static uint32_t I2Cx_SlaveFinishTransfer(I2C_RESOURCES_t *i2c,
                                         uint32_t i2c_status)
{
    /* Verify that the device is in slave mode and that a transfer is currently
     * in progress. */
    if (!((i2c->info.arm_status.mode == ARM_I2C_STATUS_MODE_SLAVE)
            && i2c->info.arm_status.busy))
    {
        return 0U;
    }

    /* When a transfer is finished, the transfer done flag should always be
     * set */
    uint32_t event = ARM_I2C_EVENT_TRANSFER_DONE;

    /* Check if less data was transferred than expected */
    if (I2Cx_GetDataCount(i2c) != i2c->xfer.num)
    {
        event |= ARM_I2C_EVENT_TRANSFER_INCOMPLETE;
    }

    /* Check if general call */
    if (i2c->info.arm_status.general_call)
    {
        event |= ARM_I2C_EVENT_GENERAL_CALL;
    }

    /* Check if a bus error was detected */
    if (I2C_STATUS_BUS_ERROR_DETECTED(i2c_status))
    {
        event |= ARM_I2C_EVENT_BUS_ERROR;
        i2c->info.arm_status.bus_error = 1U;
    }

    if (I2C_STATUS_STOP_DETECTED(i2c_status))
    {
        i2c->reg->STATUS = I2C_STOP_DETECTED_CLEAR;
    }

    i2c->info.arm_status.busy = 0U;

    return event;
}

static uint32_t I2Cx_SlaveAddressed(I2C_RESOURCES_t *i2c, uint32_t i2c_status)
{
    uint32_t event = 0;

    /* Check if addressed as a general call but responding to general calls
     * is disabled. This check is needed since recognizing general calls
     * cannot be disabled in the I2C interface. */
    if (IS_GENERAL_CALL(i2c) && !i2c->info.gen_call_en)
    {
        /* NACK to release the clock */
        Sys_I2C_NACK(i2c->reg);
    }
    /* Else: Check if the operation matches what the application is
     * expecting. For this to be true, both of the following must be true:
     * - The R/W bit received matches the expected direction
     * - The slave is waiting to be addressed
     */
    else if (DIRECTION_MATCHES(i2c) && i2c->xfer.slv_wait)
    {
        i2c->xfer.slv_wait                  = 0U;
        i2c->info.arm_status.general_call   = IS_GENERAL_CALL(i2c);
        i2c->info.arm_status.busy           = 1U;
#if I2C_DMA_CODE_EN
        i2c->xfer.dma_cnt_cmpst             = 0;
        I2Cx_StartDMATransfer(i2c);
#else
        i2c->xfer.cnt                       = 0;
        i2c->xfer.data_xfer_actv            = 1U;
        Sys_I2C_ACK(i2c->reg);
#endif

    }
    else
    {
        /* Addressed as a slave transmitter or receiver but the
         * operation was not set */
        event = (I2C_STATUS_IS_READ(i2c_status)) ?
                 ARM_I2C_EVENT_SLAVE_TRANSMIT :
                 ARM_I2C_EVENT_SLAVE_RECEIVE;
    }

    return event;
}

#if I2C_CM33_CODE_EN
static uint32_t I2Cx_MasterIRQHandler(I2C_RESOURCES_t *i2c)
{
    uint32_t event = 0U;
    uint32_t i2c_status = i2c->reg->STATUS;

    /* Check if a bus error was detected */
    if (I2C_STATUS_BUS_ERROR_DETECTED(i2c_status))
    {
        i2c->reg->STATUS = I2C_BUS_ERROR_CLEAR;

        /* A bus error while a transaction was active indicates that the
         * master lost arbitration. */
        i2c->info.arm_status.arbitration_lost  = 1U;
        i2c->info.arm_status.bus_error         = 1U;

        event |= ARM_I2C_EVENT_TRANSFER_DONE |
                 ARM_I2C_EVENT_TRANSFER_INCOMPLETE |
                 ARM_I2C_EVENT_ARBITRATION_LOST |
                 ARM_I2C_EVENT_BUS_ERROR;

        i2c->info.arm_status.busy = 0U;
    }
    /* Else: Check if the device generated a stop condition. Note that any
     * time that a byte that is sent out (including a slave address + R/W
     * bit) is NACKed, a stop condition will automatically be generated. */
    else if (I2C_STATUS_STOP_DETECTED(i2c_status))
    {
        i2c->reg->STATUS |= I2C_STOP_DETECTED_CLEAR;

        event |= ARM_I2C_EVENT_TRANSFER_DONE;

        /* Check if the stop was generated due to a NACK */
        if (I2C_STATUS_HAS_NACK(i2c_status))
        {
            /* Check if the number of data bytes that were transferred was
             * zero. In this case, the byte that was NACKed was the slave
             * address + R/W bit. */
            if (i2c->xfer.cnt == 0)
            {
                event |= (ARM_I2C_EVENT_ADDRESS_NACK |
                          ARM_I2C_EVENT_TRANSFER_INCOMPLETE);
            }
            /* Else: Check if the byte that was NACKed was a data byte that
             * was written. Since `cnt` must only count bytes that were
             * ACKed during a write and `cnt` is incremented before it is
             * known whether the transmitted byte was ACKed or NACKed, it
             * must be decremented here to compensate. */
            else if (I2C_STATUS_IS_WRITE(i2c_status))
            {
                event |= ARM_I2C_EVENT_TRANSFER_INCOMPLETE;
                i2c->xfer.cnt--;
            }
            /* Else: The master detected its own NACK. Nothing needs to be
             * done in response to the NACK in this case. */
            else
            {

            }
        }

        i2c->info.arm_status.busy = 0U;
    }
    /* Else: Check if all data bytes have been transferred. Note that this
     * condition is only ever reached and true if the transfer pending flag
     * is set since otherwise, finishing a transfer is handled when a stop
     * condition is generated. */
    else if (i2c->xfer.cnt == i2c->xfer.num)
    {
        event |= ARM_I2C_EVENT_TRANSFER_DONE;
        i2c->info.arm_status.busy = 0U;
    }
    /* Else: Check if in read mode. */
    else if (I2C_STATUS_IS_READ(i2c_status))
    {
        /* Check if a byte has been received */
        if (I2C_STATUS_RX_REQ(i2c_status))
        {
            /* Save the received byte */
            i2c->xfer.data[i2c->xfer.cnt++] = i2c->reg->RX_DATA;

            /* Check if the last byte has been read */
            if (i2c->xfer.cnt == i2c->xfer.num)
            {
                /* When a master reads the last byte, it should NACK to
                 * indicate to the slave that the read has finished. */
                Sys_I2C_NACK(i2c->reg);

                /* If a transfer isn't pending, send a stop condition */
                if (!i2c->xfer.pending)
                {
                    i2c->reg->CTRL = I2C_STOP;
                }
            }
            else
            {
                /* Master will continue reading, so ACK */
                Sys_I2C_ACK(i2c->reg);
            }
        }
        /* Else: When in master read frame non-auto ACK mode, an additional
         * interrupt is generated once the address acknowledge bit has been
         * received. No data from the hardware is required in this case;
         * this extra interrupt is used to check if the slave address + R/W
         * bit was ACKed or NACKed. When this is reached, it is known that
         * slave has ACKed. */
        else
        {
            /* This ACK resumes the read but does not send an ACK on the I2C
             * bus. */
            Sys_I2C_ACK(i2c->reg);
        }
    }
    /* Else: In write mode */
    else
    {
        /* Send the next data byte */
        i2c->reg->TX_DATA = i2c->xfer.data[i2c->xfer.cnt++];

        /* Check if all data bytes have been written and a transfer is not
         * pending. */
        if ((i2c->xfer.cnt == i2c->xfer.num) && !(i2c->xfer.pending))
        {
            /* Generate a stop condition after the last byte has been sent. */
            i2c->reg->CTRL = I2C_LAST_DATA;
        }
    }

    return event;
}

static uint32_t I2Cx_SlaveIRQHandler(I2C_RESOURCES_t *i2c)
{
    uint32_t event = 0U;
    uint32_t i2c_status = i2c->reg->STATUS;

    /* Check if repeated start was detected */
    if (I2C_STATUS_REPEATED_START_DETECTED(i2c_status))
    {
        i2c->xfer.data_xfer_actv = 0U;
        event = I2Cx_SlaveFinishTransfer(i2c, i2c_status);
        i2c->reg->STATUS = I2C_REPEATED_START_DETECTED_CLEAR;
    }
    /* Check if the I2C data register holds an address, meaning the slave has
     * recognized its address */
    else if (I2C_STATUS_DATA_IS_ADDR(i2c_status))
    {
        event = I2Cx_SlaveAddressed(i2c, i2c_status);
    }
    /* Else: Check if a bus error or stop was detected */
    else if (I2C_STATUS_BUS_ERROR_DETECTED(i2c_status) ||
            I2C_STATUS_STOP_DETECTED(i2c_status))
    {
        i2c->xfer.data_xfer_actv = 0U;
        event = I2Cx_SlaveFinishTransfer(i2c, i2c_status);
    }
    else if (i2c->xfer.data_xfer_actv)
    {
        /* Check if a new data byte was received */
        if (I2C_STATUS_RX_REQ(i2c_status))
        {
            /* Save the received byte and ACK */
            i2c->xfer.data[i2c->xfer.cnt++] = i2c->reg->RX_DATA;
            Sys_I2C_ACK(i2c->reg);

            /* Finish the transfer if all bytes have been received */
            if ((i2c->xfer.cnt == i2c->xfer.num))
            {
                i2c->xfer.data_xfer_actv = 0U;
            }
        }
        /* Else: Check if the I2C interface is ready to transmit the next data
         * byte */
        else if (I2C_STATUS_TX_REQ(i2c_status))
        {
            /* Check if the last byte that was transmitted was ACKed */
            if (I2C_STATUS_HAS_ACK(i2c_status))
            {
                /* Transmit the next byte */
                i2c->reg->TX_DATA = i2c->xfer.data[i2c->xfer.cnt++];

                /* Finish the transfer if all bytes have been transmitted */
                if (i2c->xfer.cnt == i2c->xfer.num)
                {
                    i2c->xfer.data_xfer_actv = 0U;
                }
            }
            /* Else: The master NACKed the last byte that was transmitted which
             * indicates that it is done reading */
            else
            {
                i2c->xfer.data_xfer_actv = 0U;
            }
        }
        else
        {
            /* The interrupt was triggered by an event that won't be handled.
             * This should not be reached. */
        }
    }
    /* Else: A transfer is not currently active */
    else
    {
        /* Check if the I2C interface either needs data to transmit or has
         * received data. This is true when the master writes or reads more data
         * than the slave was expecting. Since auto-ACK is disabled, these cases
         * must be handled to prevent the slave from stretching the clock
         * indefinitely. */
        if (I2C_STATUS_DATA_EVENT(i2c_status))
        {
            if (I2C_STATUS_IS_READ(i2c_status))
            {
                i2c->reg->TX_DATA = 0U;
            }
            else
            {
                Sys_I2C_NACK(i2c->reg);
            }
        }
        else
        {
            /* The interrupt was triggered by an event that won't be handled.
             * This should not be reached. */
        }
    }

    return event;
}

#else    /* if I2C_CM33_CODE_EN */
static void I2Cx_StartDMATransfer(I2C_RESOURCES_t *i2c)
{
    /* Disable I2C TX and RX interrupts */
    i2c->reg->CFG &= ~(I2C_TX_INT_ENABLE | I2C_RX_INT_ENABLE);

    /* Enable auto-ACK and I2C DMA requests */
    i2c->reg->CFG |= (I2C_AUTO_ACK_ENABLE | I2C_RX_DMA_ENABLE | I2C_TX_DMA_ENABLE);

    /* Start the DMA transfer */
    dma->Start(i2c->cfg.dma_ch, DMA_ENABLE, 0);
}

static void I2Cx_EndDMATransfer(I2C_RESOURCES_t *i2c)
{
    /* Stop the DMA */
    dma->Stop(i2c->cfg.dma_ch);

    /* Disable auto-ACK and I2C DMA requests */
    i2c->reg->CFG &= ~(I2C_AUTO_ACK_ENABLE | I2C_RX_DMA_ENABLE | I2C_TX_DMA_ENABLE);

    /* Enable I2C TX and RX interrupts */
    i2c->reg->CFG |= (I2C_TX_INT_ENABLE | I2C_RX_INT_ENABLE);
}

static void I2Cx_MasterDMAEventHandler(I2C_RESOURCES_t *i2c)
{
    if (dma->GetStatus(i2c->cfg.dma_ch).completed)
    {
        I2Cx_EndDMATransfer(i2c);
    }
    else
    {
        Sys_I2C_LastData(i2c->reg);
    }
}

static void I2Cx_SlaveDMAEventHandler(I2C_RESOURCES_t *i2c)
{
    I2Cx_EndDMATransfer(i2c);

    if ((i2c->info.arm_status.direction == ARM_I2C_STATUS_DIRECTION_TX) &&
        I2C_STATUS_HAS_NACK(i2c->reg->STATUS))
    {
        i2c->xfer.dma_cnt_cmpst = -1;
    }
}

static uint32_t I2Cx_MasterIRQHandler(I2C_RESOURCES_t *i2c)
{
    uint32_t event = 0U;

    /* Get I2C status */
    uint32_t i2c_status = i2c->reg->STATUS;

    /* Check if a bus error was detected */
    if (I2C_STATUS_BUS_ERROR_DETECTED(i2c_status))
    {
        i2c->reg->STATUS = I2C_BUS_ERROR_CLEAR;

        /* A bus error while a transaction was active indicates that the
         * master lost arbitration. */
        i2c->info.arm_status.arbitration_lost   = 1U;
        i2c->info.arm_status.bus_error          = 1U;

        event |= ARM_I2C_EVENT_TRANSFER_DONE |
                 ARM_I2C_EVENT_TRANSFER_INCOMPLETE |
                 ARM_I2C_EVENT_ARBITRATION_LOST |
                 ARM_I2C_EVENT_BUS_ERROR;

        i2c->info.arm_status.busy = 0U;
    }
    /* Else: Check if the device generated a stop condition. Not that any time
     * that a byte that is sent out (including a slave address + R/W bit) is
     * NACKed, a stop condition will automatically be generated. */
    else if (I2C_STATUS_STOP_DETECTED(i2c_status))
    {
        I2Cx_EndDMATransfer(i2c);

        i2c->reg->STATUS |= I2C_STOP_DETECTED_CLEAR;

        event = ARM_I2C_EVENT_TRANSFER_DONE;

        /* Check if the stop was generated due to a NACK */
        if (I2C_STATUS_HAS_NACK(i2c_status))
        {
            /* Check if a data transfer is not yet active. In this case, the
             * byte that was NACKed was the slave address + R/W bit. */
            if (!i2c->xfer.data_xfer_actv)
            {
                event |= (ARM_I2C_EVENT_ADDRESS_NACK |
                          ARM_I2C_EVENT_TRANSFER_INCOMPLETE);
                i2c->xfer.dma_cnt_cmpst = -DMA_XFER_CNT(i2c, dma);
            }
            /* Else: Check if the byte that was NACKed was a data byte that
             * was written. The DMA has provided more data than was actually
             * transferred so the DMA counter compensation value is set
             * accordingly. */
            else if (I2C_STATUS_IS_WRITE(i2c_status))
            {
                i2c->reg->CTRL = I2C_RESET;
                I2Cx_Configure(i2c);
                event |= ARM_I2C_EVENT_TRANSFER_INCOMPLETE;
                i2c->xfer.dma_cnt_cmpst = -2;
            }
            /* Else: The master detected its own NACK. Nothing needs to be
             * done in response to the NACK in this case. */
            else
            {

            }
        }

        i2c->xfer.data_xfer_actv = 0U;

        i2c->info.arm_status.busy = 0U;
    }
    /* Else: Check if a data transfer is active. If it is at this point, then
     * the transfer is complete. */
    else if (i2c->xfer.data_xfer_actv)
    {
        /* Ensure the RX REQ bit is cleared by reading from the RX_DATA
         * register */
        uint8_t __attribute((unused)) temp = i2c->reg->RX_DATA;

        i2c->reg->CTRL = I2C_STOP;
    }
    /* Else: The address + R/W bit that was sent out was ACKED */
    else
    {
        i2c->xfer.data_xfer_actv = 1U;
        I2Cx_StartDMATransfer(i2c);
    }

    return event;
}

static uint32_t I2Cx_SlaveIRQHandler(I2C_RESOURCES_t *i2c)
{
    uint32_t event = 0U;
    uint32_t i2c_status = i2c->reg->STATUS;

    /* Check if the I2C data register holds an address, meaning the slave has
     * recognized its address */
    if (I2C_STATUS_DATA_IS_ADDR(i2c_status))
    {
        event = I2Cx_SlaveAddressed(i2c, i2c_status);
    }
    /* Check if a bus error or stop was detected */
    else if (I2C_STATUS_REPEATED_START_DETECTED(i2c_status) ||
             I2C_STATUS_BUS_ERROR_DETECTED(i2c_status) ||
             I2C_STATUS_STOP_DETECTED(i2c_status))
    {
        if (I2C_STATUS_IS_READ(i2c_status) &&
            !dma->GetStatus(i2c->cfg.dma_ch).completed)
        {
            i2c->xfer.dma_cnt_cmpst = -1;
        }

        I2Cx_EndDMATransfer(i2c);

        event = I2Cx_SlaveFinishTransfer(i2c, i2c_status);
    }
    /* Else: Check if the I2C interface either needs data to transmit or has
     * received data. This is true when the master writes or reads more data
     * than the slave was expecting. Since auto-ACK is disabled, these cases
     * must be handled to prevent the slave from stretching the clock
     * indefinitely. */
    else if (I2C_STATUS_DATA_EVENT(i2c_status))
    {
        if (I2C_STATUS_IS_READ(i2c_status))
        {
            i2c->reg->TX_DATA = 0U;
        }
        else
        {
            Sys_I2C_NACK(i2c->reg);
        }
    }
    else
    {
        /* Do nothing. This should not be reached. */
    }

    return event;
}
#endif    /* if I2C_DMA_CODE_EN */

static ARM_DRIVER_VERSION I2Cx_GetVersion(void)
{
    return DriverVersion;
}

static ARM_I2C_CAPABILITIES I2Cx_GetCapabilities(void)
{
    return DriverCapabilities;
}

static int32_t I2Cx_Initialize(ARM_I2C_SignalEvent_t cb_event,
                              I2C_RESOURCES_t *i2c)
{
    if (i2c->info.state & I2C_INITIALIZED)
    {
        return ARM_DRIVER_OK;
    }

    /* Configure the GPIOs for I2C operation */
    Sys_I2C_GPIOConfig(i2c->reg, i2c->cfg.pin_cfg,
                       i2c->cfg.scl, i2c->cfg.sda);

    /* Reset run-time information structures */
    memset(&i2c->info, 0, sizeof(I2C_INFO_t));
    memset(&i2c->xfer, 0, sizeof(I2C_TRANSFER_INFO_t));

    /* Register the callback event handler */
    i2c->info.cb_event = cb_event;

#if I2C_DMA_CODE_EN

    /* Prepare receiver mode DMA configuration */
    DMA_CFG_t dmaCfgR =
    {
        .src_sel        = i2c->dma_trg,
        .src_step       = DMA_CFG0_SRC_ADDR_STATIC,
        .dst_sel        = DMA_TRG_MEM,
        .dst_step       = DMA_CFG0_DEST_ADDR_INCR_1,
        .ch_priority    = DMA_CH_PRI_0,
        .word_size      = DMA_CFG0_DEST_WORD_SIZE_8BITS_TO_8BITS
    };

    /* Prepare transmitter mode DMA configuration */
    DMA_CFG_t dmaCfgT =
    {
        .src_sel        = DMA_TRG_MEM,
        .src_step       = DMA_CFG0_SRC_ADDR_INCR_1,
        .dst_sel        = i2c->dma_trg,
        .dst_step       = DMA_CFG0_DEST_ADDR_STATIC,
        .ch_priority    = DMA_CH_PRI_0,
        .word_size      = DMA_CFG0_DEST_WORD_SIZE_8BITS_TO_8BITS
    };

    /* Configure the DMA channel */
    dma->Configure(i2c->cfg.dma_ch, &dmaCfgR , i2c->cfg.dma_cb);

    /* Store the RX DMA configuration */
    i2c->info.dma_rx_cfg = dma->CreateConfigWord(&dmaCfgR);

    /* Store the TX DMA configuration */
    i2c->info.dma_tx_cfg = dma->CreateConfigWord(&dmaCfgT);

#endif    /* #if I2C_DMA_CODE_EN */

    /* Indicate that initialization is done */
    i2c->info.state = I2C_INITIALIZED;

    /* Return OK */
    return ARM_DRIVER_OK;
}

static int32_t I2Cx_Uninitialize(I2C_RESOURCES_t *i2c)
{
#if I2C_DMA_CODE_EN

    /* Stop the DMA data transfer */
    dma->Stop(i2c->cfg.dma_ch);
#endif    /* if I2C_DMA_CODE_EN */

    uint8_t i2c_instance = i2c->irqn - I2C0_IRQn;

    /* Disable and clear pending I2C interrupts */
    NVIC_DisableIRQ(i2c->irqn);
    NVIC_ClearPendingIRQ(i2c->irqn);

    /* Disable the I2C interface */
    i2c->reg->CTRL = I2C_DISABLE;
    SYS_WATCHDOG_REFRESH();
    while((i2c->reg->CTRL & (1 << I2C_CTRL_ENABLE_STATUS_Pos)) !=
            I2C_STATUS_DISABLED)
    {

    }

    /* Reset pin configuration */
    GPIO->CFG[i2c->cfg.scl] = GPIO_MODE_DISABLE;
    GPIO->CFG[i2c->cfg.sda] = GPIO_MODE_DISABLE;

    /* Reset GPIO_SRC_I2C configuration */
    GPIO_SRC_I2C[i2c_instance].SDA_BYTE = (uint8_t)I2C_SDA_SRC_CONST_HIGH_BYTE;
    GPIO_SRC_I2C[i2c_instance].SCL_BYTE = (uint8_t)I2C_SCL_SRC_CONST_HIGH_BYTE;

    /* Clear I2C state */
    i2c->info.state = 0U;

    /* Remove reference to callback function */
    i2c->info.cb_event = NULL;

    /* Return OK */
    return ARM_DRIVER_OK;
}

static int32_t I2Cx_PowerControl(ARM_POWER_STATE state, I2C_RESOURCES_t *i2c)
{
    switch (state)
    {
        case ARM_POWER_OFF:
        {
            /* Disable I2C interrupts */
            NVIC_DisableIRQ(i2c->irqn);

            /* Disable the I2C block */
            i2c->reg->CTRL = I2C_DISABLE;

            /* Reset run-time information structures */
            memset((void *)&i2c->info.arm_status, 0, sizeof(ARM_I2C_STATUS));
            memset(&i2c->xfer, 0, sizeof(I2C_TRANSFER_INFO_t));

            /* Clear powered flag */
            i2c->info.state &= ~I2C_POWERED;
        }
        break;

        case ARM_POWER_FULL:
        {
            /* Check if i2c was initialized */
            if ((i2c->info.state & I2C_INITIALIZED) == 0U)
            {
                return ARM_DRIVER_ERROR;
            }
            /* Check if i2c was already powered up */
            if ((i2c->info.state & I2C_POWERED) != 0U)
            {
                return ARM_DRIVER_OK;
            }

            /* Reset run-time information structures */
            memset((void *)&i2c->info.arm_status, 0, sizeof(ARM_I2C_STATUS));
            memset(&i2c->xfer, 0, sizeof(I2C_TRANSFER_INFO_t));

            /* Clear and enable I2C interrupts */
            NVIC_ClearPendingIRQ(i2c->irqn);
            NVIC_EnableIRQ(i2c->irqn);

            /* Set powered flag */
            i2c->info.state |= I2C_POWERED;
        }
        break;

        case ARM_POWER_LOW:
        default:
            /* Return unsupported operation error */
            return ARM_DRIVER_ERROR_UNSUPPORTED;
    }

    /* Return OK */
    return ARM_DRIVER_OK;
}

static int32_t I2Cx_MasterTransmit(uint32_t addr, const uint8_t *data,
                                  uint32_t num, bool xfer_pending,
                                  I2C_RESOURCES_t *i2c)
{
#if I2C_DMA_CODE_EN
    if (xfer_pending == true)
    {
        return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
#endif    /* #if I2C_DMA_CODE_EN */

    /* Check input parameters */
    if ((data == NULL) || (num == 0U) || (addr > 0x7FU))
    {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    /* Check if driver was configured */
    if ((i2c->info.state & I2C_CONFIGURED) == 0U)
    {
        return ARM_DRIVER_ERROR;
    }

    /* Check if I2C driver is busy */
    if (i2c->info.arm_status.busy || STALLED_SLAVE(i2c))
    {
        return ARM_DRIVER_ERROR_BUSY;
    }

    /* Temporarily disable interrupts */
    NVIC_DisableIRQ(i2c->irqn);

    /* Set/clear CMSIS I2C status flags */
    i2c->info.arm_status.busy             = 1U;
    i2c->info.arm_status.mode             = ARM_I2C_STATUS_MODE_MASTER;
    i2c->info.arm_status.direction        = ARM_I2C_STATUS_DIRECTION_TX;
    i2c->info.arm_status.arbitration_lost = 0U;

    /* Clear I2C_STATUS register sticky bits */
    i2c->reg->STATUS = I2C_STATUS_CLEAR_BITS;

    /* Set transfer info */
    i2c->xfer.num               = num;
    i2c->xfer.data              = (uint8_t *)data;
    i2c->xfer.pending           = xfer_pending;
    i2c->xfer.slv_wait          = 0U;
    i2c->xfer.data_xfer_actv    = 0U;
#if I2C_CM33_CODE_EN
    i2c->xfer.cnt               = 0;
#else
    i2c->xfer.dma_cnt_cmpst     = 0;

    /* Prepare DMA for transmission */
    dma->SetConfigWord(i2c->cfg.dma_ch, i2c->info.dma_tx_cfg);

    /* Prepare DMA buffer configuration */
    DMA_ADDR_CFG_t buffCfg =
    {
        .src_addr     = (uint8_t *)data,
        .dst_addr     = &i2c->reg->TX_DATA,
        .counter_len  = num,
        .transfer_len = num
    };

    /* Configure the DMA channel */
    dma->ConfigureAddr(i2c->cfg.dma_ch, &buffCfg);

#endif    /* if I2C_CM33_CODE_EN */

    /* Generate start condition and send slave address + write bit */
    Sys_I2C_StartWrite(i2c->reg, addr);

    /* Clear and enable I2C interrupts */
    NVIC_ClearPendingIRQ(i2c->irqn);
    NVIC_EnableIRQ(i2c->irqn);

    return ARM_DRIVER_OK;
}

static int32_t I2Cx_MasterReceive(uint32_t addr, uint8_t *data,
                                 uint32_t num, bool xfer_pending,
                                 I2C_RESOURCES_t *i2c)
{
#if I2C_DMA_CODE_EN
    if (xfer_pending == true)
    {
        return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
#endif    /* #if I2C_DMA_CODE_EN */

    /* Check input parameters */
    if ((data == NULL) || (num == 0U) || (addr > 0x7FU) ||
        (addr == I2C_GENERAL_CALL_ADDR))
    {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    /* Check if driver was configured */
    if ((i2c->info.state & I2C_CONFIGURED) == 0U)
    {
        return ARM_DRIVER_ERROR;
    }

    /* Check if I2C driver is busy */
    if (i2c->info.arm_status.busy || STALLED_SLAVE(i2c))
    {
        return ARM_DRIVER_ERROR_BUSY;
    }

    /* Temporarily disable interrupts */
    NVIC_DisableIRQ(i2c->irqn);

    /* Set/clear CMSIS I2C status flags */
    i2c->info.arm_status.busy             = 1U;
    i2c->info.arm_status.mode             = ARM_I2C_STATUS_MODE_MASTER;
    i2c->info.arm_status.direction        = ARM_I2C_STATUS_DIRECTION_RX;
    i2c->info.arm_status.arbitration_lost = 0U;

    /* Clear I2C_STATUS register sticky bits */
    i2c->reg->STATUS = I2C_STATUS_CLEAR_BITS;

    /* Set transfer info */
    i2c->xfer.num               = num;
    i2c->xfer.data              = (uint8_t *)data;
    i2c->xfer.pending           = xfer_pending;
    i2c->xfer.slv_wait          = 0U;
    i2c->xfer.data_xfer_actv    = 0U;
#if I2C_CM33_CODE_EN
    i2c->xfer.cnt               = 0;
#else
    i2c->xfer.dma_cnt_cmpst     = 0;

    /* Prepare DMA for transmission */
    dma->SetConfigWord(i2c->cfg.dma_ch, i2c->info.dma_rx_cfg);

    /* Prepare DMA buffer configuration */
    DMA_ADDR_CFG_t buffCfg =
    {
        .src_addr     = &i2c->reg->RX_DATA,
        .dst_addr     = (uint8_t *)data,
        .counter_len  = num - 1,
        .transfer_len = num
    };

    /* Configure the DMA channel */
    dma->ConfigureAddr(i2c->cfg.dma_ch, &buffCfg);

#endif    /* if I2C_CM33_CODE_EN */

    /* Generate start condition and send slave address + read bit */
    Sys_I2C_StartRead(i2c->reg, addr);

    /* Clear and enable I2C interrupts */
    NVIC_ClearPendingIRQ(i2c->irqn);
    NVIC_EnableIRQ(i2c->irqn);

    return ARM_DRIVER_OK;
}

static int32_t I2Cx_SlaveTransmit(const uint8_t *data, uint32_t num,
                                 I2C_RESOURCES_t *i2c)
{
    /* Check input parameters */
    if ((data == NULL) || (num == 0U))
    {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    /* Check if driver was configured */
    if ((i2c->info.state & I2C_POWERED) == 0U)
    {
        return ARM_DRIVER_ERROR;
    }

    /* Check if slave mode is enabled */
    if (!(i2c->reg->CFG & I2C_SLAVE_ENABLE))
    {
        return ARM_DRIVER_ERROR;
    }

    /* Check if I2C driver is busy */
    if (i2c->info.arm_status.busy || STALLED_SLAVE_RECEIVER(i2c) ||
        I2C_STATUS_MASTER_ACTIVE(i2c->reg->STATUS))
    {
        return ARM_DRIVER_ERROR_BUSY;
    }

    /* Temporarily disable interrupts */
    NVIC_DisableIRQ(i2c->irqn);

    /* Set/clear CMSIS I2C status flags */
    i2c->info.arm_status.mode          = ARM_I2C_STATUS_MODE_SLAVE;
    i2c->info.arm_status.direction     = ARM_I2C_STATUS_DIRECTION_TX;
    i2c->info.arm_status.general_call  = 0U;
    i2c->info.arm_status.bus_error     = 0U;

    /* Clear I2C_STATUS register sticky bits */
    i2c->reg->STATUS = I2C_STATUS_CLEAR_BITS;

    /* Set transfer info */
    i2c->xfer.num               = num;
    i2c->xfer.data              = (uint8_t *)data;
    i2c->xfer.slv_wait          = 1U;
    i2c->xfer.data_xfer_actv    = 0U;
#if I2C_CM33_CODE_EN
    i2c->xfer.cnt               = -1;
#else
    i2c->xfer.dma_cnt_cmpst     = 0U;

    /* Prepare DMA for transmission */
    dma->SetConfigWord(i2c->cfg.dma_ch, i2c->info.dma_tx_cfg);

    /* Prepare DMA buffer configuration */
    DMA_ADDR_CFG_t buffCfg =
    {
        .src_addr     = (uint8_t *)data,
        .dst_addr     = &i2c->reg->TX_DATA,
        .counter_len  = num,
        .transfer_len = num
    };

    /* Configure the DMA channel */
    dma->ConfigureAddr(i2c->cfg.dma_ch, &buffCfg);

#endif    /* if I2C_CM33_CODE_EN */

    /* Re-enable interrupts */
    NVIC_EnableIRQ(i2c->irqn);

    if (STALLED_SLAVE_TRANSMITTER(i2c))
    {
        /* Temporarily disable interrupts */
        NVIC_DisableIRQ(i2c->irqn);

        /* Resume the transaction */
        I2Cx_SlaveAddressed(i2c, i2c->reg->STATUS);

        /* Re-enable interrupts */
        NVIC_EnableIRQ(i2c->irqn);
    }

    return ARM_DRIVER_OK;
}

static int32_t I2Cx_SlaveReceive(uint8_t *data, uint32_t num,
                                I2C_RESOURCES_t *i2c)
{
    /* Check input parameters */
    if ((data == NULL) || (num == 0U))
    {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    /* Check if driver was configured */
    if ((i2c->info.state & I2C_POWERED) == 0U)
    {
        return ARM_DRIVER_ERROR;
    }

    /* Check if slave mode is enabled */
    if (!(i2c->reg->CFG & I2C_SLAVE_ENABLE))
    {
        return ARM_DRIVER_ERROR;
    }

    /* Check if I2C driver is busy */
    if (i2c->info.arm_status.busy || STALLED_SLAVE_TRANSMITTER(i2c) ||
        I2C_STATUS_MASTER_ACTIVE(i2c->reg->STATUS))
    {
        return ARM_DRIVER_ERROR_BUSY;
    }

    /* Temporarily disable interrupts */
    NVIC_DisableIRQ(i2c->irqn);

    /* Set/clear CMSIS I2C status flags */
    i2c->info.arm_status.mode          = ARM_I2C_STATUS_MODE_SLAVE;
    i2c->info.arm_status.direction     = ARM_I2C_STATUS_DIRECTION_RX;
    i2c->info.arm_status.general_call  = 0U;
    i2c->info.arm_status.bus_error     = 0U;

    /* Clear I2C_STATUS register sticky bits */
    i2c->reg->STATUS = I2C_STATUS_CLEAR_BITS;

    /* Set transfer info */
    i2c->xfer.num               = num;
    i2c->xfer.data              = data;
    i2c->xfer.slv_wait          = 1U;
    i2c->xfer.data_xfer_actv    = 0U;
#if I2C_CM33_CODE_EN
    i2c->xfer.cnt               = -1;
#else
    i2c->xfer.dma_cnt_cmpst     = 0U;

    /* Prepare DMA for transmission */
    dma->SetConfigWord(i2c->cfg.dma_ch, i2c->info.dma_rx_cfg);

    /* Prepare DMA buffer configuration */
    DMA_ADDR_CFG_t buffCfg =
    {
        .src_addr     = &i2c->reg->RX_DATA,
        .dst_addr     = data,
        .counter_len  = i2c->xfer.num = num,
        .transfer_len = i2c->xfer.num = num
    };

    /* Configure the DMA channel */
    dma->ConfigureAddr(i2c->cfg.dma_ch, &buffCfg);

#endif    /* if I2C_CM33_CODE_EN */

    /* Re-enable interrupts */
    NVIC_EnableIRQ(i2c->irqn);

    if (STALLED_SLAVE_RECEIVER(i2c))
    {
        /* Temporarily disable interrupts */
        NVIC_DisableIRQ(i2c->irqn);

        /* Resume the transaction */
        I2Cx_SlaveAddressed(i2c, i2c->reg->STATUS);

        /* Re-enable interrupts */
        NVIC_EnableIRQ(i2c->irqn);
    }

    return ARM_DRIVER_OK;
}

static int32_t I2Cx_GetDataCount(I2C_RESOURCES_t *i2c)
{
    int32_t data_count;

    if (i2c->xfer.slv_wait)
    {
        /* Slave has not yet been addressed by master, -1 must be returned */
        data_count = -1;
    }
    else
    {
#if I2C_DMA_CODE_EN

        data_count = DMA_XFER_CNT(i2c, dma) + i2c->xfer.dma_cnt_cmpst;

#else    /* if I2C_DMA_CODE_EN */

        /* Return counter value */
        data_count = i2c->xfer.cnt;
#endif    /* if I2C_DMA_CODE_EN */
    }

    return data_count;
}

static int32_t I2Cx_Control(uint32_t control, uint32_t arg,
                           I2C_RESOURCES_t *i2c)
{
    uint32_t bus_speed, delay_cycles;

    if ((i2c->info.state & I2C_POWERED) == 0U)
    {
        /* I2C not powered */
        return ARM_DRIVER_ERROR;
    }

    switch (control)
    {
        case ARM_I2C_OWN_ADDRESS:
        {
            /* If transfer operation in progress */
            if (i2c->info.arm_status.busy)
            {
                return ARM_DRIVER_ERROR_BUSY;
            }

            if ((arg & ~ARM_I2C_ADDRESS_GC) > 0x7FU)
            {
                return ARM_DRIVER_ERROR_PARAMETER;
            }

            if (arg == 0U)
            {
                /* The slave address 0 disables slave mode and clears any
                 * assigned slave address */
                i2c->info.gen_call_en  = 0U;
                i2c->info.slave_addr   = 0U;
                i2c->reg->CFG &= ~(I2C_CFG_SLAVE_ADDRESS_Mask | I2C_SLAVE_ENABLE);
            }
            else
            {
                i2c->info.gen_call_en  = (arg & ARM_I2C_ADDRESS_GC) ? 1U : 0U;
                i2c->info.slave_addr   = arg & 0x7F;
                i2c->reg->CFG = ((i2c->reg->CFG & ~I2C_CFG_SLAVE_ADDRESS_Mask) |
                                 ((arg << I2C_CFG_SLAVE_ADDRESS_Pos) &
                                 I2C_CFG_SLAVE_ADDRESS_Mask) | I2C_SLAVE_ENABLE);
            }
        }
        break;

        case ARM_I2C_BUS_SPEED:
        {
            /* Check if the I2C driver is busy */
            if (i2c->info.arm_status.busy || STALLED_SLAVE(i2c) ||
                I2C_STATUS_MASTER_ACTIVE(i2c->reg->STATUS))
            {
                return ARM_DRIVER_ERROR_BUSY;
            }

            /* Set Bus Speed */
            switch (arg)
            {
                case ARM_I2C_BUS_SPEED_STANDARD:
                {
                    /* Standard Speed (100kHz) */
                    bus_speed = 100000;
                }
                break;

                case ARM_I2C_BUS_SPEED_FAST:
                {
                    /* Fast Speed     (400kHz) */
                    bus_speed = 400000;
                }
                break;

                case ARM_I2C_BUS_SPEED_FAST_PLUS:
                {
                    /* Fast+ Speed    (  1MHz) */
                    bus_speed = 1000000;
                }
                break;

                case ARM_I2C_BUS_SPEED_HIGH:

                /* High Speed     (3.4MHz) not supported */
                default:
                    return ARM_DRIVER_ERROR_UNSUPPORTED;
            }

            /* If system clock is not fast enough */
            if (SystemCoreClock < SYSCLOCK_I2C_MIN_RATIO * bus_speed)
            {
                return ARM_DRIVER_ERROR_UNSUPPORTED;
            }

            /* Calculate I2C Master prescale */
            i2c->info.master_prescale = SystemCoreClock / (3 * bus_speed) - 1;

            /* Calculate I2C slave prescale */
            if (SystemCoreClock < 4000000)
            {
                i2c->info.slave_prescale = I2C_SLAVE_PRESCALE_1;
            }
            else if (SystemCoreClock >= 4000000 && SystemCoreClock < 8000000)
            {
                i2c->info.slave_prescale = I2C_SLAVE_PRESCALE_2;
            }
            else if (SystemCoreClock >= 8000000 && SystemCoreClock < 12000000)
            {
                i2c->info.slave_prescale = I2C_SLAVE_PRESCALE_3;
            }
            else if (SystemCoreClock >= 12000000 && SystemCoreClock < 16000000)
            {
                i2c->info.slave_prescale = I2C_SLAVE_PRESCALE_4;
            }
            else if (SystemCoreClock >= 16000000 && SystemCoreClock < 20000000)
            {
                i2c->info.slave_prescale = I2C_SLAVE_PRESCALE_5;
            }
            else if (SystemCoreClock >= 20000000 && SystemCoreClock < 24000000)
            {
                i2c->info.slave_prescale = I2C_SLAVE_PRESCALE_6;
            }
            else if (SystemCoreClock >= 24000000 && SystemCoreClock < 48000000)
            {
                i2c->info.slave_prescale = I2C_SLAVE_PRESCALE_7;
            }
            else /* SystemCoreClock >= 48000000 */
            {
                i2c->info.slave_prescale = I2C_SLAVE_PRESCALE_14;
            }

            /* Reset and reconfigure I2C block */
            i2c->reg->CTRL = I2C_RESET;
            I2Cx_Configure(i2c);

            i2c->info.state |= I2C_CONFIGURED;
        }
        break;

        case ARM_I2C_BUS_CLEAR:
        {
            uint8_t i2c_instance = i2c->irqn - I2C0_IRQn;

            i2c->reg->CTRL = I2C_DISABLE;

            /* Reset GPIO_I2C_SRC configuration */
            GPIO_SRC_I2C[i2c_instance].SDA_BYTE = (uint8_t)I2C_SDA_SRC_CONST_HIGH;
            GPIO_SRC_I2C[i2c_instance].SCL_BYTE = (uint8_t)I2C_SCL_SRC_CONST_HIGH;

            /* Configure I2C pins as GPIO output */
            GPIO->CFG[i2c->cfg.scl] = GPIO_MODE_GPIO_OUT;
            GPIO->CFG[i2c->cfg.sda] = GPIO_MODE_GPIO_OUT;
            NVIC_DisableIRQ(i2c->irqn);

            /* Delay cycles for the standard 100KHz speed */
            delay_cycles = SystemCoreClock / 100000;

            /* Issue 9 clock pulses on SCL line */
            for (uint8_t i = 0U; i < 9U; i++)
            {
                /* Clock high */
                Sys_GPIO_Set_High(i2c->cfg.scl);
                Sys_Delay(delay_cycles);

                /* Clock low */
                Sys_GPIO_Set_Low(i2c->cfg.scl);
                Sys_Delay(delay_cycles);
            }

            /* Manually generates STOP condition - SDA goes high while SCL is
             * high, in order to end transaction. */
            Sys_GPIO_Set_Low(i2c->cfg.sda);
            Sys_Delay(delay_cycles);
            Sys_GPIO_Set_High(i2c->cfg.scl);
            Sys_Delay(delay_cycles);
            Sys_GPIO_Set_High(i2c->cfg.sda);

            /* Reconfigure SCL and SDA Pins as I2C peripheral pins */
            Sys_I2C_GPIOConfig(i2c->reg, i2c->cfg.pin_cfg,
                               i2c->cfg.scl, i2c->cfg.sda);

            /* Reset and reconfigure I2C block */
            i2c->reg->CTRL = I2C_RESET;
            I2Cx_Configure(i2c);

            /* Clear pending and enable I2C interrupts */
            NVIC_ClearPendingIRQ(i2c->irqn);
            NVIC_EnableIRQ(i2c->irqn);

            /* Send event */
            if (i2c->info.cb_event)
            {
                i2c->info.cb_event(ARM_I2C_EVENT_BUS_CLEAR);
            }
        }
        break;

        case ARM_I2C_ABORT_TRANSFER: 
        {
            if (i2c->info.arm_status.busy)
            {
                /* Disable IRQ temporarily */
                NVIC_DisableIRQ(i2c->irqn);

#if I2C_DMA_CODE_EN
                I2Cx_EndDMATransfer(i2c);
#endif    /* #if I2C_DMA_CODE_EN */

                /* If master, send stop */
                if (i2c->info.arm_status.mode == ARM_I2C_STATUS_MODE_MASTER)
                {
                    /* Delay 8 SCL cycles to allow any byte transfer currently
                     * in progress to finish. */
                    Sys_Delay(
                        8 * I2C_M_PRESCALE_TO_SYSCLK_CYCLES(i2c->info.master_prescale)
                    );

                    i2c->reg->CTRL = I2C_STOP;

                    /* Wait for stop to be issued */
                    SYS_WATCHDOG_REFRESH();
                    while (!I2C_STATUS_STOP_DETECTED(i2c->reg->STATUS))
                    {
                        /* Return error if a bus error is detected */
                        if (i2c->reg->STATUS & I2C_BUS_ERROR)
                        {
                            return ARM_DRIVER_ERROR;
                        }
                    }

                    i2c->reg->STATUS |= I2C_STOP_DETECTED_CLEAR;
                }
                /* Else: In slave mode */
                else
                {
                    /* Reset and reconfigure I2C block */
                    i2c->reg->CTRL = I2C_RESET;
                    I2Cx_Configure(i2c);
                }

                /* Clear run-time transfer info */
                memset(&i2c->xfer, 0, sizeof(I2C_TRANSFER_INFO_t));

                /* Clear ARM status structure */
                memset((void *)&i2c->info.arm_status, 0, sizeof(ARM_I2C_STATUS));

                /* Clear pending and enable I2C interrupts */
                NVIC_ClearPendingIRQ(i2c->irqn);
                NVIC_EnableIRQ(i2c->irqn);
            }
            else if (I2C_STATUS_CLK_STRETCHED(i2c->reg->STATUS))
            {
                Sys_I2C_NACK(i2c->reg);
            }
            else
            {
                /* No active transfer to abort */
            }
        }
        break;

        default:
            return ARM_DRIVER_ERROR;
    }
    return ARM_DRIVER_OK;
}

static ARM_I2C_STATUS I2Cx_GetStatus(I2C_RESOURCES_t *i2c)
{
    return (i2c->info.arm_status);
}

/* ----- I2C0 driver wrapper function definitions -------------------------- */

#if RTE_I2C0_ENABLED

#if RTE_I2C0_DMA_EN_DEFAULT
static void I2C0_DMAEventHandler(uint32_t event)
{
    /* Disable I2C Interrupts */
    NVIC_DisableIRQ(I2C0_Resources.irqn);

    if (I2C0_Resources.info.arm_status.mode == ARM_I2C_STATUS_MODE_MASTER)
    {
        I2Cx_MasterDMAEventHandler(&I2C0_Resources);
    }
    else
    {
        I2Cx_SlaveDMAEventHandler(&I2C0_Resources);
    }

    /* Re-enable I2C interrupts */
    NVIC_EnableIRQ(I2C0_Resources.irqn);
}
#endif    /* if RTE_I2C0_DMA_EN_DEFAULT */

static int32_t I2C0_Initialize(ARM_I2C_SignalEvent_t cb_event)
{
    return I2Cx_Initialize(cb_event, &I2C0_Resources);
}

static int32_t I2C0_Uninitialize(void)
{
    return I2Cx_Uninitialize(&I2C0_Resources);
}

static int32_t I2C0_PowerControl(ARM_POWER_STATE state)
{
    return I2Cx_PowerControl(state, &I2C0_Resources);
}

static int32_t I2C0_MasterTransmit(uint32_t addr, const uint8_t *data,
                                   uint32_t num, bool xfer_pending)
{
    return I2Cx_MasterTransmit(addr, data, num, xfer_pending, &I2C0_Resources);
}

static int32_t I2C0_MasterReceive(uint32_t addr, uint8_t *data, uint32_t num,
                                  bool xfer_pending)
{
    return I2Cx_MasterReceive(addr, data, num, xfer_pending, &I2C0_Resources);
}

static int32_t I2C0_SlaveTransmit(const uint8_t *data, uint32_t num)
{
    return I2Cx_SlaveTransmit(data, num, &I2C0_Resources);
}

static int32_t I2C0_SlaveReceive(uint8_t *data, uint32_t num)
{
    return I2Cx_SlaveReceive(data, num, &I2C0_Resources);
}

static int32_t I2C0_GetDataCount(void)
{
    return I2Cx_GetDataCount(&I2C0_Resources);
}

static int32_t I2C0_Control(uint32_t control, uint32_t arg)
{
    return I2Cx_Control(control, arg, &I2C0_Resources);
}

static ARM_I2C_STATUS I2C0_GetStatus(void)
{
    return I2Cx_GetStatus(&I2C0_Resources);
}

#endif    /* if RTE_I2C0_ENABLED */

/* ----- I2C1 driver wrapper function definitions -------------------------- */

#if RTE_I2C1_ENABLED

#if RTE_I2C1_DMA_EN_DEFAULT
static void I2C1_DMAEventHandler(uint32_t event)
{
    /* Disable I2C Interrupts */
    NVIC_DisableIRQ(I2C1_Resources.irqn);

    if (I2C1_Resources.info.arm_status.mode == ARM_I2C_STATUS_MODE_MASTER)
    {
        I2Cx_MasterDMAEventHandler(&I2C1_Resources);
    }
    else
    {
        I2Cx_SlaveDMAEventHandler(&I2C1_Resources);
    }

    /* Re-enable I2C interrupts */
    NVIC_EnableIRQ(I2C1_Resources.irqn);
}
#endif    /* if RTE_I2C1_DMA_EN_DEFAULT */

static int32_t I2C1_Initialize(ARM_I2C_SignalEvent_t cb_event)
{
    return I2Cx_Initialize(cb_event, &I2C1_Resources);
}

static int32_t I2C1_Uninitialize(void)
{
    return I2Cx_Uninitialize(&I2C1_Resources);
}

static int32_t I2C1_PowerControl(ARM_POWER_STATE state)
{
    return I2Cx_PowerControl(state, &I2C1_Resources);
}

static int32_t I2C1_MasterTransmit(uint32_t addr, const uint8_t *data,
                                   uint32_t num, bool xfer_pending)
{
    return I2Cx_MasterTransmit(addr, data, num, xfer_pending, &I2C1_Resources);
}

static int32_t I2C1_MasterReceive(uint32_t addr, uint8_t *data, uint32_t num,
                                  bool xfer_pending)
{
    return I2Cx_MasterReceive(addr, data, num, xfer_pending, &I2C1_Resources);
}

static int32_t I2C1_SlaveTransmit(const uint8_t *data, uint32_t num)
{
    return I2Cx_SlaveTransmit(data, num, &I2C1_Resources);
}

static int32_t I2C1_SlaveReceive(uint8_t *data, uint32_t num)
{
    return I2Cx_SlaveReceive(data, num, &I2C1_Resources);
}

static int32_t I2C1_GetDataCount(void)
{
    return I2Cx_GetDataCount(&I2C1_Resources);
}

static int32_t I2C1_Control(uint32_t control, uint32_t arg)
{
    return I2Cx_Control(control, arg, &I2C1_Resources);
}

static ARM_I2C_STATUS I2C1_GetStatus(void)
{
    return I2Cx_GetStatus(&I2C1_Resources);
}

#endif    /* if RTE_I2C1_ENABLED */

/* ----------------------------------------------------------------------------
 * IRQ Handler Definitions
 * ------------------------------------------------------------------------- */

#if RTE_I2C0_ENABLED
void I2C0_IRQHandler(void)
{
    uint32_t event;

    /* Check if the driver is in master mode and the device has not been
     * addressed as a slave */
    if ((I2C0_Resources.info.arm_status.mode == ARM_I2C_STATUS_MODE_MASTER) &&
        !I2C_STATUS_DATA_IS_ADDR(I2C0_Resources.reg->STATUS))
    {
        event = I2Cx_MasterIRQHandler(&I2C0_Resources);
    }
    else
    {
        event = I2Cx_SlaveIRQHandler(&I2C0_Resources);
    }

    /* Check if an event happened and the application registered a callback */
    if (event && I2C0_Resources.info.cb_event)
    {
        I2C0_Resources.info.cb_event(event);
    }
}
#endif    /* if RTE_I2C0_ENABLED */

#if RTE_I2C1_ENABLED
void I2C1_IRQHandler(void)
{
    uint32_t event;

    /* Check if the driver is in master mode and the device has not been
     * addressed as a slave */
    if ((I2C1_Resources.info.arm_status.mode == ARM_I2C_STATUS_MODE_MASTER) &&
        !I2C_STATUS_DATA_IS_ADDR(I2C1_Resources.reg->STATUS))
    {
        event = I2Cx_MasterIRQHandler(&I2C1_Resources);
    }
    else
    {
        event = I2Cx_SlaveIRQHandler(&I2C1_Resources);
    }

    /* Check if an event happened and the application registered a callback */
    if (event && I2C1_Resources.info.cb_event)
    {
        I2C1_Resources.info.cb_event(event);
    }
}
#endif    /* if RTE_I2C1_ENABLED */

/* ----------------------------------------------------------------------------
 * Driver Control Blocks
 * ------------------------------------------------------------------------- */

#if RTE_I2C0_ENABLED
/* I2C0 Driver Control Block */
ARM_DRIVER_I2C Driver_I2C0 =
{
    I2Cx_GetVersion,
    I2Cx_GetCapabilities,
    I2C0_Initialize,
    I2C0_Uninitialize,
    I2C0_PowerControl,
    I2C0_MasterTransmit,
    I2C0_MasterReceive,
    I2C0_SlaveTransmit,
    I2C0_SlaveReceive,
    I2C0_GetDataCount,
    I2C0_Control,
    I2C0_GetStatus
};
#endif    /* if RTE_I2C0_ENABLED */

#if RTE_I2C1_ENABLED
/* I2C1 Driver Control Block */
ARM_DRIVER_I2C Driver_I2C1 =
{
    I2Cx_GetVersion,
    I2Cx_GetCapabilities,
    I2C1_Initialize,
    I2C1_Uninitialize,
    I2C1_PowerControl,
    I2C1_MasterTransmit,
    I2C1_MasterReceive,
    I2C1_SlaveTransmit,
    I2C1_SlaveReceive,
    I2C1_GetDataCount,
    I2C1_Control,
    I2C1_GetStatus
};
#endif    /* if RTE_I2C1_ENABLED */
