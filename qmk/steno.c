#include <string.h>
#include "steno.h"
#include "keymap_steno.h"
#include "raw_hid.h"
#include "flash.h"
#include <stdio.h>

#ifdef CUSTOM_STENO

#include "hist.h"
/* #include "analog.h" */

#ifndef __AVR__
#include "app_ble_func.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

void tap_code(uint8_t code) {
    register_code(code);
    unregister_code(code);
}

void tap_code16(uint16_t code) {
    register_code16(code);
    unregister_code16(code);
}

void _delay_ms(uint16_t ms) {
    nrf_delay_ms(ms);
}
#endif

search_node_t search_nodes[SEARCH_NODES_SIZE];
uint8_t search_nodes_len = 0;
state_t state = {.space = 0, .cap = ATTR_CAPS_CAPS, .prev_glue = 0};
#ifdef OLED_DRIVER_ENABLE
char last_stroke[24];
char last_trans[128];
uint8_t last_trans_size;
#endif

// Intercept the steno key codes, searches for the stroke, and outputs the output
bool send_steno_chord_user(steno_mode_t mode, uint8_t chord[6]) {
    uint32_t stroke = qmk_chord_to_stroke(chord);
#ifdef OLED_DRIVER_ENABLE
    last_trans_size = 0;
    memset(last_trans, 0, 128);
    stroke_to_string(stroke, last_stroke, NULL);
#endif

    if (stroke == 0x1000) {  // Asterisk
        hist_undo();
        return false;
    }

    // TODO free up the next entry for boundary
    history_t new_hist;
    search_node_t *hist_nodes = malloc(search_nodes_len * sizeof(search_node_t));
    if (!hist_nodes) {
        xprintf("No memory for history!\n");
        return false;
    }
    memcpy(hist_nodes, search_nodes, search_nodes_len * sizeof(search_node_t));
    new_hist.search_nodes = hist_nodes;
    new_hist.search_nodes_len = search_nodes_len;

    uint32_t max_level_node = 0;
    uint8_t max_level = 0;
    search_on_nodes(search_nodes, &search_nodes_len, stroke, &max_level_node, &max_level);

    if (max_level_node) {
        new_hist.output.type = NODE_STRING;
        new_hist.output.node = max_level_node;
        new_hist.repl_len = max_level - 1;
    } else {
        new_hist.output.type = RAW_STROKE;
        new_hist.output.stroke = stroke;
        new_hist.repl_len = 0;
    }
    if (new_hist.repl_len) {
        state = history[(hist_ind - new_hist.repl_len + 1) % HIST_SIZE].state;
    }
    new_hist.state = state;
    steno_debug("steno(): state: space: %u, cap: %u, glue: %u\n", state.space, state.cap, state.prev_glue);
    new_hist.len = process_output(&state, new_hist.output, new_hist.repl_len);
    steno_debug("steno(): processed: state: space: %u, cap: %u, glue: %u\n", state.space, state.cap, state.prev_glue);
    if (new_hist.len) {
        hist_add(new_hist);
    }

    steno_debug("--------\n\n");
    return false;
}

#ifdef USE_SPI_FLASH
uint16_t crc8(uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i ++) {
        crc ^= data[i];
        for (uint8_t i = 0; i < 8; ++i) {
            crc = crc >> 1;
            if (crc & 1) {
                crc = crc ^ 0x8C;
            }
        }
    }
    return crc;
}

#define nack(reason) \
    data[0] = 0x55; \
    data[1] = 0xFF; \
    data[2] = (reason); \
    data[PACKET_SIZE - 1] = crc8(data, MSG_SIZE); \
    raw_hid_send(data, PACKET_SIZE);

