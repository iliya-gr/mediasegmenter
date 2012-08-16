//
//  log.h
//  mediasegmenter
//
//  Created by Iliya Grushveskiy on 16.08.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//
#include <libavutil/avutil.h>

#ifndef __SG_LOG__
#define __SG_LOG__

#define SG_LOG_QUITE    AV_LOG_QUITE
#define SG_LOG_FATAL    AV_LOG_FATAL
#define SG_LOG_ERROR    AV_LOG_ERROR
#define SG_LOG_WARNING  AV_LOG_WARNING
#define SG_LOG_INFO     AV_LOG_INFO
#define SG_LOG_VERBOSE  AV_LOG_VERBOSE
#define SG_LOG_DEBUG    AV_LOG_DEBUG

void sg_log(int level, const char* fmt, ...);
void sg_vlog(int level, const char* fmt, va_list vl);
void sg_log_init(char*);
void sg_log_set_level(int);


#endif
