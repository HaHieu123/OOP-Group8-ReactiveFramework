# ============================================================================
# CMakeLists.txt — Reactive Stream Processing Framework
# Yêu cầu: CMake >= 3.20, C++17 hoặc C++20
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# 1. Thông tin dự án
# ---------------------------------------------------------------------------
project(ReactiveStreamFramework
    VERSION     1.0.0
    DESCRIPTION "Reactive Object-Oriented Stream Processing Pipeline"
    LANGUAGES   CXX
)

# ---------------------------------------------------------------------------
# 2. Chuẩn C++ và cờ biên dịch
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)             # Tắt GNU extensions

# Export compile_commands.json cho IDE (clangd, VS Code)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# 3. Cờ cảnh báo (Warning flags) — nghiêm ngặt
# ---------------------------------------------------------------------------
if (MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion
        -Wformat=2
    )
endif()

# ---------------------------------------------------------------------------
# 4. Chế độ build (Debug / Release)
# ---------------------------------------------------------------------------
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Release: tối ưu hóa cao (phân biệt MSVC vs GCC/Clang)
if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG   "/Od /Zi")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
    set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer")
endif()

# ---------------------------------------------------------------------------
# 5. Thư viện header-only (INTERFACE)
# ---------------------------------------------------------------------------
add_library(reactive_framework INTERFACE)
target_include_directories(reactive_framework INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(reactive_framework INTERFACE cxx_std_20)

# Liên kết thread (bắt buộc cho std::thread)
find_package(Threads REQUIRED)
target_link_libraries(reactive_framework INTERFACE Threads::Threads)

# ---------------------------------------------------------------------------
# 6. Executable chính (Demo)
# ---------------------------------------------------------------------------
add_executable(reactive_demo
    src/main.cpp
)
target_link_libraries(reactive_demo PRIVATE reactive_framework)

# ---------------------------------------------------------------------------
# 7. Unit tests (tùy chọn, bật bằng -DBUILD_TESTS=ON)
# ---------------------------------------------------------------------------
option(BUILD_TESTS "Build unit tests" OFF)
if (BUILD_TESTS)
    enable_testing()

    # Test Models (BƯỚC 4)
    add_executable(test_models tests/test_models.cpp)
    target_link_libraries(test_models PRIVATE reactive_framework)
    add_test(NAME models COMMAND test_models)

    # Test Sink (BƯỚC 5)
    add_executable(test_sink tests/test_sink.cpp)
    target_link_libraries(test_sink PRIVATE reactive_framework)
    add_test(NAME sink COMMAND test_sink)

    # Test DataStream Basic (BƯỚC 6)
    add_executable(test_datastream_basic tests/test_datastream_basic.cpp)
    target_link_libraries(test_datastream_basic PRIVATE reactive_framework)
    add_test(NAME datastream_basic COMMAND test_datastream_basic)

    # Test DataStream Advanced (BƯỚC 7)
    add_executable(test_datastream_advanced tests/test_datastream_advanced.cpp)
    target_link_libraries(test_datastream_advanced PRIVATE reactive_framework)
    add_test(NAME datastream_advanced COMMAND test_datastream_advanced)

    # Các test sau CHƯA VIẾT
    # add_executable(test_observer   tests/test_observer.cpp)
    # add_executable(test_datastream tests/test_datastream.cpp)
    # target_link_libraries(test_observer   PRIVATE reactive_framework)
    # target_link_libraries(test_datastream PRIVATE reactive_framework)
    # add_test(NAME observer   COMMAND test_observer)
    # add_test(NAME datastream COMMAND test_datastream)
endif()

# ---------------------------------------------------------------------------
# 8. Benchmarks (tùy chọn, bật bằng -DBUILD_BENCHMARKS=ON)
# ---------------------------------------------------------------------------
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
if (BUILD_BENCHMARKS)
    add_executable(bench_throughput benchmarks/bench_throughput.cpp)
    target_link_libraries(bench_throughput PRIVATE reactive_framework)
endif()

# ---------------------------------------------------------------------------
# 9. In thông tin cấu hình
# ---------------------------------------------------------------------------
message(STATUS "============================================")
message(STATUS "Reactive Stream Framework v${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build tests:    ${BUILD_TESTS}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "============================================")