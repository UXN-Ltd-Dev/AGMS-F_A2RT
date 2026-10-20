/**
 * @file  app_cem102.h
 * @brief CEM102 low power application initialization header file
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

#ifndef APP_MAX30123_H_
#define APP_MAX30123_H_

#include <hw.h>
/* Include the CEM102 support library */
#include "cem102_driver.h"

#include "app_max30123.h"
/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif    /* ifdef __cplusplus */
/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/
#define 	_MAX30123_INFO_DEBUG_	1

/* ================================================================================
 *  디버그 스위치 (헤더에 없으면 기본값)
 * ================================================================================ */
#ifndef _MAX30123_INFO_DEBUG_
#define _MAX30123_INFO_DEBUG_       0
#endif
#ifndef _EIS_FIFO_INFO_DEBUG_
#define _EIS_FIFO_INFO_DEBUG_       0
#endif
#ifndef _IMPEDANCE_RESULT_DEBUG_
#define _IMPEDANCE_RESULT_DEBUG_    0
#endif
#ifndef _CA_FIFO_VERIFY_
#define _CA_FIFO_VERIFY_            1       /* 첫 검증 동안 1: 폴링마다 FIFO 구성 로그 */
#endif


#define PART_IDENTIFIER             0xff
/****************************************************************************
 * MAX30123 Defines
 ****************************************************************************/
#define MAX30123_PART_ID              0x51
#define MAX30123_CONVERT_MODE         0x60

#define MAX30123_STEP_CALIBRATION        0
#define MAX30123_STEP_DC_CURRENT         1
#define MAX30123_STEP_CHRONO_A	         2

#define MAX30123_AUTO_MODE		        1
#define MAX30123_MANUAL_MODE	        0


/* ==========================================================================
 * REGISTER MAP
 * ========================================================================== */
/* ---- STATUS ---- */

#define MAX30123_REG_STATUS1            0x00u
#define MAX30123_REG_STATUS2            0x01u

#define STATUS1_A_FULL                  (1u << 7)
#define STATUS1_CLK_CAL_DONE            (1u << 6)
#define STATUS1_ADC_DATA_RDY            (1u << 5)
#define STATUS1_SEQ_RDY                 (1u << 3)
#define STATUS1_INVALID_CFG             (1u << 2)
#define STATUS1_NVM_CHECKSUM_ERROR      (1u << 1)
#define STATUS1_PWR_RDY                 (1u << 0)
#define STATUS1_PWR_RDY_OK              0u      /* TODO: 극성 데이터시트 확인 */

#define STATUS2_BTN_DET                 (1u << 6)
#define STATUS2_V2I_DATA_HI             (1u << 5)
#define STATUS2_V2I_DATA_LO             (1u << 4)
#define STATUS2_WE1_DATA_HI             (1u << 1)
#define STATUS2_WE1_DATA_LO             (1u << 0)

/* ---- INTERRUPT ENABLE ---- */
#define MAX30123_REG_INT_EN1            0x04u
#define MAX30123_REG_INT_EN2            0x05u

#define INT_EN1_A_FULL_EN               (1u << 7)
#define INT_EN1_CLK_CAL_DONE_EN         (1u << 6)
#define INT_EN1_ADC_DATA_RDY_EN         (1u << 5)
#define INT_EN1_SEQ_RDY_EN              (1u << 3)
#define INT_EN1_INVALID_CFG_EN          (1u << 2)

/* ---- FIFO ---- */
#define MAX30123_REG_FIFO_WR_PTR        0x07u
#define MAX30123_REG_FIFO_RD_PTR        0x08u
#define MAX30123_REG_FIFO_COUNTER1      0x09u   /* [7]=DATA_COUNT[8], [6:0]=OVF_COUNTER */
#define MAX30123_REG_FIFO_COUNTER2      0x0Au   /* DATA_COUNT[7:0] */
#define MAX30123_REG_FIFO_DATA          0x0Bu   /* burst 3-byte / sample */
#define MAX30123_REG_FIFO_CONFIG1       0x0Cu   /* FIFO_A_FULL[7:0] */
#define MAX30123_REG_FIFO_CONFIG2       0x0Du

