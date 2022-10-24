#ifndef FFMPEG_H
#define FFMPEG_H

#include <stdint.h>

#define __STDC_CONSTANT_MACROS

namespace ffmpeg {
extern "C" {

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}
}

#endif // FFMPEG_H
