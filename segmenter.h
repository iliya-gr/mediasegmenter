//
//  segmenter.h
//  mediasegmenter
//
//  Created by Iliya Grushveskiy on 16.08.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//
#include <libavformat/avformat.h>

#ifndef __SEGMENTER__
#define __SEGMENTER__

typedef enum {
    MediaTypeAudio     = 0x01,
    MediaTypeVideo     = 0x02,
    MediaTypeSubtitles = 0x04
} MediaType;

typedef enum {
    IndexTypeVOD,
    IndexTypeLive,
    IndexTypeEvent
} IndexType;

typedef struct {
    AVFormatContext          *output;
    AVBitStreamFilterContext *bfilter;
    
    char            *file_base_name, *media_base_name;
    const char      *extension;
    
    AVStream        *video, *audio;
    int             source_video_index, source_audio_index;
    
    unsigned int    segment_start_index;
    
    unsigned int    segment_index;
    double          segment_duration;
    
    double          duration;
    double          target_duration;
    double          max_duration;
    
    double          *durations;
    int             durations_length;
    
    double          avg_bitrate;
    double          max_bitrate;
    
    int64_t         _pts;
    int64_t         _dts;
    
} SegmenterContext;

int  segmenter_alloc_context(SegmenterContext** output);
int  segmenter_init(SegmenterContext *context, AVFormatContext *source, char* file_base_name, char* media_base_name, 
                    double target_duration, int media_filter);

int  segmenter_open(SegmenterContext* context);
int  segmenter_close(SegmenterContext* context);

int  segmenter_write_pkt(SegmenterContext* context, AVFormatContext *source, AVPacket *pkt);

void segmenter_free_context(SegmenterContext* context);

int segmenter_write_index(SegmenterContext *context, IndexType type, char* base_url, char *index_file, unsigned int max_entries, int final);

int segmenter_delete_segments(SegmenterContext *context, unsigned int to_index);

#endif
