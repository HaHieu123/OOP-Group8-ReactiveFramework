# scripts/build.ps1
# ============================================================================
# Build Reactive Framework (Windows)
# ============================================================================
#
# Cách dùng:
#   .\scripts\build.ps1                    # Build cơ bản (demo)
#   .\scripts\build.ps1 -Tests             # Build + unit tests
#   .\scripts\build.ps1 -Benchmarks        # Build + benchmarks
#   .\scripts\build.ps1 -All               # Build tất cả
#   .\scripts\build.ps1 -All -Clean        # Clean trước khi build
#   .\scripts\build.ps1 -Tests -Debug      # Build Debug
#
# ============================================================================

param(
    [switch]$Tests,
    [switch]$Benchmarks,
    [switch]$All,
    [switch]$Debug,
    [switch]$Clean,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# ----------------------------------------------------------------------------
# Xác định thư mục gốc
# ----------------------------------------------------------------------------
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$config = if ($Debug) { "Debug" } else { "Release" }
$buildTests = $Tests -or $All
$buildBenchmarks = $Benchmarks -or $All

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "  Reactive Framework - Build" -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Root:       $root"
Write-Host "Config:     $config"
Write-Host "Tests:      $buildTests"
Write-Host "Benchmarks: $buildBenchmarks"
Write-Host "Clean:      $Clean"
Write-Host ""

# ----------------------------------------------------------------------------
# Clean
# ----------------------------------------------------------------------------
if ($Clean -and (Test-Path "build")) {
    Write-Host "[CLEAN] Xoa thu muc build/ cu..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build"
    Write-Host ""
}

# ----------------------------------------------------------------------------
# Tạo thư mục build
# ----------------------------------------------------------------------------
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
    if ($Verbose) {
        Write-Host "[INFO] Da tao thu muc build/" -ForegroundColor Gray
    }
}

# ----------------------------------------------------------------------------
# Cấu hình CMake
# ----------------------------------------------------------------------------
$cmakeArgs = @("-B", "build")

# Chỉ set CMAKE_BUILD_TYPE nếu dùng single-config generator (Ninja, Make)
# Với Visual Studio (multi-config), CMAKE_BUILD_TYPE bị bỏ qua
if ($env:CMAKE_GENERATOR -and $env:CMAKE_GENERATOR -notmatch "Visual Studio") {
    $cmakeArgs += "-DCMAKE_BUILD_TYPE=$config"
}

if ($buildTests)      { $cmakeArgs += "-DBUILD_TESTS=ON" }
if ($buildBenchmarks) { $cmakeArgs += "-DBUILD_BENCHMARKS=ON" }

# Nếu không set cờ nào, đảm bảo tắt hết
if (-not $buildTests)      { $cmakeArgs += "-DBUILD_TESTS=OFF" }
if (-not $buildBenchmarks) { $cmakeArgs += "-DBUILD_BENCHMARKS=OFF" }

Write-Host "[CMAKE] Configure:" -ForegroundColor Cyan
Write-Host "  cmake $($cmakeArgs -join ' ')"
Write-Host ""

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[FAIL] CMake configure that bai!" -ForegroundColor Red
    Write-Host "Kiem tra lai CMakeLists.txt va compiler." -ForegroundColor Yellow
    exit $LASTEXITCODE
}

# ----------------------------------------------------------------------------
# Build
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "[BUILD] Bien dich..." -ForegroundColor Cyan
Write-Host ""

& cmake --build build --config $config
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[FAIL] Build that bai!" -ForegroundColor Red
    exit $LASTEXITCODE
}

# ----------------------------------------------------------------------------
# Kiểm tra kết quả
# ----------------------------------------------------------------------------
$exeDir = Join-Path $root "build\$config"

Write-Host ""
Write-Host "[VERIFY] Kiem tra cac file .exe da tao:" -ForegroundColor Cyan

# Danh sach cac target can kiem tra
$expectedTargets = @("reactive_demo")
if ($buildTests)      { $expectedTargets += @("test_models", "test_sink", "test_datastream_basic", "test_datastream_advanced") }
if ($buildBenchmarks) { $expectedTargets += "bench_throughput" }

$allFound = $true
foreach ($target in $expectedTargets) {
    $exePath = Join-Path $exeDir "$target.exe"
    if (Test-Path $exePath) {
        Write-Host "  [OK]   $target.exe" -ForegroundColor Green
    } else {
        Write-Host "  [MISS] $target.exe" -ForegroundColor Red
        $allFound = $false
    }
}

# ----------------------------------------------------------------------------
# Kiểm tra CTest
# ----------------------------------------------------------------------------
if ($buildTests) {
    Write-Host ""
    Write-Host "[VERIFY] Kiem tra CTest:" -ForegroundColor Cyan

    $ctestFile = Join-Path $root "build\CTestTestfile.cmake"
    if (Test-Path $ctestFile) {
        Write-Host "  [OK]   CTestTestfile.cmake" -ForegroundColor Green

        # Đếm số test
        $testCount = (Select-String -Path $ctestFile -Pattern "add_test" -AllMatches).Count
        Write-Host "         $testCount test duoc dang ky" -ForegroundColor Gray
    } else {
        Write-Host "  [MISS] CTestTestfile.cmake" -ForegroundColor Red
        Write-Host "         Kiem tra muc 7 trong CMakeLists.txt" -ForegroundColor Yellow
        $allFound = $false
    }
}

# ----------------------------------------------------------------------------
# Ket qua cuoi cung
# ----------------------------------------------------------------------------
Write-Host ""
if ($allFound) {
    Write-Host "===================================================" -ForegroundColor Green
    Write-Host "  BUILD THANH CONG" -ForegroundColor Green
    Write-Host "===================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Executables tai: $exeDir" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Buoc tiep theo:" -ForegroundColor Yellow
    Write-Host "  .\scripts\run_tests.ps1       # Chay unit tests" -ForegroundColor Gray
    Write-Host "  .\scripts\run_demo.ps1        # Chay demo IoT" -ForegroundColor Gray
    Write-Host "  .\scripts\run_benchmark.ps1   # Chay benchmark" -ForegroundColor Gray
} else {
    Write-Host "===================================================" -ForegroundColor Yellow
    Write-Host "  BUILD HOAN TAT NHUNG CO FILE THIEU" -ForegroundColor Yellow
    Write-Host "===================================================" -ForegroundColor Yellow
    exit 1
}