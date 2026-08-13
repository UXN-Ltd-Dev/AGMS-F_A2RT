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


extern uint8_t custom_irq ;
uint16_t cal = 0 ;

uint8_t max30123_measure_step = 0;
uint8_t max30123_chronoamperometry = 0;
uint8_t max30123_chrono_A_wait_count = 0;
uint8_t max30123_convert_mode = 0u;

int max30123_auto_mode_count = 0;

Chron_Freq_e max30123_chrono_A_count = CHRONO_FREQ_1HZ;

const char *chrono_name[3] = {
		"FREQ  1Hz",
		"FREQ 10Hz",
		"FREQ 25Hz"
};


/***************************************************************************************************************
 *  주파수 테이블
 *	freq_hz = 1 / (10 * isp_sec),  isp_sec = chrono_clk_div / 32768
 ***************************************************************************************************************/
/*     freq_hz     isp_sec       clk_div   */
const chrono_freq_cfg_t chrono_setup[CHRONO_FREQ_COUNT] = {
    {  0.99994f,   0.1000061f,   0x0CCD},		// 1Hz
	{  9.99024f,   0.0100098f,   0x0148},		// 10Hz
    {  25.01380f,  0.0039978f,   0x0083}		// 25Hz
};

max30123_fifo_t	max30123_fifo;

/************************************************************************************************************************
 *   상수...
 ************************************************************************************************************************/
#define CA_FS_NA             1024.0f
#define CA_M1_ID             1u     		/* M1 = Chrono A */

/* FIFO data_type 상수 (기존 코드와 동일 기준) */
#ifndef FIFO_TYPE_WE1_PRE
#define FIFO_TYPE_WE1_PRE    0x02u
#endif
#ifndef FIFO_TYPE_WE1_STEP
#define FIFO_TYPE_WE1_STEP   0x03u
#endif
#ifndef FIFO_TYPE_WE1_REC
#define FIFO_TYPE_WE1_REC    0x04u   /* REC = Recovery = POST */
#endif

impedance_result_t		impedance_result[CHRONO_FREQ_COUNT];
impedance_calendar_t	impedance_calendar;

impedance_result_t		z_result[CHRONO_FREQ_COUNT];

uint16_t max30123_chrono_A_wait_tmout = 0;

/************************************************************************************************************************
 *   SPI CONTROL
 ************************************************************************************************************************/
int spidrv_write_max30123_reg_action(const  uint8_t reg,  const uint8_t  val)
{
    int result;
    uint8_t buffer[4];

    buffer[0] = val;
    result=cem102->max30132_write_reg_action(cem102_dut,   reg, buffer, 1);

    return result;
}

uint8_t spidrv_read_max30123_reg_action(const  uint8_t reg)
{
    int result;
    uint8_t buffer[4];

    result=cem102->max30132_read_reg_action(cem102_dut,  reg, buffer, 1);

    if (result == ERRNO_NO_ERROR) {
        return buffer[0];
    }

    return 0;
}



void max30123_delay_ms(int i)
{
	do {
		Sys_Delay(SystemCoreClock / 1000);	// 1ms wait
	} while(i-->0);
}

/************************************************************************************************************************
 *   mac30123 value memory attach
 ************************************************************************************************************************/
void max30123_malloc_attach(void)
{
	memset(&impedance_result[0], 0, sizeof(impedance_result_t));
	memset(&impedance_result[1], 0, sizeof(impedance_result_t));
	memset(&impedance_result[2], 0, sizeof(impedance_result_t));
	memset(&impedance_calendar, 0, sizeof(impedance_calendar_t));

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
	uint8_t tmout;
	uint8_t status;

	tmout = 200;

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
	uint8_t reg_val = 0;

	reg_val = spidrv_read_max30123_reg_action(MAX30123_REG_WE1_CFG1);

	if(idle_mode==ENABLE) {
		spidrv_write_max30123_reg_action(MAX30123_REG_PSTAT_CFG, 0x0F);		// POR_DUTY(1) | HALF_CP_CLK(1) | AFE_CHOP_EN(1) | HALF_IB(1)

		reg_val|=WE1_CFG1_CHOP_EN;
		spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG1, reg_val);
	}
	else {
		spidrv_write_max30123_reg_action(MAX30123_REG_PSTAT_CFG, 0x0A);		// POR_DUTY(1) | HALF_CP_CLK(0) | AFE_CHOP_EN(1) | HALF_IB(0)

		reg_val&=~WE1_CFG1_CHOP_EN;
		spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG1, reg_val);
	}
}

/********************************************************************************
 *	CONVERTER MODE : AUTO MODE & MANUAL MODE
 ********************************************************************************/
