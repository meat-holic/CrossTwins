#pragma once
#include <I18n.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "GfxRenderer.h"
#include "MappedInputManager.h"
#include "components/OptionPopupPaging.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

// Modal option picker drawn over the current screen (no clear) via
// fui::optionDialog. Touch hit-testing is the SDK's InteractionBuffer: each
// render registers the option buttons (plus a chrome guard rect) on the render
// task, and handleInput routes touch snapshots against that table on the loop
// task, gated by the uiReady handshake (same pattern as UiListActivity).
// render() builds into InteractionBuffer's non-published generation
// (beginPublishCycle()) and publishes it only once every hit() call for the
// frame is done (publish()), so handleInput()'s routePublished()/
// publishedData() reads on the loop task always see a complete table, never
// one render is mid-rebuilding. The publication generation rejects an old
// render if show() replaces the popup while that render is in flight.
class OptionPopup {
 public:
  void show(StrId titleId, const StrId* optionIds, int optionCount, int currentIndex,
            std::function<void(int)> onSelect) {
    std::vector<std::string> options;
    options.reserve(optionCount);
    for (int i = 0; i < optionCount; i++) options.emplace_back(I18N.get(optionIds[i]));
    showWithData(I18N.get(titleId), {}, {}, makeOwnedOptions(std::move(options)), currentIndex, std::move(onSelect));
  }

  void show(const char* titleStr, const char* const* options, int optionCount, int currentIndex,
            std::function<void(int)> onSelect) {
    std::vector<std::string> ownedOptions;
    ownedOptions.reserve(optionCount);
    for (int i = 0; i < optionCount; i++) ownedOptions.emplace_back(options[i]);
    showWithData(titleStr ? titleStr : "", {}, {}, makeOwnedOptions(std::move(ownedOptions)), currentIndex,
                 std::move(onSelect));
  }

  // As above, plus a subject line inside the dialog (a book or event title).
  // It wraps to several lines under the caption; the dialog grows to fit.
  void show(const char* titleStr, const char* headlineStr, const char* const* options, int optionCount,
            int currentIndex, std::function<void(int)> onSelect) {
    std::vector<std::string> ownedOptions;
    ownedOptions.reserve(optionCount);
    for (int i = 0; i < optionCount; i++) ownedOptions.emplace_back(options[i]);
    showWithData(titleStr ? titleStr : "", headlineStr ? headlineStr : "", {},
                 makeOwnedOptions(std::move(ownedOptions)), currentIndex, std::move(onSelect));
  }

  // Message dialog: a wrapped body under the (optional) title, like the
  // Wi-Fi forget-network prompt. Pass an empty title for a message-only look.
  void showMessage(const char* titleStr, const char* messageStr, const char* const* options, int optionCount,
                   int currentIndex, std::function<void(int)> onSelect) {
    std::vector<std::string> ownedOptions;
    ownedOptions.reserve(optionCount);
    for (int i = 0; i < optionCount; i++) ownedOptions.emplace_back(options[i]);
    showWithData(titleStr ? titleStr : "", {}, messageStr ? messageStr : "", makeOwnedOptions(std::move(ownedOptions)),
                 currentIndex, std::move(onSelect));
  }

  void show(StrId titleId, const std::vector<std::string>& options, int currentIndex,
            std::function<void(int)> onSelect) {
    showWithData(I18N.get(titleId), {}, {}, copyOptions(options), currentIndex, std::move(onSelect));
  }

  void show(StrId titleId, std::shared_ptr<const std::vector<std::string>> options, int currentIndex,
            std::function<void(int)> onSelect) {
    showWithData(I18N.get(titleId), {}, {}, std::move(options), currentIndex, std::move(onSelect));
  }

