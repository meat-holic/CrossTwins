#include <gtest/gtest.h>

#include "util/ReaderLineSpacingMigration.h"

TEST(ReaderLineSpacingMigration, PreservesEveryPreviouslyAvailableSpacing) {
  constexpr uint8_t EXPECTED[] = {1, 2, 3, 4, 5, 6};
  for (uint8_t oldIndex = 0; oldIndex < sizeof(EXPECTED); ++oldIndex) {
    EXPECT_EQ(migrateLegacyLineSpacing(oldIndex, 7, 2), EXPECTED[oldIndex]);
  }
}

TEST(ReaderLineSpacingMigration, InvalidLegacyValuesUseTheCurrentDefault) {
  EXPECT_EQ(migrateLegacyLineSpacing(6, 7, 2), 2);
  EXPECT_EQ(migrateLegacyLineSpacing(255, 7, 2), 2);
}
