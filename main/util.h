#ifndef MAIN_UTIL_H_
#define MAIN_UTIL_H_

#include "esp_system.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

char *getRandomString();
char *getRandomPin();
char *escapeJSONChars(char *input);
void URLDecode(char *buf, size_t sz);

#endif /* MAIN_UTIL_H_ */
