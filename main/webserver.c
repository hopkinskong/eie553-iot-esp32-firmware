#include "webserver.h"

#include "web_html_login_html.h"
#include "web_html_css_bootstrap_min_css.h"
#include "web_html_js_bootstrap_min_js.h"
#include "web_html_js_jquery_3_3_1_min_js.h"
#include "web_html_main_html.h"
#include "web_html_js_main_js.h"

#define WRONG_USERNAME_PASSWORD_PAGE "<span style=\"color:red; font-weight:bold;\">Incorrect username/password, please try again.</span><br /><a href=\"/\">Back</a>"
#define LOADING "Loading..."
#define LOGOUT_MSG "You have been logged out.<br /><a href=\"/\">Back</a>"
#define ERROR_PW_TOO_LONG "<span style=\"color:red; font-weight:bold;\">Password too long, maximum number of characters is 31.</span><br /><a href=\"/main\">Back</a>"
#define ERROR_PW_TOO_SHORT "<span style=\"color:red; font-weight:bold;\">Password too short, minimum number of characters is 5.</span><br /><a href=\"/main\">Back</a>"
#define PASSWORD_CHANGED_MSG "Password changed, please re-login now.<br /><a href=\"/\">Back</a>"
#define ERROR_NEW_PIN_LENGTH_INCORRECT "<span style=\"color:red; font-weight:bold;\">New PIN Code should be only 6 numeric characters.</span><br /><a href=\"/main\">Back</a>"
#define ERROR_NEW_PIN_INVALID_CHARS "<span style=\"color:red; font-weight:bold;\">New PIN Code should be containing numeric characters.</span><br /><a href=\"/main\">Back</a>"
#define PIN_CHANGED_MSG "Pin code has been changed.<br /><a href=\"/main\">Back</a>"
#define ERROR_SSID_TOO_LONG "<span style=\"color:red; font-weight:bold;\">SSID too long, maximum number of characters is 16.</span><br /><a href=\"/main\">Back</a>"
#define ERROR_SSID_EMPTY "<span style=\"color:red; font-weight:bold;\">SSID is empty.</span><br /><a href=\"/main\">Back</a>"
#define ERROR_WIFI_PASS_TOO_LONG "<span style=\"color:red; font-weight:bold;\">Wi-Fi Password too long, maximum number of characters is 16.</span><br /><a href=\"/main\">Back</a>"
#define ERROR_WIFI_PASS_TOO_SHORT "<span style=\"color:red; font-weight:bold;\">Wi-Fi Password too short, minimum number of characters is 8.</span><br /><a href=\"/main\">Back</a>"
#define WIFI_CONFIG_CHANGED_MSG "Wi-Fi Configurations has been changed.<br /><strong>The device will now reboot.</strong>"
#define LOG_CLEARED_MSG "Logs have been cleared.<br /><a href=\"/main\">Back</a>"

#define JSON_ERROR_REQ_TOO_LARGE "{ \"error\": \"REQ_TOO_LARGE\" }"
#define JSON_ERROR_NO_AUTH "{ \"error\": \"NO_AUTH\" }"
#define JSON_ERROR_UNKNOWN_CMD "{ \"error\": \"UNKNOWN_CMD\" }"
#define JSON_LOCK_OK "{\"lock\": \"OK\"}"
#define JSON_UNLOCK_OK "{\"unlock\": \"OK\"}"

static const char *TAG = "WEBSRV";

static esp_err_t get_sess_id(httpd_req_t *req, char *sess_id) {
	char *buf;
	size_t buf_len;
	ESP_LOGI(TAG, "get_sess_id: start");
	buf_len = httpd_req_get_hdr_value_len(req, "Cookie") + 1;
	if(buf_len > 1) {
		buf = malloc(buf_len);
		/* Copy null terminated value string into buffer */
		if (httpd_req_get_hdr_value_str(req, "Cookie", buf, buf_len) == ESP_OK) {
			ESP_LOGI(TAG, "Found header => Cookie: %s", buf);
			if(strlen(buf) == 23) {	// Format: IOT_SESS=XXXXXXXXXXXXXX
				if(strncmp(buf, "IOT_SESS=XXXXXXXXXXXXXX", 9) == 0) {
					uint8_t i;
					for(i=0; i<14; i++) {
						sess_id[i]=buf[9+i];
					}
					sess_id[14] = '\0';
					ESP_LOGI(TAG, "User provided session id=%s", sess_id);
					free(buf);
					return ESP_OK;	
				}else{
					free(buf);
					return ESP_FAIL;
				}
			}else{
				free(buf);
				return ESP_FAIL;
			}
		}else{
			free(buf);
			return ESP_FAIL;
		}
	}else{
		return ESP_FAIL;
	}
}

