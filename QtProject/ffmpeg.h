#ifndef FFMPEG_H
#define FFMPEG_H

#include <stdint.h>

#define __STDC_CONSTANT_MACROS

namespace ffmpeg {
extern "C" {

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

//#include "libavformat/avformat.h"
//#include "libavutil/dict.h"

//#include "libavcodec/avcodec.h"
//#include "libavformat/avformat.h"
//#include <libavutil/dict.h>
//#include "libavformat/riff.h"
//#include "libavformat/metadata.h"
//#include "libavformat/utils.h"
//#include "libavcodec/opt.h"
//#include "libavutil/rational.h"
//#include "options.h"
//#include "libavutil/avstring.h"
//#include "libavutil/internal.h"
//#include "libswscale/swscale.h"
}
}

#endif // FFMPEG_H
