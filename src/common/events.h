#ifndef EVENTS_H
#define EVENTS_H

#include "lvgl/lvgl.h"
#include "lvgl/src/misc/lv_types.h"
#include "ff_player.h"

extern ff_player_t *current_ff_player;
void event_btn_test(lv_event_t * e);
extern void event_play_video(lv_event_t * e);
extern void event_close_ff_player(lv_event_t * e);
extern void ff_player_finish_callback(ff_player_t *player);
extern void page_video(const char *video_file);
extern void event_open_bird(lv_event_t * e);
extern void event_print_test(lv_event_t * e);
extern void container_close_cb(lv_event_t *e);

#endif