// Page: /api {Need Authentication}
static esp_err_t api_get_handler(httpd_req_t *req) {
	char sess_id[15];
	if(get_sess_id(req, sess_id) == ESP_OK) {
		if(check_sesssion(sess_id) == SESSION_EXISTS) {
			char buf[128];
			char cmd[32];
			size_t len = httpd_req_get_url_query_len(req) + 1;

			if(len <= 128 && len > 1) {
				memset(buf, 0x00, 128);
				if (httpd_req_get_url_query_str(req, buf, len) == ESP_OK) {
					ESP_LOGI(TAG, "Found URL query => %s", buf);
					if (httpd_query_key_value(buf, "cmd", cmd, sizeof(cmd)) == ESP_OK) {
						ESP_LOGI(TAG, "Found URL query parameter => cmd=%s", cmd);
						if(strcmp(cmd, "status") == 0) {
							char status[512];
							const char *locked = "LOCKED";
							char ip[16];
							char *current_status;

							while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);

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
							snprintf(status, 512, "{\"id\":\"%s\",\"ip\":\"%s\",\"lock\":\"%s\",\"pin\":\"%s\",\"bright\":\"%d%%\"}", device_info.device_id, ip, current_status, device_info.pin, device_info.brightness);
							xSemaphoreGive(device_info_semaphore);

							httpd_resp_set_hdr(req, "Content-Type", "application/json");
							httpd_resp_send(req, status, strlen(status));
						}else if(strcmp(cmd, "log") == 0) {
							char *logs = smart_lock_logger_get_log_json();
							httpd_resp_set_hdr(req, "Content-Type", "application/json");
							httpd_resp_send(req, logs, strlen(logs));
							free(logs);
						}else if(strcmp(cmd, "lock") == 0) {
							ESP_LOGI(TAG, "Lock from web admin");
							smart_lock_logger_add_log(LOCKED, WIFI, NULL, 0);

							while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);
							device_info.smart_lock_status=0;
							device_info.device_unlocked_time_left=0;
							xEventGroupSetBits(oled_event_group, OLED_NEED_UPDATE_BIT); // Notify OLED Task to write to screen
							xSemaphoreGive(device_info_semaphore);

							httpd_resp_set_hdr(req, "Content-Type", "application/json");
							httpd_resp_send(req, JSON_LOCK_OK, strlen(JSON_LOCK_OK));
						}else if(strcmp(cmd, "unlock") == 0) {
							ESP_LOGI(TAG, "Unlock from web admin");
							smart_lock_logger_add_log(UNLOCKED, WIFI, NULL, 0);

							while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);
							device_info.smart_lock_status=1;
							device_info.device_unlocked_time_left=UNLOCKED_TIME_SECONDS;
							xEventGroupSetBits(oled_event_group, OLED_NEED_UPDATE_BIT); // Notify OLED Task to write to screen
							xSemaphoreGive(device_info_semaphore);

							httpd_resp_set_hdr(req, "Content-Type", "application/json");
							httpd_resp_send(req, JSON_UNLOCK_OK, strlen(JSON_UNLOCK_OK));
						}else if(strcmp(cmd, "wifi_conf") == 0) {
							char wifi_conf[256];
							while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);
							char *json_wifi_ssid = escapeJSONChars(device_info.wifi_ssid);
							xSemaphoreGive(device_info_semaphore);
							snprintf(wifi_conf, 256, "{\"ssid\": \"%s\"}", json_wifi_ssid);
							free(json_wifi_ssid);

							httpd_resp_set_hdr(req, "Content-Type", "application/json");
							httpd_resp_send(req, wifi_conf, strlen(wifi_conf));
						}else{
							ESP_LOGW(TAG, "API: Unknown cmd: %s", cmd);
							httpd_resp_set_hdr(req, "Content-Type", "application/json");
							httpd_resp_send(req, JSON_ERROR_UNKNOWN_CMD, strlen(JSON_ERROR_UNKNOWN_CMD));
						}
					}else{
						ESP_LOGW(TAG, "API: No cmd provided");
						httpd_resp_set_hdr(req, "Content-Type", "application/json");
						httpd_resp_send(req, JSON_ERROR_UNKNOWN_CMD, strlen(JSON_ERROR_UNKNOWN_CMD));
					}
				}else{
					ESP_LOGW(TAG, "API: Unable to get query string");
					httpd_resp_set_hdr(req, "Content-Type", "application/json");
					httpd_resp_send(req, JSON_ERROR_UNKNOWN_CMD, strlen(JSON_ERROR_UNKNOWN_CMD));
				}
			}else if(len <= 1) { // Query too small
				ESP_LOGW(TAG, "API: Req too small");
				httpd_resp_set_hdr(req, "Content-Type", "application/json");
				httpd_resp_send(req, JSON_ERROR_UNKNOWN_CMD, strlen(JSON_ERROR_UNKNOWN_CMD));
			}else{	// Query too large
				ESP_LOGW(TAG, "API: Req too large");
				httpd_resp_set_hdr(req, "Content-Type", "application/json");
				httpd_resp_send(req, JSON_ERROR_REQ_TOO_LARGE, strlen(JSON_ERROR_REQ_TOO_LARGE));
			}
		}else{
			// Unauthorized
			ESP_LOGW(TAG, "User provided invalid session id for /api");
			httpd_resp_set_hdr(req, "Content-Type", "application/json");
			httpd_resp_send(req, JSON_ERROR_NO_AUTH, strlen(JSON_ERROR_NO_AUTH));
		}
	}else{
		// Unauthorized
		ESP_LOGW(TAG, "Unable to obtain session id for /api");
		httpd_resp_set_hdr(req, "Content-Type", "application/json");
		httpd_resp_send(req, JSON_ERROR_NO_AUTH, strlen(JSON_ERROR_NO_AUTH));
	}
	return ESP_OK;
}
static httpd_uri_t api_page = {
	.uri = "/api",
	.method = HTTP_GET,
	.handler = api_get_handler,
	.user_ctx = NULL
};

