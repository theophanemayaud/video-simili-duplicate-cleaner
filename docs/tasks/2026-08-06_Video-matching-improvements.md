# Video matching improvements

Status: implemented; draft PR validation in progress

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

## Implementation result (2026-08-11)

The three improvements now share `VisualFingerprintBuilder` and one compact `VisualFingerprint` representation. The old `hash[]`, `grayThumb[]`, pHash-zero sentinel, and duplicate comparison branches have been removed.

- Every fresh or cached frame is analyzed through one aspect-preserving grayscale proxy capped at 64 pixels on its longest edge. A frame is low-information only when its proxy has both standard deviation below 2 and luminance range no wider than 12. This deliberately conservative rule rejects uniform frames while retaining the existing dark-detail guard.
- A low-information nominal capture receives exactly one directional +/-2 percentage-point retry when decoding is allowed. A successful substitute is stored in the nominal cache slot. Cache-only scans never read the video.
- Black bars use a near-black luminance ceiling of 20, 98% row/column coverage, 4%-45% symmetric opposing insets, at least 10% active picture, one-axis-only acceptance, and unanimous agreement across the informative extracted frames. Bars remain visible in the GUI thumbnail and are cropped only from matching inputs.
- Legacy rotate tags and Display Matrix side data are normalized into one presentation transform, with Display Matrix taking precedence. This is always active. Physically rotated fallback remains a separate persisted setting, off by default.
- Rotation-on extraction builds 0/90/180/270 fingerprints from matching tiles reduced to at most 64 pixels before rotation. Fallback requires at least 57/64 raw pHash agreement and 0.90 raw SSIM, in addition to applicable user thresholds.
- SQLite cache generation 2 invalidates the incompatible derived metadata and captures as one unit while preserving user-authored ignored pairs. There is no record conversion, mixed-generation read, or recovery strategy; users rescan normally.

The first implementation intentionally does not add persisted analysis data, per-video transform state, record migration, configurable retry/threshold machinery, or scan-wide diagnostic state. Focused tests report calibration scores when needed.

## Goals

- Recover duplicate pairs that are currently missed because one copy is physically rotated by 90, 180, or 270 degrees.
- Normalize both legacy rotation tags and FFmpeg Display Matrix presentation metadata during ordinary decoding.
- Prevent a sampled black, white, gray, or otherwise near-monochrome frame from making an otherwise usable video unmatchable or materially weakening its fingerprint.
- Make letterboxed and pillarboxed copies comparable to copies without encoded bars.
- Reduce the black-bar false positives described in issue #138.
- Preserve existing matching behavior for ordinary videos as closely as possible.
- Keep the fast path cheap for videos without low-information frames or black bars, and preserve the current rotation cost when rotated matching is disabled.
- Make the behavior deterministic and testable in no-cache, warm-cache, and cache-only scans.
- Reduce matching-specific state and branching: one analysis pass per frame, one fingerprint representation, and one pair-comparison path.
- Use a two-tier test corpus: a few megabytes of deterministic repository fixtures for required tests, plus the existing few-gigabyte `/Dev` corpus for labeled real-world regression and performance coverage.

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
- Checking the external `/Dev` corpus into this repository or building a full Cartesian product of every transform.

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

## Pre-implementation behavior and findings

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

Rotation metadata is only partially handled. `Video::getMetadata()` reads the legacy stream `rotate` dictionary tag, swaps presentation width/height for quarter turns, and `getQImageFromFrame()` transforms decoded frames. It does not read FFmpeg Display Matrix side data, which is how all eight metadata-rotated files found in the local corpus express their `-90` degree presentation. Those files therefore expose an existing normalization gap.

Normalize both metadata representations into one presentation angle and apply it once during decode. Correct presentation metadata is authoritative and remains always enabled; it must not depend on the opt-in rotated-copy setting. Once both forms are handled, copies whose only difference is correct rotation metadata should arrive in the same presentation orientation.

