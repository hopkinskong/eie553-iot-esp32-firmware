#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

// Wi-Fi Settings
#undef  WIFI_SSID
#define WIFI_SSID "73mp31"
#undef  WIFI_PW
#define WIFI_PW "3x0duz@#$*____"

// Beep settings
#undef  LOCK_BEEP_DURATION_MS
#define LOCK_BEEP_DURATION_MS 32

#undef  WRONG_PASSWORD_BEEP_DURATION_MS
#define WRONG_PASSWORD_BEEP_DURATION_MS 110

// Lock down settings
#undef  LOCKDOWN_AFTER_WRONG_PASSWORD_NUM
#define LOCKDOWN_AFTER_WRONG_PASSWORD_NUM 3

#undef  LOCKDOWN_TIME_SECONDS
#define LOCKDOWN_TIME_SECONDS 30

// Unlocked->lock timeout
#undef  UNLOCKED_TIME_SECONDS
#define UNLOCKED_TIME_SECONDS 5

// Webadmin sesssions
#undef  MAX_WEBADMIN_SESSIONS
#define MAX_WEBADMIN_SESSIONS 8

// Smart Lock Logs
#undef  SMART_LOCK_LOGS_MAX_ENTRIES
#define SMART_LOCK_LOGS_MAX_ENTRIES 64

#endif /* MAIN_CONFIG_H_ */
