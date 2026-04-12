#ifndef EYEESEE_PLAYER_H
#define EYEESEE_PLAYER_H

#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <stdbool.h>

// Eyesee-MPP 头文件
#include "vdecoder.h"
#include "vbasetype.h"
#include "sc_interface.h"
#include "memoryAdapter.h"
#include "typedef.h"

// FFmpeg 头文件（用于解封装）
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

// ALSA 头文件
#include <alsa/asoundlib.h>

typedef enum {
    EYEESEE_PLAYER_STOPPED,
    EYEESEE_PLAYER_PLAYING,
    EYEESEE_PLAYER_PAUSED
} eyesee_player_state_t;

typedef struct eyesee_player_s
{
    // FFmpeg 解封装相关
    AVFormatContext * format_ctx;
    int video_stream_index;
    int audio_stream_index;

    // Eyesee-MPP 视频解码器
    VideoDecoder * video_decoder;
    VideoStreamInfo video_stream_info;
    VConfig video_config;
    struct ScMemOpsS * mem_ops;
    int nDecodeStreamIndex;

    // 视频帧缓冲信息
    FbmBufInfo * fbm_buf_info;
    int nValidPictureNum;

    // FFmpeg 音频解码相关
    AVCodecContext * audio_codec_ctx;
    SwrContext * swr_ctx;

    // ALSA 相关
    snd_pcm_t * pcm_handle;
    snd_pcm_uframes_t frames;
    unsigned int sample_rate;
    int channels;

    // 显示相关
    lv_obj_t * video_area;
    lv_img_dsc_t img_dsc;
    uint8_t * rgb_frame_buffer;     // RGB 转换缓冲区
    int display_width;
    int display_height;

    // 播放控制
    volatile int state;
    volatile bool seek_request;
    volatile int64_t seek_pos;
    pthread_t player_thread;
    pthread_mutex_t mutex;

    // 进度信息
    volatile int64_t current_pts;
    AVRational video_time_base;
    AVRational audio_time_base;
    int64_t duration;

    char * filename;

    void (*finish_callback_ptr)(struct eyesee_player_s *);
} eyesee_player_t;

// 函数声明
eyesee_player_t * eyesee_player_create();
int eyesee_player_open(eyesee_player_t * player, const char * filename);
int eyesee_player_init_audio(eyesee_player_t * player);
int eyesee_player_init_video(eyesee_player_t * player, lv_obj_t * lv_obj);
int eyesee_player_play(eyesee_player_t * player);
int eyesee_player_pause(eyesee_player_t * player);
int eyesee_player_resume(eyesee_player_t * player);
int eyesee_player_stop(eyesee_player_t * player);
int eyesee_player_seek_pct(eyesee_player_t * player, double percent);
int eyesee_player_seek_ms(eyesee_player_t * player, int64_t target_ms);
int64_t eyesee_player_get_position_ms(eyesee_player_t * player);
int64_t eyesee_player_get_duration_ms(eyesee_player_t * player);
double eyesee_player_get_position_pct(eyesee_player_t * player);
eyesee_player_state_t eyesee_player_get_state(eyesee_player_t * player);
void eyesee_player_destroy(eyesee_player_t * player);

// 状态变化回调
void eyesee_player_set_finish_callback(eyesee_player_t * player, void (*func_ptr)(eyesee_player_t *));

// 获取视频信息
int eyesee_player_get_video_width(eyesee_player_t * player);
int eyesee_player_get_video_height(eyesee_player_t * player);

#endif // EYEESEE_PLAYER_H
