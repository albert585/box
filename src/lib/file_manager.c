#include "audio.h"
#include "player.h"
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "./file_manager.h"
#include "./container.h"
#include "./events.h"

lv_obj_t *manager = NULL;
static lv_obj_t *file_list = NULL;
static char current_path[PATH_MAX] = "/mnt/app";

// 将 LVGL 文件浏览器路径转换为实际系统路径
// A:/xxx -> /xxx (根据 lv_conf.h 中 LV_FS_POSIX_LETTER = 'A')
static const char *convert_lvgl_path(const char *lvgl_path)
{
    static char real_path[PATH_MAX];

    if (!lvgl_path) {
        return NULL;
    }

    // 检查是否是盘符开头的路径（如 A:/, B:/, C:/ 等）
    if (strlen(lvgl_path) >= 2 && lvgl_path[1] == ':') {
        // 去掉盘符前缀，直接使用后面的路径
        snprintf(real_path, sizeof(real_path), "%s", lvgl_path + 2);
    } else {
        // 不是盘符路径，直接使用
        snprintf(real_path, sizeof(real_path), "%s", lvgl_path);
    }

    return real_path;
}



void file_manager(void) {

    manager = lv_obj_create(lv_screen_active());
    lv_obj_set_size(manager, 960, 240);
    lv_obj_set_pos(manager, 0, 0);
    lv_obj_set_style_border_width(manager, 0, 0);
    lv_obj_add_event_cb(manager,event_close_manager,LV_EVENT_CLICKED,manager);


    /* Create back button */
    lv_obj_t *back_btn = lv_button_create(manager);
    lv_obj_set_size(back_btn, 60, 30);
    lv_obj_add_event_cb(back_btn, event_close_manager, LV_EVENT_CLICKED, manager);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    lv_obj_t *fmg =lv_file_explorer_create(manager);
    lv_file_explorer_set_sort(fmg, LV_EXPLORER_SORT_KIND);
    lv_obj_set_size(fmg, 960, 540);

    /* Get the file table object and set row height */

    lv_file_explorer_open_dir(fmg,"A:/");

    /* 添加文件选择事件处理 */
    lv_obj_add_event_cb(fmg, file_select_event, LV_EVENT_VALUE_CHANGED, fmg);
  }

static bool is_end_with(const char * str1, const char * str2)
{
    if(str1 == NULL || str2 == NULL)
        return false;
    
    uint16_t len1 = strlen(str1);
    uint16_t len2 = strlen(str2);
    if((len1 < len2) || (len1 == 0 || len2 == 0))
        return false;
    
    while(len2 >= 1)
    {
        if(tolower(str2[len2 - 1])  != tolower(str1[len1 - 1]))
            return false;

        len2--;
        len1--;
    }

    return true;
}

// 音视频播放器函数：根据文件路径创建或更新播放器
void media_player(const char * path)
{
    if (!path) return;

    // 转换 LVGL 路径为实际系统路径
    const char *real_path = convert_lvgl_path(path);
    if (!real_path) return;

    printf("[file_manager] Opening file: %s\n", real_path);

    // 检查是否是音频文件
    if (is_end_with(real_path, ".mp3") ||
        is_end_with(real_path, ".wav") ||
        is_end_with(real_path, ".flac") ||
        is_end_with(real_path, ".aac") ||
        is_end_with(real_path, ".ogg") ||
        is_end_with(real_path, ".m4a")) {

        printf("[file_manager] Audio file detected, creating audio player...\n");

        // 关闭文件管理器
        event_close_manager(NULL);

        // 创建音频播放页面
        page_audio((char *)real_path);

        printf("[file_manager] Audio player started successfully\n");
    }
    // 检查是否是视频文件
    else if (is_end_with(real_path, ".mp4") ||
             is_end_with(real_path, ".avi") ||
             is_end_with(real_path, ".mkv") ||
             is_end_with(real_path, ".mov") ||
             is_end_with(real_path, ".flv") ||
             is_end_with(real_path, ".wmv") ||
             is_end_with(real_path, ".webm")) {

        printf("[file_manager] Video file detected, creating video player...\n");

        // 关闭文件管理器
        event_close_manager(NULL);

        // 创建视频播放页面（使用硬件解码）
#ifdef USE_EYEESEE_MPP
        page_video_hw((char *)real_path);
#else
        page_video((char *)real_path);
#endif

        printf("[file_manager] Video player started successfully\n");
    } else {
        printf("[file_manager] Not a supported media file: %s\n", real_path);
    }
}
