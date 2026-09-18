// tests/test_sink.cpp
#include <reactive/Models.hpp>
#include <reactive/Sink.hpp>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

using namespace reactive;

int main() {
    // ========================================================================
    // Test 1: ConsoleSink với Alert
    // ========================================================================
    auto consoleSink = makeConsoleSink<Alert>(
        [](const Alert& a) { return a.toString(); }
    );

    Alert a1{1, std::chrono::system_clock::now(),
             Severity::CRITICAL, "Nhiệt độ vượt ngưỡng"};
    Alert a2{2, std::chrono::system_clock::now(),
             Severity::WARNING, "Độ ẩm cao"};

    consoleSink->onNext(a1);
    consoleSink->onNext(a2);
    consoleSink->onComplete();

    // ========================================================================
    // Test 2: FileSink CSV với Reading
    // ========================================================================
    const std::string csvPath = "test_output.csv";
    std::remove(csvPath.c_str());
    {
        auto fileSink = makeCsvFileSink<Reading>(csvPath);
        for (int i = 1; i <= 5; ++i) {
            Reading r{static_cast<std::uint32_t>(i),
                      std::chrono::system_clock::now(),
                      25.0 + i, 45.0, 1013.0};
            fileSink->onNext(r);
        }
        fileSink->onComplete();
    }

    {
        std::ifstream check(csvPath);
        assert(check.is_open());
        std::string header;
        std::getline(check, header);
        assert(header == "sensorId,timestamp,temperature,humidity,pressure");

        int lineCount = 0;
        std::string line;
        while (std::getline(check, line)) {
            if (!line.empty()) ++lineCount;
        }
        assert(lineCount == 5);
    }
    std::cout << "✅ FileSink đã ghi 5 bản ghi vào " << csvPath << "\n";

    // ========================================================================
    // Test 3: TeeSink
    // ========================================================================
    const std::string teePath = "test_tee.csv";
    std::remove(teePath.c_str());
    {
        auto tee = std::make_shared<TeeSink<Reading>>(
            std::vector<SinkPtr<Reading>>{
                makeConsoleSink<Reading>(nullptr, /*useColor=*/false),
                makeCsvFileSink<Reading>(teePath)
            }
        );

        Reading r{99, std::chrono::system_clock::now(), 30.0, 50.0, 1013.0};
        tee->onNext(r);
        tee->onComplete();
    }
    std::cout << "✅ TeeSink đã ghi ra cả console và " << teePath << "\n";

    // ========================================================================
    // Test 4: DatabaseSink với mock connection
    // ========================================================================
    class MockDbConnection : public IDbConnection {
    public:
        bool execute(const std::string& sql) override {
            executedSql_.push_back(sql);
            return true;
        }
        bool beginTransaction() override { return true; }
        bool commit() override { return true; }
        bool rollback() override { return true; }
        bool isConnected() const override { return true; }

        std::vector<std::string> executedSql_;
    };

    auto mockConn = std::make_shared<MockDbConnection>();
    auto dbSink = makeDatabaseSink<Reading>(
        mockConn,
        "readings",
        [](const Reading& r) { return r.toCsv(); },
        3
    );

    for (int i = 1; i <= 7; ++i) {
        Reading r{static_cast<std::uint32_t>(i),
                  std::chrono::system_clock::now(),
                  25.0, 45.0, 1013.0};
        dbSink->onNext(r);
    }
    dbSink->onComplete();

    assert(mockConn->executedSql_.size() == 7);
    std::cout << "✅ DatabaseSink đã thực thi 7 câu INSERT\n";

    std::cout << "\n🎉 Tất cả Sink tests passed!\n";
    return 0;
}