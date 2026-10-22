#include "connect_decoder_profile.h"

#include <cassert>
#include <cstring>

int main() {
  using namespace wxconnect::radio_profile;
  assert(std::strcmp(kId, "wx-ook43392-rtl433-f0a8156-v1") == 0);
  assert(kRevisionNumber == 1 && kCenterFrequencyMHz == 433.92f);
  assert(std::strcmp(kModulation, "OOK") == 0);
  assert(kRequestedRxBandwidthKHz == 812.0f);
  assert(kAgcCtrl2 == 0xC7 && kAgcCtrl1 == 0x40 && kAgcCtrl0 == 0xB2);
  assert(kDataRateRegisterMdmcfg3 == 0x93 && kAutomaticRssiMarginDb == 9);
  assert(std::strcmp(stateName(InitializationState::NotInitialized), "not_initialized") == 0);
  assert(std::strcmp(stateName(InitializationState::AcceptedUnverified), "init_accepted_readback_unverified") == 0);
  assert(std::strcmp(stateName(InitializationState::Failed), "initialization_failed") == 0);
  assert(kEnabledDecoderCount == 20);
}
