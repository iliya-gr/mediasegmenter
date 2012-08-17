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


#ifndef __SG_UTILS__
#define __SG_UTILS__

#define SGERROR(e)         (-(e))
#define SGUNERROR(e)       (-(e))

#define SGERROR_MEM_ALLOC          0x01
#define SGERROR_NO_STREAM          0x02
#define SGERROR_UNSUPPORTED_FORMAT 0x03
#define SGERROR_FILE_WRITE         0x04

char *sg_strerror(int error);

#endif
