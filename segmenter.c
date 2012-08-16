//
//  segmenter.c
//  mediasegmenter
//
//  Created by Iliya Grushveskiy on 16.08.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//
#include "config.h"
#include "segmenter.h"
#include "util.h"

#include <math.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/aes.h>

#define max(a,b) (((a) > (b)) ? (a) : (b))

static const char* kExtensionAAC    = "aac";
static const char* kExtensionMP3    = "mp3";
static const char* kExtensionMPEGTS = "ts";

static const char* kFormatADTS      = "adts";
static const char* kFormatMP3       = "mp3";
static const char* kFormatMPEGTS    = "mpegts";

static const unsigned int kAvgSegmentsCount = 128;

static AVStream* copy_stream(AVFormatContext *context, AVStream *source_stream) {
    AVStream *output_stream = avformat_new_stream(context, source_stream->codec->codec);
    
    AVCodecContext *output_context = output_stream->codec, *source_context = source_stream->codec;
    
    output_context->codec_id   = source_context->codec_id;
    output_context->codec_type = source_context->codec_type;
    
    if (!output_context->codec_tag) {
        if (!context->oformat->codec_tag || 
            av_codec_get_id(context->oformat->codec_tag, source_context->codec_tag) ==  output_context->codec_id ||
            av_codec_get_tag(context->oformat->codec_tag, source_context->codec_id) <=0) {
            output_context->codec_tag = source_context->codec_tag;
        }
    }
    
    
    output_context->bit_rate = source_context->bit_rate;
    output_context->rc_max_rate = source_context->rc_max_rate;
    output_context->rc_buffer_size = source_context->rc_buffer_size;
    output_context->field_order = source_context->field_order;
    output_context->extradata = av_mallocz(source_context->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
    
    if (!output_context->extradata) {
        exit(-1);
    }
    
    memcpy(output_context->extradata, source_context->extradata, source_context->extradata_size);
    
    output_context->extradata_size = source_context->extradata_size;
    
    output_context->time_base = source_context->time_base;
    
    switch (output_context->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            output_context->channel_layout = source_context->channel_layout;
            output_context->sample_rate = source_context->sample_rate;
            output_context->channels = source_context->channels;
            output_context->frame_size = source_context->frame_size;
            output_context->audio_service_type = source_context->audio_service_type;
            output_context->block_align = source_context->block_align;
            break;
            
        case AVMEDIA_TYPE_VIDEO:    
            output_context->pix_fmt = source_context->pix_fmt;
            output_context->width = source_context->width;
            output_context->height = source_context->height;
            output_context->has_b_frames = source_context->has_b_frames;
            output_context->sample_aspect_ratio = source_context->sample_aspect_ratio;
            output_stream->sample_aspect_ratio  = output_context->sample_aspect_ratio;
        default:
            break;
    }
    
    return output_stream;
}


static int load_decoder(AVCodecContext *context) {
    AVCodec *codec = avcodec_find_decoder(context->codec_id);
    
    if (!codec || avcodec_open2(context, codec, NULL)) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief allocate segmenter context and set default values
 * @param output segmenter context
 * @return 0 on success, negative error code on failure
 */
int segmenter_alloc_context(SegmenterContext** context) {
    
    SegmenterContext *_context = (SegmenterContext*)malloc(sizeof(SegmenterContext));
    
    if (!_context) {
        return SGERROR(SGERROR_MEM_ALLOC);
    }
    
    _context->output           = NULL;
    
    _context->file_base_name   = NULL;
    _context->media_base_name  = NULL;
    
    _context->segment_file_sequence = 0;
    _context->segment_sequence      = 0;
    _context->segment_index         = 0;
    _context->segment_duration      = 0;
    
    _context->duration         = 0;
    
    _context->target_duration  = 0;
    _context->max_duration     = 0;
    
    _context->max_bitrate      = 0;
    _context->avg_bitrate      = 0;
    
    _context->_pts             = 0;
    _context->_dts             = 0;
     
    _context->eof              = 0;
    
    _context->durations_length = kAvgSegmentsCount;
    _context->durations        = (double*)malloc(sizeof(double) * _context->durations_length);
    
    if (!_context->durations) {
        free(_context);
        return SGERROR(SGERROR_MEM_ALLOC);
    }
    
    _context->bfilter = av_bitstream_filter_init("h264_mp4toannexb");
    
    *context = _context;
    
    return 0;
}

/**
 * @brief free segmenter context
 * @param context segmenter context
 */
void segmenter_free_context(SegmenterContext* context) {
    av_bitstream_filter_close(context->bfilter);
    avformat_free_context(context->output);
    
    free(context->durations);
    free(context);
}

/**
 * @brief initialize segmenter with source context
 * @param context segmenter context
 * @param source input source context
 * @param filter stream filter
 * @return 0 on success, negative error code on failure
 */
int segmenter_init(SegmenterContext *context, AVFormatContext *source, char* file_base_name, char* media_base_name, 
                    double target_duration, int media_filter) {
    int i;
    int video_index = -1, audio_index = -1;
    
    for (i=0; i < source->nb_streams && (video_index < 0 || audio_index < 0); i++) {
        AVStream *_stream = source->streams[i];
        
        switch (_stream->codec->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                
                if ((media_filter & MediaTypeVideo) && !load_decoder(_stream->codec)) {
                    video_index = i;
                } 
                
                break;
                
            case AVMEDIA_TYPE_AUDIO:
                
                if ((media_filter & MediaTypeAudio) && !load_decoder(_stream->codec)) {
                    audio_index = i;
                } 
                
                break;    
                
            default:
                break;
        }
    }
    
    if (video_index < 0 && audio_index < 0) {
        return SGERROR(SGERROR_NO_STREAM);
    }
    
    context->source_video_index = video_index;
    context->source_audio_index = audio_index;
    
    AVOutputFormat *oformat;
    
    if (video_index < 0) {
        switch (source->streams[audio_index]->codec->codec_id) {
            case CODEC_ID_AAC:
                oformat = av_guess_format(kFormatADTS, NULL, NULL);
                context->extension = kExtensionAAC;
                break;
            case CODEC_ID_MP3:    
                oformat = av_guess_format(kFormatMP3, NULL, NULL);
                context->extension = kExtensionMP3;
            default:
                oformat = av_guess_format(kFormatMPEGTS, NULL, NULL);
                context->extension = kExtensionMPEGTS;
                break;
        }
    } else {
        oformat = av_guess_format(kFormatMPEGTS, NULL, NULL);
        context->extension = kExtensionMPEGTS;
    }
    
    if (!oformat) {
        return SGERROR(SGERROR_UNSUPPORTED_FORMAT);
    }
    
    if (avformat_alloc_output_context2(&context->output, oformat, NULL, NULL)) {
        return SGERROR(SGERROR_MEM_ALLOC);
    }
    
    context->video = video_index >= 0 ? copy_stream(context->output, source->streams[video_index]) : NULL;
    context->audio = audio_index >= 0 ? copy_stream(context->output, source->streams[audio_index]) : NULL;
    
    context->file_base_name  = file_base_name;
    context->media_base_name = media_base_name;
    context->target_duration = target_duration;
    
    return 0;
}

static int set_segment_duration(SegmenterContext *context, unsigned int index, double duration) {
    
    if (index - context->segment_sequence >= context->durations_length) {
        context->durations_length += kAvgSegmentsCount;
        context->durations = (double*)realloc(context->durations, context->durations_length * sizeof(double));
        
        if (!context->durations) {
            return SGERROR(SGERROR_MEM_ALLOC);
        }
    }
    
    context->durations[index - context->segment_sequence] = duration;
    context->max_duration = max(context->max_duration, duration);
    
    return 0;
}

static inline double segment_duration(SegmenterContext *context, unsigned int segment) {
    return context->durations[segment - context->segment_sequence];
}


/**
 * @brief starts next segment
 * @param context segmenter context
 * @return 0 on success, negative error code on failure
 */
static int start_segment(SegmenterContext *context) { 
    char *filename;
    int   length;
    
    length = snprintf(NULL, 0, "%s/%s%u.%s", context->file_base_name, context->media_base_name, context->segment_index, context->extension);
    
    if (!(filename = (char*)alloca(sizeof(char) * (length + 1)))) {
        return SGERROR(SGERROR_MEM_ALLOC);
    }
    
    snprintf(filename, length + 1, "%s/%s%u.%s", context->file_base_name, context->media_base_name, context->segment_index, context->extension);
    
    
    if (avio_open(&context->output->pb, filename, AVIO_FLAG_WRITE)) {
        return SGERROR(SGERROR_FILE_WRITE);
    }
    
    if (context->segment_index == 0) {
        avformat_write_header(context->output, NULL);
    }
    
    return 0;
}

/**
 * @brief finish segment
 * @param context segmenter context
 * @return 0 on success, negative error code on failure
 */
static int finish_segment(SegmenterContext *context) {
    AVFormatContext *output = context->output;
    size_t size;
    int    ret;
    
    avio_flush(output->pb);
    size = avio_size(output->pb);
    avio_close(output->pb);
    
    context->max_bitrate = max(context->max_bitrate, size * 8 / context->segment_duration);
    context->avg_bitrate = (context->avg_bitrate * context->segment_index + (size * 8 / context->segment_duration)) / (context->segment_index + 1);
    
    if ((ret = set_segment_duration(context, context->segment_index, context->segment_duration))) {
        return ret;
    }
    
    context->segment_index++;
    context->segment_duration = 0;
    
    return 0;
}

/**
 * @brief open first output segment
 * @param context segmenter context
 * @return 0 on success, negative error code on failure
 */
int segmenter_open(SegmenterContext* context) {
    return start_segment(context);
}

/**
 * @brief close last output segment
 * @param context segmenter context
 * @return 0 on success, negative error code on failure
 */
int segmenter_close(SegmenterContext* context) {
    context->eof = 1;
    
    return finish_segment(context);
}

/**
 * @brief delete segmenets out from current sequence
 * @param context segmenter context
 * @return 0 on success, negative error code on failure
 */
static int clear_segments(SegmenterContext *context) {
    int   length;
    char* filename;
    
    length = snprintf(NULL, 0, "%s/%s%u.%s", context->file_base_name, context->media_base_name, UINT_MAX, context->extension);
    
    if (!(filename = (char*)alloca(sizeof(char) * (length + 1)))) {
        return SGERROR(SGERROR_MEM_ALLOC);
    }
    
    unsigned int i;
    
    for (i = context->segment_file_sequence; i < context->segment_sequence; i++) {
        snprintf(filename, length, "%s/%s%u.%s", context->file_base_name, context->media_base_name, i, context->extension);
        unlink(filename);
    }
    
    context->segment_file_sequence = context->segment_sequence;
        
    return 0;
}

/**
 * @brief set segmenter sequence index
 * @param context segmenter context
 * @param sequence new sequence value
 * @param del whether to delete out sequence segments from disk
 */
int segmenter_set_sequence(SegmenterContext *context, unsigned int sequence, int del) {
    double duration = 0;
    
    int i;
    
    if (sequence <= context->segment_sequence || sequence >= context->segment_index) {
        return 0;
    }
    
    for (i=sequence; i < context->segment_index; i++) {
        duration += segment_duration(context, i);
    }
    
    i = sequence;
    
    while (duration < context->target_duration * 3 && i > context->segment_sequence) {
        duration += segment_duration(context, --i);
    }
        
    sequence = i;
    
    context->max_duration = 0;
    
    for (i = sequence; i < context->segment_index; i++) {
        context->durations[i - sequence] = context->durations[i - context->segment_sequence];
        context->max_duration = max(context->max_duration, context->durations[i - sequence]);
    }
    
    context->segment_sequence = sequence;
    
    if (del) {
        clear_segments(context);
    }
    
    return 0;
}

/**
 * @brief write packet from input to output
 * @param context segmenter context
 * @param source source input
 * @param pkt packet to write
 * @return 0 on success, negative error code on failure
 */
int segmenter_write_pkt(SegmenterContext* context, AVFormatContext *source, AVPacket *pkt) {
    
    AVPacket opkt;
    AVStream *stream, *output_stream;
    
    stream = source->streams[pkt->stream_index];
    
    if (pkt->stream_index != context->source_audio_index && pkt->stream_index != context->source_video_index) {
        av_free_packet(pkt);
        return 0;
    }
    
    av_init_packet(&opkt);
    
    output_stream = pkt->stream_index == context->source_audio_index ? context->audio : context->video;
    
    opkt.stream_index = output_stream->index;
    
    if (pkt->pts != AV_NOPTS_VALUE) {
        opkt.pts = av_rescale_q(pkt->pts, stream->time_base, output_stream->time_base);
    } else {
        opkt.pts = AV_NOPTS_VALUE;
    }
    
    if (pkt->dts == AV_NOPTS_VALUE) {
        opkt.dts = context->_dts;
    } else {
        opkt.dts = av_rescale_q(pkt->dts, stream->time_base, output_stream->time_base);
        context->_dts = opkt.dts;
    }
    
    opkt.duration = av_rescale_q(pkt->duration, stream->time_base, output_stream->time_base);
    opkt.flags    = pkt->flags;
    opkt.data     = pkt->data;
    opkt.size     = pkt->size;
    
    if (pkt->stream_index == context->source_video_index && context->bfilter) {
        AVPacket _pkt = opkt;
        
        av_bitstream_filter_filter(context->bfilter, context->video->codec, NULL, &_pkt.data, &_pkt.size, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
        
        av_free_packet(&opkt);
        opkt = _pkt;
        opkt.destruct = av_destruct_packet;
    }  
    
    context->duration = opkt.pts * av_q2d(output_stream->time_base);
    
    if (context->source_video_index < 0 || (output_stream == context->video && (opkt.flags & AV_PKT_FLAG_KEY))) {
        context->segment_duration = (opkt.pts - context->_pts) * av_q2d(output_stream->time_base);
    }
    
    if (context->segment_duration >= context->target_duration) {
        int ret;
        
        context->_pts = opkt.pts;
        
        if((ret = finish_segment(context))) {
            return ret;
        }
        
        if ((ret = start_segment(context))) {
            return ret;
        }
    }
    
    
    av_write_frame(context->output , &opkt);
    av_free_packet(&opkt);
    
    return 0;
}


static FILE* segmenter_open_playlist(SegmenterContext *context, char *index_file) {
    int length;
    char *filename;
    
    length = snprintf(NULL, 0, "%s/%s", context->file_base_name, index_file);
    
    if (!(filename = (char*)alloca(sizeof(char)*(length+1)))) {
        return NULL;
    }
    
    snprintf(filename, length + 1, "%s/%s", context->file_base_name, index_file);
    
    return fopen(filename, "w");
}

static void segmenter_close_playlist(FILE *out) {
    fclose(out);
}

/**
 * @brief write stream index
 * @param context segmenter context
 * @param index_file index file base name
 * @return 0 on success, negative error code on error 
 */
int segmenter_write_playlist(SegmenterContext *context, IndexType type, char* base_url, char *index_file) {
    
    if (type == IndexTypeVOD && !context->eof) {
        return 0;
    }
    
    FILE *out = segmenter_open_playlist(context, index_file);
    
    if (!out) {
        return SGERROR(SGERROR_FILE_WRITE);
    }
        
    fprintf(out, "#EXTM3U\n"
                 "#EXT-X-TARGETDURATION:%ld\n"
                 "#EXT-X-VERSION:3\n"
                 "#EXT-X-MEDIA-SEQUENCE:%u\n", lround(context->max_duration), context->segment_sequence);
    
    switch (type) {
        case IndexTypeVOD:
            fprintf(out, "#EXT-X-PLAYLIST-TYPE:VOD\n");
            break;
        case IndexTypeEvent:
            fprintf(out, "#EXT-X-PLAYLIST-TYPE:EVENT\n");
            break;
        default:
            break;
    }
    
    int i;
    for (i = context->segment_sequence; i < context->segment_index; i++) {
        fprintf(out, "#EXTINF:%ld,\n"
                     "%s%s%u.%s\n", lround(segment_duration(context, i)), base_url, context->media_base_name, i, context->extension);
    }
    
    if ((type == IndexTypeEvent || type == IndexTypeVOD) && context->eof) {
        fprintf(out, "#EXT-X-ENDLIST");
    }
    
    segmenter_close_playlist(out);
    
    return 0;
}