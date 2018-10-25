#ifndef MAIN_WIFI_CONFIG_BLE_TASK_H_
#define MAIN_WIFI_CONFIG_BLE_TASK_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "iot_device_info.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"

#include "config.h"

#include <stdlib.h>
#include <string.h>


#include "nvs.h"
#include "nvs_flash.h"

#include "oled.h"

extern iot_device_info_t device_info;
// No Semaphore is needed, as we are in single thread

extern nvs_handle nvs;

void wifi_config_ble_task(void *pvParameters);

/* Attributes State Machine */
enum
{
    IDX_SVC,

    IDX_CHAR_SSID,
    IDX_CHAR_SSID_VAL,

    IDX_CHAR_PASS,
    IDX_CHAR_PASS_VAL,

    HRS_IDX_NB,
};

#endif /* MAIN_WIFI_CONFIG_BLE_TASK_H_ */
