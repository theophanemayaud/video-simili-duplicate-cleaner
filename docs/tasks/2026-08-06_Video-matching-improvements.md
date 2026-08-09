# Video matching improvements

Status: draft feature specification and implementation plan

Related reports:

- [Discussion #178: rotated videos are missed](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/discussions/178)
- [Issue #138: embedded black bars create false positives](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/138)
- [Issue #91: all-black captures are repeatedly rejected](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/91)
- [Issue #17: arbitrary cropped-video matching](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/17)

## Summary

We should improve recall in three narrowly scoped ways:

1. Replace an uninformative near-monochrome sampled frame with one nearby sample.
2. Remove confidently detected, symmetric encoded black bars from the image used for matching.
3. Optionally compare fingerprints at 90, 180, and 270 degrees to detect physically rotated copies.

These transformations should happen while building matching fingerprints. Pair comparison should consume the normalized fingerprints and normally remain unaware of monochrome-frame substitution or black-bar cropping. Rotated matching should be controlled by a persisted setting which is off by default, leaving the current rotation-specific fingerprint work and comparison behavior unchanged unless the user explicitly enables it.

The implementation should also simplify the existing matching path rather than leave old and new representations side by side. All three improvements should share one frame-analysis and fingerprint pipeline, with one explicit validity signal and one pair-scoring implementation.

The changes must not ship based only on more true-positive examples. The release gate is no additional matches in a representative negative-pair corpus, including different vertical videos with identical pillar bars. Trying several rotations gives unrelated pairs more opportunities to cross the threshold, so rotated fallback matches still require a second visual safeguard and explicit negative-corpus validation.

## Goals

- Recover duplicate pairs that are currently missed because one copy is physically rotated by 90, 180, or 270 degrees.
- Prevent a sampled black, white, gray, or otherwise near-monochrome frame from making an otherwise usable video unmatchable or materially weakening its fingerprint.
- Make letterboxed and pillarboxed copies comparable to copies without encoded bars.
- Reduce the black-bar false positives described in issue #138.
- Preserve existing matching behavior for ordinary videos as closely as possible.
- Keep the fast path cheap for videos without low-information frames or black bars, and preserve the current rotation cost when rotated matching is disabled.
- Make the behavior deterministic and testable in no-cache, warm-cache, and cache-only scans.
- Reduce matching-specific state and branching: one analysis pass per frame, one fingerprint representation, and one pair-comparison path.

## Non-goals

- Arbitrary crop, zoom, or pan matching. That remains the larger scope of issue #17.
- Horizontal or vertical mirroring.
- Arbitrary rotation angles outside 90-degree increments.
- Matching partial clips or videos with substantially different durations.
- Treating a dark scene as corrupt or moving it to the error folder.
- Adding settings for monochrome-frame substitution or black-bar normalization. Those rules should be conservative enough to be on by default; only rotated-copy detection is opt-in.
- Migrating, converting, or lazily repairing existing cache records for the new matching approach. A whole-cache invalidation and full rescan after upgrade is acceptable.
- Changing comparison thresholds globally to compensate for the new behavior.
- A general rewrite of FFmpeg decoding, the cache database, thumbnail modes, or the comparison UI.
- Adding user-facing controls for individual rotation angles, analysis thresholds, retries, or black-bar behavior.

## Complexity budget and simplification opportunities

These are implementation requirements, not optional cleanup to defer until after the feature.

### One transient frame analysis

Build one aspect-preserving grayscale proxy per resolved frame, initially capped at 64 pixels on its longest edge. Use that same proxy for both low-information classification and black-bar edge analysis. Retain only the resulting small statistics/bar candidate and discard the pixels immediately after analyzing the frame. Do not create separate 64x64 and 256-pixel analysis images.

A 64-pixel proxy already has more spatial detail than the final 32x32 pHash and 16x16 SSIM inputs. A bar too thin to survive that proxy is unlikely to materially affect either matcher. Increase this one shared cap only if fixtures demonstrate that its boundary resolution is insufficient.

Analyze a decoded cache JPEG at its native cached size before any scaling needed for the collage. This avoids upscaling a small cached image only to downscale it again for classification.

### One capture-slot resolver

Centralize the current cache-read/fresh-decode choice and the new substitute choice in one small slot-resolution function. It should return the selected image plus its transient analysis result. Within a slot attempt, choose the nominal or successful substitute before deciding which image belongs in the nominal cache column.

The two retry policies remain deliberately different:

- the existing shortened-duration recovery handles decode failure and may restart the capture sequence;
- the new content-quality retry handles one successfully decoded but low-information slot and tries one nearby timestamp.

Both call the same low-level capture primitive, but they should not be merged into a configurable retry framework or a general state machine. `CACHE_ONLY` behavior should be decided once in the slot resolver rather than repeated in each analysis branch.

### One retained collage and one reusable scratch image

Keep the uncropped collage already needed for the GUI. While filling it, retain only small per-tile analysis results, not a second collection of full-size frames. Once all tiles are known, calculate one bar consensus and build each required matching orientation into the same reusable scratch image. Fingerprint that orientation immediately, then reuse the scratch storage for the next orientation.

With rotated-copy detection disabled, the orientation list contains only 0 degrees. With it enabled, the same loop also visits 90, 180, and 270 degrees. Do not retain normalized or rotated full-size collages after their compact fingerprints are built.

### One fingerprint and validity model

`VisualFingerprint` should replace, rather than accompany, the parallel `Video::hash[]` and `Video::grayThumb[]` fields. Its explicit `usable` flag should also replace:

- `_almostBlackBitmap` inside `computePhash()`;
- the `hash == 0` validity checks in `Video::internalProcess()`; and
- the special “both hashes are zero” handling in `VideoPairMatcher`.

Low-information policy belongs to frame/segment analysis. Use one luminance-neutral, histogram-based predicate rather than retaining the current near-monochrome check beside a new black-only check. This preserves the current intent that uniform black, gray, white, or colored frames are not matching evidence. pHash construction should only hash an already usable matching image and should not also classify content. A numerical pHash value of zero can then be treated as data rather than an error sentinel. Centralize the `cutEnds` versus single-segment validity rule in the fingerprint-building boundary instead of repeating that thumbnail-mode branch across extraction and matching.

### One deliberately simple bar detector

Use a linear inward scan from opposing edges of the shared proxy. For the first version, accept a crop only when every informative sampled tile reports the same axis and its opposing boundaries agree within one proxy pixel. Use the median agreed boundaries as the single crop for the set. If one informative tile disagrees or reports no bars, do not crop.

Use one conservative coverage/symmetry threshold set for every thumbnail mode, strong enough to be safe for `thumb1`. Multi-frame modes gain confidence from unanimity but do not introduce voting, outlier rejection, or another threshold family. Do not add contours, morphology, scene classification, or a generic crop detector.

### One pair-scoring implementation

Factor the existing pHash prefilter, optional SSIM, and duration adjustment into one fingerprint-pair comparison function. The matcher runs it for the unchanged orientation first, then loops over the three relative rotations only when the setting is enabled. The rotated policy adds its fixed pHash and SSIM safety floors around that common score calculation; it is not a second matcher.

Keep only one user preference: `Detect rotated video copies`. Do not add a 4x4 orientation cross-product, aspect-ratio eligibility rules, per-angle switches, or user-adjustable rotated thresholds. The winning relative rotation may be returned transiently for tests and verbose diagnostics, but it is not persisted in SQLite or propagated into the comparison UI in the first version.

### Keep cache and tests data-oriented

The cache continues to store only the resolved JPEG for each nominal slot. Do not persist frame classifications, actual substitute timestamps, bar boundaries, pHashes, SSIM pixels, or rotations. Compatible captures flow through the same analysis as fresh captures; incompatible data is handled only by whole-cache invalidation.

Generate transformed variants from a small set of base fixtures and drive expectations from one labeled pair table. Avoid separate test harnesses for rotation, substitution, bars, and cache modes when the same end-to-end matrix can exercise their combinations.

## Current behavior and findings

### Frame extraction

`Video::takeScreenCaptures()` samples fixed positions from `Thumbnail::percentages()`, currently between 1 and 12 frames depending on the thumbnail mode. A decode failure already triggers a different recovery path: `ofDuration` is reduced by six percentage points and the whole capture sequence is retried against the shortened usable duration. There is no retry based on the visual contents of a successfully decoded frame.

The capture cache stores JPEG frames in columns named for the nominal sample positions (`at8` through `at96`). It does not store pHashes or SSIM matrices. Existing cached captures may be reused only if they are already valid inputs to the new pipeline without special conversion. If the persisted input contract changes, invalidate the old cache as a whole and rebuild it instead of adding a migration path.

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

Metadata and frame extraction dominate scan cost. Rotation fingerprints can be derived from the already extracted collage without another FFmpeg seek or metadata read. Historical pHash measurements in `BackgroundMatchDiscovery` estimate roughly one millisecond per 1,000 pairs, and pair discovery now runs in parallel in the background. The expected rotation cost is therefore the much smaller work of producing three extra reduced fingerprints per video and performing up to three extra pHash comparisons per otherwise unmatched pair. SSIM should still run only after a rotated pHash passes its prefilter.

### Encoded black bars

There is currently no black-bar detection. Bars can cause both error types:

- Copies with and without encoded bars can fall below the threshold because their active pictures occupy different areas.
- Different videos with the same large bars can look too similar because identical black pixels dominate the reduced 32x32 pHash and 16x16 SSIM inputs. This is the behavior reported in issue #138.

A single corner-pixel check would be fast but brittle: compression noise can make a bar pixel nonzero, and real content can have black corners. Detection should instead run on a small grayscale proxy and have a cheap edge-band early return before scanning candidate bands in detail.

### False-positive risk from multiple transforms

Taking the maximum score over 0, 90, 180, and 270 degrees changes the score distribution even for unrelated videos. At a fixed threshold, that necessarily increases the opportunity for accidental matches. Black-bar cropping can have a similar effect if it removes real picture content.

The feature therefore needs:

- a persisted rotated-copy setting which defaults to off;
- a second visual check for rotated fallback matches;
- all three nonzero right-angle rotations, including 180 degrees, when the setting is enabled;
- positive and negative corpus measurements before thresholds are finalized.

## Feature specification

### 1. Low-information monochrome-frame substitution

For every freshly decoded or cached sampled frame:

1. Build the one shared, aspect-preserving grayscale analysis proxy, with its longest edge no larger than 64 pixels. Use it for both low-information and black-bar candidate analysis before discarding its pixels.
2. Classify the frame as low information only when a dominant luminance band covers nearly the complete proxy. An initial candidate is at least 98% of pixels within a luminance span no wider than 12/255; finalize both constants from fixtures. This is intentionally color-neutral, retains the current near-monochrome intent, and tolerates codec noise without using the first pixel as its reference.
3. If the frame is usable, continue unchanged.
4. If it is low information and disk decoding is allowed, make exactly one substitute attempt:
   - for a nominal position below 50%, add two percentage points;
   - for a nominal position at or above 50%, subtract two percentage points.
5. Use the substitute only if it decodes successfully and is informative. Otherwise keep the original result and mark that tile unusable for final validity checks.

Why one fixed attempt: it is deterministic, gives two duplicate copies the same requested substitute position, and places a hard upper bound on extra decoding. A later change can add more attempts only if the corpus demonstrates that one is insufficient without a meaningful performance cost.

Cache behavior:

- Store a successful substitute in the original nominal cache column. `at48`, for example, means “the resolved capture for the 48% slot,” not necessarily a frame decoded at exactly 48%.
- Apply the same current-run substitution rule to any compatible cached capture; do not add special detection or recovery logic for records written by an older version.
- `CACHE_ONLY` must never read the video. If its cached nominal frame is unusable, it cannot substitute that slot during that run.
- Low-information classification must tolerate JPEG noise because cached captures are lossy.

#### Cache transition

Do not implement a record-by-record migration strategy for this work. Before implementation, decide whether the old cached JPEG captures remain valid raw inputs:

- If they do, read them normally and derive the new fingerprints without rewriting records merely because they are old.
- If they do not, bump one cache-generation value, invalidate the old cache in full, and let the next normal scan rebuild it from the videos.

Do not add legacy fingerprint formats, conversion code, lazy repair passes, or fallback matching behavior. After invalidation, normal cache mode may take as long as a first scan; that is an accepted upgrade cost. Cache-only mode must fail cleanly for invalidated/missing entries rather than crashing or mixing generations.

Final validity remains mode-aware:

- `thumb1` fails if its original and substitute are both unusable.
- `cutEnds` remains usable when either end is informative.
- A multi-frame collage fails only when every sampled frame is unusable.

The error can remain `all screen captures black` for compatibility with the existing UI, although an internal name such as `no informative captures` is more accurate.

### 2. Conservative symmetric black-bar normalization

Black-bar removal affects only the derived matching image. The GUI review thumbnail should retain the encoded bars so the user still sees the file as it exists. A successful monochrome-frame substitute does appear in the GUI collage because it replaces the sampled frame itself.

Detection operates on each informative tile, after capture resolution and before pHash/SSIM generation:

1. Reuse the grayscale analysis proxy already built for low-information classification. Do not perform another tile downscale.
2. Fast path: inspect thin bands on all four edges. Return “no bars” unless one opposing edge pair is overwhelmingly near-black.
3. For a candidate axis, walk inward by row or column and measure the ratio of near-black pixels. Compression-tolerant near-black thresholds are required; exact RGB zero is not.
4. Accept only a pair of opposing rectangular bands which:
   - covers almost the complete row or column at every step;
   - has approximately symmetric thickness on the two sides;
   - leaves a meaningful central picture;
   - occurs on only one axis; and
   - occurs on every informative sampled frame, on the same axis, with boundaries agreeing within one proxy pixel.
5. If any requirement fails, do not crop.
6. If accepted, take the median agreed boundaries, crop those same normalized insets from every tile, and resize the remaining active picture back into that tile's original collage cell.

Using one consensus crop for the entire sampled set matters. Independently cropping dark scene content at different depths would create unstable fingerprints. The initial implementation deliberately requires unanimity rather than majority voting or outlier handling. Use one coverage and symmetry threshold set conservative enough for the single-frame `thumb1` mode; other modes get the additional protection of cross-frame agreement without mode-specific constants.

The implementation should record bar thickness as normalized fractions of frame width/height so re-encoded copies at different resolutions can agree. Initial thresholds should be conservative and finalized by the corpus, with these constraints:

- tolerate normal codec noise in black bars;
- reject one-sided borders and black corner overlays;
- support both letterbox and pillarbox layouts, including narrow vertical content inside a 16:9 canvas;
- reject ambiguous candidates with both horizontal and vertical crops;
- never crop based only on two or four individual corner pixels.

This is deliberately narrower than general crop matching in issue #17.

### 3. Opt-in rotated-copy matching

Add a checkbox labeled `Detect rotated video copies` to the scan settings. Persist it through `Prefs`/`QSettings` and default it to `false`. Its tooltip should explain that it compares 90, 180, and 270-degree versions and can add fingerprinting and comparison work. Black-frame substitution and black-bar normalization remain independent of this setting.

When the setting is enabled, build four fingerprints for each usable thumbnail segment during extraction:

- unchanged orientation;
- each tile rotated 90 degrees clockwise in its existing collage cell;
- each tile rotated 180 degrees in its existing collage cell;
- each tile rotated 90 degrees counter-clockwise in its existing collage cell.

When it is disabled, build only the unchanged fingerprint and run exactly the current orientation comparison. The rotated fingerprints use the black-bar-normalized matching image. Tile order is unchanged. No full-size rotated images need to be retained after their reduced fingerprints are produced.

Pair matching proceeds as follows:

1. If rotated-copy detection is disabled, run only the existing unchanged-orientation comparison.
2. If enabled, run the unchanged-orientation comparison first. If it matches, return it without further rotation work.
3. Otherwise compare the unchanged fingerprint on one side with the 90, 180, and 270-degree fingerprints on the other. Rotating only one side covers every relative right-angle orientation without a 4x4 cross-product.
4. A rotated fallback must pass both pHash and SSIM safeguards, even when the selected UI mode is pHash. This secondary check is required because the fallback gives a pair more matching opportunities.
5. Apply conservative minimum floors to the rotated pHash and raw SSIM scores in addition to the user-selected threshold. Initial calibration candidates are 57/64 pHash bits and 0.90 raw SSIM, but these are not release decisions until measured against the fixture corpus.
6. Use the same fingerprint-pair scoring function for base and rotated candidates. Record the winning relative rotation only in the transient `VideoPairMatchResult` for tests and verbose diagnostics; do not persist or propagate it into the comparison window in the first version.

The normal comparison path and its thresholds remain unchanged. The setting, rather than orientation metadata, controls whether the extra comparisons happen. This makes the behavior complete for right-angle rotations, including upside-down 180-degree copies, without adding any cost or false-positive opportunity to the default configuration.

### Fingerprint representation

The current `grayThumb` stores 16x16 `CV_32F` matrices. Keeping three more float matrices per segment would be needlessly expensive for very large libraries. Store compact 8-bit grayscale SSIM inputs and convert to float only for the small number of pairs that reach SSIM:

```cpp
enum class FingerprintRotation { none, clockwise90, rotated180, counterClockwise90 };

struct VisualFingerprint {
    uint64_t phash = 0;
    std::array<uint8_t, 16 * 16> ssimPixels{};
    bool usable = false;
};
```

`Video` can hold this for up to two `cutEnds` segments and four orientations. Four 256-byte SSIM inputs use the same pixel storage per segment as one current 16x16 float matrix, before small struct overhead. This replaces the current `hash[]` and `grayThumb[]` representation; it must not be stored alongside it. This keeps comprehensive right-angle support from materially multiplying per-video memory.

The zero pHash should no longer be the only validity signal. An explicit `usable` flag avoids coupling low-information policy to a hash value that can otherwise be valid data.

## Performance requirements

- Frame analysis performs one shared grayscale downscale per sampled frame for both low-information and bar checks.
- The no-bars path performs only small edge-band checks on that shared proxy.
- Detailed band walking runs only after the edge fast path finds a candidate.
- A low-information frame triggers at most one extra decode.
- With rotated-copy detection off, no rotation-specific fingerprints or comparisons are added; the other matching improvements remain independent.
- With it enabled, three rotated reduced fingerprints are computed once per video from already extracted frames; no additional FFmpeg decoding or metadata reads are needed.
- An otherwise unmatched pair performs at most three additional XOR/popcount pHash comparisons. Existing measurements estimate the current pHash path at about one millisecond per 1,000 pairs, and background discovery parallelizes this work.
- SSIM remains behind pHash, and rotated SSIM runs only for a rotated pHash candidate.
- Peak extraction memory must retain only the existing review collage plus one reusable matching scratch image. Build and fingerprint one orientation at a time, and keep only compact fingerprints.

Release target: with rotated-copy detection off, less than 5% p95 extraction-time regression on a representative no-bars corpus and no material pair-scan regression. With it enabled, record extraction and complete pair-discovery overhead for small, large, dense, and sparse libraries. Peak memory must remain comparable to the current representation. These targets may be adjusted only with measured results recorded in the implementing PR.

## False-positive safety and acceptance criteria

Create a labeled corpus before finalizing constants. It must contain at least:

Positive pairs:

- original and recompressed copy;
- original and physically rotated 90, 180, and 270-degree recompresses;
- original and copy with symmetric letterbox bars;
- original and copy with symmetric pillarbox bars;
- copies with a black frame at a nominal sample but matching content at the substitute position;
- the above in no-cache and warm-cache scans built with the current cache generation.

Negative pairs:

- different videos with the same duration and dimensions;
- different portrait videos embedded in identical 16:9 pillar bars, reproducing issue #138;
- different title cards on black backgrounds;
- unrelated near-uniform black, gray, and white frames;
- naturally dark scenes with nonuniform detail;
- frames with one-sided black borders;
- frames with black corners but no continuous bars;
- unrelated videos at each relative right-angle rotation;
- same-orientation, same-duration negatives which exercise the 180-degree fallback.

Acceptance criteria:

- All matches found by the current implementation in the corpus remain matches.
- The targeted bar-normalized and substituted-frame positive pairs match at the agreed default settings; rotated positives match when rotated-copy detection is enabled.
- With rotated-copy detection disabled, rotation fingerprints are not generated and pair decisions remain identical to the base path.
- No pair labeled negative becomes a match at the default threshold or at the lower threshold selected for the issue #138 regression set.
- Different videos sharing bars score based on their active pictures and no longer match merely because of the bars.
- A multi-frame bar crop is accepted only with unanimous axis/boundary agreement; an ambiguous or dissenting tile leaves the whole set uncropped.
- Fresh, warm-cache, and cache-only results built with the current cache generation are documented and consistent.
- An incompatible old cache is invalidated as a unit and leads to a clean cache miss/full rescan, with no record migration or mixed-generation results.
- The one-retry bound and both states of the rotated-copy preference are covered by tests.
- Performance and peak-memory targets above are measured, not inferred from operation counts.

“No new false positives” is evaluated against this corpus and existing end-to-end fixtures. It cannot be proven for every possible video, so the implementation must fail closed whenever detection is ambiguous.

Complexity acceptance criteria:

- Only one analysis proxy is built for each resolved sampled frame and shared by low-information and bar checks.
- The old `hash[]`/`grayThumb[]` representation and hash-zero validity convention are removed, not retained beside `VisualFingerprint`.
- Base and rotated candidates use one fingerprint-pair scorer; enabling rotation changes only the orientation list and applies the additional safety policy.
- No new cache columns or per-video persisted transform metadata are introduced.
- Extraction retains no more than the review collage, one matching scratch image, compact fingerprints, and small per-tile analysis records.
- The implementation adds one user-facing preference and no internal-threshold settings.

## Implementation plan

### Phase 1: fixtures and baseline

1. Add small deterministic video fixtures, generated from distinct source patterns and encoded with the project's supported FFmpeg workflow.
2. Produce rotated, bar-encoded, and black-sample variants from those sources. Keep commands or a fixture-generation script beside the tests so the intent is reproducible.
3. Record current pair scores and match decisions for every labeled positive and negative pair in pHash and SSIM modes.
4. Record no-cache, warm-cache, and cache-only behavior for the new matching paths using the current cache generation.
5. Add a small extraction benchmark for ordinary videos and videos that activate each new path.

This phase prevents tuning only against the motivating positives.

### Phase 2: extract testable visual helpers

1. Add one small, pure visual-fingerprint module containing:
   - one shared-proxy frame analysis for low information and bar candidates;
   - strict symmetric-bar agreement; and
   - tile crop/rotation plus compact pHash/SSIM fingerprint construction.
2. Do not introduce a transform class hierarchy or separate feature pipelines. Keep file I/O, FFmpeg seeking, and cache writes in `Video`; keep the small image functions deterministic and independently testable.
3. Replace `hash[]`, `grayThumb[]`, `_almostBlackBitmap`, and implicit `hash == 0` validity with `VisualFingerprint` and its explicit usable flag. Delete the superseded path in the same phase rather than retaining a compatibility branch.
4. Centralize segment construction and validity so `cutEnds` behavior is expressed once. Update SSIM input handling to consume compact 8-bit data and convert to float only at comparison time.
5. If persisted captures are incompatible, add only a whole-cache generation invalidation; do not add conversion or recovery code.

Likely files: `QtProject/app/video.h`, `QtProject/app/video.cpp`, a small `visualfingerprint.{h,cpp}` helper, `QtProject/app/comparison/internal/ssim.*`, and focused tests.

### Phase 3: monochrome-frame substitution

1. Add one capture-slot resolver for cache load versus fresh decode, native-size analysis, and selection of the nominal or substitute image.
2. Add the single directional substitute attempt without merging it with the existing shortened-duration decode-failure recovery.
3. Decide the selected image before the slot's cache write; a successful substitute occupies the nominal cache column without extra provenance fields.
4. Draw the selected image into the review collage and retain only its small analysis result for later consensus.
5. Make final rejection depend on useful fingerprints for the selected thumbnail mode.
6. Add tests for forward/backward direction, retry bounds, failed substitutes, normal compatible-cache handling, and cache-only no-I/O behavior.

### Phase 4: black-bar normalization

1. Reuse the frame-analysis proxy for the edge-band fast path and detailed linear row/column scan.
2. Require unanimous cross-tile axis/boundary agreement and reject ambiguity; do not add voting or outlier recovery.
3. Derive the cropped/resized matching collage in one reusable scratch image while retaining the uncropped review collage.
4. Run all issue #138 positive and negative fixtures before changing any threshold.
5. Measure the ordinary-video extraction overhead and tighten the early-return path if needed.

### Phase 5: opt-in rotated fingerprints and fallback

1. Add and persist the `Detect rotated video copies` preference, defaulting to off, and include it in the immutable scan-time match configuration.
2. Drive one fingerprint loop with a fixed orientation list: only unchanged when disabled, or unchanged/90/180/270 when enabled. Reuse one scratch image.
3. Factor one fingerprint-pair scorer out of the current matcher and use it for both base and rotated candidates, including one shared duration adjustment.
4. Keep the current base comparison first, then loop over all three relative rotations when enabled; do not add eligibility heuristics or a 4x4 comparison matrix.
5. Extend only the transient `VideoPairMatchResult` with the selected relative rotation. Do not add cache or comparison-UI state.
6. Add the pHash-plus-SSIM rotated safety policy around the common scorer and calibrate its minimum floors from the full corpus.
7. Verify background match discovery remains deterministic, reports the same base matches when disabled, and measures enabled-mode overhead.

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

- low-information frames detected;
- successful and failed substitutions;
- videos with accepted letterbox/pillarbox normalization;
- ambiguous bar candidates rejected;
- pairs which perform rotation fallback;
- rotated pHash candidates, rotated SSIM checks, and accepted rotated matches.

Keep these in one scan-local aggregate diagnostics structure rather than adding fields to each `Video` or persisting them. These counters make threshold and performance regressions diagnosable without adding telemetry or changing normal UI output.

## Decisions to preserve during implementation

- Normalize matching inputs; do not lower global thresholds to recover these cases.
- Use one shared per-frame analysis proxy, retain only its small analysis result, and discard its pixels before cross-frame consensus.
- Replace the old fingerprint/validity representation instead of maintaining parallel legacy and normalized paths.
- Preserve original bars in the review thumbnail.
- Keep one bounded monochrome-frame substitute attempt.
- Crop only symmetric, unanimously agreed bars and fail closed on ambiguity.
- Handle rotation metadata during decode as today; the fallback is only for physically rotated pixels.
- Keep rotated-copy detection off by default and compare 90, 180, and 270-degree variants when enabled; mirrors remain out of scope.
- Use one common pair scorer and a fixed orientation loop; do not build a second rotated matcher.
- Require both pHash and SSIM evidence for a rotated fallback.
- Treat zero additional corpus false positives as a release gate, not a best-effort aspiration.
