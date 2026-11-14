# macOS sandbox access

Run this check in the signed release app with its sandbox entitlements and local test receipt. Headless tests and files inside the app container do not exercise access granted through the native folder picker.

1. Select the original external corpus's `Videos` directory through the native folder picker. Keep the corpus outside `~/Library/Containers/`.
2. Use `CutEnds`, pHash at 100%, and disable rotated-copy detection. Scan with `Use cache: No`.
3. Preserve any existing cache and select a fresh cache location. Scan with `Yes`, then `Only`. Each scan must match the valid-video and matching-video expectations in [`tst_whole_app_scan.cpp`](../external/whole_app_scan/tst_whole_app_scan.cpp).
4. Open a comparison, navigate to another pair, and zoom a preview to exercise captures requested after the scan.
5. Quit and restart the app. Verify that the selected folders are restored, then repeat the scan without opening the folder picker or receiving another access request.
6. Select another authorized folder and quit before scanning. Restart and verify that the new selection is restored. Clear the selection, quit, and check that it remains empty after restart.

Keep each selected directory's native security-scoped URL authorized until the main window closes. Qt's file engine can release its own access between operations; FFmpeg opens and seeks files through native I/O independently of Qt. Verify that repeated scans and comparison previews reuse the session's directory grant.

The regression can find all videos but reject many during capture. A cached failure from an affected run must not be mistaken for a failed extraction in the corrected build; preserve that cache and use a fresh one for the comparison.
