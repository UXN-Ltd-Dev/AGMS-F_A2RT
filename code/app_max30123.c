/**
 * @file  app_max30123.c
 * @brief CEM102 low power application initialization and operation file
 *        source file
 *
 * @copyright @parblock
 * Copyright (c) 2024 Semiconductor Components Industries, LLC (d/b/a
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

#include <hw.h>
#include <app.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "app.h"

#include "app_cem102.h"
#include "app_utils.h"
#include "app_customss.h"

#if APP_MEASURE_ACROSS_IO_INPUTS
#include "math.h"
#endif /* APP_MEASURE_ACROSS_IO_INPUTS */

#include "app_max30123.h"
#include <max30123_reg_table.h>

#define CHRONO_A_MEASURE_TIME			(1440u)				//


uint32_t max30123_chrono_A_epoch = 0;

#if 1

/* ================================================================================
 *  DC (수동 단발)
 * ================================================================================ */
#define DC_CONV_WAIT_MS             50u     /* 변환 중 SPI 접근 금지 구간 (CONV 40 ms + 여유) */
#define DC_CONV_TMOUT_MS            3000u

/* ================================================================================
 *  보정
 * ================================================================================ */
#define CAL_OFFSET_EXPECT           32768u  /* OFFSET_SEL = 50% FS 기대 카운트 */
#define CAL_OFFSET_TOL              6554u   /* ±20 % */
#define CAL_SETTLE_MS               50u
#define CAL_CONV_WAIT_MS            350u    /* MEAS_AVE_8 × 40 ms 여유 */
#define CAL_CONV_TMOUT_MS           3000u

/* ================================================================================
 *  Chronoamperometry
 * ================================================================================ */
#define CA_AMP_MV                   10u                     /* Amplitude [mV] */
#define CA_DV_MV                    ((float)CA_AMP_MV)      /* 피팅에 쓰는 ΔV — 반드시 위와 같은 값 */
#define CA_REG_AMPLITUDE            0x35u                   /* reg table {0x35, 0x0A} = 10 mV */

#define CA_PRE_N                    4u                      /* CA_CFG1_VALUE 의 PRE */
#define CA_STEP_TARGET              ((uint16_t)CA_STEP_N)   /* 100 */

#define CA_SEQ_PERIOD_SEC           12u
#define CA_SEQ_CLK_DIV              ((uint32_t)CA_SEQ_PERIOD_SEC * 32768ul)   /* 0x060000 */

#define CA_M1_CFG_ON                ((uint8_t)(MN_MODE_CHRONO_A << MN_MODE_SHIFT))  /* SRD = 0 */
#define CA_M1_CFG_OFF               0x00u

#define CA_POLL_SEC                 2u      /* 외부 타이머 폴링 주기 */
#define CA_WAIT_MARGIN_SEC          5u
#define CA_RELAX_SEC                15u     /* 재측정 전 전극 이완 */
#define CA_RETRY_MAX                1u
#define CA_DC_DISCARD_N             5u      /* Chrono 후 버릴 DC 샘플 수 */
#define CA_SEQ_STOP_TMOUT_MS        50u     /* STOP 후 SEQ_RDY 대기 (실측으로 조정) */

/* (PRE + STEP + 1) × ISP, ms 올림 — Blank 0 이 실제로 0 샘플인지 미확인이라 1 ISP 여유 */
#define CA_DUR_MS(div)  ((uint32_t)((((uint32_t)(CA_PRE_N + CA_STEP_N + 1u)) * (uint32_t)(div) * 1000ul \
                                    + 32767ul) / 32768ul))

/* 피팅 파라미터 */
#define CAF_TAIL_N                  10u     /* I∞ 꼬리 평균 개수 */
#define CAF_FIT_NUM                 3u      /* 앞 60 % 구간으로 τ 회귀 */
#define CAF_FIT_DEN                 5u
#define CAF_FIT_MIN_PTS             5u
#define CAF_Y_FLOOR_NA              0.2f    /* I − I∞ 가 이 이하이면 회귀에서 제외 */
#define CAF_ITER                    3u      /* I∞ ↔ τ 교대 추정 횟수 */
#define CAF_IINF_MIN_NA             0.5f    /* I∞ 가 이 이하이면 Rct 측정 불가 */
#define CAF_PROBE_N                 5u      /* 3점 프로브 블록 크기 */
#define CAF_PROBE_DEN_NA            0.5f
#define CAF_TAU_WIN_DIV             3.0f    /* 유효 τ 상한 = Step 구간 / 3 */
#define CAF_TAU_HUGE                1000.0f

typedef enum {
    CA_ROW_10MS  = 0u,      /* 기본 */
    CA_ROW_100MS = 1u,      /* τ > 0.33 s */
    CA_ROW_COUNT = 2u
} ca_row_e;

typedef enum {
    CA_OUT_FAIL = 0u,
    CA_OUT_OK   = 1u,
    CA_OUT_SLOW = 2u        /* 100.01 ms 로 재측정 필요 */
} ca_outcome_e;

static const chrono_A_cfg_t ca_rows[CA_ROW_COUNT] = {
    [CA_ROW_10MS] = {
        .chrono_clk_div = 0x0148u,                          /* 10.0098 ms */
        .isp_sec        = 328.0f / 32768.0f,
        .m1_conv        = MN_CONV_SIGNED(CONV_TIME_2_76MS),
        .conv_sec       = 0.00276f,
        .duration_ms    = CA_DUR_MS(0x0148u),               /* 1052 ms */
    },
    [CA_ROW_100MS] = {
        .chrono_clk_div = 0x0CCDu,                          /* 100.006 ms */
        .isp_sec        = 3277.0f / 32768.0f,
        .m1_conv        = MN_CONV_SIGNED(CONV_TIME_40MS),
        .conv_sec       = 0.040f,
        .duration_ms    = CA_DUR_MS(0x0CCDu),               /* 10501 ms */
    },
};

/* Period 별 꼬리 오프셋 보정 [nA].
 * Rct 없는 더미(20 kΩ + 4.7 µF)의 꼬리값을 Period 별로 측정해서 넣는다. 측정 전에는 0. */
static const float ca_iinf_ofs_nA[CA_ROW_COUNT] = { 0.0f, 0.0f };

/* ================================================================================
 *  전역
 * ================================================================================ */
max30123_fifo_t        max30123_fifo;
impedance_result_t     impedance_result[CA_Z_FREQ_COUNT];
impedance_calendar_t   impedance_calendar;
ca_fit_t               ca_fit;
float                  ca_z_freq_hz[CA_Z_FREQ_COUNT];   /* impedance_result[] 각 항목의 주파수 */
uint8_t                ca_result_valid = 0u;
uint8_t                ca_row          = CA_ROW_10MS;

const chrono_A_cfg_t  *ca_active_cfg   = NULL;
uint8_t                ca_active_idx   = 0xFFu;

uint8_t  max30123_measure_step           = MAX30123_STEP_CALIBRATION;
uint8_t  max30123_chrono_A_step          = 0u;
uint8_t  max30123_chrono_A_wait_count    = 0u;
uint16_t max30123_chrono_A_measure_tmout = 0u;

