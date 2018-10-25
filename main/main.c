
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_spi_flash.h"

#include "esp_log.h"
#include "esp_event_loop.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"

#include <http_server.h>

#include "iot_device_info.h"

#include "config.h"
#include "oled.h"

#include "oled_task.h"
#include "smart_lock_task.h"
#include "light_sensor_task.h"
#include "ble_task.h"
#include "wifi_config_ble_task.h"

#include "smart_lock_logger.h"

#include "webserver.h"

#include "util.h"


static const char *TAG = "main";

esp_adc_cal_characteristics_t *adc_characteristics;

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static ip4_addr_t s_ip_addr;


// Global (non-static) vars
iot_device_info_t device_info;
SemaphoreHandle_t device_info_semaphore; // Semaphore for device_info
EventGroupHandle_t oled_event_group;
const int OLED_NEED_UPDATE_BIT = BIT0;

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
	httpd_handle_t *webserver = (httpd_handle_t *)ctx;
	switch (event->event_id) {
		case SYSTEM_EVENT_STA_START:
			esp_wifi_connect();
			break;
		case SYSTEM_EVENT_STA_GOT_IP:
			xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
			s_ip_addr = event->event_info.got_ip.ip_info.ip;
			while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);
			device_info.ip=s_ip_addr.addr;
			device_info.wifi_connected=1;
			xSemaphoreGive(device_info_semaphore);
			xEventGroupSetBits(oled_event_group, OLED_NEED_UPDATE_BIT); // Notify OLED Task to write to screen

			// Start web server
			if(*webserver == NULL) {
				ESP_LOGI(TAG, "Starting Webserver...");
				*webserver = start_webserver();
			}

			smart_lock_logger_add_log(CONNECTED, WIFI, NULL, 0);
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			// No longer connected, we update our status
			while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);
			device_info.wifi_connected=0;
			xSemaphoreGive(device_info_semaphore);
			xEventGroupSetBits(oled_event_group, OLED_NEED_UPDATE_BIT); // Notify OLED Task to write to screen

			// Stop web server
			if(*webserver) {
				stop_webserver(*webserver);
				*webserver = NULL;
			}
			/* This is a workaround as ESP32 WiFi libs don't currently
			 auto-reassociate. */
			esp_wifi_connect();
			xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
			break;
		case SYSTEM_EVENT_STA_LOST_IP:
			smart_lock_logger_add_log(DISCONNECTED, WIFI, NULL, 0);
			break;
		default:
			break;
	}
	return ESP_OK;
}

nvs_handle nvs;