The opt-in fallback addresses the separate case where pixels were physically re-encoded in a different orientation, as reported in Discussion #178. Neither current metric is rotation-invariant:

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
2. Classify the frame as low information only when a dominant luminance band covers nearly the complete proxy and meaningful spatial detail is absent. A screening candidate of at least 98% of pixels within a luminance span no wider than 12/255 incorrectly classified an existing very dark video with visible detail, so those constants are not release defaults. Finalize one simple, color-neutral predicate from the tracked fixtures and labeled local corpus; do not retain the first-pixel heuristic or add a separate black-only path.
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

Before generating fingerprints, resolve the presentation angle from either the legacy `rotate` tag or Display Matrix side data. If both representations are present, use one documented and tested precedence rule based on FFmpeg's API output; never apply both transformations. This metadata normalization is part of ordinary decoding and is covered with the setting both off and on.

Add a checkbox labeled `Detect rotated video copies` to the scan settings. Persist it through `Prefs`/`QSettings` and default it to `false`. Its tooltip should explain that it compares 90, 180, and 270-degree versions and can add fingerprinting and comparison work. Monochrome-frame substitution and black-bar normalization remain independent of this setting.

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

## Test-data audit and two-tier fixture strategy

### Existing data audit (2026-08-09)

The repository-tracked `samples/videos` set contains two encodes of the same video plus their reference metadata/thumbnails. It is about 1.0 MB total and is useful for basic extraction, cache, and same-content matching checks. It contains none of the three new feature variants and no unrelated same-duration source for negative matching.

The external local corpus at `/Users/theophanemayaud/Dev/Videos across all formats with duplicates of all kinds/Videos` currently contains 218 videos and occupies about 2.6 GB. The existing test baseline expects 214 processable videos and 84 matching videos. Its useful breadth includes:

- ten container extensions and ten decoded video codecs;
- 198 landscape, 18 portrait, and 2 square encoded dimensions;
- durations from below one second to over 27 minutes; and
- 19 byte-identical duplicate groups containing 57 files, plus re-encoded and renamed copies.

This makes it a good format, natural-content, cache, performance, and broad regression corpus. It is not yet a ground-truth matching corpus: the test asserts aggregate counts, and the corpus has no manifest identifying which exact pairs should or should not match. The existing per-file `.txt` and `.jpg` references validate extraction output, not duplicate relationships.

A read-only screening audit of metadata and the union of all current sample positions found these feature-coverage gaps:

- **Rotation:** eight files carry `-90` degree FFmpeg Display Matrix side data, but the app currently reads only the legacy `rotate` tag. They expose a metadata-normalization gap rather than proving the existing path. No likely pair was found that matched only after physically rotating pixels, and there is no known 90/180/270 physical-rotation matrix.
- **Low-information samples:** current reference hashes contain four videos with both `cutEnds` hashes zero and one with a single zero end. Applying the draft proxy rule to all twelve union sample positions found only two candidate videos; only two of nineteen nearby substitute attempts became informative. Neither supplies a labeled duplicate pair proving the feature. One candidate, `100_6248.dv`, contains visible nighttime detail despite its overwhelmingly dark background, demonstrating that the provisional histogram threshold is too aggressive by itself.
- **Black bars:** a conservative proxy screen found four single-frame candidates, but only one candidate across every sampled mode, and that was a roughly one-proxy-pixel pillar border on a file whose current reference thumbnail is empty. The `MOV06487` re-encoded pair has the same substantial pillar bars on both copies; it does not provide a barred-versus-barless positive. A dark `IMG_3561.MOV` frame looked bar-like at one position but failed cross-frame consensus, making it a useful natural negative rather than a positive.
- **Interactions:** there is no known pair combining substitution, bar normalization, and physical rotation.

These counts are screening evidence, not proposed algorithm baselines. The audit used the draft proxy concepts to locate candidates; final labels require visual review and the implementation must be evaluated through the app's actual decoder and matcher.

### Tier 1: tiny repository-tracked fixtures

Keep required unit and end-to-end coverage self-contained and fast. The existing tracked media is about 1.0 MB; target no more than roughly 2 MB of new video data so the complete tracked video set remains only a few megabytes.

