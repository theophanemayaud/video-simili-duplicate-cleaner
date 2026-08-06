# Video matching improvements

Status: draft feature specification and implementation plan

Related reports:

- [Discussion #178: rotated videos are missed](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/discussions/178)
- [Issue #138: embedded black bars create false positives](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/138)
- [Issue #91: all-black captures are repeatedly rejected](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/91)
- [Issue #17: arbitrary cropped-video matching](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/17)

## Summary

We should improve recall in three narrowly scoped ways:

1. Replace an uninformative near-black sampled frame with one nearby sample.
2. Remove confidently detected, symmetric encoded black bars from the image used for matching.
3. Try 90-degree and 270-degree fingerprints when metadata and duration strongly indicate that one video is a quarter-turn copy of the other.

These transformations should happen while building matching fingerprints. Pair comparison should consume the normalized fingerprints and normally remain unaware of black-frame substitution or black-bar cropping. Rotation is the exception because it must be a deliberately gated fallback after the unchanged-orientation comparison fails.

The changes must not ship based only on more true-positive examples. The release gate is no additional matches in a representative negative-pair corpus, including different vertical videos with identical pillar bars. Trying several rotations and taking the highest score without extra gates is explicitly not acceptable: every unrelated pair would get more opportunities to cross the threshold.

## Goals

- Recover duplicate pairs that are currently missed because one copy is physically rotated by a quarter turn.
- Prevent a sampled black/title frame from making an otherwise usable video unmatchable or materially weakening its fingerprint.
- Make letterboxed and pillarboxed copies comparable to copies without encoded bars.
- Reduce the black-bar false positives described in issue #138.
- Preserve existing matching behavior for ordinary videos as closely as possible.
- Keep the fast path cheap for videos without black frames, black bars, or an orientation mismatch.
- Make the behavior deterministic and testable in no-cache, warm-cache, and cache-only scans.

## Non-goals

- Arbitrary crop, zoom, or pan matching. That remains the larger scope of issue #17.
- Horizontal or vertical mirroring.
- Arbitrary rotation angles. The first version supports only 90-degree and 270-degree relative rotations, not 180 degrees.
- Matching partial clips or videos with substantially different durations.
- Treating a dark scene as corrupt or moving it to the error folder.
- Adding user-facing settings for the first version. The automatic rules should be conservative enough to be on by default.
- Changing comparison thresholds globally to compensate for the new behavior.

## Current behavior and findings

### Frame extraction

`Video::takeScreenCaptures()` samples fixed positions from `Thumbnail::percentages()`, currently between 1 and 12 frames depending on the thumbnail mode. A decode failure already triggers a different recovery path: `ofDuration` is reduced by six percentage points and the whole capture sequence is retried against the shortened usable duration. There is no retry based on the visual contents of a successfully decoded frame.

The capture cache stores JPEG frames in columns named for the nominal sample positions (`at8` through `at96`). It does not store pHashes or SSIM matrices. This lets bar normalization and rotation fingerprints be recomputed from existing cached captures without a schema migration.

### “All black” detection

The existing `_almostBlackBitmap` check is not actually a black-pixel test. `Video::computePhash()` resizes the complete assembled thumbnail to 32x32 and sums each pixel's distance from the first pixel. It returns the sentinel hash `0` when the complete thumbnail is nearly monochrome, regardless of whether that color is black, gray, or white.

Consequences:

- In `thumb1`, one near-monochrome sampled frame rejects the video.
- In `cutEnds`, one unusable end is tolerated; the video is rejected only when both hashes are zero.
- In the other multi-frame modes, one black frame usually does not reject the video because the check is applied to the complete collage. It still contributes an uninformative tile and can lower similarity when the other copy samples useful content there.
- `VideoPairMatcher` treats two zero hashes as non-matching, so two black captures are intentionally not considered evidence that two videos are duplicates.

Content quality should therefore be classified per sampled frame before collage assembly, while final rejection should remain based on whether the requested thumbnail mode has any useful matching content.

### Rotation

Rotation metadata is already handled. `Video::getMetadata()` reads the stream's `rotate` tag, swaps presentation width/height for quarter turns, and `getQImageFromFrame()` transforms decoded frames. Copies whose difference is only correct rotation metadata should already arrive in the same presentation orientation.

The missing case is a file whose pixels were physically re-encoded in a different orientation, as reported in Discussion #178. Neither current metric is rotation-invariant:

- pHash compares the positions of low-frequency DCT coefficients. Rotating the image moves and changes those coefficients.
- SSIM compares corresponding spatial blocks. A rotated block is compared with a different part of the other image.

For multi-frame modes, rotating the completed collage as one image is also incorrect because it moves the sampled-frame tiles. Each tile must be rotated in place so sample time `n` remains aligned with sample time `n`.

### Encoded black bars

There is currently no black-bar detection. Bars can cause both error types:

- Copies with and without encoded bars can fall below the threshold because their active pictures occupy different areas.
- Different videos with the same large bars can look too similar because identical black pixels dominate the reduced 32x32 pHash and 16x16 SSIM inputs. This is the behavior reported in issue #138.

A single corner-pixel check would be fast but brittle: compression noise can make a bar pixel nonzero, and real content can have black corners. Detection should instead run on a small grayscale proxy and have a cheap edge-band early return before scanning candidate bands in detail.

### False-positive risk from multiple transforms

Taking the maximum score over 0, 90, 180, and 270 degrees changes the score distribution even for unrelated videos. At a fixed threshold, that necessarily increases the opportunity for accidental matches. Black-bar cropping can have a similar effect if it removes real picture content.

The feature therefore needs:

- conservative transformation eligibility rules;
- a second visual check for rotated fallback matches;
- no 180-degree or mirrored fallback in the first version;
- positive and negative corpus measurements before thresholds are finalized.

## Feature specification

### 1. Low-information black-frame substitution

For every freshly decoded or cached sampled frame:

1. Build a small grayscale proxy, no larger than 64x64.
2. Classify the frame as low-information black only when both its mean luminance and its near-black pixel ratio cross conservative limits. An initial candidate is at least 98% of pixels at luminance 16 or below, with mean luminance at or below 12; finalize these values from fixtures.
3. If the frame is usable, continue unchanged.
4. If it is low-information black and disk decoding is allowed, make exactly one substitute attempt:
   - for a nominal position below 50%, add two percentage points;
   - for a nominal position at or above 50%, subtract two percentage points.
5. Use the substitute only if it decodes successfully and is not low-information black. Otherwise keep the original result and mark that tile unusable for final validity checks.

Why one fixed attempt: it is deterministic, gives two duplicate copies the same requested substitute position, and places a hard upper bound on extra decoding. A later change can add more attempts only if the corpus demonstrates that one is insufficient without a meaningful performance cost.

Cache behavior:

- Store a successful substitute in the original nominal cache column. `at48`, for example, means “the resolved capture for the 48% slot,” not necessarily a frame decoded at exactly 48%.
- A normal warm-cache scan may detect an old cached black frame, decode the substitute, and overwrite that nominal slot. This lazily repairs old caches.
- `CACHE_ONLY` must never read the video. If its cached nominal frame is black, it cannot repair that slot during that run.
- Black-frame classification must tolerate JPEG noise because cached captures are lossy.

Final validity remains mode-aware:

- `thumb1` fails if its original and substitute are both unusable.
- `cutEnds` remains usable when either end is informative.
- A multi-frame collage fails only when every sampled frame is unusable.

The error can remain `all screen captures black` for compatibility with the existing UI, although an internal name such as `no informative captures` is more accurate.

### 2. Conservative symmetric black-bar normalization

Black-bar removal affects only the derived matching image. The GUI review thumbnail should retain the encoded bars so the user still sees the file as it exists. A successful black-frame substitute does appear in the GUI collage because it replaces the sampled frame itself.

Detection operates on each informative tile, after capture resolution and before pHash/SSIM generation:

1. Downscale the tile to a grayscale proxy with a maximum dimension of 256 pixels.
2. Fast path: inspect thin bands on all four edges. Return “no bars” unless one opposing edge pair is overwhelmingly near-black.
3. For a candidate axis, walk inward by row or column and measure the ratio of near-black pixels. Compression-tolerant near-black thresholds are required; exact RGB zero is not.
4. Accept only a pair of opposing rectangular bands which:
   - covers almost the complete row or column at every step;
   - has approximately symmetric thickness on the two sides;
   - leaves a meaningful central picture;
   - occurs on only one axis; and
   - has consistent boundaries across all informative sampled frames, within a small tolerance.
5. If any requirement fails, do not crop.
6. If accepted, crop the same consensus insets from every tile and resize the remaining active picture back into that tile's original collage cell.

Using one consensus crop for the entire sampled set matters. Independently cropping dark scene content at different depths would create unstable fingerprints. For `thumb1`, where cross-frame consensus is impossible, use stricter coverage and symmetry thresholds.

The implementation should record bar thickness as normalized fractions of frame width/height so re-encoded copies at different resolutions can agree. Initial thresholds should be conservative and finalized by the corpus, with these constraints:

- tolerate normal codec noise in black bars;
- reject one-sided borders and black corner overlays;
- support both letterbox and pillarbox layouts, including narrow vertical content inside a 16:9 canvas;
- reject ambiguous candidates with both horizontal and vertical crops;
- never crop based only on two or four individual corner pixels.

This is deliberately narrower than general crop matching in issue #17.

### 3. Gated quarter-turn matching

Build three fingerprints for each usable thumbnail segment during extraction:

- unchanged orientation;
- each tile rotated 90 degrees clockwise in its existing collage cell;
- each tile rotated 90 degrees counter-clockwise in its existing collage cell.

The fingerprints use the black-bar-normalized matching image. Tile order is unchanged. No full-size rotated images need to be retained after their reduced fingerprints are produced.

Pair matching proceeds as follows:

1. Run the existing unchanged-orientation comparison first. If it matches, return it without rotation work.
2. Consider the quarter-turn fallback only when all of these are true:
   - one video's presentation orientation is landscape and the other's is portrait;
   - neither display aspect ratio is near square;
   - the display aspect ratios are reciprocal within a small tolerance, allowing resolution changes;
   - durations differ by no more than the existing one-second “same duration” tolerance; and
   - both segments have usable fingerprints.
3. Compare the unchanged fingerprint on one side with the clockwise and counter-clockwise fingerprints on the other. This covers either relative quarter turn without a 3x3 cross-product.
4. A rotated fallback must pass both pHash and SSIM safeguards, even when the selected UI mode is pHash. This secondary check is required because the fallback gives a pair more matching opportunities.
5. Apply conservative minimum floors to the rotated pHash and raw SSIM scores in addition to the user-selected threshold. Initial calibration candidates are 57/64 pHash bits and 0.90 raw SSIM, but these are not release decisions until measured against the fixture corpus.
6. Record the winning relative rotation in `VideoPairMatchResult` for tests and verbose diagnostics. The first version does not need new comparison-window controls or automatic display rotation.

The normal comparison path and its thresholds remain unchanged. The fallback does not include 180-degree rotation because same-orientation metadata provides no equally cheap eligibility gate. It can be evaluated later as a separate feature with its own false-positive evidence.

### Fingerprint representation

The current `grayThumb` stores 16x16 `CV_32F` matrices. Keeping three more float matrices per segment would be needlessly expensive for very large libraries. Store compact 8-bit grayscale SSIM inputs and convert to float only for the small number of pairs that reach SSIM:

```cpp
enum class FingerprintRotation { none, clockwise90, counterClockwise90 };

struct VisualFingerprint {
    uint64_t phash = 0;
    std::array<uint8_t, 16 * 16> ssimPixels{};
    bool usable = false;
};
```

`Video` can hold this for up to two `cutEnds` segments and three orientations. Three 256-byte SSIM inputs use less pixel storage per segment than one current 16x16 float matrix, before small struct overhead. This keeps rotation support from multiplying per-video memory.

The zero pHash should no longer be the only validity signal. An explicit `usable` flag avoids coupling black-frame policy to a hash value that can otherwise be valid data.

## Performance requirements

- The no-bars path performs only a downscale and small edge-band checks per sampled frame.
- Detailed band walking runs only after the edge fast path finds a candidate.
- A low-information frame triggers at most one extra decode.
- Base pair comparison performs exactly the current unchanged-orientation work.
- Rotation fallback is evaluated only for opposite-orientation, reciprocal-aspect, same-duration pairs after the base comparison fails.
- SSIM remains behind pHash, and rotated SSIM runs only for a rotated pHash candidate.
- Peak extraction memory must not retain full-size copies for every rotation. Build one reduced orientation at a time and keep only compact fingerprints.

Release target: less than 5% p95 extraction-time regression on a representative no-bars corpus, no material pair-scan regression for a same-orientation library, and no increase in peak memory attributable to stored rotation fingerprints. These targets may be adjusted only with measured results recorded in the implementing PR.

## False-positive safety and acceptance criteria

Create a labeled corpus before finalizing constants. It must contain at least:

Positive pairs:

- original and recompressed copy;
- original and physically rotated 90-degree/270-degree recompress;
- original and copy with symmetric letterbox bars;
- original and copy with symmetric pillarbox bars;
- copies with a black frame at a nominal sample but matching content at the substitute position;
- the above in both no-cache and warm-cache scans.

Negative pairs:

- different videos with the same duration and dimensions;
- different portrait videos embedded in identical 16:9 pillar bars, reproducing issue #138;
- different title cards on black backgrounds;
- naturally dark scenes with nonuniform detail;
- frames with one-sided black borders;
- frames with black corners but no continuous bars;
- unrelated landscape/portrait pairs whose aspect ratios are reciprocal;
- near-square videos, which must not enter rotation fallback.

Acceptance criteria:

- All matches found by the current implementation in the corpus remain matches.
- The targeted rotated, bar-normalized, and substituted-frame positive pairs match at the agreed default settings.
- No pair labeled negative becomes a match at the default threshold or at the lower threshold selected for the issue #138 regression set.
- Different videos sharing bars score based on their active pictures and no longer match merely because of the bars.
- Fresh, warm-cache, and cache-only results are documented; a cache-only run using a previously repaired cache must match the warm-cache result.
- The one-retry bound and rotation eligibility gates are covered by tests.
- Performance and peak-memory targets above are measured, not inferred from operation counts.

“No new false positives” is evaluated against this corpus and existing end-to-end fixtures. It cannot be proven for every possible video, so the implementation must fail closed whenever detection is ambiguous.

## Implementation plan

### Phase 1: fixtures and baseline

1. Add small deterministic video fixtures, generated from distinct source patterns and encoded with the project's supported FFmpeg workflow.
2. Produce rotated, bar-encoded, and black-sample variants from those sources. Keep commands or a fixture-generation script beside the tests so the intent is reproducible.
3. Record current pair scores and match decisions for every labeled positive and negative pair in pHash and SSIM modes.
4. Record no-cache, warm-cache, and cache-only behavior for the new matching paths.
5. Add a small extraction benchmark for ordinary videos and videos that activate each new path.

This phase prevents tuning only against the motivating positives.

### Phase 2: extract testable visual helpers

1. Introduce pure helpers for:
   - low-information black classification;
   - symmetric bar detection and consensus;
   - tile-wise crop/resize and quarter-turn transforms; and
   - compact pHash/SSIM fingerprint construction.
2. Keep file I/O, FFmpeg seeking, and cache writes in `Video`; keep image classification deterministic and independently testable.
3. Replace implicit `hash == 0` validity with an explicit usable flag while preserving current results before enabling new behavior.
4. Update SSIM input handling to consume compact 8-bit data and convert to float at comparison time.

Likely files: `QtProject/app/video.h`, `QtProject/app/video.cpp`, a small `visualfingerprint.{h,cpp}` helper, `QtProject/app/comparison/internal/ssim.*`, and focused tests.

### Phase 3: black-frame substitution

1. Classify each capture before drawing it into the collage.
2. Add the single directional substitute attempt.
3. Overwrite the nominal cache slot only when the substitute succeeds.
4. Make final rejection depend on useful captures for the selected thumbnail mode.
5. Add tests for forward/backward direction, retry bounds, failed substitutes, warm-cache lazy repair, and cache-only no-I/O behavior.

### Phase 4: black-bar normalization

1. Add the reduced edge-band fast path and detailed row/column scan.
2. Add cross-tile consensus and ambiguity rejection.
3. Derive a cropped/resized matching collage while retaining the uncropped review collage.
4. Run all issue #138 positive and negative fixtures before changing any threshold.
5. Measure the ordinary-video extraction overhead and tighten the early-return path if needed.

### Phase 5: quarter-turn fingerprints and fallback

1. Generate compact unchanged/clockwise/counter-clockwise fingerprints tile by tile.
2. Extend `VideoPairMatchResult` with the selected relative rotation.
3. Keep the current base comparison first and unchanged.
4. Add orientation, reciprocal-aspect, near-square, and duration gates.
5. Add the pHash-plus-SSIM rotated fallback and calibrate its minimum floors from the full corpus.
6. Verify background match discovery remains deterministic and reports the same base matches.

### Phase 6: end-to-end verification

Run, at minimum:

- the new visual-helper and pair-matcher tests;
- `test_comparison`, including background discovery;
- `test_video_simplified` on the macOS development setup;
- explicit `test_video` whole-app cases relevant to thumbnail/cache behavior; and
- the self-contained baseline from `AGENTS.md`.

Do not run the full `test_video` executable by default because it includes the separately mounted `test_100GB*` cases. Reference thumbnails and hashes will likely change when matching normalization changes; update them only after the labeled corpus passes and record why each expected value changed.

## Diagnostics

In verbose mode, report aggregate counts rather than noisy per-pair output:

- black frames detected;
- successful and failed substitutions;
- videos with accepted letterbox/pillarbox normalization;
- ambiguous bar candidates rejected;
- pairs eligible for rotation fallback;
- rotated pHash candidates, rotated SSIM checks, and accepted rotated matches.

These counters make threshold and performance regressions diagnosable without adding telemetry or changing normal UI output.

## Decisions to preserve during implementation

- Normalize matching inputs; do not lower global thresholds to recover these cases.
- Preserve original bars in the review thumbnail.
- Keep one bounded black-frame substitute attempt.
- Crop only symmetric, consensus bars and fail closed on ambiguity.
- Handle rotation metadata during decode as today; the fallback is only for physically rotated pixels.
- Start with 90-degree and 270-degree rotations, not 180-degree or mirrors.
- Require both pHash and SSIM evidence for a rotated fallback.
- Treat zero additional corpus false positives as a release gate, not a best-effort aspiration.