static chrono_a_raw_t  ca_acc;
static uint16_t        ca_wait_s;
static uint16_t        ca_deadline_s;
static uint8_t         ca_retry;
static uint8_t         ca_outcome;

/* Chrono 동안 바꾸는 DC 설정 보관 */
static uint8_t         ca_saved_valid;
static uint8_t         ca_saved_clk_msb;
static uint8_t         ca_saved_clk_lsb;
static uint8_t         ca_saved_m0_avg;



#else

#endif

/************************************************************************************************************************
 *   SPI CONTROL
 ************************************************************************************************************************/
int spidrv_write_max30123_reg_action(const  uint8_t reg,  const uint8_t  val)
{
    int result;
    uint8_t buffer[4];

    buffer[0] = val;
    result=cem102->max30123_write_reg_action(cem102_dut,   reg, buffer, 1);

    return result;
}

/******************************************************************************************
 *	reg 값을 1개씩 읽어 온다
 ******************************************************************************************/
uint8_t spidrv_read_max30123_reg_action(const  uint8_t reg)
{
    int result;
    uint8_t buffer[4];

    result=cem102->max30123_read_reg_action(cem102_dut,  reg, buffer, 1);

    if (result == ERRNO_NO_ERROR) {
        return buffer[0];
    }

    return 0;
}

/******************************************************************************************
 *	DATA BUFFER 만큼 wlen만큼 전송 한다
 ******************************************************************************************/
int spidrv_write_max30123_reg_buffer(const  uint8_t reg, uint8_t *wbuffer, uint16_t wlen)
{
    int result;

    result=cem102->max30123_write_reg_action(cem102_dut, reg, wbuffer, wlen);
    return result;
}

/******************************************************************************************
 *	DATA BUFFER 만큼 rlen만큼 읽어 온다.
 ******************************************************************************************/
int spidrv_read_max30123_reg_buffer(const  uint8_t reg, uint8_t *rbuffer, uint16_t rlen)
{
    int result;

    result = cem102->max30123_read_reg_action(cem102_dut,  reg, rbuffer, rlen);

    return result;
}


void max30123_delay_ms(int i)
{
    while (i-- > 0) {
        Sys_Delay(SystemCoreClock / 1000);
    }
}

/************************************************************************************************************************
 *   mac30123 value memory attach
 ************************************************************************************************************************/
void max30123_malloc_attach(void)
{
	memset(&impedance_result, 0, sizeof(impedance_result_t));
	memset(&impedance_calendar, 0, sizeof(impedance_calendar_t));

    memset(&ca_fit, 0, sizeof(ca_fit));
    memset(ca_z_freq_hz, 0, sizeof(ca_z_freq_hz));

}

/*************************************************************************************************************************
 * 	MAX30132 REGISTER INITIAL
 *************************************************************************************************************************/
void max30123_reg_initial(void)
{
	uint8_t i;
	uint8_t  result;

	max30123_malloc_attach();

	swmLogInfo("\r\nSERIAL ID : ");
	for(i=0;i<5;i++) {
		result = spidrv_read_max30123_reg_action(0xAA+i);
		swmLogInfo("[0x%02X] ", result);
	}
	swmLogInfo("\r\n");

	result = spidrv_read_max30123_reg_action(0xFE);
#ifdef _MAX30123_INFO_DEBUG_
	swmLogInfo(" FE : [0x%02X]\r\n", result);
#endif

	result = spidrv_read_max30123_reg_action(PART_IDENTIFIER);
#ifdef _MAX30123_INFO_DEBUG_
	swmLogInfo(" FF : [0x%02X]\r\n", result);
#endif

	if(result == MAX30123_PART_ID) {
		for(i=0;i<MAX30123_TBL_SIZE;i++) {
			spidrv_write_max30123_reg_action(max30123_reg_tbl[i].reg, max30123_reg_tbl[i].val);
		}
	}
}


/***********************************************************************************************************************
 *		MAX30132  INITIAL & POWER ON STATUS WAIT...1초 동안 기다린다...
 *		POWER ON 이 된 뒤 전원 안정화를 위해 2초 뒤 주파수 초기화를 한다...
 ***********************************************************************************************************************/
void max30123_power_on_ready(void)
{
    uint8_t tmout = 200u;
	uint8_t status;

#ifdef _MAX30123_INFO_DEBUG_
	swmLogInfo("Software Reset\r\n");
#endif
	spidrv_write_max30123_reg_action(MAX30123_REG_SYSCTRL1, SYSCTRL1_RESET);

	do {
		status = spidrv_read_max30123_reg_action(MAX30123_REG_STATUS1);

#ifdef _MAX30123_INFO_DEBUG_
		swmLogInfo("STATUS_1 : %02X\r\n", status);
#endif

		if((status&STATUS1_PWR_RDY)==STATUS1_PWR_RDY_OK) {
			break;
		}

		Sys_Delay(SystemCoreClock / 100);				// 10ms wait
	} while(tmout-->0);
}

/********************************************************************************
 *	600mV, 600mV --> 800mV, 600mV ==> 200mV
 ********************************************************************************/
void max30123_DAC_power_on(void)
{
	spidrv_write_max30123_reg_action(MAX30123_REG_DACA_MSB, 0xC8);
	spidrv_write_max30123_reg_action(MAX30123_REG_DACB_MSB, 0x96);
}

/********************************************************************************
 *
 ********************************************************************************/
void max30123_PSTAT_config_Idle_mod(const uint8_t idle_mode)
{
    uint8_t reg_val = spidrv_read_max30123_reg_action(MAX30123_REG_WE1_CFG1);

    if (idle_mode == ENABLE) {
        spidrv_write_max30123_reg_action(MAX30123_REG_PSTAT_CFG, 0x0F);   /* POR_DUTY|HALF_CP_CLK|AFE_CHOP_EN|HALF_IB */
        reg_val |= WE1_CFG1_CHOP_EN;
    } else {
        spidrv_write_max30123_reg_action(MAX30123_REG_PSTAT_CFG, 0x0A);   /* POR_DUTY|AFE_CHOP_EN */
        reg_val &= (uint8_t)~WE1_CFG1_CHOP_EN;
    }
    spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG1, reg_val);
}


void max30123_WE1_config_power_on(void)
{
    spidrv_write_max30123_reg_action(MAX30123_REG_FIFO_WR_PTR, 0x00);
    spidrv_write_max30123_reg_action(MAX30123_REG_FIFO_RD_PTR, 0x00);

    spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG1, 0x82);    /* Working Amp Enable */
    spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG2, 0xEA);    /* Offset Always On */
    spidrv_write_max30123_reg_action(MAX30123_REG_CE1_CFG,  0x91);    /* Counter Amp Enable */
}

/********************************************************************************
 *	CONVERTER MODE : AUTO MODE & MANUAL MODE
 ********************************************************************************/
void max30123_reg_convert_mode(uint8_t mode, uint8_t converter)
{
    uint8_t reg_val = spidrv_read_max30123_reg_action(MAX30123_REG_CONVERT_MODE);

    reg_val = (uint8_t)((reg_val & 0xFCu) | mode | converter);
    spidrv_write_max30123_reg_action(MAX30123_REG_CONVERT_MODE, reg_val);
}


