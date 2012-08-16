//
//  log.c
//  mediasegmenter
//
//  Created by Iliya Grushveskiy on 16.08.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//
#include "log.h"
#include <stdio.h>

static char* sg_log_app   = NULL;
static int   sg_log_level = SG_LOG_INFO;

void sg_vlog(int level, const char* fmt, va_list vl) {
    if (level <= sg_log_level) {
        
        if(sg_log_app) {
            fprintf(stderr, "%s: ", sg_log_app);
        }
        
        vfprintf(stderr, fmt, vl);
        fprintf(stderr, "\n");
    }
}

void sg_log(int level, const char* fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    sg_vlog(level, fmt, vl);
    va_end(vl);
}

void sg_log_init(char* app) {
    sg_log_app   = app;
    av_log_set_level(sg_log_level);
}

void sg_log_set_level(int level) {
    sg_log_level = level;
    av_log_set_level(level);
}