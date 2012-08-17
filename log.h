// log.h
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