/********************************************************************************
 * @brief 현재 FIFO에 저장된 유효 샘플 수를 확인합니다.
 * @return 읽기 가능한 샘플 수 (0~256)
 ********************************************************************************/
static uint16_t max30123_fifo_count_ovf(uint8_t *ovf)
{
    uint8_t buf[2] = { 0u, 0u };

    spidrv_read_max30123_reg_buffer(MAX30123_REG_FIFO_COUNTER1, buf, 2u);   /* 버스트 1회 */
    if (ovf != NULL) {
        *ovf = (uint8_t)(buf[0] & 0x7Fu);                                    /* OVF_COUNTER[6:0] */
    }
    return (uint16_t)(((uint16_t)(buf[0] & 0x80u) << 1) | buf[1]);          /* DATA_COUNT[8:0] */
}


uint16_t max30123_get_fifo_count(void)
{
    return max30123_fifo_count_ovf(NULL);
}

uint8_t max30123_get_fifo_ovf(void)
{
    uint8_t ovf = 0u;

    (void)max30123_fifo_count_ovf(&ovf);
    return ovf;
}
/********************************************************************************
 * @brief FIFO에서 샘플 1개를 읽어옵니다.
 * @param sample 읽어온 데이터를 저장할 구조체 포인터
 * @return 성공 시 true, FIFO가 비어있으면 false
 ********************************************************************************/
int8_t max30123_read_fifo_data(max30123_fifo_t *p)
{
	uint8_t buf[3];

    cem102->max30123_read_reg_action(cem102_dut, MAX30123_REG_FIFO_DATA, buf, 3);

    if (buf[0] == 0xFF && buf[1] == 0xFF && buf[2] == 0xFF) {
        return 0;
    }

    p->meas_id   = (buf[0] >> 5) & 0x07;
    p->data_type = buf[0] & 0x1F;
    p->adc_val   = (uint16_t)((buf[1] << 8) | buf[2]);

#if _MAX30123_INFO_DEBUG_
	swmLogInfo("fifo_data : [%02x][%02x][%02x] \r\n", p->meas_id, p->data_type, p->adc_val);
    max30123_delay_ms(1u);
#endif

    return 1;
}

void max30123_fifo_flush(void)
{
    /* FIFO_RO 는 넣지 않는다 — 켜지면 오버플로 시 오래된 데이터를 덮어써서
     * 버스트 앞부분이 조용히 사라진다. OFF 면 뒤가 잘리고 OVF 로 검출된다. */
    spidrv_write_max30123_reg_action(MAX30123_REG_FIFO_CONFIG2,
                                     FIFO_CFG2_FLUSH_FIFO
                                   | FIFO_CFG2_FIFO_STAT_CLR
                                   | FIFO_CFG2_A_FULL_TYPE);
}


/********************************************************************************
 *  코드 → 전류
 ********************************************************************************/
static inline float ca_code_to_nA(uint16_t raw)
{
    return CA_FS_NA * ((float)(int16_t)raw / 32768.0f);
}

static inline float dc_code_to_nA(uint16_t raw)
{
    return MAX30123_DC_FS_NA * ((float)(int16_t)raw / 32768.0f);
}

static inline int32_t ca_round(float v)
{
    return (int32_t)((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f));
}



/* ================================================================================
 *  수동 단발 변환
 *    wait_ms 동안은 SPI 를 건드리지 않는다 (변환 중 SPI 노이즈 방지)
 *    INVALID_CFG 면 즉시 종료
 * ================================================================================ */
static uint8_t max30123_single_convert(uint16_t wait_ms, uint16_t tmout_ms, uint8_t *p_status)
{
    uint8_t  st = 0u;
    uint16_t t;

    spidrv_write_max30123_reg_action(MAX30123_REG_CONVERT_MODE, CONVERT_MODE_STOP);
    max30123_delay_ms(1);                                   /* CONVERT 0→1 은 4 SR_CLK 이후 */
    spidrv_write_max30123_reg_action(MAX30123_REG_CONVERT_MODE, CONVERT_MODE_CONVERT);

    max30123_delay_ms((int)wait_ms);

    for (t = wait_ms; t <= tmout_ms; t = (uint16_t)(t + 10u)) {
        st = spidrv_read_max30123_reg_action(MAX30123_REG_STATUS1);   /* 읽으면 클리어 */
        if ((st & STATUS1_INVALID_CFG) != 0u) {
            break;
        }
        if ((st & STATUS1_ADC_DATA_RDY) != 0u) {
            if (p_status != NULL) { *p_status = st; }
            return 1u;
        }
        max30123_delay_ms(10);
    }

    if (p_status != NULL) { *p_status = st; }
    return 0u;
}

/* 기존 호출부 호환 */
uint8_t max30123_reg_convert_and_status(void)
{
    return max30123_single_convert(0u, DC_CONV_TMOUT_MS, NULL);
}

/***********************************************************************************************************************
 *	#1. MAX30123 measure calibration
 ***********************************************************************************************************************/
uint8_t max30123_measure_calibration(uint16_t *offset_out)
{
    uint8_t  m0_cfg, saved_conv, saved_avg, saved_cfg2;
    uint8_t  off_buf[2];
    uint8_t  st = 0u;
    uint8_t  ok = 0u;
    uint16_t stored, cnt;

    if (offset_out == NULL) { return 0u; }
    *offset_out = 0u;

    spidrv_write_max30123_reg_action(MAX30123_REG_PSTAT_CFG, 0x0F);

    /* M0 가 PSTAT 가 아니면 offset-only 측정이 일어나지 않는다 */
    m0_cfg = spidrv_read_max30123_reg_action(MAX30123_REG_M0_CFG);
    if (((m0_cfg >> 5) & 0x07u) != MN_MODE_PSTAT) {
        swmLogInfo("[CAL] M0 is not PSTAT : 0x%02X\r\n", m0_cfg);
        return 0u;
    }

    /* STORE_OFFSET = 1 */
    saved_cfg2 = spidrv_read_max30123_reg_action(MAX30123_REG_WE1_CFG2);
    spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG2, (uint8_t)(saved_cfg2 | WE1_CFG2_STORE_OFFSET));

    /* I_CONV_TYPE = 01 (Offset only), CONV_TIME 유지 */
    saved_conv = spidrv_read_max30123_reg_action(MAX30123_REG_M0_CONV);
    spidrv_write_max30123_reg_action(MAX30123_REG_M0_CONV,
                                     (uint8_t)((saved_conv & 0x3Fu)
                                             | (I_CONV_OFFSET_ONLY << MN_I_CONV_TYPE_SHIFT)));

    saved_avg = spidrv_read_max30123_reg_action(MAX30123_REG_M0_AVG);
    spidrv_write_max30123_reg_action(MAX30123_REG_M0_AVG, MEAS_AVE_8);

    max30123_delay_ms(CAL_SETTLE_MS);
    max30123_fifo_flush();

    if (max30123_single_convert(CAL_CONV_WAIT_MS, CAL_CONV_TMOUT_MS, &st) == 0u) {
        swmLogInfo("[CAL] convert fail, STATUS1=0x%02X\r\n", st);
        goto cleanup;
    }

    /* 보정 샘플은 버린다 */
    cnt = max30123_get_fifo_count();
    while (cnt > 0u) {
        (void)max30123_read_fifo_data(&max30123_fifo);
        cnt--;
    }

    spidrv_read_max30123_reg_buffer(MAX30123_REG_WE1_I_OFFSET_MSB, off_buf, 2u);
    stored      = (uint16_t)(((uint16_t)off_buf[0] << 8) | off_buf[1]);
    *offset_out = stored;

    if ((stored > (CAL_OFFSET_EXPECT + CAL_OFFSET_TOL)) ||
        (stored < (CAL_OFFSET_EXPECT - CAL_OFFSET_TOL))) {
        swmLogInfo("[CAL] offset out of range : %u\r\n", stored);
        goto cleanup;
    }

