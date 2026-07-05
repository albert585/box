#include "lvgl/lvgl.h"
#include "lvgl/src/misc/lv_types.h"
#include "ff_player.h"

extern ff_player_t *current_ff_player;
extern void event_play_video(lv_event_t * e);
extern void event_close_ff_player(lv_event_t * e);
extern void ff_player_finish_callback(ff_player_t *player);
extern void page_video(const char *video_file);
