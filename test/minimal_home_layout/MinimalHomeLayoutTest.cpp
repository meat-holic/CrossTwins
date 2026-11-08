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
}

TEST(MinimalHomeLayout, ImageFitPreservesAspectRatioAndDoesNotUpscale) {
  const auto portrait = minimal_home::fitImage(Rect{20, 100, 360, 400}, 800, 1200);
  EXPECT_EQ(portrait.width, 266);
  EXPECT_EQ(portrait.height, 400);
  EXPECT_EQ(portrait.x, 67);
  EXPECT_EQ(portrait.y, 100);

  const auto small = minimal_home::fitImage(Rect{20, 100, 360, 400}, 100, 80);
  EXPECT_EQ(small.width, 100);
  EXPECT_EQ(small.height, 80);
  EXPECT_EQ(small.x, 150);
  EXPECT_EQ(small.y, 260);
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
  EXPECT_EQ(minimal_home::fitImage(Rect{0, 0, 0, 20}, 100, 100).width, 0);
  EXPECT_EQ(minimal_home::fitImage(Rect{0, 0, 20, 20}, 0, 100).height, 0);
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
