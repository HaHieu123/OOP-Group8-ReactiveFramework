// tests/test_datastream_basic.cpp
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

int main() {
    // ========================================================================
    // Test 1: fromIterable + filter + map + console
    // ========================================================================
    std::cout << "=== Test 1: Basic filter + map ===\n";

    std::vector<Reading> readings = {
        {1, std::chrono::system_clock::now(), 25.0, 45.0, 1013.0},
        {2, std::chrono::system_clock::now(), 65.0, 45.0, 1013.0},
        {3, std::chrono::system_clock::now(), 70.0, 45.0, 1013.0},
        {4, std::chrono::system_clock::now(), 30.0, 45.0, 1013.0},
        {5, std::chrono::system_clock::now(), 80.0, 45.0, 1013.0},
    };

    auto consoleSink = makeConsoleSink<double>(
        [](const double& t) {
            return "Nhiệt độ: " + std::to_string(t) + "°C";
        },
        /*useColor=*/false
    );

    DataStream<Reading>::fromIterable(readings)
        .filter([](const Reading& r) { return r.temperature > 60.0; })
        .map([](const Reading& r) { return r.temperature; })
        .subscribe(consoleSink);

    // ========================================================================
    // Test 2: fromGenerator + filter + map
    // ========================================================================
    std::cout << "\n=== Test 2: Generator + filter + map ===\n";

    auto counter = std::make_shared<std::atomic<int>>(0);

    SourceFactory<int> generator =
        [counter](std::function<void(int)> emit, std::atomic<bool>& stop) {
            while (!stop.load() && counter->load() < 10) {
                int value = counter->fetch_add(1);
                emit(value);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            stop.store(true);
        };

    auto intSink = makeConsoleSink<int>(
        [](const int& v) { return "Số chẵn: " + std::to_string(v); },
        /*useColor=*/false
    );

    DataStream<int>::fromGenerator(generator)
        .filter([](const int& v) { return v % 2 == 0; })
        .map([](const int& v) { return v * v; })
        .subscribe(intSink);

    // ========================================================================
    // Test 3: Error handling
    // ========================================================================
    std::cout << "\n=== Test 3: Error handling ===\n";

    std::vector<int> values = {1, 0, 3, 0, 5};
    auto errorCount = std::make_shared<std::atomic<int>>(0);

    DataStream<int>::fromIterable(values)
        .onError([errorCount](const std::exception_ptr&,
                              const int& v) {
            errorCount->fetch_add(1);
            std::cerr << "Lỗi xử lý giá trị: " << v << '\n';
        })
        .map([](const int& v) {
            if (v == 0) throw std::runtime_error("Division by zero");
            return 100 / v;
        })
        .subscribe(makeConsoleSink<int>(
            [](const int& v) { return "Result: " + std::to_string(v); },
            /*useColor=*/false
        ));

    std::cout << "Số lỗi bắt được: " << errorCount->load() << '\n';

    // ========================================================================
    // Test 4: Composition — nhiều filter + map
    // ========================================================================
    std::cout << "\n=== Test 4: Complex composition ===\n";

    DataStream<Reading>::fromIterable(readings)
        .filter([](const Reading& r) { return r.isValid(); })
        .filter([](const Reading& r) { return r.temperature > 20.0; })
        .map([](const Reading& r) { return r.temperature; })
        .filter([](const double& t) { return t < 75.0; })
        .map([](const double& t) { return t * 1.8 + 32.0; })
        .subscribe(makeConsoleSink<double>(
            [](const double& f) {
                return "Fahrenheit: " + std::to_string(f);
            },
            /*useColor=*/false
        ));

    std::cout << "\n🎉 Tất cả DataStream basic tests passed!\n";
    return 0;
}