#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace release_version {

struct Version {
  std::uint32_t major = 0;
  std::uint32_t minor = 0;
  std::uint32_t patch = 0;
  std::uint32_t rc = 0;
  std::uint32_t revision = 0;
  bool isRc = false;
  bool hasRevision = false;
};

constexpr std::string_view normalizedTag(std::string_view tag) {
  return !tag.empty() && tag.front() == 'v' ? tag.substr(1) : tag;
}

constexpr bool parseNumber(std::string_view text, std::size_t& position, std::uint32_t& value) {
  const std::size_t begin = position;
  value = 0;
  while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
    const std::uint32_t digit = static_cast<std::uint32_t>(text[position] - '0');
    if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / 10) return false;
    value = value * 10 + digit;
    ++position;
  }
  return position > begin && (position - begin == 1 || text[begin] != '0');
}

constexpr bool parse(std::string_view tag, Version& version) {
  version = {};
  const auto text = normalizedTag(tag);
  std::size_t position = 0;
  if (!parseNumber(text, position, version.major) || position >= text.size() || text[position++] != '.') return false;
  if (!parseNumber(text, position, version.minor) || position >= text.size() || text[position++] != '.') return false;
  if (!parseNumber(text, position, version.patch)) return false;
  if (position == text.size()) return true;
  if (text.substr(position, 3) != "-rc") return false;
  position += 3;
  version.isRc = true;
  if (!parseNumber(text, position, version.rc)) return false;
  if (position == text.size()) return true;
  constexpr std::string_view REVISION = ".rev.";
  if (text.substr(position, REVISION.size()) != REVISION) return false;
  position += REVISION.size();
  version.hasRevision = true;
  if (!parseNumber(text, position, version.revision)) return false;
  return position == text.size();
}

constexpr int compare(const Version& left, const Version& right) {
  if (left.major != right.major) return left.major < right.major ? -1 : 1;
  if (left.minor != right.minor) return left.minor < right.minor ? -1 : 1;
  if (left.patch != right.patch) return left.patch < right.patch ? -1 : 1;
  if (left.isRc != right.isRc) return left.isRc ? -1 : 1;
  if (!left.isRc) return 0;
  if (left.rc != right.rc) return left.rc < right.rc ? -1 : 1;
  if (left.hasRevision != right.hasRevision) return left.hasRevision ? 1 : -1;
  if (left.revision != right.revision) return left.revision < right.revision ? -1 : 1;
  return 0;
}

constexpr bool isNewer(std::string_view candidate, std::string_view current) {
  Version candidateVersion;
  Version currentVersion;
  return parse(candidate, candidateVersion) && parse(current, currentVersion) &&
         compare(candidateVersion, currentVersion) > 0;
}

}  // namespace release_version
