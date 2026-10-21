#pragma once

#include <HalStorage.h>
#include <Logging.h>

// Closed sources only. Reserve one sibling backup and refuse to overwrite recovery data.
inline bool commitNetworkFile(const String& source, const String& destination) {
  const String backup = destination + ".davbak";
  if (Storage.exists(backup.c_str())) return false;
  const bool existed = Storage.exists(destination.c_str());
  if (existed) {
    HalFile target;
    const auto opened = Storage.openFileForReadChecked("NET", destination.c_str(), target);
    if (opened != HalStorage::ReadOpenResult::Ok) {
      LOG_ERR("NET", "Cannot inspect destination: %u", static_cast<unsigned>(opened));
      return false;
    }
    if (!target || target.isDirectory()) return false;
    if (!target.close()) return false;
  }
  if (existed && !Storage.rename(destination.c_str(), backup.c_str())) return false;
  if (!Storage.rename(source.c_str(), destination.c_str())) {
    if (existed && !Storage.rename(backup.c_str(), destination.c_str()))
      LOG_ERR("DAV", "Destination rollback failed; retained %s", backup.c_str());
    return false;
  }
  if (existed && !Storage.remove(backup.c_str())) LOG_ERR("DAV", "Retained backup %s", backup.c_str());
  return true;
}
