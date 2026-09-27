#include "Arduino.h"

namespace arduino_shim {
unsigned long nowMillis = 0;
} // namespace arduino_shim

unsigned long millis()
{
    return arduino_shim::nowMillis;
}
