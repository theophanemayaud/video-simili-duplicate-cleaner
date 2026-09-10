# Video Similarity Duplicate Cleaner

The goal of this project is to help users find duplicate or similar videos by comparing video content, then cleanup duplicates safely by making it easy to compare details, with also some automated cleanup options.

## Working Principles

- Keep it simple: one clear approach, fail fast, and avoid overly defensive fallback logic.
- No backwards compatibility is needed for in-progress changes: fully migrate old approaches instead of layering shims.
- Capture why important functionality exists, or why certain decisions (code, feature, architecture, etc.) were made, not only what it does, so future refactors can preserve intent.
- Design in a way that can be properly tested. Prefer tests that are more representative of actual use cases and thus end to end. These add value and confidence, even if they need to be refactored over time. Use lower level unit tests more for temporary testing during implementation or more complex functionality, but it's ok to delete them quickly once they are no longer relevant.

## Pull Requests

- Use Conventional Commit-style PR titles, such as `fix(comparison): restore slider after cancelled navigation`.
- Squash-merge pull requests into `master` and use the PR title as the resulting commit title.

## Project Shape

This is a C++ desktop app built with CMake and relying on static libraries like Qt 6 Widgets for the UI, FFmpeg to read video metadata and extract frames, and OpenCV to support perceptual comparisons.

- `QtProject/app`: main application code. `MainWindow` handles scanning/progress, `Video` extracts metadata/thumbnails/hashes, `Comparison` reviews matches and cleanup actions, `Db` owns cache persistence.
- `QtProject/app/*.ui`: Qt Designer forms with auto-connected `on_<object>_<signal>` slots. Keep UI changes consistent with the generated `ui_*.h` flow.
- `QtProject/tests`: `test_repo_*` targets are self-contained and use only checked-in fixtures under `QtProject/tests/repo`. Optional corpora live under `QtProject/tests/external` and use flat `test_external_*` target names. `test_repo_auto_delete` covers focused end-to-end cleanup; `test_repo_video_extraction_regression` snapshots the Nice videos; and `test_repo_video_matching` validates the tracked matching manifest.
- `samples/videos`: small representative fixtures for video-processing tests. Avoid replacing binary fixtures unless needed for the test intent.
- Keep repository-tracked video fixtures within a few MB total because every clone downloads them.
- Keep the out of repo, optional `~/Dev` video corpus lean as well; a few GB total is acceptable.
- `DEPENDENCIES.md` and `DEPLOY.md`: source of truth for dependency and packaging workflows.

## Common Commands

The main development platform is macOS; keep default agent commands on this path.

- Configure: `cmake -S QtProject --preset debug-6.10.1-macos`
- Build: `cmake --build QtProject/builds/build-debug-6.10.1-macos`
- Run focused auto-delete end-to-end tests after changing cleanup behavior or auto cleanup UI:
  `cmake --build QtProject/builds/build-debug-6.10.1-macos --target test_repo_auto_delete && ctest --test-dir QtProject/builds/build-debug-6.10.1-macos -C Debug --output-on-failure -R ^test_repo_auto_delete$`
- Run the self-contained CTest baseline:
  `ctest --test-dir QtProject/builds/build-debug-6.10.1-macos -C Debug --output-on-failure -R "^(test_comparison|test_mainwindow|test_failed_video_cache|test_repo_auto_delete|test_repo_video_matching)$"`
- Run `test_failed_video_cache` after changing which processing failures are treated as permanent versus retryable. It is self-contained and uses generated temporary files.
- CTest labels make the safe lanes explicit: `ctest --test-dir QtProject/builds/build-debug-6.10.1-macos -L repo-fixtures` runs tracked fixtures, while `-L external-fixtures` runs the optional `~/Dev` suites. `test_external_large_video_corpus` is separately labeled `external-corpus` and `requires-mounted-100gb`; run only its named functions.
- Green `test_external_whole_app_scan` function cases for regular external-corpus work: `emptyDb`, `test_whole_app_nocache`, `test_whole_app_cached`, `test_whole_app_cache_only`.
- `test_repo_video_extraction_regression` compares platform-sensitive metadata/thumbnails for the Nice videos; run it on the macOS dev setup, not Linux CI, unless refreshing reference expectations.
- `test_external_video_extraction_regression` compares every `~/Dev` video against its metadata and thumbnail references in each cache mode. Reference-detail cases (`test_check_refvidparams_nocache`, `test_check_refvidparams_withcache`, `test_check_refvidparams_withCacheOnly`) are not all green currently; run them when touching metadata, thumbnails, cache behavior, or reference data.
- `test_external_whole_app_scan` runs `~/Dev` end-to-end scans in each cache mode; run its explicit functions only when that corpus is available.
- To investigate one `~/Dev` extraction reference, run a single `test_external_video_extraction_regression` data row, for example `test_check_refvidparams_nocache:20150727_115225.mp4`.
- Prefer targeted `ctest --test-dir QtProject/builds/build-debug-6.10.1-macos ...` invocations when using CTest on macOS; the test CMake config chooses a platform plugin compatible with the current Qt build.
- Do not run `test_external_large_video_corpus` unless explicitly requested; its active functions require the mounted 100GB folder.
- Package macOS binaries: `npm run binaries`
- Rebuild vendored macOS deps only when needed: `npm run qt-macos`, `npm run ffmpeg-macos`, `npm run opencv-macos`

### Linux

Linux uses the same version pins as macOS (`package.json` `cpp-dependencies-macos`: Qt, OpenCV, FFmpeg, libaom). `./scripts/cloud-agent-install.sh` installs OS packages for the Qt xcb backend and then `qt.sh` / `opencv.sh` / `ffmpeg.sh`. Those scripts install into `$HOME/.local/video-simili-deps` (outside the git checkout so later checkouts do not wipe them) and skip work when the pinned version is already present. They do not configure or compile the app. `.cursor/environment.json` points Cloud Agents at that same script.

Use the `debug-linux` CMake preset (Ninja + `g++`, `CMAKE_PREFIX_PATH` / `PKG_CONFIG_PATH` pointing at those installs). On images where `/usr/bin/c++` is Clang, linking libstdc++ fails; the preset and the Linux Qt/OpenCV/FFmpeg scripts pin `gcc`/`g++` for that reason.

- Configure: `cmake -S QtProject --preset debug-linux`
- Build: `cmake --build QtProject/builds/build-debug-linux`
- Run the self-contained CTest baseline:
 `ctest --test-dir QtProject/builds/build-debug-linux --output-on-failure -R "^(test_comparison|test_mainwindow|test_failed_video_cache|test_repo_auto_delete|test_repo_video_matching)$"`
- Run the app: `QtProject/builds/build-debug-linux/video-simili-duplicate-cleaner`
- Do not gate Linux runs on `test_repo_video_extraction_regression`: its Nice-video metadata/thumbnail hashes are macOS-specific and mismatch on Linux by design (verify those on macOS). The `repo-fixtures` label includes it, so prefer the explicit baseline above on Linux.
- GitHub `linux-local-tests` still uses Ubuntu system packages and a one-off CMake build dir, not this preset.