#if _EIS_FIFO_INFO_DEBUG_
    swmLogInfo("[CAL] WE1_I_OFFSET = %u\r\n", stored);
#endif
    ok = 1u;

cleanup:
    spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG2, (uint8_t)(saved_cfg2 & (uint8_t)~WE1_CFG2_STORE_OFFSET));
    spidrv_write_max30123_reg_action(MAX30123_REG_M0_AVG, saved_avg);
    spidrv_write_max30123_reg_action(MAX30123_REG_M0_CONV,
                                     (uint8_t)((saved_conv & 0x3Fu)
                                             | (I_CONV_SIG_SIGNED << MN_I_CONV_TYPE_SHIFT)));
    return ok;      /* 기존 코드는 실패해도 1 을 반환했음 */
}

/***********************************************************************************************************************
 *	#2-1. MAX30123 measure DC current... 수동 모두
 ***********************************************************************************************************************/
uint8_t max30123_measure_DC_manual_mode(float *we_current)
{
    uint8_t  st = 0u;
    uint8_t  found = 0u;
    uint16_t cnt, i;

    if (we_current == NULL) { return 0u; }

    max30123_fifo_flush();

    if (max30123_single_convert(DC_CONV_WAIT_MS, DC_CONV_TMOUT_MS, &st) == 0u) {
        swmLogInfo("[DC] convert fail, STATUS1=0x%02X\r\n", st);
        return 0u;
    }

    cnt = max30123_get_fifo_count();
    if (cnt > MAX30123_FIFO_DEPTH) { cnt = MAX30123_FIFO_DEPTH; }

    for (i = 0u; i < cnt; i++) {
        if (max30123_read_fifo_data(&max30123_fifo) == 0) { break; }

        if ((max30123_fifo.meas_id   == CA_MEAS_M0) &&
            (max30123_fifo.data_type == FIFO_TYPE_WE1_PSTAT_CURR)) {
            *we_current = dc_code_to_nA(max30123_fifo.adc_val);    /* 여러 개면 마지막 값 */
            found = 1u;
#if _EIS_FIFO_INFO_DEBUG_
            swmLogInfo("[DC] raw=%d " LOG_FLOAT_MARKER " nA\r\n",
                       (int16_t)max30123_fifo.adc_val, LOG_FLOAT(*we_current));
#endif
        }
    }

    return found;
}



/***********************************************************************************************************************
 *	#2-2. MAX30123 measure DC current... 수동 모두
 ***********************************************************************************************************************/
uint8_t max30123_measure_DC_auto_mode(float *we_current)
{
	uint8_t status = 0;
    int16_t signed_raw;
    uint16_t fifo_count;
    float nA;


	status = spidrv_read_max30123_reg_action(MAX30123_REG_STATUS1);
#if _EIS_FIFO_INFO_DEBUG_
	swmLogInfo("MAX30123_REG_STATUS1 : %02x \r\n ", status);
#endif

	if (status & STATUS1_ADC_DATA_RDY) {
		fifo_count = max30123_get_fifo_count();
#if _EIS_FIFO_INFO_DEBUG_
		swmLogInfo("fifo_count : %d\r\n ", fifo_count);
		max30123_delay_ms(1);
#endif

		while (fifo_count > 0u) {
			max30123_read_fifo_data(&max30123_fifo);

			if(max30123_fifo.data_type == FIFO_TYPE_WE1_PSTAT_CURR) {
				signed_raw = (int16_t)max30123_fifo.adc_val;
				nA = ((float)signed_raw / 128.0f);
				*we_current = nA;

#if _EIS_FIFO_INFO_DEBUG_
				swmLogInfo("WE1 : %d, "LOG_FLOAT_MARKER" nA  \r\n ", signed_raw, LOG_FLOAT(nA));
				max30123_delay_ms(1);
#endif
	    	}

	    	fifo_count--;
		}

		return 1;
	}

	return 0;
}


/********************************************************************************
 *	Chronoamperometry — 레지스터
 * 	Chrono 가 바꾸는 DC 설정 보관 / 복원
 * 	CHRONO_CLK_DIV : DC 에서 Sample Average > 1 이면 ISP > CONV_TIME 조건에 쓰인다
 * 	M0_AVG         : Chrono 동안 Sample Average = 1 로 내린다
 ********************************************************************************/
static void ca_save_dc_regs(void)
{
    ca_saved_clk_msb = spidrv_read_max30123_reg_action(MAX30123_REG_CHRONO_CLK_DIV_MSB);
    ca_saved_clk_lsb = spidrv_read_max30123_reg_action(MAX30123_REG_CHRONO_CLK_DIV_LSB);
    ca_saved_m0_avg  = spidrv_read_max30123_reg_action(MAX30123_REG_M0_AVG);
    ca_saved_valid   = 1u;
}

static void ca_restore_dc_regs(void)
{
    if (ca_saved_valid == 0u) { return; }
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_CLK_DIV_MSB, ca_saved_clk_msb);
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_CLK_DIV_LSB, ca_saved_clk_lsb);
    spidrv_write_max30123_reg_action(MAX30123_REG_M0_AVG,             ca_saved_m0_avg);
    ca_saved_valid = 0u;
}

static void ca_apply_row(uint8_t row)
{
    const chrono_A_cfg_t *c;

    if (row >= (uint8_t)CA_ROW_COUNT) { row = (uint8_t)CA_ROW_10MS; }
    c = &ca_rows[row];

    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_CLK_DIV_MSB, (uint8_t)((c->chrono_clk_div >> 8) & 0xFFu));
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_CLK_DIV_LSB, (uint8_t)( c->chrono_clk_div       & 0xFFu));

    spidrv_write_max30123_reg_action(CA_REG_AMPLITUDE,               (uint8_t)CA_AMP_MV);   /* 10 mV */
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_A_CFG1, CA_CFG1_VALUE);      /* PRE 4, AUTO_SUBTRACT 1 */
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_A_CFG2, CA_STEP_SMP_REG);    /* STEP 100 */
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_A_CFG3, 0x00u);              /* Recovery 0 */
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_A_CFG4, 0x00u);              /* Blank 0 */
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_A_CFG5, 0x00u);              /* Repeat 1 */
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_A_CFG6, CA_CHRONO_A_CFG6);   /* FS */

    spidrv_write_max30123_reg_action(MAX30123_REG_M1_CONV, c->m1_conv);
    spidrv_write_max30123_reg_action(MAX30123_REG_M1_AVG,  MEAS_AVE_1);

    ca_active_cfg = c;
    ca_active_idx = row;

    memset(&ca_acc, 0, sizeof(ca_acc));
    ca_acc.isp_sec  = c->isp_sec;
    ca_acc.conv_sec = c->conv_sec;
    ca_acc.rate     = row;

