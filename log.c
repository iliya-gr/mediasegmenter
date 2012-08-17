// log.c
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