#define FIFO_CFG2_FIFO_MARK             (1u << 5)
#define FIFO_CFG2_FLUSH_FIFO            (1u << 4)
#define FIFO_CFG2_FIFO_STAT_CLR         (1u << 3)
#define FIFO_CFG2_A_FULL_TYPE           (1u << 2)
#define FIFO_CFG2_FIFO_RO               (1u << 1)   /* ★ 반드시 OFF */

#define MAX30123_FIFO_DEPTH             256u

/* FIFO data_type (FIFO_DATA[20:16]) */
#define FIFO_TYPE_WE1_OFFSET_CURR       0x00u
#define FIFO_TYPE_WE1_PSTAT_CURR        0x01u
#define FIFO_TYPE_WE1_PRE               0x02u   /* 실제로는 FIFO 미저장 */
#define FIFO_TYPE_WE1_STEP              0x03u
#define FIFO_TYPE_WE1_POST              0x04u
#define FIFO_TYPE_OFFSET_VOLTAGE        0x10u
#define FIFO_TYPE_WE1_VOLTAGE           0x11u
#define FIFO_TYPE_RE1_VOLTAGE           0x13u
#define FIFO_TYPE_CE1_VOLTAGE           0x15u
#define FIFO_TYPE_GR1_VOLTAGE           0x17u
#define FIFO_TYPE_GPIO1_VOLTAGE         0x19u
#define FIFO_TYPE_GPIO2_VOLTAGE         0x1Au
#define FIFO_TYPE_VBAT_VOLTAGE          0x1Bu
#define FIFO_TYPE_VREF_VOLTAGE          0x1Cu
#define FIFO_TYPE_VDD_VOLTAGE           0x1Du
#define FIFO_TYPE_TEMPERATURE           0x1Eu

#define FIFO_RAW_MARKER                 0xFFFFFEu
#define FIFO_RAW_INVALID                0xFFFFFFu

/* ---- SYSTEM CONTROL ---- */
#define MAX30123_REG_SYSCTRL1           0x0Eu
#define MAX30123_REG_SYSCTRL2           0x0Fu

#define SYSCTRL1_WAKE_OUT_EN            (1u << 6)
#define SYSCTRL1_WAKE_PO_MASK           (3u << 4)
#define SYSCTRL1_NVM_CHECK              (1u << 3)
#define SYSCTRL1_OPEN_CKTS_EN           (1u << 2)
#define SYSCTRL1_SHDN                   (1u << 1)
#define SYSCTRL1_RESET                  (1u << 0)

#define LOW_POWER_MODE_ACTIVE           0x0u
#define LOW_POWER_MODE_LP_STATE         0x1u
#define LOW_POWER_MODE_LP_WAKE_LO       0x3u
#define LOW_POWER_MODE_LP_WAKE_HI       0x4u
#define SYSCTRL2_WAKE_DEGLITCH          (1u << 4)
#define SYSCTRL2_LDO_BYPASS             (1u << 3)

/* ---- CLOCK / CAL ---- */
#define MAX30123_REG_SR_CLK_FINE_TUNE   0x11u
#define MAX30123_REG_ADC_CLK_FINE_TUNE  0x12u
#define MAX30123_REG_CLK_V_SPI_CNT_MSB  0x13u
#define MAX30123_REG_CLK_V_SPI_CNT_LSB  0x14u

/* ---- AUTO_CLK_DIV (tSSP) ----
 * tSSP[s] = AUTO_CLK_DIV[23:0] / 32768,  범위 0x000080~0xFFFFFF (3.906ms~512s) */
#define MAX30123_REG_AUTO_CLK_DIV_MSB   0x15u
#define MAX30123_REG_AUTO_CLK_DIV_MID   0x16u
#define MAX30123_REG_AUTO_CLK_DIV_LSB   0x17u

/* ---- CHRONO_CLK_DIV (ISP) ----
 * ISP[s] = CHRONO_CLK_DIV[15:0] / 32768,  최소 div 128 (3.906ms), 최대 2s */
