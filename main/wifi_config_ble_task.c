#include "wifi_config_ble_task.h"

static const char *TAG = "WIFI_CFG_BLE_TASK";

static void wifi_config_gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40

#define PREPARE_BUF_MAX_SIZE 1024

#define TEST_DEVICE_NAME	"EIE553_LOCK_"
#define MANUFACTURER_DATA_LEN 3

static esp_gatt_char_prop_t gatt_char_property = 0;


static uint8_t adv_config_done = 0;
#define adv_config_flag	  (1 << 0)
#define scan_rsp_config_flag (1 << 1)

#ifdef CONFIG_SET_RAW_ADV_DATA
static uint8_t raw_adv_data[] = {
		0x02, 0x01, 0x06,
		0x02, 0x0a, 0xeb, 0x03, 0x03, 0xab, 0xcd
};
static uint8_t raw_scan_rsp_data[] = {
		0x0f, 0x09, 0x45, 0x53, 0x50, 0x5f, 0x47, 0x41, 0x54, 0x54, 0x53, 0x5f, 0x44,
		0x45, 0x4d, 0x4f
};
#else

static uint8_t adv_service_uuid64[16] = {
	/* LSB <--------------------------------------------------------------------------------> MSB */
	//first uuid, 16bit, [12],[13] is the value
	0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x53, 0xE5, 0x00,
};

// The length of adv data must be less than 31 bytes
static uint8_t manufacturer_data[MANUFACTURER_DATA_LEN] = {0x45, 0x49, 0x45};
static uint8_t service_data[4] = {0x53, 0xe5, 0x00, 0x00};
//adv data
static esp_ble_adv_data_t adv_data = {
	.set_scan_rsp = false,
	.include_name = true,
	.include_txpower = false,
	.min_interval = 0x20,
	.max_interval = 0x40,
	.appearance = 0x00,
	.manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
	.p_manufacturer_data =  NULL, //&test_manufacturer[0],
	.service_data_len = 0,
	.p_service_data = NULL,
	.service_uuid_len = 16,
	.p_service_uuid = adv_service_uuid64,
	.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
	.set_scan_rsp = true,
	.include_name = true,
	.include_txpower = false,
	.min_interval = 0x20,
	.max_interval = 0x40,
	.appearance = 0x00,
	.manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
	.p_manufacturer_data =  NULL, //&test_manufacturer[0],
	.service_data_len = 0,
	.p_service_data = NULL,
	.service_uuid_len = 16,
	.p_service_uuid = adv_service_uuid64,
	.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

#endif /* CONFIG_SET_RAW_ADV_DATA */

static esp_ble_adv_params_t adv_params = {
	.adv_int_min		= 0x20,
	.adv_int_max		= 0x40,
	.adv_type		   = ADV_TYPE_IND,
	.own_addr_type	  = BLE_ADDR_TYPE_PUBLIC,
	//.peer_addr			=
	//.peer_addr_type	   =
	.channel_map		= ADV_CHNL_ALL,
	.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#define PROFILE_NUM 1
#define WIFI_CONF_APP_PROFILE_ID 0
#define WIFI_CONF_APP_ID 0x55

struct gatts_profile_inst {
	esp_gatts_cb_t gatts_cb;
	uint16_t gatts_if;
	uint16_t app_id;
	uint16_t conn_id;
	uint16_t service_handle;
	esp_gatt_srvc_id_t service_id;
	uint16_t char_handle;
	esp_bt_uuid_t char_uuid;
	esp_gatt_perm_t perm;
	esp_gatt_char_prop_t property;
	uint16_t descr_handle;
	esp_bt_uuid_t descr_uuid;
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst wifi_config_profile_tab[PROFILE_NUM] = {
	[WIFI_CONF_APP_PROFILE_ID] = {
		.gatts_cb = wifi_config_gatts_profile_event_handler,				   /* This demo does not implement, similar as profile A */
		.gatts_if = ESP_GATT_IF_NONE,	   /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
	},
	
};

typedef struct {
	uint8_t				 *prepare_buf;
	int					 prepare_len;
} prepare_type_env_t;

static prepare_type_env_t b_prepare_write_env;

uint16_t wifi_config_ble_handle_table[HRS_IDX_NB];

#define SVC_INST_ID                 0

/* Service */
static const uint16_t GATTS_SERVICE_UUID		= 0xE553;
static const uint16_t GATTS_CHAR_SSID_UUID		= 0x551D;
static const uint16_t GATTS_CHAR_PASS_UUID		= 0x0A55;
static const uint16_t primary_service_uuid		= ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid	= ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid	= ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_write			= ESP_GATT_CHAR_PROP_BIT_WRITE;
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

static const esp_gatts_attr_db_t gatt_db[HRS_IDX_NB] = {
	// Service Decl
	[IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID), (uint8_t *)&GATTS_SERVICE_UUID}},
	[IDX_CHAR_SSID] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},
	[IDX_CHAR_SSID_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_SSID_UUID, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 32, 0, NULL}},
	[IDX_CHAR_PASS] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},
	[IDX_CHAR_PASS_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_PASS_UUID, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 32, 0, NULL}},
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
	switch (event) {
	case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
		adv_config_done &= (~adv_config_flag);
		if (adv_config_done == 0){
			esp_ble_gap_start_advertising(&adv_params);
		}
		break;
	case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
		adv_config_done &= (~scan_rsp_config_flag);
		if (adv_config_done == 0){
			esp_ble_gap_start_advertising(&adv_params);
		}
		break;
	case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
		//advertising start complete event to indicate advertising start successfully or failed
		if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
			ESP_LOGE(TAG, "Advertising start failed\n");
		}
		break;
	case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
		if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
			ESP_LOGE(TAG, "Advertising stop failed\n");
		}
		else {
			ESP_LOGI(TAG, "Stop adv successfully\n");
		}
		break;
	case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
		 ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
				  param->update_conn_params.status,
				  param->update_conn_params.min_int,
				  param->update_conn_params.max_int,
				  param->update_conn_params.conn_int,
				  param->update_conn_params.latency,
				  param->update_conn_params.timeout);
		break;
	default:
		break;
	}
}




