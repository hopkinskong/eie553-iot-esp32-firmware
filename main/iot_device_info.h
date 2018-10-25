#ifndef MAIN_IOT_DEVICE_INFO_H_
#define MAIN_IOT_DEVICE_INFO_H_

typedef struct iot_device_info {
	uint32_t ip; // 32-bit IP address
	uint8_t wifi_connected; // Wi-Fi Connected = 1; otherwise = 0
	uint8_t smart_lock_status; // Locked = 0; Unlocked = 1
	uint8_t brightness; // Brightness value of light sensor
	char device_id[13]; // 12 chars, + 1 null termination
	char pin[7]; // 6 chars, + 1 null termination
	uint8_t wrong_password_count; // How many wrong password entered in a row
	uint8_t device_disabled; // Is device disabled due to multiple wrong passwords entered
	uint8_t device_disabled_time_left; // How long the device will become enable again (if disabled)
	uint8_t device_unlocked_time_left; // How long the device will become locked again (if unlocked)
	uint8_t wrong_password; // User provided a long password (signal the buzzer)
	char webadmin_username[6]; // Web admin username (must be "admin")
	char webadmin_password[24]; // Web admin password
	char wifi_ssid[17];
	char wifi_pass[17];
} iot_device_info_t;



#endif /* MAIN_IOT_DEVICE_INFO_H_ */