#define MAX30123_REG_CHRONO_CLK_DIV_MSB 0x18u
#define MAX30123_REG_CHRONO_CLK_DIV_LSB 0x19u

/* ---- DAC ---- */
#define MAX30123_REG_DACA_MSB           0x1Au   /* DACA_CODE[9:2] */
#define MAX30123_REG_DACB_MSB           0x1Bu   /* DACB_CODE[9:2] */
#define MAX30123_REG_DACAB_LSB          0x1Cu

#define MAX30123_REG_DAC_CTRL           0x20u
#define DAC_CTRL_DAC_EN                 (1u << 0)
#define DAC_CTRL_PED_SHIFT              1u

/* ---- PSTAT CONFIG ---- */
#define MAX30123_REG_PSTAT_CFG          0x22u
#define PSTAT_CFG_POR_DUTY              (1u << 3)
#define PSTAT_CFG_HALF_CP               (1u << 2)
#define PSTAT_CFG_AFE_CHOP_EN           (1u << 1)
#define PSTAT_CFG_HALF_IB               (1u << 0)

#define PSTAT_CFG_RUN                   (PSTAT_CFG_POR_DUTY | PSTAT_CFG_AFE_CHOP_EN)       /* 0x0A */
#define PSTAT_CFG_IDLE                  (PSTAT_CFG_POR_DUTY | PSTAT_CFG_HALF_CP | \
                                         PSTAT_CFG_AFE_CHOP_EN | PSTAT_CFG_HALF_IB)        /* 0x0F */

/* ---- WE1 / CE1 ---- */
#define MAX30123_REG_WE1_CFG1           0x23u
#define WE1_CFG1_AMP_EN                 (1u << 7)
#define WE1_CFG1_CHOP_EN                (1u << 6)
#define WE1_CFG1_DAC_MX_SHIFT           4u
#define WE1_CFG1_RS                     (1u << 3)
#define WE1_CFG1_SWA                    (1u << 2)
#define WE1_CFG1_SWB                    (1u << 1)
#define WE1_CFG1_SRA                    (1u << 0)

#define MAX30123_REG_WE1_CFG2           0x24u
#define WE1_CFG2_IOS_MODE               (1u << 7)
#define WE1_CFG2_OFFSET_SEL_SHIFT       5u      /* [6:5] 00=0% 01=10% 10=20% 11=50% */
#define WE1_CFG2_STORE_OFFSET           (1u << 3)
#define WE1_CFG2_FS_SHIFT               0u      /* [2:0] 000=64nA .. 100=1024nA */

#define MAX30123_REG_CE1_CFG            0x29u
#define CE1_CFG_AMP_EN                  (1u << 7)
#define CE1_CFG_DAC_MX_SHIFT            4u
#define CE1_CFG_SRB                     (1u << 0)

/* ---- CHRONO A / B ---- */
#define MAX30123_REG_CHRONO_AMPLITUDE   0x35u   /* 스텝 전압 [mV] */

#define MAX30123_REG_CHRONO_A_CFG1      0x37u   /* PRE_SMP/POST_PULSE/ALT_POL/AUTO_SUB/DELAY */
#define MAX30123_REG_CHRONO_A_CFG2      0x38u   /* STEP_SMP  (값+1) */
#define MAX30123_REG_CHRONO_A_CFG3      0x39u   /* REC_SMP   (값 그대로) */
#define MAX30123_REG_CHRONO_A_CFG4      0x3Au   /* BLANK_SMP (값+1) */
#define MAX30123_REG_CHRONO_A_CFG5      0x3Bu   /* REPEAT    (값+1) */
#define MAX30123_REG_CHRONO_A_CFG6      0x3Cu   /* OFFSET_SEL / ADC_FS_CHRONO_A */

#define MAX30123_REG_CHRONO_B_CFG1      0x3Du
#define MAX30123_REG_CHRONO_B_CFG2      0x3Eu
#define MAX30123_REG_CHRONO_B_CFG3      0x3Fu
#define MAX30123_REG_CHRONO_B_CFG4      0x40u
#define MAX30123_REG_CHRONO_B_CFG5      0x41u
#define MAX30123_REG_CHRONO_B_CFG6      0x42u

