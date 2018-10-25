#include "util.h"

static const char *TAG = "UTIL";

static const char StringsMap[64] = {
'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z', // 26
'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z', // 52
'0','1','2','3','4','5','6','7','8','9', // 62
'=', '-' // 64
};

static const char PinMap[16] = {
'0','1','2','3','4','5','6','7','8','9', // 10
'2','4','6','8','0','5' // 16 (little cheat to avoid calculations :D
};

// WARNING: Remember to call free() after use
char *getRandomString() {
	uint32_t r = esp_random();
	uint8_t a,b,c,d;
	char *output;
	a = (r>>24)&0x3F; // 6-bits max (0-63)
	b = (r>>16)&0x3F; // 6-bits max
	c = (r>>8)&0x3F; // 6-bits max
	d = r&0x3F; // 6-bits max
	output = malloc(5);
	snprintf(output, 5, "%c%c%c%c", StringsMap[a], StringsMap[b], StringsMap[c], StringsMap[d]);
	output[4] = '\0';
	ESP_LOGI(TAG, "Random string: %s", output);
	return output;
}

// WARNING: Remember to call free() after use
char *getRandomPin() {
	uint32_t r = esp_random();
	uint8_t a,b,c,d,e,f;
	char *output;
	a = (r>>20)&0x0F; // 4-bits max (0-15)
	b = (r>>16)&0x0F; // 4-bits max (0-15)
	c = (r>>12)&0x0F; // 4-bits max (0-15)
	d = (r>>8)&0x0F; // 4-bits max (0-15)
	e = (r>>4)&0x0F; // 4-bits max (0-15)
	f = r&0x0F; // 4-bits max (0-15)
	output = malloc(7);
	snprintf(output, 7, "%c%c%c%c%c%c", PinMap[a], PinMap[b], PinMap[c], PinMap[d], PinMap[e], PinMap[f]);
	output[6] = '\0';
	ESP_LOGI(TAG, "Random pin: %s", output);
	return output;
}

// WARNING: Remember to call free() after use
char *escapeJSONChars(char *input) {
	char *ptr = input;
	uint16_t special_chars_count = 0;
	while(*ptr != 0x00) {
		if(*ptr == '\\' || *ptr == '"') {
			special_chars_count++;
		}
		ptr++;
	}
	char *out = malloc(strlen(input)+special_chars_count+1);
	char *tmp = out;
	ptr = input;
	while(*ptr != 0x00) {
		if(*ptr == '\\' || *ptr == '"') {
			*tmp = '\\';
			tmp++;
		}
		*tmp = *ptr;
		tmp++;
		ptr++;
	}
	*tmp=0x00;
	return out;
}

static void urldecode2(char *dst, const char *src) {
        char a, b;
        while (*src) {
                if ((*src == '%') &&
                    ((a = src[1]) && (b = src[2])) &&
                    (isxdigit((int)a) && isxdigit((int)b))) {
                        if (a >= 'a')
                                a -= 'a'-'A';
                        if (a >= 'A')
                                a -= ('A' - 10);
                        else
                                a -= '0';
                        if (b >= 'a')
                                b -= 'a'-'A';
                        if (b >= 'A')
                                b -= ('A' - 10);
                        else
                                b -= '0';
                        *dst++ = 16*a+b;
                        src+=3;
                } else if (*src == '+') {
                        *dst++ = ' ';
                        src++;
                } else {
                        *dst++ = *src++;
                }
        }
        *dst++ = '\0';
}

void URLDecode(char *buf, size_t sz) {
	char *tmp = malloc(sz);
	urldecode2(tmp, buf);
	memset(buf, 0x00, sz);
	memcpy(buf, tmp, sz);
	free(tmp);
}