// Page: /logout {Need Authentication}
static esp_err_t logout_get_handler(httpd_req_t *req) {
	char sess_id[15];
	if(get_sess_id(req, sess_id) == ESP_OK) {
		if(check_sesssion(sess_id) == SESSION_EXISTS) {
			del_session(sess_id);
			httpd_resp_set_hdr(req, "Content-Type", "text/html");
			httpd_resp_send(req, LOGOUT_MSG, strlen(LOGOUT_MSG));
		}else{
			// Unauthorized
			ESP_LOGW(TAG, "User provided invalid session id for /logout");
			httpd_resp_set_status(req, "302");
			httpd_resp_set_hdr(req, "Location", "/");
			httpd_resp_send(req, LOADING, strlen(LOADING));
		}
	}else{
		// Unauthorized
		ESP_LOGW(TAG, "Unable to obtain session id for /logout");
		httpd_resp_set_status(req, "302");
		httpd_resp_set_hdr(req, "Location", "/");
		httpd_resp_send(req, LOADING, strlen(LOADING));
	}
	return ESP_OK;
}
static httpd_uri_t logout_page = {
	.uri = "/logout",
	.method = HTTP_GET,
	.handler = logout_get_handler,
	.user_ctx = NULL
};

// Page: /modify_webadmin_password [POST] {Need Authentication}
static esp_err_t modify_webadmin_password_post_handler(httpd_req_t *req) {
	char sess_id[15];
	if(get_sess_id(req, sess_id) == ESP_OK) {
		if(check_sesssion(sess_id) == SESSION_EXISTS) {
			char buf[128];
			char password[73]; // 24*3+1=73, URL Encode max 3 chars (%XX)->1 char, +1 for NULL
			size_t len;
			esp_err_t err;
			
			memset(buf, 0x00, 128);
			if(req->content_len > 128) {
				len=128;
			}else{
				len=req->content_len;
			}

			// Get POST value
			if(httpd_req_recv(req, buf, len) < 0) {
				return ESP_FAIL;
			}

			memset(password, 0x00, 73);
			err = httpd_query_key_value(buf, "password", password, sizeof(password));
			if (err == ESP_OK || err == ESP_ERR_HTTPD_RESULT_TRUNC) { // Allow Truncated Data pass, they must be len > 23, we will reject them
				URLDecode(password, 73);
				if(strlen(password) < 5) {
					ESP_LOGW(TAG, "User provided a very short password, rejected.");
					httpd_resp_set_hdr(req, "Content-Type", "text/html");
					httpd_resp_send(req, ERROR_PW_TOO_SHORT, strlen(ERROR_PW_TOO_SHORT));
				}else if(strlen(password) > 23) {
					ESP_LOGW(TAG, "User provided a very long password, rejected.");
					httpd_resp_set_hdr(req, "Content-Type", "text/html");
					httpd_resp_send(req, ERROR_PW_TOO_LONG, strlen(ERROR_PW_TOO_LONG));
				}else{
					ESP_LOGI(TAG, "Changing Web Admin Password to: %s", password);
					smart_lock_logger_add_log(WEB_ADMIN_PASSWORD_CHANGED, WIFI, NULL, 0);
					// Invalidate all sessions
					init_session_manager();
					while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);
					strncpy(device_info.webadmin_password, password, strlen(password));
					device_info.webadmin_password[strlen(password)] = '\0';
					xSemaphoreGive(device_info_semaphore);
					if(nvs_set_str(nvs, "wa_passwd", password) != ESP_OK) {
						ESP_LOGE(TAG, "Unable to set new Web Admin password to NVS");
					}else{
						ESP_LOGI(TAG, "New Web Admin password has been written to NVS");
					}
					httpd_resp_set_hdr(req, "Content-Type", "text/html");
					httpd_resp_send(req, PASSWORD_CHANGED_MSG, strlen(PASSWORD_CHANGED_MSG));
				}
			}else{
				ESP_LOGE(TAG, "Error: Unable to get new password");
				return ESP_FAIL;
			}
		}else{
			// Unauthorized
			ESP_LOGW(TAG, "User provided invalid session id for /modify_admin_password");
			httpd_resp_set_status(req, "302");
			httpd_resp_set_hdr(req, "Location", "/");
			httpd_resp_send(req, LOADING, strlen(LOADING));
		}
	}else{
		// Unauthorized
		ESP_LOGW(TAG, "Unable to obtain session id for /modify_admin_password");
		httpd_resp_set_status(req, "302");
		httpd_resp_set_hdr(req, "Location", "/");
		httpd_resp_send(req, LOADING, strlen(LOADING));
	}
	return ESP_OK;
}
static httpd_uri_t modify_webadmin_password_post_page = {
	.uri = "/modify_webadmin_password",
	.method = HTTP_POST,
	.handler = modify_webadmin_password_post_handler,
	.user_ctx = NULL
};

