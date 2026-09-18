// include/reactive/DataStream.hpp
#pragma once

#include "Observer.hpp"
#include "Models.hpp"
#include "Sink.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace reactive {

// ============================================================================
// 1. FORWARD DECLARATIONS & TYPEDEFS
// ============================================================================
template <typename T>
class DataStream;

template <typename T>
using PipelineFn = std::function<void(ObserverPtr<T>)>;

template <typename T>
using ErrorHandler = std::function<void(const std::exception_ptr&, const T&)>;

template <typename T>
using SourceFactory = std::function<void(std::function<void(T)>,
                                          std::atomic<bool>&)>;

// ============================================================================
// 2. INTERMEDIATE OBSERVERS
// ============================================================================

// 2.1. FilterObserver<T, Predicate>
template <typename T, typename Predicate>
class FilterObserver final : public Observer<T> {
public:
    FilterObserver(ObserverPtr<T> downstream,
                   Predicate predicate,
                   ErrorHandler<T> errorHandler)
        : downstream_(std::move(downstream))
        , predicate_(std::move(predicate))
        , errorHandler_(std::move(errorHandler))
    {}

    void onNext(const T& value) override {
        try {
            if (predicate_(value)) {
                downstream_->onNext(value);
            }
        } catch (...) {
            errorHandler_(std::current_exception(), value);
        }
    }

    void onError(const std::exception_ptr& e) override {
        downstream_->onError(e);
    }

    void onComplete() override {
        downstream_->onComplete();
    }

private:
    ObserverPtr<T>  downstream_;
    Predicate       predicate_;
    ErrorHandler<T> errorHandler_;
};

// 2.2. MapObserver<T, U, Mapper>
template <typename T, typename U, typename Mapper>
class MapObserver final : public Observer<T> {
public:
    MapObserver(ObserverPtr<U> downstream,
                Mapper mapper,
                ErrorHandler<T> errorHandler)
        : downstream_(std::move(downstream))
        , mapper_(std::move(mapper))
        , errorHandler_(std::move(errorHandler))
    {}

    void onNext(const T& value) override {
        try {
            downstream_->onNext(mapper_(value));
        } catch (...) {
            errorHandler_(std::current_exception(), value);
        }
    }

    void onError(const std::exception_ptr& e) override {
        downstream_->onError(e);
    }

    void onComplete() override {
        downstream_->onComplete();
    }

private:
    ObserverPtr<U>  downstream_;
    Mapper          mapper_;
    ErrorHandler<T> errorHandler_;
};

// 2.3. SimpleErrorObserver<T, Handler>
template <typename T, typename Handler>
class SimpleErrorObserver final : public Observer<T> {
public:
    SimpleErrorObserver(ObserverPtr<T> downstream, Handler handler)
        : downstream_(std::move(downstream))
        , handler_(std::move(handler))
    {}

    void onNext(const T& value) override {
        lastValue_ = value;
        hasValue_ = true;
        downstream_->onNext(value);
    }

    void onError(const std::exception_ptr& e) override {
        try {
            handler_(e, lastValue_);
        } catch (...) {
            downstream_->onError(std::current_exception());
            return;
        }
        downstream_->onError(e);
    }

    void onComplete() override {
        downstream_->onComplete();
    }

private:
    ObserverPtr<T> downstream_;
    Handler        handler_;
    T              lastValue_{};
    bool           hasValue_{false};
};

// 2.4. TimeBufferObserver<T>
template <typename T>
class TimeBufferObserver final : public Observer<T> {
public:
    TimeBufferObserver(ObserverPtr<std::vector<T>> downstream,
                       std::chrono::milliseconds window,
                       ErrorHandler<T> errorHandler)
        : downstream_(std::move(downstream))
        , window_(window)
        , errorHandler_(std::move(errorHandler))
        , lastFlush_(std::chrono::steady_clock::now())
    {
        buffer_.reserve(64);
    }

    void onNext(const T& value) override {
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            buffer_.push_back(value);

            const auto now = std::chrono::steady_clock::now();
            if (now - lastFlush_ >= window_) {
                flushBuffer();
                lastFlush_ = now;
            }
        } catch (...) {
            errorHandler_(std::current_exception(), value);
        }
    }

    void onError(const std::exception_ptr& e) override {
        std::lock_guard<std::mutex> lock(mutex_);
        flushBuffer();
        downstream_->onError(e);
    }

    void onComplete() override {
        std::lock_guard<std::mutex> lock(mutex_);
        flushBuffer();
        downstream_->onComplete();
    }

