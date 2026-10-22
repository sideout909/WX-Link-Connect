#pragma once
#include <Arduino.h>
#include "capture_types.h"

namespace radio_receiver {
// Only the six documented external pins. Display/touch/native USB are separate.
constexpr int kSck = 10, kMiso = 11, kMosi = 12, kCs = 13, kGdo0 = 14, kGdo2 = 21;
constexpr size_t kDecodedBufferBytes = 16384;
bool begin();
void loop();
bool ready();
// An array reference preserves the capacity at the call site; pointer sizeof()
// cannot accidentally shrink it to the address width.
bool pop(char (&output)[kDecodedBufferBytes], size_t& length, bool& truncated);
uint32_t dropped();
struct Diagnostics {
  bool ready=false,enabled=false,init_attempted=false,calibrated=false;
  int current_rssi=0,average_rssi=0,threshold=0,margin=0;
  uint32_t captured_signals=0,ignored_signals=0,decoded_signals=0,decoded_messages=0,zero_decodes=0;
  uint32_t capture_drops=0,decoder_drops=0;
};
Diagnostics diagnostics();
bool setCaptureMargin(unsigned db);
bool startRawWindow(uint32_t duration_ms);
void stopRawWindow();
bool rawWindowActive();
uint32_t rawDropped();
uint32_t rawSkipped();
bool popPulse(wxconnect::capture::PulseFrame& frame);
}  // namespace radio_receiver
