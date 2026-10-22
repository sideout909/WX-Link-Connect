#pragma once

#include <cstddef>
#include <cstdint>

namespace wxconnect::radio_profile {
// Captures one receiver configuration at a time. The registry is a reviewed
// decoder candidate set; it does not claim retail-device or RF validation.
inline constexpr char kId[] = "wx-ook43392-rtl433-f0a8156-v1";
inline constexpr char kLibrary[] = "rtl_433_ESP";
inline constexpr char kRevision[] = "f0a81569b58a60ea8b3a5efd8e9b39670e8d8df8";
inline constexpr uint32_t kRevisionNumber = 1;
inline constexpr float kCenterFrequencyMHz = 433.92f;
inline constexpr char kModulation[] = "OOK";
// Pinned rtl_433_ESP CC1101 defaults used by its fixed OOK initialization.
// Bandwidth resolves to the CC1101's 812.5 kHz setting with its 26 MHz crystal.
inline constexpr float kRequestedRxBandwidthKHz = 812.0f;
inline constexpr uint8_t kAgcCtrl2 = 0xC7;
inline constexpr uint8_t kAgcCtrl1 = 0x40;
inline constexpr uint8_t kAgcCtrl0 = 0xB2;
inline constexpr uint8_t kDataRateRegisterMdmcfg3 = 0x93;
inline constexpr uint8_t kAutomaticRssiMarginDb = 9;
inline constexpr const char* kEnabledDecoderSymbols[] = {
    "acurite_txr", "ambient_weather", "auriol_afw2a1", "cotech_36_7959",
    "emos_e6016", "emos_e6016_rain", "esperanza_ews", "fineoffset_wh1080",
    "gt_wt_02", "gt_wt_03", "homelead_hg9901", "lacrossetx", "lacrossews",
    "rainpoint", "rainpoint_hcs012arf", "schou_72543_rain", "telldus_ft0385r",
    "ttx201", "vauno_en8822c", "wssensor"};
enum class InitializationState : uint8_t { NotInitialized, AcceptedUnverified, Failed };
inline constexpr const char* stateName(InitializationState state) {
  switch (state) {
    case InitializationState::NotInitialized: return "not_initialized";
    case InitializationState::AcceptedUnverified: return "init_accepted_readback_unverified";
    case InitializationState::Failed: return "initialization_failed";
  }
  return "unknown";
}
inline constexpr size_t kEnabledDecoderCount =
    sizeof(kEnabledDecoderSymbols) / sizeof(kEnabledDecoderSymbols[0]);
}  // namespace wxconnect::radio_profile
