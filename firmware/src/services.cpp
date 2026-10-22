#include "services.h"
#include "setup_credentials.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <cstdio>
#include <time.h>

namespace services {
namespace {
Settings config;
State current;
Observation latest;
Preferences preferences;
bool preferences_ready = false;
bool mdns_ready = false;

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
  WiFi.mode(WIFI_AP_STA);
  current.ap_name = "WXConnect-" + String(uint32_t(ESP.getEfuseMac()), HEX).substring(4);
  current.ap = WiFi.softAP(current.ap_name.c_str(), config.setup_wifi_key.c_str());
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
  current.wifi = WiFi.status() == WL_CONNECTED;
  current.clock_valid = time(nullptr) > 1700000000;
  current.ip = current.wifi ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  current.ap = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
}

const Settings& settings() { return config; }
State state() { return current; }
bool clockValid() { return time(nullptr) > 1700000000; }
String portalAddress() { return current.ip.isEmpty() ? WiFi.softAPIP().toString() : current.ip; }
String portalUrl() { return "http://" + portalAddress(); }
bool localNameReady() { return mdns_ready; }
String setupWifiKey() { return config.setup_wifi_key; }
String adminPassword() { return config.setup_key; }
String setupSsid() { return current.ap_name; }
bool saveConnectSettings(const Settings& settings) {
  const bool network_changed = config.ssid != settings.ssid || config.wifi_password != settings.wifi_password;
  const Settings previous=config;
  config = settings;
  if (!persistSettings()) {config=previous;return false;}
  if (network_changed) {
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