  bool handleInput(MappedInputManager& input, const std::function<void()>& requestUpdate) {
    if (!active.load(std::memory_order_acquire)) return false;

    const freeink::ui::InputSnapshot snap = touchSnapshotFrom(input);
    if (snap.touchPressed || snap.touchReleased || snap.touchHeld) {
      // Interactions are registered on the render task; only route once the
      // first render after show() has populated the table (generation handshake).
      if (publishedGeneration.load(std::memory_order_acquire) ==
          interactionGeneration.load(std::memory_order_acquire)) {
        const freeink::ui::ActionEvent event = interactions.routePublished(snap);
        if (event && event.action == ACTION_OPTION) {
          // Tap released on an option: select it, fire, dismiss.
          selectedIndex.store(event.value, std::memory_order_release);
          closeAndSelect();
          haptic_feedback::touchAction();
          requestUpdate();
          return true;
        }
        if (event && event.action == ACTION_CHROME) {
          // Taps on the dialog chrome (title, padding) keep the popup open.
          return true;
        }
        if (snap.touchReleased && snap.touchX >= 0) {
          // Tap released outside the dialog: dismiss without firing. Swipe-end
          // releases arrive with -1,-1 coords and fall through (no dismiss).
          closeWithoutSelect();
          haptic_feedback::touchAction();
          requestUpdate();
          return true;
        }
        if (snap.touchPressed) {
          // Touch-down on an option moves the highlight (route() latched the
          // hit as the active interaction; read it back, no re-hit-testing).
          const int16_t idx = interactions.activeIndex();
          if (idx >= 0) {
            const freeink::ui::Interaction& hit = interactions.publishedData()[idx];
            if (hit.action == ACTION_OPTION && selectedIndex.load(std::memory_order_acquire) != hit.value) {
              selectedIndex.store(hit.value, std::memory_order_release);
              requestUpdate();
            }
          }
        }
      }
      return true;
    }

    return handleButtons(input.wasPressed(MappedInputManager::Button::NavPrevious),
                         input.wasPressed(MappedInputManager::Button::NavNext),
                         input.wasReleased(MappedInputManager::Button::Confirm),
                         input.wasReleased(MappedInputManager::Button::Back), requestUpdate);
  }

  bool handleButtons(const bool previous, const bool next, const bool confirm, const bool back,
                     const std::function<void()>& requestUpdate) {
    if (!active.load(std::memory_order_acquire)) return false;
    int count = 0;
    {
      const auto data = dataSnapshot();
      count = data && data->options ? static_cast<int>(data->options->size()) : 0;
    }
    if (count == 0) return true;
    if (previous) {
      const int current = selectedIndex.load(std::memory_order_acquire);
      selectedIndex.store(static_cast<int>(optionPopupPaging::adjacentSelection(count, current, true)),
                          std::memory_order_release);
      requestUpdate();
      return true;
    } else if (next) {
      const int current = selectedIndex.load(std::memory_order_acquire);
      selectedIndex.store(static_cast<int>(optionPopupPaging::adjacentSelection(count, current, false)),
                          std::memory_order_release);
      requestUpdate();
      return true;
    } else if (confirm) {
      closeAndSelect();
      requestUpdate();
      return true;
    } else if (back) {
      closeWithoutSelect();
      requestUpdate();
      return true;
    }
    return true;
  }

