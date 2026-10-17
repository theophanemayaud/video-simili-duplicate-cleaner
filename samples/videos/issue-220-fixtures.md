# Embedded cover art regression fixtures

`issue-220-audio-cover.ogg` is the unchanged `trigger.ogg` supplied in
[issue #220](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/220),
from `https://github.com/user-attachments/files/33179620/trigger.ogg.zip`.
It contains ten seconds of Vorbis audio and a 64 by 64 attached JPEG cover.
FFmpeg exposes the cover as a video stream; seeking it aborts in `oggdec.c:953`.
Keep the original container to retain the exact demuxer regression.

`issue-220-video-cover.mp4` contains one second from the tracked Nice H.264
video plus its tracked JPEG collage as cover art. It checks that ordinary video
remains processable when an attached picture is also present. Created with:

```sh
ffmpeg -i samples/videos/Nice_383p_500kbps.mp4 \
  -i samples/videos/Nice_383p_500kbps.mp4.jpg -t 1 \
  -map 0:v -map 1:v -c copy -disposition:v:1 attached_pic \
  samples/videos/issue-220-video-cover.mp4
```
