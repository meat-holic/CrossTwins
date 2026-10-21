#pragma once
inline bool fontLockHeld = false, settingsSavedWhileLocked = false;
template <class T>
class PersistableStore {
 public:
  static T& getInstance() {
    static T value;
    return value;
  }
  static T defaults() { return T(); }
  void requestResave() { ++resaves; }
  void saveToFile() {
    ++saves;
    settingsSavedWhileLocked |= fontLockHeld;
  }
  int resaves = 0, saves = 0;
};
