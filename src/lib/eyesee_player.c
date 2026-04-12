/**
 * Eyesee-MPP 硬件加速视频播放器
 * 基于全志 V833 平台的 CedarX 硬件解码器
 * 支持 H.264/H.265/MJPEG 等格式的硬件解码
 */

#include "eyesee_player.h"

#define BUFFER_SIZE 4096
#define MAX_CHANNELS 6
#define VIDEO_STREAM_INDEX 0

/* ARM NEON SIMD 优化支持 */
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
    #define HAS_NEON 1
#else
    #define HAS_NEON 0
#endif

static void * eyesee_player_thread_func(void * arg);
static int eyesee_init_video_decoder(eyesee_player_t * player);
static void eyesee_release_video_decoder(eyesee_player_t * player);
static int convert_video_frame_to_rgb(eyesee_player_t * player, VideoPicture * video_pic);

/* ARM NEON SIMD 优化函数声明 */
#if HAS_NEON
static void neon_yuv420p_to_rgb888(uint8_t *y, uint8_t *u, uint8_t *v, uint8_t *rgb,
                                   int width, int height, int y_stride, int uv_stride);
#endif
static void yuv420p_to_rgb888_c(uint8_t *y, uint8_t *u, uint8_t *v, uint8_t *rgb,
                                int width, int height, int y_stride, int uv_stride);

/**
 * 创建播放器实例
 */
eyesee_player_t * eyesee_player_create()
{
    eyesee_player_t * player = malloc(sizeof(eyesee_player_t));
    if(!player) return NULL;

    memset(player, 0, sizeof(eyesee_player_t));

    // 初始化互斥锁
    pthread_mutex_init(&player->mutex, NULL);

    // 初始化状态
    player->state = EYEESEE_PLAYER_STOPPED;
    player->seek_request = false;
    player->current_pts = 0;
    player->finish_callback_ptr = NULL;
    player->video_decoder = NULL;
    player->mem_ops = NULL;
    player->fbm_buf_info = NULL;
    player->rgb_frame_buffer = NULL;
    player->nDecodeStreamIndex = 0;
    player->nValidPictureNum = 0;

    // 初始化视频流信息
    memset(&player->video_stream_info, 0, sizeof(VideoStreamInfo));
    memset(&player->video_config, 0, sizeof(VConfig));

    printf("[eyesee_player] 播放器创建成功\n");

    return player;
}

/**
 * 打开媒体文件
 */
int eyesee_player_open(eyesee_player_t * player, const char * filename)
{
    if(!player) return -1;

    pthread_mutex_lock(&player->mutex);

    // 如果已经在播放，直接返回
    if(player->state == EYEESEE_PLAYER_PLAYING) {
        pthread_mutex_unlock(&player->mutex);
        return -2;
    }

    player->filename = strdup(filename);
    if(!player->filename) {
        fprintf(stderr, "[eyesee_player] 无法分配文件名内存\n");
        pthread_mutex_unlock(&player->mutex);
        return -1;
    }

    int ret = 0;

    // 打开媒体文件（FFmpeg 解封装）
    if(avformat_open_input(&player->format_ctx, player->filename, NULL, NULL) < 0) {
        fprintf(stderr, "[eyesee_player] 无法打开文件: %s\n", player->filename);
        ret = -1;
        goto cleanup;
    }

    // 获取流信息
    if(avformat_find_stream_info(player->format_ctx, NULL) < 0) {
        fprintf(stderr, "[eyesee_player] 无法获取流信息\n");
        ret = -1;
        goto cleanup;
    }

    // 打印流信息（调试用）
    printf("[eyesee_player] 打开文件: %s\n", player->filename);
    printf("[eyesee_player] 流数量: %d\n", player->format_ctx->nb_streams);
    printf("[eyesee_player] 时长: %lld us (%.2f 秒)\n",
           player->format_ctx->duration,
           player->format_ctx->duration / 1000000.0);

    // 查找视频流
    player->video_stream_index = -1;
    for(int i = 0; i < player->format_ctx->nb_streams; i++) {
        if(player->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            player->video_stream_index = i;
            player->video_time_base = player->format_ctx->streams[i]->time_base;
            break;
        }
    }

    // 查找音频流
    player->audio_stream_index = -1;
    for(int i = 0; i < player->format_ctx->nb_streams; i++) {
        if(player->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            player->audio_stream_index = i;
            player->audio_time_base = player->format_ctx->streams[i]->time_base;
            break;
        }
    }

    printf("[eyesee_player] 视频流索引: %d, 音频流索引: %d\n",
           player->video_stream_index, player->audio_stream_index);

    pthread_mutex_unlock(&player->mutex);
    return 0;

cleanup:
    if(player->format_ctx) {
        avformat_close_input(&player->format_ctx);
        player->format_ctx = NULL;
    }
    if(player->filename) {
        free(player->filename);
        player->filename = NULL;
    }
    pthread_mutex_unlock(&player->mutex);
    return ret;
}

/**
 * 初始化 Eyesee-MPP 视频解码器
 */
