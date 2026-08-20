# Comparison Set Browser

## Status

Proposed and implemented as a first manual-review slice in this pull request.

## Problem

The comparison window currently exposes matches as a sequence of pairs. The user can move to the previous or next pair, or seek through the theoretical pair space with a slider. This works for two copies, but it makes a family of three or more related videos appear as repeated, disconnected decisions. It is difficult to answer basic questions such as:

- How many duplicate families were found?
- Which videos belong to the same family?
- Have all members of a family been reviewed?
- Which copy is the most useful reference to keep?

The pair-space slider also represents comparisons performed rather than the user's review progress, so it is a poor primary navigation control.

## PhotoSweeper research

PhotoSweeper is useful as an interaction reference because it makes a group, rather than a pair, the unit of review. Its current documentation shows three complementary result views:

- **One by One** shows one large item from the selected group and a member filmstrip.
- **Face-to-Face** compares two members with metadata alongside the previews.
- **All in One** provides a scrollable overview of complete groups.

Across those views it keeps a persistent group sidebar, makes marked-for-removal state visible before deletion, supports ordered Auto Mark rules, and performs cleanup as a separate confirmed batch action. The relevant primary sources are the [PhotoSweeper manual](https://overmacs.com/photosweeper/downloads/en/PhotoSweeperManual.pdf), the [official product page](https://overmacs.com/), the [official release notes](https://overmacs.com/?p=releasenotes), and the [Mac App Store listing](https://apps.apple.com/us/app/photosweeper/id463362050?mt=12).

We should borrow the information architecture, not reproduce PhotoSweeper's visual assets or photo-specific features. In particular, histogram, EXIF, and subjective photo quality scoring do not map directly to this app. The useful pattern is:

1. Navigate groups.
2. Inspect members within a selected group.
3. Compare two items in detail.
4. Stage removal choices.
5. Confirm cleanup separately.

## Goals

- Make a duplicate/similarity set the primary unit of manual review.
- Combine all pairwise matches that are connected through one or more matching edges.
- Make every set and every member directly reachable without seeking through pair space.
- Preserve the existing side-by-side previews, metadata, open/reveal actions, ignore action, and safe trash behavior.
- Derive sets in memory from current match results; do not add cache migrations or a persistent set schema.
- Establish a UI and data model that can later support staged selection and batch cleanup.

## Non-goals for the first version

- Mark-for-removal state, multi-set selection, or batch deletion.
- Auto-mark rules or a redesign of the existing Auto tab.
- Persisting sets or review state between scans.
- Inline video playback, an All-in-One grid, or multiple review modes.
- Claiming that every member in a transitive set directly matches every other member.

## Set semantics

Pairwise matches form an undirected graph:

- each processed video is a vertex;
- each accepted pairwise match is an edge;
- each connected component with at least two videos is a duplicate set.

For example, if A matches B and B matches C, the UI shows one set containing A, B, and C even if A and C did not pass the matching threshold directly. This is intentional: the set represents a linked duplicate family, not an all-to-all proof.

Sets are derived from the current scan and rebuilt when matching settings, sort order, ignored pairs, deleted files, or the filename-containment filter change. Video indexes are valid only for one discovery run; no set state is reused after the video vector is reordered.

## First-version interaction

The Manual tab becomes a three-level review surface:

1. A persistent **Duplicate sets** sidebar lists each connected component with a representative thumbnail, member count, and combined size.
2. Selecting a set populates a compact **member gallery**. The first member in the current sort order is the stable reference, and selecting item 2 through N replaces the right-hand comparison video.
3. The existing side-by-side previews and metadata remain the detailed comparison surface. Previous and Next cycle through members of the selected set rather than through the global pair stream.

The sidebar updates from the contiguous, completed prefix of background discovery. While scanning, its status states that results are still being collected. When discovery finishes, it shows the final set and video counts. A library with no matches gets an explicit empty state.

The reference is deliberately based on the user's current sort order rather than a hidden quality heuristic. Sorting by largest file, filename, or creation date therefore makes the corresponding first member the reference. This is predictable and leaves a future Auto Mark policy explicit.

When the reference and selected member were not a direct matching edge, the UI may still compare their previews and metadata because both belong to the same linked component. It must not describe that relationship as a direct match. Pair-specific Ignore is only meaningful for an actual discovered edge.

## Layout

```text
+----------------------+--------------------------------------------------+
| Duplicate sets       | Reference               Selected member          |
| [thumb] Set 1        | [large preview]          [large preview]          |
|         3 videos     | metadata                 metadata                 |
|         6.8 GB       |                                                  |
| [thumb] Set 2        | Members: [Reference] [2] [3] ...                 |
|         2 videos     |          Prev / Next, existing actions           |
| ...                  |                                                  |
| 4 sets · 11 videos   |                                                  |
+----------------------+--------------------------------------------------+
```

The application remains visually native to Qt/macOS. The first version should favor clear selection states, readable thumbnails, and compact labels over custom decoration.

## Architecture

### Duplicate set builder

A small UI-independent helper consumes the video count and discovered `MatchedVideoPair` edges and returns deterministic components. A union-find implementation keeps the work close to linear in the number of videos and matching edges. Tests cover chains, triangles, disjoint sets, duplicate edges, and stable ordering.

### Background discovery

`BackgroundMatchDiscovery` remains the authority for the asynchronous scan. It exposes a read-only snapshot containing only matches in the safe contiguous prefix and a completion state derived from `preScannedEnd == maxPosition`. Out-of-order chunks beyond that prefix remain hidden, preserving the progress guarantee already used by pair navigation.

### Comparison dialog

`Comparison` owns only derived set/member indexes for the current discovery generation. It rebuilds the sidebar from the safe match snapshot, preserves the selected set when possible, and sends an explicit selected pair to the existing display method. Existing file safety checks remain in `deleteVideo`; this PR does not create a second deletion path.

## Safety and edge cases

- Deleted, missing, protected, or ignored videos/pairs must not silently bypass existing safeguards.
- Ignoring an edge can split a connected component, so sets are rebuilt from eligible edges rather than mutated in place.
- Deleting a video removes it from the next rebuild; a component that falls to one member disappears.
- Sort and matching-setting changes restart discovery and clear derived set state before accepting new results.
- A low-confidence bridge can create a larger transitive component. The UI calls these linked sets and retains pair-specific evidence instead of implying clique semantics.
- Large libraries should not perform database reads for every paint. Set rebuilding is tied to discovery progress or explicit state changes, not widget rendering.

## Validation

Automated coverage:

- duplicate-set builder: pair, chain, triangle, disjoint components, duplicate edges, and deterministic ordering;
- background discovery: safe-prefix match snapshots and completed scan state;
- existing comparison and cleanup regression tests.

Manual acceptance:

1. Open a scan containing at least one two-video set and one three-or-more-video set.
2. Confirm the sidebar counts and representative thumbnails.
3. Select a set and click every member; the reference remains stable and the right preview/metadata changes.
4. Use Previous and Next to cycle only within that set.
5. Ignore a direct pair and confirm the affected set updates or splits.
6. Trash a member and confirm it disappears while the existing trash destination and confirmations are respected.
7. Change sort order, threshold, and pHash/SSIM mode; stale sets must clear and the new scan must repopulate them.
8. Verify useful loading and empty states while discovery is running and when no matches exist.

## Follow-up direction: staged cleanup

Once group review is trusted, the next coherent change is staged cleanup:

- mark individual members for removal without deleting immediately;
- allow selecting multiple sets;
- show marked count and reclaimable size in the sidebar/gallery;
- offer explicit, ordered Auto Mark rules that choose what to keep per set;
- preview every rule result with a reason;
- execute one confirmed batch through the existing protected-folder, Apple Photos, custom-trash, and deletion safeguards.

Auto Mark must remain a proposal layer. It should never perform deletion as a side effect of applying rules.