#if _MAX30123_INFO_DEBUG_
    swmLogInfo("[CA] row=%u ISP=" LOG_FLOAT_MARKER " ms dur=%lu ms\r\n",
               (unsigned)row, LOG_FLOAT(c->isp_sec * 1000.0f), (unsigned long)c->duration_ms);
#endif
}

/*********************************************************************************
 *	시퀀서 시작. return 1 = 정상, 0 = INVALID_CFG
 *********************************************************************************/
static uint8_t ca_seq_start(void)
{
    uint8_t st;

    /* INTB 미사용 */
    spidrv_write_max30123_reg_action(MAX30123_REG_GPIO_SETUP1, GPIO_SETUP1_INTB_UNUSED);
    spidrv_write_max30123_reg_action(MAX30123_REG_INT_EN1, 0x00u);
    spidrv_write_max30123_reg_action(MAX30123_REG_INT_EN2, 0x00u);

    /* Seq. Sample Period 12 s */
    spidrv_write_max30123_reg_action(MAX30123_REG_AUTO_CLK_DIV_MSB, (uint8_t)((CA_SEQ_CLK_DIV >> 16) & 0xFFu));
    spidrv_write_max30123_reg_action(MAX30123_REG_AUTO_CLK_DIV_MID, (uint8_t)((CA_SEQ_CLK_DIV >>  8) & 0xFFu));
    spidrv_write_max30123_reg_action(MAX30123_REG_AUTO_CLK_DIV_LSB, (uint8_t)( CA_SEQ_CLK_DIV        & 0xFFu));

    /* M0 = PSTAT 유지 (시퀀서 시작 조건), Sample Average 1 (M0 CONV 40 ms > ISP 충돌 방지) */
    spidrv_write_max30123_reg_action(MAX30123_REG_M0_AVG, MEAS_AVE_1);
    spidrv_write_max30123_reg_action(MAX30123_REG_M2_CFG, 0x00u);
    spidrv_write_max30123_reg_action(MAX30123_REG_M3_CFG, 0x00u);
    spidrv_write_max30123_reg_action(MAX30123_REG_M4_CFG, 0x00u);

    max30123_fifo_flush();
    spidrv_write_max30123_reg_action(MAX30123_REG_M1_CFG, CA_M1_CFG_ON);

    spidrv_write_max30123_reg_action(MAX30123_REG_CONVERT_MODE, CONVERT_MODE_STOP);
    max30123_delay_ms(1);
    spidrv_write_max30123_reg_action(MAX30123_REG_CONVERT_MODE, CONVERT_MODE_AUTO | CONVERT_MODE_CONVERT);

    st = spidrv_read_max30123_reg_action(MAX30123_REG_STATUS1);
    if ((st & STATUS1_INVALID_CFG) != 0u) {
        swmLogInfo("[CA] INVALID_CFG st=0x%02X\r\n", st);
        return 0u;
    }
    return 1u;
}

/*********************************************************************************
 *	시퀀서 정지 → SEQ_RDY 대기 → M1 OFF → FIFO 비움
 *********************************************************************************/
void max30123_chrono_A_stop(void)
{
    uint8_t  st;
    uint16_t t;

    spidrv_write_max30123_reg_action(MAX30123_REG_CONVERT_MODE, CONVERT_MODE_STOP);

    for (t = 0u; t < CA_SEQ_STOP_TMOUT_MS; t++) {
        st = spidrv_read_max30123_reg_action(MAX30123_REG_STATUS1);
        if ((st & STATUS1_SEQ_RDY) != 0u) { break; }
        max30123_delay_ms(1);
    }

    spidrv_write_max30123_reg_action(MAX30123_REG_M1_CFG, CA_M1_CFG_OFF);
    max30123_fifo_flush();
}

/*********************************************************************************
 *	FIFO 에 있는 만큼 읽어 STEP 을 누적한다 (폴링 사이에 이어서 누적).
 *	return 1 = STEP 100 개 완성, 0 = 아직, -1 = OVF
 *********************************************************************************/
static int8_t ca_collect(void)
{
    uint8_t  ovf = 0u;
    uint16_t cnt, i;
    uint16_t n_m0 = 0u, n_pre = 0u, n_step = 0u, n_etc = 0u;

    cnt = max30123_fifo_count_ovf(&ovf);
    if (ovf != 0u) {
        swmLogInfo("[CA] FIFO OVF=%u\r\n", (unsigned)ovf);
        return -1;
    }
    if (cnt > MAX30123_FIFO_DEPTH) { cnt = MAX30123_FIFO_DEPTH; }

    for (i = 0u; i < cnt; i++) {
        if (max30123_read_fifo_data(&max30123_fifo) == 0) { break; }

        if (max30123_fifo.meas_id == CA_MEAS_M0)            { n_m0++;  continue; }  /* Chrono 중 PSTAT 버림 */
        if (max30123_fifo.meas_id != CA_MEAS_M1)            { n_etc++; continue; }
        if (max30123_fifo.data_type == FIFO_TYPE_WE1_PRE)   { n_pre++; continue; }
        if (max30123_fifo.data_type != FIFO_TYPE_WE1_STEP)  { n_etc++; continue; }

        n_step++;
        if (ca_acc.n_step < CA_STEP_TARGET) {
            ca_acc.step_nA[ca_acc.n_step] = ca_code_to_nA(max30123_fifo.adc_val);
            ca_acc.n_step++;
        }
        if (ca_acc.n_step >= CA_STEP_TARGET) { break; }      /* 나머지는 stop 에서 flush */
    }

#if _CA_FIFO_VERIFY_
    swmLogInfo("[CA] cnt=%u read=%u (M0=%u PRE=%u STEP=%u etc=%u) acc=%u/%u\r\n",
               cnt, (unsigned)(n_m0 + n_pre + n_step + n_etc),
               n_m0, n_pre, n_step, n_etc,
               (unsigned)ca_acc.n_step, (unsigned)CA_STEP_TARGET);
#else
    (void)n_m0; (void)n_pre; (void)n_step; (void)n_etc;
#endif

    return (ca_acc.n_step >= CA_STEP_TARGET) ? 1 : 0;
}


/* ================================================================================
 *  #4. Chronoamperometry — 계산
 * ================================================================================ */

/* 3점 프로브 : τ = Δ / ln[(I₁−I₂)/(I₂−I₃)]
 *   I∞ 와 상수 오프셋이 소거되어 창이 τ 보다 짧아도 범위 판정에 쓸 수 있다.
 *   return 0          : 꼬리가 평탄 (τ 가 창에 비해 작음)
 *          CAF_TAU_HUGE : 감소가 거의 직선 (τ 가 창보다 훨씬 김) */
