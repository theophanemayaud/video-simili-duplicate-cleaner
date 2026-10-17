---
name: prepare-store-release
description: Validate and prepare Windows/macOS Store release binaries, including local corpus tests and a desktop UI smoke test, before handing them over for submission.
---

# Prepare a Store release

Use the same candidate code and version for the checks and delivered artifacts. Read `AGENTS.md` for test commands and `DEPENDENCIES.md` / `DEPLOY.md` for builds and packaging. Preparation does not authorize uploading or submitting to either Store.

1. Require the candidate's GitHub `Tests` checks to pass. Manually dispatch `Windows package` on the candidate ref and require both architectures and the bundle to succeed; packaging stays manual.
2. Configure/build locally on macOS and run the self-contained baseline and macOS extraction regression from `AGENTS.md`. Also build and run the three `external-fixtures` suites against the local ~200-video corpus (currently 229 videos):

   ```sh
   cmake --build QtProject/builds/build-debug-6.10.1-macos --target test_external_video_extraction_regression test_external_whole_app_scan test_external_video_matching
   ctest --test-dir QtProject/builds/build-debug-6.10.1-macos -C Debug --output-on-failure -L external-fixtures
   ```

   The default corpus root is `~/Dev/Videos across all formats with duplicates of all kinds`; `VIDEO_SIMILI_EXTERNAL_FIXTURES` overrides it. Check that tests actually exercised the corpus, rather than skipping missing fixtures. Run the mounted 100GB suite only when explicitly requested; it is not a release prerequisite. For known reference failures, compare against the previous extraction code and report the differences; never regenerate references just to obtain green tests. Investigate new failures before declaring readiness.
3. Run `npm run binaries`. First check for an active app session and preserve existing sibling outputs as needed: the script kills the app and replaces those outputs. Verify the actual sibling `.app`, `.dmg`, and `.pkg`, including version, architectures, signing, and the local app's `_MASReceipt/receipt`. If signing blocks the script, report the exact blocker and any alternative used; do not silently substitute another build path or call it equivalent.
4. Use computer use to open that freshly packaged local app, select the external corpus's `Videos` folder in the UI, and run a scan. Confirm it finishes without a crash or blocking error, review the scan counts, then open and navigate a duplicate comparison. Capture the finished scan/comparison as evidence. Never delete or modify the real corpus during this smoke test; use copies for any cleanup test. For the current patch, also exercise its reported failure through the UI when practical. Automated tests alone do not replace this step.
5. Hand over the verified macOS Store PKG and unsigned Windows MSIXBundle with their source commit, versions, paths, and hashes. Keep local test receipts/signatures separate from submission artifacts. Use `store-release-notes` for Store wording. Put results, visual evidence, known failures, and release-specific instructions in the PR description, not committed release reports. State any missing checks explicitly; do not describe a partial run as fully validated. Submission remains with the user unless explicitly requested.
