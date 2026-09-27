#pragma once

#include <chrono>

namespace banana::rtos {

/// Logs, waits `delay` (e.g. so a web or MQTT reply still goes out), then restarts the chip. Blocks the
/// caller.
[[noreturn]] void restartAfter(std::chrono::milliseconds delay);

} // namespace banana::rtos
