#pragma once

namespace minimal_home {

class SelectionRedraw {
  int renderedSelection = 0;
  bool hasRenderedFrame = false;
  bool selectionPending = false;

 public:
  void requireFullFrame() {
    hasRenderedFrame = false;
    selectionPending = false;
  }

  void selectionChanged() { selectionPending = true; }

  bool canRedrawSelection() const { return hasRenderedFrame && selectionPending; }

  int previousSelection() const { return renderedSelection; }

  void selectionRendered(int index) {
    renderedSelection = index;
    selectionPending = false;
  }

  void fullFrameRendered(int index) {
    renderedSelection = index;
    hasRenderedFrame = true;
    selectionPending = false;
  }
};

}  // namespace minimal_home
