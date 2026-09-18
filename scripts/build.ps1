# scripts/build.ps1
# Build script for Windows

param(
    [switch]$Tests,
    [switch]$Benchmarks,
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Reactive Framework Build ===" -ForegroundColor Cyan
Write-Host "Config: $Config"
Write-Host "Tests: $($Tests.IsPresent)"
Write-Host "Benchmarks: $($Benchmarks.IsPresent)"

# Clean
if (Test-Path build) {
    Write-Host "Cleaning build/" -ForegroundColor Yellow
    Remove-Item -Recurse -Force build
}

# Configure
$cmakeArgs = @("-B", "build", "-DCMAKE_BUILD_TYPE=$Config")
if ($Tests)      { $cmakeArgs += "-DBUILD_TESTS=ON" }
if ($Benchmarks) { $cmakeArgs += "-DBUILD_BENCHMARKS=ON" }

Write-Host "`n=== CMake Configure ===" -ForegroundColor Cyan
cmake @cmakeArgs

# Build
Write-Host "`n=== CMake Build ===" -ForegroundColor Cyan
cmake --build build --config $Config

Write-Host "`n=== Done ===" -ForegroundColor Green