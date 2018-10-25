#include "smart_lock_logger.h"

static const char *TAG = "SMART_LOCK_LOGGER";

static smart_lock_logs_t logs;
static uint32_t next_event_id=0;
static SemaphoreHandle_t semaphore;

const char *NUL = "null";

static const char *get_log_source_name(smart_lock_log_source_t src) {
	switch(src) {
		case WIFI:
		return "WIFI";
		case BLE:
		return "BLE";
		case SYSTEM:
		return "SYSTEM";
		default:
		return "UNKNOWN";
	}
}

static const char *get_log_action_name(smart_lock_log_action_t act) {
	switch(act) {
		case LOCKED:
		return "LOCKED";
		case UNLOCKED:
		return "UNLOCKED";
		case LOGS_CLEARED:
		return "LOGS_CLEARED";
		case WEB_ADMIN_PASSWORD_CHANGED:
		return "WEB_ADMIN_PASSWORD_CHANGED";
		case LOCK_PIN_CODE_CHANGED:
		return "LOCK_PIN_CODE_CHANGED";
		case WEB_ADMIN_LOGGED_IN:
		return "WEB_ADMIN_LOGGED_IN";
		case WEB_ADMIN_LOGIN_FAIL:
		return "WEB_ADMIN_LOGIN_FAIL";
		case FAILED_UNLOCK_ATTEMPT:
		return "FAILED_UNLOCK_ATTEMPT";
		case DEVICE_DISABLED:
		return "DEVICE_DISABLED";
		case CONNECTED:
		return "CONNECTED";
		case DISCONNECTED:
		return "DISCONNECTED";
		case DEVICE_BOOTED:
		return "DEVICE_BOOTED";
		default:
		return "UNKNOWN";
	}
}

void smart_lock_logger_init() {
	uint16_t i;
	for(i=0; i<SMART_LOCK_LOGS_MAX_ENTRIES; i++) {
		logs.entries[i].extra_data=NULL;
		logs.entries[i].extra_data_len=0;
	}

	logs.filled_count=0;
	semaphore = xSemaphoreCreateMutex();
	if(semaphore == NULL) {
		ESP_LOGE(TAG, "Semaphore creation failed.");
		return;
	}
	ESP_LOGI(TAG, "Initialized.");
}


static void smart_lock_logger_free_single_log_extra_data(uint16_t entry) {
	if(logs.entries[entry].extra_data_len != 0) {
		ESP_LOGI(TAG, "Freeing entry %d, sze=%d", entry, logs.entries[entry].extra_data_len);
		free(logs.entries[entry].extra_data);
		logs.entries[entry].extra_data=NULL;
		logs.entries[entry].extra_data_len=0;
	}
}


void smart_lock_logger_add_log(smart_lock_log_action_t action, smart_lock_log_source_t source, char *extra_data, uint8_t extra_data_len) {
	uint16_t i;
	while(xSemaphoreTake(semaphore, (TickType_t)5) != pdTRUE);
	if(logs.filled_count == SMART_LOCK_LOGS_MAX_ENTRIES) {
		ESP_LOGW(TAG, "Logs are full, abandoning first logs");
		smart_lock_logger_free_single_log_extra_data(0);
		for(i=0; i<SMART_LOCK_LOGS_MAX_ENTRIES-1; i++) {
			logs.entries[i] = logs.entries[i+1];
		}
		logs.filled_count--;
	}
	logs.entries[logs.filled_count].event_id = next_event_id++;
	logs.entries[logs.filled_count].action = action;
	logs.entries[logs.filled_count].source = source;
	logs.entries[logs.filled_count].extra_data = extra_data;
	logs.entries[logs.filled_count].extra_data_len = extra_data_len;
	logs.filled_count++;
	xSemaphoreGive(semaphore);
}

void smart_lock_logger_clear_log() {
	uint16_t i;
	while(xSemaphoreTake(semaphore, (TickType_t)5) != pdTRUE);
	for(i=0; i<SMART_LOCK_LOGS_MAX_ENTRIES; i++) {
		smart_lock_logger_free_single_log_extra_data(i);
	}
	logs.filled_count=0;
	xSemaphoreGive(semaphore);
	ESP_LOGI(TAG, "Logs cleared.");
}

void smart_lock_logger_print_log() {
	uint16_t i;
	while(xSemaphoreTake(semaphore, (TickType_t)5) != pdTRUE);
	ESP_LOGI(TAG, "--SMART_LOCK_LOGGER_LOGS_START--");
	for(i=0; i<logs.filled_count; i++) {
		char *extra_data;
		if(logs.entries[i].extra_data == NULL) {
			extra_data = NUL;
		}else{
			extra_data = logs.entries[i].extra_data;
		}
		ESP_LOGI(TAG, "PRINT_LOG: ENT=%04d;EVT=%04d;SRC=%s;ACT=%s;DAT=%s", i, logs.entries[i].event_id, get_log_source_name(logs.entries[i].source), get_log_action_name(logs.entries[i].action), extra_data);
	}
	ESP_LOGI(TAG, "--SMART_LOCK_LOGGER_LOGS_END--");
	xSemaphoreGive(semaphore);
}

char *smart_lock_logger_get_log_json() {
	uint16_t i;
	char *out = malloc(1024);
	size_t out_sz = 1024;
	size_t written = 0;
	strcpy(out, "{\"logs\": [");
	written = strlen(out)+1;

	while(xSemaphoreTake(semaphore, (TickType_t)5) != pdTRUE);
	for(i=0; i<logs.filled_count; i++) {
		char entry_json[64]; // if extra data is too long (probably not, we may have corrupted json data)
		char *extra_data;
		uint8_t need_free=0;

		if(logs.entries[i].extra_data == NULL) {
			extra_data = NUL;
		}else{
			extra_data = escapeJSONChars(logs.entries[i].extra_data);
			need_free=1;
		}

		if(i != logs.filled_count-1) {
			snprintf(entry_json, 64, "[%d,%d,%d,\"%s\"],", logs.entries[i].event_id, logs.entries[i].source, logs.entries[i].action, extra_data);
			if(need_free == 1) free(extra_data);
		}else{
			snprintf(entry_json, 64, "[%d,%d,%d,\"%s\"]", logs.entries[i].event_id, logs.entries[i].source, logs.entries[i].action, extra_data);
			if(need_free == 1) free(extra_data);
		}

		if(written + strlen(entry_json) < out_sz) {
			out = realloc(out, out_sz+512);
			out_sz +=512;
		}
		strcat(out, entry_json);
		written += strlen(entry_json);
	}

	xSemaphoreGive(semaphore);


	if(written+2 < out_sz) {
		out = realloc(out, out_sz+2);
		out_sz += 2;
	}
	strcat(out, "]}");
	written += 2;

	return out;
}