/* CHRONO_A_CFG1 (0x37) 비트 */
#define CA_CFG1_PRE_SMP_SHIFT           6u      /* 00→1, 01→2, 10→3, 11→4 */
#define CA_CFG1_POST_PULSE_EN           (1u << 5)
#define CA_CFG1_ALT_POL                 (1u << 4)
#define CA_CFG1_AUTO_SUBTRACT           (1u << 3)
/* [2:0] DELAY */

/* PRE=4, POST_PULSE=0, ALT_POL=0, AUTO_SUBTRACT=1, DELAY=0 */
#define CA_CFG1_VALUE                   ((3u << CA_CFG1_PRE_SMP_SHIFT) | \
                                         CA_CFG1_AUTO_SUBTRACT)        /* 0xC8 */

/* ---- WE1_I_OFFSET (read-only, SPI 버스트로 읽을 것) ---- */
#define MAX30123_REG_WE1_I_OFFSET_MSB   0x80u
#define MAX30123_REG_WE1_I_OFFSET_LSB   0x81u

/* ---- CONVERT_MODE / SEQUENCER ---- */
#define MAX30123_REG_CONVERT_MODE       0x60u
#define CONVERT_MODE_SEQ_RESTART        (1u << 7)
#define CONVERT_MODE_AUTO               (1u << 1)
#define CONVERT_MODE_MANUAL             (0u << 1)
#define CONVERT_MODE_CONVERT            (1u << 0)
#define CONVERT_MODE_STOP               (0u << 0)

#define MAX30123_REG_SEQUENCE_COUNTER   0x61u

#define MAX30123_REG_M0_CFG             0x62u
#define MAX30123_REG_M0_DLY             0x63u
#define MAX30123_REG_M1_CFG             0x64u
#define MAX30123_REG_M1_DLY             0x65u
#define MAX30123_REG_M2_CFG             0x66u
#define MAX30123_REG_M3_CFG             0x68u
#define MAX30123_REG_M4_CFG             0x6Au

/* Mn_MODE[2:0] = bits[7:5],  Mn_SRD[4:0] = bits[4:0] (M1~M4) */
#define MN_MODE_SHIFT                   5u
#define MN_MODE_DISABLED                0x0u
#define MN_MODE_PSTAT                   0x1u
#define MN_MODE_CHRONO_A                0x2u
#define MN_MODE_CHRONO_B                0x3u
#define MN_MODE_AP                      0x4u
#define MN_MODE_TEMPERATURE             0x6u
#define MN_MODE_SYS_VOLTAGE             0x7u
#define MN_SRD_MASK                     0x1Fu

/* ---- Mn I/V CONV ---- */
#define MAX30123_REG_M0_CONV            0x70u
#define MAX30123_REG_M0_AVG             0x71u
#define MAX30123_REG_M1_CONV            0x72u
#define MAX30123_REG_M1_AVG             0x73u
#define MAX30123_REG_M2_CONV            0x74u
#define MAX30123_REG_M2_AVG             0x75u
#define MAX30123_REG_M3_CONV            0x76u
#define MAX30123_REG_M3_AVG             0x77u
#define MAX30123_REG_M4_CONV            0x78u
#define MAX30123_REG_M4_AVG             0x79u

#define MN_SMP_AVE_SHIFT                3u
#define MN_SMP_AVE_MASK                 0xF8u
#define MN_MEAS_AVE_MASK                0x07u
#define SMP_AVE_N(n)                    ((uint8_t)(((n) - 1u) << MN_SMP_AVE_SHIFT))

#define MEAS_AVE_1                      0x00u
#define MEAS_AVE_2                      0x01u
#define MEAS_AVE_4                      0x02u
#define MEAS_AVE_8                      0x03u
#define MEAS_AVE_16                     0x04u
#define MEAS_AVE_32                     0x05u
#define MEAS_AVE_64                     0x06u
#define MEAS_AVE_128                    0x07u

