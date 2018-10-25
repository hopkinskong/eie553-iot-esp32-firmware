#ifndef MAIN_BLE_TASK_H_
#define MAIN_BLE_TASK_H_

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

#include "smart_lock_logger.h"

#include "config.h"

#include <stdlib.h>
#include <string.h>

extern iot_device_info_t device_info;
extern SemaphoreHandle_t device_info_semaphore;
extern EventGroupHandle_t oled_event_group;
extern const int OLED_NEED_UPDATE_BIT;

void ble_task(void *pvParameters);

#endif /* MAIN_BLE_TASK_H_ */
