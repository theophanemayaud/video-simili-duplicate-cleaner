# macOS sandbox access

Run this check in the signed release app with its sandbox entitlements and local test receipt. Headless tests and files inside the app container do not exercise access granted through the native folder picker.

1. Select the original external corpus's `Videos` directory through the native folder picker. Keep the corpus outside `~/Library/Containers/`.
2. Use `CutEnds`, pHash at 100%, and disable rotated-copy detection. Scan with `Use cache: No`.
3. Preserve any existing cache and select a fresh cache location. Scan with `Yes`, then `Only`. Each scan must match the valid-video and matching-video expectations in [`tst_whole_app_scan.cpp`](../external/whole_app_scan/tst_whole_app_scan.cpp).
4. Open a comparison, navigate to another pair, and zoom a preview to exercise captures requested after the scan.
5. Restart the app and repeat the scan without selecting the folder again, checking that the persisted folder permission works.

Keep native file access authorized throughout each FFmpeg capture. Qt's security-scoped file engine releases access when its owning file object is destroyed; FFmpeg opens and seeks files through native I/O independently of Qt. A temporary `QFileInfo` during metadata extraction does not keep that access alive for later captures.

The regression can find all videos but reject many during capture. A cached failure from an affected run must not be mistaken for a failed extraction in the corrected build; preserve that cache and use a fresh one for the comparison.
