# Test Video Simplified

## Overview

This is a simplified, fast, and repo-tracked video test suite that uses the two sample videos in `samples/videos/` to verify video scanning and thumbnail generation produces consistent results.

## Test Structure

### Data-Driven Testing

The test uses Qt Test's data-driven testing framework to create a **test matrix** with:
- **Videos**: 2 (Nice_383p_500kbps.mp4, Nice_720p_1000kbps.mp4)
- **Cache modes**: 3 (NO_CACHE, WITH_CACHE, CACHE_ONLY)
- **SSIM thresholds**: 7 (100%, 99%, 98%, 97%, 96%, 95%, 90%)

**Total test cases**: 42 combinations

Each test case:
1. Scans the video with specified cache mode
2. Compares metadata against reference CSV
3. Compares thumbnail against reference using SSIM

## Expected Behavior

### NO_CACHE Mode
All thresholds from 90%-100% should **PASS**. NO_CACHE mode produces deterministic, bit-identical results with SSIM = 1.0 (100% similarity).

### WITH_CACHE Mode
- **100%, 99%, 98% thresholds**: Expected to **FAIL** 
  - Cache stores thumbnails at lower JPEG quality (60 instead of original)
  - Typical SSIM: ~97.5%
- **97%, 96%, 95%, 90% thresholds**: Should **PASS**
  - These thresholds accommodate expected cache quality degradation

### CACHE_ONLY Mode
Behaves similarly to WITH_CACHE mode (same quality degradation).

## Regenerating Reference Data

If the video processing algorithm changes or you need to update reference data:

1. Build the generator:
   ```bash
   cd QtProject/builds/build-debug-6.9.3-macos
   cmake --build . --target generate_reference_data
   ```

2. Run it from project root:
   ```bash
   cd /Users/theophanemayaud/Dev/video-simili-duplicate-cleaner
   ./QtProject/builds/build-debug-6.9.3-macos/tests/generate_reference_data
   ```

3. Commit the updated reference data:
   ```bash
   git add QtProject/tests/test_video_simplified/reference_data/
   git commit -m "Update test_video_simplified reference data"
   ```

## Running Tests

### Via CTest
```bash
cd QtProject/builds/build-debug-6.9.3-macos
ctest -R test_video_simplified
```

### Directly
```bash
./QtProject/builds/build-debug-6.9.3-macos/tests/test_video_simplified
```

### Run specific test combinations
```bash
# Run only NO_CACHE tests
./QtProject/builds/build-debug-6.9.3-macos/tests/test_video_simplified "nocache"

# Run only one video's tests
./QtProject/builds/build-debug-6.9.3-macos/tests/test_video_simplified "Nice_383p"
```

## Reference Data Format

### Metadata Format (Text files)
Simple key-value format with one property per line (stored as `.txt` files). This format is easy to read, edit, and diff in version control.

### Thumbnail Format
JPEG images stored as `.jpg` files, same naming as video files.

