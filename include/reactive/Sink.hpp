// include/reactive/Sink.hpp
#pragma once

#include "Observer.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace reactive {

// ============================================================================
// ISink<T> — Interface chung cho mọi Sink
// ============================================================================
template <typename T>
class ISink : public Observer<T> {
public:
    ~ISink() override = default;

    virtual void flush() = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

template <typename T>
using SinkPtr = std::shared_ptr<ISink<T>>;

// ============================================================================
// ConsoleSink<T> — In ra màn hình
// ============================================================================
template <typename T>
class ConsoleSink final : public ISink<T> {
public:
    using Formatter = std::function<std::string(const T&)>;

    explicit ConsoleSink(Formatter formatter = nullptr,
                         std::ostream& out = std::cout,
                         bool useColor = true)
        : formatter_(formatter ? std::move(formatter)
                               : [](const T& v) { return defaultFormat(v); })
        , out_(&out)
        , useColor_(useColor)
    {}

    void onNext(const T& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string s = formatter_(value);
        *out_ << (useColor_ ? colorize(s) : s) << '\n';
    }

    void onError(const std::exception_ptr& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (error) std::rethrow_exception(error);
        } catch (const std::exception& e) {
            *out_ << (useColor_ ? "\033[31m" : "")
                  << "[ERROR] " << e.what()
                  << (useColor_ ? "\033[0m" : "") << '\n';
        } catch (...) {
            *out_ << "[ERROR] Unknown exception\n";
        }
    }

    void onComplete() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (useColor_) {
            *out_ << "\033[32m[DONE] Console sink hoàn tất\033[0m\n";
        } else {
            *out_ << "[DONE] Console sink hoàn tất\n";
        }
        out_->flush();
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        out_->flush();
    }

    [[nodiscard]] std::string name() const override {
        return "ConsoleSink";
    }

private:
    Formatter     formatter_;
    std::ostream* out_;
    bool          useColor_;
    std::mutex    mutex_;

    static std::string defaultFormat(const T& v) {
        if constexpr (requires { v.toString(); }) {
            return v.toString();
        } else if constexpr (requires { v.toJson(); }) {
            return v.toJson();
        } else {
            return std::string{"<unformatted>"};
        }
    }

    static std::string colorize(const std::string& s) {
        if (s.find("[CRITICAL]") != std::string::npos) {
            return "\033[1;31m" + s + "\033[0m";
        } else if (s.find("[WARNING]") != std::string::npos) {
            return "\033[1;33m" + s + "\033[0m";
        } else if (s.find("[INFO]") != std::string::npos) {
            return "\033[1;32m" + s + "\033[0m";
        }
        return s;
    }
};

// ============================================================================
// FileSink<T> — Ghi ra file (RAII)
// ============================================================================
template <typename T>
class FileSink final : public ISink<T> {
public:
    using Formatter = std::function<std::string(const T&)>;

    enum class Format { CSV, JSON, RAW };

    explicit FileSink(const std::string& path,
                      Formatter formatter = nullptr,
                      Format format = Format::CSV)
        : path_(path)
        , formatter_(formatter ? std::move(formatter)
                               : makeDefaultFormatter(format))
        , format_(format)
    {
        file_.open(path, std::ios::out | std::ios::app | std::ios::binary);
        if (!file_.is_open()) {
            throw std::runtime_error("FileSink: không thể mở file " + path_);
        }

        if (format == Format::CSV) {
            file_.seekp(0, std::ios::end);
            if (file_.tellp() == 0) {
                file_ << "sensorId,timestamp,temperature,humidity,pressure\n";
            }
        }
    }

    ~FileSink() override {
        try {
            flush();
        } catch (...) {}
    }

    FileSink(const FileSink&) = delete;
    FileSink& operator=(const FileSink&) = delete;

    void onNext(const T& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        file_ << formatter_(value) << '\n';
        const std::size_t n = ++count_;
        if (n % 1000 == 0) {
            file_.flush();
        }
    }

    void onError(const std::exception_ptr& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (error) std::rethrow_exception(error);
        } catch (const std::exception& e) {
            file_ << "ERROR," << e.what() << '\n';
            file_.flush();
        }
    }

    void onComplete() override {
        std::lock_guard<std::mutex> lock(mutex_);
        file_.flush();
        if (file_.is_open()) file_.close();
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) file_.flush();
    }

    [[nodiscard]] std::string name() const override {
        return "FileSink(" + path_ + ")";
    }

    [[nodiscard]] std::size_t count() const noexcept {
        return count_.load();
    }

private:
    std::string              path_;
    std::ofstream            file_;
    Formatter                formatter_;
    Format                   format_;
    std::mutex               mutex_;
    std::atomic<std::size_t> count_{0};

    static Formatter makeDefaultFormatter(Format fmt) {
        switch (fmt) {
            case Format::CSV:
                return [](const T& v) -> std::string {
                    if constexpr (requires { v.toCsv(); }) {
                        return v.toCsv();
                    } else {
                        return std::string{"<no toCsv>"};
                    }
                };
            case Format::JSON:
                return [](const T& v) -> std::string {
                    if constexpr (requires { v.toJson(); }) {
                        return v.toJson();
                    } else {
                        return std::string{"<no toJson>"};
                    }
                };
            case Format::RAW:
            default:
                return [](const T& v) -> std::string {
                    if constexpr (requires { v.toString(); }) {
                        return v.toString();
                    } else {
                        return std::string{"<no toString>"};
                    }
                };
        }
    }
};

