/**
 * @file  app_msg_handler.c
 * @brief Message Handler source file
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

#include <stdio.h>
#include <app.h>
#include "app_customss.h"
/* Application headers */
#include "app_temperature_sensor.h"

//#define APP_DEV_VER                  "V320R"
#define APP_DEV_NAME                 "AGMS_I10-2_%s_%02X%02X"


#if 0
#define PARAM_INTV_MIN	3200		// * 1.25ms...4.000sec
#define PARAM_INTV_MAX	3200		// * 1.25ms...4.000sec
#define PARAM_LATENCY	2			//  connSupervisionTimeout / (connIntervalMax * 2))-1)
#define PARAM_TIMEOUT	2500		// ((2450*10)/(4000*2))-1 = 2
#else
#define PARAM_INTV_MIN	MIN_CONNECTION_INTERVAL		// * 1.25ms...2.000sec
#define PARAM_INTV_MAX	MAX_CONNECTION_INTERVAL		// * 1.25ms...2.000sec
#define PARAM_LATENCY	SLAVE_LATENCY			//  connSupervisionTimeout / (connIntervalMax * 2))-1)
#define PARAM_TIMEOUT	CONNECTION_SUPERVISION_TIMEOUT		// ((2000*10)/(2000*2))-1 = 4
#endif

typedef void (*generic_func_pointer)(void);
static generic_func_pointer service_init_queue[APP_MAX_PROF_SVC_COUNT] = {0};

#ifndef GATT_MAX_MTU_SIZE
    #define GATT_MAX_MTU_SIZE     517
#endif


static uint8_t	ParamUpdateCmd=0;			// 2024.05.10
static uint8_t	customss_Notify_Time=0;		// 2024.05.10

volatile uint32_t adv_start_reset_time = 0;
volatile uint32_t param_update_time = 0;


uint8_t device_name[APP_DEVICE_NAME_LEN + 1];
char APP_DEV_VER_BUFF[8];


uint8_t app_adv_data[ADV_DATA_LEN], app_scan_rsp_data[ADV_DATA_LEN];
uint8_t app_adv_data_len, app_scan_rsp_data_len;

cust_svc_desc app_cust_svc_db[APP_NUM_CUST_SVC];
uint16_t app_disc_svc_count[APP_MAX_NB_CON];

GAPM_ActivityStatus_t advActivityStatus;

//connection_interval_time_t	connection_interval_time;


int con_param_update_attempt=0;

extern conn_param_info_t conn_param_info[APP_MAX_NB_CON];

struct gapm_set_dev_config_cmd devConfigCmd =
{
    .operation = GAPM_SET_DEV_CONFIG,
    .role = GAP_ROLE_ALL,
    .renew_dur = APP_BD_RENEW_DUR,
    .addr.addr = APP_BLE_PRIVATE_ADDR,
    .irk.key = APP_IRK,
#if SECURE_CONNECTION
    .pairing_mode = (GAPM_PAIRING_SEC_CON | GAPM_PAIRING_LEGACY),
#else
    .pairing_mode = GAPM_PAIRING_LEGACY,
#endif
    .privacy_cfg = (GAPM_PRIVACY_TYPE | GAPM_ADDRESS_TYPE),
    .gap_start_hdl = GAPM_DEFAULT_GAP_START_HDL,
    .gatt_start_hdl = GAPM_DEFAULT_GATT_START_HDL,
    .att_cfg = GAPM_DEFAULT_ATT_CFG,
    .sugg_max_tx_octets = GAPM_DEFAULT_TX_OCT_MAX,
    .sugg_max_tx_time = GAPM_DEFAULT_TX_TIME_MAX,
    .max_mtu = GAPM_DEFAULT_MTU_MAX,
    .max_mps = GAPM_DEFAULT_MPS_MAX,
    .max_nb_lecb = GAPM_DEFAULT_MAX_NB_LECB,
    .audio_cfg = GAPM_DEFAULT_AUDIO_CFG,
    .tx_pref_phy = GAP_PHY_ANY,
    .rx_pref_phy = GAP_PHY_ANY
};

struct gapm_adv_create_param advParam =
{
    .type = GAPM_ADV_TYPE_LEGACY,
    .disc_mode = APP_ADV_DISCOVERY_MODE,
    .prop = APP_ADV_PROPERTIES,
    .filter_pol = ADV_ALLOW_SCAN_ANY_CON_ANY,
    .max_tx_pwr = DEF_TX_POWER,
    .prim_cfg = {
        .adv_intv_min = APP_ADV_INT_MIN,
        .adv_intv_max = APP_ADV_INT_MAX,
        .chnl_map = APP_ADV_CHMAP,
        .phy = GAPM_PHY_TYPE_LE_1M,
     },
};

union gapc_bond_cfm_data pairingRsp =
{
    .pairing_feat =
    {
        .iocap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT,
        .oob = GAP_OOB_AUTH_DATA_NOT_PRESENT,
        .key_size = KEY_LEN,
        .ikey_dist = (GAP_KDIST_IDKEY | GAP_KDIST_SIGNKEY),
        .rkey_dist = (GAP_KDIST_ENCKEY | GAP_KDIST_IDKEY | GAP_KDIST_SIGNKEY),
        .auth = GAP_AUTH_REQ_SEC_CON_BOND,
        .sec_req = GAP_SEC1_NOAUTH_PAIR_ENC,
    }
};

