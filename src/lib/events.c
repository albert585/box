#include "events.h"
#include "file_manager.h"
#include "lvgl/src/core/lv_obj_pos.h"
#include "lvgl/src/display/lv_display.h"
#include "settings.h"
#include "container.h"
#include "../main.h"
#include "audio.h"
#include "player.h"
#include "ff_player.h"
#ifdef USE_EYEESEE_MPP
#include "eyesee_player.h"
#endif
#include "virsual_novel/visual_novel_engine.h"
#include "lv_lib_100ask/lv_lib_100ask.h"
#include "button.h"
#include "lvgl/src/libs/ffmpeg/lv_ffmpeg.h"
extern lv_obj_t *parent;
lv_obj_t *obj_2048 = NULL;
ff_player_t *current_ff_player = NULL;
static lv_obj_t *native_ffmpeg_player = NULL;

#ifdef USE_EYEESEE_MPP
eyesee_player_t *current_eyesee_player = NULL;
#endif

void event_open_manager(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
      lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);
      file_manager();
    }
}

void event_close_manager(lv_event_t * e){
  // 如果是通过代码调用（非事件触发），e 可能为 NULL
  lv_obj_t * manager = NULL;
  if (e) {
    manager = lv_event_get_user_data(e);
  } else {
    // 如果没有事件，直接使用全局 manager 变量
    extern lv_obj_t *manager;
    manager = manager;
  }

  if (manager) {
    lv_obj_del(manager);
    manager = NULL;
  }

  lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
  settings_close();
}

void event_open_settings(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
      settings();
    }
}

void btn_robot_click(lv_event_t * e)
{
    (void)e; // 避免未使用参数警告
    switchRobot();
}

// 文件选择事件处理
void file_select_event(lv_event_t * e)
{
    lv_obj_t * file_explorer = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_VALUE_CHANGED) {
        // 获取当前路径和选中的文件名
        const char * cur_path = lv_file_explorer_get_current_path(file_explorer);
        const char * sel_fn = lv_file_explorer_get_selected_file_name(file_explorer);

        if (cur_path && sel_fn) {
            // 检查并去除盘符前缀（如 "A:"）
            if (strlen(cur_path) >= 2 && cur_path[1] == ':') {
                cur_path += 2; // 跳过盘符，例如 "A:" -> 指向 "/"
            }

            // 构建完整路径（LVGL 格式）
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s%s", cur_path, sel_fn);

            printf("[events] File selected: %s\n", full_path);

            // 检查文件扩展名
            const char *ext = strrchr(sel_fn, '.');
            if (ext) {
                // 检查是否是视频文件
                if (strcasecmp(ext, ".mp4") == 0 || 
                    strcasecmp(ext, ".avi") == 0 || 
                    strcasecmp(ext, ".mkv") == 0 ||
                    strcasecmp(ext, ".mov") == 0) {
                    // 视频文件
                    page_video(full_path);
                } else {
                    // 音频文件
                    page_audio(full_path);
                }
            } else {
                // 没有扩展名，默认当作音频文件
                page_audio(full_path);
            }
        }
    }
}



void event_open_visual_novel(lv_event_t * e)
{
    (void)e; // 避免未使用参数警告
    vn_engine_start();
}

void event_close_visual_novel(lv_event_t * e)
{
    (void)e;
    vn_engine_deinit();
}

void event_close_ff_player(lv_event_t * e)
{
    (void)e;
    if(current_ff_player) {
        // 先停止播放（会等待线程结束）
        player_stop(current_ff_player);

        // 删除视频显示对象（如果存在）
        if(current_ff_player->video_area) {
            // 检查对象是否仍然有效
            if(lv_obj_is_valid(current_ff_player->video_area)) {
                lv_obj_del(current_ff_player->video_area);
            }
            current_ff_player->video_area = NULL;
        }

        // 销毁播放器
        player_destroy(current_ff_player);
        current_ff_player = NULL;
    }
    else if(native_ffmpeg_player) {
        // 停止并删除原生播放器
        lv_ffmpeg_player_set_cmd(native_ffmpeg_player, LV_FFMPEG_PLAYER_CMD_STOP);
        if(lv_obj_is_valid(native_ffmpeg_player)) {
            lv_obj_del(native_ffmpeg_player);
        }
        native_ffmpeg_player = NULL;
    }
    // 恢复主界面显示
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
}

void ff_player_finish_callback(ff_player_t *player)
{
    if (!player) return;

    // 如果销毁的是当前播放器，清空全局指针
    if (current_ff_player == player) {
        printf("[events] Current ff_player destroyed\n");
        current_ff_player = NULL;
    }
}

void event_open_2048(lv_event_t *e){
    (void)e;
    lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);
    obj_2048 = lv_100ask_2048_create(lv_screen_active());
    
    if (obj_2048 == NULL) {
        printf("[2048] Failed to create 2048 game object\n");
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    lv_obj_set_size(obj_2048, 200, 200);
    
    if (btn_exit != NULL) {
        lv_obj_set_size(btn_exit, 40, 40);
    }

    lv_obj_center(obj_2048);
}
void event_close_2048(lv_event_t *e){
  (void)e;
  
  if (obj_2048 != NULL) {
    lv_obj_del(obj_2048);
    obj_2048 = NULL;
  }
  
  lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
}

