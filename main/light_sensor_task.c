#include "light_sensor_task.h"

static const char *TAG = "LIGHT_SEN_TASK";

void light_sensor_task(void *pvParameters) {
	ESP_LOGI(TAG, "Task starting up...");
	uint32_t adc_reading = 0, voltage = 0;
	int32_t brightness=0;
	while(1) {
		// Get ADC Reading
		adc_reading=0;
		for(int i=0; i<512; i++) {
			adc_reading += adc1_get_raw(ADC1_CHANNEL_4);
		}
		adc_reading /= 512;
		// Get ADC Voltage from reading
		voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_characteristics);
		
		// 1150mV=Dark; 3150mV=Bright
		// Assume Linear Relationship
		brightness=100*(voltage-1150);
		brightness/=(3150-1150);
		if(brightness > 100) brightness=100;
		if(brightness < 0) brightness=0;

		//ESP_LOGI(TAG, "%dmV (%d%%)", voltage, brightness);

		while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);
		if(device_info.brightness != brightness) {
			device_info.brightness=brightness;
			xEventGroupSetBits(oled_event_group, OLED_NEED_UPDATE_BIT); // Notify OLED Task to write to screen
		}
		xSemaphoreGive(device_info_semaphore);

		vTaskDelay(500 / portTICK_RATE_MS);
	}
}
