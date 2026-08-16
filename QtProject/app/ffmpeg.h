#ifndef FFMPEG_H
#define FFMPEG_H

#include <stdint.h>

#define __STDC_CONSTANT_MACROS

namespace ffmpeg
{
extern "C" {

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/display.h"
#include "libswscale/swscale.h"
}
} // namespace ffmpeg

#endif // FFMPEG_H