// Page: /modify_lock_pin [POST] {Need Authentication}
static esp_err_t modify_lock_pin_post_handler(httpd_req_t *req) {
	char sess_id[15];
	if(get_sess_id(req, sess_id) == ESP_OK) {
		if(check_sesssion(sess_id) == SESSION_EXISTS) {
			char buf[128];
			char pin[8];
			size_t len;
			esp_err_t err;
			
			memset(buf, 0x00, 128);
			if(req->content_len > 128) {
				len=128;
			}else{
				len=req->content_len;
			}

			// Get POST value
			if(httpd_req_recv(req, buf, len) < 0) {
				return ESP_FAIL;
			}

			memset(pin, 0x00, 8);
			err = httpd_query_key_value(buf, "new_pin", pin, sizeof(pin));
			if (err == ESP_OK || err == ESP_ERR_HTTPD_RESULT_TRUNC) { // Allow Truncated Data pass, their length must not be 6 (must be 7, as buf size=8) if truncated, we will reject them
				if(strlen(pin) != 6) {
					ESP_LOGW(TAG, "Length of new PIN is not 6, rejected.");
					httpd_resp_set_hdr(req, "Content-Type", "text/html");
					httpd_resp_send(req, ERROR_NEW_PIN_LENGTH_INCORRECT, strlen(ERROR_NEW_PIN_LENGTH_INCORRECT));
				}else{
					uint8_t i;
					uint8_t invalid_char_found=0;
					for(i=0; i<5; i++) {
						if(pin[i] != '0' && pin[i] != '1' && pin[i] != '2' && pin[i] != '3' && pin[i] != '4' && pin[i] != '5' && pin[i] != '6' && pin[i] != '7' && pin[i] != '8' && pin[i] != '9') {
							invalid_char_found=1;
							break;
						}
					}

							ESP_LOGI(TAG, "New PIN has been written to NVS");
					if(invalid_char_found == 1) {
						ESP_LOGW(TAG, "Invalid, non-numeric character(s) found in new PIN, rejected.");
						httpd_resp_set_hdr(req, "Content-Type", "text/html");
						httpd_resp_send(req, ERROR_NEW_PIN_INVALID_CHARS, strlen(ERROR_NEW_PIN_INVALID_CHARS));
					}else{
						ESP_LOGI(TAG, "Changing Lock Pin to: %s", pin);
						smart_lock_logger_add_log(LOCK_PIN_CODE_CHANGED, WIFI, NULL, 0);
						while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);
						strncpy(device_info.pin, pin, strlen(pin));
						device_info.pin[strlen(pin)] = '\0';
						xEventGroupSetBits(oled_event_group, OLED_NEED_UPDATE_BIT); // Notify OLED Task to write to screen
						xSemaphoreGive(device_info_semaphore);
						if(nvs_set_str(nvs, "pin", pin) != ESP_OK) {
							ESP_LOGE(TAG, "Unable to set new PIN to NVS");
						}else{
							ESP_LOGI(TAG, "New PIN has been written to NVS");
						}
						httpd_resp_set_hdr(req, "Content-Type", "text/html");
						httpd_resp_send(req, PIN_CHANGED_MSG, strlen(PIN_CHANGED_MSG));
					}
				}
			}else{
				ESP_LOGE(TAG, "Error: Unable to get new PIN");
				return ESP_FAIL;
			}
		}else{
			// Unauthorized
			ESP_LOGW(TAG, "User provided invalid session id for /modify_lock_pin");
			httpd_resp_set_status(req, "302");
			httpd_resp_set_hdr(req, "Location", "/");
			httpd_resp_send(req, LOADING, strlen(LOADING));
		}
	}else{
		// Unauthorized
		ESP_LOGW(TAG, "Unable to obtain session id for /modify_lock_pin");
		httpd_resp_set_status(req, "302");
		httpd_resp_set_hdr(req, "Location", "/");
		httpd_resp_send(req, LOADING, strlen(LOADING));
	}
	return ESP_OK;
}
static httpd_uri_t modify_lock_pin_post_page = {
	.uri = "/modify_lock_pin",
	.method = HTTP_POST,
	.handler = modify_lock_pin_post_handler,
	.user_ctx = NULL
};