static void wifi_config_gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	esp_err_t err;
	char ble_dev_name[24];
	char buf[256];
	switch (event) {
	case ESP_GATTS_REG_EVT:
		// No Semaphore is needed, as we are in single thread
		snprintf(ble_dev_name, 24, "IOT_%s", device_info.device_id);
		err = esp_ble_gap_set_device_name(ble_dev_name);
		if(err != ESP_OK) {
			ESP_LOGE(TAG, "Set BLE Device name failed, error=%x", err);
		}else{
			ESP_LOGI(TAG, "Named BLE Device to: %s", ble_dev_name);
		}


		//config adv data
		esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
		if (ret){
			ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
		}
		adv_config_done |= adv_config_flag;
		//config scan response data
		ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
		if (ret){
			ESP_LOGE(TAG, "config scan response data failed, error code = %x", ret);
		}
		adv_config_done |= scan_rsp_config_flag;

		err = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID);
		if(err != ESP_OK) {
			ESP_LOGE(TAG, "Unable to create attribute table, error=%x", err);
		}

		break;
	case ESP_GATTS_READ_EVT: {
		ESP_LOGI(TAG, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d\n", param->read.conn_id, param->read.trans_id, param->read.handle);
		// Read is not permitted.
		}
		break;
	case ESP_GATTS_WRITE_EVT: {
			ESP_LOGI(TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d\n", param->write.conn_id, param->write.trans_id, param->write.handle);
			if (!param->write.is_prep){
				ESP_LOGI(TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
				if(wifi_config_ble_handle_table[IDX_CHAR_SSID_VAL] == param->write.handle) {
					ESP_LOGI(TAG, "User has written SSID");
					if(param->write.len <= 16) {
						memcpy(device_info.wifi_ssid, param->write.value, param->write.len);
						device_info.wifi_ssid[param->write.len] = '\0';
						if(nvs_set_str(nvs, "ssid", device_info.wifi_ssid) != ESP_OK) {
							ESP_LOGE(TAG, "Unable to set new SSID to NVS");
						}
						nvs_commit(nvs);
					}else{
						ESP_LOGW(TAG, "Wifi SSID not written, len(%d) > 16", param->write.len);
					}
					
					snprintf(buf, 256, ">New WiFi Config\nSSID&PW Maximum\n16 Characters\n\nSSID=551DPW=0A55\nNew SSID/PW:\n%s\n%s", device_info.wifi_ssid, device_info.wifi_pass);
					oled_clear();
					oled_write_text(buf);
				}else if(wifi_config_ble_handle_table[IDX_CHAR_PASS_VAL] == param->write.handle) {
					ESP_LOGI(TAG, "User has written PASS");
					if(param->write.len <= 16 && param->write.len >= 8) {
						memcpy(device_info.wifi_pass, param->write.value, param->write.len);
						device_info.wifi_pass[param->write.len] = '\0';
						if(nvs_set_str(nvs, "wifi_pw", device_info.wifi_pass) != ESP_OK) {
							ESP_LOGE(TAG, "Unable to set new wifi password to NVS");
						}
						nvs_commit(nvs);
					}else{
						ESP_LOGW(TAG, "Wifi password not written, len(%d) > 16 or < 8", param->write.len);
					}
					
					snprintf(buf, 256, ">New WiFi Config\nSSID&PW Maximum\n16 Characters\n\nSSID=551DPW=0A55\nNew SSID/PW:\n%s\n%s", device_info.wifi_ssid, device_info.wifi_pass);
					oled_clear();
					oled_write_text(buf);
				}
				esp_log_buffer_hex(TAG, param->write.value, param->write.len);

				
			}
		}
		break;
	case ESP_GATTS_EXEC_WRITE_EVT:
		ESP_LOGI(TAG,"ESP_GATTS_EXEC_WRITE_EVT");
		esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
		break;
	case ESP_GATTS_MTU_EVT:
		ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
		break;
	case ESP_GATTS_CONF_EVT:
		ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status = %d", param->conf.status);
		break;
	case ESP_GATTS_START_EVT:
		ESP_LOGI(TAG, "ESP_GATTS_START_EVT, status = %d; service_handle=%d", param->start.status, param->start.service_handle);
		break;
	case ESP_GATTS_CONNECT_EVT: {
			esp_ble_conn_update_params_t conn_params = {0};
			memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
			conn_params.latency = 0;
			conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
			conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
			conn_params.timeout = 400;
			ESP_LOGI(TAG, "CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
					 param->connect.conn_id,
					 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
					 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
			esp_ble_gap_update_conn_params(&conn_params);
		}
		break;
	case ESP_GATTS_DISCONNECT_EVT:
		ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT");
		esp_ble_gap_start_advertising(&adv_params);
		break;
	case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
			if(param->add_attr_tab.status != ESP_GATT_OK) {
				ESP_LOGE(TAG, "Create attribute table failed, err=0x%x", param->add_attr_tab.status);
			}else if(param->add_attr_tab.num_handle != HRS_IDX_NB) {
				ESP_LOGE(TAG, "Create attribute table abnormally, num_handle (%d), expected (%d)", param->add_attr_tab.num_handle, HRS_IDX_NB);
			}else{
				ESP_LOGI(TAG, "Create attribute table successfully, number of handles=%d", param->add_attr_tab.num_handle);
			}
			memcpy(wifi_config_ble_handle_table, param->add_attr_tab.handles, sizeof(wifi_config_ble_handle_table));
			esp_ble_gatts_start_service(wifi_config_ble_handle_table[IDX_SVC]);
		}
		break;
	case ESP_GATTS_STOP_EVT:
	case ESP_GATTS_OPEN_EVT:
	case ESP_GATTS_CANCEL_OPEN_EVT:
	case ESP_GATTS_CLOSE_EVT:
	case ESP_GATTS_LISTEN_EVT:
	case ESP_GATTS_CONGEST_EVT:
	case ESP_GATTS_UNREG_EVT:
	case ESP_GATTS_DELETE_EVT:
	default:
		break;
	}
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	/* If event is register event, store the gatts_if for each profile */
	if (event == ESP_GATTS_REG_EVT) {
		if (param->reg.status == ESP_GATT_OK) {
			wifi_config_profile_tab[WIFI_CONF_APP_PROFILE_ID].gatts_if = gatts_if;
		}else{
			ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d\n",
			param->reg.app_id,
			param->reg.status);
			return;
		}
	}

	/* If the gatts_if equal to profile A, call profile A cb handler,
	* so here call each profile's callback */
	do {
		int idx;
		for (idx = 0; idx < PROFILE_NUM; idx++) {
			if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
			gatts_if == wifi_config_profile_tab[idx].gatts_if) {
				if (wifi_config_profile_tab[idx].gatts_cb) {
					wifi_config_profile_tab[idx].gatts_cb(event, gatts_if, param);
				}
			}
		}
	}while (0);
}

void wifi_config_ble_task(void *pvParameters) {
	esp_err_t err;
	
	ESP_LOGI(TAG, "Task starting up...");
	
	err = esp_ble_gatts_register_callback(gatts_event_handler);
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "BLE GATTs callback registration failed, err = %x", err);
		return;
	}

	err = esp_ble_gap_register_callback(gap_event_handler);
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "BLE GAP callback registration failed, err = %x", err);
		return;
	}

	err = esp_ble_gatts_app_register(WIFI_CONF_APP_ID);
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "BLE GATTs application registration failed, err = %x", err);
		return;
	}

	err = esp_ble_gatt_set_local_mtu(500);
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "BLE local MTU set failed, err = %x", err);
		return;
	}


	while(1) {
		vTaskDelay(500 / portTICK_RATE_MS);
	}
}
