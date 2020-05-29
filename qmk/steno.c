#include <string.h>
#include "steno.h"
#include "keymap_steno.h"
#include "raw_hid.h"
/* #include "action_layer.h" */
/* #include "eeconfig.h" */

#ifdef CUSTOM_STENO

#include "sdcard/fat.h"
#include "sdcard/partition.h"
#include "sdcard/sd_raw.h"

#include "hist.h"

search_node_t search_nodes[SEARCH_NODES_SIZE];
uint8_t search_nodes_len = 0;
state_t state = {.space = 0, .cap = 1, .prev_glue = 0};

bool send_steno_chord_user(steno_mode_t mode, uint8_t chord[6]) {
    uint32_t stroke = qmk_chord_to_stroke(chord);

    if (stroke == 0x1000) {  // Asterisk
        hist_undo();
        return false;
    }

    // TODO free up the next entry for boundary
    history_t new_hist;
    search_node_t *hist_nodes = malloc(search_nodes_len * sizeof(search_node_t));
    if (!hist_nodes) {
        xprintf("Can't allocate memory for history!\n");
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
        steno_debug("  steno(): state: space: %u, cap: %u, glue: %u\n", state.space, state.cap, state.prev_glue);
    }
    new_hist.state = state;
    new_hist.len = process_output(&state, new_hist.output, new_hist.repl_len);
    steno_debug("  steno(): processed: state: space: %u, cap: %u, glue: %u\n", state.space, state.cap, state.prev_glue);
    if (new_hist.len) {
        hist_add(new_hist);
    }

    steno_debug("\n");
    return false;
}

static uint8_t find_file_in_dir(struct fat_fs_struct* fs, struct fat_dir_struct* dd, const char* name, struct fat_dir_entry_struct* dir_entry) {
    while (fat_read_dir(dd, dir_entry)) {
        if (strcmp(dir_entry->long_name, name) == 0) {
            fat_reset_dir(dd);
            return 1;
        }
    }
    return 0;
}

static struct fat_file_struct* open_file_in_dir(struct fat_fs_struct* fs, struct fat_dir_struct* dd, const char* name) {
    struct fat_dir_entry_struct file_entry;
    if (!find_file_in_dir(fs, dd, name, &file_entry)) {
        return 0;
    }
    return fat_open_file(fs, &file_entry);
}

void keyboard_post_init_user(void) {
    _delay_ms(2000);
    if (!sd_raw_init()) {
        goto error;
    }

    struct partition_struct *partition = partition_open();
    if (!partition) {
        goto error;
    }

    /* open file system */
    struct fat_fs_struct *fs = fat_open(partition);
    if (!fs) {
        goto error;
    }

    /* open root directory */
    struct fat_dir_entry_struct directory;
    fat_get_dir_entry_of_path(fs, "/", &directory);

    struct fat_dir_struct *dd = fat_open_dir(fs, &directory);
    if (!dd) {
        goto error;
    }
    
    /* search file in current directory and open it */
    file = open_file_in_dir(fs, dd, "steno.bin");
    if (!file) {
        goto error;
    }

#ifdef OLED_DRIVER_ENABLE
    oled_set_contrast(0);
#endif

    return;
error:
    xprintf("Can't init\n");
    while(1);
}

void matrix_init_user() {
    steno_set_mode(STENO_MODE_GEMINI);
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    xprintf("recv: ");
    for (uint8_t i = 0; i < length; i ++) {
        xprintf(" %02X", data[i]);
    }
    xprintf("\n");
    /* oled_write((char *) data, false); */
    uint8_t buf[32] = {'A', 'C', 'K', 0};
    raw_hid_send(buf, 32);
    xprintf("Sent?!\n");
}

#endif
