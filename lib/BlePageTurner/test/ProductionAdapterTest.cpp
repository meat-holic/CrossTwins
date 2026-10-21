#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "BlePageTurner.h"
#include "RadioPort.h"
#include "Runtime.h"
#include "fake_ble.h"

namespace {
constexpr const char* kFirst = "AA:BB:CC:DD:EE:01";
constexpr const char* kSecond = "AA:BB:CC:DD:EE:02";
constexpr uint8_t kMap[] = {0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
                            0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x03,
                            0x19, 0x00, 0x29, 0x65, 0x25, 0x65, 0x75, 0x08, 0x95, 0x06, 0x81, 0x00, 0xC0};
std::vector<bleturner::Action> actions;
bool heldCpu = false;
bleturner::Heap heap() { return {160000, 65536}; }
bool yes() { return true; }
bool no() { return false; }
bool deliver(bleturner::Action action) {
  actions.push_back(action);
  return true;
}
void restart() {}
void hold(bool held) { heldCpu = held; }
void log(bool, const char*, va_list) {}
const bleturner::Host host{heap, yes, deliver, yes, no, restart, hold, log, nullptr};
class ProductionAdapterTest : public ::testing::Test {
 protected:
  bleturner::Config config;
  int report;
  void SetUp() override {
    fakeble::resetWorld();
    bleturner::detail::resetForTests();
    actions.clear();
    heldCpu = false;
    config.enabled = 1;
    bleturner::begin(host, config);
    int map = fakeble::addCharacteristic(0x2A4B, true, false, false);
    fakeble::setValue(map, kMap, sizeof(kMap));
    report = fakeble::addInputReport();
  }
  void TearDown() override {
    EXPECT_TRUE(bleturner::stopNow(3000));
    EXPECT_FALSE(fakeble::taskDeletedWhileHeld());
  }
  void seedBonds() {
    ASSERT_TRUE(bleturner::switchOn());
    ASSERT_TRUE(fakeble::connectTo(kFirst));
    bleturner::disconnect();
    ASSERT_TRUE(fakeble::connectTo(kSecond));
    ASSERT_EQ(bleturner::bondCount(), 2);
    ASSERT_TRUE(bleturner::switchOff());
    ASSERT_TRUE(bleturner::switchOn());
  }
  void pass(unsigned visit = 1) {
    bleturner::Scene scene{};
    scene.where = bleturner::Where::Reader;
    scene.visit = visit;
    scene.pageShown = true;
    bleturner::tick(scene);
  }
};
TEST_F(ProductionAdapterTest, TaskStartFailureCleansResourcesConsumesMessageAndRetries) {
  fakeble::failNextTaskCreate();
  EXPECT_FALSE(bleturner::switchOn());
  EXPECT_FALSE(fakeble::taskExists());
  EXPECT_FALSE(fakeble::clientExists());
  EXPECT_FALSE(bleturner::status().running);
  EXPECT_FALSE(heldCpu);
  char failure[48];
  ASSERT_TRUE(bleturner::takeConnectFailure(failure, sizeof(failure)));
  EXPECT_STREQ(failure, "No memory for BLE connection task");
  EXPECT_FALSE(bleturner::takeConnectFailure(failure, sizeof(failure)));
  EXPECT_TRUE(bleturner::switchOn());
  EXPECT_TRUE(bleturner::status().running);
}
TEST_F(ProductionAdapterTest, InvalidSelectedAddressNeverFallsBackToAnotherBond) {
  seedBonds();
  config.pick = 0;
  std::strcpy(config.peerAddr, "AA:BB:CC:DD:EE:FF");
  const auto calls = fakeble::connectCalls();
  EXPECT_FALSE(bleturner::port::armBondedReconnect(0, config.peerAddr));
  for (unsigned i = 0; i < 10; ++i) {
    fakeble::advanceMillis(3000);
    bleturner::port::poll();
  }
  EXPECT_EQ(fakeble::connectCalls(), calls);
  EXPECT_FALSE(bleturner::status().connected);
}
TEST_F(ProductionAdapterTest, EmptySelectedAddressNeverFallsBackToAnotherBond) {
  seedBonds();
  const auto calls = fakeble::connectCalls();
  EXPECT_FALSE(bleturner::port::armBondedReconnect(0, nullptr));
  EXPECT_FALSE(bleturner::port::armBondedReconnect(0, ""));
  for (unsigned i = 0; i < 10; ++i) {
    fakeble::advanceMillis(3000);
    bleturner::port::poll();
  }
  EXPECT_EQ(fakeble::connectCalls(), calls);
}
TEST_F(ProductionAdapterTest, SelectedRemoteTurnsPagesAndManualDisconnectStaysOff) {
  seedBonds();
  config.pick = 0;
  std::strcpy(config.peerAddr, kSecond);
  pass();
  // Explicit selected plan is consumed by the SDK worker, not a replacement reconnect model.
  ASSERT_TRUE(fakeble::waitForWorkerIdle());
  ASSERT_TRUE(bleturner::status().connected);
  EXPECT_STREQ(bleturner::linked().addr, kSecond);
  pass();  // discard pre-reader input once on link
  for (unsigned i = 0; i < 8; ++i) {
    uint8_t frame[8] = {0, 0, 0x4E, 0, 0, 0, 0, 0};
    ASSERT_TRUE(fakeble::notify(report, frame, sizeof(frame)));
    pass();
    frame[2] = 0;
    ASSERT_TRUE(fakeble::notify(report, frame, sizeof(frame)));
    pass();
  }
  EXPECT_EQ(actions.size(), 8u);
  for (auto action : actions) EXPECT_EQ(action, bleturner::Action::NextPage);
  bleturner::disconnect();
  const auto calls = fakeble::connectCalls();
  for (unsigned i = 0; i < 5; ++i) {
    fakeble::advanceMillis(3000);
    pass();
    ASSERT_TRUE(fakeble::waitForWorkerIdle());
  }
  EXPECT_EQ(fakeble::connectCalls(), calls);
}
TEST_F(ProductionAdapterTest, RepeatedEnableDisableAndSleepKeepNvsBonds) {
  seedBonds();
  for (unsigned i = 0; i < 12; ++i) {
    EXPECT_TRUE(bleturner::beforeSleep(2000));
    EXPECT_FALSE(heldCpu);
    EXPECT_FALSE(fakeble::taskExists());
    EXPECT_TRUE(bleturner::switchOn());
    EXPECT_EQ(bleturner::bondCount(), 2);
  }
}
TEST_F(ProductionAdapterTest, ExistingChosenLinkSurvivesNewVisitAndPeerDropRearmsOnlyIt) {
  seedBonds();
  ASSERT_TRUE(fakeble::connectTo(kSecond));  // Settings connected it without a selected plan.
  config.pick = 0;
  std::strcpy(config.peerAddr, kSecond);
  const auto calls = fakeble::connectCalls();
  pass();
  pass(2);
  EXPECT_TRUE(bleturner::status().connected);
  EXPECT_EQ(fakeble::connectCalls(), calls);
  fakeble::peerDisconnect();
  fakeble::advanceMillis(3000);
  pass(2);
  ASSERT_TRUE(fakeble::waitForWorkerIdle());
  EXPECT_EQ(fakeble::connectCalls(), calls + 1);
  EXPECT_STREQ(bleturner::linked().addr, kSecond);
  EXPECT_EQ(fakeble::connectAddresses().back(), kSecond);
}

TEST_F(ProductionAdapterTest, SettingsServiceUsesOnlyTheSelectedBond) {
  seedBonds();
  config.pick = 0;
  std::strcpy(config.peerAddr, kSecond);
  fakeble::advanceMillis(6000);
  bleturner::service();
  ASSERT_TRUE(fakeble::waitForWorkerIdle());
  ASSERT_TRUE(bleturner::status().connected);
  EXPECT_STREQ(bleturner::linked().addr, kSecond);
}
TEST_F(ProductionAdapterTest, SettingsForgetChoiceDoesNotConnectRemainingBond) {
  seedBonds();
  config.pick = 0;
  std::strcpy(config.peerAddr, kSecond);
  ASSERT_TRUE(bleturner::forget(kSecond));
  const auto calls = fakeble::connectCalls();
  fakeble::advanceMillis(6000);
  bleturner::service();
  ASSERT_TRUE(fakeble::waitForWorkerIdle());
  EXPECT_EQ(fakeble::connectCalls(), calls);
  EXPECT_FALSE(bleturner::status().connected);
}
TEST_F(ProductionAdapterTest, ExplicitPairMayChangeTheSelectedBondAfterLink) {
  seedBonds();
  config.pick = 0;
  std::strcpy(config.peerAddr, kFirst);
  ASSERT_TRUE(bleturner::pair(kSecond, "Chosen in Settings"));
  ASSERT_TRUE(fakeble::waitForWorkerIdle());
  EXPECT_TRUE(bleturner::service());
  EXPECT_STREQ(config.peerAddr, kSecond);
  EXPECT_STREQ(config.peerName, "Chosen in Settings");
}
TEST_F(ProductionAdapterTest, FailedExplicitPairNeverFallsBackOrReplacesTheChoice) {
  seedBonds();
  config.pick = 0;
  std::strcpy(config.peerAddr, kFirst);
  fakeble::failConnects(true);
  ASSERT_TRUE(bleturner::pair("AA:BB:CC:DD:EE:FF", "Offline"));
  ASSERT_TRUE(fakeble::waitForWorkerIdle());
  const auto calls = fakeble::connectCalls();
  for (unsigned i = 0; i < 5; ++i) {
    fakeble::advanceMillis(6000);
    EXPECT_FALSE(bleturner::service());
    ASSERT_TRUE(fakeble::waitForWorkerIdle());
  }
  EXPECT_EQ(fakeble::connectCalls(), calls);
  EXPECT_STREQ(config.peerAddr, kFirst);
  char failure[48];
  EXPECT_TRUE(bleturner::takeConnectFailure(failure, sizeof(failure)));
  EXPECT_FALSE(bleturner::takeConnectFailure(failure, sizeof(failure)));
}

TEST_F(ProductionAdapterTest, SelectedPolicySurvivesScanTimerExpiryAfterExplicitPair) {
  seedBonds();
  config.pick = 0;
  std::strcpy(config.peerAddr, kFirst);
  ASSERT_TRUE(bleturner::pair(kSecond, "B"));
  ASSERT_TRUE(fakeble::waitForWorkerIdle());
  ASSERT_TRUE(bleturner::service());
  bleturner::scan(15000);
  fakeble::peerDisconnect();
  const auto calls = fakeble::connectCalls();
  fakeble::advanceMillis(6000);
  EXPECT_FALSE(bleturner::service());
  EXPECT_EQ(fakeble::connectCalls(), calls);
  // The radio timer expires without updating the SDK's cached scanning_ field.
  NimBLEDevice::getScan()->stop();
  fakeble::advanceMillis(9001);
  EXPECT_FALSE(bleturner::service());
  ASSERT_TRUE(fakeble::waitForWorkerIdle());
  EXPECT_EQ(fakeble::connectCalls(), calls + 1);
  EXPECT_EQ(fakeble::connectAddresses().back(), kSecond);
}

}  // namespace