#define MN_I_CONV_TYPE_SHIFT            6u
#define MN_V_CONV_TYPE_BIT              (1u << 4)

#define I_CONV_OFF_PLUS_SIG             0x0u
#define I_CONV_OFFSET_ONLY              0x1u
#define I_CONV_BOTH                     0x2u
#define I_CONV_SIG_SIGNED               0x3u    /* 2's complement — 측정용 */

/* CONV_TIME : 값 순서가 단조롭지 않다. CONV_TIME < ISP 여야 한다. */
#define CONV_TIME_200MS                 0x0u
#define CONV_TIME_100MS                 0x1u
#define CONV_TIME_40MS                  0x2u
#define CONV_TIME_20MS                  0x3u
#define CONV_TIME_33MS                  0x4u
#define CONV_TIME_16MS                  0x5u
#define CONV_TIME_2_76MS                0x6u

#define MN_CONV_SIGNED(ct)              ((uint8_t)((I_CONV_SIG_SIGNED << MN_I_CONV_TYPE_SHIFT) | \
                                                   MN_V_CONV_TYPE_BIT | (ct)))

/* ---- GPIO ---- */
#define MAX30123_REG_GPIO_SETUP1        0xA8u
#define GPIO_SETUP1_LOCK_OCFG_SHIFT     6u
#define GPIO_SETUP1_INTB_OCFG_SHIFT     4u
#define GPIO_SETUP1_GPIO2_OCFG_SHIFT    2u
#define GPIO_SETUP1_GPIO1_OCFG_SHIFT    0u
/* OCFG: 00=open-drain active-low, 01=drive-high, 10=drive-low, 11=Hi-Z */

/* INTB 미연결 → Hi-Z */
#define GPIO_SETUP1_INTB_UNUSED         ((0x01u << GPIO_SETUP1_LOCK_OCFG_SHIFT) | \
                                         (0x03u << GPIO_SETUP1_INTB_OCFG_SHIFT))

/* ==========================================================================
 * AUTO MODE (시퀀서) 설정
 * ========================================================================== */
/* tSSP = 120s : AUTO_CLK_DIV = 120 × 32768 = 3,932,160 = 0x3C0000
 * Mn_SRD 가 5bit(최대 32배)이므로 impedance 60분을 얻으려면 tSSP ≥ 112.5s */

#if 1
#define CA_TSSP_SEC                     120u
#define CA_AUTO_CLK_DIV_MSB             0x3Cu
#define CA_AUTO_CLK_DIV_MID             0x00u
#define CA_AUTO_CLK_DIV_LSB             0x00u
#else
#define CA_TSSP_SEC             		40u
#define CA_AUTO_CLK_DIV_MSB     		0x14u
#define CA_AUTO_CLK_DIV_MID     		0x00u
#define CA_AUTO_CLK_DIV_LSB     		0x00u
#endif

/* M0 = PSTAT, 매 tSSP → 글루코스 2분 주기 */
#define CA_M0_CFG                       ((uint8_t)(MN_MODE_PSTAT << MN_MODE_SHIFT))
#define CA_M0_CONV                      MN_CONV_SIGNED(CONV_TIME_20MS)  /* 0xD3 */
#define CA_M0_AVG                       MEAS_AVE_8

/* M1 = Chrono A, SRD 29 → 30 × 120s = 60분
 * TODO: SRD 가 선형(SRD+1)인지 지수(2^SRD)인지 데이터시트 확인.
 *       지수라면 CA_M1_SRD = 5 (32 × 120s = 64분) */
#define CA_M1_SRD                       29u
#define CA_M1_CFG                       ((uint8_t)((MN_MODE_CHRONO_A << MN_MODE_SHIFT) | \
                                                   (CA_M1_SRD & MN_SRD_MASK)))  /* 0x5D */

#define CA_MEAS_M0                      0u
#define CA_MEAS_M1                      1u      /* 유일한 이름. CA_MEAS_ID / CA_M1_ID 폐기 */


