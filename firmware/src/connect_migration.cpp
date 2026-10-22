#include "connect_migration.h"

#include "connect_migration_policy.h"
#include "connect_transfer.h"
#include "firmware_version.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <lwip/dns.h>
#include <lwip/tcpip.h>
#include <memory>
#include <mbedtls/sha256.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern const uint8_t root_bundle[] asm("_binary_certs_x509_crt_bundle_bin_start");

namespace wxconnect::connect_migration {
namespace {
portMUX_TYPE state_mux = portMUX_INITIALIZER_UNLOCKED;
State current;
TaskHandle_t worker_task = nullptr;
uint32_t restart_at_ms = 0;

void publish(Phase phase, bool busy, bool restart_pending, uint8_t progress,
             const char* message, const char* version = nullptr) {
  portENTER_CRITICAL(&state_mux);
  current.phase = phase;
  current.busy = busy;
  current.restart_pending = restart_pending;
  current.progress = progress;
  if (message) std::snprintf(current.message, sizeof(current.message), "%s", message);
  if (version) std::snprintf(current.target_version, sizeof(current.target_version), "%s", version);
  portEXIT_CRITICAL(&state_mux);
}

void fail(const char* message, const char* version = nullptr) {
  publish(Phase::Failed, false, false, 0, message, version);
}

// DNS callbacks can arrive after timeout. Permanent, ticketed state avoids
// retaining a task-stack IPAddress like the pinned blocking hostByName path.
portMUX_TYPE dns_mux = portMUX_INITIALIZER_UNLOCKED;
transfer::Lookup dns_lookup;
char dns_hostname[256]{};
void dnsDone(const char*, const ip_addr_t* address, void* context) {
  const auto ticket = uint32_t(reinterpret_cast<uintptr_t>(context));
  const uint32_t ip = address && IP_IS_V4(address) ? ip4_addr_get_u32(ip_2_ip4(address)) : 0;
  portENTER_CRITICAL(&dns_mux);
  dns_lookup.complete(ticket, ip);
  portEXIT_CRITICAL(&dns_mux);
}
void dnsStart(void* context) {
  const auto ticket = uint32_t(reinterpret_cast<uintptr_t>(context));
  char hostname[sizeof(dns_hostname)];
  portENTER_CRITICAL(&dns_mux);
  const bool active = dns_lookup.ticket == ticket && dns_lookup.pending;
  std::memcpy(hostname, dns_hostname, sizeof(hostname));
  portEXIT_CRITICAL(&dns_mux);
  if (!active) return;
  ip_addr_t result{};
  const err_t error = dns_gethostbyname_addrtype(hostname, &result, dnsDone, context,
                                                LWIP_DNS_ADDRTYPE_IPV4);
  if (error == ERR_OK) dnsDone(hostname, &result, context);
  else if (error != ERR_INPROGRESS) dnsDone(hostname, nullptr, context);
}
bool resolve(const char* hostname, IPAddress& result) {
  if (!hostname || std::strlen(hostname) >= sizeof(dns_hostname)) return false;
  portENTER_CRITICAL(&dns_mux);
  const uint32_t ticket = dns_lookup.begin();
  std::snprintf(dns_hostname, sizeof(dns_hostname), "%s", hostname);
  portEXIT_CRITICAL(&dns_mux);
  const err_t queued = tcpip_try_callback(dnsStart, reinterpret_cast<void*>(uintptr_t(ticket)));
  const uint32_t started = millis();
  while (queued == ERR_OK && uint32_t(millis() - started) < 5000) {
    portENTER_CRITICAL(&dns_mux);
    const bool done = dns_lookup.finished;
    const uint32_t address = dns_lookup.address;
    portEXIT_CRITICAL(&dns_mux);
    if (done) { result = IPAddress(address); return address != 0; }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  portENTER_CRITICAL(&dns_mux);
  dns_lookup.cancel(ticket);
  portEXIT_CRITICAL(&dns_mux);
  return false;
}
class BoundedTlsClient : public WiFiClientSecure {
 public:
  void beginRequest() { request_started_ = millis(); armed_ = true; reason_ = "Network request failed"; }
  void endRequest() { armed_ = false; }
  const char* reason() const { return reason_; }
  int connect(const char* host, uint16_t port, int32_t timeout) override {
    if (expired()) return 0;
    IPAddress ip;
    if (!resolve(host, ip)) { reason_ = "DNS lookup failed or exceeded 5 seconds"; return 0; }
    if (expired()) return 0;
    setTimeout(uint32_t(std::max<int32_t>(1, (timeout + 999) / 1000)));
    // Preserve SNI and hostname verification with the CA bundle. Never use an
    // insecure client or verify the certificate against the resolved IP.
    const int connected = WiFiClientSecure::connect(ip, port, host, nullptr, nullptr, nullptr);
    if (!connected) {
      char detail[64]{};
      const int code = lastError(detail, sizeof(detail));
      std::snprintf(tls_reason_, sizeof(tls_reason_), "TLS/connect failed (%d): %.40s", code, detail);
      reason_ = tls_reason_;
    }
    return connected;
  }
  int available() override { return expired() ? 0 : WiFiClientSecure::available(); }
  uint8_t connected() override { return expired() ? 0 : WiFiClientSecure::connected(); }
  int read(uint8_t* bytes, size_t size) override {
    return expired() ? -1 : WiFiClientSecure::read(bytes, size);
  }
 private:
  bool expired() {
    if (!armed_ || uint32_t(millis() - request_started_) < 45000) return false;
    reason_ = "Release request exceeded 45 seconds";
    WiFiClientSecure::stop(); return true;
  }
  bool armed_ = false;
  uint32_t request_started_ = 0;
  const char* reason_ = "Network request failed";
  char tls_reason_[96]{};
};

int get(HTTPClient& http, BoundedTlsClient& tls, String url) {
  tls.beginRequest();
  tls.setCACertBundle(root_bundle);
  tls.setHandshakeTimeout(6);
  tls.setTimeout(8);
  http.useHTTP10(true);
  http.setConnectTimeout(4000);
  http.setTimeout(8000);
  const char* headers[] = {"Location"};
  http.collectHeaders(headers, 1);

  for (unsigned redirect = 0; redirect <= 5; ++redirect) {
    if (!migration_policy::downloadHost(url.c_str())) return -100;
    if (!http.begin(tls, url)) return -101;
    http.addHeader("User-Agent", String("WXLink-Connect/") + firmware_version::current);
    const auto snapshot = state();
    char stage[128];
    std::snprintf(stage, sizeof(stage), "%s: secure request %u/6 (DNS max 5s)",
        snapshot.phase == Phase::Checking ? "WX-Link release" : "WX-Link image", redirect + 1);
    publish(snapshot.phase, true, false, snapshot.progress, stage);
    const int code = http.GET();
    if (code != 301 && code != 302 && code != 303 && code != 307 && code != 308) { tls.endRequest(); return code; }
    const String location = http.header("Location");
    http.end();
    tls.stop();
    if (redirect == 5 || location.isEmpty() || location.indexOf('\r') >= 0 ||
        location.indexOf('\n') >= 0) return -102;
    if (location.startsWith("/") && !location.startsWith("//") &&
        url.startsWith("https://github.com/")) {
      url = String("https://github.com") + location;
    } else {
      url = location;
    }
  }
  return -102;
}

bool readManifest(HTTPClient& http, BoundedTlsClient& tls, String& url,
                  uint32_t slot_bytes, String& version, String& image_url,
                  String& image_sha, uint32_t& image_size) {
  publish(Phase::Checking, true, false, 0, "Checking the latest WX-Link release");
  const int code = get(http, tls, String(migration_policy::kManifestUrl));
  if (code != 200) {
    http.end();
    char reason[128];
    std::snprintf(reason, sizeof(reason), "Release lookup failed (%d): %.78s; retry is available",
                  code, code < 0 ? tls.reason() : code == 404 ? "No published manifest" : "HTTP error");
    fail(reason);
    return false;
  }
  const int manifest_bytes = http.getSize();
  if (manifest_bytes < 1 || manifest_bytes > 4096) {
    http.end();
    fail("WX-Link manifest size is invalid; Connect is unchanged");
    return false;
  }
  std::unique_ptr<char, decltype(&heap_caps_free)> body(
      static_cast<char*>(heap_caps_malloc(size_t(manifest_bytes) + 1,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)), heap_caps_free);
  if (!body) { http.end(); fail("Manifest buffer unavailable; Connect is unchanged"); return false; }
  auto* stream = http.getStreamPtr();
  const auto read = transfer::readExact(*stream, reinterpret_cast<uint8_t*>(body.get()),
      size_t(manifest_bytes), [](){return millis();}, [](){vTaskDelay(pdMS_TO_TICKS(1));},
      [&](){return http.connected();});
  http.end();
  if (read != transfer::ReadResult::Complete) {
    fail("Manifest body timed out or disconnected; retry is available"); return false;
  }
  body.get()[manifest_bytes] = '\0';
  StaticJsonDocument<1536> document;
  const DeserializationError parse_error = deserializeJson(document,
      static_cast<const char*>(body.get()), size_t(manifest_bytes));
  if (parse_error || document.overflowed() || !document.is<JsonObject>()) {
    fail("WX-Link manifest is invalid; Connect is unchanged");
    return false;
  }

  if (!document["schema"].is<uint32_t>() || !document["flash_bytes"].is<uint32_t>() ||
      !document["size"].is<uint32_t>() || !document["version"].is<const char*>() ||
      !document["board"].is<const char*>() ||
      !document["partition_layout"].is<const char*>() ||
      !document["firmware_url"].is<const char*>() ||
      !document["sha256"].is<const char*>()) {
    fail("WX-Link manifest fields have invalid types; Connect is unchanged");
    return false;
  }
  version = document["version"].as<const char*>();
  image_url = document["firmware_url"] | "";
  image_sha = document["sha256"] | "";
  image_size = document["size"] | uint32_t(0);
  const auto* target = esp_ota_get_next_update_partition(nullptr);
  const migration_policy::Manifest manifest{
      document["schema"].as<uint32_t>(),
      document["flash_bytes"].as<uint32_t>(),
      image_size,
      document["board"].is<const char*>() ? document["board"].as<const char*>() : nullptr,
      document["partition_layout"].is<const char*>()
          ? document["partition_layout"].as<const char*>() : nullptr,
      version.c_str(), image_sha.c_str(), image_url.c_str()};
  const uint32_t slot = target ? target->size : 0;
  if (!target || slot > slot_bytes ||
      !migration_policy::validManifest(manifest,
                                       ESP.getFlashChipSize(), slot)) {
    fail("WX-Link release is incompatible or not stable; Connect is unchanged", version.c_str());
    return false;
  }
  image_sha.toLowerCase();
  url = image_url;
  return true;
}

bool downloadAndInstall(HTTPClient& http, BoundedTlsClient& tls, const String& url,
                        const String& version, const String& expected_sha,
                        uint32_t image_size) {
  publish(Phase::Downloading, true, false, 0, "Downloading and verifying WX-Link", version.c_str());
  const int code = get(http, tls, url);
  if (code != 200 || http.getSize() != int(image_size)) {
    http.end();
    fail("WX-Link download size or status mismatch; Connect is unchanged", version.c_str());
    return false;
  }
  if (!Update.begin(image_size, U_FLASH)) {
    http.end();
    fail("Inactive firmware slot is unavailable; Connect is unchanged", version.c_str());
    return false;
  }

  mbedtls_sha256_context hash;
  mbedtls_sha256_init(&hash);
  bool ok = mbedtls_sha256_starts_ret(&hash, 0) == 0;
  uint8_t buffer[1024];
  size_t received = 0;
  const uint32_t started = millis();
  uint32_t last_data = started;
  auto* stream = http.getStreamPtr();
  while (ok && received < image_size) {
    if (millis() - started > 180000) { ok = false; break; }
    const size_t available = stream->available();
    if (available) {
      constexpr size_t probe = sizeof(esp_image_header_t) +
                               sizeof(esp_image_segment_header_t) + sizeof(uint32_t);
      const size_t count = received ? std::min(sizeof(buffer), std::min(available, size_t(image_size - received))) : probe;
      const auto read = transfer::readExact(*stream, buffer, count,
          [](){return millis();}, [](){vTaskDelay(pdMS_TO_TICKS(1));},
          [&](){return http.connected();}, 10000, 3000);
      if (read != transfer::ReadResult::Complete) { ok = false; break; }
      const int bytes = int(count);
      uint32_t app_magic = 0;
      if (!received && bytes >= int(probe))
        std::memcpy(&app_magic, buffer + probe - sizeof(app_magic), sizeof(app_magic));
      if (!received && (bytes < int(probe) || buffer[0] != 0xe9 ||
          reinterpret_cast<const esp_image_header_t*>(buffer)->chip_id != ESP_CHIP_ID_ESP32S3 ||
          app_magic != ESP_APP_DESC_MAGIC_WORD)) { ok = false; break; }
      if (mbedtls_sha256_update_ret(&hash, buffer, size_t(bytes)) != 0 ||
          Update.write(buffer, size_t(bytes)) != size_t(bytes)) { ok = false; break; }
      received += size_t(bytes);
      last_data = millis();
      const uint8_t progress = uint8_t(received * 100 / image_size);
      if (progress != state().progress)
        publish(Phase::Downloading, true, false, progress,
                "Downloading and verifying WX-Link", version.c_str());
    } else if (!http.connected() || millis() - last_data > 10000) {
      ok = false;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  uint8_t digest[32]{};
  char actual[65]{};
  if (mbedtls_sha256_finish_ret(&hash, digest) != 0) ok = false;
  mbedtls_sha256_free(&hash);
  http.end();
  for (unsigned i = 0; i < 32; ++i) std::snprintf(actual + i * 2, 3, "%02x", digest[i]);
  if (!ok || received != image_size || expected_sha != actual) {
    Update.abort();
    fail("WX-Link image validation failed; Connect is unchanged", version.c_str());
    return false;
  }
  if (!Update.end()) {
    Update.abort();
    fail("WX-Link image could not be activated; Connect is unchanged", version.c_str());
    return false;
  }

  restart_at_ms = millis() + 5000;
  publish(Phase::ReadyToRestart, false, true, 100,
          "WX-Link verified; restarting in a few seconds", version.c_str());
  return true;
}

void runMigration() {
  if (WiFi.status() != WL_CONNECTED || time(nullptr) <= 1700000000) {
    fail("Connect needs Home Wi-Fi and valid time for a secure transition");
    return;
  }
  // HTTPClient keeps a borrowed client pointer even after some failed end()
  // paths in Arduino 2.0.17. It must be destroyed before its TLS transport.
  BoundedTlsClient tls;
  HTTPClient http;
  String manifest_url, version, image_url, image_sha;
  uint32_t image_size = 0;
  const auto* target = esp_ota_get_next_update_partition(nullptr);
  if (!target || !readManifest(http, tls, manifest_url, target ? target->size : 0,
                               version, image_url, image_sha, image_size)) return;
  downloadAndInstall(http, tls, image_url, version, image_sha, image_size);
}

void worker(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    runMigration();
  }
}
}  // namespace

bool begin() {
  if (worker_task) return true;
  current = State{};
  const BaseType_t created = xTaskCreatePinnedToCore(worker, "wx-main-transition",
      16384, nullptr, 1, &worker_task, 0);
  if (created != pdPASS) {
    worker_task = nullptr;
    current.worker_ready = false;
    std::snprintf(current.message, sizeof(current.message), "Transition worker unavailable");
    return false;
  }
  current.worker_ready = true;
  return true;
}

State state() {
  State copy;
  portENTER_CRITICAL(&state_mux);
  copy = current;
  portEXIT_CRITICAL(&state_mux);
  return copy;
}

bool request() {
  portENTER_CRITICAL(&state_mux);
  if (!worker_task || !current.worker_ready || current.busy || current.restart_pending) {
    portEXIT_CRITICAL(&state_mux);
    return false;
  }
  current.phase = Phase::Checking;
  current.target_version[0] = '\0';
  current.busy = true;
  current.progress = 0;
  std::snprintf(current.message, sizeof(current.message), "WX-Link transition queued");
  portEXIT_CRITICAL(&state_mux);
  xTaskNotifyGive(worker_task);
  return true;
}

bool blocksCapture() {
  const State current_state = state();
  return current_state.busy || current_state.restart_pending;
}

bool restartDue() {
  const State current_state = state();
  return current_state.restart_pending && int32_t(millis() - restart_at_ms) >= 0;
}

}  // namespace wxconnect::connect_migration