private:
    ObserverPtr<std::vector<T>>  downstream_;
    std::chrono::milliseconds    window_;
    ErrorHandler<T>              errorHandler_;
    std::vector<T>               buffer_;
    std::chrono::steady_clock::time_point lastFlush_;
    std::mutex                   mutex_;

    void flushBuffer() {
        if (buffer_.empty()) return;
        downstream_->onNext(buffer_);
        buffer_.clear();
        buffer_.reserve(64);
    }
};

// 2.5. CountBufferObserver<T>
template <typename T>
class CountBufferObserver final : public Observer<T> {
public:
    CountBufferObserver(ObserverPtr<std::vector<T>> downstream,
                        std::size_t count,
                        ErrorHandler<T> errorHandler)
        : downstream_(std::move(downstream))
        , count_(count)
        , errorHandler_(std::move(errorHandler))
    {
        buffer_.reserve(count_);
    }

    void onNext(const T& value) override {
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            buffer_.push_back(value);

            if (buffer_.size() >= count_) {
                flushBuffer();
            }
        } catch (...) {
            errorHandler_(std::current_exception(), value);
        }
    }

    void onError(const std::exception_ptr& e) override {
        std::lock_guard<std::mutex> lock(mutex_);
        flushBuffer();
        downstream_->onError(e);
    }

    void onComplete() override {
        std::lock_guard<std::mutex> lock(mutex_);
        flushBuffer();
        downstream_->onComplete();
    }

private:
    ObserverPtr<std::vector<T>>  downstream_;
    std::size_t                  count_;
    ErrorHandler<T>              errorHandler_;
    std::vector<T>               buffer_;
    std::mutex                   mutex_;

    void flushBuffer() {
        if (buffer_.empty()) return;
        downstream_->onNext(buffer_);
        buffer_.clear();
        buffer_.reserve(count_);
    }
};

// 2.6. LambdaObserver<T>
template <typename T>
class LambdaObserver final : public Observer<T> {
public:
    template <typename OnNext, typename OnError, typename OnComplete>
    LambdaObserver(OnNext onNext, OnError onError, OnComplete onComplete)
        : onNext_(std::move(onNext))
        , onError_(std::move(onError))
        , onComplete_(std::move(onComplete))
    {}

    void onNext(const T& value) override {
        try {
            onNext_(value);
        } catch (...) {
            onError_(std::current_exception());
        }
    }

    void onError(const std::exception_ptr& e) override {
        try {
            onError_(e);
        } catch (...) {
            std::cerr << "[LambdaObserver] onError handler threw\n";
        }
    }

    void onComplete() override {
        try {
            onComplete_();
        } catch (...) {
            std::cerr << "[LambdaObserver] onComplete handler threw\n";
        }
    }

private:
    std::function<void(const T&)>                  onNext_;
    std::function<void(const std::exception_ptr&)> onError_;
    std::function<void()>                          onComplete_;
};

// ============================================================================
// 3. DataStream<T>
// ============================================================================
template <typename T>
class DataStream {
public:
    explicit DataStream(PipelineFn<T> pipeline,
                        std::string name = "DataStream")
        : pipeline_(std::move(pipeline))
        , name_(std::move(name))
        , errorHandler_([](const std::exception_ptr& e, const T&) {
              try {
                  if (e) std::rethrow_exception(e);
              } catch (const std::exception& ex) {
                  std::cerr << "[DataStream] Unhandled error: "
                            << ex.what() << '\n';
              } catch (...) {
                  std::cerr << "[DataStream] Unknown error\n";
              }
          })
    {}

    DataStream(DataStream&&) noexcept = default;
    DataStream& operator=(DataStream&&) noexcept = default;
    DataStream(const DataStream&) = delete;
    DataStream& operator=(const DataStream&) = delete;

    // ------------------------------------------------------------------------
    // 3.1. fromIterable — overload cho container
    // ------------------------------------------------------------------------
    template <typename Iterable>
    static DataStream<T> fromIterable(Iterable items,
                                       std::string name = "fromIterable")
    {
        auto shared = std::make_shared<Iterable>(std::move(items));

        return DataStream<T>(
            [shared](ObserverPtr<T> sink) {
                try {
                    for (const auto& item : *shared) {
                        sink->onNext(item);
                    }
                    sink->onComplete();
                } catch (...) {
                    sink->onError(std::current_exception());
                }
            },
            std::move(name)
        );
    }