struct gap_dev_name_buff
{
    uint16_t length;                       /**< name length */
    uint8_t value[APP_DEVICE_NAME_LEN];    /**< name value */
};

const struct gap_dev_name_buff getDevInfoCfmName =
{
    .length = APP_DEVICE_NAME_LEN,
    .value  = APP_DEVICE_NAME
};


/****************************************************************************************
 * CONNECTION INTERVAL TIME...default setting
 ****************************************************************************************/
struct connection_interval_time conn_interval_time = {
	.intv_min = MIN_CONNECTION_INTERVAL,
	.intv_max = MAX_CONNECTION_INTERVAL,
	.latency = SLAVE_LATENCY,
	.sv_tmout = CONNECTION_SUPERVISION_TIMEOUT
};


const union gapc_dev_info_val getDevInfoCfmAppearance =
{
    .appearance = APP_DEVICE_APPEARANCE
};

const union gapc_dev_info_val getDevInfoCfmSlvParams =
{
    .slv_pref_params = {APP_PREF_SLV_MIN_CON_INTERVAL,
                   APP_PREF_SLV_MAX_CON_INTERVAL,
                   APP_PREF_SLV_LATENCY,
                   APP_PREF_SLV_SUP_TIMEOUT}
};

const union gapc_dev_info_val* getDevInfoCfm[] =
{
    [GAPC_DEV_NAME] = (const union gapc_dev_info_val*) &getDevInfoCfmName,
    [GAPC_DEV_APPEARANCE] = &getDevInfoCfmAppearance,
    [GAPC_DEV_SLV_PREF_PARAMS] = &getDevInfoCfmSlvParams
};





static void SetConnectionCfmParams(uint8_t conidx, struct gapc_connection_cfm* cfm);

static uint8_t total_service_index = 0;
static void App_BLE_CUSSAddToDatabaseCS0(void)
{
    GATTM_AddAttributeDatabase(CUSTOMSS_GetDatabaseDescription(CUST_SVC0), CS0_NB);
}

#if CEM102_DIAG_FUNCTIONS
static void App_BLE_CUSSAddToDatabaseCS1(void)
{
    GATTM_AddAttributeDatabase(CUSTOMSS_GetDatabaseDescription(CUST_SVC1), CS1_NB);
}
#endif /* CEM102_DIAG_FUNCTIONS */

static void App_BLE_ServiceInitQueue(void)
{
    service_init_queue[APP_STD_PRF_INDEX] = (generic_func_pointer)BASS_ProfileTaskAddCmd;
    service_init_queue[APP_CUS_SVC0_INDEX] = (generic_func_pointer)App_BLE_CUSSAddToDatabaseCS0;

#if CEM102_DIAG_FUNCTIONS
    service_init_queue[APP_CUS_SVC1_INDEX] = (generic_func_pointer)
                                             App_BLE_CUSSAddToDatabaseCS1;
#endif /* CEM102_DIAG_FUNCTIONS */
}

static void App_BLE_AddNextService(void)
{
    static bool svc_initialized = false;

    if (GAPM_GetProfileAddedCount() < APP_NUM_STD_PRF ||
        GATTM_GetServiceAddedCount() < APP_NUM_CUST_SVC)
    {
        if (!svc_initialized)
        {
            App_BLE_ServiceInitQueue();
            svc_initialized = true;
        }

        if (total_service_index < APP_MAX_PROF_SVC_COUNT)
        {
#ifdef SWMTRACE_OUTPUT
            switch(total_service_index)
            {
                case APP_STD_PRF_INDEX:
                    swmLogInfo("Adding BASS profile\r\n");
                    break;
#if CEM102_DIAG_FUNCTIONS
                case APP_CUS_SVC1_INDEX:
                    swmLogInfo("Adding CUSSAddToDatabaseCS1\r\n");
                    break;
#endif /* CEM102_DIAG_FUNCTIONS */
                default: /* APP_CUS_SVC0_INDEX */
                    swmLogInfo("Adding CUSSAddToDatabaseCS0\r\n");
                    break;
            }
#endif
            service_init_queue[total_service_index]();
            total_service_index++;
        }
    }
}

