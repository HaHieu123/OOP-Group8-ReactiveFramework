// include/reactive/Models.hpp
#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <optional>
#include <cmath>
#include <ostream>
#include <unordered_map>

namespace reactive {

// ============================================================================
// 1. ENUM: Severity — Mức độ nghiêm trọng của cảnh báo
// ============================================================================
enum class Severity : std::uint8_t {
    INFO     = 0,
    WARNING  = 1,
    CRITICAL = 2
};

// Chuyển Severity thành chuỗi (dùng cho logging)
inline const char* to_string(Severity s) noexcept {
    switch (s) {
        case Severity::INFO:     return "INFO";
        case Severity::WARNING:  return "WARNING";
        case Severity::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

// ============================================================================
// 2. STRUCT: Sensor — Metadata của cảm biến
// ============================================================================
struct Sensor {
    std::uint32_t id{0};
    std::string   name;
    std::string   location;
    double        minThreshold{0.0};
    double        maxThreshold{100.0};

    [[nodiscard]] bool isOutOfRange(double value) const noexcept {
        return value < minThreshold || value > maxThreshold;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return id != 0
            && !name.empty()
            && !location.empty()
            && minThreshold < maxThreshold;
    }
};

// ============================================================================
// 3. STRUCT: Reading — Một lần đọc dữ liệu từ cảm biến
// ============================================================================
struct Reading {
    std::uint32_t                         sensorId{0};
    std::chrono::system_clock::time_point timestamp{};
    double                                temperature{0.0};  // °C
    double                                humidity{0.0};     // %
    double                                pressure{0.0};     // hPa

    Reading() = default;

    Reading(std::uint32_t sid,
            std::chrono::system_clock::time_point ts,
            double temp, double hum, double pres) noexcept
        : sensorId(sid), timestamp(ts)
        , temperature(temp), humidity(hum), pressure(pres) {}

    [[nodiscard]] bool isValid() const noexcept {
        const bool tempOk = temperature >= -100.0 && temperature <= 200.0;
        const bool humOk  = humidity    >=    0.0 && humidity    <= 100.0;
        const bool presOk = pressure    >=  800.0 && pressure    <= 1200.0;
        const bool idOk   = sensorId != 0;
        return tempOk && humOk && presOk && idOk;
    }

    [[nodiscard]] std::string toCsv() const {
        std::ostringstream oss;
        oss << sensorId << ','
            << std::chrono::system_clock::to_time_t(timestamp) << ','
            << std::fixed << std::setprecision(2)
            << temperature << ','
            << humidity << ','
            << pressure;
        return oss.str();
    }

    [[nodiscard]] std::string toJson() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2)
            << R"({"sensorId":)" << sensorId
            << R"(,"timestamp":)" << std::chrono::system_clock::to_time_t(timestamp)
            << R"(,"temperature":)" << temperature
            << R"(,"humidity":)" << humidity
            << R"(,"pressure":)" << pressure
            << '}';
        return oss.str();
    }

    [[nodiscard]] bool operator==(const Reading& other) const noexcept {
        return sensorId    == other.sensorId
            && timestamp   == other.timestamp
            && temperature == other.temperature
            && humidity    == other.humidity
            && pressure    == other.pressure;
    }

    [[nodiscard]] bool operator!=(const Reading& other) const noexcept {
        return !(*this == other);
    }
};

// ============================================================================
// 4. STRUCT: Alert — Cảnh báo khi vượt ngưỡng
// ============================================================================
struct Alert {
    std::uint32_t                         sensorId{0};
    std::chrono::system_clock::time_point timestamp{};
    Severity                              severity{Severity::INFO};
    std::string                           message;

    Alert() = default;

    Alert(std::uint32_t sid,
          std::chrono::system_clock::time_point ts,
          Severity sev,
          std::string msg)
        : sensorId(sid), timestamp(ts)
        , severity(sev), message(std::move(msg)) {}

    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << '[' << to_string(severity) << "] "
            << "Sensor #" << sensorId << " @ "
            << std::chrono::system_clock::to_time_t(timestamp)
            << " — " << message;
        return oss.str();
    }

    [[nodiscard]] std::string toJson() const {
        auto escape = [](const std::string& s) {
            std::string out;
            out.reserve(s.size());
            for (char c : s) {
                switch (c) {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n";  break;
                    case '\r': out += "\\r";  break;
                    case '\t': out += "\\t";  break;
                    default:   out += c;      break;
                }
            }
            return out;
        };

        std::ostringstream oss;
        oss << R"({"sensorId":)" << sensorId
            << R"(,"timestamp":)" << std::chrono::system_clock::to_time_t(timestamp)
            << R"(,"severity":")" << to_string(severity) << '"'
            << R"(,"message":")" << escape(message) << '"'
            << '}';
        return oss.str();
    }

    static Alert fromReading(const Reading& r,
                             const Sensor&  s,
                             double warningRatio = 0.9) {
        Alert alert;
        alert.sensorId  = r.sensorId;
        alert.timestamp = r.timestamp;

        if (!s.isValid()) {
            alert.severity = Severity::WARNING;
            alert.message  = "Sensor metadata không hợp lệ";
            return alert;
        }

        if (r.temperature > s.maxThreshold) {
            alert.severity = Severity::CRITICAL;
            alert.message  = "Nhiệt độ vượt ngưỡng tối đa: "
                           + std::to_string(r.temperature)
                           + "°C > " + std::to_string(s.maxThreshold) + "°C";
        } else if (r.temperature < s.minThreshold) {
            alert.severity = Severity::CRITICAL;
            alert.message  = "Nhiệt độ dưới ngưỡng tối thiểu: "
                           + std::to_string(r.temperature)
                           + "°C < " + std::to_string(s.minThreshold) + "°C";
        } else {
            const double warningMax = s.maxThreshold * warningRatio;
            const double warningMin = s.minThreshold * (2.0 - warningRatio);
            if (r.temperature > warningMax || r.temperature < warningMin) {
                alert.severity = Severity::WARNING;
                alert.message  = "Nhiệt độ gần ngưỡng: "
                               + std::to_string(r.temperature) + "°C";
            } else {
                alert.severity = Severity::INFO;
                alert.message  = "Nhiệt độ bình thường: "
                               + std::to_string(r.temperature) + "°C";
            }
        }
        return alert;
    }
};

// ============================================================================
// 5. HELPER: SensorRegistry — Quản lý danh sách Sensor
// ============================================================================
class SensorRegistry {
public:
    void add(Sensor s) {
        if (s.isValid()) {
            sensors_[s.id] = std::move(s);
        }
    }

    [[nodiscard]] std::optional<Sensor> find(std::uint32_t id) const {
        auto it = sensors_.find(id);
        if (it != sensors_.end()) return it->second;
        return std::nullopt;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return sensors_.size();
    }

private:
    std::unordered_map<std::uint32_t, Sensor> sensors_;
};

// ============================================================================
// 6. operator<< — Cho phép in các struct ra std::ostream
// ============================================================================
inline std::ostream& operator<<(std::ostream& os, const Reading& r) {
    return os << r.toJson();
}

inline std::ostream& operator<<(std::ostream& os, const Sensor& s) {
    return os << "Sensor#" << s.id << "(" << s.name << "@" << s.location << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Alert& a) {
    return os << a.toString();
}

} // namespace reactive