void max30123_reg_convert_mode(uint8_t mode, uint8_t converter)
{
	uint8_t reg_val = 0;

	reg_val = spidrv_read_max30123_reg_action(MAX30123_REG_CONVERT_MODE);
	reg_val&= 0xFC;
	reg_val = (reg_val | mode | converter);

	spidrv_write_max30123_reg_action(MAX30123_REG_CONVERT_MODE, reg_val);
}



/********************************************************************************
 * CONVERT MODE START
 * STATUS 1 상태 확인
 ********************************************************************************/
uint8_t max30123_reg_convert_and_status(void)
{
	uint8_t results = 0;
	uint8_t status = 0;
	int loop = 300;

	spidrv_write_max30123_reg_action(MAX30123_REG_CONVERT_MODE, CONVERT_MODE_CONVERT);

	do {
		status = spidrv_read_max30123_reg_action(MAX30123_REG_STATUS1);

		if (status & STATUS1_ADC_DATA_RDY) {
			results = 1;
			break;
		}
		max30123_delay_ms(10);
	} while (loop-- > 0);

	return results;
}
/********************************************************************************
 * @brief 현재 FIFO에 저장된 유효 샘플 수를 확인합니다.
 * @return 읽기 가능한 샘플 수 (0~256)
 ********************************************************************************/
uint16_t max30123_get_fifo_count(void)
{
	uint8_t msb = 0;
	uint8_t lsb = 0;

	msb = spidrv_read_max30123_reg_action(MAX30123_REG_FIFO_COUNTER1);
	lsb = spidrv_read_max30123_reg_action(MAX30123_REG_FIFO_COUNTER2);

	return (uint16_t)(((msb & 0x80u) << 1) | lsb);	// MSB의 최하위 1비트와 LSB 8비트를 조합하여 9비트 카운트 생성
}

/********************************************************************************
 * @brief FIFO에서 샘플 1개를 읽어옵니다.
 * @param sample 읽어온 데이터를 저장할 구조체 포인터
 * @return 성공 시 true, FIFO가 비어있으면 false
 ********************************************************************************/
int8_t max30123_read_fifo_data(max30123_fifo_t *p)
{
	uint8_t buf[3];


    cem102->max30132_read_reg_action(cem102_dut,
                                     MAX30123_REG_FIFO_DATA,
                                     buf, 3);

#if 0 	//#ifdef _MAX30123_INFO_DEBUG_
	swmLogInfo("max30123_get_fifo_count : [%02x][%02x][%02x] \r\n", buf[0], buf[1],buf[2] );
#endif

    if (buf[0] == 0xFF && buf[1] == 0xFF && buf[2] == 0xFF) {
        return 0;
    }

    p->meas_id   = (buf[0] >> 5) & 0x07;
    p->data_type = buf[0] & 0x1F;
    p->adc_val   = (uint16_t)((buf[1] << 8) | buf[2]);

    return 1;
}


/***********************************************************************************************************************
 *	MAX30123 measure calibration
 ***********************************************************************************************************************/
uint8_t max30123_measure_calibration(uint16_t *offset_out)
{
	uint8_t we1_cfg1;
	uint8_t we1_cfg2;
	uint8_t m0_conv;
    uint8_t saved_conv;
    uint8_t off_buf[2];
    uint16_t stored;
	uint16_t fifo_count;

// STEP 0 : SWB/SRB Open - 센서 신호 차단
    spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG1,0x80);		// SWB_OPEN
    spidrv_write_max30123_reg_action(MAX30123_REG_CE1_CFG, 0x90);		// SRB_OPEN
    max30123_delay_ms(10u);

// STEP 1 : STORE_OFFSET = 1
    we1_cfg2 = spidrv_read_max30123_reg_action(MAX30123_REG_WE1_CFG2);
    we1_cfg2|= WE1_CFG2_STORE_OFFSET;
	spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG2, we1_cfg2);

#if _EIS_FIFO_INFO_DEBUG_
	swmLogInfo("MAX30123_REG_WE1_CFG2 : [0x%02X] \r\n", we1_cfg2);
#endif

// STEP 2 : M0 I_CONV_TYPE → 01 (Offset only)
	m0_conv = spidrv_read_max30123_reg_action(MAX30123_REG_M0_CONV);
    saved_conv = m0_conv;   					// 나중에 복원용
    m0_conv = (m0_conv & 0x3Fu) | (I_CONV_OFFSET_ONLY << MN_I_CONV_TYPE_SHIFT);	// 하위 6비트 유지 (V_CONV, CONV_TIME)
	spidrv_write_max30123_reg_action(MAX30123_REG_M0_CONV, m0_conv);

#if _EIS_FIFO_INFO_DEBUG_
	swmLogInfo("MAX30123_REG_M0_CONV : [0x%02X] \r\n", m0_conv);
#endif

