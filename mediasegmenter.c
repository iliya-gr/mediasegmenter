#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <getopt.h>

#define max(a,b)               (((a) > (b)) ? (a) : (b))
#define log_failure(fmt, ...)  (fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__))
#define log(verbose, fmt, ...) ((verbose) && fprintf(stdout, fmt "\n", ##__VA_ARGS__))

static AVStream*       copy_stream(AVFormatContext *target_context, AVStream *source_stream);
static int             load_decoder(AVCodecContext *context);

static const char* kExtensionAAC    = "aac";
static const char* kExtensionMP3    = "mp3";
static const char* kExtensionMPEGTS = "ts";

static const char* kFormatADTS      = "adts";
static const char* kFormatMP3       = "mp3";
static const char* kFormatMPEGTS    = "mpegts";

static const unsigned int kAvgSegmentsCount = 128;

#define SGERROR(e)         (-(e))
#define SGUNERROR(e)       (-(e))

#define SGERROR_MEM_ALLOC          0x01
#define SGERROR_NO_STREAM          0x02
#define SGERROR_UNSUPPORTED_FORMAT 0x03
#define SGERROR_FILE_WRITE         0x04

typedef enum {
    SegmenterStreamAudio = 0x01,
    SegmenterStreamVideo = 0x02
} SegmenterStreamType;

typedef struct {
    AVFormatContext          *output;
    AVBitStreamFilterContext *bfilter;
    
    char            *file_base_name, *media_base_name;
    const char      *extension;
    
    AVStream        *video, *audio;
    int             source_video_index, source_audio_index;
    
    unsigned int    segment_index;
    double          segment_duration;
    
    double          target_duration;
    double          max_duration;
    
    double          *durations;
    int             durations_length;
    
    double          avg_bitrate;
    double          max_bitrate;
    
    int64_t         _pts;
    int64_t         _dts;
    
} SegmenterContext;

int  segmenter_alloc_context(SegmenterContext** output, char* file_base_name, char* media_base_name, double target_duration);
int  segmenter_init(SegmenterContext *context, AVFormatContext *source, SegmenterStreamType selector);

int  segmenter_open_output(SegmenterContext* context);
int  segmenter_close(SegmenterContext* context);
int  segmenter_next_segment(SegmenterContext* context);

int  segmenter_write_pkt(SegmenterContext* context, AVFormatContext *source, AVPacket *pkt);

void segmenter_free_context(SegmenterContext* context);

int  segmenter_write_index(SegmenterContext* context, char* base_url, char* index_file);

char *segmenter_format_error(int error);

/**
 * @brief allocate segmenter context and set default values
 * @param output segmenter context
 * @param file_base_name segment file path base name e.g. /tmp/
 * @param media_base_name segment file base name e.g. segment-
 * @param target_duration segment target duration 
 * @return 0 on success, negative error code on failure
 */
int segmenter_alloc_context(SegmenterContext** output, char* file_base_name, char* media_base_name, double target_duration) {
    
    SegmenterContext *context = (SegmenterContext*)malloc(sizeof(SegmenterContext));
    
    if (!context) {
        return SGERROR(SGERROR_MEM_ALLOC);
    }
    
    context->output           = NULL;
    
    context->file_base_name   = file_base_name;
    context->media_base_name  = media_base_name;
    
    context->segment_index    = 0;
    context->segment_duration = 0;
    
    context->target_duration  = target_duration;
    context->max_duration     = 0;
    
    context->max_bitrate      = 0;
    context->avg_bitrate      = 0;
    
    context->_pts             = 0;
    context->_dts             = 0;
    
    context->durations_length = kAvgSegmentsCount;
    context->durations        = (double*)malloc(sizeof(double) * context->durations_length);
    
    if (!context->durations) {
        free(context);
        return SGERROR(SGERROR_MEM_ALLOC);
    }
    
    context->bfilter = av_bitstream_filter_init("h264_mp4toannexb");
    
    *output = context;
    
    return 0;
}

/**
 * @brief initialize segmenter with source context
 * @param context segmenter context
 * @param source input source context
 * @param selector stream selector
 * @return 0 on success, negative error code on failure
 */
