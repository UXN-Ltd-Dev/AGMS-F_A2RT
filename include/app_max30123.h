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

/****************************************************************************
 * MAX30123 Defines
 ****************************************************************************/
#define MAX30123_PART_ID              0x51
#define MAX30123_CONVERT_MODE         0x60

#define MAX30123_STEP_CALIBRATION        0
#define MAX30123_STEP_DC_CURRENT         1
#define MAX30123_STEP_IMPEDANCE	         2

#define MAX30123_AUTO_MODE		        1
#define MAX30123_MANUAL_MODE	        0


#if 0
/****************************************************************************
 * MAX30123 Register  Address
 ****************************************************************************/
#define REG_STATUS1         0x00    /* 상태 레지스터 1         */
#define REG_INT_EN1         0x04    /* 인터럽트 활성 레지스터  */
#define REG_FIFO_CFG1       0x0C    /* FIFO_A_FULL             */
#define REG_FIFO_CFG2       0x0D    /* FIFO FLUSH              */
#define REG_FIFO_WR_PTR     0x07    /* FIFO 쓰기 포인터        */
#define REG_FIFO_RD_PTR     0x08    /* FIFO 읽기 포인터        */
#define REG_FIFO_CNT1       0x09    /* FIFO 카운터 상위        */
#define REG_FIFO_CNT2       0x0A    /* FIFO 카운터 하위        */
#define REG_FIFO_DATA       0x0B    /* FIFO 데이터             */
#define REG_SYS_CFG1        0x0E    /* 시스템 제어 1           */
#define REG_DACA_MSB        0x1A    /* DAC A MSB (WE 전압)     */
#define REG_DACB_MSB        0x1B    /* DAC B MSB (CE 전압)     */
#define REG_DACAB_LSB       0x1C    /* DAC A/B LSB             */
#define REG_DAC_CTRL        0x20    /* DAC 제어 (DAC_EN)       */
#define REG_PSTAT_CFG       0x22    /* PSTAT 설정 (WE1_AMP_EN) */
#define REG_CE1_CFG         0x25    /* CE 앰프 설정            */
#define REG_ADC_CFG1        0x31    /* ADC FS, I_CONV_TYPE     */
#define REG_ADC_CFG2        0x32    /* Offset, Conv.Time       */
#define REG_ADC_CFG3        0x33    /* Sample Average          */
#define REG_M0_CFG          0x35    /* M0 MODE                 */
#define REG_M0_AVG          0x36    /* Meas. Average           */
#define REG_AUTO_CLK_H      0x15    /* SSP 클럭 분주 상위      */
#define REG_AUTO_CLK_M      0x16    /* SSP 클럭 분주 중간      */
#define REG_AUTO_CLK_L      0x17    /* SSP 클럭 분주 하위      */
#define REG_CONVERT         0x3B    /* AUTO / CONVERT 비트     */
#define REG_PART_ID         0xFF    /* PART ID                 */
#endif
/****************************************************************************
 * MAX30123 define bit
 ****************************************************************************/
/* STATUS1 (0x00) bits */
#define STATUS1_A_FULL                  (1u << 7)
#define STATUS1_CLK_CAL_DONE            (1u << 6)
#define STATUS1_ADC_DATA_RDY            (1u << 5)
#define STATUS1_SEQ_RDY                 (1u << 3)
#define STATUS1_INVALID_CFG             (1u << 2)
#define STATUS1_NVM_CHECKSUM_ERROR      (1u << 1)
#define STATUS1_PWR_RDY                 (1u << 0)

#define STATUS1_PWR_RDY_OK               0


/* STATUS2 (0x01) bits */
#define STATUS2_BTN_DET                 (1u << 6)
#define STATUS2_V2I_DATA_HI             (1u << 5)
#define STATUS2_V2I_DATA_LO             (1u << 4)
#define STATUS2_WE1_DATA_HI             (1u << 1)
#define STATUS2_WE1_DATA_LO             (1u << 0)


#define INT_EN1_ADC_DATA_RDY    (1u << 2)   /* ADC_RDY 인터럽트 */
#define AUTO_BIT                (1u << 0)   /* Sequencer 시작   */

