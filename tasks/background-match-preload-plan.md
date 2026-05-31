# Background Match Preload Plan

## Goal

Make sparse match discovery non-blocking.

When a library contains many videos but only a few duplicate or similar matches, finding the next match can require scanning a very large number of pair comparisons. Today that scan happens mainly when the user presses "next", so reviewing duplicates becomes a repeated cycle of waiting, deciding, clicking, and waiting again.

The goal is to let comparison matching run continuously in the background after videos are loaded, while still allowing the user to start reviewing immediately. If the user wants to wait before reviewing, the app should be able to keep pre-finding matches until much or all of the comparison space is explored. If the user starts right away, background matching should keep working ahead of them and reduce or eliminate waits on later navigation.

This is similar in spirit to background video discovery, but better suited to interactive review: matching should not block the comparison window, should not prevent manual cleanup, and should adapt when the user navigates next, previous, or seeks to a different position.

Longer term, this model could also support moving some expensive video preprocessing out of the initial scan and into later non-blocking work. Metadata needed to list videos should stay in discovery, but heavier derived data such as additional image extraction could eventually be produced by the same kind of background pipeline.

## Design Shape

Keep the responsibilities separated:

- Workers discover candidate content matches.
- Workers publish explored safe ranges.
- The main thread combines safe ranges to know which positions are fully explored.
- The main thread decides whether a candidate match is still displayable.
- The main thread owns UI, navigation, cleanup actions, and worker lifecycle.

The important distinction is between a **candidate content match** and a **displayable match**. Workers should answer "did these two videos match by content under the current comparison settings?" The main thread should answer "should this candidate still be shown to the user?" The split is a performance tradeoff: workers can focus on the expensive content-matching logic, while the main thread can quickly filter out few candidates that are no longer relevant due to user actions.

## Comparison Space

All possible video pairs are represented as a linear 1-based comparison position:

- `positionForPair(left, right)` maps a video pair to a position.
- `pairAtPosition(position)` maps a position back to a video pair.

`_videos` is the stable comparison universe during review. Videos are not removed from the comparison list when a file is trashed, moved, renamed, or ignored, so pair positions remain meaningful. Resorting is different: it changes the order of `_videos`, so it changes the mapping between positions and pairs.

## Worker Ownership

Workers own disjoint subsets of the linear comparison space. The current approach uses parity:

- worker 0 owns even positions.
- worker 1 owns odd positions.

A worker safe range means: every position in that range that belongs to the worker has been evaluated with the current comparison settings.

A combined safe range means: every position in that range has been evaluated by its owning worker. The main thread can safely conclude whether the range contains any queued candidate matches, subject to final displayability checks, and wait for new candidate matches to be discovered when needed.

## Safe Range Invariants

The range logic should be explicit and tested around these rules:

- A position is safe if its owning worker has covered it.
- A combined safe range `[start, end]` is safe only if every position from `start` to `end` is safe.
- Safe ranges should merge/simplify: if two ranges touch with no unsafe position between them, they should become one range.
- Combined safe ranges may be assembled from adjacent worker ranges, not only overlapping worker ranges.
- Worker ranges may contain holes; combined safe ranges must never cross a hole.
- The first and last comparison positions must be handled correctly, including very small video sets.
- A worker may scan backward temporarily, then when reaching the end of the current unexplored range, it should return to forward scanning from the next unexplored position.
- When a worker reaches one end of the space, it should wrap to the earliest remaining unexplored region if any exists.

## Displayability Checks

Before displaying a queued candidate match, the main thread should redo cheap displayability checks:

- both files still exist.
- neither video is marked as trashed.
- the pair is not currently ignored.
- names are still contained, if that setting is treated as a display filter.

This repeated work is acceptable because displaying a match is rare compared with evaluating all possible pairs, and it avoids reinitializing workers for cleanup actions that do not change the comparison space.

Current cleanup behavior to preserve:

- Delete/trash actions do not remove videos from `_videos`; they mark the `Video` as `trashed` and remove cache entries.
- Apple Photos "deletion" may leave the original file present on disk, so checking `trashed` is required in addition to checking file existence.
- Manual move does not update the video's stored path; the old path becomes unavailable, so file-existence checks skip later candidates for that video.
- Filename swap updates `_filePathName` for both video objects, but keeps the same `_videos` indexes.
- Ignoring a pair writes to the ignored-pairs DB. Treat ignored-pair state as a display filter, not as part of safe-range discovery.

## Reset Rules

No generation id is needed if changes that redefine content matching or pair positions use a strict stop/reset discipline:

1. Stop workers and wait for them to finish.
2. Apply the result-affecting change.
3. Clear preloaded matches.
4. Clear all worker safe ranges and combined safe ranges.
5. Restart workers from the best current position and direction.

Reset workers for changes that redefine unchecked content-match results or pair positions:

- comparison mode changes.
- threshold changes.
- thumbnail mode changes.
- SSIM block size or duration modifier changes.
- sort order changes.

Do not reset workers for changes that only affect displayability of already found candidates:

- deleting, trashing, moving, or renaming files.
- ignoring a pair.
- name-containment filtering, if enabled as a display filter.

If name containment or ignored-pair state were baked into worker discovery, then those settings would become reset-triggering inputs. Prefer keeping them as display filters so safe ranges stay reusable.

## Components

### `SafeRanges`

Move range math into a small isolated helper that can be tested without constructing `Comparison`.

Responsibilities:

- Add a safe range for a worker.
- Merge ranges for one worker using that worker's stride.
- Keep worker ranges normalized, with adjacent/overlapping ranges merged.
- Build combined safe ranges from the worker-owned ranges.
- Help the controller find unexplored space for a worker when starting, retargeting, or wrapping.

