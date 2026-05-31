# Background Match Preload Plan

## Goal

Make sparse match discovery non-blocking.

When a library contains many videos but only a few duplicate or similar matches, finding the next match can require scanning a very large number of pair comparisons. Today that scan happens mainly when the user presses "next", so reviewing duplicates becomes a repeated cycle of waiting, deciding, clicking, and waiting again.

The goal is to let comparison matching run continuously in the background after videos are loaded, while still allowing the user to start reviewing immediately. If the user wants to wait before reviewing, the app should be able to keep pre-finding matches until much or all of the comparison space is explored. If the user starts right away, background matching should keep working ahead of them and reduce or eliminate waits on later navigation.

This is similar in spirit to background video discovery, but better suited to interactive review: matching should not block the comparison window, should not prevent manual cleanup, and should adapt when the user navigates next, previous, or seeks to a different position.

The intent is to keep pair discovery owned by workers and keep UI navigation owned by the main thread. Workers publish matches and safe ranges; the main thread combines those ranges to know when a requested navigation area has been fully explored.

Longer term, this model could also support moving some expensive video preprocessing out of the initial scan and into later non-blocking work. Metadata needed to list videos should stay in discovery, but heavier derived data such as additional image extraction could eventually be produced by the same kind of background pipeline.

## Core Model

All possible video pairs are represented as a linear 1-based comparison position:

- `positionForPair(left, right)` maps a video pair to a position.
- `pairAtPosition(position)` maps a position back to a video pair.

Workers own disjoint subsets of this linear pair space. The current approach uses parity:

- worker 0 owns even positions.
- worker 1 owns odd positions.

A worker's safe range means: every position in that range that belongs to the worker has been evaluated with the current comparison settings.

A combined safe range means: every position in that range has been evaluated by its owning worker, so the main thread can safely conclude whether the range contains a displayable match.

## Important Invariants

The range logic should be explicit and tested around these rules:

- A position is safe if its owning worker has covered it.
- A combined safe range `[start, end]` is safe only if every position from `start` to `end` is safe.
- Combined safe ranges may be assembled from adjacent worker ranges, not only overlapping worker ranges.
- Worker ranges may contain holes; combined safe ranges must never cross a hole.
- The first and last comparison positions must be handled correctly, including very small video sets.
- A worker may scan backward temporarily, then return to forward scanning from the next unexplored position.
- When a worker reaches one end of the space, it should wrap to the earliest remaining unexplored region if any exists.

## State Ownership

### Main Thread

The main thread owns:

- `_videos` ordering and mutable video state.
- UI state.
- comparison preferences.
- worker start, stop, reset, and retarget requests.
- display of the selected duplicate pair.

The main thread should stop workers before applying changes that can affect comparison results.

This includes:

- comparison mode changes.
- threshold changes.
- thumbnail mode changes.
- SSIM block size or duration modifier changes.
- sort order changes.
- setting "names must be contained" changes.
- ignoring a duplicate pair.
- deleting, moving, trashing, or renaming videos.

After the change is applied, the main thread clears preloaded matches and safe ranges, then starts workers again from the appropriate position.

### Workers

Workers own:

- discovering matches in their assigned positions.
- tracking their explored safe ranges.
- periodically publishing matches and newly safe ranges.
- honoring retarget requests between chunks.

Workers should not read live UI widgets. They should receive the comparison settings they need when they are started or reset.

Workers also should avoid reading mutable state while it can be changed by the main thread. The simplest rule is: any result-affecting mutation first stops workers, then mutates state, then clears/restarts worker state.

## Suggested Components

### `SafeRanges`

Move range math into a small isolated helper that can be tested without constructing `Comparison`.

Responsibilities:

- Add a safe range for a worker.
- Merge ranges for one worker using that worker's stride.
- Answer whether a position is covered by its owning worker.
- Return the combined safe range containing a position.
- Return the next unexplored position for a worker from a requested position and direction.
- Return whether all positions in the comparison space are safe.

