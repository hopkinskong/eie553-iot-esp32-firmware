#include "smart_lock_task.h"

static const char *TAG = "SMRT_LCK_TASK";

void smart_lock_task(void *pvParameters) {
	ESP_LOGI(TAG, "Task starting up...");
	uint8_t currentLockedState=0;
	uint8_t play_sound_type=0; // 0=not playing sound; 1=lock; 2=unlock; 3=wrong password

	uint32_t alreadyDisabledForSeq=0;
	uint32_t alreadyUnlockedForSeq=0;

	// Default is locked
	gpio_set_level(GPIO_NUM_16, 1);
	gpio_set_level(GPIO_NUM_17, 0);
	gpio_set_level(GPIO_NUM_4, 1);
	vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
	gpio_set_level(GPIO_NUM_4, 0);
	vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);

	while(1) {
		play_sound_type=0;

		while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);

		if(device_info.wrong_password == 1) {
			device_info.wrong_password=0;
			play_sound_type=3;
		}

		if(device_info.device_disabled == 1) {
			if(device_info.device_disabled_time_left == 0) {
				alreadyDisabledForSeq=0;
				device_info.device_disabled=0;
				xEventGroupSetBits(oled_event_group, OLED_NEED_UPDATE_BIT); // Notify OLED Task to write to screen
			}else{
				alreadyDisabledForSeq++;
				if(alreadyDisabledForSeq == 5) { // 200ms*5=1s
					alreadyDisabledForSeq=0;
					device_info.device_disabled_time_left--;
					xEventGroupSetBits(oled_event_group, OLED_NEED_UPDATE_BIT); // Notify OLED Task to write to screen
				}
			}
		}

		if(device_info.device_unlocked_time_left > 0) {
			alreadyUnlockedForSeq++;
			if(alreadyUnlockedForSeq == 5) {
				alreadyUnlockedForSeq=0;
				device_info.device_unlocked_time_left--;
				if(device_info.device_unlocked_time_left == 0) {
					device_info.smart_lock_status=0;
				}
				xEventGroupSetBits(oled_event_group, OLED_NEED_UPDATE_BIT); // Notify OLED Task to write to screen
			}
		}

		if(device_info.smart_lock_status == 0 && device_info.smart_lock_status != currentLockedState) { // unlocked->locked
			ESP_LOGI(TAG, "Status changed from unlocked->locked");
			currentLockedState=0;
			play_sound_type=1;
		}else if(device_info.smart_lock_status == 1 && device_info.smart_lock_status != currentLockedState){ // locked->unlocked
			ESP_LOGI(TAG, "Status changed from locked->unlocked");
			currentLockedState=1;
			play_sound_type=2;
		}

		xSemaphoreGive(device_info_semaphore);

		if(currentLockedState == 0) { // Locked, show RED LED
			gpio_set_level(GPIO_NUM_16, 1);
			gpio_set_level(GPIO_NUM_17, 0);
		}else{ // Unlocked, show GREEN LED
			gpio_set_level(GPIO_NUM_17, 1);
			gpio_set_level(GPIO_NUM_16, 0);
		}

		if(play_sound_type == 1) { // LOCK Sound (Single Beep)
			play_sound_type=0;
			gpio_set_level(GPIO_NUM_4, 1);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 0);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
		}else if(play_sound_type == 2) { // UNLOCK Sound (Five Beeps)
			play_sound_type=0;
			gpio_set_level(GPIO_NUM_4, 1);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 0);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 1);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 0);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 1);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 0);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 1);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 0);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 1);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 0);
			vTaskDelay(LOCK_BEEP_DURATION_MS/portTICK_PERIOD_MS);
		}else if(play_sound_type == 3) { // Wrong Password Sound (long, two beeps)
			play_sound_type=0;
			gpio_set_level(GPIO_NUM_4, 1);
			vTaskDelay(WRONG_PASSWORD_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 0);
			vTaskDelay(WRONG_PASSWORD_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 1);
			vTaskDelay(WRONG_PASSWORD_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			gpio_set_level(GPIO_NUM_4, 0);
			vTaskDelay(WRONG_PASSWORD_BEEP_DURATION_MS/portTICK_PERIOD_MS);
			
		}
		
		vTaskDelay(200/portTICK_PERIOD_MS);
	}
}
