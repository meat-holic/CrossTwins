#include <gtest/gtest.h>

#include "src/components/themes/HomeMenuGeometry.h"
#include "src/components/themes/minimal/MinimalHomeLayout.h"
#include "src/components/themes/minimal/MinimalSelectionRedraw.h"

namespace {
using minimal_home::Rect;

bool contains(const Rect outer, const Rect inner) {
  return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.width <= outer.x + outer.width &&
         inner.y + inner.height <= outer.y + outer.height;
}
}  // namespace

TEST(MinimalHomeLayout, AlignmentKeepsRowsInsideTheMenuBand) {
  constexpr int menuX = 24;
  constexpr int menuWidth = 432;
  constexpr int sidePadding = 18;
  constexpr int rowWidth = 170;

  EXPECT_EQ(home_menu::alignedRowX(menuX, menuWidth, sidePadding, rowWidth, home_menu::Alignment::LEFT), 42);
  EXPECT_EQ(home_menu::alignedRowX(menuX, menuWidth, sidePadding, rowWidth, home_menu::Alignment::CENTER), 155);
  EXPECT_EQ(home_menu::alignedRowX(menuX, menuWidth, sidePadding, rowWidth, home_menu::Alignment::RIGHT), 268);

  EXPECT_EQ(home_menu::alignedTextX(42, 170, 100, 12, home_menu::Alignment::LEFT), 54);
  EXPECT_EQ(home_menu::alignedTextX(42, 170, 100, 12, home_menu::Alignment::CENTER), 77);
  EXPECT_EQ(home_menu::alignedTextX(42, 170, 100, 12, home_menu::Alignment::RIGHT), 100);
}

TEST(MinimalHomeLayout, CenterCropUsesTheTargetAspectAndLeavesExactImagesOneToOne) {
  const auto exact = minimal_home::centerCropImage(Rect{3, 62, 474, 695}, 474, 695);
  EXPECT_EQ(exact.x, 3);
  EXPECT_EQ(exact.y, 62);
  EXPECT_EQ(exact.width, 474);
  EXPECT_EQ(exact.height, 695);
  EXPECT_FLOAT_EQ(exact.cropX, 0.0f);
  EXPECT_FLOAT_EQ(exact.cropY, 0.0f);

  const auto square = minimal_home::centerCropImage(Rect{20, 100, 360, 400}, 800, 800);
  EXPECT_GT(square.cropX, 0.0f);
  EXPECT_FLOAT_EQ(square.cropY, 0.0f);
  EXPECT_EQ(square.x, 20);
  EXPECT_EQ(square.y, 100);
  EXPECT_EQ(square.width, 360);
  EXPECT_EQ(square.height, 400);
}

TEST(MinimalHomeLayout, SmallerImagesStayOneToOneAndAreCentered) {
  const auto small = minimal_home::centerCropImage(Rect{20, 100, 360, 400}, 90, 100);
  EXPECT_EQ(small.width, 90);
  EXPECT_EQ(small.height, 100);
  EXPECT_EQ(small.x, 155);
  EXPECT_EQ(small.y, 250);
  EXPECT_FLOAT_EQ(small.cropX, 0.0f);
  EXPECT_FLOAT_EQ(small.cropY, 0.0f);
}

TEST(MinimalHomeLayout, TallerSourcesAreCenterCroppedWithoutStretching) {
  const auto crop = minimal_home::centerCropImage(Rect{20, 100, 360, 400}, 1000, 2000);
  EXPECT_FLOAT_EQ(crop.cropX, 0.0f);
  EXPECT_GT(crop.cropY, 0.0f);
  EXPECT_EQ(crop.x, 20);
  EXPECT_EQ(crop.y, 100);
  EXPECT_EQ(crop.width, 360);
  EXPECT_EQ(crop.height, 400);
  EXPECT_GE(crop.x, 20);
  EXPECT_LE(crop.x + crop.width, 380);
  EXPECT_GE(crop.y, 100);
  EXPECT_LE(crop.y + crop.height, 500);
}

TEST(MinimalHomeLayout, PortraitAndLandscapeBandsStayWithinTheSafeArea) {
  const Rect safeAreas[] = {{8, 8, 464, 744}, {40, 8, 752, 464}, {8, 8, 752, 464}, {8, 40, 752, 464}};
  for (const Rect safe : safeAreas) {
    const auto layout = minimal_home::calculateLayout(safe, safe.y + 54, 46, 5, 42, 6, 12);
    EXPECT_TRUE(contains(safe, layout.background));
    EXPECT_TRUE(contains(layout.background, layout.recent));
    EXPECT_TRUE(contains(layout.background, layout.menu));
    EXPECT_LE(layout.recent.y + layout.recent.height, layout.menu.y);
  }
}

