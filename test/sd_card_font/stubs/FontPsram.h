#pragma once

// Host-test stub for FreeInkFont's PSRAM-preferring helpers (FontPsram.h):
// plain heap on the host, same contracts (psramNewArray is nothrow, nullptr
// on OOM). Allocation goes through nothrow operator new[] so the test's
// fail-next-allocation override (see SdCardFontTest.cpp) can inject OOM.
#include <algorithm>
#include <array>
#include <cassert>
#include <new>
#include <vector>

namespace fontTestAlloc {
struct Entry {
  void* pointer = nullptr;
  size_t bytes = 0;
};
inline std::array<Entry, 256> entries;
inline size_t liveBytes = 0;
inline size_t peakBytes = 0;
inline void allocated(void* p, size_t bytes) {
  if (!p) return;
  for (auto& e : entries)
    if (!e.pointer) {
      e = {p, bytes};
      liveBytes += bytes;
      peakBytes = std::max(peakBytes, liveBytes);
      return;
    }
  assert(false && "font allocation ledger full");
}
inline void released(void* p) {
  if (!p) return;
  for (auto& e : entries)
    if (e.pointer == p) {
      liveBytes -= e.bytes;
      e = {};
      return;
    }
  assert(false && "unpaired font allocation");
}
}  // namespace fontTestAlloc
namespace freeink {
namespace font {

template <typename T>
using PsramVector = std::vector<T>;

template <typename T>
T* psramNewArray(std::size_t n) {
  auto* p = new (std::nothrow) T[n ? n : 1];
  fontTestAlloc::allocated(p, (n ? n : 1) * sizeof(T));
  return p;
}

template <typename T>
void psramDeleteArray(T* p) {
  fontTestAlloc::released(p);
  delete[] p;
}

}  // namespace font
}  // namespace freeink
