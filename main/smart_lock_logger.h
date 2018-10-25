#ifndef MAIN_SMART_LOCK_LOGGER_H_
#define MAIN_SMART_LOCK_LOGGER_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "iot_device_info.h"
#include "util.h"

#include "config.h"

typedef enum smart_lock_log_source {
	WIFI=0,
	BLE=1,
	SYSTEM=2
} smart_lock_log_source_t;

typedef enum smart_lock_log_action {
	LOCKED=0,
	UNLOCKED=1,
	LOGS_CLEARED=2,
	WEB_ADMIN_PASSWORD_CHANGED=3,
	LOCK_PIN_CODE_CHANGED=4,
	WEB_ADMIN_LOGGED_IN=5,
	WEB_ADMIN_LOGIN_FAIL=6,
	FAILED_UNLOCK_ATTEMPT=7,
	DEVICE_DISABLED=8,
	CONNECTED=9,
	DISCONNECTED=10,
	DEVICE_BOOTED=11
} smart_lock_log_action_t;

typedef struct smart_lock_log_entry {
	uint32_t event_id;
	smart_lock_log_action_t action;
	smart_lock_log_source_t source;
	char *extra_data;
	uint8_t extra_data_len;
} smart_lock_log_entry_t;

typedef struct smart_lock_logs {
	smart_lock_log_entry_t entries[SMART_LOCK_LOGS_MAX_ENTRIES];
	uint16_t filled_count;
} smart_lock_logs_t;

void smart_lock_logger_init();
void smart_lock_logger_add_log(smart_lock_log_action_t action, smart_lock_log_source_t source, char *extra_data, uint8_t extra_data_len);
void smart_lock_logger_clear_log();
void smart_lock_logger_print_log();
char *smart_lock_logger_get_log_json();

#endif /* MAIN_SMART_LOCK_LOGGER_H_ */
