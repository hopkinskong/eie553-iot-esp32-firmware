#ifndef MAIN_SMART_LOCK_TASK_H_
#define MAIN_SMART_LOCK_TASK_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "iot_device_info.h"

#include "config.h"

#include "oled.h"

extern iot_device_info_t device_info;
extern SemaphoreHandle_t device_info_semaphore;
extern EventGroupHandle_t oled_event_group;
extern const int OLED_NEED_UPDATE_BIT;

void smart_lock_task(void *pvParameters);

#endif /* MAIN_SMART_LOCK_TASK_H_ */