/****************************************************************************
 * MAX30123 setup value
 ****************************************************************************/
#define MAX30123_PART_ID        0x51    /* MAX30123 PART ID     */

/* DAC 전압 (Sensor Channels 탭 기준) */
#define DAC_A_CODE              0xC8    /* 800mV MSB (0xC8=200) */
#define DAC_B_CODE              0x96    /* 600mV MSB (0x96=150) */
/* WE 바이어스 = 800 - 600 = 200mV                             */

/* ADC 설정 */
#define ADC_FS_NA               256     /* Full Scale = 256nA   */

/* DC 변환 파라미터 */
#define MAX30123_ADC_FS_nA      256.0f  /* ADC_CFG1=0x0B, FS[3:2]=10 → 256nA */
#define MAX30123_DC_OFFSET_nA    40.0f  /* 50% offset 기준 (MAX30132 DC와 동일) */
#define MAX30123_FIFO_MAX_SAMPLES 32u   /* 한 번에 읽을 최대 FIFO 샘플 수 */
/*
 * REG_ADC_CFG1:
 *   ADC_FS[2:0]    = 010 → 256nA
 *   I_CONV_TYPE    = 11  → Signal Signed (자동 Offset 차감)
 *   → 0b 0000 1011 = 0x0B
 *
 * REG_ADC_CFG2:
 *   Conv. Time     = 16.67ms → CONV_TIME[2:0] = 001
 *   Offset         = 50% FS  → OFFSET[1:0]    = 10
 *   → 0b 0000 1010 = 0x0A  (Conv.Time=1, Offset=50%)
 *
 * REG_ADC_CFG3:
 *   Sample Average = 8       → SMP_AVE[2:0]   = 011
 *   → 0b 0000 0011 = 0x03
 */
#define ADC_CFG1_VAL            0x0B    /* FS=256nA, Signed     */
#define ADC_CFG2_VAL            0x0A    /* Conv=16.67ms, Off=50%*/
#define ADC_CFG3_VAL            0x03    /* Sample Avg = 8       */

/* Sequencer 설정 */
/*
 * SSP = 10초
 *   AUTO_CLK_DIV = 32768 × 10 = 327680 = 0x050000
 *
 * M0_MODE = PSTAT(001), Meas.Avg = 6
 *   REG_M0_CFG  = 0x01 (PSTAT)
 *   REG_M0_AVG  = 0x06 (Meas.Avg = 6)
 *
 * FIFO_A_FULL = 1 → 1개 쌓이면 INTB
 * (SSP=10초 × Meas.Avg=6 = 60초마다 FIFO 1개 저장 → INTB)
 */
#define M0_MODE_PSTAT           0x01
#define MEAS_AVG_6              0x06
#define FIFO_A_FULL_1           0x01    /* FIFO Level = 1       */


///////////////////////////////////////////////////////////////////////////////////////////

/* ---------- STATUS ---------- */
#define MAX30123_REG_STATUS1            0x00u
#define MAX30123_REG_STATUS2            0x01u

/* ---------- INTERRUPT ENABLE ---------- */
#define MAX30123_REG_INT_EN1            0x04u
#define MAX30123_REG_INT_EN2            0x05u

#define INT_EN1_A_FULL_EN               (1u << 7)
#define INT_EN1_CLK_CAL_DONE_EN         (1u << 6)
#define INT_EN1_ADC_DATA_RDY_EN         (1u << 5)
#define INT_EN1_SEQ_RDY_EN              (1u << 3)
#define INT_EN1_INVALID_CFG_EN          (1u << 2)

/* ---------- FIFO ---------- */
#define MAX30123_REG_FIFO_WR_PTR        0x07u
#define MAX30123_REG_FIFO_RD_PTR        0x08u
#define MAX30123_REG_FIFO_COUNTER1      0x09u   /* [7]=DATA_COUNT[8], [6:0]=OVF_COUNTER */
#define MAX30123_REG_FIFO_COUNTER2      0x0Au   /* FIFO_DATA_COUNT[7:0] */
#define MAX30123_REG_FIFO_DATA          0x0Bu   /* burst 3-byte per sample */
#define MAX30123_REG_FIFO_CONFIG1       0x0Cu   /* FIFO_A_FULL[7:0] */
#define MAX30123_REG_FIFO_CONFIG2       0x0Du

