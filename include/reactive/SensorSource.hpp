// include/reactive/SensorSource.hpp
#pragma once

#include "Source.hpp"
#include "Models.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <thread>
#include <utility>

namespace reactive {

// ============================================================================
// SensorSource — Giả lập một cảm biến IoT phát dữ liệu theo tần số cho trước
// ============================================================================
class SensorSource final : public Source<Reading> {
public:
    SensorSource(Sensor sensor,
                 double baseTemperature,
                 double noiseLevel,
                 std::chrono::milliseconds interval)
        : sensor_(std::move(sensor))
        , baseTemp_(baseTemperature)
        , noise_(noiseLevel)
        , interval_(interval)
        , running_(false)
        , rng_(std::random_device{}())
        , tempDist_(baseTemperature, noiseLevel)
        , humDist_(45.0, 5.0)
        , pressDist_(1013.0, 2.0)
    {
        (void)baseTemp_;
        (void)noise_;
    }

    ~SensorSource() override {
        stop();
    }

    SensorSource(const SensorSource&) = delete;
    SensorSource& operator=(const SensorSource&) = delete;

    void start() override {
        if (running_.exchange(true)) return;
        thread_ = std::thread([this] { emitLoop(); });
    }

    void stop() override {
        if (!running_.exchange(false)) return;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    [[nodiscard]] bool isRunning() const override {
        return running_.load();
    }

    [[nodiscard]] std::optional<Reading> tryGet() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        Reading r = std::move(queue_.front());
        queue_.pop();
        return r;
    }

    [[nodiscard]] const Sensor& sensor() const noexcept { return sensor_; }
    [[nodiscard]] std::uint64_t emittedCount() const noexcept {
        return emitted_.load();
    }

private:
    Sensor                            sensor_;
    double                            baseTemp_;
    double                            noise_;
    std::chrono::milliseconds         interval_;
    std::atomic<bool>                 running_;
    std::thread                       thread_;
    std::queue<Reading>               queue_;
    std::mutex                        mutex_;
    std::atomic<std::uint64_t>        emitted_{0};

    std::mt19937                      rng_;
    std::normal_distribution<double>  tempDist_;
    std::normal_distribution<double>  humDist_;
    std::normal_distribution<double>  pressDist_;

    void emitLoop() {
        while (running_.load()) {
            Reading r{
                sensor_.id,
                std::chrono::system_clock::now(),
                tempDist_(rng_),
                humDist_(rng_),
                pressDist_(rng_)
            };

            {
                std::lock_guard<std::mutex> lock(mutex_);
                queue_.push(std::move(r));
                ++emitted_;
            }

            std::this_thread::sleep_for(interval_);
        }
    }
};

} // namespace reactive