void app_main() {
	uint8_t mac[6];
	esp_err_t err;
	static httpd_handle_t webserver = NULL;
	size_t sz;
	char nvs_ssid[17];
	char nvs_wifi_pw[17];
	char nvs_dev_id[13];
	char nvs_pin[7];
	char nvs_webadmin_password[24];
	uint8_t hard_reset = 0;
	uint32_t gpio0_count=0;
	uint32_t gpio0_loop=0;

	vTaskDelay(800/portTICK_PERIOD_MS);
	ESP_LOGI(TAG, "EIE553 IOT Board Firmware Starting Up...");

	esp_efuse_mac_get_default(mac);
	ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	// IO0 Init
	ESP_LOGI(TAG, "Initializing IO0 GPIO...");
	gpio_config_t io_config;
	io_config.intr_type=GPIO_PIN_INTR_DISABLE;
	io_config.mode=GPIO_MODE_INPUT;
	io_config.pin_bit_mask=(1ULL << 0);
	io_config.pull_down_en=0;
	io_config.pull_up_en=1;
	gpio_config(&io_config);

	ESP_LOGI(TAG, "[HW_INIT] I2C...");
	// I2C init
	i2c_config_t i2c_conf;
	i2c_conf.mode = I2C_MODE_MASTER;
	i2c_conf.sda_io_num = 21;
	i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
	i2c_conf.scl_io_num = 22;
	i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	i2c_conf.master.clk_speed = 400*1000; // 400KHz
	i2c_param_config(I2C_NUM_0, &i2c_conf);
	i2c_driver_install(I2C_NUM_0, i2c_conf.mode, 0, 0, 0);

	// OLED Init
	ESP_LOGI(TAG, "[HW_INIT] OLED...");
	oled_init();
	// OLED Event Groups
	oled_event_group = xEventGroupCreate();
	xEventGroupSetBits(oled_event_group, OLED_NEED_UPDATE_BIT);
	oled_clear();


	oled_clear();
	oled_write_text("Initialization:\nNVS");
	// NVS Init
	ESP_LOGI(TAG, "[HW_INIT] NVS...");
	err = nvs_flash_init();
	if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
	
	err = nvs_open("storage", NVS_READWRITE, &nvs);
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "Error \"%s\" when opening NVS handle!\n", esp_err_to_name(err));
		return;
	}

	oled_clear();
	oled_write_text("Initialization:\nWi-Fi");
	// Wi-Fi Init
	ESP_LOGI(TAG, "[HW_INIT] Wi-Fi...");
	tcpip_adapter_init();
	wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, &webserver));
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	
	oled_clear();
	oled_write_text("Initialization:\nBLE");
	// BLE Init
	ESP_LOGI(TAG, "[HW_INIT] BLE...");
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	err = esp_bt_controller_init(&bt_cfg);
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "Bluetooth controller initialization failed, err = %x", err);
		return;
	}
	// BT Controller Enable
	err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "Bluetooth controller enable failed, err = %x", err);
		return;
	}
	// Bluedroid Init
	err = esp_bluedroid_init();
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "Bluedroid initialization failed, err = %x", err);
		return;
	}
	// Bluedroid Enable
	err = esp_bluedroid_enable();
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "Bluedroid enable failed, err = %x", err);
		return;
	}


	while(1) {
		if(gpio_get_level(GPIO_NUM_0) == 0) {
			gpio0_count++;
		}
		vTaskDelay(10/portTICK_PERIOD_MS);
		gpio0_loop++;

		if(gpio0_loop > 200) {
			break;
		}
	}
	if(gpio0_count > 110) {
		ESP_LOGW(TAG, "HW Reset via IO0???");
		oled_clear();		
		oled_write_text("HOLD 5s\nto HW RESET");
		uint8_t i;
		char tmp_buf[32];
		for (i=0; i<5; i++) {
			oled_clear();
			snprintf(tmp_buf, 32, "HOLD %ds\nto HW RESET", (5-i));
			oled_write_text(tmp_buf);
			gpio0_loop=0;
			gpio0_count=0;
			while(gpio0_loop < 100) {
				if(gpio_get_level(GPIO_NUM_0) == 0) gpio0_count++;
				vTaskDelay(10/portTICK_PERIOD_MS);
				gpio0_loop++;
			}
			if(gpio0_count < 82) break;
		}
		if(i == 5) {
			ESP_LOGW(TAG, "Confirmed HW Reset!");
			hard_reset = 1;
		}
	}

	// Get SSID
	err = nvs_get_str(nvs, "ssid", NULL, &sz);
	if(err == ESP_OK && sz > 17) {
		// Wifi ssid max length is 16 characters (+1 for NULL)
		ESP_LOGE(TAG, "SSID too large(%d), resetting the device.", sz);
		hard_reset=1;
	}else if(err == ESP_OK && sz <= 17) {
		err = nvs_get_str(nvs, "ssid", nvs_ssid, &sz);
		ESP_LOGI(TAG, "Got Wifi SSID from NVS: %s", nvs_ssid);
	}else if(err == ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGW(TAG, "SSID not found in NVS, performing hard reset...");
		hard_reset=1;
	}else{
		ESP_LOGE(TAG, "Error when reading SSID: %s", esp_err_to_name(err));
	}

	// Get Wifi password
	err = nvs_get_str(nvs, "wifi_pw", NULL, &sz);
	if(err == ESP_OK && sz > 17) {
		// Wifi password max length is 16 characters (+1 for NULL)
		ESP_LOGE(TAG, "Wifi password large(%d), resetting the device.", sz);
		hard_reset=1;
	}else if(err == ESP_OK && sz <= 17) {
		err = nvs_get_str(nvs, "wifi_pw", nvs_wifi_pw, &sz);
		// Don't show wifi password to log for security reasons
	}else if(err == ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGW(TAG, "Wifi password not found in NVS, performing hard reset...");
		hard_reset=1;
	}else{
		ESP_LOGE(TAG, "Error when reading wifi password: %s", esp_err_to_name(err));
	}

	// Get device id
	err = nvs_get_str(nvs, "dev_id", NULL, &sz);
	if(err == ESP_OK && sz != 13) {
		ESP_LOGE(TAG, "Invalid device ID length(%d), resetting the device.", sz);
		hard_reset=1;
	}else if(err == ESP_OK && sz == 13) {
		err = nvs_get_str(nvs, "dev_id", nvs_dev_id, &sz);
		ESP_LOGI(TAG, "Got Device ID from NVS: %s", nvs_dev_id);
	}else if(err == ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGW(TAG, "Device ID not found in NVS, performing hard reset...");
		hard_reset=1;
	}else{
		ESP_LOGE(TAG, "Error when reading device ID: %s", esp_err_to_name(err));
	}

	// Get PIN
	err = nvs_get_str(nvs, "pin", NULL, &sz);
	if(err == ESP_OK && sz != 7) {
		ESP_LOGE(TAG, "Invalid PIN length(%d), resetting the device.", sz);
		hard_reset=1;
	}else if(err == ESP_OK && sz == 7) {
		err = nvs_get_str(nvs, "pin", nvs_pin, &sz);
		ESP_LOGI(TAG, "Got PIN from NVS: %s", nvs_pin);
	}else if(err == ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGW(TAG, "PIN not found in NVS, performing hard reset...");
		hard_reset=1;
	}else{
		ESP_LOGE(TAG, "Error when reading PIN: %s", esp_err_to_name(err));
	}

	// Get Webadmin Password
	err = nvs_get_str(nvs, "wa_passwd", NULL, &sz);
	if(err == ESP_OK && sz > 24) {
		// Webadmin password max length is 23 characters (+1 for NULL)
		ESP_LOGE(TAG, "Web Admin Password too large(%d), resetting the device.", sz);
		hard_reset=1;
	}else if(err == ESP_OK && sz <= 24) {
		err = nvs_get_str(nvs, "wa_passwd", nvs_webadmin_password, &sz);
		ESP_LOGI(TAG, "Got Web Admin Password from NVS: %s", nvs_webadmin_password);
	}else if(err == ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGW(TAG, "Web Admin Password not found in NVS, performing hard reset...");
		hard_reset=1;
	}else{
		ESP_LOGE(TAG, "Error when reading Web Admin Password: %s", esp_err_to_name(err));
	}

	if(hard_reset == 1) {
		ESP_LOGW(TAG, "Performing HW Reset, removing all values from NVS...");
		// Erase
		nvs_erase_key(nvs, "dev_id");
		nvs_erase_key(nvs, "wa_passwd");
		nvs_erase_key(nvs, "pin");
		nvs_erase_key(nvs, "wifi_pw");
		nvs_erase_key(nvs, "ssid");
		nvs_commit(nvs);

		// Generate new device ID
		char *a, *b, *c;
		char new_dev_id[13];
		a=getRandomString(); b=getRandomString(); c=getRandomString();
		new_dev_id[0]=a[0]; new_dev_id[1]=a[1]; new_dev_id[2]=a[2]; new_dev_id[3]=a[3];
		new_dev_id[4]=b[0]; new_dev_id[5]=b[1]; new_dev_id[6]=b[2]; new_dev_id[7]=b[3];
		new_dev_id[8]=c[0]; new_dev_id[9]=c[1]; new_dev_id[10]=c[2]; new_dev_id[11]=c[3];
		new_dev_id[12]='\0';
		free(a); free(b); free(c);
		ESP_LOGI(TAG, "Generated new device ID: %s", new_dev_id);
		if(nvs_set_str(nvs, "dev_id", new_dev_id) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to set new device ID");
			return;
		}
		// Generate default Web Admin Password
		ESP_LOGI(TAG, "Generated default web admin password");
		if(nvs_set_str(nvs, "wa_passwd", "admin") != ESP_OK) {
			ESP_LOGE(TAG, "Unable to set new device ID");
			return;
		}
		// Generate new PIN
		char *pin = getRandomPin();
		ESP_LOGI(TAG, "Generated new PIN: %s", pin);
		if(nvs_set_str(nvs, "pin", pin) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to set new PIN");
			free(pin);
			return;
		}
		free(pin);

		nvs_commit(nvs);
		
		char buf[256];
		snprintf(buf, 256, "IOT Smart Lock\nhas been reset!\n\nConfig SSID with\nBLE, then reboot\nID:%s\nSSID=551DPW=0A55", new_dev_id);
		strcpy(device_info.device_id, new_dev_id);
		
		oled_clear();
		oled_write_text(buf);
		xTaskCreate(&wifi_config_ble_task, "wifi_cfg_ble_task", 4096, NULL, 8, NULL);
		while(1) {
			vTaskDelay(10/portTICK_PERIOD_MS);
		}
	}



	// Setting global vars
	device_info.ip=0;
	device_info.wifi_connected=0;
	device_info.smart_lock_status=0;
	device_info_semaphore = xSemaphoreCreateMutex();
	if(device_info_semaphore == NULL) {
		ESP_LOGE(TAG, "device_info_semaphore creation failed.");
		return;
	}
	strcpy(device_info.pin, nvs_pin);
	strcpy(device_info.device_id, nvs_dev_id);
	strcpy(device_info.webadmin_username, nvs_pin);
	strcpy(device_info.webadmin_username, "admin");
	strcpy(device_info.webadmin_password, nvs_webadmin_password);
	strcpy(device_info.wifi_ssid, nvs_ssid);
	strcpy(device_info.wifi_pass, nvs_wifi_pw);
	smart_lock_logger_init(); // Init custom smart lock logger


	ESP_LOGI(TAG, "Performing HW Init, please wait...");



	oled_clear();
	oled_write_text("Initialization:\nADC");
	// ADC Init
	ESP_LOGI(TAG, "[HW_INIT] ADC...");
	if(esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
		printf("eFuse TP: Supported\n");
	}else{
		printf("eFuse TP: NOT Supported\n");
	}
	if(esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
		printf("eFuse VRef: Supported\n");
	}else{
		printf("eFuse VRef: NOT Supported\n");
	}
	adc1_config_width(ADC_WIDTH_BIT_12);
	adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11); // Full-scale voltage 3.9V, input is max 3.3V (1/2 Voltage Divider)
	adc_characteristics = calloc(1, sizeof(esp_adc_cal_characteristics_t));
	esp_adc_cal_value_t cal_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_characteristics);
	if(cal_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
		printf("ADC is characterized using Two Point Value\n");
	}else if(cal_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
		printf("ADC is characterized using eFuse VRef\n");
	}else if(cal_type == ESP_ADC_CAL_VAL_DEFAULT_VREF) {
		printf("ADC is characterized using Default VRef\n");
	}

	oled_clear();
	oled_write_text("Initialization:\nADC");
	// GPIO Init
	ESP_LOGI(TAG, "[HW_INIT] GPIO...");
	io_config.intr_type=GPIO_PIN_INTR_DISABLE;
	io_config.mode=GPIO_MODE_OUTPUT;
	io_config.pin_bit_mask=(1ULL << 17) | (1ULL << 16) | (1ULL << 4); // 17=Green LED; 16=Red LED; 4=Buzzer
	io_config.pull_down_en=1;
	io_config.pull_up_en=0;
	gpio_config(&io_config);

	oled_clear();
	oled_write_text("Initialization:\nWi-Fi");
	
	// Connect wifi now
	oled_clear();
	oled_write_text("Initialization:\nConnecting Wi-Fi");
	wifi_config_t wifi_config = {
		.sta = {
			.ssid = "",
			.password = ""
		}
	};
	memcpy((char *)wifi_config.sta.ssid, device_info.wifi_ssid, 17);
	memcpy((char *)wifi_config.sta.password, device_info.wifi_pass, 17);
	
	tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, "eie553-iot-board");
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

	oled_clear();
	oled_write_text("Initialization:\nAll Done");
	vTaskDelay(1100/portTICK_PERIOD_MS);

	smart_lock_logger_add_log(DEVICE_BOOTED, SYSTEM, NULL, 0);

	// Create required FreeRTOS Tasks
	xTaskCreate(&oled_task, "oled_task", 2048, NULL, 8, NULL);
	xTaskCreate(&smart_lock_task, "smart_lock_task", 2048, NULL, 8, NULL);
	xTaskCreate(&light_sensor_task, "light_sensor_task", 2048, NULL, 8, NULL);
	xTaskCreate(&ble_task, "ble_task", 4096, NULL, 8, NULL);

	while(1) {
	
		vTaskDelay(5000/portTICK_PERIOD_MS);
	}
}