// STEP 3 : FIFO flush
	spidrv_write_max30123_reg_action(MAX30123_REG_FIFO_CONFIG2,
										FIFO_CFG2_FLUSH_FIFO |
										FIFO_CFG2_FIFO_STAT_CLR |
										FIFO_CFG2_A_FULL_TYPE);

	if(max30123_reg_convert_and_status() == 0)
		return 0;

// STEP 4 : FIFO에서 캘리브레이션 데이터 읽기 (버려도 됨)
	fifo_count = max30123_get_fifo_count();
	while (fifo_count > 0u) {
	    max30123_read_fifo_data(&max30123_fifo);
	    fifo_count--;
	}


// STEP 5 : WE1_I_OFFSET 검증
	off_buf[0] = spidrv_read_max30123_reg_action(MAX30123_REG_WE1_I_OFFSET_MSB);
	off_buf[1] = spidrv_read_max30123_reg_action(MAX30123_REG_WE1_I_OFFSET_LSB);
    stored = ((uint16_t)off_buf[0] << 8) | off_buf[1];
	*offset_out = stored;

#if _EIS_FIFO_INFO_DEBUG_
	swmLogInfo("WE1_I_OFFSET : 0x%02X 0x%02X [%d] \r\n", off_buf[0], off_buf[1], stored);
#endif

// STEP 6 :
    we1_cfg2 &= ~WE1_CFG2_STORE_OFFSET;
	spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG2, we1_cfg2);

#if _EIS_FIFO_INFO_DEBUG_
	swmLogInfo("8. MAX30123_REG_WE1_CFG2 : [0x%02X] \r\n", we1_cfg2);
#endif

// STEP 7 :	M0 I_CONV_TYPE → 11 (Signed) 복원 ----
    saved_conv = (saved_conv & 0x3Fu)|(I_CONV_SIG_SIGNED << MN_I_CONV_TYPE_SHIFT);
	spidrv_write_max30123_reg_action(MAX30123_REG_M0_CONV, saved_conv);

#if _EIS_FIFO_INFO_DEBUG_
	swmLogInfo("9. MAX30123_REG_M0_CONV : [0x%02X] \r\n", saved_conv);
    max30123_delay_ms(10);
#endif

// STEP 8 : SWB/SRB Close

	spidrv_write_max30123_reg_action(MAX30123_REG_PSTAT_CFG, 0x0F);		// POR_DUTY(1) | HALF_CP_CLK(1) | AFE_CHOP_EN(1) | HALF_IB(1)
	we1_cfg1 = (WE1_CFG1_AMP_EN | WE1_CFG1_CHOP_EN | WE1_CFG1_SWB);
	spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG1, we1_cfg1);
	spidrv_write_max30123_reg_action(MAX30123_REG_CE1_CFG, 0x91);		// SRB_CLOSE


    return 1;
}

/***********************************************************************************************************************
 *	MAX30123 measure DC current... 수동 모두
 ***********************************************************************************************************************/
uint8_t max30123_measure_DC_oneshot(void)
{
    int16_t signed_raw;
    uint16_t fifo_count;
    float nA;

    /* FIFO flush */
    spidrv_write_max30123_reg_action(MAX30123_REG_FIFO_CONFIG2,
                                     FIFO_CFG2_FLUSH_FIFO |
                                     FIFO_CFG2_FIFO_STAT_CLR);

	if(max30123_reg_convert_and_status() == 0)
		return 0;

	fifo_count = max30123_get_fifo_count();
	while (fifo_count > 0u) {
		max30123_read_fifo_data(&max30123_fifo);
		if(max30123_fifo.data_type == FIFO_TYPE_WE1_PSTAT_CURR) {
			signed_raw = (int16_t)max30123_fifo.adc_val;
//			nA = 256.0f * ((float)signed_raw / 327681);
			nA = ((float)signed_raw / 128.0f);

			afe_102->we1_current = nA;
			afe_102->we2_current = nA;


#if _EIS_FIFO_INFO_DEBUG_
	swmLogInfo("WE1 : %d, "LOG_FLOAT_MARKER" nA  \r\n ", signed_raw, LOG_FLOAT(nA));
	max30123_delay_ms(1);
#endif
    	}

    	fifo_count--;
	}

    return 1;
}



/***********************************************************************************************************************
 *	MAX30123 measure DC current... 수동 모두
 ***********************************************************************************************************************/
