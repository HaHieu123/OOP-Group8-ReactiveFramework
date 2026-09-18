// src/main.cpp
// ============================================================================
// DEMO: Hệ thống giám sát môi trường IoT đa luồng
// ============================================================================
#include <reactive/Models.hpp>
#include <reactive/Observer.hpp>
#include <reactive/Sink.hpp>
#include <reactive/DataStream.hpp>
#include <reactive/Source.hpp>
#include <reactive/SensorSource.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using namespace reactive;
using namespace std::chrono_literals;

// ============================================================================
// GLOBAL: Cờ dừng chương trình (Ctrl+C)
// ============================================================================
static std::atomic<bool> g_stopRequested{false};

static void signalHandler(int) {
    g_stopRequested.store(true);
}

// ============================================================================
// Stopwatch
// ============================================================================
class Stopwatch {
public:
    Stopwatch() : start_(std::chrono::steady_clock::now()) {}

    void reset() {
        start_ = std::chrono::steady_clock::now();
    }

    [[nodiscard]] double elapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

// ============================================================================
// Stats
// ============================================================================
struct Stats {
    std::atomic<std::uint64_t> totalReadings{0};
    std::atomic<std::uint64_t> validReadings{0};
    std::atomic<std::uint64_t> alertsCritical{0};
    std::atomic<std::uint64_t> alertsWarning{0};
    std::atomic<std::uint64_t> errors{0};

    mutable std::mutex       latencyMutex;
    std::vector<double>      latenciesMs;

    void recordLatency(double ms) {
        std::lock_guard<std::mutex> lock(latencyMutex);
        latenciesMs.push_back(ms);
    }

    [[nodiscard]] double percentile(double p) const {
        std::lock_guard<std::mutex> lock(latencyMutex);
        if (latenciesMs.empty()) return 0.0;
        auto sorted = latenciesMs;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t idx = static_cast<std::size_t>(
            std::floor(p * static_cast<double>(sorted.size() - 1))
        );
        return sorted[idx];
    }

    void printSummary(double elapsedSec) const {
        const double throughput = (elapsedSec > 0.0)
            ? static_cast<double>(totalReadings.load()) / elapsedSec
            : 0.0;

        std::cout << "\n+===================================================+\n";
        std::cout << "|              KET QUA DO LUONG                     |\n";
        std::cout << "+===================================================+\n";
        std::cout << "| Tong so Readings:        " << std::setw(10)
                  << totalReadings.load() << "      |\n";
        std::cout << "| Readings hop le:         " << std::setw(10)
                  << validReadings.load() << "      |\n";
        std::cout << "| Alerts CRITICAL:         " << std::setw(10)
                  << alertsCritical.load() << "      |\n";
        std::cout << "| Alerts WARNING:          " << std::setw(10)
                  << alertsWarning.load() << "      |\n";
        std::cout << "| Loi:                     " << std::setw(10)
                  << errors.load() << "      |\n";
        std::cout << "+===================================================+\n";
        std::cout << "| Thoi gian chay:          " << std::setw(10)
                  << std::fixed << std::setprecision(2) << elapsedSec
                  << " s    |\n";
        std::cout << "| Throughput:              " << std::setw(10)
                  << std::fixed << std::setprecision(0) << throughput
                  << " msg/s |\n";
        std::cout << "+===================================================+\n";
        std::cout << "| Latency p50:             " << std::setw(10)
                  << std::fixed << std::setprecision(3)
                  << percentile(0.50) << " ms   |\n";
        std::cout << "| Latency p95:             " << std::setw(10)
                  << std::fixed << std::setprecision(3)
                  << percentile(0.95) << " ms   |\n";
        std::cout << "| Latency p99:             " << std::setw(10)
                  << std::fixed << std::setprecision(3)
                  << percentile(0.99) << " ms   |\n";
        std::cout << "+===================================================+\n";
    }
};

// ============================================================================
// StatsSink
// ============================================================================
class StatsSink final : public ISink<Alert> {
public:
    explicit StatsSink(Stats& stats) : stats_(stats) {}