Most image-policy cases should use generated in-memory `QImage`/pixel fixtures and add no binary files: uniform black/gray/white frames, dark nonuniform detail, title cards, symmetric bars, one-sided borders, black corners, inconsistent cross-frame boundaries, and right-angle tile transforms.

For decoder/cache/end-to-end wiring, add one reproducible group of tiny videos with no canvas edge longer than 200 pixels, approximately 10 seconds long, low frame rate, no audio, and efficiently encoded. Use narrow portrait sources so identical pillar bars occupy enough of the encoded frame to be a meaningful negative guard:

1. two visually distinct base sources `A` and `B` with identical duration/dimensions and asymmetric moving detail;
2. physical 90, 180, and 270-degree re-encodes of `A`, with rotation metadata removed;
3. letterboxed and pillarboxed copies of `A`;
4. a pillarboxed copy of `B` using exactly the same bars as `A`, for the issue #138-style negative; and
5. one copy of `A` with short near-monochrome windows at deterministic sample positions, while the specified +/-2 percentage-point substitutes remain informative; and
6. one physically rotated copy of `A` carrying inverse Display Matrix metadata so its intended presentation is unchanged.

That is a ten-video feature matrix. The same files supply positives and negatives: `A` against its variants must match under the applicable setting, while every `A`/`B` pairing must remain negative, including relative rotations and identical pillar bars. Use asymmetric sources so 180-degree behavior cannot pass accidentally. The metadata-corrected copy must match even when rotated-copy detection is disabled.

Track the binaries, one small manifest, and the exact FFmpeg generation script/commands. The script documents reproducibility but tests consume the checked-in binaries and must not require an FFmpeg executable at runtime. Use one new self-contained end-to-end test to exercise actual extraction, warm cache, cache-only reuse, base matching, the rotated setting off/on, and representative `thumb1`, multi-frame, and `cutEnds` paths. Keep threshold and edge-policy permutations in pure helper tests rather than multiplying video files.

### Tier 2: labeled external `/Dev` corpus

Keep the external corpus for realistic encoding diversity, natural negatives, performance, and full-library false-positive checks. It is already about 2.6 GB; prefer derivatives of small existing sources, budget new files to at most roughly 250 MB, and aim to keep the corpus near or below 3 GB total.

Add only the real-world cases absent from the audit:

- physical 90, 180, and 270-degree re-encodes of one real source;
- one correctly oriented companion for an existing Display Matrix file, to verify metadata normalization independently of fallback matching;
- barless, letterboxed, and pillarboxed versions of one source plus a different source with identical bars;
- early- and late-position monochrome-window variants with informative substitutes;
- at least one transform encoded differently from its source, rather than a byte-identical copy; and
- one combined stress variant, such as physical rotation plus encoded bars plus a replaceable monochrome sample, instead of a full transform Cartesian product.

Reuse and label existing natural cases where useful: metadata-rotated files for the unchanged path, `100_6248.dv` as a dark-detail classifier guard, `IMG_3561.MOV` as a cross-frame bar-consensus rejection, and the two barred `MOV06487` encodes as same-bars/same-content coverage.

Add a versioned `matching-ground-truth.csv` to the external corpus repository with one row per video: relative path, expected processing state, stable content-group ID, physical orientation, and fixture tags. Files in the same content group are positives subject to the rotation setting; different content groups are negatives. Seed the manifest from the 19 exact duplicate groups and current matcher candidates, then manually review the remaining ambiguous groups once. Tests must report incorrect pair identities, not merely a changed aggregate match count.

Use the same manifest reader and pair assertions for the tracked and external tiers. The tracked suite runs normally; the external suite remains an explicit local test which skips cleanly when its corpus path is absent. Both tiers run through the app's actual fingerprint/matcher code, while the external tier additionally records extraction time, pair-discovery time, and peak memory.

### Prepared red-test baseline (2026-08-09)

The fixture phase is now implemented without implementing the matching behavior:

