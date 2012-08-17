// segmenter.h
// Copyright (C) 2012  Iliya Grushevskiy <iliya.gr@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

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
    
    char            *buf;
    size_t          buf_size;
    
    char            *file_base_name, *media_base_name;
    const char      *extension;
    
    
    AVStream        *video, *audio;
    int             source_video_index, source_audio_index;
    
    unsigned int    segment_file_sequence;
    unsigned int    segment_sequence;
    unsigned int    segment_index;
    double          segment_duration;
    
    double          duration;
    double          target_duration;
    double          max_duration;
    
    double          *durations;
    size_t          durations_size;
    
    double          avg_bitrate;
    double          max_bitrate;
    
    int64_t         _pts;
    int64_t         _dts;
    
    int             eof;
    
} SegmenterContext;

int  segmenter_alloc_context(SegmenterContext**);
int  segmenter_init(SegmenterContext *context, AVFormatContext *source, char* file_base_name, char* media_base_name, 
                        double target_duration, int media_filter);

int  segmenter_open(SegmenterContext*);
int  segmenter_close(SegmenterContext*);

int  segmenter_write_pkt(SegmenterContext* context, AVFormatContext *source, AVPacket *pkt);

void segmenter_free_context(SegmenterContext*);

int  segmenter_set_sequence(SegmenterContext*, unsigned int sequence, int del);
int  segmenter_write_playlist(SegmenterContext*, IndexType type, char* base_url, char *index_file);


#endif
