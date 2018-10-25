#include "session_manager.h"

static const char *TAG = "SESS_MGR";

static web_session_t sessions[MAX_WEBADMIN_SESSIONS];

void init_session_manager() {
	uint8_t i;
	for(i=0; i<MAX_WEBADMIN_SESSIONS; i++) {
		sessions[i].in_use=0;
	}
	ESP_LOGI(TAG, "Session manager initalized");
}

session_state_t check_sesssion(char *sess_id) {
	uint8_t i;
	for(i=0; i<MAX_WEBADMIN_SESSIONS; i++) {
		if(strcmp(sessions[i].sess_id, sess_id) == 0 && sessions[i].in_use == 1) {
			return SESSION_EXISTS;
		}
	}
	return SESSION_NOT_EXISTS;
}

void add_sesssion(char *sess_id) {
	uint8_t i;
	for(i=0; i<MAX_WEBADMIN_SESSIONS; i++) {
		if(sessions[i].in_use == 0) {
			sessions[i].in_use=1;
			strcpy(sessions[i].sess_id, sess_id);
			ESP_LOGI(TAG, "Added sess_id:%s, slot=%d", sess_id, i);
			return;
		}
	}

	// Not enough space to store sessions, remove the first one :D
	sessions[0].in_use=1;
	strcpy(sessions[0].sess_id, sess_id);
	ESP_LOGW(TAG, "Not enough session storage, using the first slot, sess_id=%s", sess_id);
	print_sessions();
}

void del_session(char *sess_id) {
	uint8_t i;
	for(i=0; i<MAX_WEBADMIN_SESSIONS; i++) {
		if(sessions[i].in_use == 1 && strcmp(sessions[i].sess_id, sess_id) == 0) {
			sessions[i].in_use=0;
			ESP_LOGI(TAG, "Deleted sess_id=%s, slot=%d", sess_id, i);
			return;
		}
	}
}

void print_sessions() {
	uint8_t i;
	ESP_LOGI(TAG, "Current Sessions: ");
	for(i=0; i<MAX_WEBADMIN_SESSIONS; i++) {
		if(sessions[i].in_use == 1) {
			ESP_LOGI(TAG, "%d: %s", i, sessions[i].sess_id);
		}
	}
}
