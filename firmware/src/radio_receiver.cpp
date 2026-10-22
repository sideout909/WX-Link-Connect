#include "radio_receiver.h"
#include "capture_pulse.h"
#include "connect_decoder_profile.h"
#include <rtl_433_ESP.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <new>
#include <atomic>
#include <cstring>

namespace radio_receiver {
namespace {
rtl_433_ESP receiver;
// Holds the worst-case JSON-escaped form of every accepted 2047-byte native
// payload, plus oversized output so the broker can reject and count it.
constexpr size_t kMessageSize = kDecodedBufferBytes;
struct Message { char json[kMessageSize]; size_t length = 0; bool truncated = false; };
// Decoder callbacks run in task context. Allocate the fixed pool before reception;
// keep scarce internal RAM available to Wi-Fi and HTTP. Never allocate in callbacks.
Message* messages = nullptr;
QueueHandle_t queue = nullptr, available = nullptr;
wxconnect::capture::PulseQueue pulse_queue;
char* decoder_buffer = nullptr;
bool initialized = false;
bool initialization_attempted = false;
volatile uint32_t queue_drops = 0;
volatile uint32_t pulse_drops=0,pulse_skips=0;
std::atomic<uint64_t> raw_window_end_ms{0},last_pulse_copy_ms{0};
void decoded(char* json, size_t length, int truncated) {
  // Called from the decoder task. Only copy to a bounded queue; no UI, TLS,
  // configuration, or shared weather-model access from this context.
  if (!json || length >= kMessageSize) { ++queue_drops; return; }
  uint8_t slot=0;
  if(xQueueReceive(available,&slot,0)!=pdTRUE){++queue_drops;return;}
  Message& message=messages[slot];
  message.length = length;
  message.truncated = truncated != 0;
  memcpy(message.json,json,length+1);
  if (xQueueSend(queue,&slot,0)!=pdTRUE){xQueueSend(available,&slot,0);++queue_drops;}
}
void rawDecoded(const int* pulses,const int* gaps,unsigned int count,
                unsigned long duration_us,int rssi){
  const uint64_t now=uint64_t(esp_timer_get_time())/1000;
  const uint64_t until=raw_window_end_ms.load(std::memory_order_acquire);
  if(!until||now>=until)return;
  uint64_t previous=last_pulse_copy_ms.load(std::memory_order_relaxed);
  if(previous&&now-previous<wxconnect::capture::kRawPulseIntervalMs){++pulse_skips;return;}
  const auto result=pulse_queue.copyFromCallback(pulses,gaps,count,duration_us,rssi);
  if(result==wxconnect::capture::PulsePushResult::Full){++pulse_drops;return;}
  if(result==wxconnect::capture::PulsePushResult::Invalid){++pulse_skips;return;}
  last_pulse_copy_ms.store(now,std::memory_order_relaxed);
}
}
bool begin() {
  if (!messages) {
    messages = static_cast<Message*>(heap_caps_malloc(sizeof(Message) * 4,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (messages) for (unsigned i = 0; i < 4; ++i) new (messages + i) Message{};
  }
  if (!decoder_buffer) decoder_buffer = static_cast<char*>(heap_caps_malloc(kMessageSize,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!messages || !decoder_buffer) {
    heap_caps_free(messages); messages = nullptr;
    heap_caps_free(decoder_buffer); decoder_buffer = nullptr;
    initialization_attempted = true;
    return false;
  }
  if (!queue) queue=xQueueCreate(4,sizeof(uint8_t));
  if (!available) available=xQueueCreate(4,sizeof(uint8_t));
  if (!queue||!available)return false;
  if(uxQueueMessagesWaiting(available)==0){for(uint8_t i=0;i<4;++i)xQueueSend(available,&i,0);}
  receiver.setCallbackWithStatus(decoded,decoder_buffer,kMessageSize);
  receiver.setRawPulsesCallback(rawDecoded);
  initialization_attempted=true;
  initialized=receiver.begin(kGdo0,wxconnect::radio_profile::kCenterFrequencyMHz);
  if (initialized) receiver.enableReceiver();
  return initialized;
}
void loop() { if(initialized)receiver.loop(); }
bool ready() { return initialized; }
bool pop(char (&output)[kDecodedBufferBytes],size_t& length,bool& truncated) {
  if(!queue)return false;
  uint8_t slot=0;
  if(xQueueReceive(queue,&slot,0)!=pdTRUE)return false;
  const Message& message=messages[slot];
  memcpy(output,message.json,message.length+1);length=message.length;truncated=message.truncated;
  xQueueSend(available,&slot,0);
  return true;
}
uint32_t dropped() { return queue_drops; }
Diagnostics diagnostics(){
  Diagnostics d;d.ready=initialized;d.init_attempted=initialization_attempted;if(!initialized)return d;
  const auto s=receiver.statusSnapshot();d.enabled=s.receiverEnabled;
  d.current_rssi=s.currentRssi;d.average_rssi=s.averageRssi;d.threshold=s.rssiThreshold;
  d.margin=rtl_433_ESP::rssiThresholdDelta;
  d.captured_signals=s.capturedSignals;d.ignored_signals=s.ignoredSignals;d.calibrated=s.rssiCalibrated;
  d.decoded_signals=s.decodedSignals;d.decoded_messages=s.decodedMessages;
  d.zero_decodes=s.zeroDecodedSignals;d.capture_drops=s.droppedCaptureBuffers;
  d.decoder_drops=s.droppedDecoderQueue;return d;
}
bool setCaptureMargin(unsigned db){
  if(!initialized||db<3||db>15)return false;
  receiver.setRSSIThreshold(db);return true;
}
bool startRawWindow(uint32_t duration_ms){
  if(!initialized||!wxconnect::capture::validRawWindowDuration(duration_ms))return false;
  const uint64_t now=uint64_t(esp_timer_get_time())/1000;
  last_pulse_copy_ms.store(0,std::memory_order_relaxed);
  raw_window_end_ms.store(now+duration_ms,std::memory_order_release);
  return true;
}
void stopRawWindow(){raw_window_end_ms.store(0,std::memory_order_release);}
bool rawWindowActive(){const uint64_t end=raw_window_end_ms.load(std::memory_order_acquire);return end&&uint64_t(esp_timer_get_time())/1000<end;}
uint32_t rawDropped(){return pulse_drops;}
uint32_t rawSkipped(){return pulse_skips;}
bool popPulse(wxconnect::capture::PulseFrame& frame){return pulse_queue.pop(frame);}
}
