#ifndef MAIN_SESSION_MANAGER_H_
#define MAIN_SESSION_MANAGER_H_

#include "esp_log.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>

typedef enum session_state {
SESSION_NOT_EXISTS=0,
SESSION_EXISTS=1
} session_state_t;

typedef struct web_session {
	char sess_id[15];
	uint8_t in_use;
} web_session_t;

void init_session_manager();
session_state_t check_sesssion(char *sess_id);
void add_sesssion(char *sess_id);
void del_session(char *sess_id);
void print_sessions();

#endif /* MAIN_SESSION_MANAGER_H_ */