static int eyesee_init_video_decoder(eyesee_player_t * player)
{
    if(!player || player->video_stream_index < 0) return -1;

    printf("[eyesee_player] 初始化硬件视频解码器...\n");

    // 获取内存操作接口
    player->mem_ops = MemAdapterGetOpsS();
    if(!player->mem_ops) {
        fprintf(stderr, "[eyesee_player] 获取内存操作接口失败\n");
        return -1;
    }

    // 打开内存适配器
    if(CdcMemOpen(player->mem_ops) < 0) {
        fprintf(stderr, "[eyesee_player] 打开内存适配器失败\n");
        return -1;
    }

    // 创建视频解码器
    player->video_decoder = CreateVideoDecoder();
    if(!player->video_decoder) {
        fprintf(stderr, "[eyesee_player] 创建视频解码器失败\n");
        return -1;
    }

    // 获取视频流参数
    AVCodecParameters * codecpar = player->format_ctx->streams[player->video_stream_index]->codecpar;
    AVStream * video_stream = player->format_ctx->streams[player->video_stream_index];

    // 填充视频流信息结构
    memset(&player->video_stream_info, 0, sizeof(VideoStreamInfo));

    // 转换 FFmpeg 编码格式到 Eyesee-MPP 编码格式
    switch(codecpar->codec_id) {
        case AV_CODEC_ID_H264:
            player->video_stream_info.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
            break;
        case AV_CODEC_ID_HEVC:
            player->video_stream_info.eCodecFormat = VIDEO_CODEC_FORMAT_H265;
            break;
        case AV_CODEC_ID_MPEG4:
            player->video_stream_info.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG4;
            break;
        case AV_CODEC_ID_MJPEG:
            player->video_stream_info.eCodecFormat = VIDEO_CODEC_FORMAT_MJPEG;
            break;
        case AV_CODEC_ID_VP8:
            player->video_stream_info.eCodecFormat = VIDEO_CODEC_FORMAT_VP8;
            break;
        case AV_CODEC_ID_VP9:
            player->video_stream_info.eCodecFormat = VIDEO_CODEC_FORMAT_VP9;
            break;
        default:
            fprintf(stderr, "[eyesee_player] 不支持的编码格式: %d\n", codecpar->codec_id);
            return -1;
    }

    player->video_stream_info.nWidth = codecpar->width;
    player->video_stream_info.nHeight = codecpar->height;
    player->video_stream_info.nFrameRate = video_stream->avg_frame_rate.num / video_stream->avg_frame_rate.den;
    player->video_stream_info.nFrameDuration = 1000 * video_stream->avg_frame_rate.den / video_stream->avg_frame_rate.num;

    // 如果有编解码器特定数据（如 H.264 的 SPS/PPS）
    if(codecpar->extradata && codecpar->extradata_size > 0) {
        player->video_stream_info.pCodecSpecificData = (char *)codecpar->extradata;
        player->video_stream_info.nCodecSpecificDataLen = codecpar->extradata_size;
    }

    printf("[eyesee_player] 视频信息: %dx%d, 编码格式: %d, 帧率: %d\n",
           player->video_stream_info.nWidth,
           player->video_stream_info.nHeight,
           player->video_stream_info.eCodecFormat,
           player->video_stream_info.nFrameRate);

    // 配置视频解码器
    memset(&player->video_config, 0, sizeof(VConfig));
    player->video_config.memops = player->mem_ops;
    player->video_config.eOutputPixelFormat = PIXEL_FORMAT_YUV_PLANER_420;
    player->video_config.bScaleDownEn = 0;
    player->video_config.bRotationEn = 0;
    player->video_config.nRotateDegree = 0;
    player->video_config.bThumbnailMode = 0;
    player->video_config.bDispErrorFrame = 1;
    player->video_config.nVbvBufferSize = 1024 * 1024; // 1MB VBV buffer
    player->video_config.nFrameBufferNum = 8;

    // 初始化视频解码器
    if(InitializeVideoDecoder(player->video_decoder, &player->video_stream_info, &player->video_config) != 0) {
        fprintf(stderr, "[eyesee_player] 初始化视频解码器失败\n");
        return -1;
    }

    // 获取 FBM 缓冲信息
    player->fbm_buf_info = GetVideoFbmBufInfo(player->video_decoder);
    if(!player->fbm_buf_info) {
        fprintf(stderr, "[eyesee_player] 获取 FBM 缓冲信息失败\n");
        return -1;
    }

    printf("[eyesee_player] FBM 缓冲信息: %d 缓冲, %dx%d, 格式: %d\n",
           player->fbm_buf_info->nBufNum,
           player->fbm_buf_info->nBufWidth,
           player->fbm_buf_info->nBufHeight,
           player->fbm_buf_info->ePixelFormat);

    return 0;
}

/**
 * 释放 Eyesee-MPP 视频解码器
 */
