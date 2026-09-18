# scripts/clean.ps1
# Xóa tất cả file rác sinh ra khi build/test/run (Windows)

$ErrorActionPreference = "SilentlyContinue"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "=== Cleaning reactive-framework ===" -ForegroundColor Cyan
Write-Host "Root: $root"
Write-Host ""

# File rác trong build
Remove-Item -Force "build\Release\alerts.json"
Remove-Item -Force "build\Release\test_output.csv"
Remove-Item -Force "build\Release\test_tee.csv"
Remove-Item -Force "build\Debug\alerts.json"
Remove-Item -Force "build\Debug\test_output.csv"
Remove-Item -Force "build\Debug\test_tee.csv"

# File rác trong root
Remove-Item -Force "alerts.json"
Remove-Item -Force "test_output.csv"
Remove-Item -Force "test_tee.csv"

# Xóa thư mục build hoàn toàn
Remove-Item -Recurse -Force "build"

Write-Host "Da xoa file rac trong $root" -ForegroundColor Green