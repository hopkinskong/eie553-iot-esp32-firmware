#include "oled_task.h"

static const char *TAG = "OLED_TASK";

void oled_task(void *pvParameters) {
	char buf[256];
	char ip[16];
	const char *locked = "LOCKED";
	char *current_status;
	ESP_LOGI(TAG, "Task starting up...");
	while(1) {
		xEventGroupWaitBits(oled_event_group, OLED_NEED_UPDATE_BIT, true, true, portMAX_DELAY);
		//xEventGroupClearBits(oled_event_group, OLED_NEED_UPDATE_BIT);

		while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);

		oled_clear();
		if(device_info.device_disabled == 0) {
			if(device_info.smart_lock_status == 0) {
				current_status=(char *)locked;
			}else{
			char unlocked[16];
			snprintf(unlocked, 16, "UNLOCKED -%ds", device_info.device_unlocked_time_left);
			current_status=unlocked;
			}
		}else{
			char disabled[16];
			snprintf(disabled, 16, "DISABLED -%ds", device_info.device_disabled_time_left);
			current_status=disabled;
		}

		if(device_info.wifi_connected == 0) {
			snprintf(ip, 16, "DISCONNECTED");
		}else{
			snprintf(ip, 16, "%d.%d.%d.%d", (uint8_t)(device_info.ip), (uint8_t)(device_info.ip>>8), (uint8_t)(device_info.ip>>16), (uint8_t)(device_info.ip>>24));
		}

		snprintf(buf, 256, "IOT Smart Lock\n\nID:%s IP:\n%s\n\n>>%s\nBrightness:%d%%\nPin:%s", device_info.device_id, ip, current_status, device_info.brightness, device_info.pin);

		xSemaphoreGive(device_info_semaphore);
		
		oled_write_text(buf);
	}
}