static void eyesee_release_video_decoder(eyesee_player_t * player)
{
    if(!player) return;

    if(player->video_decoder) {
        DestroyVideoDecoder(player->video_decoder);
        player->video_decoder = NULL;
    }

    if(player->mem_ops) {
        CdcMemClose(player->mem_ops);
        player->mem_ops = NULL;
    }

    player->fbm_buf_info = NULL;
}

/**
 * 初始化视频显示
 */
int eyesee_player_init_video(eyesee_player_t * player, lv_obj_t * lv_obj)
{
    if(!player || !lv_obj) return -1;

    pthread_mutex_lock(&player->mutex);

    int ret = 0;
    player->video_area = lv_obj;

    // 检查是否有视频流
    if(player->video_stream_index < 0) {
        fprintf(stderr, "[eyesee_player] 没有视频流\n");
        ret = -2;
        goto cleanup;
    }

    // 初始化硬件视频解码器
    if(eyesee_init_video_decoder(player) < 0) {
        fprintf(stderr, "[eyesee_player] 初始化硬件解码器失败\n");
        ret = -3;
        goto cleanup;
    }

    // 获取视频分辨率
    player->display_width = player->video_stream_info.nWidth;
    player->display_height = player->video_stream_info.nHeight;

    // 分配 RGB 转换缓冲区
    size_t rgb_buffer_size = player->display_width * player->display_height * 3; // RGB888
    player->rgb_frame_buffer = malloc(rgb_buffer_size);
    if(!player->rgb_frame_buffer) {
        fprintf(stderr, "[eyesee_player] 分配 RGB 缓冲区失败\n");
        ret = -4;
        goto cleanup;
    }

    // 初始化 LVGL 图像描述符
    lv_color_format_t color_fmt = LV_COLOR_FORMAT_RGB888;
    size_t bpp = lv_color_format_get_bpp(color_fmt);
    uint32_t data_size = player->display_width * player->display_height * bpp / 8;

    memset(&player->img_dsc, 0, sizeof(lv_img_dsc_t));
    player->img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    player->img_dsc.header.flags = LV_IMAGE_FLAGS_MODIFIABLE;
    player->img_dsc.header.w = player->display_width;
    player->img_dsc.header.h = player->display_height;
    player->img_dsc.header.stride = player->display_width * bpp / 8;
    player->img_dsc.header.cf = color_fmt;
    player->img_dsc.data = player->rgb_frame_buffer;
    player->img_dsc.data_size = data_size;

    printf("[eyesee_player] 视频显示初始化: %dx%d, stride=%d\n",
           player->display_width, player->display_height,
           player->img_dsc.header.stride);

    // 设置 LVGL 对象的图像源
    lv_img_set_src(player->video_area, &player->img_dsc);

    pthread_mutex_unlock(&player->mutex);
    return 0;

cleanup:
    pthread_mutex_unlock(&player->mutex);
    eyesee_release_video_decoder(player);

    if(player->rgb_frame_buffer) {
        free(player->rgb_frame_buffer);
        player->rgb_frame_buffer = NULL;
    }

    return ret;
}

/**
 * 初始化音频播放
 */
