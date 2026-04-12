#include "lvgl/lvgl.h"
#include "lvgl/src/misc/lv_types.h"
#include "ff_player.h"

#ifdef USE_EYEESEE_MPP
#include "eyesee_player.h"
#endif

extern void event_close_manager(lv_event_t * e);
extern void event_open_manager(lv_event_t * e);
extern void slider_event_cb(lv_event_t * e);
extern void event_open_settings(lv_event_t *e);
extern void btn_robot_click(lv_event_t * e);
extern void file_select_event(lv_event_t * e);
extern void event_open_visual_novel(lv_event_t * e);
extern void event_close_visual_novel(lv_event_t * e);
extern void event_open_2048(lv_event_t * e);
extern void event_play_video(lv_event_t * e);

// FFmpeg 软件解码播放器相关
extern void page_video(const char *video_file);
extern ff_player_t *current_ff_player;
extern void event_close_ff_player(lv_event_t * e);
extern void ff_player_finish_callback(ff_player_t *player);

#ifdef USE_EYEESEE_MPP
// Eyesee-MPP 硬件解码播放器相关
extern eyesee_player_t *current_eyesee_player;
extern void event_close_eyesee_player(lv_event_t * e);
extern void eyesee_player_finish_callback(eyesee_player_t *player);
extern void page_video_hw(const char *video_file);
#endif
