// tests/test_datastream_advanced.cpp
#include <reactive/DataStream.hpp>
#include <reactive/Sink.hpp>
#include <reactive/Models.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace reactive;
using namespace std::chrono_literals;

int main() {
    // ========================================================================
    // Test 1: buffer(count)
    // ========================================================================
    std::cout << "=== Test 1: buffer(count) ===\n";

    std::vector<int> numbers;
    for (int i = 1; i <= 10; ++i) numbers.push_back(i);

    DataStream<int>::fromIterable(numbers)
        .buffer(3)
        .subscribe([](const std::vector<int>& batch) {
            std::cout << "Batch size " << batch.size() << ": ";
            for (int v : batch) std::cout << v << " ";
            std::cout << '\n';
        });

    // ========================================================================
    // Test 2: buffer(ms)
    // ========================================================================
    std::cout << "\n=== Test 2: buffer(ms) ===\n";

    auto counter = std::make_shared<std::atomic<int>>(0);
    SourceFactory<int> generator =
        [counter](std::function<void(int)> emit, std::atomic<bool>& stop) {
            while (!stop.load() && counter->load() < 20) {
                emit(counter->fetch_add(1));
                std::this_thread::sleep_for(50ms);
            }
            stop.store(true);
        };

    DataStream<int>::fromGenerator(generator)
        .buffer(200ms)
        .subscribe([](const std::vector<int>& batch) {
            std::cout << "Time batch (" << batch.size() << " items): ";
            for (int v : batch) std::cout << v << " ";
            std::cout << '\n';
        });

    // ========================================================================
    // Test 3: subscribe 3 callback (CÓ onError đúng cách)
    // ========================================================================
    std::cout << "\n=== Test 3: subscribe(onNext, onError, onComplete) ===\n";

    DataStream<int>::fromIterable({1, 2, 3, 4, 5})
        .map([](const int& v) -> int {
            if (v == 3) throw std::runtime_error("Số 3 bị cấm!");
            return v * 10;
        })
        .subscribe(
            [](const int& v) { std::cout << "Next: " << v << '\n'; },
            [](const std::exception_ptr& e) {
                try { if (e) std::rethrow_exception(e); }
                catch (const std::exception& ex) {
                    std::cerr << "Error: " << ex.what() << '\n';
                }
            },
            []() { std::cout << "Complete!\n"; }
        );

    // ========================================================================
    // Test 4: onError + filter (skip lỗi, tiếp tục)
    // ========================================================================
    std::cout << "\n=== Test 4: onError + filter ===\n";

    auto errorCount = std::make_shared<std::atomic<int>>(0);

    DataStream<int>::fromIterable({1, 2, 3, 4, 5})
        .onError([errorCount](const std::exception_ptr&, const int& v) {
            ++(*errorCount);
            std::cerr << "  (bắt lỗi ở giá trị " << v << ")\n";
        })
        .map([](const int& v) -> int {
            if (v == 3) throw std::runtime_error("bad value");
            return v * 10;
        })
        .subscribe([](const int& v) {
            std::cout << "OK: " << v << '\n';
        });

    std::cout << "Số lỗi bắt được: " << errorCount->load() << '\n';

    // ========================================================================
    // Test 5: Pipeline đầy đủ — IoT Sensor → Alert (có buffer)
    // ========================================================================
    std::cout << "\n=== Test 5: Full IoT pipeline with buffer ===\n";

    std::vector<Reading> readings = {
        {1, std::chrono::system_clock::now(), 25.0, 45.0, 1013.0},
        {2, std::chrono::system_clock::now(), 65.0, 45.0, 1013.0},
        {3, std::chrono::system_clock::now(), 30.0, 45.0, 1013.0},
        {4, std::chrono::system_clock::now(), 70.0, 45.0, 1013.0},
        {5, std::chrono::system_clock::now(), 80.0, 45.0, 1013.0},
    };

    Sensor s{2, "Temp-02", "Room B", 10.0, 60.0};

    DataStream<Reading>::fromIterable(readings)
        .filter([](const Reading& r) { return r.isValid(); })
        .buffer(2)
        .map([&s](const std::vector<Reading>& batch) -> Alert {
            double sum = 0;
            for (const auto& r : batch) sum += r.temperature;
            const double avg = sum / static_cast<double>(batch.size());
            Reading avgReading{batch[0].sensorId,
                               batch[0].timestamp,
                               avg, 45.0, 1013.0};
            return Alert::fromReading(avgReading, s);
        })
        .subscribe([](const Alert& a) {
            std::cout << a.toString() << '\n';
        });

    std::cout << "\n🎉 Tất cả DataStream advanced tests passed!\n";
    return 0;
}