// Page: /modify_wifi_config [POST] {Need Authentication}
static esp_err_t modify_wifi_config_post_handler(httpd_req_t *req) {
	char sess_id[15];
	if(get_sess_id(req, sess_id) == ESP_OK) {
		if(check_sesssion(sess_id) == SESSION_EXISTS) {
			char buf[192];
			char wifi_ssid[55]; // 18*3+1=55, URL Encode max 3 chars (%XX)->1 char, +1 for NULL
			char wifi_pass[55]; // 18*3+1=55, URL Encode max 3 chars (%XX)->1 char, +1 for NULL
			size_t len;
			esp_err_t err;
			
			memset(buf, 0x00, 192);
			if(req->content_len > 192) {
				len=192;
			}else{
				len=req->content_len;
			}

			// Get POST value
			if(httpd_req_recv(req, buf, len) < 0) {
				return ESP_FAIL;
			}

			memset(wifi_ssid, 0x00, 55);
			err = httpd_query_key_value(buf, "new_ssid", wifi_ssid, sizeof(wifi_ssid));
			if (err == ESP_OK || err == ESP_ERR_HTTPD_RESULT_TRUNC) { // Allow Truncated Data pass, they must be len > 16, we will reject them
				URLDecode(wifi_ssid, 55);
				if(strlen(wifi_ssid) > 16) {
					ESP_LOGW(TAG, "Length of new SSID > 16, rejected.");
					httpd_resp_set_hdr(req, "Content-Type", "text/html");
					httpd_resp_send(req, ERROR_SSID_TOO_LONG, strlen(ERROR_SSID_TOO_LONG));
				}else if(strlen(wifi_ssid) == 0) {
					ESP_LOGW(TAG, "Length of new SSID = 0, rejected.");
					httpd_resp_set_hdr(req, "Content-Type", "text/html");
					httpd_resp_send(req, ERROR_SSID_EMPTY, strlen(ERROR_SSID_EMPTY));
				}else{
					memset(wifi_pass, 0x00, 55);
					err = httpd_query_key_value(buf, "new_wifi_pass", wifi_pass, sizeof(wifi_pass));
					if (err == ESP_OK || err == ESP_ERR_HTTPD_RESULT_TRUNC) { // Allow Truncated Data pass, they must be len > 16, we will reject them
						URLDecode(wifi_pass, 55);
						if(strlen(wifi_pass) > 16) {
							ESP_LOGW(TAG, "Length of new Wi-Fi Password > 16, rejected.");
							httpd_resp_set_hdr(req, "Content-Type", "text/html");
							httpd_resp_send(req, ERROR_WIFI_PASS_TOO_LONG, strlen(ERROR_WIFI_PASS_TOO_LONG));
						}else if(strlen(wifi_pass) < 8) {
							ESP_LOGW(TAG, "Length of new Wi-Fi Password < 8, rejected.");
							httpd_resp_set_hdr(req, "Content-Type", "text/html");
							httpd_resp_send(req, ERROR_WIFI_PASS_TOO_SHORT, strlen(ERROR_WIFI_PASS_TOO_SHORT));
						}else{
							ESP_LOGI(TAG, "Changing Wifi Config, new SSID: %s, password HIDDEN", wifi_ssid);
							while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);
							strncpy(device_info.wifi_ssid, wifi_ssid, strlen(wifi_ssid));
							device_info.wifi_ssid[strlen(wifi_ssid)] = '\0';
							strncpy(device_info.wifi_pass, wifi_pass, strlen(wifi_pass));
							device_info.wifi_pass[strlen(wifi_pass)] = '\0';
							xSemaphoreGive(device_info_semaphore);
							if(nvs_set_str(nvs, "ssid", wifi_ssid) != ESP_OK) {
								ESP_LOGE(TAG, "Unable to set new SSID to NVS");
							}else{
								ESP_LOGI(TAG, "New SSID has been written to NVS");
							}
							if(nvs_set_str(nvs, "wifi_pw", wifi_pass) != ESP_OK) {
								ESP_LOGE(TAG, "Unable to set new Wi-Fi password to NVS");
							}else{
								ESP_LOGI(TAG, "New Wi-Fi password has been written to NVS");
							}
							httpd_resp_set_hdr(req, "Content-Type", "text/html");
							httpd_resp_send(req, WIFI_CONFIG_CHANGED_MSG, strlen(WIFI_CONFIG_CHANGED_MSG));
							vTaskDelay(500/portTICK_PERIOD_MS);
							esp_restart();
						}
					}else{
						ESP_LOGE(TAG, "Error: Unable to get new Wi-Fi Password");
						return ESP_FAIL;
					}
				}
			}else{
				ESP_LOGE(TAG, "Error: Unable to get new SSID");
				return ESP_FAIL;
			}
			
		}else{
			// Unauthorized
			ESP_LOGW(TAG, "User provided invalid session id for /modify_wifi_config");
			httpd_resp_set_status(req, "302");
			httpd_resp_set_hdr(req, "Location", "/");
			httpd_resp_send(req, LOADING, strlen(LOADING));
		}
	}else{
		// Unauthorized
		ESP_LOGW(TAG, "Unable to obtain session id for /modify_wifi_config");
		httpd_resp_set_status(req, "302");
		httpd_resp_set_hdr(req, "Location", "/");
		httpd_resp_send(req, LOADING, strlen(LOADING));
	}
	return ESP_OK;
}
static httpd_uri_t modify_wifi_config_post_page = {
	.uri = "/modify_wifi_config",
	.method = HTTP_POST,
	.handler = modify_wifi_config_post_handler,
	.user_ctx = NULL
};


