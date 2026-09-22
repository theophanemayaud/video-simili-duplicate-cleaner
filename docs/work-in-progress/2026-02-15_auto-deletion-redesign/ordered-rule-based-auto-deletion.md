# Ordered Rule-Based Auto Deletion (WIP)

Date: 2026-02-15
Status: Draft proposal
Scope: Comparison window auto-deletion logic and UI

## Context

Current auto deletion in the comparison window offers three fixed modes:

- `Auto trash identical`
- `Auto trash by size`
- `Auto trash by date`

These are useful but rigid. Users cannot express ordered priorities such as:

- keep highest file size, then if tied keep earliest created date
- prefer videos with GPS metadata, then prefer higher resolution
- keep lowest size for a space-saving strategy

The existing code also already hints at a refactor path: one mode (`date`) uses `AutoDeleteConfig`, while the others still use duplicated inline logic.

## Current State (Code Survey)

### Existing behavior and where it lives

- Main auto-deletion handlers are in `QtProject/app/comparison.cpp`.
- Three mode entry points:
  - `on_identicalFilesAutoTrash_clicked()`
  - `on_autoDelOnlySizeDiffersButton_clicked()`
  - `on_pushButton_onlyTimeDiffersAutoTrash_clicked()` (delegates to `autoDeleteLoopthrough`)
- Shared date-mode decision logic is in `Comparison::AutoDeleteConfig::videoToDelete(...)`.
- Auto-tab UI controls are in `QtProject/app/comparison.ui`.

### Metadata already available

From `QtProject/app/videometadata.h` and conversion in `Video::videoToMetadata(...)`:

- file size
- creation timestamp
- modified timestamp
- width/height
- GPS coordinates
- bitrate, framerate, codec, audio, duration
- additional metadata map

This is enough to support the requested first criteria set without adding new extraction logic.

## Problem Statement

The current mode-based implementation has these limitations:

- fixed and non-composable decision logic
- duplicate comparison code across modes
- limited extensibility for future threshold rules
- no ordered multi-criteria decision chain

Result: adding each new strategy requires more special-case code and more UI buttons.

## Goals

- Replace fixed-mode decision logic with ordered, user-defined criteria.
- Preserve current safety behavior (locked folders, Apple Photos handling, confirmations).
- Keep sort-order independence.
- Provide backward compatibility through presets equivalent to existing modes.
- Prepare for future threshold criteria (for example, only consider resolution > X).

## Non-Goals (Initial Delivery)

- No machine-learning quality scoring.
- No cross-pair global optimization (keep this pairwise as today).
- No change to actual delete backend behavior (`deleteVideo` stays authoritative).

## Options Considered

### Option A - Add more fixed modes

Add new hardcoded buttons and handlers for each new strategy.

- Pros: quick for one-off additions
- Cons: code duplication grows; still cannot compose/ordering; poor UX scalability

### Option B - Rule engine without UI (config only)

Implement internal rule chain and expose only presets at first.

- Pros: low UI risk, good technical foundation
- Cons: delayed user value for custom ordering

### Option C - Rule engine + simple ordered-rule UI (recommended)

Implement a reusable rule engine and expose editable ordered criteria in Auto tab.

- Pros: solves extensibility and UX need now; aligns with future thresholds
- Cons: moderate UI and persistence work

## Recommended Direction

Adopt **Option C** in phases, with an MVP that keeps UI changes small:

1. internal rule engine + preset mapping
2. basic ordered list editor (add/remove/up/down + keep direction per rule)
3. threshold rule type in a later phase

## Feature Specification

### 1) Rule Types

Define two rule kinds:

- **Decision rule**: can choose which side to delete when values differ.
- **Filter rule** (future-ready): can block auto-delete unless condition passes.

For v1 delivery, decision rules are mandatory; filter rules can be optional but model should allow them.

### 2) Built-In Decision Criteria (v1)

- file size: keep highest or keep lowest
- modified timestamp: keep earliest or keep latest
- created timestamp: keep earliest or keep latest
- GPS metadata presence: keep with GPS or keep without GPS
- resolution (width * height): keep higher or keep lower

### 3) Future Threshold Criteria (v2+)

Supported by data model but initially hidden/disabled in UI:

- only if resolution >= X
- only if file size >= Y
- only if has GPS

### 4) Ordered Evaluation Semantics

Given a matched pair `(left, right)`:

1. run global guards (exists, not already trashed, optional filename containment, etc.)
2. evaluate rules in user-defined order
3. each decision rule returns one of:
   - delete left
   - delete right
   - no decision (equal values or missing data according to policy)
4. first decisive rule wins
5. if no rule decides, do not auto-delete this pair

Default policy for missing values: `no decision` (safe by default).

### 5) Presets / Backward Compatibility

Ship presets that map current behavior:

- **Identical**: strict equality gate + fallback deterministic side choice
- **By Size**: duration/resolution/fps compatibility gate + file size descending preference
- **By Date**: all-equal-except-date gate + created/modified date preference

This keeps existing workflows and avoids immediate breaking change.

### 6) UX (Auto Tab)

Proposed Auto tab update:

- ordered criteria list (top = highest priority)
- buttons: add, remove, move up, move down
- per-rule preference selector (highest/lowest, earliest/latest, with/without)
- preset selector: Identical / Keep biggest / Keep by date / Custom
- keep existing safety toggles:
  - names must be contained
  - disable delete confirmation

Threshold rule inputs can be displayed later behind an "Advanced" section.

### 7) Persistence

Store ordered rules in settings (QSettings), using a serializable format (JSON string is sufficient).

Recommended keys:

- `auto_delete_rules` (serialized ordered list)
- `auto_delete_preset` (selected preset name/id)
- optional: `auto_delete_thresholds` (future)

If no custom rules exist, load a default preset equivalent to current date mode or current app default.

## Technical Design

### Core model (new)

Create small reusable types, for example in new files:

- `QtProject/app/autodeleterules.h`
- `QtProject/app/autodeleterules.cpp`

Suggested structures:

- `enum class AutoDeleteCriterion`
- `enum class AutoDeletePreference`
- `struct AutoDeleteRule` (criterion, preference, enabled, optional params)
- `class AutoDeleteRuleEngine` with `decide(leftMeta, rightMeta, userSettings)`

### Comparison integration

Refactor `Comparison::autoDeleteLoopthrough(...)` to delegate pair decision to rule engine.

- keep deletion execution via existing `deleteVideo(...)`
- keep existing confirmation flow
- keep progress and UI updates
- keep locked-folder and Apple Photos behavior untouched

### Existing code migration steps

1. migrate date mode into generic rule path (already close)
2. port size/identical mode checks into reusable rule/preset definitions
3. remove duplicated condition blocks from click handlers
4. keep legacy buttons temporarily as wrappers that load presets and call shared runner

## Implementation Plan

### Phase 1 - Extract and unify internals (no UI redesign)

- Add rule model + engine classes.
- Extend `AutoDeleteConfig` or replace with richer config object.
- Route all three existing mode buttons to one shared `autoDeleteLoopthrough` path.
- Verify behavioral parity with existing three modes.

Deliverable: technical refactor with no visible UX regression.

### Phase 2 - Add ordered custom rules in UI

- Add list editor controls to `comparison.ui`.
- Add load/save of custom rules in `Prefs`/QSettings.
- Add "Custom ordered rules" action/button.
- Keep legacy preset buttons for discoverability.

Deliverable: users can create and order criteria.

### Phase 3 - Preset polish and migration

- Explicit preset dropdown + reset-to-preset action.
- Migrate legacy behavior labels to presets.
- Optional: deprecate old separate mode sections after validation.

Deliverable: clean UX combining presets and custom mode.

### Phase 4 - Threshold criteria (future)

- Extend rule schema with threshold operators and typed values.
- Add advanced editor controls for threshold input.
- Add guardrail validation (invalid thresholds disable auto-run with clear message).

Deliverable: hybrid ordered-decision + threshold filtering.

## Testing Plan

### Unit tests

Extend `QtProject/tests/test_comparison/tst_test_comparison.cpp` and add new rule-engine-focused tests:

- each criterion directionality (higher/lower, earlier/later, with/without)
- ordered tie-break behavior
- no-decision behavior when values equal or missing
- preset parity tests for existing three modes

### Integration/manual checks

- run each preset on sample duplicate sets
- verify locked folders are still never deleted
- verify Apple Photos path still goes through album behavior
- verify stop/continue/disable confirmation dialogs still work
- verify behavior is independent of current sort order

## Risks and Mitigations

- **Behavior drift from current modes**
  Mitigation: parity tests and legacy preset wrappers during migration.

- **UI complexity growth**
  Mitigation: phased rollout, start with minimal list editor and presets.

- **Ambiguous or missing metadata (for example GPS absent)**
  Mitigation: safe default `no decision` and explicit future missing-data policy.

- **Performance concerns in large sets**
  Mitigation: metadata conversion already exists; avoid repeated expensive extraction.

## Open Questions

- Should custom auto rules be global app settings, or per comparison session?
- Should there be a configurable fallback when no rule decides (for example keep left/right), or always no delete?
- For GPS rule, should invalid/placeholder GPS values be treated as "without GPS"?
- Should thresholds apply as preconditions before any decision rule, or inline in ordered chain?

## Proposed Next Engineering Task Breakdown

1. Introduce `AutoDeleteRuleEngine` and parity tests for existing 3 modes.
2. Refactor all auto-delete buttons to shared rule execution path.
3. Add persisted custom ordered rules and basic Auto-tab editor.
4. Add threshold rule support after custom ordering stabilizes.