TEST(MinimalHomeLayout, EmptyOrInvalidImageTargetHasZeroPlacement) {
  EXPECT_EQ(minimal_home::centerCropImage(Rect{0, 0, 0, 20}, 100, 100).width, 0);
  EXPECT_EQ(minimal_home::centerCropImage(Rect{0, 0, 20, 20}, 0, 100).height, 0);
}

TEST(MinimalHomeLayout, RecommendedX3X4ImageAreasMatchSafeAreaAndFooterGeometry) {
  const auto x4Portrait = minimal_home::calculateLayout(Rect{3, 9, 474, 748}, 62, 48, 4, 42, 6, 18);
  EXPECT_EQ(x4Portrait.background.x, 3);
  EXPECT_EQ(x4Portrait.background.y, 62);
  EXPECT_EQ(x4Portrait.background.width, 474);
  EXPECT_EQ(x4Portrait.background.height, 695);

  const auto x4PortraitInverted = minimal_home::calculateLayout(Rect{3, 43, 474, 748}, 62, 48, 4, 42, 6, 18);
  EXPECT_EQ(x4PortraitInverted.background.x, 3);
  EXPECT_EQ(x4PortraitInverted.background.y, 62);
  EXPECT_EQ(x4PortraitInverted.background.width, 474);
  EXPECT_EQ(x4PortraitInverted.background.height, 729);

  const auto x4Landscape = minimal_home::calculateLayout(Rect{43, 3, 748, 474}, 62, 48, 4, 42, 6, 18);
  EXPECT_EQ(x4Landscape.background.x, 43);
  EXPECT_EQ(x4Landscape.background.y, 62);
  EXPECT_EQ(x4Landscape.background.width, 748);
  EXPECT_EQ(x4Landscape.background.height, 415);

  const auto x4LandscapeCounterClockwise = minimal_home::calculateLayout(Rect{9, 3, 748, 474}, 62, 48, 4, 42, 6, 18);
  EXPECT_EQ(x4LandscapeCounterClockwise.background.x, 9);
  EXPECT_EQ(x4LandscapeCounterClockwise.background.y, 62);
  EXPECT_EQ(x4LandscapeCounterClockwise.background.width, 748);
  EXPECT_EQ(x4LandscapeCounterClockwise.background.height, 415);

  const auto x3Portrait = minimal_home::calculateLayout(Rect{3, 9, 522, 740}, 62, 48, 4, 42, 6, 18);
  EXPECT_EQ(x3Portrait.background.x, 3);
  EXPECT_EQ(x3Portrait.background.y, 62);
  EXPECT_EQ(x3Portrait.background.width, 522);
  EXPECT_EQ(x3Portrait.background.height, 687);

  const auto x3PortraitInverted = minimal_home::calculateLayout(Rect{3, 43, 522, 740}, 62, 48, 4, 42, 6, 18);
  EXPECT_EQ(x3PortraitInverted.background.x, 3);
  EXPECT_EQ(x3PortraitInverted.background.y, 62);
  EXPECT_EQ(x3PortraitInverted.background.width, 522);
  EXPECT_EQ(x3PortraitInverted.background.height, 721);

  const auto x3Landscape = minimal_home::calculateLayout(Rect{43, 3, 740, 522}, 62, 48, 4, 42, 6, 18);
  EXPECT_EQ(x3Landscape.background.x, 43);
  EXPECT_EQ(x3Landscape.background.y, 62);
  EXPECT_EQ(x3Landscape.background.width, 740);
  EXPECT_EQ(x3Landscape.background.height, 463);

  const auto x3LandscapeCounterClockwise = minimal_home::calculateLayout(Rect{9, 3, 740, 522}, 62, 48, 4, 42, 6, 18);
  EXPECT_EQ(x3LandscapeCounterClockwise.background.x, 9);
  EXPECT_EQ(x3LandscapeCounterClockwise.background.y, 62);
  EXPECT_EQ(x3LandscapeCounterClockwise.background.width, 740);
  EXPECT_EQ(x3LandscapeCounterClockwise.background.height, 463);
}

TEST(MinimalSelectionRedraw, RequiresACompleteFrameBeforePartialSelectionPaint) {
  minimal_home::SelectionRedraw state;
  state.requireFullFrame();
  state.selectionChanged();
  EXPECT_FALSE(state.canRedrawSelection());

  state.fullFrameRendered(2);
  EXPECT_FALSE(state.canRedrawSelection());
  EXPECT_EQ(state.previousSelection(), 2);
}

TEST(MinimalSelectionRedraw, CoalescedChangesUseTheLastPaintedSelection) {
  minimal_home::SelectionRedraw state;
  state.fullFrameRendered(0);
  state.selectionChanged();
  state.selectionChanged();

  EXPECT_TRUE(state.canRedrawSelection());
  EXPECT_EQ(state.previousSelection(), 0);
  state.selectionRendered(2);
  EXPECT_FALSE(state.canRedrawSelection());
  EXPECT_EQ(state.previousSelection(), 2);
}