// Page: /clear_logs {Need Authentication}
static esp_err_t clear_logs_get_handler(httpd_req_t *req) {
	char sess_id[15];
	if(get_sess_id(req, sess_id) == ESP_OK) {
		if(check_sesssion(sess_id) == SESSION_EXISTS) {
			smart_lock_logger_clear_log();
			smart_lock_logger_add_log(LOGS_CLEARED, WIFI, NULL, 0);
			httpd_resp_set_hdr(req, "Content-Type", "text/html");
			httpd_resp_send(req, LOG_CLEARED_MSG, strlen(LOG_CLEARED_MSG));
		}else{
			// Unauthorized
			ESP_LOGW(TAG, "User provided invalid session id for /clear_logs");
			httpd_resp_set_status(req, "302");
			httpd_resp_set_hdr(req, "Location", "/");
			httpd_resp_send(req, LOADING, strlen(LOADING));
		}
	}else{
		// Unauthorized
		ESP_LOGW(TAG, "Unable to obtain session id for /clear_logs");
		httpd_resp_set_status(req, "302");
		httpd_resp_set_hdr(req, "Location", "/");
		httpd_resp_send(req, LOADING, strlen(LOADING));
	}
	return ESP_OK;
}
static httpd_uri_t clear_logs_get_page = {
	.uri = "/clear_logs",
	.method = HTTP_GET,
	.handler = clear_logs_get_handler,
	.user_ctx = NULL
};

