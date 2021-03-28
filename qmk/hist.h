#pragma once

#include "stroke.h"

#define HIST_SIZE 32
#define HIST_MASK 0x1F
#define HIST_LIMIT(a) ((a) & HIST_MASK)

typedef struct __attribute__((packed)) {
    uint8_t space : 1;
    uint8_t cap : 2;
    uint8_t glue : 1;
} state_t;

typedef struct __attribute__((packed)) {
    uint8_t len;
    state_t state;
    uint8_t ortho_len;       // Length of orthographic entries' total strokes
    uint32_t stroke : 24;
    // Pointer + strokes length of the bucket; invalid if 0 or -1
    uint32_t bucket;
    uint8_t end_buf[8];
} history_t;

void hist_undo(uint8_t h_ind);
history_t *hist_get(uint8_t ind);
state_t process_output(uint8_t h_ind);
