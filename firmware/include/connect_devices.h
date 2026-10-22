#pragma once

// Reviewed 433 MHz OOK capture registry. This is deliberately separate from
// the upstream all-protocol list and excludes FSK, disabled and unresolved RF
// candidates. Symbols and scope are mirrored in connect_decoder_profile.h.
#define MY_DEVICES 1
#define INCLUDE_RTL_433_DEVICES_H_
#define DEVICES \
  DECL(acurite_txr) \
  DECL(ambient_weather) \
  DECL(auriol_afw2a1) \
  DECL(cotech_36_7959) \
  DECL(emos_e6016) \
  DECL(emos_e6016_rain) \
  DECL(esperanza_ews) \
  DECL(fineoffset_wh1080) \
  DECL(gt_wt_02) \
  DECL(gt_wt_03) \
  DECL(homelead_hg9901) \
  DECL(lacrossetx) \
  DECL(lacrossews) \
  DECL(rainpoint) \
  DECL(rainpoint_hcs012arf) \
  DECL(schou_72543_rain) \
  DECL(telldus_ft0385r) \
  DECL(ttx201) \
  DECL(vauno_en8822c) \
  DECL(wssensor)
#define NUMOFDEVICES 20

#ifdef __cplusplus
extern "C" {
#endif
#if __has_include("r_device.h")
#include "r_device.h"
#else
typedef struct r_device r_device;
#endif
extern r_device const acurite_txr;
extern r_device const ambient_weather;
extern r_device const auriol_afw2a1;
extern r_device const cotech_36_7959;
extern r_device const emos_e6016;
extern r_device const emos_e6016_rain;
extern r_device const esperanza_ews;
extern r_device const fineoffset_wh1080;
extern r_device const gt_wt_02;
extern r_device const gt_wt_03;
extern r_device const homelead_hg9901;
extern r_device const lacrossetx;
extern r_device const lacrossews;
extern r_device const rainpoint;
extern r_device const rainpoint_hcs012arf;
extern r_device const schou_72543_rain;
extern r_device const telldus_ft0385r;
extern r_device const ttx201;
extern r_device const vauno_en8822c;
extern r_device const wssensor;
#ifdef __cplusplus
}
#endif