float ca_probe_tau(const chrono_a_raw_t *in)
{
    float    i1 = 0.0f, i2 = 0.0f, i3 = 0.0f;
    float    num, den, d;
    uint16_t k, n, gap;

    if (in == NULL) { return 0.0f; }
    n = (uint16_t)in->n_step;
    if (n < (3u * CAF_PROBE_N)) { return 0.0f; }

    gap = (uint16_t)((n - CAF_PROBE_N) / 2u);

    for (k = 0u; k < CAF_PROBE_N; k++) {
        i1 += in->step_nA[k];
        i2 += in->step_nA[gap + k];
        i3 += in->step_nA[(2u * gap) + k];
    }
    i1 /= (float)CAF_PROBE_N;
    i2 /= (float)CAF_PROBE_N;
    i3 /= (float)CAF_PROBE_N;

    num = i1 - i2;
    den = i2 - i3;

    if (den < CAF_PROBE_DEN_NA) { return 0.0f; }             /* 뒤쪽이 평탄 */
    if (num <= den)             { return CAF_TAU_HUGE; }     /* 감소율 차이 없음 → 매우 긴 τ */

    d = logf(num / den);
    if (d < 1e-4f) { return CAF_TAU_HUGE; }

    return ((float)gap * in->isp_sec) / d;
}

/* Randles 피팅
 *   샘플 k 는 적분 구간 [k·ISP, k·ISP + conv] 의 평균이므로
 *     관측값 = I∞ + A·g·e^(−k·ISP/τ),   g = (τ/conv)·(1 − e^(−conv/τ))
 *   ① I∞ = 꼬리 평균 → ② ln(I − I∞) 회귀로 τ, A·g → ③ 꼬리의 잔여 감쇠를 빼서 I∞ 갱신
 *   ②③ 을 CAF_ITER 회 반복 (3-파라미터 추정의 근사) */
uint8_t max30123_step_fit(const chrono_a_raw_t *in, ca_fit_t *out)
{
    uint16_t k, n, n_fit, cnt;
    float    isp, conv, t, y;
    float    tail_mean = 0.0f, mean_e;
    float    sw, w, sx, sy, sxx, sxy, den, slope, icept;
    float    iinf, tau = 0.0f, ag = 0.0f, g, a, i0;
    float    rs, rtot, rct;
    uint8_t  it, row;

    if ((in == NULL) || (out == NULL)) { return 0u; }

    n = (uint16_t)in->n_step;
    if (n < (CAF_TAIL_N + CAF_FIT_MIN_PTS)) { return 0u; }

    memset(out, 0, sizeof(*out));
    out->n_exp = 1u;

    isp   = in->isp_sec;
    conv  = in->conv_sec;
    row   = (uint8_t)in->rate;
    n_fit = (uint16_t)(((uint32_t)n * CAF_FIT_NUM) / CAF_FIT_DEN);

    for (k = (uint16_t)(n - CAF_TAIL_N); k < n; k++) {
        tail_mean += in->step_nA[k];
    }
    tail_mean /= (float)CAF_TAIL_N;
    iinf = tail_mean;

    for (it = 0u; it < CAF_ITER; it++) {
        sw = 0.0f; sx = 0.0f; sy = 0.0f; sxx = 0.0f; sxy = 0.0f; cnt = 0u;

        /* ln 영역 가중 회귀 : w = y²
         * 작은 y 의 ln 값은 노이즈가 크게 부풀려지므로 그만큼 가중치를 낮춘다 */
        for (k = 0u; k < n_fit; k++) {
            y = in->step_nA[k] - iinf;
            if (y <= CAF_Y_FLOOR_NA) { continue; }
            w  = y * y;
            t  = (float)k * isp;
            y  = logf(y);
            sw += w;  sx += w * t;  sy += w * y;  sxx += w * t * t;  sxy += w * t * y;
            cnt++;
        }
        if (cnt < CAF_FIT_MIN_PTS) { return 0u; }

        den = (sw * sxx) - (sx * sx);
        if (den <= 0.0f) { return 0u; }

        slope = ((sw * sxy) - (sx * sy)) / den;              /* −1/τ */
        icept = (sy - (slope * sx)) / sw;                    /* ln(A·g) */
        if (slope >= -1e-4f) { return 0u; }

        tau = -1.0f / slope;
        ag  = expf(icept);

        /* 꼬리 평균에 남아 있는 감쇠분을 빼서 I∞ 갱신 */
        mean_e = 0.0f;
        for (k = (uint16_t)(n - CAF_TAIL_N); k < n; k++) {
            mean_e += expf(-((float)k * isp) / tau);
        }
        mean_e /= (float)CAF_TAIL_N;
        iinf = tail_mean - (ag * mean_e);
    }

    /* 적분창 보정 → t = 0 진폭 */
    g  = (conv > 0.0f) ? ((tau / conv) * (1.0f - expf(-conv / tau))) : 1.0f;
    a  = ag / g;

    /* Period 별 꼬리 오프셋 보정 (A 에는 영향 없음) */
    if (row < (uint8_t)CA_ROW_COUNT) {
        iinf -= ca_iinf_ofs_nA[row];
    }
    i0 = iinf + a;

    out->tau_sec[0] = tau;
    out->a_nA[0]    = a;
    out->i_inf_nA   = iinf;
    out->i0_nA      = i0;

    if (iinf < CAF_IINF_MIN_NA) {
        swmLogInfo("[CA] I_inf too small -> Rct not measurable\r\n");
        return 0u;
    }
    if ((i0 <= iinf) || (i0 > (CA_FS_NA * 2.0f))) { return 0u; }

    rs   = (CA_DV_MV / i0)   * 1.0e6f;                       /* mV / nA × 1e6 = Ω */
    rtot = (CA_DV_MV / iinf) * 1.0e6f;
    rct  = rtot - rs;
    if (rct < 1.0f) { return 0u; }

    out->rs_ohm      = rs;
    out->r_total_ohm = rtot;
    out->r1_ohm      = rct;                                  /* Rct */
    out->c1_F        = tau * rtot / (rs * rct);              /* τ / (Rs ∥ Rct) */

    return 1u;
}

/* Z(ω) = Rs + Rct / (1 + jωRctCdl) */
uint8_t max30123_z_from_fit(const ca_fit_t *f, float freq_hz, impedance_result_t *out)
{
    float w, wt, d, zr, zi;

    if ((f == NULL) || (out == NULL) || (freq_hz <= 0.0f)) { return 0u; }
    if ((f->rs_ohm <= 0.0f) || (f->r1_ohm <= 0.0f) || (f->c1_F <= 0.0f)) { return 0u; }

    w  = 6.28318531f * freq_hz;
    wt = w * f->r1_ohm * f->c1_F;
    d  = 1.0f + (wt * wt);

    zr = f->rs_ohm + (f->r1_ohm / d);
    zi = -(f->r1_ohm * wt) / d;

    out->z_real      = ca_round(zr);
    out->z_imag      = ca_round(zi);
    out->z_mag       = ca_round(sqrtf((zr * zr) + (zi * zi)));
    out->z_phase_deg = ca_round(atan2f(zi, zr) * 57.29578f);

    return 1u;
}

