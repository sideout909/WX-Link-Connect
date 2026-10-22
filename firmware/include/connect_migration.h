#pragma once

#include <cstdint>

namespace wxconnect::connect_migration {

enum class Phase : uint8_t { Idle, Checking, Downloading, ReadyToRestart, Failed };

struct State {
  Phase phase = Phase::Idle;
  bool worker_ready = false;
  bool busy = false;
  bool restart_pending = false;
  uint8_t progress = 0;
  char target_version[18]{};
  char message[128] = "Switch only after WX-Link includes support for this station.";
};

bool begin();
State state();
bool request();
bool blocksCapture();
bool restartDue();

}  // namespace wxconnect::connect_migration
