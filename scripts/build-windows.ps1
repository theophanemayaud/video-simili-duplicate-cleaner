param(
    [Parameter()]
    [ValidateSet("x64", "x86", "arm64")]
    [string]$Architecture = "x64",
    
    [Parameter()]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    
    [Parameter()]
    [switch]$Clean,
    
    [Parameter()]
    [switch]$Rebuild
)

Write-Host "Building Video Simili Duplicate Cleaner for Windows..." -ForegroundColor Green
Write-Host "Architecture: $Architecture" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor Cyan

# Set up paths
$rootDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $rootDir "build-$Architecture"
$sourceDir = Join-Path $rootDir "QtProject"

# Clean if requested
if ($Clean -or $Rebuild) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path $buildDir) {
        Remove-Item -Path $buildDir -Recurse -Force
    }
}

# Check if vcpkg is set up
if (-not $env:VCPKG_ROOT) {
    Write-Warning "VCPKG_ROOT not set. Make sure vcpkg is installed and dependencies are available."
}

# Configure cmake
Write-Host "Configuring cmake..." -ForegroundColor Yellow
$cmakeArgs = @(
    "-B", $buildDir
    "-S", $sourceDir
    "-A", $Architecture
)

if ($env:VCPKG_ROOT) {
    $toolchainPath = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainPath"
}

try {
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configuration failed"
    }
    
    # Build
    Write-Host "Building..." -ForegroundColor Yellow
    & cmake --build $buildDir --config $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
    
    Write-Host "Build completed successfully!" -ForegroundColor Green
    Write-Host "Output location: $buildDir\$Configuration\" -ForegroundColor Cyan
}
catch {
    Write-Error "Build failed: $_"
    exit 1
}