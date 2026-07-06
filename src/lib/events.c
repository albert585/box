#include "events.h"
#include "lvgl/src/core/lv_obj_pos.h"
#include "lvgl/src/display/lv_display.h"
#include "container.h"
#include "../main.h"
#include "audio.h"
#include "player.h"
#include "ff_player.h"
#include "button.h"
extern lv_obj_t *parent;
ff_player_t *current_ff_player = NULL;
void event_btn_test(lv_event_t * e){
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t * btn = lv_event_get_target(e);
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        lv_label_set_text(label, "OK");
        printf("Button Clicked\n");
    }
}


void event_close_ff_player(lv_event_t * e)
{
    (void)e;
    if(current_ff_player) {
        player_stop(current_ff_player);

        if(current_ff_player->video_area) {
            if(lv_obj_is_valid(current_ff_player->video_area)) {
                lv_obj_del(current_ff_player->video_area);
            }
            current_ff_player->video_area = NULL;
        }

        player_destroy(current_ff_player);
        current_ff_player = NULL;
    }
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
}

void ff_player_finish_callback(ff_player_t *player)
{
    if (!player) return;

    if (current_ff_player == player) {
        printf("[events] Current ff_player destroyed\n");
        current_ff_player = NULL;
    }
}

void event_play_video(lv_event_t * e)
{
    (void)e;
    page_video("/mnt/app/neuro.mp4");
}

void page_video(const char *video_file)
{
    if (!video_file) {
        printf("[video] Invalid video file path\n");
        return;
    }

    printf("[video] Playing video file: %s\n", video_file);
    
    // 隐藏主界面
    lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);
    
    // 创建视频播放器
    current_ff_player = player_create();
    if (!current_ff_player) {
        printf("[video] Failed to create video player\n");
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    // 打开视频文件
    if (player_open(current_ff_player, video_file) != 0) {
        printf("[video] Failed to open video file\n");
        player_destroy(current_ff_player);
        current_ff_player = NULL;
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    // 初始化音频
    if (player_init_audio(current_ff_player) != 0) {
        printf("[video] Failed to initialize audio\n");
        player_destroy(current_ff_player);
        current_ff_player = NULL;
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    // 创建视频显示对象
    lv_obj_t *video_obj = lv_obj_create(lv_screen_active());
    if (!video_obj) {
        printf("[video] Failed to create video display object\n");
        player_destroy(current_ff_player);
        current_ff_player = NULL;
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    // 设置视频显示对象大小
    int32_t scr_width = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t scr_height = lv_display_get_vertical_resolution(lv_display_get_default());
    lv_obj_set_size(video_obj, scr_width, scr_height);
    lv_obj_center(video_obj);
    
    // 初始化视频
    if (player_init_video(current_ff_player, video_obj) != 0) {
        printf("[video] Failed to initialize video\n");
        lv_obj_del(video_obj);
        player_destroy(current_ff_player);
        current_ff_player = NULL;
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    // 设置播放完成回调
    player_set_finish_callback(current_ff_player, ff_player_finish_callback);
    
    printf("[video] Video playback started\n");
}