// Handle the HID packets, mostly for downloading and uploading the dictionary.
void raw_hid_receive(uint8_t *data, uint8_t length) {
    static mass_write_info_t mass_write_infos[PACKET_SIZE];
    static uint8_t mass_write_packet_num = 0;
    static uint8_t mass_write_packet_ind = 0;
    static uint32_t mass_write_addr = 0;

    if (mass_write_packet_num) {
        mass_write_info_t info = mass_write_infos[mass_write_packet_ind];
        uint8_t crc = crc8(data, info.len);
        if (crc != info.crc) {
            xprintf("calc: %X, info: %X\n", crc, info.crc);
            nack(0x04);
            return;
        }
        flash_write(mass_write_addr, data, info.len);
        mass_write_addr += info.len;
        mass_write_packet_ind ++;
        if (mass_write_packet_ind == mass_write_packet_num) {
            mass_write_packet_num = 0;
        }

        data[0] = 0x55;
        data[1] = 0x01;
        data[PACKET_SIZE - 1] = crc8(data, MSG_SIZE);
        raw_hid_send(data, PACKET_SIZE);
        return;
    }

    if (data[0] != 0xAA) {
        xprintf("head\n");
        nack(0x01);
        return;
    }

    uint8_t crc = crc8(data, MSG_SIZE);
    if (crc != data[PACKET_SIZE - 1]) {
        xprintf("CRC: %X\n", crc);
        nack(0x02);
        return;
    }

    uint32_t addr;
    uint8_t len;
    /* xprintf("head: %X\n", data[1]); */
    switch (data[1]) {
        case 0x01:;
            addr = (uint32_t) data[3] | (uint32_t) data[4] << 8 | (uint32_t) data[5] << 16;
            len = data[2];
            flash_write(addr, data + 6, len);
            data[0] = 0x55;
            data[1] = 0x01;
            break;
        case 0x02:;
            addr = (uint32_t) data[3] | (uint32_t) data[4] << 8 | (uint32_t) data[5] << 16;
            len = data[2];
            data[1] = 0x02;
            data[2] = len;
            flash_read(addr, data + 6, len);
            break;
        case 0x03:;
            addr = (uint32_t) data[3] | (uint32_t) data[4] << 8 | (uint32_t) data[5] << 16;
            flash_erase_page(addr);
            data[1] = 0x01;
            break;
        case 0x04:;
            mass_write_addr = (uint32_t) data[3] | (uint32_t) data[4] << 8 | (uint32_t) data[5] << 16;
            mass_write_packet_num = data[2];
            mass_write_packet_ind = 0;
            memcpy(mass_write_infos, data + 6, sizeof(mass_write_infos));
            data[1] = 0x01;
            break;
        default:;
            nack(0x03);
            return;
    }

    data[0] = 0x55;
    data[PACKET_SIZE - 1] = crc8(data, MSG_SIZE);
    raw_hid_send(data, PACKET_SIZE);
}
#endif

// Setup the necessary stuff, init SD card or SPI flash. Delay so that it's easy for `hid-listen` to recognize
// the keyboard
void keyboard_post_init_user(void) {
    /* _delay_ms(2000); */
#ifdef USE_SPI_FLASH
    flash_init();
#else
    extern FATFS fat_fs;
    if (pf_mount(&fat_fs)) {
        xprintf("Volume\n");
        goto error;
    }

    FRESULT res = pf_open("STENO.BIN");
    if (res) {
        xprintf("File: %X\n", res);
        goto error;
    }

#ifdef OLED_DRIVER_ENABLE
    oled_set_contrast(0);
#endif

    xprintf("init\n");
    return;
error:
    xprintf("Can't init\n");
    while(1);
#endif
#ifndef __AVR__
#ifdef OLED_DRIVER_ENABLE
    steno_debug("oled_init: %u", oled_init(0));
#endif
#endif
}

void matrix_init_user() {
    steno_set_mode(STENO_MODE_GEMINI);
#ifndef __AVR__
    set_usb_enabled(true);
#endif
}

#ifdef OLED_DRIVER_ENABLE
void oled_task_user(void) {
#ifdef __AVR__
    uint16_t adc = analogReadPin(B5);
    char buf[32];
    uint16_t volt = (uint32_t) adc * 33 * 2 * 10 / 1024;
    sprintf(buf, "BAT: %u.%uV\n", volt / 100, volt % 100);
    oled_write(buf, false);
#endif
    oled_write_ln(last_stroke, false);
    oled_write_ln(last_trans, false);
}
#endif

#endif