void BLE_ConfigHandler(ke_msg_id_t const msg_id, void const *param,
                            ke_task_id_t const dest_id,
                            ke_task_id_t const src_id)
{
    switch (msg_id)
    {
        case GAPM_CMP_EVT:
        {
            const struct gapm_cmp_evt *p = param;

            if (p->operation == GAPM_RESET) /* Step 2 */
            {
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("__GAPM_RESET completed. Setting BLE device configuration...\r\n");
#endif

                /* Check privacy_cfg bit 0 to identify address type, public if not set */
                if (devConfigCmd.privacy_cfg & GAPM_CFG_ADDR_PRIVATE)
                {
#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("    devConfigCmd address to set static private random\r\n");
#endif
                }
                else
                {
                    /* Read Device BLE Public Address */
                    uint8_t ble_dev_addr_len = GAP_BD_ADDR_LEN;
                    uint8_t ble_dev_addr_buf[GAP_BD_ADDR_LEN] = {0};

                    uint8_t ble_dev_addr_code;

                    /* Make sure proper BLE public address has been read and saved into ble_public_addr
                     * using Device_BLE_Public_Address_Read() before calling Device_BLE_Param_Get() */
                    Device_BLE_Param_Get(PARAM_ID_BD_ADDRESS, &ble_dev_addr_len, ble_dev_addr_buf);

                    ble_dev_addr_code = ble_dev_addr_buf[1] + ble_dev_addr_buf[0];
                    if(ble_dev_addr_code>0xFF)
                    	rsl15_info->device_code =  (uint8_t)(ble_dev_addr_code-0xFF);
                    else
                    	rsl15_info->device_code =  (uint8_t)(ble_dev_addr_code);


#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("    Device BLE public address read: ");
#endif
                    for (int i = 0; i < GAP_BD_ADDR_LEN; i++)
                    {
#ifdef SWMTRACE_OUTPUT
                        swmLogInfo("0x%02x ", ble_dev_addr_buf[i]);
#endif
                    }
#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("\r\n");
                    swmLogInfo("    devConfigCmd address set to public : [%d]\r\n", rsl15_info->device_code);
#endif
                    memcpy(devConfigCmd.addr.addr, ble_dev_addr_buf, GAP_BD_ADDR_LEN);


                    // add sodykim
                    memset(device_name, 0, APP_DEVICE_NAME_LEN+1);
//                	snprintf((char *)device_name, APP_DEVICE_NAME_LEN + 1, APP_DEV_NAME, APP_DEV_VER, ble_dev_addr_buf[1], ble_dev_addr_buf[0]);
                	snprintf((char *)device_name, APP_DEVICE_NAME_LEN + 1, APP_DEV_NAME, APP_DEV_VER_BUFF, ble_dev_addr_buf[1], ble_dev_addr_buf[0]);



                }

                PrepareAdvScanData();			// add sodykim for advertising

                /* Send a device configuration request to the BLE stack.
                 * The stack sends back a GAPM_CMP_EVT / GAPM_SET_DEV_CONFIG upon completion. */
                GAPM_SetDevConfigCmd(&devConfigCmd);

                /* Set discSvcCount, custom service database and maximum number of custom services
                 * in GATT environment at GAPM_RESET */
                GATT_SetEnvData(app_disc_svc_count, app_cust_svc_db, APP_NUM_CUST_SVC);
            }
            else if (p->operation == GAPM_SET_DEV_CONFIG &&
                     p->status == GAP_ERR_NO_ERROR) /* Step 3 - Add BASS profile */
            {
                App_BLE_AddNextService();
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("__GAPM_SET_DEV_CONFIG completed.\r\n");
                swmLogInfo("    Adding BLE BASS profile...\r\n");
#endif
            }
        }
        break;

        case GAPM_PROFILE_ADDED_IND: /* Step 4 - Add CUST Service */
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("__GAPM_PROFILE_ADDED_IND - profile added count=%d\r\n",
                       GAPM_GetProfileAddedCount());

            swmLogInfo("    BLE profile BASS added successfully...\r\n");
#endif
            /* Request the stack to add our custom service server to the attribute database.
             * The stack sends back a GATTM_ADD_SVC_RSP event. */
            App_BLE_AddNextService();
        }
        break;

        case GATTM_ADD_SVC_RSP: /* Step 5 - Custom service added */
        {
            if (GATTM_GetServiceAddedCount() < APP_NUM_CUST_SVC)
            {
                App_BLE_AddNextService();
            }
            else
            {
#ifdef SWMTRACE_OUTPUT
                /* Request the stack to create an advertising activity.
                 * The stack sends back a GAPM_ACTIVITY_CREATED_IND. See ActivityHandler for next steps. */
                swmLogInfo("    Creating Advertising activity...\r\n");
#endif
                GAPM_ActivityCreateAdvCmd(&advActivityStatus, GAPM_OWN_ADDR_TYPE, &advParam);
            }
        }
        break;
    }
}

void BLE_ActivityHandler(ke_msg_id_t const msg_id, void const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    switch (msg_id)
    {
        case GAPM_CMP_EVT:
        {
            const struct gapm_cmp_evt *p = param;
            if (p->operation == GAPM_SET_ADV_DATA) /* Step 7 */
            {
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("__GAPM_SET_ADV_DATA status = %d. Start advertising activity...\r\n",p->status);
#endif
                GAPM_AdvActivityStart(advActivityStatus.actv_idx, 0, 0);

                /* From now on, this device is advertising. Any peer device can
                 * connect, discover services, pair/bond/encrypt, etc.
                 * When a peer device tries to connect, the stack sends back to the application
                 * a GAPC_CONNECTION_REQ_IND. See ConnectionHandler for more details. */
            }
        }
        break;

        case GAPM_ACTIVITY_CREATED_IND: /* Step 6 */
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("__GAPM_ACTIVITY_CREATED_IND actv_idx = %d. Setting adv and scan data...\r\n",
                    advActivityStatus.actv_idx);
#endif
            /* Request the stack to set the advertising and scan response data.
             * The stack sends back a GAPM_CMP_EVT: operation = GAPM_SET_ADV_DATA. */
            GAPM_SetAdvDataCmd(GAPM_SET_SCAN_RSP_DATA, advActivityStatus.actv_idx,
                    app_adv_data_len, app_adv_data);
            GAPM_SetAdvDataCmd(GAPM_SET_ADV_DATA, advActivityStatus.actv_idx,
                    app_adv_data_len, app_adv_data);
        }
        break;

        case GAPM_ACTIVITY_STOPPED_IND: /* Step 9(c) */
        {
            /* The advertising activity is stopped upon receiving
             * connection request. Restart advertising if not connected to
             * maximum number of peers configured for this application */
            if ((GAPC_ConnectionCount() < APP_MAX_NB_CON))
            {
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("__GAPM_ACTIVITY_STOPPED_IND. Restarting advertising...\r\n");
#endif
                if (advActivityStatus.state == ACTIVITY_STATE_NOT_STARTED)
                {
                    GAPM_AdvActivityStart(advActivityStatus.actv_idx, 0, 0);
                }
            }
        }
        break;
    }
}