    // ------------------------------------------------------------------------
    // 3.2. fromIterable — overload cho initializer_list
    // ------------------------------------------------------------------------
    static DataStream<T> fromIterable(std::initializer_list<T> items,
                                       std::string name = "fromIterable")
    {
        auto shared = std::make_shared<std::vector<T>>(items);

        return DataStream<T>(
            [shared](ObserverPtr<T> sink) {
                try {
                    for (const auto& item : *shared) {
                        sink->onNext(item);
                    }
                    sink->onComplete();
                } catch (...) {
                    sink->onError(std::current_exception());
                }
            },
            std::move(name)
        );
    }

    // ------------------------------------------------------------------------
    // 3.3. fromGenerator
    // ------------------------------------------------------------------------
    static DataStream<T> fromGenerator(SourceFactory<T> factory,
                                        std::string name = "fromGenerator")
    {
        auto sharedFactory =
            std::make_shared<SourceFactory<T>>(std::move(factory));

        return DataStream<T>(
            [sharedFactory](ObserverPtr<T> sink) {
                auto stopFlag = std::make_shared<std::atomic<bool>>(false);
                try {
                    (*sharedFactory)(
                        [sink](T value) { sink->onNext(value); },
                        *stopFlag
                    );
                    stopFlag->store(true);
                    sink->onComplete();
                } catch (...) {
                    stopFlag->store(true);
                    sink->onError(std::current_exception());
                }
            },
            std::move(name)
        );
    }

    // ------------------------------------------------------------------------
    // 3.4. filter
    // ------------------------------------------------------------------------
    template <typename Predicate>
    DataStream<T> filter(Predicate predicate,
                         std::string name = "filter") const
    {
        static_assert(
            std::is_invocable_r_v<bool, Predicate, const T&>,
            "Predicate phải có signature: bool(const T&)"
        );

        auto upstream     = pipeline_;
        auto errorHandler = errorHandler_;
        auto pred         = std::make_shared<Predicate>(std::move(predicate));

        return DataStream<T>(
            [upstream, errorHandler, pred](ObserverPtr<T> sink) {
                auto filtered =
                    std::make_shared<FilterObserver<T, Predicate>>(
                        sink, *pred, errorHandler
                    );
                upstream(filtered);
            },
            std::move(name)
        );
    }

    // ------------------------------------------------------------------------
    // 3.5. map
    // ------------------------------------------------------------------------
    template <typename Mapper>
    auto map(Mapper mapper, std::string name = "map") const
        -> DataStream<std::invoke_result_t<Mapper, const T&>>
    {
        using U = std::invoke_result_t<Mapper, const T&>;

        static_assert(
            std::is_invocable_v<Mapper, const T&>,
            "Mapper phải có signature: U(const T&)"
        );

        auto upstream     = pipeline_;
        auto errorHandler = errorHandler_;
        auto mapFn        = std::make_shared<Mapper>(std::move(mapper));

        return DataStream<U>(
            [upstream, errorHandler, mapFn](ObserverPtr<U> sink) {
                auto mapped =
                    std::make_shared<MapObserver<T, U, Mapper>>(
                        sink, *mapFn, errorHandler
                    );
                upstream(mapped);
            },
            std::move(name)
        );
    }

    // ------------------------------------------------------------------------
    // 3.6. buffer(count)
    // ------------------------------------------------------------------------
    DataStream<std::vector<T>> buffer(std::size_t count,
                                       std::string name = "bufferCount") const
    {
        if (count == 0) {
            throw std::invalid_argument("buffer count phải > 0");
        }

        auto upstream     = pipeline_;
        auto errorHandler = errorHandler_;

        return DataStream<std::vector<T>>(
            [upstream, errorHandler, count](
                ObserverPtr<std::vector<T>> sink) {
                auto bufferObs =
                    std::make_shared<CountBufferObserver<T>>(
                        sink, count, errorHandler
                    );
                upstream(bufferObs);
            },
            std::move(name)
        );
    }

    // ------------------------------------------------------------------------
    // 3.7. buffer(ms)
    // ------------------------------------------------------------------------
    DataStream<std::vector<T>> buffer(std::chrono::milliseconds window,
                                       std::string name = "bufferTime") const
    {
        auto upstream     = pipeline_;
        auto errorHandler = errorHandler_;

        return DataStream<std::vector<T>>(
            [upstream, errorHandler, window](
                ObserverPtr<std::vector<T>> sink) {
                auto bufferObs =
                    std::make_shared<TimeBufferObserver<T>>(
                        sink, window, errorHandler
                    );
                upstream(bufferObs);
            },
            std::move(name)
        );
    }

