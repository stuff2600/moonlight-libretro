#include "FFmpegVideoDecoder.hpp"
#include "Log.h"

// Disables the deblocking filter at the cost of image quality
#define DISABLE_LOOP_FILTER 0x1
// Uses the low latency decode flag (disables multithreading)
#define LOW_LATENCY_DECODE 0x2
// Threads process each slice, rather than each frame
#define SLICE_THREADING 0x4
// Uses nonstandard speedup tricks
#define FAST_DECODE 0x8
// Uses bilinear filtering instead of bicubic
#define BILINEAR_FILTERING 0x10
// Uses a faster bilinear filtering with lower image quality
#define FAST_BILINEAR_FILTERING 0x20

#define DECODER_BUFFER_SIZE 92 * 1024

FFmpegVideoDecoder::FFmpegVideoDecoder(IFFmpegHardwareVideoDecoder* hardware_video_decoder) {
    m_hardware_video_decoder = hardware_video_decoder;
    
    pthread_mutex_init(&m_mutex, NULL);
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
    pthread_mutex_destroy(&m_mutex);
    cleanup();
    
    if (m_hardware_video_decoder) {
        delete m_hardware_video_decoder;
    }
}

int FFmpegVideoDecoder::setup(int video_format, int width, int height, int redraw_rate, void *context, int dr_flags) {
    av_log_set_level(AV_LOG_QUIET);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,10,100)
    avcodec_register_all();
#endif
    
    av_init_packet(&m_packet);
    
    int perf_lvl = SLICE_THREADING;
    
    //ffmpeg_decoder = perf_lvl & VAAPI_ACCELERATION ? VAAPI : SOFTWARE;
    switch (video_format) {
        case VIDEO_FORMAT_H264:
            m_decoder = avcodec_find_decoder_by_name("h264");
            break;
        case VIDEO_FORMAT_H265:
            m_decoder = avcodec_find_decoder_by_name("hevc");
            break;
    }
    
    if (m_decoder == NULL) {
        LOG("Couldn't find decoder\n");
        return -1;
    }
    
    m_decoder_context = avcodec_alloc_context3(m_decoder);
    if (m_decoder_context == NULL) {
        LOG("Couldn't allocate context\n");
        return -1;
    }
    
    if (perf_lvl & DISABLE_LOOP_FILTER)
        // Skip the loop filter for performance reasons
        m_decoder_context->skip_loop_filter = AVDISCARD_ALL;
    
    if (perf_lvl & LOW_LATENCY_DECODE)
        // Use low delay single threaded encoding
        m_decoder_context->flags |= AV_CODEC_FLAG_LOW_DELAY;
    
    if (perf_lvl & SLICE_THREADING)
        m_decoder_context->thread_type = FF_THREAD_SLICE;
    else
        m_decoder_context->thread_type = FF_THREAD_FRAME;
    
    m_decoder_context->thread_count = 2;
    
    m_decoder_context->width = width;
    m_decoder_context->height = height;
    m_decoder_context->pix_fmt = AV_PIX_FMT_YUV420P;
    
    int err = avcodec_open2(m_decoder_context, m_decoder, NULL);
    if (err < 0) {
        LOG("Couldn't open codec\n");
        return err;
    }
    
    m_frames_count = 2;
    m_frames = (AVFrame**)malloc(m_frames_count * sizeof(AVFrame*));
    if (m_frames == NULL) {
        LOG("Couldn't allocate frames\n");
        return -1;
    }
    
    for (int i = 0; i < m_frames_count; i++) {
        m_frames[i] = av_frame_alloc();
        if (m_frames[i] == NULL) {
            LOG("Couldn't allocate frame\n");
            return -1;
        }
    }
    
    m_ffmpeg_buffer = (char*)malloc(DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
    if (m_ffmpeg_buffer == NULL) {
        LOG("Not enough memory\n");
        cleanup();
        return -1;
    }
    
    return DR_OK;
}

void FFmpegVideoDecoder::cleanup() {
    if (m_decoder_context) {
        avcodec_close(m_decoder_context);
        av_free(m_decoder_context);
        m_decoder_context = NULL;
    }
    
    if (m_frames) {
        for (int i = 0; i < m_frames_count; i++) {
            if (m_frames[i])
                av_frame_free(&m_frames[i]);
        }
        
        free(m_frames);
        m_frames = nullptr;
    }
    
    if (m_ffmpeg_buffer) {
        free(m_ffmpeg_buffer);
        m_ffmpeg_buffer = nullptr;
    }
}

int FFmpegVideoDecoder::submit_decode_unit(PDECODE_UNIT decode_unit) {
    if (decode_unit->fullLength < DECODER_BUFFER_SIZE) {
        PLENTRY entry = decode_unit->bufferList;
        int length = 0;
        while (entry != NULL) {
            memcpy(m_ffmpeg_buffer + length, entry->data, entry->length);
            length += entry->length;
            entry = entry->next;
        }
        decode(m_ffmpeg_buffer, length);
        
        if (pthread_mutex_lock(&m_mutex) == 0) {
            m_frame = get_frame(true);
            
            // Push event!!
            pthread_mutex_unlock(&m_mutex);
        }
    }
    return DR_OK;
}

int FFmpegVideoDecoder::capabilities() const {
    return CAPABILITY_SLICES_PER_FRAME(4) | CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC | CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC | CAPABILITY_DIRECT_SUBMIT;
}

int FFmpegVideoDecoder::decode(char* indata, int inlen) {
    m_packet.data = (uint8_t *)indata;
    m_packet.size = inlen;
    
    int err = avcodec_send_packet(m_decoder_context, &m_packet);
    if (err < 0) {
        char error[512];
        av_strerror(err, error, sizeof(error));
        LOG_FMT("Decode failed - %s\n", error);
    }
    
    return err < 0 ? err : 0;
}

AVFrame* FFmpegVideoDecoder::get_frame(bool native_frame) {
    int err = avcodec_receive_frame(m_decoder_context, m_frames[m_next_frame]);
    if (err == 0) {
        m_current_frame = m_next_frame;
        m_next_frame = (m_current_frame + 1) % m_frames_count;
        
        if (/*ffmpeg_decoder == SOFTWARE ||*/ native_frame)
            return m_frames[m_current_frame];
    } else if (err != AVERROR(EAGAIN)) {
        char error[512];
        av_strerror(err, error, sizeof(error));
        LOG_FMT("Receive failed - %d/%s\n", err, error);
    }
    return NULL;
}

AVFrame* FFmpegVideoDecoder::frame() const {
    return m_frame;
}