void BLE_ConnectionHandler(ke_msg_id_t const msg_id, void const *param,
                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint8_t conidx = KE_IDX_GET(src_id);
    switch (msg_id)
    {
        case GAPC_CMP_EVT:
        {
            const struct gapc_cmp_evt *p = param;

            if (p->operation == GAPC_UPDATE_PARAMS)
            {
#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("__GAPC_CMP_EVT: GAP_UPDATE_PARAMS: condix=%d\r\n", conidx);
#endif
                /* A connection parameter update was requested by the device.
                 * Check if the peer device returned an error. */
                if (p->status != GAP_ERR_NO_ERROR)
                {
#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("  Received an error while attempting a connection parameter update.\r\n");
#endif
                }
            }
            break;
        }
        case GAPC_CONNECTION_REQ_IND: /* Step 8 */
        {
            const struct gapc_connection_req_ind *p = param;
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("__GAPC_CONNECTION_REQ_IND conidx=%d\r\n", conidx);
#endif
            /* If the peer device address is private resolvable and bond list is not empty */
            if (GAP_IsAddrPrivateResolvable(p->peer_addr.addr, p->peer_addr_type) &&
                BondList_Size() > 0) /* Step 9(a) */
            {
                /* Ask the stack to resolve the address with the IRKs we have in our bond list.
                 * In case of success, the stack returns GAPM_ADDR_SOLVED_IND.
                 * If not successful (not bonded previously) the stack returns GAPM_CMP_EVT /
                 * GAPM_RESOLV_ADDR with status GAP_ERR_NOT_FOUND (see below). */
                GAPM_ResolvAddrCmd(conidx, p->peer_addr.addr);
            }
            else /* Step 9(b) */
            {
                /* Address is not private resolvable or bondlist empty. Confirms connection right away.
                 * If the device was previously bonded, the LTK is included. */
                APP_SendConCfm(conidx);
            }

#if 0
            /* Update connection parameter information to be used for
             * parameter update request */
            conn_param_info[conidx].con_interval = p->con_interval;
            conn_param_info[conidx].update_attempt = 0;

            /* If Parameter Update has been suspended, change it to BLOCKED state
             * for the scheduler to schedule it. */
            if (Scheduler_Get_TaskState(APP_CONN_PARAM_UDPATE_TASK_NUM) == TASK_SUSPENDED)
            {
                Scheduler_Set_TaskState(APP_CONN_PARAM_UDPATE_TASK_NUM, TASK_BLOCKED);
            }
#else

			ParamUpdateCmd = API_TRUE;
			ke_timer_set(APP_CUSS_PARAM_UPDATE_REQ, TASK_APP, TIMER_SETTING_S(BLE_PARAM_UPDATE_TIME));

			if(rsl15_info->date_time_update!=DATE_TIME_UPDATE_SUCCESS) {										// RTC Time Initial...o?? ???? ?? ?? 1??? ????...???? ??u..
				ke_timer_set(APP_CUSS_AGMS_REQ_DATE_TIME, KE_BUILD_ID(TASK_APP, conidx),TIMER_SETTING_S(3));	// 3sec ??
			}



#if 0
			if(!customss_Notify_Time) {
				customss_Notify_Time = API_TRUE;

				CUSTOMSS_NotifyOnTimeout(TIMER_SETTING_S(10));
	            swmLogInfo("CUSTOMSS_NotifyOnTimeout = %d\r\n", customss_Notify_Time);
			}
#endif

#endif
        }
        break;

        case GAPC_DISCONNECT_IND:
        {
            const BondInfo_t *p_bondinfo = NULL;
            /* If record is bonded and disconnection is due to MIC failure, it's likely
            *  that we tried to encrypt with outdated bond information. So, let's
            *  remove it. */
            if (GAPC_IsBonded(conidx) &&
                ((p_bondinfo = GAPC_GetBondInfo(conidx)) != NULL) &&
                (((struct gapc_disconnect_ind *)param)->reason
                            == CO_ERROR_TERMINATED_MIC_FAILURE))
            {
                BondList_Remove(p_bondinfo->state);
            }

            /* Invalidate the connection handle so that number of connection is
             * correctly updated. */
            GAPC_ConnectionHandle_Invalidate(conidx);

            /* If advertising activity is stopped, restart advertising while
             * not connected to maximum number of peers for this application */
            uint8_t cur_conn_cnt = GAPC_ConnectionCount();

            if (cur_conn_cnt == (APP_MAX_NB_CON - 1))
            {
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("    Restarting advertising...\r\n");
#endif
                GAPM_AdvActivityStart(advActivityStatus.actv_idx, 0, 0);
            }

#if 0
            /* Stop BASS */
            disable_BASS_Tasks();
#endif


#if 0
            /* If there is no connection currently and connection parameter request task
             * has been running, suspend it.
             */
            if ((Scheduler_Get_TaskState(APP_CONN_PARAM_UDPATE_TASK_NUM) == TASK_BLOCKED) &&
                (cur_conn_cnt == 0))
            {
                Scheduler_Set_TaskState(APP_CONN_PARAM_UDPATE_TASK_NUM, TASK_SUSPENDED);
            }
#endif


#ifdef SWMTRACE_OUTPUT
            swmLogInfo("__GAPC_DISCONNECT_IND: reason = %d\r\n",
                    ((struct gapc_disconnect_ind*)param)->reason);
#endif


        }
        break;

        case GAPM_ADDR_SOLVED_IND: /* Step 10(a) */
        {
            /* Private address resolution was successful */
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("__GAPM_ADDR_SOLVED_IND\r\n");
#endif
            conidx = KE_IDX_GET(dest_id);
            APP_SendConCfm(conidx);
        }
        break;

        case GAPM_CMP_EVT: /* Step 10(b) */
        {
            /* Private address resolution couldn't find an IRK that resolves this address.
             * (i.e. peer device not previously bonded). Confirm connection without LTK. */
            const struct gapm_cmp_evt *p = param;
            if ((p->operation == GAPM_RESOLV_ADDR) &&
                (p->status == GAP_ERR_NOT_FOUND))
            {
                conidx = KE_IDX_GET(dest_id);
                APP_SendConCfm(conidx);
            }
        }
        break;

        case GAPC_PARAM_UPDATE_REQ_IND: /* Step 11 */
        {
            /* Peer device requested update in connection params. Accept it. */
            GAPC_ParamUpdateCfm(conidx, true, 0xFFFF, 0xFFFF);
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("GAPC_PARAM_UPDATE_REQ_IND\r\n");
#endif
        }
        break;

        case GAPC_GET_DEV_INFO_REQ_IND: /* Step 12 */
        {
            /* Peer device requested information about the device (such as name,
             * appearance, slv pref. params). See getDevInfoCfm for details.  */
            const struct gapc_get_dev_info_req_ind *p = param;
            GAPC_GetDevInfoCfm(conidx, p->req, getDevInfoCfm[p->req]);
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("GAPC_GET_DEV_INFO_REQ_IND: req = %d\r\n", p->req);
#endif
        }
        break;

        case GAPC_PARAM_UPDATED_IND:
        {
            const struct gapc_param_updated_ind *p = param;

#ifdef SWMTRACE_OUTPUT
            swmLogInfo("GAPC_PARAM_UPDATED_IND\r\n");
            swmLogInfo("  gapc_param_updated_ind->con_interval = %d\r\n", p->con_interval);
            swmLogInfo("  gapc_param_updated_ind->con_latency = %d\r\n", p->con_latency);
            swmLogInfo("  gapc_param_updated_ind->sup_to = %d\r\n", p->sup_to);
#endif

#if 0
            /* Update connection parameter for connection parameter update task */
            conn_param_info[conidx].con_interval = p->con_interval;
            conn_param_info[conidx].update_attempt = 0;

            /* If new connection parameter is not desired value and parameter update task has been suspended,
             * change it to BLOCKED state for the scheduler to schedule it. */
            if ((Scheduler_Get_TaskState(APP_CONN_PARAM_UDPATE_TASK_NUM) == TASK_SUSPENDED) &&
                (p->con_interval >= MASTER_DEF_MIN_CONN_INT && p->con_interval < MASTER_DEF_MAX_CONN_INT))
            {
                Scheduler_Set_TaskState(APP_CONN_PARAM_UDPATE_TASK_NUM, TASK_BLOCKED);
            }
#else

/**********************************************************************************************************************
 *	Default Minimum & Maximum Connection Interval from master device side
 **********************************************************************************************************************/
            if ((p->con_interval > MASTER_DEF_MIN_CONN_INT && p->con_interval < MASTER_DEF_MAX_CONN_INT) && (con_param_update_attempt < MAX_CON_PARAM_UPDATE_ATTEMPT))
            {
#if 0
            	GAPC_ParamUpdateCmd(conidx, MIN_CONNECTION_INTERVAL, MAX_CONNECTION_INTERVAL,
                                    SLAVE_LATENCY, CONNECTION_SUPERVISION_TIMEOUT,
                                    MIN_CONNECTION_EVENT_DURATION, MAX_CONNECTION_EVENT_DURATION);
#else
				ke_timer_set(APP_CUSS_PARAM_UPDATE_REQ, TASK_APP, TIMER_SETTING_S(BLE_PARAM_UPDATE_TIME));			// 2sec interval
#endif
				swmLogInfo("con_param_update_attempt : %d \r\n", con_param_update_attempt);
                con_param_update_attempt++;
            }

/****************************************************************************************************************************************
  *	connection parameter update confirm..2024.05.09
  *	if connection interval time & latency is equal, close parameter update time.
  ****************************************************************************************************************************************/
			if((ParamUpdateCmd==API_TRUE)&&(p->con_interval==conn_interval_time.intv_min)&&(p->con_latency==conn_interval_time.latency)) {
				ParamUpdateCmd=API_FALSE;

				ke_timer_clear(APP_CUSS_PARAM_UPDATE_REQ, TASK_APP);
				swmLogInfo("APP_CUSS_PARAM_UPDATE_REQ CLEAR!!!!\r\n");
            } 
#endif
        }
        break;
    }
}

