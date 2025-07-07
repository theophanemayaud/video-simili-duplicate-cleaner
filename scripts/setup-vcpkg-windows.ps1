param(
    [Parameter()]
    [ValidateSet("x64", "x86", "arm64")]
    [string]$Architecture = "x64",
    
    [Parameter()]
    [switch]$CrossCompile
)

Write-Host "Setting up vcpkg for Video Simili Duplicate Cleaner..." -ForegroundColor Green

# Check if vcpkg root is set
if (-not $env:VCPKG_ROOT) {
    Write-Error "VCPKG_ROOT environment variable not set."
    Write-Host "Please set VCPKG_ROOT to point to your vcpkg installation." -ForegroundColor Yellow
    Write-Host "Example: `$env:VCPKG_ROOT = 'C:\vcpkg'" -ForegroundColor Yellow
    exit 1
}

Write-Host "Using vcpkg from: $env:VCPKG_ROOT" -ForegroundColor Cyan

# Check if vcpkg exists
$vcpkgPath = Join-Path $env:VCPKG_ROOT "vcpkg.exe"
if (-not (Test-Path $vcpkgPath)) {
    Write-Error "vcpkg.exe not found in $env:VCPKG_ROOT"
    Write-Host "Please run bootstrap-vcpkg.bat in your vcpkg directory first." -ForegroundColor Yellow
    exit 1
}

# Copy custom triplets
Write-Host "Copying custom triplets..." -ForegroundColor Cyan
$communityTripletsDir = Join-Path $env:VCPKG_ROOT "triplets\community"
if (-not (Test-Path $communityTripletsDir)) {
    New-Item -Path $communityTripletsDir -ItemType Directory -Force
}

$scriptDir = Split-Path -Parent $PSScriptRoot
$tripletsDir = Join-Path $scriptDir "triplets"
Copy-Item -Path "$tripletsDir\*.cmake" -Destination $communityTripletsDir -Force

Write-Host "Building for architecture: $Architecture" -ForegroundColor Cyan

# Set triplet based on architecture
$triplet = switch ($Architecture) {
    "arm64" { "arm64-windows-static" }
    "x86" { "x86-windows-static" }
    default { "x64-windows-static" }
}

Write-Host "Using triplet: $triplet" -ForegroundColor Cyan

# Install dependencies
try {
    Write-Host "Installing ffmpeg..." -ForegroundColor Yellow
    & $vcpkgPath install "ffmpeg[avcodec,avformat,avutil,swresample,swscale]:$triplet"
    
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install ffmpeg"
    }
    
    Write-Host "Installing opencv4..." -ForegroundColor Yellow
    & $vcpkgPath install "opencv4[core,imgproc]:$triplet"
    
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install opencv4"
    }
    
    Write-Host "Dependencies installed successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "To build the project, run:" -ForegroundColor Cyan
    Write-Host "cmake -B build -S QtProject -A $Architecture" -ForegroundColor White
    Write-Host "cmake --build build --config Release" -ForegroundColor White
    
    if ($CrossCompile) {
        Write-Host ""
        Write-Host "Cross-compilation setup complete!" -ForegroundColor Green
        Write-Host "You can now build Windows $Architecture binaries from your current platform." -ForegroundColor Cyan
    }
}
catch {
    Write-Error "Failed to install dependencies: $_"
    exit 1
}