- The repository contains the planned ten-video matrix under `samples/videos/matching`. Its final video payload is 104,369 bytes and the generator plus manifest bring the matrix to 107,971 bytes. The complete tracked `samples/videos` tree is 1,129,319 bytes.
- The tracked A/B sources are temporally stable 40x160, ten-second, 10 fps H.264 videos. This keeps the +/-2% case focused on substitution instead of motion drift. The matrix includes physical 90/180/270-degree copies, letterbox and pillarbox copies, identical-bar A/B negatives, monochrome windows at 8% and 96% with informative 10%/94% substitutes, and a physically rotated Display Matrix copy.
- The external corpus adds eleven derivatives under `Videos/Matching feature fixtures`, so the existing whole-app and per-video tests exercise the expanded 229-video tree as well as the focused feature matrix. The final video payload is 6,933,906 bytes; references, generator, and manifest included, the feature addition is about 7.17 MB.
- The external manifest has 229 rows: all 218 legacy videos plus the eleven derivatives. The strict current matcher produced 154 pair edges forming 54 complete, filename-consistent groups. Those groups seed the no-new-cross-group baseline. A tagged 22-video subset supplies the feature contracts and natural guards.
- Both generation scripts reproduce byte-identical outputs on the current FFmpeg toolchain. Tests consume the checked-in files and do not invoke FFmpeg.

`test_video_matching_features` is a required CI target. Its focused contracts now pass for Display Matrix presentation normalization, monochrome substitution, letterbox/pillarbox normalization, physical 90/180/270-degree matching, the rotation preference, whole-cache invalidation, and conservative in-memory bar analysis. Its tracked end-to-end rows pass in no-cache, warm-cache, and cache-only modes with rotation both disabled and enabled. The same target runs four optional local external feature rows and a separate legacy-corpus no-new-cross-group baseline; external rows skip when the corpus is absent.

The manifest excludes substantially different-duration partial clips from required recovery while still rejecting them as cross-content false positives. This keeps the feature aligned with its explicit non-goal rather than weakening the matcher to satisfy two pre-existing partial-clip cases.

### Validation results (2026-08-11)

- Required comparison, main-window, auto-delete, simplified-video, and matching-feature tests pass locally.
- The tracked ten-video matrix passes every fresh, warm-cache, and cache-only row, with rotation both off and on. Rotation-on no-cache extraction is 15 ms versus 12 ms off on this tiny synthetic set.
- The 22-video real-world feature set passes all labeled pairs. After reducing matching scratch tiles before rotation, no-cache extraction is 1.122 s with rotation on versus 1.096 s off. The identical-pillar-bar negative remains far below the rotated floor.
- The legacy 218-video labeled baseline takes 20.895 s versus its recorded 20.828 s pre-implementation run (+0.3%) and adds no cross-content matches.
- The expanded ordinary `/Dev` whole-app scan finds 229 files, 226 valid videos, and 89 matched videos. No-cache is 3.380 s total; warm-cache is 1.110 s assessed; cache-only is 0.659 s assessed.
- On the mounted 100GB corpus, rotation off finds 12,506 files, 12,352 valid videos, and 6,552 matched videos in 237.641 s extraction / 246.092 s total. Rotation on finds the same files and valid videos and 6,556 matched videos in 253.217 s extraction / 276.596 s total: approximately +6.6% extraction and +12.4% total for the opt-in setting. Pair comparison itself remains 23.379 s with rotation on versus 8.451 s off.
- A warm-cache 100GB assessed scan completes in 44.978 s, below its 50-second test budget, and finds 12,351 valid / 6,543 matched videos.
- The 100GB per-file reference audit reports 279 changed thumbnails out of 12,506 (2.2%), with no metadata or decode regressions. Representative inspection shows the large visual changes are intended corrections of previously sideways Display Matrix videos; SSIM-close changes are matching-fingerprint bar normalization. The stored reference files have not been refreshed automatically.

The one-shot macOS process-memory readings varied between runs and allocators: maximum resident size was 2.63 GB off and 3.71 GB on, while the reported peak-footprint metric moved in the opposite direction. The retained fingerprint representation itself remains comparable to the old representation: four compact 256-byte SSIM inputs per segment replace one 256-float SSIM matrix per segment. Treat the one-shot process figures as diagnostic rather than a stable memory regression measurement.