int segmenter_init(SegmenterContext *context, AVFormatContext *source, SegmenterStreamType stream) {
    int i;
    int video_index = -1, audio_index = -1;
    
    for (i=0; i < source->nb_streams && (video_index < 0 || audio_index < 0); i++) {
        AVStream *_stream = source->streams[i];
        
        switch (_stream->codec->codec_type) {
            case AVMEDIA_TYPE_VIDEO:

                if ((stream & SegmenterStreamVideo) && !load_decoder(_stream->codec)) {
                    video_index = i;
                } 
                
                break;
                
            case AVMEDIA_TYPE_AUDIO:
                
                if ((stream & SegmenterStreamAudio) && !load_decoder(_stream->codec)) {
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
    
    return 0;
}

/**
 * @brief store current segment duration in durations list
 * @param context segmenter context
 * @return 0 on success, negative error code on failure
 */
static int segmenter_store_segment_duration(SegmenterContext *context) {
    
    if (context->segment_index >= context->durations_length) {
        context->durations_length += kAvgSegmentsCount;
        context->durations = (double*)realloc(context->durations, context->durations_length * sizeof(double));
        
        if (!context->durations) {
            return SGERROR(SGERROR_MEM_ALLOC);
        }
    }
    
    context->durations[context->segment_index] = context->segment_duration;
    context->max_duration = max(context->max_duration, context->segment_duration);
    
    return 0;
}

/**
 * @brief saves changes to file and open next segment output
 * @param context segmenter context
 * @return 0 on success, negative error code on failure
 */
int segmenter_next_segment(SegmenterContext* context) {
    AVFormatContext *output = context->output;
    
    int    length;
    char*  filename;
    size_t size;
    int    ret;
    
    avio_flush(output->pb);
    size = avio_size(output->pb);
    avio_close(output->pb);
    
    context->max_bitrate = max(context->max_bitrate, size * 8 / context->segment_duration);
    context->avg_bitrate = (context->avg_bitrate * context->segment_index + (size * 8 / context->segment_duration)) / (context->segment_index + 1);
    
    if ((ret = segmenter_store_segment_duration(context))) {
        return ret;
    }
    
    
    context->segment_index++;
    context->segment_duration = 0;
    
    length = snprintf(NULL, 0, "%s/%s%u.%s", context->file_base_name, context->media_base_name, context->segment_index, context->extension);

    if (!(filename = (char*)alloca(sizeof(char) * (length + 1)))) {
        return SGERROR(SGERROR_MEM_ALLOC);
    }
    
    snprintf(filename, length + 1, "%s/%s%u.%s", context->file_base_name, context->media_base_name, context->segment_index, context->extension);


    if (avio_open(&context->output->pb, filename, AVIO_FLAG_WRITE)) {
        return SGERROR(SGERROR_FILE_WRITE);
    }
    
    return 0;
}

/**
 * @brief open first output segment
 * @param context segmenter context
 * @return 0 on success, negative error code on failure
 */
int segmenter_open_output(SegmenterContext* context) {
    int   length;
    char *filename;
    
    length = snprintf(NULL, 0, "%s/%s%u.%s", context->file_base_name, context->media_base_name, context->segment_index, context->extension);
    
    if (!(filename = (char*)alloca(sizeof(char) * (length + 1)))) {
        return SGERROR(SGERROR_MEM_ALLOC);
    }
    
    snprintf(filename, length + 1, "%s/%s%u.%s", context->file_base_name, context->media_base_name, context->segment_index, context->extension);
    
    if (avio_open(&context->output->pb, filename, AVIO_FLAG_WRITE)) {
        return SGERROR(SGERROR_FILE_WRITE);
    }
    
    avformat_write_header(context->output, NULL);
    
    return 0;
}

/**
 * @brief close last output segment
 * @param context segmenter context
 * @return 0 on success, negative error code on failure
 */
int segmenter_close(SegmenterContext* context) {
    AVFormatContext *output = context->output;
    size_t size;
    int    ret;
    
    avio_flush(output->pb);
    size = avio_size(output->pb);
    avio_close(output->pb);
    
    context->max_bitrate = max(context->max_bitrate, size * 8 / context->segment_duration);
    context->avg_bitrate = (context->avg_bitrate * context->segment_index + (size * 8 / context->segment_duration)) / (context->segment_index + 1);
    
    if ((ret = segmenter_store_segment_duration(context))) {
        return ret;
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
    
    if (context->source_video_index < 0 || (output_stream == context->video && (opkt.flags & AV_PKT_FLAG_KEY))) {
        context->segment_duration = (opkt.pts - context->_pts) * av_q2d(output_stream->time_base);
    }
    
    if (context->segment_duration >= context->target_duration) {
        int ret;
        
        context->_pts = opkt.pts;
        
        if((ret = segmenter_next_segment(context))){
            return ret;
        }
    }
    
    
    av_write_frame(context->output , &opkt);
    av_free_packet(&opkt);
    
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
 * @brief write stream index
 * @param context segmenter context
 * @param index_file index file base name
 * @return 0 on success, negative error code on error 
 */
int segmenter_write_index(SegmenterContext* context, char* base_url, char* index_file) {
    int length;
    char *filename;
    
    length = snprintf(NULL, 0, "%s/%s.m3u8", context->file_base_name, index_file);
    
    if (!(filename = (char*)alloca(sizeof(char)*(length+1)))) {
        return SGERROR(SGERROR_MEM_ALLOC);
    }
        
    snprintf(filename, length + 1, "%s/%s.m3u8", context->file_base_name, index_file);
    
    FILE *out = fopen(filename, "w+");
    
    if (!out) {
        return SGERROR(SGERROR_FILE_WRITE);
    }
    fprintf(out, "#EXTM3U\n"
                 "#EXT-X-PLAYLIST-TYPE:VOD\n"
                 "#EXT-X-TARGETDURATION:%.0lf\n"
                 "#EXT-X-VERSION:3\n"
                 "#EXT-X-MEDIA-SEQUENCE:0\n", context->max_duration);
    int i;
    for (i=0; i <= context->segment_index; i++) {
        fprintf(out, "#EXTINF:%.1lf,\n"
                     "%s%s%u.%s\n", context->durations[i], base_url, context->media_base_name, i, context->extension);
    }
        
    fprintf(out, "#EXT-X-ENDLIST");
    fclose(out);
    
    return 0;
}

char *segmenter_format_error(int error) {
    char* errstr = NULL;
    
    switch (error) {
        case SGERROR_MEM_ALLOC:
            errstr = "can't allocate memory";
            break;
        case SGERROR_FILE_WRITE:
            errstr = "can't open file for writing";
            break;
        case SGERROR_NO_STREAM:
            errstr = "no suitable streams found";
            break;
        case SGERROR_UNSUPPORTED_FORMAT:
            errstr = "unsupported output format";
            break;
        default:
            errstr = "unkown error";
            break;
    }
    
    return strndup(errstr, strlen(errstr));
}

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
           "\t" "-i <name> | --index-file=<name>           : index file name (default prog_index)\n"
           "\t" "-I        | --generate-variant-plist      : generate plist file for variantplaylistcreator\n"
           "\t" "-B <name> | --base-media-file-name=<name> : base media file name (default fileSequence)\n"
           "\t" "-l <path> | --log-file=<path>             : enable log file\n"
           "\t" "-q        | --quiet                       : only output errors\n"
           "\t" "-a        | --audio-only                  : only use audio from the stream\n"
           "\t" "-A        | --video-only                  : only use video from the stream\n"
           , name);
}


#define DEFAULT_BASE_URL             ""
#define DEFAULT_FILE_BASE            ""
#define DEFAULT_BASE_MEDIA_FILE_NAME "fileSequence"
#define DEFAULT_INDEX_FILE           "prog_index"

int main(int argc, char **argv) {
    
    struct option options_long[] = {
        {"version",                    no_argument,       NULL, 'v'},
        {"help",                       no_argument,       NULL, 'h'},
        {"base-url",                   required_argument, NULL, 'b'},
        {"target-duration",            required_argument, NULL, 't'},
        {"file-base",                  required_argument, NULL, 'f'},
        {"index-file",                 required_argument, NULL, 'i'},
        {"generate-variant-plist",     no_argument,       NULL, 'I'},
        {"base-media-file-name",       required_argument, NULL, 'B'},
        {"log-file",                   required_argument, NULL, 'l'},
        {"quiet",                      no_argument,       NULL, 'q'},
        {"audio-only",                 no_argument,       NULL, 'a'},
        {"video-only",                 no_argument,       NULL, 'A'},
        {0, 0, 0, 0}
    };
    
    char* options_short = "vhb:t:f:i:IB:l:qaAVzs";
    
    int ret;
    int option_index = 0;
    
    char *base_url             = DEFAULT_BASE_URL, 
         *file_base            = DEFAULT_FILE_BASE,
         *base_media_file_name = DEFAULT_BASE_MEDIA_FILE_NAME, 
         *index_file           = DEFAULT_INDEX_FILE,
         *log_file             = NULL,
         *source_file          = NULL;
    
    int    generate_variant_plist = 0, quiet = 0, audio_only = 0, video_only = 0;
    double target_duration = 10;
    
    int c;
    do{
        c = getopt_long(argc, argv, options_short, options_long, &option_index);
        
        switch (c) {
            case 'v':
                print_version();
                exit(EXIT_SUCCESS);
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case 'b':
                base_url = optarg;
                break;
            case 't': 
                target_duration = atof(optarg);
                break;
            case 'f': 
                file_base = optarg;
                break;
            case 'i':
                index_file = optarg;
                break;
            case 'I':
                generate_variant_plist = 1;
                break;
            case 'B':
                base_media_file_name = optarg;
                break;
            case 'l':
                log_file = optarg;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'a':
                audio_only = 1;
                break;
            case 'A': 
                video_only = 1;
                break;
        }
    } while (c!= -1);
    
    if (optind < argc) {
        source_file = argv[optind];
    }
    
    if (!source_file){
        log_failure("no source file was supplied");
        exit(EXIT_FAILURE);
    }
    
    if (!file_base) {
        log_failure("no file base was supplied");
        exit(EXIT_FAILURE);
    }
    
    AVFormatContext  *source_context = NULL;
    SegmenterContext *output_context = NULL;
    
    av_register_all();
    
    if(avformat_open_input(&source_context, source_file, NULL, NULL)) {
        log_failure("can't open input file %s", source_file);
        exit(EXIT_FAILURE);
    }
    
    if (avformat_find_stream_info(source_context, NULL)) {
        log(!quiet, "Warning: can't load input file info");
    }
    
    if ((ret = segmenter_alloc_context(&output_context, file_base, base_media_file_name, target_duration))) {
        log_failure("allocate context, %s", segmenter_format_error(SGUNERROR(ret)));
        exit(EXIT_FAILURE);
    }
    
    SegmenterStreamType streams = audio_only ? SegmenterStreamAudio : 
                                    (video_only ? SegmenterStreamVideo : SegmenterStreamVideo | SegmenterStreamAudio);
    
    if ((ret = segmenter_init(output_context, source_context, streams))) {
        log_failure("initialize context, %s", segmenter_format_error(SGUNERROR(ret)));
        exit(EXIT_FAILURE);
    }
    
    if((ret =segmenter_open_output(output_context))){
        log_failure("open output, %s", segmenter_format_error(SGUNERROR(ret)));
        exit(EXIT_FAILURE);
    }
    
    AVPacket pkt;
    
    while (av_read_frame(source_context, &pkt) >= 0) {
        if ((ret = segmenter_write_pkt(output_context, source_context, &pkt))) {
            log_failure("write packet, %s", segmenter_format_error(SGUNERROR(ret)));
            exit(EXIT_FAILURE);
        }
    }
    
    segmenter_close(output_context);
    
    if ((ret = segmenter_write_index(output_context, base_url, index_file))) {
        log_failure("write index, %s", segmenter_format_error(SGUNERROR(ret)));
        exit(EXIT_FAILURE);
    }
    
    segmenter_free_context(output_context);
    
    avformat_close_input(&source_context);
    
    
    return 0;
}

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