/* FIFO_CONFIG2 bits */
#define FIFO_CFG2_FIFO_MARK             (1u << 5)
#define FIFO_CFG2_FLUSH_FIFO            (1u << 4)
#define FIFO_CFG2_FIFO_STAT_CLR         (1u << 3)
#define FIFO_CFG2_A_FULL_TYPE           (1u << 2)
#define FIFO_CFG2_FIFO_RO               (1u << 1)

/* ---------- SYSTEM CONTROL ---------- */
#define MAX30123_REG_SYSCTRL1           0x0Eu
#define MAX30123_REG_SYSCTRL2           0x0Fu

/* SYSCTRL1 (0x0E) bits */
#define SYSCTRL1_WAKE_OUT_EN            (1u << 6)
#define SYSCTRL1_WAKE_PO_MASK           (3u << 4)
#define SYSCTRL1_NVM_CHECK              (1u << 3)
#define SYSCTRL1_OPEN_CKTS_EN           (1u << 2)
#define SYSCTRL1_SHDN                   (1u << 1)
#define SYSCTRL1_RESET                  (1u << 0)

/* SYSCTRL2 (0x0F) LOW_POWER_MODE[2:0] 값 */
#define LOW_POWER_MODE_ACTIVE           0x0u    /* 정상 동작 */
#define LOW_POWER_MODE_LP_STATE         0x1u    /* 최저 누설(30nA), CSB low로 wake */
#define LOW_POWER_MODE_LP_WAKE_LO       0x3u    /* 버튼 active-low wake */
#define LOW_POWER_MODE_LP_WAKE_HI       0x4u    /* 버튼 active-high wake */
#define SYSCTRL2_WAKE_DEGLITCH          (1u << 4)   /* 0=10ms, 1=1s */
#define SYSCTRL2_LDO_BYPASS             (1u << 3)

/* ---------- CLOCK / CAL ---------- */
#define MAX30123_REG_SR_CLK_FINE_TUNE   0x11u
#define MAX30123_REG_ADC_CLK_FINE_TUNE  0x12u
#define MAX30123_REG_CLK_V_SPI_CNT_MSB  0x13u
#define MAX30123_REG_CLK_V_SPI_CNT_LSB  0x14u

/* ---------- GPIO SETUP 1 (0xA8) ----------
 * bits [7:6] LOCK_OCFG   bits [5:4] INTB_OCFG
 * bits [3:2] GPIO2_OCFG  bits [1:0] GPIO1_OCFG
 *
 * xCFG 값:  00 = Open-drain active-low (INTB default)
 *           01 = Active-drive high
 *           10 = Active-drive low   ← 외부 풀업 불필요
 *           11 = Hi-Z
 */
#define MAX30123_REG_GPIO_SETUP1        0xA8u

#define GPIO_SETUP1_LOCK_OCFG_SHIFT     6u
#define GPIO_SETUP1_INTB_OCFG_SHIFT     4u
#define GPIO_SETUP1_GPIO2_OCFG_SHIFT    2u
#define GPIO_SETUP1_GPIO1_OCFG_SHIFT    0u

/* 편의 매크로: INTB Active-Drive-Low + LOCK default(push-pull high) */
#define GPIO_SETUP1_INTB_DRIVE_LOW      ((0x01u << GPIO_SETUP1_LOCK_OCFG_SHIFT) | \
                                         (0x02u << GPIO_SETUP1_INTB_OCFG_SHIFT))
/* = 0x60 */

/* ---------- FIFO 태그 (Table 7) ----------
 *
 * FIFO_DATA[23:0] 구조:
 *   [23:21] MEAS  — 시퀀서 슬롯 번호 (M0~M6 → 0~6)
 *   [20:16] TYPE  — 데이터 소스 식별
 *   [15:0]  DATA  — ADC_DATA[15:0]
 *
 * 태그 바이트 = FIFO_DATA[23:16] = {MEAS[2:0], TYPE[4:0]}  (상위 8비트)
 *
 * 아래 define 은 TYPE 필드(5-bit)만 정의. 실제 태그 바이트에서 MEAS 를
 * 추출하려면: meas = (tag >> 5) & 0x07, type = tag & 0x1F
 */
