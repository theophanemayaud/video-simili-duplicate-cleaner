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

// FFmpeg 7 exposes SWS_BICUBIC as a macro while the macOS headers expose it
// as an enum. Keep one namespaced constant for callers on both platforms.
constexpr int swsBicubic = SWS_BICUBIC;
} // namespace ffmpeg

#endif // FFMPEG_H
