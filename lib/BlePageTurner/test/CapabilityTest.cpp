#include <gtest/gtest.h>

#include "RadioPolicy.h"

TEST(BleCapabilityTest, SelectsTheMcuWithoutAssumingExternalBlePools) {
#ifdef TEST_S3
  EXPECT_EQ(&bleturner::activeProfile(), &bleturner::kS3InternalProfile);
#else
  EXPECT_EQ(&bleturner::activeProfile(), &bleturner::kX3Profile);
#endif
  const auto& profile = bleturner::activeProfile();
  EXPECT_EQ(profile.minimumLargest, 32768u);
  EXPECT_EQ(profile.startFreeBytes, 81920u);
  EXPECT_EQ(profile.homeStartLargest, 0u);
  bleturner::RadioInputs input{};
  input.enabled = input.pageShown = input.hasBond = true;
  input.where = bleturner::Where::Home;
  EXPECT_EQ(bleturner::radioVerdict(input), bleturner::Why::NotReading);
}
