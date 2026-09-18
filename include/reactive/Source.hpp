// include/reactive/Source.hpp
#pragma once

#include <optional>

namespace reactive {

template <typename T>
class Source {
public:
    virtual ~Source() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;
    [[nodiscard]] virtual std::optional<T> tryGet() = 0;
};

} // namespace reactive