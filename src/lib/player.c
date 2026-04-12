#include "player.h"

#define CUSTOM_SYMBOL_BACK "\uF053"
#define CUSTOM_SYMBOL_CYCLE "\uF021"
#define CUSTOM_SYMBOL_BAN "\uF05E"

static bool ui_hidden;
static lv_obj_t * btn_control;
static lv_obj_t * btn_control_label;
static ff_player_t * player;
static lv_obj_t * slider_volume;
static lv_obj_t * slider_progress;
static lv_obj_t * btn_back;
static lv_timer_t * timer;

static void back_click(lv_event_t * e);
static void slider_progress_released(lv_event_t * e);
static void slider_volume_changed(lv_event_t * e);
static void touch_clicked(lv_event_t * e);
static void control_click(lv_event_t * e);
static void timer_tick(lv_event_t * e);
static void player_finished(ff_player_t p);

static void control_click(lv_event_t * e)
{
    if(!player) return;
    player_state_t state = player_get_state(player);
    if(state == PLAYER_PAUSED) {
        audio_enable();
        player_resume(player);
        lv_label_set_text(btn_control_label, LV_SYMBOL_PAUSE "");
    }
    if(state == PLAYER_PLAYING) {
        player_pause(player);
        lv_label_set_text(btn_control_label, LV_SYMBOL_PLAY "");
        audio_disable();
    }
}

static void player_finished(ff_player_t p)
{
    lv_label_set_text(btn_control_label, LV_SYMBOL_PLAY "");
    audio_disable();
}

static void slider_volume_changed(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    int value         = lv_slider_get_value(slider);
    
    audio_volume_set(value);
}

static void slider_progress_released(lv_event_t * e)
{
    if(!player) return;
    player_state_t state = player_get_state(player);
    if(state == PLAYER_PLAYING || state == PLAYER_PAUSED) {
        lv_obj_t * slider = lv_event_get_target(e);
        int value         = lv_slider_get_value(slider);
        player_seek_pct(player, value);
    }
}

static void touch_clicked(lv_event_t * e) 
{
    ui_hidden = !ui_hidden;
    if(ui_hidden) {
        lv_obj_add_flag(slider_volume, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(slider_progress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_control, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_back, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(slider_volume, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(slider_progress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_control, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_back, LV_OBJ_FLAG_HIDDEN);
    }
}

static void timer_tick(lv_event_t * e)
{
    if(!player) return;
    lv_slider_set_value(slider_progress, player_get_position_pct(player), LV_ANIM_OFF);
}

static void back_click(lv_event_t * e)
{
    if(timer) lv_timer_del(timer);
    if(player) player_destroy(player);
    player = NULL;
    audio_disable();
    setDontDeepSleep(false);
}