  bool processRender(GfxRenderer& renderer, const MappedInputManager& input) const {
    if (!active.load(std::memory_order_acquire)) return false;
    const auto popupLabels = input.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, popupLabels.btn1, popupLabels.btn2, popupLabels.btn3, popupLabels.btn4);
    render(renderer);
    renderer.displayBuffer();
    return true;
  }

  void render(const GfxRenderer& renderer) const {
    if (!active.load(std::memory_order_acquire)) return;
    const uint32_t renderGeneration = interactionGeneration.load(std::memory_order_acquire);
    namespace fui = freeink::ui;

    // Per-render target: a GfxRendererTarget is a renderer reference plus
    // three font ids, so rebuilding it here is trivially cheap and always
    // tracks the live orientation and uiScale fonts; a target held across
    // show() would stale-bind both after a rotation or scale change.
    fui::GfxRendererTarget target = makeUiTarget(renderer);
    const fui::ThemeTokens& theme = refreshSharedUiThemeTokens(target);
    // Frame stores a const DeviceContext&; keep it in a local that outlives
    // the frame (a deviceContext() temporary would dangle).
    const fui::DeviceContext device = target.deviceContext();
    // Routing happens on the loop task against the member buffer; the frame
    // itself never dispatches, so it gets an empty snapshot.
    const fui::InputSnapshot noInput{};

    // Builds into the generation handleInput()'s routePublished()/
    // publishedData() aren't currently reading, so the loop task never sees
    // this table mid-rebuild — see publish() below and
    // InteractionBuffer::beginPublishCycle().
    interactions.beginPublishCycle();
    fui::Frame<INTERACTION_CAPACITY> frame(target, device, noInput, interactions);

    const auto& metrics = UITheme::getInstance().getMetrics();
    const auto data = dataSnapshot();
    if (!data || !data->options) return;
    const auto& allOptions = *data->options;
    const int totalOptions = static_cast<int>(allOptions.size());
    const int selected = selectedIndex.load(std::memory_order_acquire);
    fui::DialogOption options[MAX_OPTIONS];

    fui::OptionDialogProps props;
    props.title = data->title.empty() ? nullptr : data->title.c_str();
    props.headline = data->headline.empty() ? nullptr : data->headline.c_str();
    char pageIndicator[24];
    if (!data->message.empty()) {
      props.message = data->message.c_str();
      props.messageText.font = fui::GfxRendererTarget::FONT_BODY;
      props.messageText.align = fui::TextAlign::Center;
      props.messageText.maxLines = 6;
    }
    props.options = options;
    props.optionCount = 0;
    props.verticalOptions = true;
    // Touch only: physical buttons stay on the legacy wrap/confirm path above,
    // so the buffer never competes with it for focus/confirm dispatch.
    props.inputMask = fui::InputTouch;
    props.titleText.font = fui::GfxRendererTarget::FONT_BODY;
    props.titleText.bold = true;
    props.titleText.align = fui::TextAlign::Center;
    // Captions like "Remove from Recent Books?" overflow the narrow portrait
    // dialog in one line; let them wrap and the panel grow.
    props.titleText.maxLines = 2;
    props.headlineText.font = fui::GfxRendererTarget::FONT_BODY;
    props.headlineText.align = fui::TextAlign::Center;
    props.headlineText.maxLines = 3;
    props.buttonText.font = fui::GfxRendererTarget::FONT_BODY;
    const int16_t innerPadding = static_cast<int16_t>(metrics.optionPopupInnerPadding);
    props.padding = fui::Insets{innerPadding, innerPadding, innerPadding, innerPadding};
    props.gap = static_cast<int16_t>(metrics.optionPopupItemSpacing);
    // Rounded invert-fill themes use a black pill, not the default gray focus cursor.
    if (theme.listSelectionStyle == fui::SelectionStyle::InvertFill && theme.listRowRadius > 0) {
      props.buttonStyles = fui::defaultButtonStyles();
      props.buttonStyles.focused = props.buttonStyles.selected;
      fui::setStyleRadius(props.buttonStyles, theme.listRowRadius);
    }
    // defaultPopupStyles() has no border, so opt in using the per-theme frame metrics.
    props.styles = fui::defaultPopupStyles();
    props.styles.normal.border = fui::Paint::solid(fui::Color::Black);
    props.styles.normal.borderWidth = static_cast<uint8_t>(metrics.popupFrameThickness);
    props.styles.normal.radius = static_cast<uint8_t>(metrics.popupCornerRadius);
    props.styles.selected = props.styles.normal;
    props.styles.focused = props.styles.normal;
    props.styles.active = props.styles.normal;
    props.styles.disabled = props.styles.normal;
    props.buttonHeight =
        fui::clampI16(target.lineHeight(fui::GfxRendererTarget::FONT_BODY) + metrics.optionPopupSelectionVPadding * 2);

    // Fixed fraction of the screen, clamped by the theme's side margins; the
    // old max-text-width sizing is gone, long labels wrap inside the buttons.
    const fui::Rect screen = device.screen();
    const int16_t width =
        fui::clampI16(std::min<int>(screen.width * 3 / 4, screen.width - metrics.optionPopupDialogSideMargin * 2));

    // Fit as many rows as the active UI font and current orientation allow.
    // The dialog's chrome is measured by the same routine used for drawing;
    // only the row count is bounded by the fixed interaction buffer.
    auto visibleCount = [&]() {
      const int16_t chromeHeight = fui::optionDialogHeight(target, props, width);
      const size_t availableHeight =
          chromeHeight < screen.height ? static_cast<size_t>(screen.height - chromeHeight) : 0;
      return optionPopupPaging::visibleCountForHeight(totalOptions, MAX_OPTIONS, availableHeight,
                                                      static_cast<size_t>(std::max<int16_t>(1, props.buttonHeight)),
                                                      static_cast<size_t>(std::max<int16_t>(0, props.gap)));
    };
    size_t pageSize = visibleCount();
    if (pageSize == 0 && totalOptions > 0) pageSize = 1;
    if (data->message.empty() && pageSize < static_cast<size_t>(totalOptions)) {
      snprintf(pageIndicator, sizeof(pageIndicator), "%d/%d", selected + 1, totalOptions);
      props.message = pageIndicator;
      props.messageText.font = fui::GfxRendererTarget::FONT_BODY;
      props.messageText.align = fui::TextAlign::Center;
      props.messageText.maxLines = 1;
      pageSize = visibleCount();
      if (pageSize == 0 && totalOptions > 0) pageSize = 1;
    }

    const auto window = optionPopupPaging::windowForSelection(totalOptions, selected, pageSize);
    const int firstOption = static_cast<int>(window.first);
    const uint8_t count = static_cast<uint8_t>(window.count);
    for (uint8_t i = 0; i < count; ++i) {
      const int optionIndex = firstOption + i;
      options[i].label = allOptions[optionIndex].c_str();
      options[i].action = ACTION_OPTION;
      options[i].value = static_cast<int16_t>(optionIndex);
      options[i].state = (optionIndex == selected) ? fui::StateFocused : fui::StateNormal;
    }
    props.optionCount = count;

    const int16_t height = fui::clampI16(fui::optionDialogHeight(target, props, width), 0, screen.height);
    const fui::Rect dialogRect = fui::centeredRect(screen, fui::Size{width, height});

    // Chrome guard first, options after: route() scans newest-first, so the
    // option buttons win inside the dialog and the guard absorbs the rest.
    frame.hit(dialogRect, ACTION_CHROME, 0, fui::InputTouch);
    fui::optionDialog(frame, dialogRect, props);
    // Atomically make this generation the one handleInput() reads, now that
    // every hit() call for this frame is done.
    interactions.publish();
    publishedGeneration.store(renderGeneration, std::memory_order_release);
  }

  bool isActive() const { return active.load(std::memory_order_acquire); }

  // Close without firing the callback (the surface under the popup is going
  // away, e.g. its host screen closes from outside the popup's own input).
  void dismiss() { closeWithoutSelect(); }

 private:
  // A page keeps the DialogOption array on the stack and interaction table
  // small while button navigation can still reach every option. +1 slot for
  // the chrome guard rect.
  static constexpr int MAX_OPTIONS = optionPopupPaging::MAX_VISIBLE_OPTIONS;
  static constexpr size_t INTERACTION_CAPACITY = MAX_OPTIONS + 1;
  static constexpr freeink::ui::ActionId ACTION_OPTION = 1;
  static constexpr freeink::ui::ActionId ACTION_CHROME = 2;

  struct PopupData {
    std::string title;
    std::string headline;
    std::string message;
    std::shared_ptr<const std::vector<std::string>> options;
  };

  static std::shared_ptr<const std::vector<std::string>> makeOwnedOptions(std::vector<std::string> options) {
    return std::make_shared<const std::vector<std::string>>(std::move(options));
  }

  static std::shared_ptr<const std::vector<std::string>> copyOptions(const std::vector<std::string>& options) {
    return std::make_shared<const std::vector<std::string>>(options);
  }

  std::shared_ptr<const PopupData> dataSnapshot() const { return data_.load(std::memory_order_acquire); }

  void showWithData(std::string titleStr, std::string headlineStr, std::string messageStr,
                    std::shared_ptr<const std::vector<std::string>> options, const int currentIndex,
                    std::function<void(int)> onSelect) {
    active.store(false, std::memory_order_release);
    interactionGeneration.fetch_add(1, std::memory_order_acq_rel);
    auto data = std::make_shared<PopupData>();
    data->title = std::move(titleStr);
    data->headline = std::move(headlineStr);
    data->message = std::move(messageStr);
    data->options = std::move(options);
    const int count = data->options ? static_cast<int>(data->options->size()) : 0;
    selectedIndex.store(currentIndex >= 0 && currentIndex < count ? currentIndex : 0, std::memory_order_release);
    onSelectCallback = std::move(onSelect);
    data_.store(std::shared_ptr<const PopupData>(std::move(data)), std::memory_order_release);
    active.store(count > 0, std::memory_order_release);
  }

  void closeWithoutSelect() {
    active.store(false, std::memory_order_release);
    interactionGeneration.fetch_add(1, std::memory_order_acq_rel);
    onSelectCallback = nullptr;
    data_.store({}, std::memory_order_release);
  }

  void closeAndSelect() {
    const int selected = selectedIndex.load(std::memory_order_acquire);
    active.store(false, std::memory_order_release);
    interactionGeneration.fetch_add(1, std::memory_order_acq_rel);
    data_.store({}, std::memory_order_release);
    auto callback = std::move(onSelectCallback);
    onSelectCallback = nullptr;
    if (callback) callback(selected);
  }

  std::atomic<bool> active{false};
  std::atomic<std::shared_ptr<const PopupData>> data_;
  std::atomic<int> selectedIndex{0};
  std::function<void(int)> onSelectCallback;
  // The render publishes the generation represented by the interaction table;
  // handleInput ignores a table from a popup that has since been replaced.
  mutable freeink::ui::InteractionBuffer<INTERACTION_CAPACITY> interactions;
  std::atomic<uint32_t> interactionGeneration{0};
  mutable std::atomic<uint32_t> publishedGeneration{static_cast<uint32_t>(-1)};
};