#define FIFO_TYPE_WE1_OFFSET_CURR       0x00u   /* WE1 오프셋 전류 */
#define FIFO_TYPE_WE1_PSTAT_CURR        0x01u   /* WE1 PSTAT 전류 */
#define FIFO_TYPE_WE1_PRE               0x02u   /* WE1 PRE  (Chrono / AP) */
#define FIFO_TYPE_WE1_STEP              0x03u   /* WE1 STEP (Chrono / AP) */
#define FIFO_TYPE_WE1_POST              0x04u   /* WE1 POST (Chrono / AP, Recovery/Neg 동일) */
/* 0x05 ~ 0x0F : reserved */
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

/* 특수 전체-24비트 태그 */
#define FIFO_RAW_MARKER                 0xFFFFFEu   /* User Marker */
#define FIFO_RAW_INVALID                0xFFFFFFu   /* Empty/Invalid */
#define FIFO_TAG_MARKER                 0xFFu       /* 상위 byte = 0xFF, 하위 16 = 0xFFFE → marker */
#define FIFO_TAG_INVALID                0xFFu       /* 상위 byte = 0xFF, 하위 16 = 0xFFFF → invalid */

/* ---------- FIFO 최대 depth ---------- */
#define MAX30123_FIFO_DEPTH             256u

/* ---------- AUTO_CLK_DIV (0x15~0x17) ---------- */
#define MAX30123_REG_AUTO_CLK_DIV_MSB   0x15u   /* AUTO_CLK_DIV[23:16] */
#define MAX30123_REG_AUTO_CLK_DIV_MID   0x16u   /* AUTO_CLK_DIV[15:8]  */
#define MAX30123_REG_AUTO_CLK_DIV_LSB   0x17u   /* AUTO_CLK_DIV[7:0]   */
/* tSSP(sec) = AUTO_CLK_DIV / 32768
 * 범위: 0x000080 ~ 0xFFFFFF → 3.906ms ~ 512s */

/* ---------- CHRONO_CLK_DIV (0x18~0x17) ---------- */
#define MAX30123_REG_CHRONO_CLK_DIV_MSB   0x18u   /* AUTO_CLK_DIV[15:8]  */
#define MAX30123_REG_CHRONO_CLK_DIV_LSB   0x19u   /* AUTO_CLK_DIV[7:0]   */

/* tSSP(sec) = AUTO_CLK_DIV / 32768
 * 범위: 0x000080 ~ 0xFFFFFF → 3.906ms ~ 512s */


/* ---------- DAC (0x1A~0x1C) ---------- */
#define MAX30123_REG_DACA_MSB           0x1Au   /* DACA_CODE[9:2] */
#define MAX30123_REG_DACB_MSB           0x1Bu   /* DACB_CODE[9:2] */


/* ---------- DAC CONTROL (0x20) ---------- */
#define MAX30123_REG_DAC_CTRL           0x20u
#define DAC_CTRL_DAC_EN                 (1u << 0)
#define DAC_CTRL_PED_SHIFT              1u
/* DAC_PEDESTAL[1:0]: 00=0mV, 01=64mV, 10=128mV, 11=256mV */

/* ---------- PSTAT CONFIG (0x22) ---------- */
#define MAX30123_REG_PSTAT_CFG          0x22u
#define PSTAT_CFG_POR_DUTY              (1u << 5)
#define PSTAT_CFG_HALF_CP               (1u << 2)
#define PSTAT_CFG_HALF_IB               (1u << 0)

/* ---------- WE1 CONFIG (0x23~0x24) ---------- */
#define MAX30123_REG_WE1_CFG1           0x23u
#define WE1_CFG1_AMP_EN                 (1u << 7)
#define WE1_CFG1_CHOP_EN                (1u << 6)
#define WE1_CFG1_DAC_MX_SHIFT           4u      /* [5:4] WE1_DAC_MX */
#define WE1_CFG1_RS                     (1u << 3)
#define WE1_CFG1_SWA                    (1u << 2)
#define WE1_CFG1_SWB                    (1u << 1)
#define WE1_CFG1_SRA                    (1u << 0)