/* 측정 창이 근거가 되는 대역 안에서 로그 등간격으로 주파수 선택
 *   f_hi = f_max/2 = 1/(4π·ISP),  f_lo = 2·f_min = 2/(2π·T),  T = STEP × ISP */
static void ca_z_freqs(const chrono_A_cfg_t *c, float *f)
{
    float    T    = (float)CA_STEP_TARGET * c->isp_sec;
    float    f_hi = 1.0f / (2.0f * 6.28318531f * c->isp_sec);
    float    f_lo = 2.0f / (6.28318531f * T);
    uint16_t i;

    if (CA_Z_FREQ_COUNT == 1u) {
        f[0] = sqrtf(f_lo * f_hi);
        return;
    }
    for (i = 0u; i < (uint16_t)CA_Z_FREQ_COUNT; i++) {
        f[i] = f_lo * expf(logf(f_hi / f_lo) * (float)i / (float)(CA_Z_FREQ_COUNT - 1u));
    }
}

static uint8_t ca_analyze(void)
{
    const chrono_A_cfg_t *c = ca_active_cfg;
    float    tau_max, tau_p;
    uint16_t i;

    if (c == NULL) { return CA_OUT_FAIL; }

    tau_max = ((float)CA_STEP_TARGET * c->isp_sec) / CAF_TAU_WIN_DIV;
    tau_p   = ca_probe_tau(&ca_acc);

#if _MAX30123_INFO_DEBUG_
    swmLogInfo("[CA] probe tau=" LOG_FLOAT_MARKER " s (max " LOG_FLOAT_MARKER ")\r\n",
               LOG_FLOAT(tau_p), LOG_FLOAT(tau_max));
#endif

    /* 1 s 창으로 부족 → 100.01 ms 재측정 */
    if ((ca_row == CA_ROW_10MS) && (tau_p > tau_max)) {
        return CA_OUT_SLOW;
    }

    if (max30123_step_fit(&ca_acc, &ca_fit) == 0u) {
        return CA_OUT_FAIL;
    }

    if (ca_fit.tau_sec[0] > tau_max) {
        return (ca_row == CA_ROW_10MS) ? CA_OUT_SLOW : CA_OUT_FAIL;
    }

    ca_z_freqs(c, ca_z_freq_hz);
    for (i = 0u; i < (uint16_t)CA_Z_FREQ_COUNT; i++) {
        (void)max30123_z_from_fit(&ca_fit, ca_z_freq_hz[i], &impedance_result[i]);
    }

#if 1// _IMPEDANCE_RESULT_DEBUG_
    swmLogInfo("[CA] row=%u tau=" LOG_FLOAT_MARKER " s I0=" LOG_FLOAT_MARKER " Iinf=" LOG_FLOAT_MARKER " nA\r\n",
               (unsigned)ca_row, LOG_FLOAT(ca_fit.tau_sec[0]),
               LOG_FLOAT(ca_fit.i0_nA), LOG_FLOAT(ca_fit.i_inf_nA));
    max30123_delay_ms(1);
    swmLogInfo("[CA] Rs=" LOG_FLOAT_MARKER " k Rct=" LOG_FLOAT_MARKER " k Cdl=" LOG_FLOAT_MARKER " uF\r\n",
               LOG_FLOAT(ca_fit.rs_ohm * 0.001f), LOG_FLOAT(ca_fit.r1_ohm * 0.001f),
               LOG_FLOAT(ca_fit.c1_F * 1.0e6f));
    max30123_delay_ms(1);
    for (i = 0u; i < (uint16_t)CA_Z_FREQ_COUNT; i++) {
        swmLogInfo("[Z] f=" LOG_FLOAT_MARKER " Hz Re=%ld Im=%ld |Z|=%ld ph=%ld\r\n",
                   LOG_FLOAT(ca_z_freq_hz[i]),
                   (long)impedance_result[i].z_real, (long)impedance_result[i].z_imag,
                   (long)impedance_result[i].z_mag,  (long)impedance_result[i].z_phase_deg);
        max30123_delay_ms(1);
    }
#endif

    return CA_OUT_OK;
}



/* ================================================================================
 *  #5. Chronoamperometry — 상태 머신 (외부 타이머 Call_Custom_NTF_Timeout 로 재진입)
 *
 *   0 : 행 적용 + 시퀀서 시작
 *   1 : 2 s 마다 FIFO 수거, STEP 100 개 → 정지 → 계산
 *   2 : 결과 판정 (전송 / 100.01 ms 재측정 / 재시도)
 *   3 : DC 설정 복원 → DC 수동 측정으로 복귀
 * ================================================================================ */
