#!/usr/bin/env bash

set -euo pipefail

fixture_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ffmpeg_bin="${FFMPEG_BIN:-ffmpeg}"

if ! command -v "$ffmpeg_bin" >/dev/null 2>&1; then
    echo "FFmpeg is required to regenerate the matching fixtures." >&2
    exit 1
fi

temporary_dir="$(mktemp -d)"
trap 'rm -r "$temporary_dir"' EXIT

common_output=(
    -an
    -c:v libx264
    -preset veryslow
    -crf 35
    -pix_fmt yuv420p
    -g 10
    -movflags +faststart
)

run_ffmpeg() {
    "$ffmpeg_bin" -hide_banner -loglevel error -y "$@"
}

# A and B are deliberately asymmetric, temporally stable portrait sources.
# Stability keeps the +/-2% substitution fixture focused on low-information
# replacement rather than measuring motion between neighboring timestamps.
# Their matching pillar bars therefore cannot be used as duplicate evidence.
run_ffmpeg -f lavfi -i "testsrc2=size=40x160:rate=10:duration=0.1" \
    -vf "tpad=stop_mode=clone:stop_duration=9.9" \
    "${common_output[@]}" "$fixture_dir/a-original.mp4"
run_ffmpeg -f lavfi -i "testsrc=size=40x160:rate=10:duration=0.1" \
    -vf "tpad=stop_mode=clone:stop_duration=9.9" \
    "${common_output[@]}" "$fixture_dir/b-original.mp4"

run_ffmpeg -i "$fixture_dir/a-original.mp4" -vf "transpose=clock" \
    "${common_output[@]}" "$fixture_dir/a-rot90.mp4"
run_ffmpeg -i "$fixture_dir/a-original.mp4" -vf "hflip,vflip" \
    "${common_output[@]}" "$fixture_dir/a-rot180.mp4"
run_ffmpeg -i "$fixture_dir/a-original.mp4" -vf "transpose=cclock" \
    "${common_output[@]}" "$fixture_dir/a-rot270.mp4"

# Preserve the active picture exactly and add symmetric encoded bars around it.
run_ffmpeg -i "$fixture_dir/a-original.mp4" -vf "pad=40:200:0:20:color=black" \
    "${common_output[@]}" "$fixture_dir/a-letterbox.mp4"
run_ffmpeg -i "$fixture_dir/a-original.mp4" -vf "pad=160:160:60:0:color=black" \
    "${common_output[@]}" "$fixture_dir/a-pillarbox.mp4"
run_ffmpeg -i "$fixture_dir/b-original.mp4" -vf "pad=160:160:60:0:color=black" \
    "${common_output[@]}" "$fixture_dir/b-pillarbox.mp4"

# cutEnds samples 8% and 96%; the proposed one-shot substitutes are 10% and 94%.
run_ffmpeg -i "$fixture_dir/a-original.mp4" \
    -vf "drawbox=x=0:y=0:w=iw:h=ih:color=black:t=fill:enable='between(t,0.65,0.95)+between(t,9.45,9.75)'" \
    "${common_output[@]}" "$fixture_dir/a-monochrome-ends.mp4"

# The pixels are rotated clockwise, while the Display Matrix asks players to
# rotate them counter-clockwise for the same intended presentation as A.
run_ffmpeg -display_rotation:v:0 90 -i "$fixture_dir/a-rot90.mp4" \
    -map 0:v:0 -c copy -movflags +faststart "$fixture_dir/a-display-matrix.mp4"

total_bytes=0
for fixture in "$fixture_dir"/*.mp4; do
    fixture_bytes="$(stat -f %z "$fixture" 2>/dev/null || stat -c %s "$fixture")"
    total_bytes=$((total_bytes + fixture_bytes))
done

echo "Generated 10 matching fixtures (${total_bytes} bytes total)."
if ((total_bytes > 2000000)); then
    echo "Fixture budget exceeded: expected at most 2000000 bytes." >&2
    exit 1
fi