Possible shape:

```cpp
class SafeRanges
{
  public:
    void clear();
    void addWorkerRange(int worker, SafeRange range);
    bool positionCovered(int64_t position) const;
    std::optional<SafeRange> combinedRangeContaining(int64_t position) const;
    bool nextUncoveredForWorker(int worker, int64_t from, Direction direction, int64_t& result) const;
};
```

### `PreloadController`

Optionally move worker lifecycle out of `Comparison` once the behavior stabilizes.

Responsibilities:

- Start workers.
- Stop workers.
- Reset all worker state.
- Retarget workers to a requested position and direction.
- Own preloaded matches and safe ranges.
- Expose thread-safe snapshots to the main thread.

`Comparison` would then ask the controller for:

- next preloaded match after current position.
- previous preloaded match before current position.
- safe range containing a requested position.
- current preload progress.

## Worker Loop

Each worker repeatedly finds the next unexplored position it owns, then scans in the active direction.

High-level flow:

1. Read current target direction and priority position.
2. Find the nearest unexplored owned position from that target.
3. Compare pairs in that direction using the worker's stride.
4. Buffer matches found.
5. Continue until one of these happens:
   - 250 ms have elapsed since the last published batch.
   - the worker reaches an already safe position.
   - the worker reaches the beginning or end.
   - a stop or retarget request is seen.
6. Publish buffered matches and the newly safe range.
7. If the worker was scanning backward, return to forward scanning from the next unexplored position after the backward-covered area.
8. If the worker reaches the end, wrap to the first remaining unexplored owned position.

The 250 ms publishing rule keeps the UI responsive without assuming a fixed chunk size. Some comparisons are cheap and some are expensive, so time-based batches should produce steadier feedback than fixed-size batches.

## Navigation Behavior

### Initial Open

- Reset worker state.
- Start workers scanning forward from position 1.
- `Next` waits until either:
  - a preloaded match is available in the combined safe range after the current position.
  - the combined safe range reaches the end with no match.

### Next

- Ensure workers are running forward from `currentPosition + 1`.
- If the next match is already preloaded inside the combined safe range, display it.
- Otherwise wait briefly while processing UI events and worker updates.
- If the combined safe range reaches the end and no match exists, show the existing "out of videos" flow.

### Previous

- Retarget workers backward from `currentPosition - 1`.
- If a previous match is already preloaded inside the combined safe range, display it.
- Otherwise wait while workers explore backward.
- If the combined safe range reaches position 1 with no match, return to the first available match or keep current fallback behavior.
- After the backward request is resolved, workers return to forward discovery of unexplored positions.

### Seek

- Stop or retarget workers to the requested comparison position.
- If keeping current results, only trust preloaded matches whose positions are inside combined safe ranges.
- Prefer retargeting over full reset when settings have not changed.
- Reset only when the underlying comparison result may have changed.

## Reset Rules

No generation id is needed if result-affecting changes are applied with a strict stop/reset discipline:

1. Stop workers and wait for them to finish.
2. Apply the result-affecting change.
3. Clear preloaded matches.
4. Clear all worker safe ranges and combined safe ranges.
5. Restart workers from the best current position and direction.

This keeps stale worker results out of the UI without needing to tag every event with a generation.

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
- next uses preloaded match when available.
- previous can retarget workers backward.
- changing threshold stops and clears preload state before restarting.
- changing sort order stops workers before sorting `_videos`.
- ignoring a pair clears stale preloaded matches.
- deleting or moving a video stops workers before mutating video state.

## Implementation Order

1. Extract and test `SafeRanges`.
2. Fix combined safe range calculation to use per-position ownership, not only overlapping worker ranges.
3. Add time-based worker publishing with a 250 ms interval.
4. Enforce stop/reset before every result-affecting mutation.
5. Remove direct worker reads from UI state.
6. Optionally extract `PreloadController` once the behavior is stable.
7. Add focused integration tests for next, previous, reset, and retarget behavior.