void BLE_PairingHandler(ke_msg_id_t const msg_id, void const *param,
                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint8_t conidx = KE_IDX_GET(src_id);
    const BondInfo_t *p_bondinfo = NULL;
    static bool is_bond_bit_set[APP_MAX_NB_CON] = {false};

    switch (msg_id)
    {
        case GAPC_BOND_REQ_IND:  /* Step 13(a) - peer device wants to pair. Exchange keys */
        {
            const struct gapc_bond_req_ind *p = param;
            switch (p->request)
            {
                case GAPC_PAIRING_REQ:
                {
                    /* Remove old information from BondList (if bonded before) */
                    if ((GAPC_IsBonded(conidx)) &&
                        ((p_bondinfo = GAPC_GetBondInfo(conidx)) != NULL))
                    {
                        BondList_Remove(p_bondinfo->state);
                    }
#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("Invalidate...\r\n");
#endif
                    GAPC_BondInfoStatus_Invalidate(conidx);
                    GAPC_SaveBondInfo(conidx);

                    /*
                     * Check if bond authentication (bond bit field in the pairing request from peer). Used later to check
                     * if record should be added to the bond list.
                     */
                    is_bond_bit_set[conidx] = p->data.auth_req & GAP_AUTH_BOND;

                    bool accept = BondList_Size() < BONDLIST_MAX_SIZE;
#if SECURE_CONNECTION
                    if (p->data.auth_req & GAP_AUTH_SEC_CON)
                    {
                        pairingRsp.pairing_feat.auth = GAP_AUTH_REQ_SEC_CON_BOND;
                        pairingRsp.pairing_feat.sec_req = GAP_SEC1_NOAUTH_PAIR_ENC;
                        pairingRsp.pairing_feat.iocap = GAP_IO_CAP_DISPLAY_YES_NO;
                    }
                    else
#endif
                    {
                        pairingRsp.pairing_feat.auth = GAP_AUTH_REQ_NO_MITM_BOND;
                        pairingRsp.pairing_feat.sec_req = GAP_NO_SEC;
                    }
#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("__GAPC_BOND_REQ_IND / GAPC_PAIRING_REQ: accept = %d conidx=%d\r\n", accept, conidx);
#endif
                    GAPC_BondCfm(conidx, GAPC_PAIRING_RSP, accept, &pairingRsp);
                }
                break;

                case GAPC_NC_EXCH:
                {
#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("__GAPC_BOND_REQ_IND / GAPC_NC_EXCH: accept = %d conidx=%d\r\n", true, conidx);

                    /* Print GAPC_NC_EXCH key */
                    uint32_t ncExch = p->data.nc_data.value[3] << 24;
                    ncExch |= p->data.nc_data.value[2] << 16;
                    ncExch |= p->data.nc_data.value[1] << 8;
                    ncExch |= p->data.nc_data.value[0];
                    swmLogInfo("__GAPC_BOND_REQ_IND / GAPC_NC_EXCH: key = %d\r\n", ncExch);
#endif
                    /* For now we set accept=true for GAPC_NC_EXCH request in order to accept
                     * request and complete pairing successfully. */

                    /* Send confirmation */
                    GAPC_BondCfm(conidx, GAPC_NC_EXCH, true, &pairingRsp);
                }
                break;

                case GAPC_LTK_EXCH:
                {
#ifdef SWMTRACE_OUTPUT
                    /* Prepare and send random LTK (legacy only) */
                    swmLogInfo("__GAPC_BOND_REQ_IND / GAPC_LTK_EXCH\r\n");
#endif
                    union gapc_bond_cfm_data ltkExch = {0};
                    ltkExch.ltk.ediv = co_rand_hword();
                    for (uint8_t i = 0, i2 = GAP_RAND_NB_LEN; i < GAP_RAND_NB_LEN; i++, i2++)
                    {
                        ltkExch.ltk.randnb.nb[i] = co_rand_byte();
                        ltkExch.ltk.ltk.key[i] = co_rand_byte();
                        ltkExch.ltk.ltk.key[i2] = co_rand_byte();
                    }
                    GAPC_BondCfm(conidx, GAPC_LTK_EXCH, true, &ltkExch); /* Send confirmation */
                }
                break;

                case GAPC_TK_EXCH: /* Prepare and send TK */
                {
#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("__GAPC_BOND_REQ_IND / GAPC_TK_EXCH\r\n");
#endif
                    /* IO Capabilities are set to GAP_IO_CAP_NO_INPUT_NO_OUTPUT in this application.
                     * Therefore TK exchange is NOT performed. It is always set to 0 (Just Works algorithm). */
                }
                break;

                case GAPC_IRK_EXCH:
                {
#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("__GAPC_BOND_REQ_IND / GAPC_IRK_EXCH\rn");
#endif
                    union gapc_bond_cfm_data irkExch = {0};
                    memcpy(irkExch.irk.addr.addr.addr, GAPM_GetDeviceConfig()->addr.addr, GAP_BD_ADDR_LEN);
                    irkExch.irk.addr.addr_type = GAPM_GetDeviceConfig()->privacy_cfg;
                    memcpy(irkExch.irk.irk.key, GAPM_GetDeviceConfig()->irk.key, GAP_KEY_LEN);
                    GAPC_BondCfm(conidx, GAPC_IRK_EXCH, true, &irkExch); /* Send confirmation */
                }
                break;

                case GAPC_CSRK_EXCH:
                {
#ifdef SWMTRACE_OUTPUT
                    swmLogInfo("__GAPC_BOND_REQ_IND / GAPC_CSRK_EXCH\r\n");
#endif
                    union gapc_bond_cfm_data csrkExch = {0};
                    GAPC_BondCfm(conidx, GAPC_CSRK_EXCH, true, &csrkExch); /* Send confirmation */
                }
                break;
            }
        }
        break;

        case GAPC_BOND_IND: /* Step 14(a) - pairing finished */
        {
            const struct gapc_bond_ind *p = param;
            if (p->info == GAPC_PAIRING_SUCCEED)
            {
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("__GAPC_BOND_IND / GAPC_PAIRING_SUCCEED\r\n");
#endif

                if (is_bond_bit_set[conidx])
                {
                    GAPC_AddRecordToBondList(conidx); /* STEP 15 */
                    swmLogInfo("__GAPC_BOND_IND: index=%d, used bonds=%d, max=%d\r\n",
                        (int)conidx,
                        BondList_Size(),
                        BONDLIST_MAX_SIZE
                    );
                }
            }
            else if (p->info == GAPC_PAIRING_FAILED)
            {
#ifdef SWMTRACE_OUTPUT
                swmLogError("__GAPC_BOND_IND / GAPC_PAIRING_FAILED reason=%d\r\n", p->data.reason);
#endif
            }
        }
        break;

        case GAPC_ENCRYPT_REQ_IND: /* Step 13(b) */
        {
            /* Peer device was bonded previously and wants to encrypt the link.
             * Accept request if the bond information is valid & EDIV/RAND match */
            const struct gapc_encrypt_req_ind *p = param;
            p_bondinfo = GAPC_GetBondInfo(conidx);
            if (p_bondinfo != NULL)
            {
                bool found = (GAPC_IsBonded(conidx) &&
                              (p->ediv == p_bondinfo->ediv) &&
                              !memcmp(p->rand_nb.nb, p_bondinfo->rand, GAP_RAND_NB_LEN));
#ifdef SWMTRACE_OUTPUT
                swmLogInfo("__GAPC_ENCRYPT_REQ_IND: bond information %s\r\n", (found ? "FOUND" : "NOT FOUND"));
#endif
                GAPC_EncryptCfm(conidx, found, p_bondinfo->ltk, GAP_KEY_LEN);
            }
        }
        break;

        case GAPC_ENCRYPT_IND: /* Step 14(b)  */
        {
#ifdef SWMTRACE_OUTPUT
            swmLogInfo("__GAPC_ENCRYPT_IND: Link encryption is ON\r\n");
#endif
        }
        break;
    }
}

