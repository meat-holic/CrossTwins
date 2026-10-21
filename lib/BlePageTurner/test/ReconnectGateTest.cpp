#include <gtest/gtest.h>

#include <cstring>

#include "Fakes.h"
#include "Runtime.h"

namespace {
class ReconnectGateTest : public ::testing::Test {
 protected:
  bleturner::Config config;
  void SetUp() override {
    fake::reset();
    config.enabled = 1;
    bleturner::begin(fake::hostFns(), config);
  }
  void pass(unsigned visit = 1) { bleturner::tick(fake::reading(visit)); }
};

TEST_F(ReconnectGateTest, FirstPolicyLoadsUnknownNvsBondsWithNoSelectedAddressOrTables) {
  pass();
  ASSERT_TRUE(bleturner::status().running);
  pass();
  EXPECT_EQ(fake::radio().armedPick, 1);
  EXPECT_TRUE(fake::radio().armedAddr.empty());
  EXPECT_EQ(config.remoteCount, 0);
  EXPECT_NE(bleturner::why(), bleturner::Why::NoBond);
}
TEST_F(ReconnectGateTest, SelectedPolicyWithoutChoiceDoesNotAllocateOrShowConnecting) {
  config.pick = 0;
  pass();
  EXPECT_EQ(fake::radio().creates, 0u);
  EXPECT_EQ(fake::host().yields, 0u);
  EXPECT_EQ(bleturner::why(), bleturner::Why::NoBond);
  EXPECT_EQ(bleturner::linkNote(), bleturner::LinkNote::None);
  bleturner::requestConnect();
  pass();
  EXPECT_EQ(fake::radio().creates, 0u);
  EXPECT_EQ(bleturner::linkNote(), bleturner::LinkNote::Failed);
}
TEST_F(ReconnectGateTest, KnownEmptyBondsDoNotRestartButPairingRemainsAvailable) {
  fake::radio().bonds.clear();
  pass();  // begin is required once to read NVS without assuming settings are the bond registry.
  ASSERT_TRUE(bleturner::switchOff());
  pass(2);
  EXPECT_EQ(fake::radio().creates, 1u);
  EXPECT_EQ(bleturner::why(), bleturner::Why::NoBond);
  EXPECT_TRUE(bleturner::switchOn());  // explicit settings pairing is not gated
}
TEST_F(ReconnectGateTest, ForgettingThePriorityDoesNotHideOtherUnmappedBonds) {
  std::strcpy(config.peerAddr, "7d:de:5c:bd:ae:ca");
  pass();
  ASSERT_TRUE(bleturner::forget(config.peerAddr));
  fake::radio().bonds.erase(fake::radio().bonds.begin());
  ASSERT_TRUE(bleturner::beforeScreenChange());
  pass(2);
  EXPECT_EQ(fake::radio().creates, 2u);
  EXPECT_TRUE(bleturner::status().running);
}
TEST_F(ReconnectGateTest, StaleSelectedAddressIsNotAnEligibleKnownBond) {
  config.pick = 0;
  std::strcpy(config.peerAddr, "AA:BB:CC:DD:EE:FF");
  pass();
  EXPECT_EQ(bleturner::why(), bleturner::Why::Ok);  // running policy does not tear down a radio
  ASSERT_TRUE(bleturner::switchOff());
  pass(2);
  EXPECT_EQ(fake::radio().creates, 1u);
  EXPECT_EQ(bleturner::why(), bleturner::Why::NoBond);
}
TEST_F(ReconnectGateTest, RepeatedEnableDisablePreservesSettingsAndSleepRearms) {
  std::strcpy(config.peerAddr, "11:22:33:44:55:66");
  for (unsigned i = 1; i <= 16; ++i) {
    pass(i);
    ASSERT_TRUE(bleturner::status().running);
    EXPECT_TRUE(bleturner::beforeSleep(2000));
    EXPECT_FALSE(bleturner::status().running);
    EXPECT_FALSE(bleturner::status().stopping);
  }
  EXPECT_EQ(fake::radio().creates, 16u);
  EXPECT_EQ(config.pick, 1);
  EXPECT_STREQ(config.peerAddr, "11:22:33:44:55:66");
  EXPECT_FALSE(fake::host().fullSpeed);
}
TEST_F(ReconnectGateTest, SleepingVisitAndRequestDoNotStartTheRadio) {
  auto scene = fake::reading();
  scene.sleeping = true;
  bleturner::requestConnect();
  bleturner::tick(scene);
  EXPECT_EQ(fake::radio().creates, 0u);
  EXPECT_EQ(fake::host().yields, 0u);
  pass(2);
  EXPECT_EQ(fake::radio().creates, 1u);
}

TEST_F(ReconnectGateTest, ManualDisconnectRequiresAnExplicitRequestAcrossVisits) {
  pass();
  pass();
  fake::radio().connected = true;
  pass();
  bleturner::disconnect();
  pass(2);
  const auto starts = fake::radio().creates;
  ASSERT_TRUE(bleturner::beforeScreenChange());
  pass(3);
  EXPECT_EQ(fake::radio().creates, starts);
  bleturner::requestConnect();
  pass(3);
  EXPECT_EQ(fake::radio().creates, starts + 1);
  EXPECT_TRUE(bleturner::status().running);
}

TEST_F(ReconnectGateTest, SeparateStatusFlagsDoNotCancelAPendingSuccessfulPair) {
  EXPECT_TRUE(bleturner::pair("11:22:33:44:55:66", "New remote"));
  // A legitimate onLinkUp transition can expose both fields as false.
  fake::radio().connected = false;
  fake::radio().connecting = false;
  EXPECT_FALSE(bleturner::service());
  fake::radio().connected = true;
  fake::radio().addr = "11:22:33:44:55:66";
  EXPECT_TRUE(bleturner::service());
  EXPECT_STREQ(config.peerAddr, "11:22:33:44:55:66");
}

}  // namespace
