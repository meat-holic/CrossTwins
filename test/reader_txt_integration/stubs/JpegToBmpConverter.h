#pragma once
#include "HalStorage.h"
inline bool jpegSucceeds = true;
struct JpegToBmpConverter {
  static bool jpegFileToBmpStream(HalFile&, HalFile&) { return jpegSucceeds; }
};