/* ==========================================================================
 * 측정 상수 — 레지스터와 반드시 쌍을 이룬다
 * ========================================================================== */
/* 스텝 전압. CA_VSTEP_REG 가 0x35 로 기록되고 CA_VSTEP_MV 가 계산에 쓰인다.
 * ★ 두 값이 어긋나면 모든 R 이 비례해서 틀린다 (과거 2배 오차의 원인) */
#define CA_VSTEP_REG                    0x05u
#define CA_VSTEP_MV                     5.0f

/* ADC full scale. 0x3C bits[2:0] = 100 = 1024nA 와 쌍 */
#define CA_FS_NA                        1024.0f
#define CA_CHRONO_A_CFG6                0x64u

/* DC(PSTAT) full scale. 0x24 bits[2:0] = 010 = 256nA 와 쌍 */
#define MAX30123_DC_FS_NA               256.0f

/* 전 행 공통 : STEP 100 샘플 */
#define CA_STEP_SMP_REG                 99u
#define CA_STEP_N                       100u
#define CA_FIFO_N                       101u    /* PSTAT 1 + STEP 100 */
#define CA_STEP_COUNT                   104u    /* 배열 크기 (여유 4) */

#define CA_IINF_AVG_N                   10u     /* I∞ 꼬리 평균 개수 */
#define CA_FIT_FRAC_NUM                 3u      /* 회귀 구간 = n_step × 3/5 */
#define CA_FIT_FRAC_DEN                 5u
#define CA_FIT_MIN_PTS                  8u
#define CA_Y_FLOOR_NA                   1.0f    /* ln() 입력 하한 */

/* 3점 프로브 */
#define CA_PROBE_ROW                    CHRONO_SPS_15M6
#define CA_PROBE_BLK                    5u
#define CA_PROBE_DEN_NA                 5.0f    /* 분모 유효 하한 (σ의 ~5배) */


/* ==========================================================================
 * SPS 테이블
 * ========================================================================== */
typedef enum {
    CHRONO_SPS_3M9  = 0,    /* ISP   3.906 ms  τ  19.5 ~ 39   ms */
    CHRONO_SPS_7M8,         /* ISP   7.813 ms  τ  39   ~ 78   ms */
    CHRONO_SPS_15M6,        /* ISP  15.625 ms  τ  78   ~ 156  ms */
    CHRONO_SPS_31M2,        /* ISP  31.25  ms  τ 156   ~ 313  ms */
    CHRONO_SPS_62M5,        /* ISP  62.5   ms  τ 313   ~ 625  ms */
    CHRONO_SPS_125M,        /* ISP 125     ms  τ 625   ms ~ 1.25 s */
    CHRONO_SPS_250M,        /* ISP 250     ms  τ   1.25 ~ 2.5  s */
    CHRONO_SPS_COUNT
} chron_A_sps_e;

typedef struct {
    uint16_t chrono_clk_div;    /* 0x18/0x19 : ISP = div / 32768      */
    float    isp_sec;           /* ISP [s]                            */
    uint8_t  m1_conv;           /* 0x72 : signed | CONV_TIME          */
    float    conv_sec;          /* ★ m1_conv 의 CONV_TIME 과 반드시 일치 */
    uint32_t duration_ms;       /* (PRE 4 + STEP 100 + BLANK 1) × ISP */
} chrono_A_cfg_t;

extern const chrono_A_cfg_t   chrono_A_cfg[CHRONO_SPS_COUNT];
extern const chrono_A_cfg_t  *ca_active_cfg;
extern uint8_t                ca_active_idx;


/* ==========================================================================
 * 자료형
 * ========================================================================== */
typedef struct {
    uint8_t  meas_id;       /* FIFO_DATA[23:21] */
    uint8_t  data_type;     /* FIFO_DATA[20:16] */
    uint16_t adc_val;       /* FIFO_DATA[15:0]  */
} max30123_fifo_t;
extern max30123_fifo_t  max30123_fifo;

