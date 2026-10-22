#include "capture_session.h"
#include "capture_jsonl.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace wxconnect::capture;

int main() {
  const uint8_t entropy[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 255};
  char generated_id[kSessionIdChars + 1]{};
  formatSessionId(entropy, generated_id);
  assert(std::string(generated_id) == "000102030405060708090a0b0c0d0eff");

  ClockMap clock;
  CaptureSession session;
  Record start;
  constexpr char id[] = "0123456789abcdef0123456789abcdef";
  assert(session.start(id, Source::Live, "ook-433920-v1", 0xfffffff0u, clock, start));
  assert(start.event == Event::Session && start.seq == 0 && !start.time_valid);
  assert(start.session_id.data() == std::string(id));
  char session_line[2048];
  const size_t session_size = encodeJsonl(start, session_line, sizeof(session_line));
  assert(session_size && detail::JsonValidator(session_line, session_size - 1).valid());
  const std::string session_json(session_line, session_size);
  assert(session_json.find("\"decoder_library\":\"rtl_433_ESP\"") != std::string::npos);
  assert(session_json.find("\"decoder_revision\":\"f0a81569b58a60ea8b3a5efd8e9b39670e8d8df8\"") != std::string::npos);
  assert(session_json.find("\"decoder_symbols\":[\"acurite_txr\"") != std::string::npos);
  assert(session_json.find("\"wssensor\"]") != std::string::npos);
  assert(session_json.find("\"profile_revision\":1") != std::string::npos);
  assert(session_json.find("\"radio_readback_verified\":false") != std::string::npos);
  assert(session_json.find("\"requested_rssi_margin_db\":9") != std::string::npos);
  assert(session_json.find("\"firmware\":\"WXLink Connect 0.1.1\"") != std::string::npos);
  assert(session_json.find("\"board\":\"panellan_sc05_x\"") != std::string::npos);

  Record packet;
  const std::string native = R"({"model":"unknown","value":0,"extra":"retain","repeat":1})";
  assert(session.enqueueNative(native.data(), native.size(), 0xfffffffau, clock) == PushResult::Queued);
  assert(session.enqueueNative(native.data(), native.size(), 0xfffffffbu, clock) == PushResult::Queued);
  assert(session.stop(0xfffffffcu, clock, packet) == false);  // Drain before end.
  assert(session.popPacket(packet, clock));
  assert(packet.event == Event::Packet && packet.seq == 1);
  assert(packet.native_json_size == native.size());
  assert(std::string(packet.native_json.data(), packet.native_json_size) == native);
  assert(!packet.time_valid && packet.utc_ms == 0);
  char line[wxconnect::capture::maxEscapedNativeBytes()+512];
  const size_t line_size=encodeJsonl(packet,line,sizeof(line));
  assert(line_size && detail::JsonValidator(line,line_size-1).valid());
  assert(std::string(line,line_size).find("\"native_json\":\"{\\\"model\\\"")!=std::string::npos);
  assert(encodeJsonl(packet,line,32)==0);
  assert(session.popPacket(packet, clock) && packet.seq == 2);

  // millis() wrap extends monotonically, and UTC becomes available only after sync.
  const uint64_t wrapped = clock.observe(2u);
  assert(wrapped == 0x100000002ULL);
  constexpr uint64_t epoch = 1792281600000ULL;
  assert(clock.synchronize(3u, epoch));
  assert(session.enqueueNative("{\"zero\":0}", 10, 13u, clock) == PushResult::Queued);
  assert(session.popPacket(packet, clock) && packet.seq == 3);
  assert(packet.time_valid && packet.utc_ms == epoch + 10);

  Record end;
  assert(session.stop(14u, clock, end));
  assert(end.event == Event::End && end.seq == 4 && !session.active());
  end.capture_counters.decoder_truncation_drops = 3;
  end.receiver_drops = 2;
  end.capture_diagnostics_valid = true;
  std::strncpy(end.stop_reason.data(), "requested", end.stop_reason.size() - 1);
  const size_t end_size=encodeJsonl(end,line,sizeof(line));
  assert(end_size && detail::JsonValidator(line,end_size-1).valid());
  const std::string end_json(line,end_size);
  assert(end_json.find("\"decoder_truncation_drops\":3")!=std::string::npos);
  assert(end_json.find("\"receiver_drops\":2")!=std::string::npos);
  assert(end_json.find("\"stop_reason\":\"requested\"")!=std::string::npos);
  assert(session.enqueueNative("{}", 2, 15u, clock) == PushResult::Inactive);

  CaptureSession bounded;
  assert(bounded.start(id, Source::Synthetic, "test", 1, clock, start));
  std::string at_limit = "{\"x\":\"" + std::string(kNativeJsonCapacity - 9, 'a') + "\"}";
  assert(at_limit.size() == kNativeJsonCapacity - 1);
  assert(bounded.enqueueNative(at_limit.data(), at_limit.size(), 2, clock) == PushResult::Queued);
  const std::string too_large = "{\"x\":\"" + std::string(kNativeJsonCapacity - 8, 'a') + "\"}";
  assert(too_large.size() == kNativeJsonCapacity);
  assert(bounded.enqueueNative(too_large.data(), too_large.size(), 3, clock) == PushResult::Oversize);
  assert(bounded.enqueueNative("{bad}", 5, 4, clock) == PushResult::InvalidJson);
  for (size_t i = 1; i < kQueueDepth; ++i)
    assert(bounded.enqueueNative("{\"repeat\":true}", 15, uint32_t(4 + i), clock) == PushResult::Queued);
  assert(bounded.enqueueNative("{}", 2, 10, clock) == PushResult::QueueFull);
  assert(bounded.counters().oversize_drops == 1);
  assert(bounded.counters().invalid_json_drops == 1);
  assert(bounded.counters().queue_full_drops == 1);
  assert(bounded.counters().packet_records == 0);
  bounded.storageDrop();
  assert(bounded.counters().storage_drops == 1);
  bounded.transportDrop();
  assert(bounded.counters().transport_drops == 1);
  for (size_t i = 0; i < kQueueDepth; ++i) assert(bounded.popPacket(packet, clock));
  assert(bounded.counters().packet_records == kQueueDepth);
  assert(!bounded.popPacket(packet, clock));

  CaptureSession aborted;
  assert(aborted.start(id, Source::Live, "test", 1, clock, start));
  assert(aborted.enqueueNative("{}", 2, 2, clock) == PushResult::Queued);
  aborted.abortForStorageFailure();
  assert(!aborted.active() && aborted.queued() == 0);
  assert(aborted.counters().storage_drops == 1);

  // IDs, profiles and source labels are constrained before starting a session.
  CaptureSession rejected;
  assert(!rejected.start("not-an-id", Source::Replay, "replay", 1, clock, start));
  assert(!rejected.start(id, Source::Replay, "", 1, clock, start));
  std::puts("Capture session checks passed: IDs, sequences, wrap, UTC, exact JSON, repeats, bounds and loss counters.");
}
