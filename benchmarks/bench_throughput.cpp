// benchmarks/bench_throughput.cpp
// ============================================================================
// BENCHMARK SUITE - Reactive Stream Processing Framework
// ============================================================================
#include <reactive/DataStream.hpp>
#include <reactive/Sink.hpp>
#include <reactive/Models.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace reactive;
using namespace std::chrono;

// ============================================================================
// NullSink - sink rong, chi dem
// ============================================================================
template <typename T>
class NullSink final : public ISink<T> {
public:
    std::atomic<std::uint64_t> count{0};

    void onNext(const T&) override { ++count; }
    void onError(const std::exception_ptr&) override {}
    void onComplete() override {}
    void flush() override {}

    [[nodiscard]] std::string name() const override { return "NullSink"; }
};

// ============================================================================
// BenchResult
// ============================================================================
struct BenchResult {
    std::string   name;
    double        throughput;   // msg/s
    double        latencyP50;   // ms
    double        latencyP95;   // ms
    double        latencyP99;   // ms
    double        elapsedSec;
    std::uint64_t count;
};

// ============================================================================
// Percentile helper
// ============================================================================
static double percentile(std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    const std::size_t idx = static_cast<std::size_t>(
        p * static_cast<double>(sorted.size() - 1)
    );
    return sorted[idx];
}

// ============================================================================
// Bench 1: Throughput toi da
// ============================================================================
BenchResult benchMaxThroughput(std::size_t n) {
    std::cout << "\n[Bench 1] Max Throughput: " << n << " items\n";

    std::vector<int> data(n);
    std::iota(data.begin(), data.end(), 0);

    auto sink = std::make_shared<NullSink<int>>();

    const auto t0 = steady_clock::now();
    DataStream<int>::fromIterable(data)
        .filter([](const int& x) { return x % 2 == 0; })
        .map([](const int& x) { return x * 2; })
        .subscribe(sink);
    const auto t1 = steady_clock::now();

    const double elapsed = duration<double>(t1 - t0).count();
    const double tp = static_cast<double>(sink->count.load()) / elapsed;

    std::cout << "  -> Count: " << sink->count.load()
              << " | Time: " << std::fixed << std::setprecision(3)
              << elapsed << " s"
              << " | Throughput: " << std::setprecision(0)
              << tp << " msg/s\n";

    return {"MaxThroughput", tp, 0.0, 0.0, 0.0, elapsed,
            sink->count.load()};
}

// ============================================================================
// Bench 2: Latency (do dung - per-item timestamp)
// ============================================================================
BenchResult benchLatency(std::size_t n) {
    std::cout << "\n[Bench 2] Latency: " << n << " items\n";

    std::vector<int> data(n);
    std::iota(data.begin(), data.end(), 0);

    // Moi item di kem timestamp rieng de do latency chinh xac
    struct TimedItem {
        int                                   value;
        steady_clock::time_point              created;
    };

    std::vector<TimedItem> timed;
    timed.reserve(n);
    for (int x : data) {
        timed.push_back({x, steady_clock::now()});
    }

    std::vector<double> latencies;
    latencies.reserve(n);

    const auto t0 = steady_clock::now();
    DataStream<TimedItem>::fromIterable(timed)
        .filter([](const TimedItem& item) {
            return item.value % 2 == 0;
        })
        .map([](const TimedItem& item) {
            return item.value * 2;
        })
        .subscribe([&latencies](const int& /*value*/) {
            // Latency thuc su: khong do duoc chinh xac tu day
            // vi chung ta khong co timestamp cua item goc
            // -> Day la han che cua pipeline don gian
            latencies.push_back(0.0);
        });

    // Do lai theo cach khac: do toan bo thoi gian, chia cho so item
    const auto t1 = steady_clock::now();
    const double elapsed = duration<double>(t1 - t0).count();
    const double avgLatencyMs = (elapsed * 1000.0) / static_cast<double>(n);

    // Do latency pipeline-only: chay tung item rieng le
    latencies.clear();
    latencies.reserve(n / 2);
    for (std::size_t i = 0; i < n; i += 2) {
        const auto start = steady_clock::now();

        DataStream<int>::fromIterable(std::vector<int>{static_cast<int>(i)})
            .filter([](const int& x) { return x % 2 == 0; })
            .map([](const int& x) { return x * 2; })
            .subscribe([](const int&) {});

        const auto end = steady_clock::now();
        const double ms = duration<double, std::milli>(end - start).count();
        latencies.push_back(ms);
    }

    std::sort(latencies.begin(), latencies.end());
    const double p50 = percentile(latencies, 0.50);
    const double p95 = percentile(latencies, 0.95);
    const double p99 = percentile(latencies, 0.99);

    std::cout << "  -> Items: " << n
              << " | Elapsed: " << std::fixed << std::setprecision(3)
              << elapsed << " s"
              << " | Avg: " << std::setprecision(4) << avgLatencyMs << " ms"
              << " | p50: " << p50 << " ms"
              << " | p95: " << p95 << " ms"
              << " | p99: " << p99 << " ms\n";

    return {"Latency", static_cast<double>(n) / elapsed,
            p50, p95, p99, elapsed, static_cast<std::uint64_t>(n)};
}

