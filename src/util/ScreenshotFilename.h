#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace screenshot_filename {

enum class ParseResult : uint8_t { NotNumberedScreenshot, Valid, Overflow };

inline ParseResult parseNumber(const char* filename, uint64_t& number) {
  constexpr char prefix[] = "screenshot_";
  constexpr char suffix[] = ".bmp";
  constexpr size_t prefixLength = sizeof(prefix) - 1;
  constexpr size_t suffixLength = sizeof(suffix) - 1;
  constexpr size_t minimumDigits = 5;

  if (filename == nullptr) return ParseResult::NotNumberedScreenshot;
  const size_t length = std::strlen(filename);
  if (length < prefixLength + minimumDigits + suffixLength || std::strncmp(filename, prefix, prefixLength) != 0 ||
      std::strcmp(filename + length - suffixLength, suffix) != 0) {
    return ParseResult::NotNumberedScreenshot;
  }

  const size_t digitsEnd = length - suffixLength;
  uint64_t parsed = 0;
  for (size_t i = prefixLength; i < digitsEnd; ++i) {
    const char digit = filename[i];
    if (digit < '0' || digit > '9') return ParseResult::NotNumberedScreenshot;
    const uint64_t value = static_cast<uint64_t>(digit - '0');
    if (parsed > (std::numeric_limits<uint64_t>::max() - value) / 10) return ParseResult::Overflow;
    parsed = parsed * 10 + value;
  }

  number = parsed;
  return ParseResult::Valid;
}

inline bool observeName(const char* filename, uint64_t& largestNumber) {
  uint64_t number = 0;
  const ParseResult result = parseNumber(filename, number);
  if (result == ParseResult::Overflow) return false;
  if (result == ParseResult::Valid && number > largestNumber) largestNumber = number;
  return true;
}

inline bool nextAfter(uint64_t largestNumber, uint64_t& nextNumber) {
  if (largestNumber == std::numeric_limits<uint64_t>::max()) return false;
  nextNumber = largestNumber + 1;
  return true;
}

inline bool increment(uint64_t& number) {
  if (number == std::numeric_limits<uint64_t>::max()) return false;
  ++number;
  return true;
}

inline bool formatPath(const char* directory, const uint64_t number, char* path, const size_t pathSize) {
  if (directory == nullptr || directory[0] == '\0' || number == 0 || path == nullptr || pathSize == 0) return false;
  const int length = std::snprintf(path, pathSize, "%s/screenshot_%05llu.bmp", directory,
                                   static_cast<unsigned long long>(number));
  return length >= 0 && static_cast<size_t>(length) < pathSize;
}

}  // namespace screenshot_filename