void max30123_measure_ChronoAmperometry(void)
{
	int8_t r;
	uint32_t current_epoch;
	int mok;

	switch (max30123_chrono_A_step) {
		case 0:
			if (ca_saved_valid == 0u) {
				ca_save_dc_regs();
			}
			ca_apply_row(ca_row);
			ca_result_valid = 0u;
			ca_outcome      = CA_OUT_FAIL;

			if (ca_seq_start() == 0u) {
				max30123_chrono_A_stop();
				max30123_chrono_A_step = 2u;
				Call_Custom_NTF_Timeout(1u);
				break;
			}

			ca_wait_s     = 0u;
			ca_deadline_s = (uint16_t)(CA_SEQ_PERIOD_SEC
						  + ((ca_active_cfg->duration_ms + 999u) / 1000u)
						  + CA_WAIT_MARGIN_SEC);
	#if _MAX30123_INFO_DEBUG_
			swmLogInfo("[CA] start row=%u deadline=%u s\r\n", (unsigned)ca_row, (unsigned)ca_deadline_s);
	#endif
			max30123_chrono_A_step = 1u;
			Call_Custom_NTF_Timeout(CA_POLL_SEC);

			max30123_chrono_A_epoch = get_epoch_time(&calendar);				// 전송 시간을 기록 한다.
			break;

		case 1:
			r = ca_collect();

			if (r == 0) {
				ca_wait_s = (uint16_t)(ca_wait_s + CA_POLL_SEC);
				if (ca_wait_s < ca_deadline_s) {
					Call_Custom_NTF_Timeout(CA_POLL_SEC);
					break;
				}
				swmLogInfo("[CA] timeout acc=%u/%u\r\n", (unsigned)ca_acc.n_step, (unsigned)CA_STEP_TARGET);
			}

			max30123_chrono_A_stop();                           /* 다 모은 뒤에만 정지 */

			ca_outcome = (r == 1) ? ca_analyze() : (uint8_t)CA_OUT_FAIL;

			max30123_chrono_A_step = 2u;
			Call_Custom_NTF_Timeout(1u);
			break;

		case 2:
			if (ca_outcome == CA_OUT_OK) {
				ca_result_valid = 1u;
				impedance_calendar.epoch = get_epoch_time(&calendar);
				max30123_send_attribute(APP_ATTR_SEND_AGMS_VALUE);

				max30123_chrono_A_step = 3u;
				Call_Custom_NTF_Timeout(1u);
				break;
			}

			if ((ca_outcome == CA_OUT_SLOW) && (ca_row == CA_ROW_10MS)) {
	#if _MAX30123_INFO_DEBUG_
				swmLogInfo("[CA] tau too long -> 100.01 ms\r\n");
	#endif
				ca_row   = CA_ROW_100MS;
				ca_retry = 0u;
				max30123_chrono_A_step = 0u;
				Call_Custom_NTF_Timeout(CA_RELAX_SEC);
				break;
			}

			if (ca_retry < CA_RETRY_MAX) {
				ca_retry++;
				swmLogInfo("[CA] retry %u/%u row=%u\r\n",
						   (unsigned)ca_retry, (unsigned)CA_RETRY_MAX, (unsigned)ca_row);
				max30123_chrono_A_step = 0u;
				Call_Custom_NTF_Timeout(CA_RELAX_SEC);
				break;
			}

			swmLogInfo("[CA] fail row=%u\r\n", (unsigned)ca_row);
			max30123_chrono_A_step = 3u;
			Call_Custom_NTF_Timeout(1u);
			break;

		case 3:
			ca_restore_dc_regs();
			ca_row   = CA_ROW_10MS;                             /* 다음 회차는 다시 10.01 ms 부터 */
			ca_retry = 0u;

			max30123_chrono_A_wait_count = CA_DC_DISCARD_N;     /* Chrono 교란 후 DC 몇 개 버림 */
			max30123_chrono_A_step       = 0u;
			max30123_measure_step        = MAX30123_STEP_DC_CURRENT;

			current_epoch = get_epoch_time(&calendar);
			current_epoch = (uint32_t)(current_epoch - max30123_chrono_A_epoch);
			mok = current_epoch%10;
			mok = 10 - mok;
			swmLogInfo("[CA] pass time = [%d][%d]\r\n", (uint32_t)(max30123_chrono_A_epoch - current_epoch), mok);
			Call_Custom_NTF_Timeout(mok);

			device_info.agms_mode = AGMS_MODE_CHRONO_A;
			max30123_send_attribute(APP_ATTR_SEND_AGMS_VALUE);

			break;

		default:
			max30123_chrono_A_stop();
			ca_restore_dc_regs();
			ca_row   = CA_ROW_10MS;
			ca_retry = 0u;
			max30123_chrono_A_step = 0u;
			max30123_measure_step  = MAX30123_STEP_DC_CURRENT;
			break;
    }
}


/* ================================================================================
 *  전체 진행
 * ================================================================================ */
void max30123_measure_Process(void)
{
    float    we_current = 0.0f;
    uint16_t cal = 0u;

    if (read_custom_irq() != 1) {	return;    }
    clear_custom_irq();

    switch (max30123_measure_step) {
		case MAX30123_STEP_CALIBRATION:
			if (max30123_measure_calibration(&cal) != 0u) {
				max30123_measure_step = MAX30123_STEP_DC_CURRENT;
			}
			break;

		case MAX30123_STEP_DC_CURRENT:
			if (max30123_measure_DC_manual_mode(&we_current) == 0u) {
				break;
			}

			if (calendar.update != DATE_TIME_UPDATE_SUCCESS) {
				break;
			}

#if 1
			append_Potentiostat_data();
			max30123_send_attribute(APP_ATTR_SEND_AGMS_VALUE);

#else
			rb_push(&ring_buff, we_current, we_current);

	#if _EIS_FIFO_INFO_DEBUG_
			swmLogInfo("[DC] " LOG_FLOAT_MARKER " nA [%d]\r\n", LOG_FLOAT(we_current), ring_buff.count);
	#endif

			if (ring_buff.count >= 6) {                          /* 10 s × 6 = 60 s */

				if(calendar.resync == CALENDAR_UPDATE_SUCCESS) {			// 실제 사용할 앱인지 확인이 완료 되면 그 떄 전송을 한다.
					rb_flush_trimmed_average(&ring_buff, &device_info.we1_current, &device_info.we2_current);
					append_Potentiostat_data();
					max30123_send_attribute(APP_ATTR_SEND_AGMS_VALUE);
				}
			}
#endif
			break;

		case MAX30123_STEP_CHRONO_A:
			max30123_measure_ChronoAmperometry();
			break;

		default:
			break;
    }
}

/***************************************************************************************************
 *	Chrono 주기 타이머 (1분 단위 호출)
 ***************************************************************************************************/
void max30123_M1_chrono_A_Timeout_Task(void)
{
    max30123_chrono_A_measure_tmout++;
    max30123_chrono_A_measure_tmout %= CHRONO_A_MEASURE_TIME;

    if (max30123_chrono_A_measure_tmout == 0u) {
        max30123_measure_step  = MAX30123_STEP_CHRONO_A;
        max30123_chrono_A_step = 0u;

#if _MAX30123_INFO_DEBUG_
        swmLogInfo("M1_chrono_A start\r\n");
#endif
    }
}

/******************************************************************************************
 *  레지스터 확인 (Chrono 시작 직후 호출)
 *   기대값 : 35=0A (10 mV)  15/16/17=06/00/00 (12 s)  60=03  62=20
 *            18/19 = 01/48 (10.01 ms) 또는 0C/CD (100.01 ms)
 *******************************************************************************************/
void max30123_reg_dump(void)
{
    swmLogInfo("[CHK] 35=%02X 37=%02X 38=%02X 39=%02X 3A=%02X 3B=%02X 3C=%02X\r\n",
        (unsigned)spidrv_read_max30123_reg_action(0x35),
        (unsigned)spidrv_read_max30123_reg_action(0x37),
        (unsigned)spidrv_read_max30123_reg_action(0x38),
        (unsigned)spidrv_read_max30123_reg_action(0x39),
        (unsigned)spidrv_read_max30123_reg_action(0x3A),
        (unsigned)spidrv_read_max30123_reg_action(0x3B),
        (unsigned)spidrv_read_max30123_reg_action(0x3C));
    max30123_delay_ms(1);
    swmLogInfo("[CHK] 15=%02X 16=%02X 17=%02X 18=%02X 19=%02X 60=%02X 62=%02X 64=%02X 70=%02X 71=%02X 72=%02X\r\n",
        (unsigned)spidrv_read_max30123_reg_action(0x15),
        (unsigned)spidrv_read_max30123_reg_action(0x16),
        (unsigned)spidrv_read_max30123_reg_action(0x17),
        (unsigned)spidrv_read_max30123_reg_action(0x18),
        (unsigned)spidrv_read_max30123_reg_action(0x19),
        (unsigned)spidrv_read_max30123_reg_action(0x60),
        (unsigned)spidrv_read_max30123_reg_action(0x62),
        (unsigned)spidrv_read_max30123_reg_action(0x64),
        (unsigned)spidrv_read_max30123_reg_action(0x70),
        (unsigned)spidrv_read_max30123_reg_action(0x71),
        (unsigned)spidrv_read_max30123_reg_action(0x72));
}




