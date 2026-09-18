# scripts/run_tests.ps1
# Chạy tất cả unit tests (Windows)

param(
    [switch]$Verbose,
    [switch]$Debug
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location "$root\build"

$config = if ($Debug) { "Debug" } else { "Release" }

Write-Host "=== Chay tests ===" -ForegroundColor Cyan

$ctestArgs = @("-C", $config, "--output-on-failure")
if ($Verbose) { $ctestArgs += "-V" }

ctest @ctestArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n=== Tat ca tests PASSED ===" -ForegroundColor Green
} else {
    Write-Host "`n=== Co tests FAILED ===" -ForegroundColor Red
    exit 1
}