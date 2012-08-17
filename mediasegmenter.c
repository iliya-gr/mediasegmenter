// mediasegmenter.c
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

#include "config.h"
#include <stdio.h>
#include <getopt.h>
#include <libavformat/avformat.h>
#include "segmenter.h"
#include "util.h"
#include "log.h"

#define DEFAULT_BASE_URL             ""
#define DEFAULT_FILE_BASE            ""
#define DEFAULT_BASE_MEDIA_FILE_NAME "fileSequence"
#define DEFAULT_INDEX_FILE           "prog_index.m3u8"

void print_version() {
    printf("%s: %s\n", PACKAGE, PACKAGE_VERSION);
}

void print_usage(char* name) {
    printf("Usage:%s [options] <file> where options are:\n"
           "\t" "-h        | --help                        : print this help message\n"
           "\t" "-v        | --version                     : print version number\n"
           "\t" "-b <url>  | --base-url=<url>              : base url (omit for relative URLs)\n"
           "\t" "-t <dur>  | --target-duration=<dur>       : target duration for each segment\n"
           "\t" "-f <path> | --file-base=<path>            : path at which to store index and media files\n"
           "\t" "-i <name> | --index-file=<name>           : index file name (default prog_index.m3u8)\n"
           "\t" "-I        | --generate-variant-plist      : generate plist file for variantplaylistcreator\n"
           "\t" "-B <name> | --base-media-file-name=<name> : base media file name (default fileSequence)\n"
           "\t" "-l <path> | --log-file=<path>             : enable log file\n"
           "\t" "-q        | --quiet                       : only output errors\n"
           "\t" "-a        | --audio-only                  : only use audio from the stream\n"
           "\t" "-A        | --video-only                  : only use video from the stream\n"
           "\t" "-l        | --live                        : write live stream index file\n"
           "\t" "-e        | --live-event                  : write live event stream index file\n"
           "\t" "-w <num>  | --sliding-window-entries      : maximum number of entries in index file\n"
           "\t" "-D        | --delete-files                : delete files after they expire\n"
           , name);
}

struct config {
    char *base_url;
    char *file_base;
    char *media_file_name;
    char *index_file;
    char *source_file;
    
    int          media;
    IndexType    type;
    
    int stat;
    int playlist_entries;
    int delete;
    
    double duration;
};

