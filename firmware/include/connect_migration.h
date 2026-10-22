#pragma once

#include <cstdint>
#include "connect_migration_policy.h"

namespace wxconnect::connect_migration {

enum class Phase : uint8_t { Idle, Checking, Downloading, ReadyToRestart, Failed };

struct State {
  Phase phase = Phase::Idle;
  migration_policy::Target target = migration_policy::Target::Main;
  bool worker_ready = false;
  bool busy = false;
  bool install = true;
  bool restart_pending = false;
  uint8_t progress = 0;
  char target_version[18]{};
  char message[128] = "Switch only after WX-Link includes support for this station.";
};

bool begin();
State state();
bool request(migration_policy::Target target=migration_policy::Target::Main, bool install=true);
bool blocksCapture();
bool restartDue();

}  // namespace wxconnect::connect_migration