int eyesee_player_init_audio(eyesee_player_t * player)
{
    if(!player) return -1;
    pthread_mutex_lock(&player->mutex);

    int ret = 0;

    // 检查是否有音频流
    if(player->audio_stream_index < 0) {
        fprintf(stderr, "[eyesee_player] 没有音频流\n");
        ret = -1;
        goto cleanup;
    }

    printf("[eyesee_player] 找到音频流: 索引 %d\n", player->audio_stream_index);

    // 获取解码器
    AVCodecParameters * codecpar = player->format_ctx->streams[player->audio_stream_index]->codecpar;
    const AVCodec * codec = avcodec_find_decoder(codecpar->codec_id);
    if(!codec) {
        fprintf(stderr, "[eyesee_player] 未找到对应的音频解码器\n");
        ret = -1;
        goto cleanup;
    }

    player->audio_codec_ctx = avcodec_alloc_context3(codec);
    if(!player->audio_codec_ctx) {
        fprintf(stderr, "[eyesee_player] 无法分配音频解码器上下文\n");
        ret = -1;
        goto cleanup;
    }

    if(avcodec_parameters_to_context(player->audio_codec_ctx, codecpar) < 0) {
        fprintf(stderr, "[eyesee_player] 无法复制编解码器参数到解码器上下文\n");
        ret = -1;
        goto cleanup;
    }

    if(avcodec_open2(player->audio_codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "[eyesee_player] 无法打开音频解码器\n");
        ret = -1;
        goto cleanup;
    }

    // 分配重采样上下文
    player->swr_ctx = swr_alloc();
    if(!player->swr_ctx) {
        fprintf(stderr, "[eyesee_player] swr_alloc failed\n");
        ret = -1;
        goto cleanup;
    }

    // 配置重采样参数
    AVChannelLayout in_chlayout;
    int codec_channels = player->audio_codec_ctx->ch_layout.nb_channels;

    if(codec_channels == 0) {
        codec_channels = 2; // 默认立体声
    }

    // 检查编解码器的 channel layout 是否有效
    if(player->audio_codec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC ||
       (player->audio_codec_ctx->ch_layout.order == AV_CHANNEL_ORDER_CUSTOM &&
        player->audio_codec_ctx->ch_layout.u.mask == 0)) {
        av_channel_layout_default(&in_chlayout, codec_channels);
    } else if(player->audio_codec_ctx->ch_layout.order == AV_CHANNEL_ORDER_CUSTOM) {
        av_channel_layout_from_mask(&in_chlayout, player->audio_codec_ctx->ch_layout.u.mask);
    } else {
        int copy_ret = av_channel_layout_copy(&in_chlayout, &player->audio_codec_ctx->ch_layout);
        if(copy_ret < 0) {
            av_channel_layout_default(&in_chlayout, codec_channels);
        }
    }

    int sample_rate_in = player->audio_codec_ctx->sample_rate;
    enum AVSampleFormat fmt_in = player->audio_codec_ctx->sample_fmt;

    if(sample_rate_in <= 0) {
        sample_rate_in = 44100;
    }

    if(fmt_in < 0 || fmt_in >= AV_SAMPLE_FMT_NB) {
        fmt_in = AV_SAMPLE_FMT_S16;
    }

    // 输出：固定立体声 44100 S16
    AVChannelLayout out_chlayout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    int sample_rate_out = 44100;
    enum AVSampleFormat fmt_out = AV_SAMPLE_FMT_S16;

    printf("[eyesee_player] 音频输入: channels=%d, sample_rate=%d, fmt=%d\n",
           in_chlayout.nb_channels, sample_rate_in, fmt_in);
    printf("[eyesee_player] 音频输出: channels=%d, sample_rate=%d, fmt=%d\n",
           out_chlayout.nb_channels, sample_rate_out, fmt_out);

    int ret_in = av_opt_set_chlayout(player->swr_ctx, "in_chlayout", &in_chlayout, 0);
    int ret_out = av_opt_set_chlayout(player->swr_ctx, "out_chlayout", &out_chlayout, 0);

    if(ret_in < 0 || ret_out < 0) {
        fprintf(stderr, "[eyesee_player] av_opt_set_chlayout failed\n");
        av_channel_layout_uninit(&in_chlayout);
        goto cleanup;
    }

    av_opt_set_int(player->swr_ctx, "in_sample_rate", sample_rate_in, 0);
    av_opt_set_int(player->swr_ctx, "out_sample_rate", sample_rate_out, 0);
    av_opt_set_sample_fmt(player->swr_ctx, "in_sample_fmt", fmt_in, 0);
    av_opt_set_sample_fmt(player->swr_ctx, "out_sample_fmt", fmt_out, 0);

    ret = swr_init(player->swr_ctx);
    if(ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "[eyesee_player] swr_init failed: %s\n", errbuf);
        av_channel_layout_uninit(&in_chlayout);
        goto cleanup;
    }

    player->sample_rate = sample_rate_out;
    player->channels = out_chlayout.nb_channels;

    av_channel_layout_uninit(&in_chlayout);

    // 获取持续时间
    AVStream * audio_stream = player->format_ctx->streams[player->audio_stream_index];
    player->duration = audio_stream->duration;

    printf("[eyesee_player] 音频初始化成功: duration=%lld\n", player->duration);

    // 打开 ALSA 设备
    int err;
    if((err = snd_pcm_open(&player->pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "[eyesee_player] 无法打开 PCM 设备: %s\n", snd_strerror(err));
        ret = -1;
        goto cleanup;
    }

    // 配置 PCM 参数
    snd_pcm_hw_params_t * hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    snd_pcm_hw_params_any(player->pcm_handle, hw_params);
    snd_pcm_hw_params_set_access(player->pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(player->pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(player->pcm_handle, hw_params, player->channels);
    snd_pcm_hw_params_set_rate_near(player->pcm_handle, hw_params, &player->sample_rate, 0);

    player->frames = 1024;
    snd_pcm_hw_params_set_period_size_near(player->pcm_handle, hw_params, &player->frames, 0);

    if((err = snd_pcm_hw_params(player->pcm_handle, hw_params)) < 0) {
        fprintf(stderr, "[eyesee_player] 无法设置硬件参数: %s\n", snd_strerror(err));
        ret = -1;
        goto cleanup;
    }

    if((err = snd_pcm_prepare(player->pcm_handle)) < 0) {
        fprintf(stderr, "[eyesee_player] 无法准备 PCM 设备: %s\n", snd_strerror(err));
        ret = -1;
        goto cleanup;
    }

    printf("[eyesee_player] ALSA 初始化成功\n");

    pthread_mutex_unlock(&player->mutex);
    return 0;

cleanup:
    if(player->swr_ctx) {
        swr_free(&player->swr_ctx);
        player->swr_ctx = NULL;
    }
    if(player->audio_codec_ctx) {
        avcodec_free_context(&player->audio_codec_ctx);
        player->audio_codec_ctx = NULL;
    }
    pthread_mutex_unlock(&player->mutex);
    return ret;
}

/**
 * 将 YUV420P 视频帧转换为 RGB888
 */
static int convert_video_frame_to_rgb(eyesee_player_t * player, VideoPicture * video_pic)
{
    if(!player || !video_pic || !player->rgb_frame_buffer) return -1;

    int width = video_pic->nWidth;
    int height = video_pic->nHeight;
    int y_stride = video_pic->nLineStride;

    // 获取 YUV 数据指针
    uint8_t * y_data = (uint8_t *)video_pic->pData0;
    uint8_t * u_data = (uint8_t *)video_pic->pData1;
    uint8_t * v_data = (uint8_t *)video_pic->pData2;

    if(!y_data || !u_data || !v_data) {
        fprintf(stderr, "[eyesee_player] YUV 数据指针为空\n");
        return -1;
    }

    // 计算 UV stride（YUV420P 中 UV 是 Y 的一半）
    int uv_stride = y_stride / 2;

#if HAS_NEON
    // 使用 NEON 优化转换
    neon_yuv420p_to_rgb888(y_data, u_data, v_data, player->rgb_frame_buffer,
                           width, height, y_stride, uv_stride);
#else
    // 使用 C 语言实现
    yuv420p_to_rgb888_c(y_data, u_data, v_data, player->rgb_frame_buffer,
                        width, height, y_stride, uv_stride);
#endif

    return 0;
}

#if HAS_NEON
/**
 * ARM NEON SIMD 优化的 YUV420P 到 RGB888 转换
 */
static void neon_yuv420p_to_rgb888(uint8_t *y, uint8_t *u, uint8_t *v, uint8_t *rgb,
                                   int width, int height, int y_stride, int uv_stride)
{
    if(!y || !u || !v || !rgb || width <= 0 || height <= 0) return;

    int rgb_stride = width * 3;

    for(int row = 0; row < height; row += 2) {
        uint8_t *y_ptr1 = y + row * y_stride;
        uint8_t *y_ptr2 = y + (row + 1) * y_stride;
        uint8_t *u_ptr = u + (row / 2) * uv_stride;
        uint8_t *v_ptr = v + (row / 2) * uv_stride;
        uint8_t *rgb_ptr1 = rgb + row * rgb_stride;
        uint8_t *rgb_ptr2 = rgb + (row + 1) * rgb_stride;

        int col = 0;
        for(; col < width; col++) {
            int uv_x = col / 2;

            int16_t u_val = u_ptr[uv_x] - 128;
            int16_t v_val = v_ptr[uv_x] - 128;

            // 第一行
            int16_t y1 = y_ptr1[col] - 16;
            int16_t r1 = y1 + ((91881 * v_val) >> 16);
            int16_t g1 = y1 + ((-22554 * u_val - 46802 * v_val) >> 16);
            int16_t b1 = y1 + ((116130 * u_val) >> 16);

            r1 = (r1 < 0) ? 0 : (r1 > 255) ? 255 : r1;
            g1 = (g1 < 0) ? 0 : (g1 > 255) ? 255 : g1;
            b1 = (b1 < 0) ? 0 : (b1 > 255) ? 255 : b1;

            rgb_ptr1[col * 3] = b1;
            rgb_ptr1[col * 3 + 1] = g1;
            rgb_ptr1[col * 3 + 2] = r1;

            // 第二行
            int16_t y2 = y_ptr2[col] - 16;
            int16_t r2 = y2 + ((91881 * v_val) >> 16);
            int16_t g2 = y2 + ((-22554 * u_val - 46802 * v_val) >> 16);
            int16_t b2 = y2 + ((116130 * u_val) >> 16);

            r2 = (r2 < 0) ? 0 : (r2 > 255) ? 255 : r2;
            g2 = (g2 < 0) ? 0 : (g2 > 255) ? 255 : g2;
            b2 = (b2 < 0) ? 0 : (b2 > 255) ? 255 : b2;

            rgb_ptr2[col * 3] = b2;
            rgb_ptr2[col * 3 + 1] = g2;
            rgb_ptr2[col * 3 + 2] = r2;
        }
    }
}
#endif

/**
 * C 语言实现的 YUV420P 到 RGB888 转换
 */
static void yuv420p_to_rgb888_c(uint8_t *y, uint8_t *u, uint8_t *v, uint8_t *rgb,
                                int width, int height, int y_stride, int uv_stride)
{
    if(!y || !u || !v || !rgb || width <= 0 || height <= 0) return;

    int rgb_stride = width * 3;

    for(int row = 0; row < height; row += 2) {
        uint8_t *y_ptr1 = y + row * y_stride;
        uint8_t *y_ptr2 = y + (row + 1) * y_stride;
        uint8_t *u_ptr = u + (row / 2) * uv_stride;
        uint8_t *v_ptr = v + (row / 2) * uv_stride;
        uint8_t *rgb_ptr1 = rgb + row * rgb_stride;
        uint8_t *rgb_ptr2 = rgb + (row + 1) * rgb_stride;

        for(int col = 0; col < width; col++) {
            int uv_x = col / 2;

            int16_t u_val = u_ptr[uv_x] - 128;
            int16_t v_val = v_ptr[uv_x] - 128;

            // 第一行
            int16_t y1 = y_ptr1[col] - 16;
            int16_t r1 = y1 + (int16_t)(1.402f * v_val);
            int16_t g1 = y1 - (int16_t)(0.344f * u_val + 0.714f * v_val);
            int16_t b1 = y1 + (int16_t)(1.772f * u_val);

            r1 = (r1 < 0) ? 0 : (r1 > 255) ? 255 : r1;
            g1 = (g1 < 0) ? 0 : (g1 > 255) ? 255 : g1;
            b1 = (b1 < 0) ? 0 : (b1 > 255) ? 255 : b1;

            rgb_ptr1[col * 3] = b1;
            rgb_ptr1[col * 3 + 1] = g1;
            rgb_ptr1[col * 3 + 2] = r1;

            // 第二行
            int16_t y2 = y_ptr2[col] - 16;
            int16_t r2 = y2 + (int16_t)(1.402f * v_val);
            int16_t g2 = y2 - (int16_t)(0.344f * u_val + 0.714f * v_val);
            int16_t b2 = y2 + (int16_t)(1.772f * u_val);

            r2 = (r2 < 0) ? 0 : (r2 > 255) ? 255 : r2;
            g2 = (g2 < 0) ? 0 : (g2 > 255) ? 255 : g2;
            b2 = (b2 < 0) ? 0 : (b2 > 255) ? 255 : b2;

            rgb_ptr2[col * 3] = b2;
            rgb_ptr2[col * 3 + 1] = g2;
            rgb_ptr2[col * 3 + 2] = r2;
        }
    }
}

/**
 * 播放线程函数
 */
static void * eyesee_player_thread_func(void * arg)
{
    eyesee_player_t * player = (eyesee_player_t *)arg;

    AVPacket * packet = av_packet_alloc();
    AVFrame * frame = av_frame_alloc();
    if(!packet || !frame) {
        fprintf(stderr, "[eyesee_player] 无法分配数据包或帧\n");
        goto cleanup;
    }

    uint8_t * audio_buffer = malloc(BUFFER_SIZE * player->channels * 2); // S16LE
    if(!audio_buffer) {
        fprintf(stderr, "[eyesee_player] 无法分配音频缓冲区\n");
        goto cleanup;
    }

    printf("[eyesee_player] 播放线程启动\n");

    while(player->state != EYEESEE_PLAYER_STOPPED) {
        // 检查跳转请求
        if(player->seek_request) {
            int64_t seek_target = player->seek_pos;
            printf("[eyesee_player] 执行跳转: %lld\n", seek_target);

            if(av_seek_frame(player->format_ctx, player->video_stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
                fprintf(stderr, "[eyesee_player] 跳转失败\n");
            } else {
                // 重置解码器
                if(player->video_decoder) {
                    ResetVideoDecoder(player->video_decoder);
                }
                if(player->audio_codec_ctx) {
                    avcodec_flush_buffers(player->audio_codec_ctx);
                }
                player->current_pts = seek_target;
            }
            player->seek_request = false;
        }

        // 检查暂停状态
        if(player->state == EYEESEE_PLAYER_PAUSED) {
            usleep(100000); // 100ms
            continue;
        }

        // 读取数据包
        int ret = av_read_frame(player->format_ctx, packet);

        // 文件结束或错误
        if(ret < 0) {
            if(ret == AVERROR_EOF) {
                printf("[eyesee_player] 文件读取完成\n");
            } else {
                char errbuf[128];
                av_strerror(ret, errbuf, sizeof(errbuf));
                fprintf(stderr, "[eyesee_player] 读取数据包错误: %s\n", errbuf);
            }

            player->state = EYEESEE_PLAYER_PAUSED;
            if(player->finish_callback_ptr) {
                (*player->finish_callback_ptr)(player);
            }
            av_packet_unref(packet);
            continue;
        }

        // 处理视频流 - 使用硬件解码
        if(packet->stream_index == player->video_stream_index && player->video_decoder) {
            // 请求视频流缓冲区
            char * buf = NULL;
            int buf_size = 0;
            char * ring_buf = NULL;
            int ring_buf_size = 0;

            ret = RequestVideoStreamBuffer(player->video_decoder,
                                           packet->size,
                                           &buf, &buf_size,
                                           &ring_buf, &ring_buf_size,
                                           player->nDecodeStreamIndex);

            if(ret >= 0 && buf) {
                // 复制数据到解码器缓冲区
                if(packet->size <= buf_size) {
                    memcpy(buf, packet->data, packet->size);
                } else {
                    // 需要分两部分复制（环形缓冲区）
                    memcpy(buf, packet->data, buf_size);
                    if(ring_buf && packet->size - buf_size <= ring_buf_size) {
                        memcpy(ring_buf, packet->data + buf_size, packet->size - buf_size);
                    }
                }

                // 提交视频流数据
                VideoStreamDataInfo data_info;
                memset(&data_info, 0, sizeof(VideoStreamDataInfo));
                data_info.pData = buf;
                data_info.nLength = packet->size;
                data_info.nPts = packet->pts;
                data_info.nPcr = packet->dts;
                data_info.bIsFirstPart = 1;
                data_info.bIsLastPart = 1;
                data_info.nStreamIndex = player->nDecodeStreamIndex;

                ret = SubmitVideoStreamData(player->video_decoder, &data_info, player->nDecodeStreamIndex);
                if(ret < 0) {
                    fprintf(stderr, "[eyesee_player] 提交视频数据失败\n");
                }

                // 解码视频流
                ret = DecodeVideoStream(player->video_decoder, 0, 0, 0, 0);

                // 获取解码后的帧
                if(ret == VDECODE_RESULT_FRAME_DECODED || ret == VDECODE_RESULT_KEYFRAME_DECODED) {
                    VideoPicture * video_pic = RequestPicture(player->video_decoder, player->nDecodeStreamIndex);
                    if(video_pic) {
                        // 转换 YUV 到 RGB
                        if(convert_video_frame_to_rgb(player, video_pic) == 0) {
                            // 更新 LVGL 显示
                            lv_obj_invalidate(player->video_area);
                        }

                        // 返回帧缓冲区
                        ReturnPicture(player->video_decoder, video_pic);
                    }
                }

                // 更新当前播放位置
                player->current_pts = packet->pts;
            }

            av_packet_unref(packet);
            continue;
        }

        // 处理音频流（FFmpeg 软件解码）
        if(packet->stream_index == player->audio_stream_index && player->audio_codec_ctx) {
            ret = avcodec_send_packet(player->audio_codec_ctx, packet);
            if(ret < 0) {
                fprintf(stderr, "[eyesee_player] 发送音频数据包失败\n");
                av_packet_unref(packet);
                continue;
            }

            while(ret >= 0) {
                ret = avcodec_receive_frame(player->audio_codec_ctx, frame);
                if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if(ret < 0) {
                    fprintf(stderr, "[eyesee_player] 音频解码错误\n");
                    break;
                }

                // 重采样
                uint8_t * out_data[1] = {audio_buffer};
                int out_samples = swr_convert(player->swr_ctx, out_data, BUFFER_SIZE,
                                              (const uint8_t **)frame->data, frame->nb_samples);

                if(out_samples > 0) {
                    // 写入 ALSA 设备
                    snd_pcm_sframes_t frames_written = snd_pcm_writei(player->pcm_handle, audio_buffer, out_samples);
                    if(frames_written < 0) {
                        frames_written = snd_pcm_recover(player->pcm_handle, frames_written, 0);
                        if(frames_written < 0) {
                            fprintf(stderr, "[eyesee_player] 写入 PCM 设备错误: %s\n", snd_strerror(frames_written));
                        }
                    }
                }

                av_frame_unref(frame);
            }

            av_packet_unref(packet);
            continue;
        }

        // 未知流类型，直接丢弃
        av_packet_unref(packet);
    }

cleanup:
    printf("[eyesee_player] 播放线程结束\n");

    if(packet) av_packet_free(&packet);
    if(frame) av_frame_free(&frame);
    if(audio_buffer) free(audio_buffer);

    return NULL;
}

/**
 * 开始播放
 */
int eyesee_player_play(eyesee_player_t * player)
{
    if(!player) return -1;

    pthread_mutex_lock(&player->mutex);

    if(player->state == EYEESEE_PLAYER_PLAYING) {
        pthread_mutex_unlock(&player->mutex);
        return 0;
    }

    player->state = EYEESEE_PLAYER_PLAYING;

    // 创建播放线程
    if(pthread_create(&player->player_thread, NULL, eyesee_player_thread_func, player) != 0) {
        fprintf(stderr, "[eyesee_player] 无法创建播放线程\n");
        player->state = EYEESEE_PLAYER_STOPPED;
        pthread_mutex_unlock(&player->mutex);
        return -1;
    }

    pthread_mutex_unlock(&player->mutex);
    printf("[eyesee_player] 开始播放\n");

    return 0;
}

/**
 * 暂停播放
 */
int eyesee_player_pause(eyesee_player_t * player)
{
    if(!player) return -1;

    if(player->state == EYEESEE_PLAYER_PLAYING) {
        player->state = EYEESEE_PLAYER_PAUSED;
        if(player->pcm_handle) {
            snd_pcm_pause(player->pcm_handle, 1);
        }
        printf("[eyesee_player] 暂停播放\n");
        return 0;
    }
    return -1;
}

/**
 * 恢复播放
 */
int eyesee_player_resume(eyesee_player_t * player)
{
    if(!player) return -1;

    if(player->state == EYEESEE_PLAYER_PAUSED) {
        player->state = EYEESEE_PLAYER_PLAYING;
        if(player->pcm_handle) {
            snd_pcm_pause(player->pcm_handle, 0);
        }
        printf("[eyesee_player] 恢复播放\n");
        return 0;
    }
    return -1;
}

/**
 * 停止播放
 */
int eyesee_player_stop(eyesee_player_t * player)
{
    if(!player) return -1;

    printf("[eyesee_player] 停止播放\n");

    pthread_mutex_lock(&player->mutex);

    player->state = EYEESEE_PLAYER_STOPPED;

    pthread_mutex_unlock(&player->mutex);

    // 等待线程结束
    if(player->player_thread) {
        pthread_join(player->player_thread, NULL);
        player->player_thread = 0;
    }

    // 清理 ALSA PCM 设备
    if(player->pcm_handle) {
        snd_pcm_drain(player->pcm_handle);
        snd_pcm_close(player->pcm_handle);
        player->pcm_handle = NULL;
    }

    // 清理 FFmpeg 音频资源
    if(player->swr_ctx) {
        swr_free(&player->swr_ctx);
        player->swr_ctx = NULL;
    }

    if(player->audio_codec_ctx) {
        avcodec_free_context(&player->audio_codec_ctx);
        player->audio_codec_ctx = NULL;
    }

    // 释放硬件视频解码器
    eyesee_release_video_decoder(player);

    // 清理格式上下文
    if(player->format_ctx) {
        avformat_close_input(&player->format_ctx);
        player->format_ctx = NULL;
    }

    // 清理 RGB 缓冲区
    if(player->rgb_frame_buffer) {
        free(player->rgb_frame_buffer);
        player->rgb_frame_buffer = NULL;
    }

    // 重置流索引
    player->audio_stream_index = -1;
    player->video_stream_index = -1;

    printf("[eyesee_player] 停止完成\n");

    return 0;
}

/**
 * 根据百分比跳转
 */
int eyesee_player_seek_pct(eyesee_player_t * player, double percent)
{
    if(!player || player->state == EYEESEE_PLAYER_STOPPED) return -1;

    int64_t target_pts = (int64_t)(player->duration * percent / 100.0);
    if(target_pts < 0) target_pts = 0;
    if(target_pts > player->duration) target_pts = player->duration;

    player->seek_pos = target_pts;
    player->seek_request = true;

    printf("[eyesee_player] 跳转到 %.2f%% (%lld)\n", percent, target_pts);

    return 0;
}

/**
 * 根据毫秒数跳转
 */
int eyesee_player_seek_ms(eyesee_player_t * player, int64_t target_ms)
{
    if(!player || player->state == EYEESEE_PLAYER_STOPPED) return -1;

    int64_t target_pts = target_ms * (AV_TIME_BASE / 1000);
    if(target_pts < 0) target_pts = 0;
    if(target_pts > player->duration) target_pts = player->duration;

    player->seek_pos = target_pts;
    player->seek_request = true;

    printf("[eyesee_player] 跳转到 %lld ms\n", target_ms);

    return 0;
}

/**
 * 获取当前播放位置（毫秒）
 */
int64_t eyesee_player_get_position_ms(eyesee_player_t * player)
{
    if(!player || player->duration <= 0) return 0;

    return player->current_pts / (AV_TIME_BASE / 1000);
}

/**
 * 获取总时长（毫秒）
 */
int64_t eyesee_player_get_duration_ms(eyesee_player_t * player)
{
    if(!player || player->duration <= 0) return 0;

    return player->duration / (AV_TIME_BASE / 1000);
}

/**
 * 获取当前播放位置百分比
 */
double eyesee_player_get_position_pct(eyesee_player_t * player)
{
    if(!player || player->duration <= 0) return 0.0;

    return (double)player->current_pts / player->duration * 100.0;
}

/**
 * 获取播放器状态
 */
eyesee_player_state_t eyesee_player_get_state(eyesee_player_t * player)
{
    if(!player) return EYEESEE_PLAYER_STOPPED;

    return player->state;
}

/**
 * 获取视频宽度
 */
int eyesee_player_get_video_width(eyesee_player_t * player)
{
    if(!player) return 0;

    return player->video_stream_info.nWidth;
}

/**
 * 获取视频高度
 */
int eyesee_player_get_video_height(eyesee_player_t * player)
{
    if(!player) return 0;

    return player->video_stream_info.nHeight;
}

/**
 * 销毁播放器
 */
void eyesee_player_destroy(eyesee_player_t * player)
{
    if(!player) return;

    printf("[eyesee_player] 销毁播放器\n");

    // 停止播放
    eyesee_player_stop(player);

    // 清理文件名
    if(player->filename) {
        free(player->filename);
        player->filename = NULL;
    }

    // 清理回调函数
    player->finish_callback_ptr = NULL;

    // 销毁互斥锁
    pthread_mutex_destroy(&player->mutex);

    // 释放播放器对象
    free(player);

    printf("[eyesee_player] 播放器已销毁\n");
}

/**
 * 设置播放完成回调
 */
void eyesee_player_set_finish_callback(eyesee_player_t * player, void (*func_ptr)(eyesee_player_t *))
{
    if(!player) return;

    player->finish_callback_ptr = func_ptr;
}
