// Minimal Arduino shim so the original PidCtrl.cpp builds on the host.
#pragma once

#include <cstddef>

unsigned long millis();

namespace arduino_shim {
/// Time returned by millis(); set by the test.
extern unsigned long nowMillis;
} // namespace arduino_shim