// Page: / (login.html)
static esp_err_t index_get_handler(httpd_req_t *req) {
	httpd_resp_set_hdr(req, "Content-Type", "text/html");
	httpd_resp_send(req, web_html_login_html, sizeof(web_html_login_html));
	return ESP_OK;
}
static httpd_uri_t index_page = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = index_get_handler,
	.user_ctx = NULL
};

// Page: /main (main.html) {Need Authentication}
static esp_err_t main_get_handler(httpd_req_t *req) {
	char sess_id[15];
	if(get_sess_id(req, sess_id) == ESP_OK) {
		if(check_sesssion(sess_id) == SESSION_EXISTS) {
			httpd_resp_set_hdr(req, "Content-Type", "text/html");
			httpd_resp_send(req, web_html_main_html, sizeof(web_html_main_html));
		}else{
			// Unauthorized
			ESP_LOGW(TAG, "User provided invalid session id for /main");
			httpd_resp_set_status(req, "302");
			httpd_resp_set_hdr(req, "Location", "/");
			httpd_resp_send(req, LOADING, strlen(LOADING));
		}
	}else{
		// Unauthorized
		ESP_LOGW(TAG, "Unable to obtain session id for /main");
		httpd_resp_set_status(req, "302");
		httpd_resp_set_hdr(req, "Location", "/");
		httpd_resp_send(req, LOADING, strlen(LOADING));
	}
	return ESP_OK;
}
static httpd_uri_t main_page = {
	.uri = "/main",
	.method = HTTP_GET,
	.handler = main_get_handler,
	.user_ctx = NULL
};

// Page: /js/main.js {Need Authentication}
static esp_err_t main_js_get_handler(httpd_req_t *req) {
	char sess_id[15];
	if(get_sess_id(req, sess_id) == ESP_OK) {
		if(check_sesssion(sess_id) == SESSION_EXISTS) {		
			httpd_resp_set_hdr(req, "Content-Type", "text/javascript");
			httpd_resp_send(req, web_html_js_main_js, sizeof(web_html_js_main_js));
		}else{
			// Unauthorized
			ESP_LOGW(TAG, "User provided invalid session id for /js/main.js");
			httpd_resp_set_status(req, "302");
			httpd_resp_set_hdr(req, "Location", "/");
			httpd_resp_send(req, LOADING, strlen(LOADING));
		}
	}else{
		// Unauthorized
		ESP_LOGW(TAG, "Unable to obtain session id for /js/main.js");
		httpd_resp_set_status(req, "302");
		httpd_resp_set_hdr(req, "Location", "/");
		httpd_resp_send(req, LOADING, strlen(LOADING));
	}
	return ESP_OK;
}
static httpd_uri_t main_js_page = {
	.uri = "/js/main.js",
	.method = HTTP_GET,
	.handler = main_js_get_handler,
	.user_ctx = NULL
};