static void SetConnectionCfmParams(uint8_t conidx, struct gapc_connection_cfm *cfm)
{
    cfm->svc_chg_handle = 0;
    cfm->ltk_present = false;
    cfm->pairing_lvl = GAP_PAIRING_UNAUTH;

    cfm->cli_feat = 0;
    cfm->cli_info = 0;
    cfm->gatt_start_handle = 0;
    cfm->gatt_end_handle = 0;
    cfm->svc_chg_handle = 0;

    const BondInfo_t *p_bondinfo = GAPC_GetBondInfo(conidx);
    if ((GAPC_IsBonded(conidx)) && (p_bondinfo != NULL))
    {
        cfm->ltk_present = true;
        memcpy(cfm->rcsrk.key, p_bondinfo->csrk, KEY_LEN);
        cfm->lsign_counter = 0xFFFFFFFF;
        cfm->rsign_counter = 0;
        cfm->pairing_lvl = p_bondinfo->pairing_lvl;
    }
#ifdef SWMTRACE_OUTPUT
    swmLogInfo("  connectionCfm->ltk_present = %d\r\n", cfm->ltk_present);
    swmLogInfo("  connectionCfm->pairing_lvl = %d\r\n", cfm->pairing_lvl);
#endif
}

void PrepareAdvScanData(void)
{
    uint8_t companyID[] = APP_COMPANY_ID;
//    uint8_t devName[]   = APP_DEVICE_NAME;

#if ADV_EXTENSION == 1
    bool adv_extended_enabled = true;
#else
    bool adv_extended_enabled = false;
#endif

    /* Assemble advertising data as device name + company ID and
     * copy into app_adv_data */
    app_adv_data_len = 0;
    GAP_AddAdvData(adv_extended_enabled, APP_DEVICE_NAME_LEN + 1, GAP_AD_TYPE_COMPLETE_NAME,
    		device_name, app_adv_data, &app_adv_data_len);
    GAP_AddAdvData(adv_extended_enabled, APP_COMPANY_ID_LEN + 1, GAP_AD_TYPE_MANU_SPECIFIC_DATA,
                   companyID, app_adv_data, &app_adv_data_len);

    /* Set scan response data as company ID */
    app_scan_rsp_data_len = 0;
    GAP_AddAdvData(adv_extended_enabled, APP_COMPANY_ID_LEN + 1, GAP_AD_TYPE_MANU_SPECIFIC_DATA,
                   companyID, app_scan_rsp_data, &app_scan_rsp_data_len);
}

