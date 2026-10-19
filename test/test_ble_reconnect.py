"""Exercise the production adapter and pinned SDK reconnect methods on a host.

Only clock, transport, scan state and critical sections are faked. This is not a
NimBLE/radio test; the SDK methods are extracted unchanged, not reimplemented.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sdk = (ROOT / "freeink-sdk/libs/network/BleKeyboardHost/src/BleKeyboardHost.cpp").read_text(encoding="utf-8")
adapter = (ROOT / "lib/BlePageTurner/src/SdkRadio.cpp").read_text(encoding="utf-8")


def method(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


constants = "\n".join(re.findall(
    r"^constexpr (?:uint8_t|uint32_t) k(?:ReleaseTimeoutMs|ReconnectBackoffMs|SelectedReconnectAttempts|SelectedReconnectWindowMs) = .*;$",
    sdk, re.M))
assert len(constants.splitlines()) == 4

harness = r"""
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#define portENTER_CRITICAL(mux) ((void)0)
#define portEXIT_CRITICAL(mux) ((void)0)
uint32_t clockMs = 10000;
uint32_t millis() { return clockMs; }
struct Scan { bool active = false; bool isScanning() { return active; } } scan;
struct NimBLEDevice { static Scan* getScan() { return &scan; } };
struct Client { bool isConnected() { return false; } void disconnect() {} };
Client* g_client = nullptr;
bool g_connecting = false, g_autoReconnect = true;
uint32_t g_lastReconnectMs = 0, g_selectedStartedMs = 0, g_lastReportMs = 0;
uint8_t g_reconnectIdx = 0, g_selectedAttemptsLeft = 0;
uint32_t g_lastGenericCode = 0;
char g_selectedAddr[18] = {};
bool g_connTask = true;
""" + constants + "\n" + method(sdk, "void clearSelectedPlanLocked()") + r"""
struct BleKeyboardHost {
  bool begun_ = true, connected_ = false, scanning_ = false;
  uint8_t heldUsage_ = 0, prevKeys_[6] = {}, rawReports_ = 0;
  uint32_t rawCode_ = 0;
  bool rawDropped_ = false, connecting_ = false, connectFailed_ = false;
  uint8_t rawRestCount_ = 0;
  char connAddr_[18] = {}, connectFailure_[48] = {};
  struct Bond { char addr[18] = {}; } bonds_[4];
  uint8_t bondCount_ = 0;
  std::vector<std::string> attempts;
  void pushRawLocked(uint32_t, bool, uint32_t, int, int, int) {}
  bool connectInternal(const char* addr, bool explicitRequest) {
    if (explicitRequest) { clearSelectedPlanLocked(); g_autoReconnect = true; }
    attempts.emplace_back(addr);
    g_connecting = true;
    return true;
  }
  bool connect(const char* addr) { return connectInternal(addr, true); }
  void poll();
  bool armSelectedPeerReconnect(const char* addr);
  void disconnect();
  void onLinkDown();
  void onConnectFailed(const char* reason);
};
""" + "\n".join(method(sdk, signature) for signature in (
    "void BleKeyboardHost::poll()", "bool BleKeyboardHost::armSelectedPeerReconnect(",
    "void BleKeyboardHost::disconnect()", "void BleKeyboardHost::onLinkDown()",
    "void BleKeyboardHost::onConnectFailed(")) + r"""
BleKeyboardHost radio;
namespace bleturner::port {
BleKeyboardHost& hid() { return radio; }
""" + method(adapter, "bool armBondedReconnect(") + r"""
}
constexpr const char* addresses[] = {
  "00:00:00:00:00:01", "00:00:00:00:00:02",
  "00:00:00:00:00:03", "00:00:00:00:00:04"};
void reset(uint8_t count) {
  radio = BleKeyboardHost{};
  radio.bondCount_ = count;
  for (uint8_t i = 0; i < count; ++i) strcpy(radio.bonds_[i].addr, addresses[i]);
  clearSelectedPlanLocked();
  g_autoReconnect = true; g_connecting = false; scan.active = false;
  g_reconnectIdx = 0; g_lastReconnectMs = 0; clockMs = 10000;
}
void failedAttempt() {
  g_connecting = false;
  radio.onConnectFailed("transport failed");
  clockMs += kReconnectBackoffMs + 1;
}
int main() {
  using bleturner::port::armBondedReconnect;
  for (uint8_t count : {0, 1, 2, 4}) {
    reset(count);
    assert(!armBondedReconnect(1, count ? addresses[count - 1] : ""));
    for (int i = 0; i < 8; ++i) { radio.poll(); failedAttempt(); }
    assert(radio.attempts.size() == (count ? 8u : 0u));
    if (count) for (int i = 0; i < 8; ++i) assert(radio.attempts[i] == addresses[i % count]);
  }
  reset(2);
  assert(armBondedReconnect(0, addresses[1]));
  for (int i = 0; i < 10; ++i) { radio.poll(); failedAttempt(); }
  assert(radio.attempts.size() == kSelectedReconnectAttempts);
  for (const auto& addr : radio.attempts) assert(addr == addresses[1]);
  clockMs += kSelectedReconnectWindowMs;
  radio.poll();
  assert(radio.attempts.size() == kSelectedReconnectAttempts); // never falls through to another bond
  radio.connected_ = true; strcpy(radio.connAddr_, addresses[1]);
  radio.onLinkDown(); clockMs += kReconnectBackoffMs + 1; radio.poll();
  assert(radio.attempts.size() == kSelectedReconnectAttempts + 1);
  assert(radio.attempts.back() == addresses[1]); // selected peer drop renews its bounded plan
  reset(2);
  assert(!armBondedReconnect(0, nullptr));
  assert(!armBondedReconnect(0, ""));
  assert(!armBondedReconnect(0, addresses[3])); // unknown choice: unchanged SDK fallback
  radio.poll(); assert(radio.attempts.back() == addresses[0]);
  reset(2);
  assert(!armBondedReconnect(1, nullptr));
  scan.active = true; radio.poll(); assert(radio.attempts.empty());
  scan.active = false; g_connecting = true; radio.poll(); assert(radio.attempts.empty());
  g_connecting = false; radio.connected_ = true; radio.poll(); assert(radio.attempts.empty());
  radio.onLinkDown(); radio.poll(); assert(radio.attempts.size() == 1); // peer dropped link
  failedAttempt(); radio.poll(); assert(radio.attempts.back() == addresses[1]);
  failedAttempt(); radio.disconnect(); radio.poll();
  assert(radio.attempts.size() == 2); // explicit disconnect remains off
  // New begin/end clear the plan and begin enables auto-reconnect (checked below).
  reset(2); assert(armBondedReconnect(0, addresses[1]));
  reset(2); assert(!armBondedReconnect(1, addresses[1])); radio.poll();
  assert(radio.attempts.back() == addresses[0]);
  std::cout << "production adapter + SDK: 0/1/2/4 bonds, selected-only, exhausted plan, scan/connect/link/off passed\n";
}
"""

# Guard the lifecycle assumptions that make omission of a selected plan safe.
begin = method(sdk, "bool BleKeyboardHost::begin(")
end = method(sdk, "bool BleKeyboardHost::end(")
assert "g_autoReconnect = true;" in begin and "clearSelectedPlanLocked();" in begin
assert "clearSelectedPlanLocked();" in end

with tempfile.TemporaryDirectory(prefix="ble-reconnect-") as directory:
    cpp = Path(directory) / "reconnect.cpp"
    exe = Path(directory) / "reconnect.exe"
    cpp.write_text(harness, encoding="utf-8")
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
