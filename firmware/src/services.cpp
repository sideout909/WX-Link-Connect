#include "services.h"
#include "setup_credentials.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <cstdio>
#include <cstring>
#include <utility>
#include <time.h>

namespace services {
namespace {
Settings config;
State current;
Observation latest;
Preferences preferences;
bool preferences_ready = false;
bool mdns_ready = false;
DNSServer setup_dns;
bool setup_dns_ready = false;
uint32_t last_mdns_attempt = 0;
String mdns_station_address;
WifiScan wifi_scan;
bool scan_started = false;
uint32_t scan_started_at = 0;

void pollWifiScan() {
  if (std::strcmp(wifi_scan.state, "running")) return;
  const int result = WiFi.scanComplete();
  if (result == WIFI_SCAN_RUNNING) {
    if (uint32_t(millis() - scan_started_at) < 15000) return;
    esp_wifi_scan_stop();
    wifi_scan.state = "timeout";
  } else if (result < 0) {
    wifi_scan.state = "failed";
  } else {
    for (int i = 0; i < result && i < 256; ++i) {
      String ssid = WiFi.SSID(i);
      if (!ssid.length()) continue;
      const int32_t rssi = WiFi.RSSI(i);
      size_t slot = 0;
      while (slot < wifi_scan.count && wifi_scan.networks[slot].ssid != ssid) ++slot;
      if (slot < wifi_scan.count && wifi_scan.networks[slot].rssi >= rssi) continue;
      if (slot == wifi_scan.count) {
        if (wifi_scan.count < wifi_scan.networks.size()) ++wifi_scan.count;
        else {
          slot = 0;
          for (size_t j = 1; j < wifi_scan.count; ++j)
            if (wifi_scan.networks[j].rssi < wifi_scan.networks[slot].rssi) slot = j;
          if (wifi_scan.networks[slot].rssi >= rssi) continue;
        }
      }
      wifi_scan.networks[slot] = {ssid, rssi, WiFi.encryptionType(i) != WIFI_AUTH_OPEN};
    }
    // Strongest first, with at most sixteen retained names and no BSSIDs.
    for (size_t i = 0; i < wifi_scan.count; ++i)
      for (size_t j = i + 1; j < wifi_scan.count; ++j)
        if (wifi_scan.networks[j].rssi > wifi_scan.networks[i].rssi)
          std::swap(wifi_scan.networks[i], wifi_scan.networks[j]);
    wifi_scan.state = "complete";
  }
  WiFi.scanDelete();
}

void startLocalName() {
  last_mdns_attempt = millis();
  mdns_ready = MDNS.begin("wxlink");
  if (mdns_ready) MDNS.addService("http", "tcp", 80);
}

void loadConnectSettings() {
  if (!preferences_ready) return;
  const String saved = preferences.getString("settings", "");
  if (saved.isEmpty()) return;
  StaticJsonDocument<1536> document;
  if (deserializeJson(document, saved) || !document.is<JsonObject>()) return;

  config.ssid = document["ssid"] | config.ssid;
  config.wifi_password = document["wifi_password"] | config.wifi_password;
  config.setup_key = document["setup_key"] | config.setup_key;
  config.setup_wifi_key = document["setup_wifi_key"] | config.setup_wifi_key;
  config.timezone = document["timezone"] | config.timezone;
  config.date_format = document["date_format"] | config.date_format;
  config.display_sleep_mode = document["display_sleep_mode"] | config.display_sleep_mode;
  config.display_theme = document["display_theme"] | config.display_theme;
  config.clock_24h = document["clock_24h"] | config.clock_24h;
  config.admin_custom = document["admin_custom"] | config.admin_custom;
  config.display_timeout_s = document["display_timeout_s"] | config.display_timeout_s;
  config.station_id = document["station_id"] | config.station_id;
  config.channel = document["channel"].is<const char*>()
                       ? document["channel"].as<String>()[0]
                       : config.channel;
  config.station_confirmed = document["station_confirmed"] | config.station_confirmed;
  config.radio_enabled = document["radio_enabled"] | config.radio_enabled;
}

void startWifi() {
  WiFi.setHostname("wxlink");
  WiFi.mode(WIFI_AP_STA);
  current.ap_name = "WXConnect-" + String(uint32_t(ESP.getEfuseMac()), HEX).substring(4);
  current.ap = WiFi.softAP(current.ap_name.c_str(), config.setup_wifi_key.c_str());
  if (current.ap) {
    WiFi.softAPsetHostname("wxlink");
    setup_dns_ready = setup_dns.start(53, "wxlink.local", WiFi.softAPIP());
  }
  startLocalName();
  if (!config.ssid.isEmpty()) WiFi.begin(config.ssid.c_str(), config.wifi_password.c_str());
}

bool persistSettings() {
  if (!preferences_ready) return false;
  StaticJsonDocument<1536> document;
  document["ssid"] = config.ssid;
  document["wifi_password"] = config.wifi_password;
  document["setup_key"] = config.setup_key;
  document["setup_wifi_key"] = config.setup_wifi_key;
  document["timezone"] = config.timezone;
  document["date_format"] = config.date_format;
  document["display_sleep_mode"] = config.display_sleep_mode;
  document["display_theme"] = config.display_theme;
  document["clock_24h"] = config.clock_24h;
  document["admin_custom"] = config.admin_custom;
  document["display_timeout_s"] = config.display_timeout_s;
  document["station_id"] = config.station_id;
  document["channel"] = String(config.channel);
  document["station_confirmed"] = config.station_confirmed;
  document["radio_enabled"] = config.radio_enabled;
  String saved;
  return serializeJson(document, saved) > 0 && preferences.putString("settings", saved) > 0;
}
}  // namespace

void begin() {
  preferences_ready = preferences.begin("wxconnect", false);
  if (preferences_ready) loadConnectSettings();
  setup_credentials::ensure(config.setup_wifi_key, config.setup_key, config.admin_custom, [](){
    char generated[25];std::snprintf(generated,sizeof(generated),"WX%08lx%08lx",
        static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
    return String(generated);
  });
  persistSettings();
  current.portal_tested = true;
  current.portal_passed = false;
  current.portal_test_status = "Capture setup interface is pending";
  startWifi();
  configTzTime(config.timezone.c_str(), "pool.ntp.org", "time.nist.gov");
}

void loop() {
  pollWifiScan();
  if (setup_dns_ready) setup_dns.processNextRequest();
  current.wifi = WiFi.status() == WL_CONNECTED;
  const String station_address = current.wifi ? WiFi.localIP().toString() : String();
  const bool address_changed = station_address != mdns_station_address;
  if (address_changed) {
    MDNS.end(); mdns_ready = false;
    mdns_station_address = station_address;
  }
  if (address_changed || (!mdns_ready && uint32_t(millis() - last_mdns_attempt) >= 5000))
    startLocalName();
  current.clock_valid = time(nullptr) > 1700000000;
  current.ip = current.wifi ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  current.ap = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
}

const Settings& settings() { return config; }
State state() { return current; }
bool clockValid() { return time(nullptr) > 1700000000; }
String portalAddress() { return current.ip.isEmpty() ? WiFi.softAPIP().toString() : current.ip; }
String portalUrl() {
  return mdns_ready || (!current.wifi && setup_dns_ready)
      ? String("http://wxlink.local") : "http://" + portalAddress();
}
bool localNameReady() { return mdns_ready; }
String setupWifiKey() { return config.setup_wifi_key; }
String adminPassword() { return config.setup_key; }
String setupSsid() { return current.ap_name; }
bool startWifiScan() {
  if (!std::strcmp(wifi_scan.state, "running") ||
      (scan_started && uint32_t(millis() - scan_started_at) < 10000)) return false;
  WiFi.scanDelete();
  wifi_scan = WifiScan{};
  scan_started = true; scan_started_at = millis();
  const int result = WiFi.scanNetworks(true, false, false, 120);
  wifi_scan.state = result == WIFI_SCAN_FAILED ? "failed" : "running";
  return result != WIFI_SCAN_FAILED;
}
const WifiScan& wifiScan() { return wifi_scan; }
bool saveConnectSettings(const Settings& settings, const String& new_admin, bool reconnect) {
  const bool network_changed = config.ssid != settings.ssid || config.wifi_password != settings.wifi_password;
  const Settings previous=config;
  config = settings;
  setup_credentials::chooseAdmin(config.setup_wifi_key, config.setup_key, config.admin_custom,
      new_admin, [](){
        char generated[25];std::snprintf(generated,sizeof(generated),"WX%08lx%08lx",
            static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
        return String(generated);
      });
  if (!persistSettings()) {config=previous;return false;}
  if (network_changed && reconnect) {
    WiFi.disconnect(false, false);
    if (config.ssid.length()) WiFi.begin(config.ssid.c_str(), config.wifi_password.c_str());
  }
  return true;
}
bool changeAdminPassword(const String& password) {
  const String previous_admin=config.setup_key,previous_wifi=config.setup_wifi_key;
  const bool previous_custom=config.admin_custom;
  setup_credentials::chooseAdmin(config.setup_wifi_key, config.setup_key, config.admin_custom,
      password, [](){
        char generated[25];std::snprintf(generated,sizeof(generated),"WX%08lx%08lx",
            static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
        return String(generated);
      });
  if(persistSettings())return true;
  config.setup_key=previous_admin;config.setup_wifi_key=previous_wifi;config.admin_custom=previous_custom;
  return false;
}
void observeDiscovery(const station_discovery::Packet&, uint32_t) {}

void publish(const Observation& observation) { latest = observation; }

void bindHistory(const weather::Store*, uint64_t (*)()) {}
}  // namespace services