#define MAX30123_REG_WE1_CFG2           0x24u
/* [7] WE1_IOS_MODE, [6:5] WE1_OFFSET_SEL, [3] STORE_OFFSET, [2:0] ADC_FS_WE1 */
#define WE1_CFG2_STORE_OFFSET           (1u << 3)
#define WE1_CFG2_OFFSET_SEL_SHIFT       4u
#define WE1_CFG2_FS_SHIFT               0u
/* WE1_OFFSET_SEL: 00=0%, 01=10%, 10=20%, 11=50% of FS */
/* ADC_FS_WE1: 000=64nA, 001=128nA, 010=256nA, 011=512nA, 100=1024nA ... */

/* ---------- CE1 CONFIG (0x29) ---------- */
#define MAX30123_REG_CE1_CFG            0x29u
#define CE1_CFG_AMP_EN                  (1u << 7)
#define CE1_CFG_DAC_MX_SHIFT            4u      /* [5:4] CE1_DAC_MX */
#define CE1_CFG_SRB                    (1u << 0)

#define MAX30123_REG_CHRONO_B_CFG1       0x3Du
#define MAX30123_REG_CHRONO_B_CFG2       0x3Eu
#define MAX30123_REG_CHRONO_B_CFG3       0x3Fu
#define MAX30123_REG_CHRONO_B_CFG4       0x40u
#define MAX30123_REG_CHRONO_B_CFG5       0x41u
#define MAX30123_REG_CHRONO_B_CFG6       0x42u


/* ---------- CHRONO AMPLITUDE (0x35) ---------- */
#define MAX30123_REG_CHRONO_AMPLITUDE 	0x35u

#define MAX30123_REG_CHRONO_A_CFG1 		0x37u
#define MAX30123_REG_CHRONO_A_CFG2 		0x38u
#define MAX30123_REG_CHRONO_A_CFG3 		0x39u
#define MAX30123_REG_CHRONO_A_CFG4 		0x3Au
#define MAX30123_REG_CHRONO_A_CFG5 		0x3Bu
#define MAX30123_REG_CHRONO_A_CFG6 		0x3Cu

/* ---------- WE1_I_OFFSET (0x80~0x81, read-only) ---------- */
#define MAX30123_REG_WE1_I_OFFSET_MSB   0x80u
#define MAX30123_REG_WE1_I_OFFSET_LSB   0x81u

/* ---------- CONVERT_MODE (0x60) ---------- */
#define MAX30123_REG_CONVERT_MODE       0x60u
#define CONVERT_MODE_SEQ_RESTART        (1u << 7)
#define CONVERT_MODE_AUTO               (1u << 1)
#define CONVERT_MODE_CONVERT            (1u << 0)
#define CONVERT_MODE_STOP            	(0u << 0)
#define CONVERT_MODE_MANUAL             (0u << 1)

/* ---------- Mn CONFIG (0x62~0x6D) ----------
 * M0: 0x62 config, 0x63 delay
 * M1: 0x64 config, 0x65 delay
 * M2: 0x66 config, 0x67 delay
 * M3: 0x68 config, 0x69 delay
 * M4: 0x6A config, 0x6B delay
 */
#define MAX30123_REG_M0_CFG             0x62u
#define MAX30123_REG_M0_DLY             0x63u
#define MAX30123_REG_M1_CFG             0x64u
#define MAX30123_REG_M1_DLY             0x65u
#define MAX30123_REG_M2_CFG             0x66u
#define MAX30123_REG_M3_CFG             0x68u
#define MAX30123_REG_M4_CFG             0x6Au


/* Mn_MODE[2:0] bits [7:5] */
#define MN_MODE_SHIFT                   5u
#define MN_MODE_DISABLED                0x0u
#define MN_MODE_PSTAT                   0x1u
#define MN_MODE_CHRONO_A                0x2u
#define MN_MODE_CHRONO_B                0x3u
#define MN_MODE_AP                      0x4u
#define MN_MODE_TEMPERATURE             0x6u
#define MN_MODE_SYS_VOLTAGE             0x7u