typedef struct {
    float   step_nA[CA_STEP_COUNT];
    uint8_t n_step;         /* 수집된 STEP 샘플 수 */
    uint8_t rate;           /* chron_A_sps_e — 어느 행으로 측정했는지 */
    float   isp_sec;        /* 측정 시점 스냅샷 */
    float   conv_sec;       /* 측정 시점 스냅샷 (시간축 보정용) */
} chrono_a_raw_t;

#define CA_NEXP_MAX     2u

typedef struct {
    float   i_inf_nA;
    float   i0_nA;
    uint8_t n_exp;                      /* 1 = 단일 지수, 2 = peel-off */
    float   a_nA   [CA_NEXP_MAX];       /* [0] fast, [1] slow */
    float   tau_sec[CA_NEXP_MAX];

    float   rs_ohm;
    float   r_total_ohm;
    float   r1_ohm, c1_F;               /* Rct / Cdl  (전극 계면) */
    float   r2_ohm, c2_F;               /* 막 / 확산  (n_exp==2 에서만) */
} ca_fit_t;

typedef struct {
    int32_t z_real;
    int32_t z_imag;
    int32_t z_mag;
    int32_t z_phase_deg;
} impedance_result_t;

/* Z 출력 주파수 — 행별 유효 대역 안에서 고른다 */
#define CA_Z_FREQ_COUNT     3u
extern impedance_result_t   impedance_result[CA_Z_FREQ_COUNT];

typedef struct {
    uint8_t  year, mon, day;
    uint8_t  hour, min, sec;
    uint8_t  update, dummy2;
    uint32_t epoch;
} impedance_calendar_t;
extern impedance_calendar_t impedance_calendar;

/* 레인지 상태 */
typedef enum {
    CA_RANGE_PROBE = 0,     /* 프로브 행으로 탐색 중            */
    CA_RANGE_SETTLING,      /* ISP 변경 직후 — 다음 세트 폐기   */
    CA_RANGE_LOCKED         /* 정상 측정                        */
} ca_range_state_e;

extern ca_range_state_e ca_range_state;
extern uint8_t          ca_result_valid;
extern ca_fit_t         ca_fit;



















/****************************************************************************
 * FUNCTION
 ****************************************************************************/
extern int spidrv_write_max30123_reg_action(const  uint8_t reg,  const uint8_t  val);
extern uint8_t spidrv_read_max30123_reg_action(const  uint8_t reg);

extern int spidrv_write_max30123_reg_buffer(const  uint8_t reg, uint8_t *wbuffer, uint16_t wlen);
extern int spidrv_read_max30123_reg_buffer(const  uint8_t reg, uint8_t *rbuffer, uint16_t rlen);

extern void max30123_delay_ms(int i);
extern void max30123_malloc_attach(void);
extern void max30123_reg_initial(void);

void max30123_power_on_ready(void);
void max30123_DAC_power_on(void);
void max30123_PSTAT_config_Idle_mod(const uint8_t idle_mode);
void max30123_reg_convert_mode(uint8_t mode, uint8_t converter);
uint8_t max30123_reg_convert_and_status(void);

uint16_t max30123_get_fifo_count(void);
uint8_t max30123_get_fifo_ovf(void);
int8_t max30123_read_fifo_data(max30123_fifo_t *p);
void max30123_fifo_flush(void);

uint8_t max30123_measure_calibration(uint16_t *offset_out);

uint8_t max30123_measure_DC_manual_mode(float *we_current);
uint8_t max30123_measure_DC_auto_mode(float *we_current);


uint8_t max30123_chrono_A_measurement_auto_mode(chrono_a_raw_t *out);
uint8_t max30123_chrono_A_measurement_manual_mode(chrono_a_raw_t *out);
uint8_t max30123_impedance_calcualtion(const chrono_a_raw_t *in, impedance_result_t *out);

void max30123_measure_Process(void);
void max30123_measure_ChronoAmperometry(void);
void max30123_M1_chrono_A_sps(chron_A_sps_e idx);
void max30123_M1_chrono_A_Timeout_Task(void);

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif    /* INCLUDE_APP_CEM102_H_ */
