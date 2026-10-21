#pragma once

#include <BoardConfig.h>

#if FREEINK_CAP_HAPTIC
#include <HalHaptics.h>

#include "CrossPointSettings.h"
#endif

// Call only after the application accepts an interactive touch action.
// Sampling a contact or classifying a gesture must remain silent.
namespace haptic_feedback {
inline void touchAction(bool longPress = false) {
#if FREEINK_CAP_HAPTIC
  const bool enabled = SETTINGS.vibration != CrossPointSettings::VIBRATION_OFF;
  if (longPress) {
    HalHaptics::longPress(enabled, SETTINGS.hapticIntensity);
  } else {
    HalHaptics::feedback(enabled, true, SETTINGS.hapticIntensity);
  }
#else
  (void)longPress;
#endif
}
}  // namespace haptic_feedback