/* Mn_SRD[4:0] bits [4:0] — Skip Rate Divider (M1~M4) */

/* ---------- Mn I/V CONV CONFIG (0x70~0x7C) ----------
 * M0: 0x70,  M1: 0x72,  M2: 0x74,  M3: 0x76,
 * M4: 0x78,  M5: 0x7A,  M6: 0x7C
 *
 * [7:6] Mn_I_CONV_TYPE   [4] Mn_V_CONV_TYPE   [2:0] Mn_CONV_TIME
 */
#define MAX30123_REG_M0_CONV            0x70u
#define MAX30123_REG_M1_CONV            0x72u
#define MAX30123_REG_M2_CONV            0x74u
#define MAX30123_REG_M3_CONV            0x76u
#define MAX30123_REG_M4_CONV            0x78u
#define MAX30123_REG_M5_CONV            0x7Au
#define MAX30123_REG_M6_CONV            0x7Cu

#define MN_I_CONV_TYPE_SHIFT            6u
#define MN_V_CONV_TYPE_BIT              (1u << 4)

/* I_CONV_TYPE 값 */
#define I_CONV_OFF_PLUS_SIG             0x0u    /* 00: ISIG+IOFF (unsigned) */
#define I_CONV_OFFSET_ONLY              0x1u    /* 01: IOFF only (unsigned) */
#define I_CONV_BOTH                     0x2u    /* 10: IOFF + (ISIG+IOFF) (unsigned×2) */
#define I_CONV_SIG_SIGNED               0x3u    /* 11: ISIG signed (2's complement) */

/* CONV_TIME 값 */
#define CONV_TIME_200MS                 0x0u
#define CONV_TIME_100MS                 0x1u
#define CONV_TIME_40MS                  0x2u
#define CONV_TIME_20MS                  0x3u
#define CONV_TIME_33MS                  0x4u
#define CONV_TIME_16MS                  0x5u

/* ---------- SAMPLE COUNT (0x18~0x19) ---------- */
#define MAX30123_REG_SAMPLE_COUNT_MSB   0x18u
#define MAX30123_REG_SAMPLE_COUNT_LSB   0x19u
/* 0 = 무한 연속, >0 = N 시퀀스 후 정지 */

/* ---------- Mn MEAS/SMP AVE ----------
 * M0: 0x90 = [7:5] M0_MEAS_AVE, [4:0] M0_SMP_AVE
 */
#define MAX30123_REG_M0_AVE             0x90u

#define M0_PSTAT_ENABLE					1
#define M0_PSTAT_DISABLE					0


#define M1_CHRONO_A_ENABLE					1
#define M1_CHRONO_A_DISABLE					0

/****************************************************************************
 * 측정 주파수 인덱스
 ****************************************************************************/
typedef enum {
    CHRONO_FREQ_1HZ=0,
	CHRONO_FREQ_10HZ,
	CHRONO_FREQ_25HZ,
	CHRONO_FREQ_COUNT
} Chron_Freq_e;


/****************************************************************************
 * VALUABLE
 ****************************************************************************/
typedef struct {			// 데이터 해석을 위한 구조체
    uint8_t  meas_id;   	// M0~M6 (비트 23:21) [cite: 1678, 1679]
    uint8_t  data_type; 	// 데이터 종류 (비트 20:16) [cite: 1678, 1680]
    uint16_t adc_val;   	// 16비트 ADC 결과 [cite: 1687]
} max30123_fifo_t;
extern max30123_fifo_t	max30123_fifo;

/****************************************************************************
 *  설정값 (max30123_chrono_A_setup() 과 일치)
 ****************************************************************************/
#define CA_MEAS_ID          3u       /* EVK 확인: Chrono = Meas 3 */
#define CA_TAG_STEP         0x03u
#define CA_TAG_POST         0x04u

#define CA_ISP_SEC          0.010f   /* 10ms */
#define CA_FREQ_HZ          10.0f
#define CA_VSTEP_MV         10.0f
#define CA_VPOST_MV         0.0f    /* POST: 전압 0mV (baseline 복귀) */

