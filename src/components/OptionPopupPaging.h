#pragma once

#include <cstddef>

namespace optionPopupPaging {

// Keep the interaction buffer bounded. The actual count displayed may be
// smaller when the active UI font leaves less room for dialog rows.
constexpr size_t MAX_VISIBLE_OPTIONS = 16;

struct Window {
  size_t first;
  size_t count;
};

inline size_t visibleCountForHeight(const size_t total, const size_t maximum, const size_t availableHeight,
                                    const size_t buttonHeight, const size_t gap) {
  if (total == 0 || maximum == 0 || buttonHeight == 0) return 0;

  // optionDialogHeight adds one gap before the option group, then one button
  // and one gap per row except the final row.
  const size_t firstRowHeight = gap + buttonHeight;
  if (availableHeight < firstRowHeight) return 0;

  const size_t rowStride = buttonHeight + gap;
  const size_t fitting = 1 + (availableHeight - firstRowHeight) / rowStride;
  return fitting < total ? (fitting < maximum ? fitting : maximum)
                         : (total < maximum ? total : maximum);
}

inline Window windowForSelection(const size_t total, const size_t selected,
                                 const size_t visibleOptions = MAX_VISIBLE_OPTIONS) {
  if (total == 0) return {0, 0};
  const size_t safeSelection = selected < total ? selected : total - 1;
  const size_t pageSize = visibleOptions == 0 ? 1 : visibleOptions;
  const size_t first = (safeSelection / pageSize) * pageSize;
  const size_t remaining = total - first;
  return {first, remaining < pageSize ? remaining : pageSize};
}

inline size_t adjacentSelection(const size_t total, const size_t selected, const bool previous) {
  if (total == 0) return 0;
  const size_t safeSelection = selected < total ? selected : 0;
  if (previous) return safeSelection == 0 ? total - 1 : safeSelection - 1;
  return safeSelection + 1 == total ? 0 : safeSelection + 1;
}

}  // namespace optionPopupPaging
