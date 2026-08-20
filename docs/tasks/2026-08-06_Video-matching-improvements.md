# Video matching improvements

Status: implemented and validated in draft PR #198.

Related reports:

- [Discussion #178: rotated videos are missed](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/discussions/178)
- [Issue #138: embedded black bars create false positives](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/138)
- [Issue #91: all-black captures are repeatedly rejected](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/91)
- [Issue #17: arbitrary cropped-video matching](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/17)

## Outcome

Duplicate recall improves in three narrow, independent ways:

1. A low-information sampled frame gets one nearby replacement attempt.
2. Clearly encoded symmetric black bars are removed from matching input only.
3. Physical 90, 180, and 270-degree rotations can be compared when the user enables the opt-in setting.

The ordinary matching path remains the default. The feature does not lower global thresholds or relax the normal pHash/SSIM policy. Every new transform must remain safe against the labeled negative corpus: no newly matched cross-content pair is accepted.

## Runtime design

### Capture resolution and low-information frames

`Video::resolveCaptureSlot()` owns the per-sample choice:

- Read a cached JPEG when available; otherwise decode the nominal percentage when disk decoding is allowed.
- Analyze that selected image once through the shared grayscale analysis image.
- If it is low information, make exactly one deterministic substitution: `+2%` before 50%, otherwise `-2%` toward the video centre.
- Use the replacement only if it decoded and is informative. A successful replacement is stored in the nominal cache slot.

The existing decode-failure recovery remains outside this resolver: it shortens the usable duration by 6% and restarts the capture sequence. It handles missing frames, whereas the single substitution handles successfully decoded but unhelpful content. They deliberately are not a generic retry system.

The analysis image preserves the frame aspect ratio and caps its longest edge at 64 pixels. A frame is informative when either its grayscale standard deviation is at least `2.0` or its range is wider than `12`. This protects dark frames with visible detail while rejecting uniform black, white, grey, and colour frames.

### Conservative black-bar normalization

The same analysis image performs a cheap edge check and, only on a candidate, a linear inward scan. A bar crop is accepted only when each informative extracted frame reports:

- one axis only;
- opposing bands with 98% near-black (`<= 20`) coverage;
- symmetric widths within one analysis pixel;
- each band between 4% and 45% of that edge; and
- the same axis and normalized boundaries across all informative frames.

The median agreed boundaries crop every matching tile consistently. Any ambiguity, one-sided band, mixed axis, or frame disagreement leaves the matching image unchanged. The GUI review collage always retains the original bars.

The maximum 45% inset on each opposing side already guarantees at least 10% active picture, so there is no second redundant active-area threshold.

### Fingerprints and rotated matching

`VisualFingerprint` is the single matching representation: a 64-bit pHash, a compact 16x16 SSIM grid, and an explicit `usable` flag. It replaces the former hash-zero and parallel-image validity conventions.

The unchanged matching image follows the direct historical collage path whenever no crop is accepted. Cropped and/or rotated images are built per tile, preserving sample order. A tile is reduced to a 64-pixel maximum edge before rotation because the fingerprint inputs are only 32x32 (pHash) and 16x16 (SSIM).

Presentation metadata is normalized during every fresh decode. FFmpeg Display Matrix takes precedence over the legacy `rotate` tag, and only one transform is applied. This is separate from physical rotation matching.

`Detect rotated video copies` is the sole new user setting. It is persisted and defaults to off. When disabled, only the unchanged fingerprint is made and compared. When enabled, extraction additionally makes 90, 180, and 270-degree fingerprints; matching first tries unchanged and then the three explicit relative rotations. There is no 4x4 orientation matrix.

Rotated fallback requires all of the following before it can match:

- raw pHash similarity of at least `57/64`;
- raw SSIM of at least `0.90`, even in pHash mode; and
- the applicable user pHash/SSIM threshold after the usual duration adjustment.

Those fixed floors account for the extra false-positive opportunity created by trying several transforms. Ordinary SSIM comparisons retain their pHash prefilter of `44/64`; pHash bit differences use `std::popcount`.

### Cache policy

The cache continues to store only JPEG captures for nominal sample positions. Restored JPEGs pass through the current analysis and fingerprint pipeline, with no schema version, migration, invalidation, conversion, or lazy repair machinery.

Freshly decoded frames receive the current presentation-metadata normalization. An already cached metadata-rotated capture can retain the old orientation/dimensions until a no-cache scan or explicit cache removal. That small limitation is accepted in exchange for simple, compatible cache behavior. `CACHE_ONLY` never reads the video, so it cannot replace an uninformative cached frame during that run.

## Non-goals

- Arbitrary crop, zoom, pan, partial-clip, mirror, or non-right-angle rotation matching.
- Treating a dark but detailed frame as corrupt.
- Per-angle controls or settings for analysis/bar/retry thresholds.
- Persisting frame analysis, substitute timestamps, crop metadata, pHashes, SSIM grids, or chosen rotations.
- Cache migration, record invalidation, or recovery of old metadata/captures.
- A generalized FFmpeg retry framework, transform hierarchy, or second rotation matcher.
- Changing comparison UI state or global thresholds to compensate for the feature.

## Test data and validation

### Repository fixtures

The self-contained `samples/videos/matching-ground-truth.csv` manifest covers 12 videos: the two established `Nice` duplicates plus a ten-video A/B matrix. The tracked addition is about 108 KB, keeping all repository video fixtures within the few-megabyte budget.

The A/B matrix contains asymmetric base videos; physical 90/180/270 copies; letterbox and pillarbox copies; an A/B same-pillarbar negative; monochrome windows at 8% and 96% whose 10%/94% substitutes are informative; and a Display Matrix case. `test_repo_video_matching` covers pixel-level analysis guards, extraction, warm-cache/cache-only reuse, metadata normalization, rotation enabled/disabled, expected positive pairs, and the no-new-cross-group-pairs contract.

### Optional external corpus

The `~/Dev` corpus adds eleven labeled feature derivatives under `Videos/Matching feature fixtures`, yielding 229 manifest rows and a 22-video focused subset. It provides realistic formats, natural dark-detail/bar-consensus guards, labeled physical rotations, and a 218-video legacy no-new-cross-group baseline. It is exercised by `test_external_video_matching`, with extraction and whole-app coverage in `test_external_video_extraction_regression` and `test_external_whole_app_scan`; it is not required for CI.

The separately mounted 100GB corpus is measured by `test_external_large_video_corpus` only. It is intentionally not part of normal development runs.

### Recorded validation (2026-08-11)

- The tracked matrix passed fresh, warm-cache, and cache-only scans with rotation both off and on. Tiny no-cache extraction was 12 ms off and 15 ms on.
- The 22-video external feature subset passed every labeled pair. No-cache extraction was 1.096 s off and 1.122 s on after reducing tiles before rotation.
- The 218-video external baseline took 20.895 s versus 20.828 s before implementation (+0.3%) and added no cross-content pair. The expanded whole-app corpus found 229 files, 226 valid videos, and 89 matched videos.
- The mounted corpus found 12,506 files, 12,352 valid videos, and 6,552 matches with rotation off; rotation on found 6,556 matches. The opt-in setting measured +6.6% extraction and +12.4% total scan time. Warm-cache assessment completed in 44.978 s, under its 50-second budget.

## Implementation map and invariants

- `Video` resolves captures, preserves its review collage, and owns FFmpeg/cache I/O.
- `VisualFingerprintBuilder` contains deterministic image analysis, bar consensus, tile transformation, and compact fingerprint construction.
- `VideoPairMatcher` has one common scorer for unchanged and rotated candidates. Rotated candidates add safety floors rather than a second scoring implementation.
- The manifest stores only the pair-relevant orientation required by tests; generated fixture names and tags document physical transforms, avoiding duplicate inert metadata.

Keep these invariants during future work: reuse cached JPEGs; fail closed on ambiguous bars; keep one bounded monochrome substitution; retain the direct unchanged collage path; preserve separate decode-failure and content-quality recovery; keep physical rotations opt-in; and require labeled negative evidence before making matching more permissive.