uint8_t max30123_measure_DC_auto_mode(void)
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
		while (fifo_count > 0u) {
			max30123_read_fifo_data(&max30123_fifo);
			if(max30123_fifo.data_type == FIFO_TYPE_WE1_PSTAT_CURR) {
				signed_raw = (int16_t)max30123_fifo.adc_val;
	//			nA = 256.0f * ((float)signed_raw / 327681);
				nA = ((float)signed_raw / 128.0f);

				afe_102->we1_current = nA;
				afe_102->we2_current = nA;


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


/************************************************************************************
 *	Chrono A ADC 코드 → nA 변환
 *	I_CONV_TYPE = Signed, FS = 1024nA, 16-bit
 *	I_nA = 1024 × (int16_t)raw / 32768
 ************************************************************************************/
static inline float ca_code_to_nA(uint16_t raw)
{
    return CA_FS_NA * ((float)(int16_t)raw / 32768.0f);
}


/************************************************************************************
 *  max30123 chronoamperometry measurement
 ************************************************************************************/
uint8_t max30123_chrono_A_measurement_auto_mode(chrono_a_raw_t *out)
{
	uint8_t status = 0;
	uint16_t fifo_count;
	float	nA;

#if _EIS_FIFO_INFO_DEBUG_
    swmLogInfo("max30123_chrono_A_measurement\r\n");
#endif

	status = spidrv_read_max30123_reg_action(MAX30123_REG_STATUS1);
    swmLogInfo("chrono_A : %02x\r\n", status);

	if (status & STATUS1_ADC_DATA_RDY) {

	    // 2. FIFO 엔트리 수 확인
	    fifo_count = max30123_get_fifo_count();

#if _EIS_FIFO_INFO_DEBUG_
	    swmLogInfo("[CA] FIFO count=%d\r\n", fifo_count);
#endif

	    if (fifo_count == 0u) {					// FIFO가 없거나 너무 많은 숫자가 들어 오면

#if _EIS_FIFO_INFO_DEBUG_
	        swmLogInfo("[CA] ERR: FIFO empty\r\n");
#endif
	        return 0u;
	    }

	    out->n_step = out->n_post = 0;

	    // 3. FIFO 읽기 루프
	    while(fifo_count > 0) {
	        max30123_read_fifo_data(&max30123_fifo);

	        if (max30123_fifo.meas_id != CA_M1_ID) {		// M1(Chrono A) 데이터만 처리. M0 PSTAT 값은 SKIP
	            continue;
	        }

	        nA = ca_code_to_nA(max30123_fifo.adc_val);

	        switch(max30123_fifo.data_type) {
	        	case FIFO_TYPE_WE1_PSTAT_CURR:
					afe_102->we1_current = nA;
					afe_102->we2_current = nA;
	        		break;

	        	case FIFO_TYPE_WE1_PRE:
					break;

				case FIFO_TYPE_WE1_STEP:
		            if (out->n_step < CA_TOTAL_STEP) {
		                out->step_nA[out->n_step++] = nA;
		            }

	#if _EIS_FIFO_INFO_DEBUG_
					swmLogInfo("[STEP]  raw=0x%04X  " LOG_FLOAT_MARKER " nA\r\n", max30123_fifo.adc_val, LOG_FLOAT(nA));
					max30123_delay_ms(1);
	#endif
					break;

				case FIFO_TYPE_WE1_REC:
		            if (out->n_post < CA_TOTAL_POST) {
		                out->post_nA[out->n_post++] = nA;
		            }

	#if _EIS_FIFO_INFO_DEBUG_
					swmLogInfo("[POST]  raw=0x%04X  " LOG_FLOAT_MARKER " nA\r\n", max30123_fifo.adc_val,  LOG_FLOAT(nA));
					max30123_delay_ms(1);
	#endif
					break;

				default:
					break;
	        }

	#if _EIS_FIFO_INFO_DEBUG_
	        swmLogInfo("FIFO : [%d]--> 0x%02X, %d\r\n", fifo_count, max30123_fifo.data_type, (int16_t)max30123_fifo.adc_val);
	        max30123_delay_ms(1);
	#endif
	    	fifo_count--;
	    }

	    if ((out->n_step == 0u) || (out->n_post == 0u)) {
	#if _EIS_FIFO_INFO_DEBUG_
	        swmLogInfo("[CA] ERR: 샘플 부족\r\n");
	#endif
	        return 0u;
	    }
	#if _EIS_FIFO_INFO_DEBUG_
	        swmLogInfo("max30123_chrono_A_measurement success\r\n");
	        max30123_delay_ms(1);
	#endif

		return 1;

	}

	return 0;

}

/************************************************************************************
 *  max30123 chronoamperometry measurement
 ************************************************************************************/
uint8_t max30123_chrono_A_measurement(chrono_a_raw_t *out)
{
	uint16_t fifo_count;
	float	nA;

    //	1. FIFO flush
    spidrv_write_max30123_reg_action(MAX30123_REG_FIFO_CONFIG2,
                                     FIFO_CFG2_FLUSH_FIFO |
                                     FIFO_CFG2_FIFO_STAT_CLR);

#if _EIS_FIFO_INFO_DEBUG_
    swmLogInfo("max30123_chrono_A_measurement\r\n");
#endif

	if(max30123_reg_convert_and_status() == 0)
		return 0;

    // 2. FIFO 엔트리 수 확인
    fifo_count = max30123_get_fifo_count();
#if _EIS_FIFO_INFO_DEBUG_
    swmLogInfo("[CA] FIFO count=%d\r\n", fifo_count);
#endif
    if (fifo_count == 0u) {
#if _EIS_FIFO_INFO_DEBUG_
        swmLogInfo("[CA] ERR: FIFO empty\r\n");
#endif
        return 0u;
    }

    out->n_step = out->n_post = 0;

    // 3. FIFO 읽기 루프
    while(fifo_count > 0) {
        max30123_read_fifo_data(&max30123_fifo);

        if (max30123_fifo.meas_id != CA_M1_ID) {		// M1(Chrono A) 데이터만 처리. M0 PSTAT 값은 SKIP
            continue;
        }

        nA = ca_code_to_nA(max30123_fifo.adc_val);

        switch(max30123_fifo.data_type) {
        	case FIFO_TYPE_WE1_PRE:
				break;

			case FIFO_TYPE_WE1_STEP:
	            if (out->n_step < CA_TOTAL_STEP) {
	                out->step_nA[out->n_step++] = nA;
	            }

#if _EIS_FIFO_INFO_DEBUG_
				swmLogInfo("[STEP]  raw=0x%04X  " LOG_FLOAT_MARKER " nA\r\n", max30123_fifo.adc_val, LOG_FLOAT(nA));
				max30123_delay_ms(1);
#endif
				break;

			case FIFO_TYPE_WE1_REC:
	            if (out->n_post < CA_TOTAL_POST) {
	                out->post_nA[out->n_post++] = nA;
	            }

#if _EIS_FIFO_INFO_DEBUG_
				swmLogInfo("[POST]  raw=0x%04X  " LOG_FLOAT_MARKER " nA\r\n", max30123_fifo.adc_val,  LOG_FLOAT(nA));
				max30123_delay_ms(1);
#endif
				break;

			default:
				break;
        }

#if _EIS_FIFO_INFO_DEBUG_
        swmLogInfo("FIFO : [%d]--> 0x%02X, %d\r\n", fifo_count, max30123_fifo.data_type, (int16_t)max30123_fifo.adc_val);
        max30123_delay_ms(1);
#endif
    	fifo_count--;
    }

    if ((out->n_step == 0u) || (out->n_post == 0u)) {
#if _EIS_FIFO_INFO_DEBUG_
        swmLogInfo("[CA] ERR: 샘플 부족\r\n");
#endif
        return 0u;
    }
#if _EIS_FIFO_INFO_DEBUG_
        swmLogInfo("max30123_chrono_A_measurement success\r\n");
        max30123_delay_ms(1);
#endif

	return 1;

}

/*******************************************************************
 *  max30123_impedance_measurement
 *
 *  Chrono A STEP + POST 샘플로 10Hz DFT 수행 → Z 계산
 *
 *  전압 파형 정의:
 *    k = 0 ~ n_step-1 : V = +CA_VSTEP_MV (+30mV)
 *    k = n_step ~ N-1 : V =  CA_VPOST_MV  (0mV, baseline 복귀)
 *
 *  DFT (e^(-j2π×f×k×ISP)):
 *    X_real = Σ x[k] × cos(2π × f × k × ISP)
 *    X_imag = Σ x[k] × sin(2π × f × k × ISP)   (부호 주의: -j → -sin)
 *
 *  Z 계산 (복소수 나눗셈):
 *    Z = V_DFT / I_DFT
 *    Z_real = (V_re×I_re + V_im×I_im) / |I|²  × 1e6  [Ω]
 *    Z_imag = (V_im×I_re - V_re×I_im) / |I|²  × 1e6  [Ω]
 *    (×1e6: mV/nA → V/A = Ω)
 ********************************************************************/

uint8_t max30123_impedance_calcualtion(const chrono_a_raw_t *in, impedance_result_t *out)
{
    float   v_re = 0.0f, v_im = 0.0f;
    float   i_re = 0.0f, i_im = 0.0f;
    float   i_mag_sq;
    float   phase;
    float   v_k, i_k;
    float   FREQ_HZ, ISP_SEC;

    uint8_t k;
    uint8_t in_step;
    uint8_t cycle, step_pos, post_k, post_pos;
    uint32_t t_idx;  										/* 절대 시간 인덱스 */

    uint8_t n_per_cycle = CA_STEP_COUNT + CA_POST_COUNT;  	/* 11 */
    uint8_t n_total     = in->n_step + in->n_post;        	/* 44 */

    double z_real, z_imag, z_mag, z_phase_deg;

    if (out == NULL || n_total == 0u) {
        return 0u;
    }

    memset(out, 0, sizeof(*out));

    for (k = 0u; k < n_total; k++) {

        in_step = (k < in->n_step);   									// k<32: STEP,  k>=32: POST

        v_k = in_step ? CA_VSTEP_MV : CA_VPOST_MV;						// 전압
        i_k = in_step ? in->step_nA[k] : in->post_nA[k - in->n_step];	// 전류

        if (in_step) {
            cycle    = k / CA_STEP_COUNT;        	 					// 몇 번째 사이클
            step_pos = k % CA_STEP_COUNT;         						// 사이클 내 위치
            t_idx    = (uint32_t)cycle * n_per_cycle + step_pos;

        } else {
            post_k   = k - in->n_step;
            cycle    = post_k / CA_POST_COUNT;
            post_pos = post_k % CA_POST_COUNT;
            t_idx    = (uint32_t)cycle * n_per_cycle + CA_STEP_COUNT + post_pos;
        }

#if 0
        phase = 2.0f * M_PI * CA_FREQ_HZ * (float)t_idx * CA_ISP_SEC;
#else
        FREQ_HZ = chrono_setup[max30123_chrono_A_count].freq_hz;
        ISP_SEC = chrono_setup[max30123_chrono_A_count].isp_sec;
        phase = 2.0f * M_PI * FREQ_HZ * (float)t_idx * ISP_SEC;
#endif

#if 0 //#if _EIS_FIFO_INFO_DEBUG_
		swmLogInfo("Freq = " LOG_FLOAT_MARKER ",  isp sec = " LOG_FLOAT_MARKER "\r\n",
					LOG_FLOAT(FREQ_HZ), LOG_FLOAT(ISP_SEC));
		max30123_delay_ms(1);
#endif

        v_re += v_k * cosf(phase);
        v_im -= v_k * sinf(phase);

        i_re += i_k * cosf(phase);
        i_im -= i_k * sinf(phase);

#if _IMPEDANCE_INFO_DEBUG_
        swmLogInfo("[DFT] k=%2d  V=" LOG_FLOAT_MARKER " mV  I=" LOG_FLOAT_MARKER " nA\r\n", k, LOG_FLOAT(v_k), LOG_FLOAT(i_k));
        max30123_delay_ms(1);
#endif
    }

#if _IMPEDANCE_INFO_DEBUG_
    swmLogInfo("[DFT] V_re=" LOG_FLOAT_MARKER "  V_im=" LOG_FLOAT_MARKER " mV\r\n", LOG_FLOAT(v_re), LOG_FLOAT(v_im));
    swmLogInfo("[DFT] I_re=" LOG_FLOAT_MARKER "  I_im=" LOG_FLOAT_MARKER " nA\r\n", LOG_FLOAT(i_re), LOG_FLOAT(i_im));
    max30123_delay_ms(1);
#endif

    i_mag_sq = (i_re * i_re) + (i_im * i_im);
    if (i_mag_sq < 1e-6f) {
#if _IMPEDANCE_INFO_DEBUG_
        swmLogInfo("[EIS] ERR: |I|≈0\r\n");
#endif
        return 0u;
    }

    z_real      = (v_re * i_re + v_im * i_im) / i_mag_sq * 1.0e6f;
    z_imag      = (v_im * i_re - v_re * i_im) / i_mag_sq * 1.0e6f;
    z_mag       = sqrtf(z_real * z_real + z_imag * z_imag);
    z_phase_deg = atan2f(z_imag, z_real) * 57.2957914;//(180.0f / M_PI);

    out->z_real 	= (int32_t)z_real;
    out->z_imag 	= (int32_t)z_imag;
    out->z_mag 		= (int32_t)z_mag;
    out->z_phase_deg= (int32_t)z_phase_deg;


#if _IMPEDANCE_RESULT_DEBUG_
    swmLogInfo("*****************************************\r\n");
    swmLogInfo("[%s] Z_real  = %ld Ohm\r\n", (char *)chrono_name[max30123_chrono_A_count], out->z_real);
    swmLogInfo("[%s] Z_imag  = %ld Ohm\r\n", (char *)chrono_name[max30123_chrono_A_count], out->z_imag);
    swmLogInfo("[%s] |Z|     = %ld Ohm\r\n", (char *)chrono_name[max30123_chrono_A_count], out->z_mag);
    swmLogInfo("[%s] Phase   = %ld deg\r\n", (char *)chrono_name[max30123_chrono_A_count], out->z_phase_deg);
    swmLogInfo("*****************************************\r\n\r\n");
#endif

    return 1u;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//


/*****************************************************************************
 *  max30123_M1_chrono_A Enable & Disable
 *****************************************************************************/
void max30123_M1_chrono_A_mode(int8_t state)
{
	uint8_t reg_value = 0;

	reg_value = spidrv_read_max30123_reg_action(MAX30123_REG_M1_CFG);
	reg_value&= 0x1F;

	if(state==M1_CHRONO_A_ENABLE)
		reg_value|=0x40;

	spidrv_write_max30123_reg_action(MAX30123_REG_M1_CFG, reg_value);   			// Chrono A measurement enable
}

/*****************************************************************************
 *  max30123 WE Config
 *****************************************************************************/
void max30123_WE1_config_power_on(void)
{
	uint8_t regVal;

	/* FIFO Flush */
    spidrv_write_max30123_reg_action(MAX30123_REG_FIFO_WR_PTR, 0x00);
    spidrv_write_max30123_reg_action(MAX30123_REG_FIFO_RD_PTR, 0x00);

    regVal = spidrv_read_max30123_reg_action(MAX30123_REG_WE1_CFG1);
#if _EIS_FIFO_INFO_DEBUG_
    swmLogInfo("WE1_CFG1 : 0x%02X\r\n", regVal);
#endif

    regVal = spidrv_read_max30123_reg_action(MAX30123_REG_WE1_CFG2);
#if _EIS_FIFO_INFO_DEBUG_
    swmLogInfo("WE1_CFG2 : 0x%02X\r\n", regVal);
#endif

    regVal = spidrv_read_max30123_reg_action(MAX30123_REG_CE1_CFG);
#if _EIS_FIFO_INFO_DEBUG_
    swmLogInfo("CE1_CFG1 : 0x%02X\r\n", regVal);
#endif

    /* 레지스터 복원 */
	spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG1, 0x82);	// WE1 CONFIG 1: Working Amp Enable
    spidrv_write_max30123_reg_action(MAX30123_REG_WE1_CFG2, 0xEA);	// WE1 CONFIG 2: Offset Always On = ON
    spidrv_write_max30123_reg_action(MAX30123_REG_CE1_CFG, 0x91);	// CE1 CONFIG: Counter Amp Enable
}


///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
//
//	max30123
//

/**************************************************************************************
 * MAX30123 단계별 진행 상태..
 **************************************************************************************/
void max30123_measure_Process(void)
{
	if(read_custom_irq()!=1)
		return;

	clear_custom_irq();

	switch(max30123_measure_step) {
		case MAX30123_STEP_CALIBRATION:
			if(max30123_measure_calibration(&cal)) {					// 보정이 되어야 다음 단계로 넘어 간다...
				max30123_measure_step = MAX30123_STEP_DC_CURRENT;
				max30123_reg_convert_mode(CONVERT_MODE_AUTO, CONVERT_MODE_CONVERT);
			}

#if _EIS_FIFO_INFO_DEBUG_
			swmLogInfo("calibration : [%d] \r\n", cal);
			max30123_delay_ms(1);
#endif
			break;

		case MAX30123_STEP_DC_CURRENT:
			if(max30123_measure_DC_auto_mode()) {
				if(rsl15_info->date_time_update == DATE_TIME_UPDATE_SUCCESS) {	// 시간 초기화 이후
					if(max30123_chrono_A_wait_count>0)	max30123_chrono_A_wait_count--;

					if(max30123_chrono_A_wait_count==0) {						// chrono_A 측정 이후 FIFO 안정화를 위해 6번 데이터를 버린다...
//						LSAD_measure_sensor_level();
						append_lsad_lvl_payload();									// add_Queue Buffer
					}

					app_custom_send_payload_packet();
				}
			}
			break;

		case MAX30123_STEP_IMPEDANCE:
			max30123_measure_ChronoAmperometry();
			break;

		default:
			break;
	}


}

/***********************************************************************************************************************
 *  MAX30123 CA (ChronoAmperometry)
 ***********************************************************************************************************************/

#if 1
void max30123_measure_ChronoAmperometry(void)
{
    chrono_a_raw_t 	raw;

	switch(max30123_chronoamperometry) {
		case 0:
			max30123_reg_convert_mode(CONVERT_MODE_AUTO, CONVERT_MODE_STOP);
		    max30123_delay_ms(2);
		    max30123_M1_chrono_A_mode(M1_CHRONO_A_DISABLE);

			Call_Custom_NTF_Timeout(1);											// call CUSTOM NTF TIMEOUT 1sec
			max30123_chrono_A_count = 0u;
            max30123_chronoamperometry = 1u;
			break;

		case 1:
		    max30123_M1_chrono_A_Freq(max30123_chrono_A_count);					// 주파수 변경...
		    max30123_delay_ms(2);

		    max30123_M1_chrono_A_mode(M1_CHRONO_A_ENABLE);						// Chrono_A
		    max30123_delay_ms(2);

			max30123_reg_convert_mode(CONVERT_MODE_AUTO, CONVERT_MODE_CONVERT);
            max30123_chronoamperometry = 2u;
			Call_Custom_NTF_Timeout(3);											// call CUSTOM NTF TIMEOUT 3sec
			break;

		case 2:
			if(max30123_chrono_A_measurement_auto_mode(&raw)) {
				max30123_impedance_calcualtion(&raw, &impedance_result[max30123_chrono_A_count]);		// impedance result
				read_calendar(&impedance_calendar);								// impedance calendar 정보

				max30123_reg_convert_mode(CONVERT_MODE_AUTO, CONVERT_MODE_STOP);
				Call_Custom_NTF_Timeout(2);										// call CUSTOM NTF TIMEOUT 5sec
				max30123_M1_chrono_A_mode(M1_CHRONO_A_DISABLE);					// Sequencer M1 Disable

				max30123_chrono_A_count++;
				if(max30123_chrono_A_count>=CHRONO_FREQ_COUNT) {
					max30123_chrono_A_count = 0u;
					max30123_chronoamperometry = 3u;
				}

				else {
					max30123_chronoamperometry = 1u;
				}

				Call_Custom_NTF_Timeout(1);											// call CUSTOM NTF TIMEOUT 1sec
			}

			else {
				Call_Custom_NTF_Timeout(3);											// call CUSTOM NTF TIMEOUT5sec
			}

			break;

		case 3:																		//
			max30123_reg_convert_mode(CONVERT_MODE_AUTO, CONVERT_MODE_CONVERT);
			max30123_chrono_A_wait_count = 3;										// 3개 데이터를 버리고 난 뒤 부터 데이터를 가져 오도록 한다..

            max30123_chronoamperometry = 0u;
            max30123_measure_step = MAX30123_STEP_DC_CURRENT;
			break;

		default:
            max30123_chronoamperometry = 0u;
			break;
	}
}
#else
void max30123_measure_ChronoAmperometry(void)
{
    chrono_a_raw_t 	raw;

	switch(max30123_chronoamperometry) {
		case 0:
			max30123_reg_convert_mode(CONVERT_MODE_MANUAL, CONVERT_MODE_STOP);
			max30123_M1_chrono_A_mode(M1_CHRONO_A_ENABLE);

			Call_Custom_NTF_Timeout(1);											// call CUSTOM NTF TIMEOUT 1sec
            max30123_chronoamperometry = 1u;
			break;

		case 1:
			max30123_reg_convert_mode(CONVERT_MODE_AUTO, CONVERT_MODE_CONVERT);
            max30123_chronoamperometry = 2u;
			Call_Custom_NTF_Timeout(5);											// call CUSTOM NTF TIMEOUT 5sec
			break;

		case 2:
			if(max30123_chrono_A_measurement_auto_mode(&raw)) {
				max30123_impedance_calcualtion(&raw, &impedance_result);		// impedance result
				read_calendar(&impedance_calendar);								// impedance calendar 정보

				max30123_reg_convert_mode(CONVERT_MODE_AUTO, CONVERT_MODE_STOP);
				max30123_M1_chrono_A_mode(M1_CHRONO_A_DISABLE);					// Sequencer M1 Disable

				max30123_chronoamperometry = 3u;
				Call_Custom_NTF_Timeout(1);											// call CUSTOM NTF TIMEOUT 1sec
			}

			else {
				Call_Custom_NTF_Timeout(3);											// call CUSTOM NTF TIMEOUT5sec
			}

			break;

		case 3:																		//
			max30123_reg_convert_mode(CONVERT_MODE_AUTO, CONVERT_MODE_CONVERT);
			max30123_chrono_A_wait_count = 3;										// 3개 데이터를 버리고 난 뒤 부터 데이터를 가져 오도록 한다..

            max30123_chronoamperometry = 0u;
            max30123_measure_step = MAX30123_STEP_DC_CURRENT;
			break;

		default:
            max30123_chronoamperometry = 0u;
			break;
	}
}
#endif

/************************************************************************************************************
 * CHRONOAMPEROMETRY FREQUENCE
 ************************************************************************************************************/
void max30123_M1_chrono_A_Freq(Chron_Freq_e idx)
{
    uint32_t div;

    if (idx >= CHRONO_FREQ_COUNT)
        return;

    div = chrono_setup[idx].chrono_clk_div & 0x00FFFFFFu;

    /* CHRONO_CLK_DIV [15:8] / [7:0] */
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_CLK_DIV_MSB, (uint8_t)((div >> 8) & 0xFFu));
    spidrv_write_max30123_reg_action(MAX30123_REG_CHRONO_CLK_DIV_LSB, (uint8_t)( div       & 0xFFu));

    if(idx == CHRONO_FREQ_25HZ)
    	spidrv_write_max30123_reg_action(MAX30123_REG_M1_CONV, 0xD7);		// ConV Time is 1.38ms
    else
    	spidrv_write_max30123_reg_action(MAX30123_REG_M1_CONV, 0xD6);		// ConV Time is 2.76ms


#if _EIS_FIFO_INFO_DEBUG_
    swmLogInfo("[Freq]  idx :%d,  div= (0x%06lX)\r\n",  idx, (unsigned long)div);
#endif

}