    void onNext(const Alert& a) override {
        switch (a.severity) {
            case Severity::CRITICAL:
                ++stats_.alertsCritical;
                break;
            case Severity::WARNING:
                ++stats_.alertsWarning;
                break;
            case Severity::INFO:
                break;
        }
    }

    void onError(const std::exception_ptr&) override {
        ++stats_.errors;
    }

    void onComplete() override {}
    void flush() override {}

    [[nodiscard]] std::string name() const override {
        return "StatsSink";
    }

private:
    Stats& stats_;
};

// ============================================================================
// MockDbConnection
// ============================================================================
class MockDbConnection final : public IDbConnection {
public:
    bool execute(const std::string& sql) override {
        std::lock_guard<std::mutex> lock(mutex_);
        ++executedCount_;
        lastSql_ = sql;
        return true;
    }

    bool beginTransaction() override { return true; }
    bool commit() override { return true; }
    bool rollback() override { return true; }
    [[nodiscard]] bool isConnected() const override { return true; }

    [[nodiscard]] std::size_t executedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return executedCount_;
    }

private:
    mutable std::mutex mutex_;
    std::size_t        executedCount_{0};
    std::string        lastSql_;
};

// ============================================================================
// MAIN
// ============================================================================
int main() {
    std::cout << "=== Quick Start Example ===\n";
    
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};
    
    DataStream<int>::fromIterable(data)
        .filter([](const int& x) { return x % 2 == 0; })
        .map([](const int& x) { return x * 10; })
        .subscribe([](const int& v) {
            std::cout << v << '\n';
        });
    
    std::cout << "\n=== IoT Demo ===\n";
    std::signal(SIGINT, signalHandler);

    std::cout << "+===================================================+\n";
    std::cout << "|  REACTIVE STREAM PROCESSING - IoT DEMO            |\n";
    std::cout << "|  (Nhan Ctrl+C de dung som)                        |\n";
    std::cout << "+===================================================+\n\n";

    // ------------------------------------------------------------------------
    // 1. Cấu hình
    // ------------------------------------------------------------------------
    constexpr auto DURATION      = 5s;
    constexpr auto EMIT_INTERVAL = 10ms;
    constexpr auto BUFFER_WINDOW = 500ms;
    (void)BUFFER_WINDOW;

    // ------------------------------------------------------------------------
    // 2. Đăng ký sensor metadata
    // ------------------------------------------------------------------------
    SensorRegistry registry;
    registry.add({1, "Temp-A", "Phong A", 10.0, 60.0});
    registry.add({2, "Temp-B", "Phong B", 15.0, 55.0});
    registry.add({3, "Temp-C", "Phong C", 20.0, 50.0});

    std::cout << "[CONFIG] Da dang ky " << registry.size() << " sensors\n";

    // ------------------------------------------------------------------------
    // 3. Tạo SensorSources
    // ------------------------------------------------------------------------
    std::vector<std::shared_ptr<SensorSource>> sources;
    {
        auto s1 = registry.find(1);
        auto s2 = registry.find(2);
        auto s3 = registry.find(3);
        if (s1) sources.push_back(std::make_shared<SensorSource>(
            *s1, 35.0, 8.0, EMIT_INTERVAL));
        if (s2) sources.push_back(std::make_shared<SensorSource>(
            *s2, 45.0, 10.0, EMIT_INTERVAL));
        if (s3) sources.push_back(std::make_shared<SensorSource>(
            *s3, 30.0, 6.0, EMIT_INTERVAL));
    }

    std::cout << "[CONFIG] Da tao " << sources.size() << " sensor sources\n";
    std::cout << "[CONFIG] Moi sensor phat " << (1000 / EMIT_INTERVAL.count())
              << " Hz, chay trong " << DURATION.count() << " s\n\n";

    // ------------------------------------------------------------------------
    // 4. Tạo sinks
    // ------------------------------------------------------------------------
    Stats stats;

    auto consoleSink = makeConsoleSink<Alert>(
        [](const Alert& a) { return a.toString(); },
        true
    );

    auto fileSink = makeJsonFileSink<Alert>("alerts.json");

    auto mockDb = std::make_shared<MockDbConnection>();
    auto dbSink = makeDatabaseSink<Alert>(
        mockDb,
        "alerts",
        [](const Alert& a) {
            std::string msg = a.message;
            std::string escaped;
            escaped.reserve(msg.size());
            for (char c : msg) {
                if (c == '\'') escaped += "''";
                else escaped += c;
            }
            return std::to_string(a.sensorId) + ",'" +
                   to_string(a.severity) + "','" + escaped + "'";
        },
        50
    );

    auto statsSink = std::make_shared<StatsSink>(stats);

    auto teeSink = std::make_shared<TeeSink<Alert>>(
        std::vector<SinkPtr<Alert>>{fileSink, dbSink, statsSink}
    );

    // ------------------------------------------------------------------------
    // 5. Xây dựng pipeline
    // ------------------------------------------------------------------------
    std::cout << "[PIPELINE] Xay dung pipeline...\n";

    Stopwatch pipelineStopwatch;

    auto aggregatedGenerator =
        [&sources, &stats, &registry](
            std::function<void(Reading)> emit,
            std::atomic<bool>& stop) {

            for (auto& src : sources) src->start();

            const auto endTime =
                std::chrono::steady_clock::now() + DURATION;

            while (!stop.load()
                   && !g_stopRequested.load()
                   && std::chrono::steady_clock::now() < endTime) {
                bool gotAny = false;
                for (auto& src : sources) {
                    while (auto r = src->tryGet()) {
                        emit(*r);
                        ++stats.totalReadings;
                        gotAny = true;
                    }
                }
                if (!gotAny) {
                    std::this_thread::sleep_for(1ms);
                }
            }

            for (auto& src : sources) src->stop();
            stop.store(true);
        };

    // Reset stopwatch ngay trước khi subscribe → đo latency chính xác
    pipelineStopwatch.reset();

    DataStream<Reading>::fromGenerator(aggregatedGenerator)
        .filter([&stats](const Reading& r) {
            const bool ok = r.isValid();
            if (ok) ++stats.validReadings;
            return ok;
        })
        .filter([&registry](const Reading& r) {
            return registry.find(r.sensorId).has_value();
        })
        .map([&registry](const Reading& r) -> Alert {
            auto sensor = registry.find(r.sensorId);
            if (!sensor) {
                throw std::runtime_error(
                    "Sensor khong ton tai: " + std::to_string(r.sensorId)
                );
            }
            return Alert::fromReading(r, *sensor);
        })
        .filter([](const Alert& a) {
            return a.severity == Severity::WARNING
                || a.severity == Severity::CRITICAL;
        })
        .subscribe([&stats, &consoleSink, &teeSink,
                    &pipelineStopwatch](const Alert& a) {
            const double latency = pipelineStopwatch.elapsedMs();
            stats.recordLatency(latency);

            if (a.severity == Severity::CRITICAL) {
                consoleSink->onNext(a);
            }

            teeSink->onNext(a);
        });

    // ------------------------------------------------------------------------
    // 6. Kết thúc
    // ------------------------------------------------------------------------
    const double elapsedSec = pipelineStopwatch.elapsedMs() / 1000.0;

    fileSink->flush();
    dbSink->flush();
    teeSink->flush();

    // ------------------------------------------------------------------------
    // 7. In kết quả
    // ------------------------------------------------------------------------
    stats.printSummary(elapsedSec);

    std::cout << "\n[DB] Da thuc thi " << mockDb->executedCount()
              << " cau INSERT\n";
    std::cout << "[FILE] Alerts da ghi vao alerts.json\n";

    std::cout << "\n[SENSOR] So readings moi sensor da phat:\n";
    for (const auto& src : sources) {
        std::cout << "  - " << src->sensor().name
                  << ": " << src->emittedCount() << " readings\n";
    }

    std::cout << "\nDemo hoan tat!\n";
    return 0;
}