void event_play_video(lv_event_t *e)
{
    (void)e;
    printf("[video] Play video button clicked (using LVGL native ffmpeg)\n");

    // 视频文件路径
    const char *video_file = "/mnt/app/neuro.mp4";
    printf("[video] Playing video file: %s\n", video_file);

    // 隐藏主界面
    lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);

    // 创建 LVGL 原生 ffmpeg 播放器
    native_ffmpeg_player = lv_ffmpeg_player_create(lv_screen_active());
    if (!native_ffmpeg_player) {
        printf("[video] Failed to create native ffmpeg player\n");
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // 设置播放器大小为全屏
    int32_t scr_width = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t scr_height = lv_display_get_vertical_resolution(lv_display_get_default());
    lv_obj_set_size(native_ffmpeg_player, scr_width, scr_height);
    lv_obj_center(native_ffmpeg_player);

    // 设置视频文件
    lv_result_t ret = lv_ffmpeg_player_set_src(native_ffmpeg_player, video_file);
    if (ret != LV_RESULT_OK) {
        printf("[video] Failed to set video source (result=%d)\n", ret);
        lv_obj_del(native_ffmpeg_player);
        native_ffmpeg_player = NULL;
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // 启用自动重播
    lv_ffmpeg_player_set_auto_restart(native_ffmpeg_player, false);

    // 开始播放
    lv_ffmpeg_player_set_cmd(native_ffmpeg_player, LV_FFMPEG_PLAYER_CMD_START);

    printf("[video] Native ffmpeg playback started\n");
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

// void event_audio_test(lv_event_t * e)     /* 暂时不需要了 */
// {
//     (void)e;
//     printf("[audio_test] Audio test button clicked\n");

//     // 简单的音频测试：播放一个测试文件
//     const char *test_file = "/mnt/app/factory/1khz.wav";
//     printf("[audio_test] Playing test file: %s\n", test_file);

//     // 如果当前没有播放器，创建一个新的
//     if (current_player == NULL) {
//         printf("[audio_test] Creating new player instance\n");
//         current_player = player_create(parent);  // 使用 parent 容器
//         if (!current_player) {
//             printf("[audio_test] Failed to create player\n");
//             return;
//         }
//     }

//     // 设置音频文件并自动播放
//     player_set_file(current_player, test_file);
//     player_toggle_play_pause(current_player);

//     printf("[audio_test] Audio test started\n");
// }

#ifdef USE_EYEESEE_MPP
void eyesee_player_finish_callback(eyesee_player_t *player)
{
    if (!player) return;

    if (current_eyesee_player == player) {
        printf("[events] Current eyesee_player finished\n");
    }
}

void event_close_eyesee_player(lv_event_t * e)
{
    (void)e;
    if (current_eyesee_player) {
        eyesee_player_stop(current_eyesee_player);

        eyesee_player_destroy(current_eyesee_player);
        current_eyesee_player = NULL;
    }
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
}

void page_video_hw(const char *video_file)
{
    if (!video_file) {
        printf("[video_hw] Invalid video file path\n");
        return;
    }

    printf("[video_hw] Opening with eyesee-mpp HW decoder: %s\n", video_file);

    // 如果已有播放器，先停止
    if (current_eyesee_player) {
        eyesee_player_stop(current_eyesee_player);
        eyesee_player_destroy(current_eyesee_player);
        current_eyesee_player = NULL;
    }

    lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);

    // 创建播放器实例
    current_eyesee_player = eyesee_player_create();
    if (!current_eyesee_player) {
        printf("[video_hw] Failed to create eyesee player\n");
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // 打开文件
    if (eyesee_player_open(current_eyesee_player, video_file) != 0) {
        printf("[video_hw] Failed to open file\n");
        eyesee_player_destroy(current_eyesee_player);
        current_eyesee_player = NULL;
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // 创建视频显示对象
    int32_t scr_w = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t scr_h = lv_display_get_vertical_resolution(lv_display_get_default());

    lv_obj_t *video_obj = lv_image_create(lv_screen_active());
    if (!video_obj) {
        printf("[video_hw] Failed to create video image object\n");
        eyesee_player_destroy(current_eyesee_player);
        current_eyesee_player = NULL;
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_set_size(video_obj, scr_w, scr_h);
    lv_obj_center(video_obj);

    // 初始化硬件视频解码
    if (eyesee_player_init_video(current_eyesee_player, video_obj) != 0) {
        printf("[video_hw] Failed to init HW video\n");
        lv_obj_del(video_obj);
        eyesee_player_destroy(current_eyesee_player);
        current_eyesee_player = NULL;
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // 初始化音频（失败不阻断）
    if (eyesee_player_init_audio(current_eyesee_player) != 0) {
        printf("[video_hw] Audio init failed (video-only mode)\n");
    }

    // 设置播放完成回调
    eyesee_player_set_finish_callback(current_eyesee_player, eyesee_player_finish_callback);

    // 开始播放
    if (eyesee_player_play(current_eyesee_player) != 0) {
        printf("[video_hw] Failed to start playback\n");
        lv_obj_del(video_obj);
        eyesee_player_destroy(current_eyesee_player);
        current_eyesee_player = NULL;
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    printf("[video_hw] HW playback started: %dx%d\n",
           eyesee_player_get_video_width(current_eyesee_player),
           eyesee_player_get_video_height(current_eyesee_player));
}
#endif
