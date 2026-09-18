// include/reactive/Observer.hpp
#pragma once

#include <memory>
#include <exception>

namespace reactive {

template <typename T>
class Observer {
public:
    virtual ~Observer() = default;

    virtual void onNext(const T& value) = 0;
    virtual void onError(const std::exception_ptr& error) = 0;
    virtual void onComplete() = 0;
};

template <typename T>
using ObserverPtr = std::shared_ptr<Observer<T>>;

} // namespace reactive