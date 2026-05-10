# Video Similarity Duplicate Cleaner

The goal of this project is to help users find duplicate or similar videos by comparing video content, then cleanup duplicates safely by making it easy to compare details, with also some automated cleanup options.

## Working Principles

- Keep it simple: one clear approach, fail fast, and avoid overly defensive fallback logic.
- No backwards compatibility is needed for in-progress changes: fully migrate old approaches instead of layering shims.
- Capture why important functionality exists, or why certain decisions (code, feature, architecture, etc.) were made, not only what it does, so future refactors can preserve intent.
- Design in a way that can be properly tested. Prefer tests that are more representative of actual use cases and thus end to end. These add value and confidence, even if they need to be refactored over time. Use lower level unit tests more for temporary testing during implementation or more complex functionality, but it's ok to delete them quickly once they are no longer relevant.

## Project Shape

This is a C++ desktop app built with CMake and relying on static libraries like Qt 6 Widgets for the UI, FFmpeg to read video metadata and extract frames, and OpenCV to support perceptual comparisons.

- `QtProject/app`: main application code. `MainWindow` handles scanning/progress, `Video` extracts metadata/thumbnails/hashes, `Comparison` reviews matches and cleanup actions, `Db` owns cache persistence.
- `QtProject/app/*.ui`: Qt Designer forms with auto-connected `on_<object>_<signal>` slots. Keep UI changes consistent with the generated `ui_*.h` flow.
- `QtProject/tests`: Qt Test executables. `test_auto_delete` covers focused end-to-end cleanup behavior using checked-in sample videos and a temporary custom trash folder; `test_video_simplified` uses checked-in sample videos for metadata/thumbnail checks; `test_video` mixes useful local-fixture checks with slow/external suites, so run only explicit functions by default.
- `samples/videos`: small representative fixtures for video-processing tests. Avoid replacing binary fixtures unless needed for the test intent.
- `DEPENDENCIES.md` and `DEPLOY.md`: source of truth for dependency and packaging workflows.

## Common Commands

The main development platform is macOS; keep default agent commands on this path.

- Configure: `cmake -S QtProject --preset debug-6.10.1-macos`
- Build: `cmake --build QtProject/builds/build-debug-6.10.1-macos`
- Run focused auto-delete end-to-end tests after changing cleanup behavior or auto cleanup UI:
  `cmake --build QtProject/builds/build-debug-6.10.1-macos --target test_auto_delete && ctest --test-dir QtProject/builds/build-debug-6.10.1-macos -C Debug --output-on-failure -R ^test_auto_delete$`
- Run the self-contained CTest baseline:
  `ctest --test-dir QtProject/builds/build-debug-6.10.1-macos -C Debug --output-on-failure -R "^(test_comparison|test_mainwindow|test_auto_delete)$"`
- Green `test_video` function cases for regular work: `emptyDb`, `test_whole_app_nocache`, `test_whole_app_cached`, `test_whole_app_cache_only`.
- `test_video_simplified` uses checked-in sample videos but compares platform-sensitive thumbnails/hashes; run it on the macOS dev setup, not Linux CI, unless refreshing reference expectations.
- `test_video` whole-app and reference-detail cases depend on an external local fixture folder; run explicit functions only when that folder is available. Reference-detail cases (`test_check_refvidparams_nocache`, `test_check_refvidparams_withcache`, `test_check_refvidparams_withCacheOnly`) are not all green currently; run them when touching metadata, thumbnails, cache behavior, or reference data.
- To investigate one `test_video` fixture, run a single data row with `test_function:data_tag`, for example `test_check_refvidparams_nocache:20150727_115225.mp4`.
- Prefer targeted `ctest --test-dir QtProject/builds/build-debug-6.10.1-macos ...` invocations when using CTest on macOS; the test CMake config chooses a platform plugin compatible with the current Qt build.
- Do not run full `test_video` unless explicitly requested; it includes active `test_100GB*` cases requiring an extra mounted folder.
- Package macOS binaries: `npm run binaries`
- Rebuild vendored macOS deps only when needed: `npm run qt-macos`, `npm run ffmpeg-macos`, `npm run opencv-macos`