// Page: / (login.html) [POST]
static esp_err_t index_post_handler(httpd_req_t *req) {
	char buf[128];
	char username[32];
	char password[32];
	size_t len;
	
	memset(buf, 0x00, 128);
	if(req->content_len > 128) {
		len=128;
	}else{
		len=req->content_len;
	}

	// Get POST value
	if(httpd_req_recv(req, buf, len) < 0) {
		return ESP_FAIL;
	}

	memset(username, 0x00, 32);
	if (httpd_query_key_value(buf, "username", username, sizeof(username)) == ESP_OK) {
		ESP_LOGI(TAG, "Username: %s", username);
	}
	memset(password, 0x00, 32);
	if (httpd_query_key_value(buf, "password", password, sizeof(password)) == ESP_OK) {
		ESP_LOGI(TAG, "Password: %s", password);
	}

	while(xSemaphoreTake(device_info_semaphore, (TickType_t)5) != pdTRUE);
	if(strcmp(device_info.webadmin_username, username) == 0 && strcmp(device_info.webadmin_password, password) == 0) {
		// Username and password correct, authenticated
		char sess_id[16];
		char cookie[48];
		char *a, *b, *c;
		a=getRandomString();
		b=getRandomPin();
		c=getRandomString();
		snprintf(sess_id, 16, "%s%s%s", a, b, c);
		free(a);
		free(b);
		free(c);
		ESP_LOGI(TAG, "Webadmin: User logged in: %s", sess_id);
		smart_lock_logger_add_log(WEB_ADMIN_LOGGED_IN, WIFI, NULL, 0);
		add_sesssion(sess_id);
		snprintf(cookie, 48, "IOT_SESS=%s; HttpOnly; Path=/", sess_id);
		httpd_resp_set_status(req, "302");
		httpd_resp_set_hdr(req, "Set-Cookie", cookie);
		httpd_resp_set_hdr(req, "Location", "/main");
		httpd_resp_send(req, LOADING, strlen(LOADING));
	}else{
		ESP_LOGI(TAG, "Webadmin: User supplied wrong password");
		smart_lock_logger_add_log(WEB_ADMIN_LOGIN_FAIL, WIFI, NULL, 0);
		httpd_resp_set_hdr(req, "Content-Type", "text/html");
		httpd_resp_send(req, WRONG_USERNAME_PASSWORD_PAGE, strlen(WRONG_USERNAME_PASSWORD_PAGE));
	}
	xSemaphoreGive(device_info_semaphore);

	
	return ESP_OK;
}
static httpd_uri_t index_post_page = {
	.uri = "/",
	.method = HTTP_POST,
	.handler = index_post_handler,
	.user_ctx = NULL
};

// Page: /css/bootstrap.min.css
static esp_err_t bootstrap_css_get_handler(httpd_req_t *req) {
	httpd_resp_set_hdr(req, "Content-Type", "text/css");
	httpd_resp_send(req, web_html_css_bootstrap_min_css, sizeof(web_html_css_bootstrap_min_css));
	return ESP_OK;
}
static httpd_uri_t bootstrap_css_page = {
	.uri = "/css/bootstrap.min.css",
	.method = HTTP_GET,
	.handler = bootstrap_css_get_handler,
	.user_ctx = NULL
};

// Page: /js/bootstrap.min.js
static esp_err_t bootstrap_js_get_handler(httpd_req_t *req) {
	httpd_resp_set_hdr(req, "Content-Type", "text/javascript");
	httpd_resp_send(req, web_html_js_bootstrap_min_js, sizeof(web_html_js_bootstrap_min_js));
	return ESP_OK;
}
static httpd_uri_t bootstrap_js_page = {
	.uri = "/js/bootstrap.min.js",
	.method = HTTP_GET,
	.handler = bootstrap_js_get_handler,
	.user_ctx = NULL
};

// Page: /js/jquery-3.3.1.min.js
static esp_err_t jquery_js_get_handler(httpd_req_t *req) {
	httpd_resp_set_hdr(req, "Content-Type", "text/javascript");
	httpd_resp_send(req, web_html_js_jquery_3_3_1_min_js, sizeof(web_html_js_jquery_3_3_1_min_js));
	return ESP_OK;
}
static httpd_uri_t jquery_js_page = {
	.uri = "/js/jquery-3.3.1.min.js",
	.method = HTTP_GET,
	.handler = jquery_js_get_handler,
	.user_ctx = NULL
};

httpd_handle_t start_webserver() {
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.max_uri_handlers=14;

	init_session_manager();

	// Start the server
	ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
	if(httpd_start(&server, &config) == ESP_OK) {
		ESP_LOGI(TAG, "Server started, registering URIs");
		httpd_register_uri_handler(server, &index_page);
		httpd_register_uri_handler(server, &index_post_page);
		httpd_register_uri_handler(server, &bootstrap_css_page);
		httpd_register_uri_handler(server, &bootstrap_js_page);
		httpd_register_uri_handler(server, &jquery_js_page);
		httpd_register_uri_handler(server, &main_page);
		httpd_register_uri_handler(server, &main_js_page);
		httpd_register_uri_handler(server, &modify_webadmin_password_post_page);
		httpd_register_uri_handler(server, &logout_page);
		httpd_register_uri_handler(server, &api_page);
		httpd_register_uri_handler(server, &modify_lock_pin_post_page);
		httpd_register_uri_handler(server, &modify_wifi_config_post_page);
		httpd_register_uri_handler(server, &clear_logs_get_page);
		return server;
	}

	ESP_LOGE(TAG, "Unable to start the server");
	return NULL;
}

void stop_webserver(httpd_handle_t server) {
	// Stop the HTTPD server
	ESP_LOGI(TAG, "Stopping server");
	httpd_stop(server);
}
