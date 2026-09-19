# Reactive Stream Processing Framework (C++20)

A minimal, header-only reactive stream processing framework inspired by
RxCpp and ReactiveX, written in modern C++20.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-green.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## Features

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

## Quick Start

### Yêu cầu

- **CMake** ≥ 3.20
- **Compiler** hỗ trợ C++20:
  - MSVC ≥ 19.30 (Visual Studio 2022)
  - GCC ≥ 11
  - Clang ≥ 14

### Build

```bash
git clone https://github.com/<your-username>/reactive-framework.git
cd reactive-framework
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

## Ví dụ sử dụng

### Ví dụ 1: Cơ bản — filter + map

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

Output:
```
20
40
60
80
```

### Ví dụ 2: IoT Sensor → Alert

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

### Ví dụ 3: Buffer theo thời gian

```cpp
DataStream<Reading>::fromGenerator(sensorGenerator)
    .buffer(std::chrono::milliseconds(500))
    .map([](const std::vector<Reading>& batch) {
        return computeAverage(batch);
    })
    .subscribe(sink);
```

---

## Cấu trúc dự án

```
reactive-framework/
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
└── benchmarks/
    └── bench_throughput.cpp
```

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

## Kết quả benchmark (mẫu)

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

## License

MIT License — xem [LICENSE](LICENSE) để biết chi tiết.

---

## Tác giả

- Ha Hieu — [HaHieu123](https://github.com/HaHieu123)

## Đóng góp
Mọi đóng góp đều được chào đón! Xem [CONTRIBUTING.md](CONTRIBUTING.md) để biết chi tiết.

### Giải thích từng lệnh

| Lệnh | Tác dụng |
|---|---|
| `Remove-Item -Force <file>` | Xóa file, kể cả file read-only |
| `-ErrorAction SilentlyContinue` | Không báo lỗi nếu file không tồn tại |
| `Remove-Item -Recurse -Force build` | Xóa cả thư mục `build/` và nội dung bên trong |
| `Write-Host "..."` | In thông báo xác nhận |

### Tại sao cần xóa?

Các file này **được sinh ra khi chạy** chương trình, không phải source code:

| File | Sinh ra bởi | Khi nào |
|---|---|---|
| `alerts.json` | `reactive_demo.exe` | Chạy demo IoT |
| `test_output.csv` | `test_sink.exe` | Chạy test sink |
| `test_tee.csv` | `test_sink.exe` | Chạy test TeeSink |
| `build/` | CMake + MSVC | Mỗi lần build |

Nếu commit nhầm, repo GitHub sẽ có **hàng chục MB file rác** không cần thiết.

### Kiểm tra trước khi commit

Sau khi xóa, chạy lệnh sau để xác nhận:

```powershell
git status
```

**Kết quả mong đợi:** Không thấy `build/`, `alerts.json`, `test_output.csv`, `test_tee.csv` trong danh sách.

Nếu vẫn thấy → kiểm tra file `.gitignore` đã có các dòng sau chưa:

```gitignore
build/
alerts.json
test_output.csv
test_tee.csv
*.log
```

### Tự động hóa — thêm vào script build

Nếu bạn có file `scripts/build.ps1`, thêm đoạn clean vào đầu script:

```powershell
# scripts/build.ps1

# Clean trước khi build
Write-Host "=== Cleaning old build ===" -ForegroundColor Yellow
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
Remove-Item -Force build\Release\alerts.json -ErrorAction SilentlyContinue
Remove-Item -Force build\Release\test_output.csv -ErrorAction SilentlyContinue
Remove-Item -Force build\Release\test_tee.csv -ErrorAction SilentlyContinue

# ... phần build còn lại
```

### Tương đương trên Linux/macOS

Nếu bạn dùng Linux/macOS, lệnh tương đương là:

```bash
cd reactive-framework

# Xóa file output
rm -f build/Release/alerts.json
rm -f build/Release/test_output.csv
rm -f build/Release/test_tee.csv

# Xóa thư mục build
rm -rf build

echo "Da xoa file rac"
```

### Cách chạy định kỳ (tùy chọn)

Nếu bạn muốn clean trước **mọi lần build**, tạo alias trong PowerShell profile:

```powershell
# Mở profile
notepad $PROFILE

# Thêm dòng này
function rf-clean {
    cd D:\reactive-framework
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
    Remove-Item -Force build\Release\alerts.json -ErrorAction SilentlyContinue
    Remove-Item -Force build\Release\test_output.csv -ErrorAction SilentlyContinue
    Remove-Item -Force build\Release\test_tee.csv -ErrorAction SilentlyContinue
    Write-Host "Da clean reactive-framework" -ForegroundColor Green
}

# Lưu file, đóng Notepad
# Mở lại PowerShell, gõ: rf-clean
```

Sau đó chỉ cần gõ `rf-clean` ở bất kỳ đâu là xóa sạch build + file rác.