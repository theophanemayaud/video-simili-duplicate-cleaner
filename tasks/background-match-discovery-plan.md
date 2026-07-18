# Background Match Discovery

## User need

Sparse duplicate libraries make manual review frustrating because finding every
next match can require scanning a large part of the quadratic video-pair space.
The app should use the time spent reviewing one pair to discover later matches,
without making navigation itself asynchronous or coupling workers to user input.

## Design

`BackgroundMatchDiscovery` performs one forward pass over the linear pair space.
Several workers claim fixed-size contiguous chunks from an atomic counter. Chunk
results are delivered to the main thread, which owns the candidate map and chunk
completion bitset.

Workers can finish out of order, but `safeEnd` only advances through contiguous
completed chunks from position 1. Therefore every pair at or before `safeEnd` is
known to have been evaluated. The slider paints this safe prefix green.

Navigation remains synchronous:

- Next and seek consume sparse discovered candidates inside the safe prefix.
- Previous consumes discovered candidates after synchronously checking any gap
  between the current position and the safe prefix.
- Outside the safe prefix, navigation uses the existing pair-by-pair scan.
- Foreground and background scans may duplicate work. This is intentional: the
  small amount of wasted computation avoids worker retargeting, priorities,
  shared navigation state, and complex cancellation rules.

## Matching and filtering

`VideoPairMatcher` is a pure synchronous operation shared by foreground and
background scans. Workers receive an immutable configuration snapshot.

Discovery applies settings that change the content-match result, including the
comparison mode, threshold, thumbnail mode, SSIM block size, and duration
modifiers. Changes to those inputs restart discovery.

Cheap, mutable display filters are applied when a candidate is consumed and do
not restart discovery:

- file existence and trashed state;
- ignored-pair state;
- filename containment.

Sort changes restart discovery because a partially safe linear prefix cannot be
translated into a safe prefix after pair positions are reordered.
