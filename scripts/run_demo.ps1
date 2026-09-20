# scripts/run_demo.ps1
# ============================================================================
# Chạy demo IoT Reactive Framework (Windows)
# ============================================================================
#
# Cách dùng:
#   .\scripts\run_demo.ps1                    # Chạy bản Release
#   .\scripts\run_demo.ps1 -Debug             # Chạy bản Debug
#   .\scripts\run_demo.ps1 -ShowOutput        # Mở alerts.json sau khi chạy
#   .\scripts\run_demo.ps1 -Build             # Build trước nếu chưa có
#   .\scripts\run_demo.ps1 -Build -All        # Build với tất cả (tests + benchmarks)
#   .\scripts\run_demo.ps1 -Duration 10       # Chạy 10 giây (mặc định 5)
#
# ============================================================================

param(
    [switch]$Debug,
    [switch]$Build,
    [switch]$ShowOutput,
    [switch]$Verbose,
    [int]$Duration = 5
)

$ErrorActionPreference = "Stop"

# ----------------------------------------------------------------------------
# Xác định đường dẫn
# ----------------------------------------------------------------------------
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$config = if ($Debug) { "Debug" } else { "Release" }
$exePath = Join-Path $root "build\$config\reactive_demo.exe"
$alertPath = Join-Path $root "build\$config\alerts.json"

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "  Reactive Framework - IoT Demo" -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Root:         $root"
Write-Host "Config:       $config"
Write-Host "Executable:   $exePath"
Write-Host "Alert output: $alertPath"
Write-Host "Duration:     $Duration s"
Write-Host ""

# ----------------------------------------------------------------------------
# Build nếu cần
# ----------------------------------------------------------------------------
if ($Build) {
    Write-Host "[BUILD] Build truoc khi chay..." -ForegroundColor Yellow

    $buildScript = Join-Path $PSScriptRoot "build.ps1"
    if (-not (Test-Path $buildScript)) {
        Write-Host "Loi: Khong tim thay scripts\build.ps1" -ForegroundColor Red
        Write-Host "Chay: .\scripts\build.ps1 -All" -ForegroundColor Yellow
        exit 1
    }

    if ($Debug) {
        & $buildScript -All -Debug
    } else {
        & $buildScript -All
    }

    Write-Host ""
}

# ----------------------------------------------------------------------------
# Kiểm tra executable tồn tại
# ----------------------------------------------------------------------------
if (-not (Test-Path $exePath)) {
    Write-Host "Loi: Khong tim thay: $exePath" -ForegroundColor Red
    Write-Host ""
    Write-Host "Co the ban chua build. Chay mot trong cac lenh sau:" -ForegroundColor Yellow
    Write-Host "  .\scripts\build.ps1                     # Chi build demo" -ForegroundColor Yellow
    Write-Host "  .\scripts\build.ps1 -All                # Build tat ca" -ForegroundColor Yellow
    Write-Host "  .\scripts\run_demo.ps1 -Build           # Tu dong build + chay" -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

# ----------------------------------------------------------------------------
# Xóa alerts.json cũ để chắc chắn lần chạy này tạo file mới
# ----------------------------------------------------------------------------
if (Test-Path $alertPath) {
    if ($Verbose) {
        Write-Host "[INFO] Xoa file alerts.json cu" -ForegroundColor Gray
    }
    Remove-Item -Force $alertPath
}

# ----------------------------------------------------------------------------
# Chạy demo
# ----------------------------------------------------------------------------
Write-Host "[RUN] Chay demo..." -ForegroundColor Green
Write-Host "---------------------------------------------------"
Write-Host ""

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
& $exePath
$exitCode = $LASTEXITCODE
$stopwatch.Stop()

Write-Host ""
Write-Host "---------------------------------------------------"

# ----------------------------------------------------------------------------
# Kiểm tra kết quả
# ----------------------------------------------------------------------------
if ($exitCode -ne 0) {
    Write-Host "[FAIL] Demo ket thuc voi exit code: $exitCode" -ForegroundColor Red
    exit $exitCode
}

Write-Host "[DONE] Demo hoan tat trong $([math]::Round($stopwatch.Elapsed.TotalSeconds, 2)) s" -ForegroundColor Green
Write-Host ""

# ----------------------------------------------------------------------------
# Kiểm tra file output
# ----------------------------------------------------------------------------
if (Test-Path $alertPath) {
    $fileInfo = Get-Item $alertPath
    $sizeKB = [math]::Round($fileInfo.Length / 1KB, 2)
    $lineCount = (Get-Content $alertPath | Measure-Object -Line).Lines

    Write-Host "[FILE] alerts.json: $sizeKB KB, $lineCount dong" -ForegroundColor Cyan

    if ($Verbose) {
        Write-Host ""
        Write-Host "--- 5 dong dau tien cua alerts.json ---" -ForegroundColor Gray
        Get-Content $alertPath -TotalCount 5 | ForEach-Object {
            Write-Host "  $_" -ForegroundColor Gray
        }
        Write-Host "---------------------------------------" -ForegroundColor Gray
    }

    # Mở file nếu được yêu cầu
    if ($ShowOutput) {
        Write-Host ""
        Write-Host "[OPEN] Mo alerts.json..." -ForegroundColor Yellow
        Start-Process $alertPath
    }
} else {
    Write-Host "[WARN] Khong tim thay alerts.json" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "===================================================" -ForegroundColor Green
Write-Host "  DEMO HOAN TAT" -ForegroundColor Green
Write-Host "===================================================" -ForegroundColor Green