// ============================================================================
// IDbConnection — Interface trừu tượng cho DB
// ============================================================================
class IDbConnection {
public:
    virtual ~IDbConnection() = default;

    virtual bool execute(const std::string& sql) = 0;
    virtual bool beginTransaction() = 0;
    virtual bool commit() = 0;
    virtual bool rollback() = 0;
    [[nodiscard]] virtual bool isConnected() const = 0;
};

using DbConnectionPtr = std::shared_ptr<IDbConnection>;

// ============================================================================
// DatabaseSink<T> — Ghi vào DB theo batch
// ============================================================================
template <typename T>
class DatabaseSink final : public ISink<T> {
public:
    using Serializer = std::function<std::string(const T&)>;
    using TableName  = std::string;

    explicit DatabaseSink(DbConnectionPtr connection,
                          TableName table,
                          Serializer serializer,
                          std::size_t batchSize = 100)
        : connection_(std::move(connection))
        , table_(std::move(table))
        , serializer_(std::move(serializer))
        , batchSize_(batchSize == 0 ? 1 : batchSize)
    {
        if (!connection_ || !connection_->isConnected()) {
            throw std::runtime_error("DatabaseSink: kết nối DB không hợp lệ");
        }
        buffer_.reserve(batchSize_);
    }

    ~DatabaseSink() override {
        try {
            flush();
        } catch (...) {}
    }

    void onNext(const T& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.push_back(serializer_(value));
        if (buffer_.size() >= batchSize_) {
            try {
                flushBatchLocked();
            } catch (const std::exception& e) {
                std::cerr << "[DatabaseSink] flush batch thất bại: "
                          << e.what() << '\n';
            }
        }
    }

    void onError(const std::exception_ptr& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (error) std::rethrow_exception(error);
        } catch (const std::exception& e) {
            std::cerr << "[DatabaseSink] Lỗi: " << e.what() << '\n';
            if (connection_) connection_->rollback();
        }
    }

    void onComplete() override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            flushBatchLocked();
            if (connection_) connection_->commit();
        } catch (const std::exception& e) {
            std::cerr << "[DatabaseSink] onComplete thất bại: "
                      << e.what() << '\n';
        }
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        flushBatchLocked();
    }

    [[nodiscard]] std::string name() const override {
        return "DatabaseSink(" + table_ + ")";
    }

private:
    DbConnectionPtr          connection_;
    TableName                table_;
    Serializer               serializer_;
    std::size_t              batchSize_;
    std::vector<std::string> buffer_;
    std::mutex               mutex_;

    void flushBatchLocked() {
        if (buffer_.empty() || !connection_) return;

        if (!connection_->beginTransaction()) {
            std::cerr << "[DatabaseSink] Không thể bắt đầu transaction\n";
            buffer_.clear();
            return;
        }

        try {
            for (const auto& row : buffer_) {
                const std::string sql = "INSERT INTO " + table_
                                      + " VALUES (" + row + ");";
                if (!connection_->execute(sql)) {
                    throw std::runtime_error("INSERT thất bại: " + sql);
                }
            }
            connection_->commit();
            buffer_.clear();
        } catch (...) {
            connection_->rollback();
            buffer_.clear();
            throw;
        }
    }
};

// ============================================================================
// TeeSink<T> — Ghi đồng thời ra nhiều Sink
// ============================================================================
template <typename T>
class TeeSink final : public ISink<T> {
public:
    explicit TeeSink(std::vector<SinkPtr<T>> sinks)
        : sinks_(std::move(sinks)) {}

    void onNext(const T& value) override {
        for (auto& s : sinks_) s->onNext(value);
    }
    void onError(const std::exception_ptr& e) override {
        for (auto& s : sinks_) s->onError(e);
    }
    void onComplete() override {
        for (auto& s : sinks_) s->onComplete();
    }
    void flush() override {
        for (auto& s : sinks_) s->flush();
    }
    [[nodiscard]] std::string name() const override {
        return "TeeSink(" + std::to_string(sinks_.size()) + " sinks)";
    }

private:
    std::vector<SinkPtr<T>> sinks_;
};

// ============================================================================
// Factory functions
// ============================================================================
template <typename T>
SinkPtr<T> makeConsoleSink(
    typename ConsoleSink<T>::Formatter formatter = nullptr,
    bool useColor = true)
{
    return std::make_shared<ConsoleSink<T>>(
        std::move(formatter), std::cout, useColor);
}

template <typename T>
SinkPtr<T> makeCsvFileSink(const std::string& path)
{
    return std::make_shared<FileSink<T>>(
        path, nullptr, FileSink<T>::Format::CSV);
}

template <typename T>
SinkPtr<T> makeJsonFileSink(const std::string& path)
{
    return std::make_shared<FileSink<T>>(
        path, nullptr, FileSink<T>::Format::JSON);
}

template <typename T>
SinkPtr<T> makeDatabaseSink(DbConnectionPtr conn,
                            const std::string& table,
                            typename DatabaseSink<T>::Serializer serializer,
                            std::size_t batchSize = 100)
{
    return std::make_shared<DatabaseSink<T>>(
        std::move(conn), table, std::move(serializer), batchSize);
}

} // namespace reactive