int main(int argc, char **argv) {
    
    sg_log_init(argv[0]);
    
    struct option options_long[] = {
        {"version",                    no_argument,       NULL, 'v'},
        {"help",                       no_argument,       NULL, 'h'},
        {"base-url",                   required_argument, NULL, 'b'},
        {"target-duration",            required_argument, NULL, 't'},
        {"file-base",                  required_argument, NULL, 'f'},
        {"index-file",                 required_argument, NULL, 'i'},
        {"generate-variant-plist",     no_argument,       NULL, 'I'},
        {"base-media-file-name",       required_argument, NULL, 'B'},
        {"quiet",                      no_argument,       NULL, 'q'},
        {"audio-only",                 no_argument,       NULL, 'a'},
        {"video-only",                 no_argument,       NULL, 'A'},
        {"live",                       no_argument,       NULL, 'l'},
        {"live-event",                 no_argument,       NULL, 'e'},
        {"sliding-window-entries",     required_argument, NULL, 'w'},
        {"delete-files",               no_argument,       NULL, 'D'},
        {0, 0, 0, 0}
    };
    
    char* options_short = "vhb:t:f:i:IB:qaAlew:D";
    
    struct config config;
    
    config.base_url             = DEFAULT_BASE_URL;
    config.file_base            = DEFAULT_FILE_BASE;
    config.media_file_name = DEFAULT_BASE_MEDIA_FILE_NAME;
    config.index_file           = DEFAULT_INDEX_FILE;
    config.source_file          = NULL;
    
    config.media = MediaTypeAudio | MediaTypeVideo;
    config.type  = IndexTypeVOD;
    
    config.playlist_entries = 0;
    config.delete           = 0;
    
    config.duration = 10;
    
    int ret;
    int option_index = 0;
    
    opterr = 0;
    
    int c;
    do{
        c = getopt_long(argc, argv, options_short, options_long, &option_index);
        
        switch (c) {
            case 'v': print_version();      exit(EXIT_SUCCESS); break;
            case 'h': print_usage(argv[0]); exit(EXIT_SUCCESS); break;
                
            case 'b': config.base_url        = optarg;       break;
            case 't': config.duration        = atof(optarg); break;
            case 'f': config.file_base       = optarg;       break;
            case 'i': config.index_file      = optarg;       break;
            case 'I': config.stat            = 1;            break;
            case 'B': config.media_file_name = optarg;       break;
                
            case 'q': sg_log_set_level(SG_LOG_FATAL);           break;
            case 'a': config.media            = MediaTypeAudio; break;
            case 'A': config.media            = MediaTypeVideo; break;
            case 'l': config.type             = IndexTypeLive;  break;
            case 'e': config.type             = IndexTypeEvent; break;
            case 'w': config.playlist_entries = atoi(optarg);   break;
            case 'D': config.delete           = 1;              break;
            
            case '?':
                fprintf(stderr ,"%s: invalid option '%s'\n", argv[0], argv[optind - 1]);
                exit(EXIT_FAILURE);
                break;
        }
    } while (c!= -1);
    
    if (optind < argc) {
        config.source_file = argv[optind];
    }
    
    if (!config.source_file){
        sg_log(SG_LOG_FATAL, "no source file was supplied");
        exit(EXIT_FAILURE);
    }
        
    AVFormatContext  *source_context = NULL;
    SegmenterContext *output_context = NULL;
    
    av_register_all();
    
    if(avformat_open_input(&source_context, config.source_file, NULL, NULL)) {
        sg_log(SG_LOG_FATAL, "can't open input file '%s'", config.source_file);
        exit(EXIT_FAILURE);
    }
    
    if (avformat_find_stream_info(source_context, NULL)) {
        sg_log(SG_LOG_WARNING, "Warning: can't load input file info");
    }
    
    if ((ret = segmenter_alloc_context(&output_context))) {
        sg_log(SG_LOG_FATAL, "allocate context, %s", sg_strerror(SGUNERROR(ret)));
        exit(EXIT_FAILURE);
    }
    
    
    if ((ret = segmenter_init(output_context, source_context, config.file_base, config.media_file_name, config.duration, config.media))) {
        sg_log(SG_LOG_FATAL, "initialize context, %s", sg_strerror(SGUNERROR(ret)));
        exit(EXIT_FAILURE);
    }
    
    if((ret = segmenter_open(output_context))){
        sg_log(SG_LOG_FATAL, "open output, %s", sg_strerror(SGUNERROR(ret)));
        exit(EXIT_FAILURE);
    }
    
    AVPacket pkt;
    unsigned int prev_index = 0;
    
    while (av_read_frame(source_context, &pkt) >= 0) {
        
        if ((ret = segmenter_write_pkt(output_context, source_context, &pkt))) {
            sg_log(SG_LOG_FATAL, "write packet, %s", sg_strerror(SGUNERROR(ret)));
            exit(EXIT_FAILURE);
        }
        
        if (prev_index < output_context->segment_index) {
            prev_index = output_context->segment_index;
            
            if (config.playlist_entries && config.type == IndexTypeLive) {
                segmenter_set_sequence(output_context, output_context->segment_index - config.playlist_entries, config.delete);
            }
            
            segmenter_write_playlist(output_context, config.type, config.base_url, config.index_file);
        }
    }
    
    segmenter_close(output_context);
    
    if ((ret = segmenter_write_playlist(output_context, config.type, config.base_url, config.index_file))) {
        sg_log(SG_LOG_FATAL, "write index, %s", sg_strerror(SGUNERROR(ret)));
        exit(EXIT_FAILURE);
    }
    
    segmenter_free_context(output_context);
    
    avformat_close_input(&source_context);
    
    
    return 0;
}