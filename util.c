// util.h
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

#include <stdlib.h>
#include <string.h>
#include "util.h"

char *sg_strerror(int error) {
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