// ============================================================================
// Bench 3: Imperative equivalent
// ============================================================================
BenchResult benchImperative(std::size_t n) {
    std::cout << "\n[Bench 3] Imperative equivalent: " << n << " items\n";

    std::vector<int> data(n);
    std::iota(data.begin(), data.end(), 0);

    std::uint64_t count = 0;

    const auto t0 = steady_clock::now();
    for (const int x : data) {
        if (x % 2 != 0) continue;
        const int y = x * 2;
        ++count;
        // Ghi nho: khong do per-item latency trong imperative
        // vi no khong co y nghia (chi la 1 vong lap)
        (void)y;
    }
    const auto t1 = steady_clock::now();

    const double elapsed = duration<double>(t1 - t0).count();
    const double tp = static_cast<double>(count) / elapsed;

    std::cout << "  -> Count: " << count
              << " | Time: " << std::fixed << std::setprecision(3)
              << elapsed << " s"
              << " | Throughput: " << std::setprecision(0) << tp
              << " msg/s\n";

    return {"Imperative", tp, 0.0, 0.0, 0.0, elapsed, count};
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    std::cout << "+===================================================+\n";
    std::cout << "|    REACTIVE FRAMEWORK - BENCHMARK SUITE           |\n";
    std::cout << "+===================================================+\n";

    constexpr std::size_t N = 1'000'000;   // 1 trieu (nho hon de chay nhanh)

    auto r1 = benchMaxThroughput(N);
    auto r2 = benchLatency(N);
    auto r3 = benchImperative(N);

    // Bang so sanh
    std::cout << "\n+==========================================================+\n";
    std::cout << "|              BANG SO SANH TONG HOP                       |\n";
    std::cout << "+==========================================================+\n";
    std::cout << "| Benchmark       | Throughput (M/s) | p50 (ms) | p99 (ms) |\n";
    std::cout << "+=================+==================+==========+==========+\n";

    auto printRow = [](const BenchResult& r) {
        std::cout << "| " << std::setw(15) << std::left << r.name << " | "
                  << std::setw(16) << std::right << std::fixed
                  << std::setprecision(3)
                  << (r.throughput / 1'000'000.0) << " | "
                  << std::setw(8) << std::setprecision(4) << r.latencyP50
                  << " | "
                  << std::setw(8) << r.latencyP99 << " |\n";
    };
    printRow(r1);
    printRow(r2);
    printRow(r3);

    std::cout << "+==========================================================+\n";

    // Phan tich
    std::cout << "\nPHAN TICH:\n";
    if (r3.throughput > 0.0) {
        std::cout << "  * Reactive / Imperative (throughput): "
                  << std::fixed << std::setprecision(2)
                  << (r1.throughput / r3.throughput) << "x\n";
    }
    if (r3.latencyP99 > 0.0) {
        std::cout << "  * Reactive / Imperative (p99 latency): "
                  << (r2.latencyP99 / r3.latencyP99) << "x\n";
    }

    return 0;
}