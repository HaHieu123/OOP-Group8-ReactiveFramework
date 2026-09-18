#include <reactive/Models.hpp>
#include <cassert>
#include <iostream>

using namespace reactive;

int main() {
    // Test 1: Sensor validity
    Sensor s{1, "Temp-01", "Room A", 10.0, 60.0};
    assert(s.isValid());
    assert(s.isOutOfRange(70.0));
    assert(!s.isOutOfRange(30.0));

    // Test 2: Reading validity
    Reading r{1, std::chrono::system_clock::now(), 65.0, 45.0, 1013.0};
    assert(r.isValid());

    // Test 3: Alert generation
    Alert a = Alert::fromReading(r, s);
    assert(a.severity == Severity::CRITICAL);
    assert(a.sensorId == 1);
    std::cout << a.toString() << "\n";

    // Test 4: Serialization
    std::cout << "CSV: "  << r.toCsv()  << "\n";
    std::cout << "JSON: " << r.toJson() << "\n";

    std::cout << "All Models tests passed!\n";
    return 0;
}