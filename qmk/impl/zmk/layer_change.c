/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include "disp.h"
#include "steno.h"

#if STENO_COMMAND_DISPLAY

int layer_change_listener(const zmk_event_t *e) {
  if (zmk_keymap_highest_layer_active()) {
    disp_show_command_layer();
  } else {
    disp_unshow_command_layer();
  }
  return 0;
}

ZMK_LISTENER(layer_change, layer_change_listener);
ZMK_SUBSCRIPTION(layer_change, zmk_layer_state_changed);

#endif  /* STENO_COMMAND_DISPLAY */
