"""Compile production Settings event handling with Runtime/adapter/SDK transport fakes.

Only UI drawing is omitted: loop, signature and the status portion of refresh
are extracted unchanged. This exercises an event published after the UI has
already observed the SDK's disconnected flags, without a signature change.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ.get("BLETURNER_SDK_ROOT", ROOT / "freeink-sdk"))
source = (ROOT / "src/activities/settings/BleSettingsActivity.cpp").read_text(encoding="utf-8")


def method(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


refresh = method("void BleSettingsActivity::refresh(")
refresh = refresh[:refresh.index("  // Labels first:")] + "}\n"
has_consumer = "bool BleSettingsActivity::consumeConnectFailure()" in source
declaration = "void refresh(const bool connectionFailed = false);" if has_consumer else "void refresh();"
consumer = method("bool BleSettingsActivity::consumeConnectFailure()") if has_consumer else ""
harness = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "BlePageTurner.h"
#include "Runtime.h"
#include "fake_ble.h"
#define tr(id) #id
unsigned errorLogs = 0;
#define LOG_ERR(...) (++errorLogs)
struct Settings {
  bleturner::Config ble;
  unsigned saves = 0;
  void saveToFile() { ++saves; }
} SETTINGS;
struct MappedInputManager {
  enum class Button { Confirm };
  bool wasLongPressed(Button, unsigned long) const { return false; }
};
struct UiListActivity { void loop() {} };
struct BleSettingsActivity;
struct RenderLock { explicit RenderLock(BleSettingsActivity&) {} };
struct BleSettingsActivity : UiListActivity {
  enum class View { Menu, Mapping };
  View view = View::Menu;
  static constexpr unsigned long CLEAR_HOLD_MS = 700;
  MappedInputManager mappedInput;
  struct { int selected = 0; } nav;
  unsigned long lastPollMs = 0;
  uint32_t shownSignature = 0;
  bleturner::Action learnAction = bleturner::Action::None;
  char status[96]{};
  unsigned updates = 0;
  void clearMapping(int) {}
  void stepLearning() {}
  void requestUpdate() { ++updates; }
  bool consumeConnectFailure();
  uint32_t signature() const;
  void loop();
''' + declaration + r'''
};
''' + consumer + "\n" + method("void BleSettingsActivity::loop()") + "\n" + method(
    "uint32_t BleSettingsActivity::signature() const") + "\n" + refresh + r'''
constexpr const char* kOld = "AA:BB:CC:DD:EE:01";
constexpr const char* kNew = "AA:BB:CC:DD:EE:02";
constexpr uint8_t kMap[] = {0x05,0x01,0x09,0x06,0xA1,0x01,0x05,0x07,0x19,0xE0,0x29,0xE7,
  0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,0x95,0x01,0x75,0x08,0x81,0x03,
  0x19,0x00,0x29,0x65,0x25,0x65,0x75,0x08,0x95,0x06,0x81,0x00,0xC0};
bleturner::Heap heap() { return {160000,65536}; }
bool yes() { return true; }
bool no() { return false; }
bool deliver(bleturner::Action) { return false; }
void restart() {}
void hold(bool) {}
void log(bool,const char*,va_list) {}
const bleturner::Host host{heap,yes,deliver,yes,no,restart,hold,log,nullptr};
unsigned checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if(!(value)) { ++failures; std::printf("FAIL line %d: %s\n",__LINE__,#value); } } while(0)
void reset() {
  fakeble::resetWorld();
  bleturner::detail::resetForTests();
  SETTINGS = Settings{};
  SETTINGS.ble.enabled=1;
  SETTINGS.ble.pick=0;
  std::strcpy(SETTINGS.ble.peerAddr,kOld);
  errorLogs=0;
  bleturner::begin(host,SETTINGS.ble);
  int map=fakeble::addCharacteristic(0x2A4B,true,false,false);
  fakeble::setValue(map,kMap,sizeof(kMap));
  fakeble::addInputReport();
  CHECK(bleturner::switchOn());
}
void pass(BleSettingsActivity& ui) {
  fakeble::advanceMillis(250);
  ui.loop();
}
void preparePublicationGap(BleSettingsActivity& ui) {
  fakeble::failConnects(true);
  CHECK(bleturner::pair(kNew,"New remote"));
  CHECK(fakeble::waitForWorkerIdle());
  // Retain Runtime's pending request, but model the instant before a callback
  // publishes its failure latch: the two link flags already read false.
  char setupFailure[48];
  CHECK(fakeble::host().takeConnectFailure(setupFailure,sizeof(setupFailure)));
  fakeble::host().onLinkDown();
  ui.refresh();
  CHECK(!bleturner::status().connected && !bleturner::status().connecting);
}
int main() {
  reset();
  BleSettingsActivity ui;
  preparePublicationGap(ui);
  const auto signature=ui.signature();
  const auto attempts=fakeble::connectCalls();
  fakeble::host().onConnectFailed("Published after signature");
  CHECK(ui.signature()==signature);
  pass(ui);
  CHECK(errorLogs==1);
  CHECK(std::strcmp(ui.status,"STR_BT_FAILED")==0);
  CHECK(ui.updates==1);
  for(unsigned i=0;i<8;++i) pass(ui);
  CHECK(errorLogs==1 && ui.updates==1);
  CHECK(fakeble::connectCalls()==attempts);
  char failure[48];
  CHECK(!bleturner::takeConnectFailure(failure,sizeof(failure)));
  // A fresh explicit retry must clear the old result and save only the new link.
  fakeble::failConnects(false);
  CHECK(bleturner::pair(kNew,"New remote"));
  CHECK(fakeble::waitForWorkerIdle());
  pass(ui);
  CHECK(SETTINGS.saves==1);
  CHECK(std::strcmp(SETTINGS.ble.peerAddr,kNew)==0);
  CHECK(bleturner::status().connected);
  CHECK(errorLogs==1);
  CHECK(bleturner::beforeSleep(2000));

  reset();
  BleSettingsActivity success;
  preparePublicationGap(success);
  pass(success); // both flags false alone are not a failure
  CHECK(errorLogs==0 && SETTINGS.saves==0);
  fakeble::failConnects(false);
  CHECK(fakeble::connectTo(kNew));
  pass(success);
  CHECK(SETTINGS.saves==1 && bleturner::status().connected);
  CHECK(std::strcmp(SETTINGS.ble.peerAddr,kNew)==0);
  CHECK(errorLogs==0);
  // Disable must consume an already-published event without showing an enabled status.
  CHECK(bleturner::switchOff());
  SETTINGS.ble.enabled=0;
  success.refresh();
  fakeble::host().onConnectFailed("Late event while disabled");
  const auto offSignature=success.signature();
  pass(success);
  CHECK(success.signature()==offSignature);
  CHECK(!bleturner::takeConnectFailure(failure,sizeof(failure)));
  CHECK(std::strcmp(success.status,"STR_STATE_OFF")==0);
  CHECK(bleturner::beforeSleep(2000));
  std::printf("Settings production loop/signature/status + Runtime/SDK: %u checks, %u failed\n",checks,failures);
  return failures ? 1 : 0;
}
'''

with tempfile.TemporaryDirectory(prefix="ble-settings-failure-") as directory:
    cpp = Path(directory) / "settings.cpp"
    exe = Path(directory) / "settings.exe"
    cpp.write_text(harness, encoding="utf-8")
    ble = SDK / "libs/network/BleKeyboardHost"
    command = [os.environ.get("CXX", "c++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
               "-Wno-unused-variable", "-pthread", "-DBLETURNER_TESTING", "-DFREEINK_CAP_BLE_HID_HOST=1"]
    for include in [ROOT / "lib/BlePageTurner/test/sdk_stubs", ble / "test/host/stubs",
                    ble / "test/host", ble / "include", ble / "src",
                    ROOT / "lib/BlePageTurner/include", ROOT / "lib/BlePageTurner/src"]:
        command += ["-I", str(include)]
    command += [str(cpp), str(ROOT / "lib/BlePageTurner/src/Runtime.cpp"),
                str(ROOT / "lib/BlePageTurner/src/SdkRadio.cpp"), str(ble / "src/BleKeyboardHost.cpp"),
                str(ble / "src/HidKeymap.cpp"), str(ble / "test/host/fake_ble.cpp"), "-o", str(exe)]
    subprocess.run(command, check=True)
    subprocess.run([str(exe)], check=True)
