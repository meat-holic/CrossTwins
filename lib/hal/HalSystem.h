#pragma once

#include <cstdint>
#include <string>

namespace HalSystem {
struct StackFrame {
  uint32_t sp;
  uint32_t spp[8];
};

void begin();

// Dump panic info to SD card if necessary
void checkPanic();
void clearPanic();

std::string getPanicInfo(bool full = false);
bool isRebootFromPanic();
// Reads the SoC's internal sensor (not ambient temperature). Returns false when
// unsupported or on failure; output is only written after a successful read and cleanup.
bool readInternalTemperatureCelsius(float& celsius);
}  // namespace HalSystem
