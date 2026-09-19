# scripts/run_tests.ps1
# Chạy tất cả unit tests (Windows)

param(
    [switch]$Verbose,
    [switch]$Debug
)

$ErrorActionPreference = "Stop"

# Di chuyển về thư mục gốc dự án
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Kiểm tra thư mục build có tồn tại không
if (-not (Test-Path "build")) {
    Write-Host "Loi: Thu muc 'build' khong ton tai!" -ForegroundColor Red
    Write-Host "Chay truoc: .\scripts\build.ps1 -Tests" -ForegroundColor Yellow
    exit 1
}

# Kiểm tra CTestTestfile.cmake có tồn tại không
$ctestFile = Join-Path $root "build\CTestTestfile.cmake"
if (-not (Test-Path $ctestFile)) {
    Write-Host "Loi: Khong tim thay CTestTestfile.cmake trong build\" -ForegroundColor Red
    Write-Host "Co the ban chua build voi -DBUILD_TESTS=ON" -ForegroundColor Yellow
    Write-Host "Chay lai: .\scripts\build.ps1 -Tests -Clean" -ForegroundColor Yellow
    exit 1
}

$config = if ($Debug) { "Debug" } else { "Release" }

Write-Host "=== Chay tests ($config) ===" -ForegroundColor Cyan

Set-Location "$root\build"

$ctestArgs = @("-C", $config, "--output-on-failure")
if ($Verbose) { $ctestArgs += "-V" }

# Chạy ctest
& ctest @ctestArgs
$ctestExitCode = $LASTEXITCODE

# Reset về thư mục gốc
Set-Location $root

# Kiểm tra kết quả đúng cách
if ($ctestExitCode -ne 0) {
    Write-Host "`n=== CO TESTS FAILED (exit code: $ctestExitCode) ===" -ForegroundColor Red
    exit $ctestExitCode
}

Write-Host "`n=== TAT CA TESTS PASSED ===" -ForegroundColor Green