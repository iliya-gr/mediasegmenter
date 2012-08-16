//
//  util.h
//  mediasegmenter
//
//  Created by Iliya Grushveskiy on 16.08.12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef __SG_UTIL__
#define __SG_UTIL__

#define SGERROR(e)         (-(e))
#define SGUNERROR(e)       (-(e))

#define SGERROR_MEM_ALLOC          0x01
#define SGERROR_NO_STREAM          0x02
#define SGERROR_UNSUPPORTED_FORMAT 0x03
#define SGERROR_FILE_WRITE         0x04

#endif
