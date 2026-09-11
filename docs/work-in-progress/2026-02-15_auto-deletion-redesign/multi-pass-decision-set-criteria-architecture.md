# Multi-Pass Decision Set Criteria Architecture (WIP)

Date: 2026-02-15
Status: Draft architecture proposal
Scope: Auto deletion criteria engine, presets, saved decision sets, and pass-by-pass execution

## Why this architecture

The desired behavior is not just "one mode picks one winner". It is:

- guardrails ("only apply if...")
- ordered preferences ("keep best/worst by...")
- reusable named decision sets
- sequential passes from strict to loose
- user control between passes

This needs a first-class model for **conditions**, **comparators**, and **execution plans**.

## Core Design Principles

- **Safety first**: no deletion unless a decision is explicitly reached.
- **Composability**: combine multiple conditions and tie-breakers.
- **Determinism**: same inputs, same outcome.
- **Auditability**: every auto decision should explain which rule/pass triggered it.
- **Progressive strictness**: run strict sets first, looser sets later.

## Conceptual model

### 1) DecisionSet

A `DecisionSet` is a reusable policy unit, for one pass:

- name and description
- enabled flag
- list of applicability conditions (AND/OR expression)
- ordered decision criteria (first decisive criterion wins)
- tie/no-decision policy
- confirmation policy override (optional)

### 2) RunPlan

A `RunPlan` is an ordered list of `DecisionSet` IDs to execute in sequence:

- plan name (for example "Conservative cleanup")
- decision set order (strict -> loose)
- stop/continue strategy between passes
- dry-run and approval settings

### 3) Criterion kinds

There are two fundamentally different criterion families:

- **Applicability conditions**: "Only apply if X"
- **Decision criteria**: "Keep best/worst by Y"

This clean separation is critical for clarity and future extensibility.

## Criteria primitives

### A) Applicability conditions

Each condition checks one parameter (or relation) and returns `true`/`false`.

Supported operators (initial target):

- `equal`
- `not_equal`
- `greater_than`
- `greater_or_equal`
- `less_than`
- `less_or_equal`
- `within_delta` (for tolerant equality, for example duration)
- `exists` / `missing` (for optional metadata such as GPS/audio)
- `both_exist`

Condition scopes:

- **pair-relation** (compare left vs right), for example:
  - resolution equal
  - timestamps within 3 seconds
- **single-side predicate** (must hold for each side), for example:
  - file size above threshold
  - both have GPS

### B) Decision criteria

Each decision criterion compares left and right and returns:

- `delete_left`
- `delete_right`
- `no_decision` (equal or incomparable)

Initial decision fields:

- file size
- resolution (pixel count)
- created timestamp
- modified timestamp
- bitrate
- framerate
- audio bitrate / audio presence (as available)
- GPS presence

Direction:

- `keep_best` (delete worse)
- `keep_worst` (delete better) for intentionally space-saving modes

Field-specific "best" definition is explicit in code (for example larger file size = better, earlier date can be configured).

## Evaluation semantics

Given a matched pair `(left, right)` and one `DecisionSet`:

1. Evaluate applicability expression:
   - if false: skip pair for this set.
2. Evaluate decision criteria in order:
   - first decisive criterion wins.
3. If all return `no_decision`:
   - apply tie/no-decision policy (default: skip pair).
4. If decision found:
   - execute deletion through existing safe path (`deleteVideo`).

### Tie/no-decision policies (recommended)

- default: `skip_pair` (safest)
- optional advanced:
  - `keep_left`
  - `keep_right`
  - `ask_user`

Default should remain `skip_pair` for safety.

## Logical expression model for conditions

To support "only apply if X and Y are equal for A" and similar logic, conditions should support grouping:

- AND groups
- OR groups
- optional NOT wrapper (future)

Practical representation:

- expression tree (`ConditionNode`) with group/operator leaves
- or normalized groups: list of AND groups joined by OR

Recommendation: start with normalized OR-of-AND groups (simpler UI and serialization).

## Data schema (JSON-serializable)

Suggested persisted schema (high-level):

- `decision_sets`: array of decision set objects
- `run_plans`: array of run plan objects
- `default_run_plan_id`

Decision set object:

- `id`
- `name`
- `description`
- `enabled`
- `applicability_groups` (OR-of-AND conditions)
- `decision_chain` (ordered criteria)
- `no_decision_policy`
- `safety_overrides` (optional)

Condition object:

- `field`
- `operator`
- `value` (typed)
- `delta` (for tolerant operators)
- `missing_value_policy`

