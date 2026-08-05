set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# The short triplet name also keeps generated Windows linker paths below the
# legacy MAX_PATH limit.
# OpenCV's ARM64 cross-build can promote compiler-detected optional features
# into the required CPU baseline. Keep the baseline to mandatory NEON on
# Windows ARM; newer FP16/BF16/dot-product paths must stay optional and be
# selected only when the runtime reports that the hardware supports them.
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCPU_BASELINE=NEON"
)