    // ------------------------------------------------------------------------
    // 3.8. onError
    // ------------------------------------------------------------------------
    template <typename Handler>
    DataStream<T> onError(Handler handler,
                          std::string name = "onError") const
    {
        static_assert(
            std::is_invocable_r_v<void, Handler,
                                  const std::exception_ptr&, const T&>,
            "Handler phải có signature: void(const exception_ptr&, const T&)"
        );

        auto upstream   = pipeline_;
        auto newHandler = std::make_shared<Handler>(std::move(handler));

        return DataStream<T>(
            [upstream, newHandler](ObserverPtr<T> sink) {
                auto wrapped =
                    std::make_shared<SimpleErrorObserver<T, Handler>>(
                        sink, *newHandler
                    );
                upstream(wrapped);
            },
            std::move(name)
        );
    }

        // ------------------------------------------------------------------------
    // 3.9. subscribe — Overload 1: ObserverPtr<T>
    // ------------------------------------------------------------------------
    void subscribe(ObserverPtr<T> sink) const {
        if (!pipeline_) {
            throw std::runtime_error("DataStream::subscribe: pipeline rỗng");
        }
        pipeline_(std::move(sink));
    }

    // ------------------------------------------------------------------------
    // 3.10. subscribe — Overload 2: SinkPtr<T> = shared_ptr<ISink<T>>
    // ------------------------------------------------------------------------
    void subscribe(const SinkPtr<T>& sink) const {
        subscribe(ObserverPtr<T>(sink));
    }

    // ------------------------------------------------------------------------
    // 3.10b. subscribe — Overload 2b: shared_ptr<DerivedSink<T>>
    //        Dùng cho shared_ptr<NullSink<int>>, shared_ptr<ConsoleSink<...>>, ...
    // ------------------------------------------------------------------------
    template <typename SinkType,
              typename = std::enable_if_t<
                  std::is_base_of_v<ISink<T>, SinkType>
                  && !std::is_same_v<SinkType, ISink<T>>>>
    void subscribe(const std::shared_ptr<SinkType>& sink) const {
        subscribe(ObserverPtr<T>(std::static_pointer_cast<ISink<T>>(sink)));
    }

    // ------------------------------------------------------------------------
    // 3.11. subscribe — Overload 3: 3 lambda callback
    // ------------------------------------------------------------------------
    template <typename OnNext, typename OnError, typename OnComplete,
              typename = std::enable_if_t<
                  std::is_invocable_v<OnNext, const T&>
                  && !std::is_base_of_v<ISink<T>, OnNext>
                  && !std::is_same_v<OnNext, ObserverPtr<T>>>>
    void subscribe(OnNext onNext, OnError onError, OnComplete onComplete) const {
        auto observer = std::make_shared<LambdaObserver<T>>(
            std::move(onNext), std::move(onError), std::move(onComplete)
        );
        subscribe(ObserverPtr<T>(observer));
    }

    // ------------------------------------------------------------------------
    // 3.12. subscribe — Overload 4: chỉ onNext (lambda)
    // ------------------------------------------------------------------------
    template <typename OnNext,
              typename = std::enable_if_t<
                  std::is_invocable_v<OnNext, const T&>
                  && !std::is_base_of_v<ISink<T>, OnNext>
                  && !std::is_same_v<OnNext, ObserverPtr<T>>>>
    void subscribe(OnNext onNext) const {
        subscribe(
            std::move(onNext),
            [](const std::exception_ptr& e) {
                try { if (e) std::rethrow_exception(e); }
                catch (const std::exception& ex) {
                    std::cerr << "[DataStream] Error: "
                              << ex.what() << '\n';
                }
            },
            []() { /* no-op */ }
        );
    }

    // ------------------------------------------------------------------------
    // 3.13. subscribeOnAsync
    // ------------------------------------------------------------------------
    DataStream<T> subscribeOnAsync(std::string name = "subscribeOnAsync") const {
        auto upstream = pipeline_;

        return DataStream<T>(
            [upstream](ObserverPtr<T> sink) {
                std::thread([upstream, sink]() {
                    upstream(sink);
                }).detach();
            },
            std::move(name)
        );
    }

    // ------------------------------------------------------------------------
    // 3.14. getters
    // ------------------------------------------------------------------------
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    PipelineFn<T>   pipeline_;
    std::string     name_;
    ErrorHandler<T> errorHandler_;
};

} // namespace reactive