Decision criterion object:

- `field`
- `direction` (`keep_best` / `keep_worst` or explicit keep higher/lower/earlier/later)
- `missing_value_policy` (`no_decision`, `treat_missing_as_worst`, etc.)

## C++ architecture proposal

### New domain types

- `AutoCriterionField`
- `AutoOperator`
- `AutoDirection`
- `AutoCondition`
- `AutoDecisionCriterion`
- `AutoDecisionSet`
- `AutoRunPlan`
- `AutoDecisionOutcome`

### New engine components

- `AutoCriteriaEvaluator`
  - evaluates conditions and decision criteria on `VideoMetadata`
- `AutoDecisionSetExecutor`
  - executes one decision set over matched pairs
- `AutoRunPlanExecutor`
  - runs multiple decision sets pass-by-pass
- `AutoDecisionAudit`
  - captures outcome details for logs/review UI

### Integration points

- keep pair iteration and safety path in `Comparison` (existing behavior)
- delegate decisioning to new engine
- keep existing buttons as wrappers:
  - each legacy button loads a predefined decision set or run plan

## Multi-pass execution model

Recommended pass lifecycle:

1. select run plan
2. optional dry-run preview for pass N
3. execute pass N
4. show summary:
   - deleted count
   - skipped count
   - top reasons
5. user chooses:
   - continue to next pass
   - stop
   - edit next pass settings

This preserves control while allowing iterative cleanup.

## Save/load model

### Persisted entities

- reusable `DecisionSet` library
- reusable `RunPlan` library
- last-used decision set and plan

### Storage strategy

Use QSettings with JSON payload strings:

- `auto_decision_sets_v1`
- `auto_run_plans_v1`
- `auto_default_run_plan_id`

Version each schema key (`_v1`) to allow migration later.

## Example decision sets (matching requested scenarios)

### Set A: Strict identical metadata, decide by file size

Applicability (AND):

- duration within 1s
- resolution equal
- framerate within 0.1
- codec equal
- audio equal
- GPS equal

Decision chain:

1. keep larger file size

No-decision:

- skip pair

### Set B: Prefer GPS when otherwise equivalent

Applicability (AND):

- all metadata except GPS equal
- smaller file is not strictly better by size policy

Decision chain:

1. keep with GPS presence
2. keep larger file size

No-decision:

- skip pair

### Set C: Final cleanup (looser)

Applicability (AND):

- both have audio metadata
- timestamps within tolerance

Decision chain:

1. keep higher resolution
2. keep higher audio bitrate
3. keep larger file size

No-decision:

- skip pair

## UI architecture proposal

### Main Auto UI sections

- Decision Set editor:
  - conditions builder (groups)
  - ordered decision chain
- Run Plan editor:
  - list of decision sets per pass with ordering
- Execution controls:
  - dry run
  - run current pass
  - run full plan with pause-between-passes

### Usability safeguards

- preset templates for common safe setups
- read-only "reason preview" string for each rule
- warn on overly broad conditions (for example no applicability conditions)

## Auditing and explainability

For each deletion, record:

- run plan ID
- pass number
- decision set ID
- first decisive criterion
- values compared

Expose this in status log and optional review table. This increases trust and helps debugging.

## Risk controls

- default to dry-run for newly created decision sets
- require confirmation before starting pass 1
- optional "max deletions per pass" safety cap
- keep locked-folder and Apple Photos protections unchanged

## Migration / backward compatibility

Map legacy modes to built-in decision sets:

- `Auto trash identical` -> strict-identical-by-size set
- `Auto trash by size` -> size-priority set
- `Auto trash by date` -> date-priority set

Legacy buttons can remain as aliases to preserve existing user habits.

## Suggested rollout

### Phase 1: Engine foundation

- implement condition + decision criterion evaluator
- port existing 3 modes as built-in decision sets
- parity tests

### Phase 2: Decision set persistence + editor

- create/edit/save/load custom sets
- dry run and per-set execution

### Phase 3: Run plans (multi-pass)

- create ordered pass plans
- pass-by-pass summaries and continue/stop control

### Phase 4: Advanced criteria

- richer threshold operators
- missing-value policy per criterion
- optional scoring/ranking fallback

## Final recommendation

Adopt a **DecisionSet + RunPlan** architecture with strict separation of:

- applicability conditions ("only apply if...")
- ordered decision chain ("keep by...")

This directly supports the examples requested, enables safe iterative cleanup, and scales to future threshold logic without adding new hardcoded auto-delete modes.
