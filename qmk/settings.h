#pragma once

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t start_cap : 1;
} settings_t;

extern settings_t settings;

void read_settings(void);
void save_settings(void);
