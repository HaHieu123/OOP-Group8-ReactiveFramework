# Reactive Stream Processing Framework (C++20)

A minimal, header-only reactive stream processing framework inspired by
RxCpp and ReactiveX, written in modern C++20.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-green.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()

---

## Mục lục

- [Tính năng](#tính-năng)
- [Yêu cầu](#yêu-cầu)
- [Cài đặt](#cài-đặt)
- [Ví dụ nhanh](#ví-dụ-nhanh)
- [Scripts tự động hóa](#scripts-tự-động-hóa)
- [Kiến trúc](#kiến-trúc)
- [Cấu trúc dự án](#cấu-trúc-dự-án)
- [Benchmark](#benchmark)
- [Đóng góp](#đóng-góp)
- [License](#license)

---

## Tính năng

- **Header-only** — chỉ cần `#include <reactive/DataStream.hpp>`
- **Type-safe** — kiểm tra kiểu tại compile-time
- **Lazy** — pipeline chưa chạy cho đến khi `subscribe()`
- **Immutable** — mỗi operator trả về stream mới
- **Thread-safe** — mọi sink đều dùng mutex
- **RAII** — tự động quản lý tài nguyên

### Operators

| Nhóm | Operator | Mô tả |
|---|---|---|
| **Source** | `fromIterable`, `fromGenerator` | Tạo stream từ container/generator |
| **Transform** | `filter`, `map` | Lọc và biến đổi |
| **Window** | `buffer(count)`, `buffer(ms)` | Gom phần tử theo số lượng/thời gian |
| **Error** | `onError` | Xử lý lỗi tập trung |
| **Terminal** | `subscribe(...)` | Kích hoạt pipeline |

### Sinks

| Sink | Mục đích |
|---|---|
| `ConsoleSink<T>` | In ra stdout với ANSI colors |
| `FileSink<T>` | Ghi ra CSV/JSON (RAII, auto-flush) |
| `DatabaseSink<T>` | Batch insert qua `IDbConnection` |
| `TeeSink<T>` | Fan-out tới nhiều sink |

---

## Yêu cầu

- **CMake** ≥ 3.20
- **Compiler** hỗ trợ C++20:
  - MSVC ≥ 19.30 (Visual Studio 2022)
  - GCC ≥ 11
  - Clang ≥ 14

---

## Cài đặt

### Clone repository

```bash
git clone https://github.com/HaHieu123/OOP-Group8-ReactiveFramework.git
cd OOP-Group8-ReactiveFramework
```

### Build bằng CMake

```bash
cmake -B build -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=ON
cmake --build build --config Release
```

### Chạy test

```bash
cd build
ctest -C Release --output-on-failure
```

### Chạy demo

```bash
./build/Release/reactive_demo.exe       # Windows
./build/reactive_demo                   # Linux/macOS
```

### Chạy benchmark

```bash
./build/Release/bench_throughput.exe    # Windows
./build/bench_throughput                # Linux/macOS
```

---

## Ví dụ nhanh

### Ví dụ 1 — filter + map

```cpp
#include <reactive/DataStream.hpp>
#include <reactive/Sink.hpp>

#include <iostream>
#include <vector>

using namespace reactive;

int main() {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};

    DataStream<int>::fromIterable(data)
        .filter([](const int& x) { return x % 2 == 0; })
        .map([](const int& x) { return x * 10; })
        .subscribe([](const int& v) {
            std::cout << v << '\n';
        });

    return 0;
}
```

**Output:**
```
20
40
60
80
```

### Ví dụ 2 — IoT Sensor → Alert

```cpp
#include <reactive/DataStream.hpp>
#include <reactive/Sink.hpp>
#include <reactive/Models.hpp>

using namespace reactive;

int main() {
    SensorRegistry registry;
    registry.add({1, "Temp-A", "Room A", 10.0, 60.0});

    std::vector<Reading> readings = {
        {1, std::chrono::system_clock::now(), 25.0, 45.0, 1013.0},
        {1, std::chrono::system_clock::now(), 65.0, 45.0, 1013.0},
        {1, std::chrono::system_clock::now(), 70.0, 45.0, 1013.0},
    };

    auto sink = makeConsoleSink<Alert>(nullptr, /*useColor=*/false);

    DataStream<Reading>::fromIterable(readings)
        .filter([](const Reading& r) { return r.isValid(); })
        .map([&registry](const Reading& r) {
            return Alert::fromReading(r, *registry.find(r.sensorId));
        })
        .filter([](const Alert& a) {
            return a.severity != Severity::INFO;
        })
        .subscribe(sink);
}
```

### Ví dụ 3 — Buffer theo thời gian

```cpp
DataStream<Reading>::fromGenerator(sensorGenerator)
    .buffer(std::chrono::milliseconds(500))
    .map([](const std::vector<Reading>& batch) {
        return computeAverage(batch);
    })
    .subscribe(sink);
```

---

## Scripts tự động hóa

Dự án cung cấp sẵn các script PowerShell (Windows) và Bash (Linux/macOS)
để tự động hóa các tác vụ thường gặp.

### Windows — PowerShell

| Script | Mục đích |
|---|---|
| `scripts\build.ps1` | Build dự án (tests/benchmarks tùy chọn) |
| `scripts\clean.ps1` | Xóa `build/` và file rác |
| `scripts\run_tests.ps1` | Chạy tất cả unit tests |
| `scripts\run_demo.ps1` | Chạy demo IoT |
| `scripts\run_benchmark.ps1` | Chạy benchmark |

**Ví dụ:**

```powershell
# Build tất cả (tests + benchmarks) + clean trước
.\scripts\build.ps1 -All -Clean

# Chạy tests
.\scripts\run_tests.ps1

# Chạy demo + mở alerts.json
.\scripts\run_demo.ps1 -ShowOutput

# Chạy benchmark 3 lần
.\scripts\run_benchmark.ps1 -Repeat 3

# Xóa file rác
.\scripts\clean.ps1
```

> **Lưu ý:** Nếu PowerShell báo `execution of scripts is disabled`, chạy một lần:
> ```powershell
> Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
> ```

### Linux / macOS — Bash

| Script | Mục đích |
|---|---|
| `scripts/build.sh` | Build dự án |
| `scripts/clean.sh` | Xóa `build/` và file rác |

**Ví dụ:**

```bash
# Cấp quyền chạy (chỉ lần đầu)
chmod +x scripts/*.sh

# Build tất cả + clean trước
./scripts/build.sh --all --clean

# Xóa file rác
./scripts/clean.sh
```

### Tổng hợp lệnh nhanh

| Tác vụ | Windows | Linux/macOS |
|---|---|---|
| Build cơ bản | `.\scripts\build.ps1` | `./scripts/build.sh` |
| Build + tests | `.\scripts\build.ps1 -Tests` | `./scripts/build.sh --tests` |
| Build tất cả | `.\scripts\build.ps1 -All` | `./scripts/build.sh --all` |
| Build + clean | `.\scripts\build.ps1 -All -Clean` | `./scripts/build.sh --all --clean` |
| Xóa file rác | `.\scripts\clean.ps1` | `./scripts/clean.sh` |
| Chạy tests | `.\scripts\run_tests.ps1` | `cd build && ctest -C Release` |
| Chạy demo | `.\scripts\run_demo.ps1` | `./build/reactive_demo` |
| Chạy benchmark | `.\scripts\run_benchmark.ps1` | `./build/bench_throughput` |

---

## Kiến trúc

```
+-------------+     +-------------+     +-------------+
|  Source<T>  | --> | DataStream  | --> |   Sink<T>   |
+-------------+     |     <T>     |     +-------------+
                    +-------------+
                          |
              +-----------+-----------+
              |           |           |
          filter()     map()      buffer()
              |           |           |
              +-----------+-----------+
                          |
                    subscribe()
```

**Core pattern:** Continuation-Passing Style (CPS)

- `DataStream<T>` lưu một `std::function<void(ObserverPtr<T>)>` — mô tả pipeline.
- Mỗi operator wrap pipeline cũ và trả về pipeline mới.
- Chỉ khi gọi `subscribe()`, pipeline mới thực sự chạy.

---

## Cấu trúc dự án

```
OOP-Group8-ReactiveFramework/
├── CMakeLists.txt
├── LICENSE
├── README.md
├── .gitignore
├── include/
│   └── reactive/
│       ├── Models.hpp           # Domain: Sensor, Reading, Alert
│       ├── Observer.hpp         # Observer interface
│       ├── Sink.hpp             # ConsoleSink, FileSink, DatabaseSink
│       ├── DataStream.hpp       # Core framework
│       ├── Source.hpp           # Source interface
│       └── SensorSource.hpp     # IoT sensor simulator
├── src/
│   └── main.cpp                 # Demo
├── tests/
│   ├── test_models.cpp
│   ├── test_sink.cpp
│   ├── test_datastream_basic.cpp
│   └── test_datastream_advanced.cpp
├── benchmarks/
│   └── bench_throughput.cpp
└── scripts/
    ├── build.ps1
    ├── clean.ps1
    ├── run_tests.ps1
    ├── run_demo.ps1
    ├── run_benchmark.ps1
    ├── build.sh
    └── clean.sh
```

---

## Benchmark

| Benchmark | Throughput | p99 Latency |
|---|---|---|
| Reactive filter+map | ~30 M msg/s | ~0.9 µs |
| Imperative tương đương | ~150 M msg/s | ~0.05 µs |
| **Tỷ lệ** | **1/5×** | **1/18×** |

**Đánh đổi:** Reactive chậm hơn ~5× về throughput, nhưng:

- Code ngắn hơn **5×**
- Thêm operator mới nhanh hơn **12×**
- Error handling tập trung
- Đa luồng tự động

---

## Đóng góp

Mọi đóng góp đều được chào đón! Vui lòng xem
[CONTRIBUTING.md](CONTRIBUTING.md) để biết chi tiết về:

- Quy trình tạo Pull Request
- Coding style
- Cách báo lỗi

---

## Tác giả

- **Ha Hieu** — [@HaHieu123](https://github.com/HaHieu123)

---

## License

MIT License — xem [LICENSE](LICENSE) để biết chi tiết.