The exact query methods can stay flexible while the range semantics settle. Start with the smallest API the controller needs, and grow it from tests rather than guessing every helper upfront.

Sketch:

```cpp
struct SafeRange
{
    int64_t start = 0;
    int64_t end = 0;
};

class SafeRanges
{
  public:
    void clear();
    void addWorkerRange(int worker, SafeRange range);
    ...
};
```

Internally, `SafeRanges` stores multiple ranges, because explored space can have holes:

```cpp
QVector<SafeRange> workerRanges[2];
QVector<SafeRange> combinedRanges;
```

The public methods operate on that collection. `addWorkerRange()` adds one newly explored interval, then merges it into the appropriate worker's vector. Combined ranges are derived from the worker ranges whenever the controller needs to know what is safe for navigation.

`SafeRange` is inclusive: `{start: 4, end: 10}` covers the positions from 4 through 10 according to the context where it is used.

For a worker range, the worker ownership/stride is implied. For example, if worker 0 owns even positions, worker 0 range `{4, 10}` means positions `4, 6, 8, 10` were checked by worker 0. It does not claim anything about positions `5, 7, 9`.

For a combined range, every position in the interval is covered by its owning worker. Combined range `{4, 10}` means positions `4, 5, 6, 7, 8, 9, 10` are all safe.

### `PreloadController`

To better separate concerns, move worker lifecycle and preload state out of `Comparison`.

Responsibilities:

- Start workers.
- Stop workers.
- Reset all worker state.
- Retarget workers to a requested position and direction.
- Own preloaded candidate matches.
- Own `SafeRanges`.
- Expose thread-safe navigation/display snapshots to the main thread.

`Comparison` asks the controller for:

- next preloaded candidate match after current position.
- previous preloaded candidate match before current position.
- whether navigation around a requested position is already safely explored.
- current preload progress for display.

## Worker Loop

Each worker repeatedly finds the next unexplored position it owns, then scans in the active direction.

High-level flow:

1. Read current target direction and priority position.
2. Find the nearest unexplored owned position from that target.
3. Compare pairs in that direction using the worker's stride.
4. Buffer candidate matches found.
5. Continue until one of these happens:
   - 250 ms have elapsed since the last published batch.
   - the worker reaches an already safe position.
   - the worker reaches the beginning or end.
   - a stop or retarget request is seen.
6. Emit buffered candidates and the newly safe range.
7. If the worker was scanning backward, return to forward scanning from the next unexplored position after the backward-covered area.
8. If the worker reaches the end, wrap to the first remaining unexplored owned position.

The 250 ms publishing rule keeps the UI responsive without assuming a fixed chunk size. Some comparisons are cheap and some are expensive, so time-based batches should produce steadier feedback than fixed-size batches.

Workers should not read live UI widgets. They should receive the comparison settings they need when they are started or reset.

## Navigation Behavior

### Initial Open

- Reset worker state.
- Start workers scanning forward from position 1.
- The first display waits until either:
  - a displayable candidate is available in the combined safe range after the current position.
  - the combined safe range reaches the end with no displayable match.

### Next

- Ensure workers are running forward from `currentPosition + 1`.
- If the next queued candidate is inside the combined safe range and passes displayability checks, display it.
- If a queued candidate fails displayability checks, skip it and keep looking.
- Otherwise wait briefly while processing UI events and worker updates.
- If the combined safe range reaches the end and no displayable match exists, show the existing "out of videos" flow.

### Previous

- Retarget workers backward from `currentPosition - 1`.
- If a previous queued candidate is inside the combined safe range and passes displayability checks, display it.
- If a queued candidate fails displayability checks, skip it and keep looking.
- Otherwise wait while workers explore backward.
- If the combined safe range reaches position 1 with no displayable match, return to the first available match or keep current fallback behavior.
- After the backward request is resolved, workers return to forward discovery of unexplored positions.

### Seek

- Retarget workers to the requested comparison position.
- Only trust queued candidates whose positions are inside combined safe ranges.
- Prefer retargeting over full reset when settings have not changed.
- Reset only when the underlying comparison result or pair-position mapping may have changed.

## Test Plan

Add tests for `SafeRanges` first, before expanding UI behavior:

- one worker range merge with stride 2.
- combined ranges from adjacent odd/even coverage.
- combined ranges do not cross holes.
- combined range at beginning.
- combined range at end.
- very small spaces: max comparison count 0, 1, and 2.
- next unexplored forward from inside a covered range.
- next unexplored backward from inside a covered range.
- backward scan then forward resume.
- wrap to earliest remaining unexplored position after reaching end.

Then add integration-style tests around `Comparison` behavior:

- opening comparison starts preload and still displays the first match.
- next uses a preloaded candidate when available.
- previous can retarget workers backward.
- changing threshold stops and clears preload state before restarting.
- changing sort order stops workers before sorting `_videos`.
- ignoring a pair does not clear safe ranges and skips the ignored candidate before display.
- deleting, moving, or trashing a video does not clear safe ranges and skips unavailable candidates before display.

## Implementation Order

1. Extract and test `SafeRanges`.
2. Add `PreloadController` as the owner of workers, candidate matches, and safe ranges.
3. Fix combined safe range calculation to use per-position ownership, not only overlapping worker ranges.
4. Separate candidate content matching from main-thread displayability filtering.
5. Add time-based worker publishing with a 250 ms interval.
6. Enforce stop/reset only for comparison-space or content-match changes.
7. Remove direct worker reads from UI state.
8. Add focused integration tests for next, previous, reset, retarget, and display filtering behavior.
