#ifndef MAIN_WEBSERVER_H_
#define MAIN_WEBSERVER_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <esp_log.h>
#include <http_server.h>
#include "iot_device_info.h"
#include "util.h"
#include "session_manager.h"

#include "smart_lock_logger.h"

#include "esp_system.h"


#include "nvs.h"
#include "nvs_flash.h"

extern nvs_handle nvs;

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

extern iot_device_info_t device_info;
extern SemaphoreHandle_t device_info_semaphore;
extern EventGroupHandle_t oled_event_group;
extern const int OLED_NEED_UPDATE_BIT;

#endif /* MAIN_WEBSERVER_H_ */