#define CA_REPEAT       4u
#define CA_STEP_COUNT   7u                          /* 1cycle STEP 수 */
#define CA_POST_COUNT   3u                          /* 1cycle POST 수 */
#define CA_TOTAL_STEP   (CA_STEP_COUNT * CA_REPEAT) /* 배열 크기 = 32 */
#define CA_TOTAL_POST   (CA_POST_COUNT * CA_REPEAT) /* 배열 크기 = 12 */
#define CA_N_PER_CYCLE  (CA_STEP_COUNT + CA_POST_COUNT) /* DFT 주기 = 11 */

typedef struct {
    float   step_nA[CA_TOTAL_STEP]; /* 32 */
    float   post_nA[CA_TOTAL_POST]; /* 12 */
    uint8_t n_step;
    uint8_t n_post;
} chrono_a_raw_t;

typedef struct {
    int32_t z_real;       	/* 저항 성분 [Ω] */
    int32_t z_imag;       	/* 리액턴스 성분 [Ω] */
    int32_t z_mag;        	/* |Z| [Ω] */
    int32_t z_phase_deg;  	/* 위상각 [degree] */
} impedance_result_t;
extern impedance_result_t	impedance_result[CHRONO_FREQ_COUNT];
extern impedance_result_t	z_result[CHRONO_FREQ_COUNT];

typedef struct {
	uint8_t year;
	uint8_t mon;
	uint8_t day;

	uint8_t hour;
	uint8_t min;
	uint8_t sec;

	uint8_t update;
	uint8_t dummy2;

	uint32_t epoch;
} impedance_calendar_t;
extern impedance_calendar_t	impedance_calendar;


#define CHRONO_A_START_TIME		60				// 1min * 60 = 60min is 1hour
extern uint16_t max30123_chrono_A_wait_tmout;


/****************************************************************************
 * 1Hz, 10Hz, 25Hz...Impedance measurement
 ****************************************************************************/
#define EIS_CHRONO_CLK_HZ   32768.0f   /* ISP = CHRONO_CLK_DIV / 32768 */

// 주파수별 설정 (샘플 구조 7+3x4 는 세 주파수 공통)
typedef struct {
    float       freq_hz;        // 실제 여기 주파수 = 1/(10*isp_sec) [Hz]
    float       isp_sec;        // 실제 ISP [s] = chrono_clk_div/32768
    uint32_t    chrono_clk_div; // CHRONO_CLK_DIV 레지스터 값 (16bit)
} chrono_freq_cfg_t;

// 주파수 테이블 (조회용)
extern const chrono_freq_cfg_t   chrono_table[CHRONO_FREQ_COUNT];

/****************************************************************************
 * FUNCTION
 ****************************************************************************/

extern int spidrv_write_max30123_reg_action(const  uint8_t reg,  const uint8_t  val);
extern uint8_t spidrv_read_max30123_reg_action(const  uint8_t reg);

extern void max30123_delay_ms(int i);

extern void max30123_malloc_attach(void);
extern void max30123_reg_initial(void);

extern void max30123_power_on_ready(void);
extern void max30123_DAC_power_on(void);

uint8_t max30123_reg_convert_and_status(void);

uint16_t max30123_get_fifo_count(void);
int8_t max30123_read_fifo_data(max30123_fifo_t *p);

uint8_t max30123_measure_calibration(uint16_t *offset_out);
uint8_t max30123_measure_DC_oneshot(void);
uint8_t max30123_measure_DC_auto_mode(void);

void max30123_M1_chrono_A_mode(int8_t state);
void max30123_WE1_config_power_on(void);

uint8_t max30123_chrono_A_measurement(chrono_a_raw_t *out);
uint8_t max30123_impedance_calcualtion(const chrono_a_raw_t *in, impedance_result_t *out);

void max30123_measure_ChronoAmperometry(void);

void max30123_measure_Process(void);
void max30123_measure_ChronoAmperometry(void);


void max30123_M1_chrono_A_Freq(Chron_Freq_e idx);



/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif    /* INCLUDE_APP_CEM102_H_ */
