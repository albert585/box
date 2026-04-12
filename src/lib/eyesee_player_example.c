/**
 * Eyesee-MPP 播放器使用示例
 * 展示如何集成硬件加速视频播放器到 LVGL 应用
 */

#include "eyesee_player.h"
#include "lvgl/lvgl.h"

static eyesee_player_t * g_player = NULL;
static lv_obj_t * video_img = NULL;

// 播放完成回调函数
static void on_player_finish(eyesee_player_t * player)
{
    printf("[example] 视频播放完成\n");
    // 可以在这里添加播放完成后的逻辑，如播放下一个视频
}

// 打开视频文件并播放
int example_play_video(const char * filename, lv_obj_t * parent)
{
    if(!parent) {
        fprintf(stderr, "[example] 父对象为空\n");
        return -1;
    }

    // 如果已有播放器在运行，先停止
    if(g_player) {
        eyesee_player_stop(g_player);
        eyesee_player_destroy(g_player);
        g_player = NULL;
    }

    // 创建视频显示区域
    if(!video_img) {
        video_img = lv_img_create(parent);
        lv_obj_set_size(video_img, 800, 480); // 设置视频显示大小
        lv_obj_center(video_img);
    }

    // 创建播放器实例
    g_player = eyesee_player_create();
    if(!g_player) {
        fprintf(stderr, "[example] 创建播放器失败\n");
        return -1;
    }

    // 打开视频文件
    if(eyesee_player_open(g_player, filename) < 0) {
        fprintf(stderr, "[example] 打开视频文件失败: %s\n", filename);
        eyesee_player_destroy(g_player);
        g_player = NULL;
        return -1;
    }

    // 初始化视频显示（使用硬件解码）
    if(eyesee_player_init_video(g_player, video_img) < 0) {
        fprintf(stderr, "[example] 初始化视频显示失败\n");
        eyesee_player_destroy(g_player);
        g_player = NULL;
        return -1;
    }

    // 初始化音频播放
    if(eyesee_player_init_audio(g_player) < 0) {
        fprintf(stderr, "[example] 初始化音频失败（可能视频没有音频流）\n");
        // 音频初始化失败不一定是错误，视频可能只有视频流
    }

    // 设置播放完成回调
    eyesee_player_set_finish_callback(g_player, on_player_finish);

    // 开始播放
    if(eyesee_player_play(g_player) < 0) {
        fprintf(stderr, "[example] 开始播放失败\n");
        eyesee_player_destroy(g_player);
        g_player = NULL;
        return -1;
    }

    printf("[example] 开始播放视频: %s\n", filename);
    printf("[example] 视频分辨率: %dx%d\n",
           eyesee_player_get_video_width(g_player),
           eyesee_player_get_video_height(g_player));

    return 0;
}

// 暂停播放
void example_pause_video(void)
{
    if(g_player) {
        eyesee_player_pause(g_player);
        printf("[example] 视频已暂停\n");
    }
}

// 恢复播放
void example_resume_video(void)
{
    if(g_player) {
        eyesee_player_resume(g_player);
        printf("[example] 视频已恢复\n");
    }
}

// 停止播放
void example_stop_video(void)
{
    if(g_player) {
        eyesee_player_stop(g_player);
        eyesee_player_destroy(g_player);
        g_player = NULL;
        video_img = NULL;
        printf("[example] 视频已停止\n");
    }
}

// 跳转到指定百分比位置
void example_seek_video(double percent)
{
    if(g_player) {
        eyesee_player_seek_pct(g_player, percent);
        printf("[example] 跳转到 %.1f%%\n", percent);
    }
}

// 获取播放进度百分比
double example_get_video_progress(void)
{
    if(g_player) {
        return eyesee_player_get_position_pct(g_player);
    }
    return 0.0;
}

// 获取播放位置（毫秒）
int64_t example_get_video_position_ms(void)
{
    if(g_player) {
        return eyesee_player_get_position_ms(g_player);
    }
    return 0;
}

// 获取视频总时长（毫秒）
int64_t example_get_video_duration_ms(void)
{
    if(g_player) {
        return eyesee_player_get_duration_ms(g_player);
    }
    return 0;
}

// 检查播放器是否正在播放
bool example_is_video_playing(void)
{
    if(g_player) {
        return eyesee_player_get_state(g_player) == EYEESEE_PLAYER_PLAYING;
    }
    return false;
}

// 创建播放器控制 UI
void example_create_player_ui(lv_obj_t * parent)
{
    // 创建控制按钮容器
    lv_obj_t * ctrl_cont = lv_obj_create(parent);
    lv_obj_set_size(ctrl_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ctrl_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(ctrl_cont, 10, 0);
    lv_obj_align(ctrl_cont, LV_ALIGN_BOTTOM_MID, 0, -20);

    // 播放/暂停按钮
    lv_obj_t * btn_play = lv_btn_create(ctrl_cont);
    lv_obj_t * lbl_play = lv_label_create(btn_play);
    lv_label_set_text(lbl_play, "播放/暂停");
    lv_obj_add_event_cb(btn_play, (lv_event_cb_t)example_resume_video, LV_EVENT_CLICKED, NULL);

    // 暂停按钮
    lv_obj_t * btn_pause = lv_btn_create(ctrl_cont);
    lv_obj_t * lbl_pause = lv_label_create(btn_pause);
    lv_label_set_text(lbl_pause, "暂停");
    lv_obj_add_event_cb(btn_pause, (lv_event_cb_t)example_pause_video, LV_EVENT_CLICKED, NULL);

    // 停止按钮
    lv_obj_t * btn_stop = lv_btn_create(ctrl_cont);
    lv_obj_t * lbl_stop = lv_label_create(btn_stop);
    lv_label_set_text(lbl_stop, "停止");
    lv_obj_add_event_cb(btn_stop, (lv_event_cb_t)example_stop_video, LV_EVENT_CLICKED, NULL);

    // 进度条
    lv_obj_t * slider = lv_slider_create(parent);
    lv_obj_set_width(slider, 600);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -80);

    // 这里可以添加进度条事件处理来支持拖动跳转
}

/*
 * 使用示例:
 *
 * // 在应用初始化时:
 * lv_obj_t * screen = lv_scr_act();
 * example_play_video("/mnt/UDISK/video.mp4", screen);
 * example_create_player_ui(screen);
 *
 * // 在应用退出时:
 * example_stop_video();
 */