## False-positive safety and acceptance criteria

Create a labeled corpus before finalizing constants. It must contain at least:

Positive pairs:

- original and recompressed copy;
- original and physically rotated 90, 180, and 270-degree recompresses;
- original and copy with symmetric letterbox bars;
- original and copy with symmetric pillarbox bars;
- copies with a black frame at a nominal sample but matching content at the substitute position;
- one pair combining physical rotation, encoded bars, and a replaceable monochrome sample;
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
- An incompatible old cache's derived metadata and captures are invalidated as a unit and lead to a clean cache miss/full rescan, with no record migration or mixed-generation results; user-authored ignored pairs remain intact.
- The one-retry bound and both states of the rotated-copy preference are covered by tests.
- Performance and peak-memory targets above are measured, not inferred from operation counts.
- The tracked ten-video matrix and its references add no more than roughly 2 MB, keeping all repository-tracked video fixtures within a few MB total.
- The external corpus has pair-level ground truth and reports the exact unexpected/missing pairs; aggregate counts alone are no longer accepted as feature validation.
- External derivatives add no more than roughly 250 MB and keep the full local corpus near or below 3 GB unless a larger addition is separately justified.
- The existing dark-detail and inconsistent-bar natural cases remain negative classifier/crop guards.

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

1. Define one simple manifest schema and one pair-assertion helper shared by the tracked and external tiers.
2. Add in-memory image fixtures for classifier, bar-boundary/consensus, crop, and rotation policy without adding video files.
3. Generate and check in the ten-video synthetic matrix and its reproducible FFmpeg script, staying within the roughly 2 MB addition budget.
4. Add and manually review the external corpus manifest, then generate only the missing real-world derivatives within the roughly 250 MB addition budget.
5. Record current pHash/SSIM scores and exact pair decisions for every labeled positive and negative pair, with rotation disabled and enabled.
6. Record no-cache, warm-cache, and cache-only behavior for the tracked matrix and current-generation external corpus.
7. Add a small extraction benchmark for ordinary videos and videos that activate each new path; keep the complete external scan as an explicit local benchmark.

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

1. Normalize legacy `rotate` tags and Display Matrix side data into one presentation transform, applied once and independent of the fallback setting.
2. Add and persist the `Detect rotated video copies` preference, defaulting to off, and include it in the immutable scan-time match configuration.
3. Drive one fingerprint loop with a fixed orientation list: only unchanged when disabled, or unchanged/90/180/270 when enabled. Reuse one scratch image.
4. Factor one fingerprint-pair scorer out of the current matcher and use it for both base and rotated candidates, including one shared duration adjustment.
5. Keep the current base comparison first, then loop over all three relative rotations when enabled; do not add eligibility heuristics or a 4x4 comparison matrix.
6. Extend only the transient `VideoPairMatchResult` with the selected relative rotation. Do not add cache or comparison-UI state.
7. Add the pHash-plus-SSIM rotated safety policy around the common scorer and calibrate its minimum floors from the full corpus.
8. Verify background match discovery remains deterministic, reports the same base matches when disabled, and measures enabled-mode overhead.

### Phase 6: end-to-end verification

Run, at minimum:

- the new visual-helper and pair-matcher tests;
- the new self-contained tracked-video matching test in no-cache, warm-cache, and cache-only modes;
- `test_comparison`, including background discovery;
- `test_video_simplified` on the macOS development setup;
- explicit `test_video` whole-app cases relevant to thumbnail/cache behavior;
- the manifest-driven external `/Dev` corpus test when that corpus is available; and
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
- Normalize legacy rotation tags and Display Matrix side data during decode; the opt-in fallback is only for physically rotated pixels.
- Keep rotated-copy detection off by default and compare 90, 180, and 270-degree variants when enabled; mirrors remain out of scope.
- Use one common pair scorer and a fixed orientation loop; do not build a second rotated matcher.
- Require both pHash and SSIM evidence for a rotated fallback.
- Treat zero additional corpus false positives as a release gate, not a best-effort aspiration.