void APP_SendConCfm(uint8_t conidx)
{
    struct gapc_connection_cfm cfm = {0};
    SetConnectionCfmParams(conidx, &cfm);

    /* Send connection confirmation */
    GAPC_ConnectionCfm(conidx, &cfm);

    /* Get Task IDs for each added profile/s */
    uint16_t added_profile_task_id[APP_MAX_NB_PROFILES];
    memcpy(added_profile_task_id, GAP_GetProfileAddedTaskId(), APP_MAX_NB_PROFILES);

    /* Enable standard profile services using task IDs */
    for (uint8_t i = 0; i < APP_MAX_NB_PROFILES; i++)
    {
        if (added_profile_task_id[i] == TASK_ID_BASS)
        {

#if 0    		// sodykim delete ...because UXN not use Schedule Task
            /* Enable BASS */
            BASS_EnableReq(conidx);
            enable_BASS_Tasks();
#endif
        }
    }

#if CEM102_DIAG_FUNCTIONS
    /* Start MTU exchange */
    GATTC_MtuExchange(conidx);
#endif /* CEM102_DIAG_FUNCTIONS */

    /* Start the timer for CUSTOM service server */
    if (GATT_GetEnv()->cust_svc_db &&
        GATT_GetEnv()->cust_svc_db[0].cust_svc_start_hdl)
    {
        CUSTOMSS_StartTimer();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	BELOW USER SETTING
//

/*
  * @brief Prepare and send GAPC_PARAM_UPDATE_CMD to perform an update on parameters
  *
  * Prepare and send GAPC_PARAM_UPDATE_CMD to perform an update of connection parameters.
  *
  * @param [in] conidx     Connection identifier
  * @param [in] intv_min   Minimum of connection interval (value = intv_max * 1.25 ms)
  * @param [in] intv_max   Maximum of connection interval (value = intv_max * 1.25 ms)
  * @param [in] time_out   Link supervision timeout (value = time_out * 10 ms)
  * @param [in] latency    connection latency (connSupervisionTimeout / (connIntervalMax * 2))-1))
  * @param [in] ce_len_min Minimum CE length (value = ce_len_min * 0.625 ms)
  * @param [in] ce_len_max Maximum CE length (value = ce_len_min * 0.625 ms)
*/


/***********************************************************************************************************************************************************************************
  *
  *		CONNECTION ??? ?????????? ?????? ???? ??? ???...
  *
  ***********************************************************************************************************************************************************************************/

void ParamUpdateHandler(ke_msg_id_t const msg_id, void const *param, ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint8_t connectionCount;
	uint8_t conidx;

	connectionCount = GAPC_ConnectionCount();
	conidx = KE_IDX_GET(dest_id);


#ifdef SWMTRACE_OUTPUT
		swmTrace_printf("ParamUpdateHandler : [%d][%d]\r\n", conidx, connectionCount);
#endif


/* Process the event that triggered the handler */
	switch (msg_id)		{
	/* Event: a service has been added to the database, start notifications */
		case APP_CUSS_PARAM_UPDATE_REQ:
			if(connectionCount>0) {

				if(progress_time_condition(param_update_time, 2)) {								// duplication..minimum 2 second command interval time
					param_update_time = Sys_RTC_Value_Seconds();

#ifdef SWMTRACE_OUTPUT
					swmTrace_printf("APP_CUSS_PARAM_UPDATE_REQ : [%d][%d][%d][%d]\r\n", conidx, connectionCount, src_id, dest_id);
#endif
					GAPC_ParamUpdateCmd(conidx, conn_interval_time.intv_min, conn_interval_time.intv_max, conn_interval_time.latency, conn_interval_time.sv_tmout, 0xFFFF, 0xFFFF);
				}

				ParamUpdateCmd = API_TRUE;
				ke_timer_set(APP_CUSS_PARAM_UPDATE_REQ, TASK_APP, TIMER_SETTING_S(BLE_PARAM_UPDATE_TIME+3));			// 3sec interval
			}

			else {
				ParamUpdateCmd = API_TRUE;
				ke_timer_set(APP_CUSS_PARAM_UPDATE_REQ, TASK_APP, TIMER_SETTING_S(BLE_PARAM_UPDATE_TIME+3));			// 3sec interval
			}

		break;

		default:
			break;
	}
}



