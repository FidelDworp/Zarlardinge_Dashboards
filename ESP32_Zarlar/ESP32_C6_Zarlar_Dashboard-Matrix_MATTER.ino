/* ============================================================
   Zarlar Dashboard v5.2
   ESP32-C6 (32-pin 16MB) @ 192.168.0.60
   Filip Delannoy

   BOARD INSTELLINGEN (Arduino IDE):
     Board        : ESP32-C6 Dev Module (espressif:arduino-esp32-master)
     Upload Speed : 921600
     Flash Size   : 16MB
     Partition    : Custom → partitions_16mb.csv (naast .ino)

   EERSTE GEBRUIK / FACTORY RESET:
     1. Upload sketch → verbindt automatisch met WiFi
     2. Bij geen WiFi → AP "Zarlar-Setup" verschijnt
     3. Verbind met "Zarlar-Setup" → browser opent /settings automatisch
     4. Vul WiFi SSID + wachtwoord in, sla op → herstart
     5. Daarna: open http://192.168.0.60/settings voor controllers

   NETWERK:
     Fixed IP  : 192.168.0.60
     Toegang   : http://192.168.0.60/ (geen mDNS — Matter intern)
     AP SSID   : Zarlar-Setup (bij geen WiFi)

   MATTER:
     - MatterOnOffPlugin: HOME/UIT (1 endpoint)
     - Pairing + reset via /settings pagina
     - Serial: reset-matter / reset-all / status / matrix-test

   STATUSMATRIX — 16x16 WS2812B op IO4 (Pixel-line, shield R3=33Ω):
     Rij 0  : S-HVAC
     Rij 1  : S-ECO
     Rij 2  : S-OUTSIDE (gereserveerd, dim paars)
     Rij 3  : S-ACCESS  (gereserveerd, dim paars)
     Rij 4  : Separator (amber)
     Rij 5  : R-BandB
     Rij 6  : R-BADK
     Rij 7  : R-INKOM
     Rij 8  : R-KEUKEN
     Rij 9  : R-WASPL
     Rij 10 : R-EETPL / TESTROOM (controller idx 13)
     Rij 11 : R-ZITPL
     Rij 12-15: leeg

   ⚠️  SERPENTINE RICHTING: matrix wordt bij eerste gebruik getest via
       /matrix_test of Serial commando 'matrix-test'. Pas MATRIX_FLIP_H
       aan als kolommen gespiegeld zijn.

   14apr26        v5.2  ROOT CAUSE FIX heap-fragmentatie:
                        struct Controller: String last_json → char last_json[700] (BSS).
                        Poll functies: HTTPClient stream → direct lezen in char buffer.
                        logControllerToSheets(): snprintf POST-body, geen String-allocatie.
                        getStatusJson(): strstr/atoi in plaats van String.indexOf/substring.
                        jF()/jI(): const char* i.p.v. const String& → geen String-kopie.
                        Render-functies: const char* j i.p.v. const String& j.
                        Reboot-knop toegevoegd aan /settings.
                        Auto-herstart bij largest_block < 25KB voor > 60s.
   02apr26        v5.1  5 Controllers vervangen door "static damage!": ZITPL EETPL, OUTSIDE, ACCESS en INKOM (Nieuwe IDs)
   01apr26        v5.0  Automatische ESP32/Photon fallback per matrix-rij.
                        MatrixRowDef struct: esp_idx + photon_idx + sys_idx.
                        MROW volgorde gecorrigeerd: stemt nu exact overeen
                        met SVG labelsheet (rij 5=BandB...11=ZITPL).
                        updateMatrix(): ESP32 actief? → ESP32. Anders Photon.
                        Anders zwart. Geen reflash nodig bij transitie.
                        Serial 'status' toont per rij welke controller actief is.
   01apr26        v4.9  MROW fix: Photon indices 16-19 → 15-18 (off-by-one).
                        Rijen 2,3 naar -1 (gereserveerd, geen PROBLEEM melding meer).
   01apr26        v4.8  fetchPhoton() vereenvoudigd: worker /sensor ipv Particle API.
                        getToken() + PT variabele verwijderd — token niet meer in browser.
                        Serial hint 'status' na poll cyclus.
   01apr26        v4.7  Photon poll via Cloudflare Worker. renderPhotonRow().
                        renderPhotonRow() met Photon JSON keys (a,d,e,g,i,j,k,l,q,s,t,u,x).
                        MROW: rijen 5-8 → P-Badkamer/Inkom/Keuken/Waspl tijdens transitie.
                        Heap-guard 30KB voor TLS poll.
   01apr26        v4.6  Separator + gereserveerde rijen → zwart.
                        MROW rij 10: R-EETPL idx 13→11 (geverifieerd via status).
                        Serial 'status': matrix ctrl-index debug toegevoegd.
                        Verouderde HVAC key-waarschuwing verwijderd.
   26mar26        v4.5  ECO renderer herwerkt: collector→pomp→6 boilerlagen→energie→heap.
                        Pomp relay weg (PWM volstaat), HOME global weg, FreeHeap% toegevoegd.
   26mar26        v4.4  ROOM col13: pixel_on_str wit evenredig met actieve pixels.
   26mar26        v4.3  ROOM col10: werkelijke kamerkleur RGB. Col11: dag=geel/nacht=purper.
                        Col12: LDR1 geel omgekeerd evenredig. Col13: TSL2561 lux amber log.
   26mar26        v4.2  Boot-animatie: rode pixel door HVAC+ECO (rij 0-1),
                        blauwe pixel door ROOM-rijen (5-11). SVG labels v3.
   26mar26        v4.1  HVAC_KEY_* defines gecorrigeerd op basis van
                        geverifieerde v1.19 sketch (keys p,q,r,s,t,u,v,
                        y,aa,x,h,ae,ac). §3.6 overnamedocument aangevuld.
   26mar26        v4.0  16x16 NeoPixel statusmatrix op IO4.
                        Helderheid instelbaar via /settings UI.
                        /matrix_test endpoint + Serial 'matrix-test'.
                        Boot-animatie: sweep per rij groen.
                        Renderer per controller type (room/eco/hvac/reserved).
   17mar26        v3.0  Matter: MatterOnOffPlugin HOME/UIT. #define Serial Serial0.
                        ESPmDNS verwijderd. matterNuclearReset() — settings bewaard.
                        Matter sectie in /settings. Serial commando's.
   17mar26        v2.9.1 Bugfix: ECO RSSI-key "p" ipv "ac".
   17mar26        v2.9  RSSI op dashboard-knoppen.
   17mar26        v2.8  Header hersteld. Ping port 80→53.
   17mar26        v2.7  Matter-voorbereiding: mDNS verwijderd, 16MB board.
   17mar26        v2.6  WiFi Strength Tester: /wifi pagina.
   16mar26        v2.5  /info → /settings. max_rows naar GAS.
   16mar26        v2.4  HOME/UIT broadcast naar alle rooms.
   11mar26        v2.3  WiFi sleep uit. AP + captive portal.
   11mar26        v2.0  Volledig herschreven.
   ============================================================ */

// ⚠️ Verplicht voor ESP32-C6 (RISC-V) — vóór alle #include statements
#define Serial Serial0

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <Matter.h>
#include <MatterEndPoints/MatterOnOffPlugin.h>
#include <Adafruit_NeoPixel.h>

// ============================================================
// CONTROLLER TYPES & STATUS
// ============================================================
#define TYPE_PHOTON  0
#define TYPE_SYSTEM  1
#define TYPE_ROOM    2

#define STATUS_INACTIVE  0   // grijs
#define STATUS_PENDING   1   // geel
#define STATUS_ONLINE    2   // groen
#define STATUS_OFFLINE   3   // rood

// ============================================================
// CONTROLLER STRUCT
// v5.2: last_json is char[700] in BSS — geen heap-allocatie meer.
// Eliminineert de graduele heap-fragmentatie die elke poll-cyclus
// optrad door String realloc van ~10 actieve controllers.
// BSS-kost: 22 × 700 = 15.4 KB (vast, geen fragmentatie).
// ============================================================
struct Controller {
  const char* name;
  const char* ip;
  const char* photon_id;
  int         type;
  bool        active;
  int         status;
  String      sheets_url;
  char        last_json[700];   // ← v5.2: was String last_json
  unsigned long last_poll;
};

// ============================================================
// CONTROLLER TABEL
// ============================================================
#define NUM_CONTROLLERS 22

Controller controllers[NUM_CONTROLLERS] = {
  {"S-HVAC",    "192.168.0.70", "", TYPE_SYSTEM, true,  STATUS_PENDING,  "", {}, 0},
  {"S-ECO",     "192.168.0.71", "", TYPE_SYSTEM, true,  STATUS_PENDING,  "", {}, 0},
  {"S-OUTSIDE", "192.168.0.72", "", TYPE_SYSTEM, false, STATUS_INACTIVE, "", {}, 0},
  {"S-ACCESS",  "192.168.0.82", "", TYPE_SYSTEM, false, STATUS_INACTIVE, "", {}, 0},
  {"FUT1",      "192.168.0.73", "", TYPE_SYSTEM, false, STATUS_INACTIVE, "", {}, 0},
  {"FUT2",      "192.168.0.74", "", TYPE_SYSTEM, false, STATUS_INACTIVE, "", {}, 0},
  {"R-BandB",   "192.168.0.75", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", {}, 0},
  {"R-BADK",    "192.168.0.76", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", {}, 0},
  {"R-INKOM",   "192.168.0.77", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", {}, 0},
  {"R-KEUKEN",  "192.168.0.78", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", {}, 0},
  {"R-WASPL",   "192.168.0.79", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", {}, 0},
  {"R-EETPL",   "192.168.0.80", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", {}, 0},
  {"R-ZITPL",   "192.168.0.81", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", {}, 0},
  {"R-TESTROOM","192.168.0.80", "", TYPE_ROOM,   true,  STATUS_PENDING,  "", {}, 0},
  {"P-BandB",   "", "30002c000547343233323032", TYPE_PHOTON, true,  STATUS_PENDING,  "", {}, 0},
  {"P-Badkamer","", "5600420005504b464d323520", TYPE_PHOTON, true,  STATUS_PENDING,  "", {}, 0},
  {"P-Inkom",   "", "2c0026000747343232363230", TYPE_PHOTON, true,  STATUS_PENDING,  "", {}, 0},
  {"P-Keuken",  "", "310017001647373335333438", TYPE_PHOTON, true,  STATUS_PENDING,  "", {}, 0},
  {"P-Waspl",   "", "33004f000e504b464d323520", TYPE_PHOTON, true,  STATUS_PENDING,  "", {}, 0},
  {"P-Eetpl",   "", "3c0030000a47353138383138", TYPE_PHOTON, true,  STATUS_PENDING,  "", {}, 0},
  {"P-Zitpl",   "", "200033000547373336323230", TYPE_PHOTON, true,  STATUS_PENDING,  "", {}, 0},
  {"P-TESTROOM","", "200033000547373336323230", TYPE_PHOTON, true,  STATUS_PENDING,  "", {}, 0},
};

// ============================================================
// NVS
// ============================================================
Preferences preferences;
const char* NVS_NS            = "zarlar";
const char* NVS_WIFI_SSID     = "wifi_ssid";
const char* NVS_WIFI_PASS     = "wifi_pass";
const char* NVS_POLL_MIN      = "poll_min";
const char* NVS_ROOM_SCRIPT   = "room_script";
const char* NVS_HOME_GLOBAL   = "home_global";
const char* NVS_MATRIX_BRIGHT = "matrix_br";

// ============================================================
// GLOBALS
// ============================================================
WebServer  server(80);
DNSServer  dnsServer;

String wifi_ssid       = "";
String wifi_pass       = "";
int    poll_minutes    = 10;
String room_script_url = "";

bool          ap_mode          = false;
unsigned long last_poll_cycle  = 0;
int           poll_index       = 0;
bool          polling_active   = false;
unsigned long poll_step_timer  = 0;
bool          home_mode_global = false;

// WiFi tester — 3 primitieven
static bool wt_scan_pending = false;
static int  wt_snap_rssi    = 0;
static int  wt_snap_ping    = -1;

// Matter
MatterOnOffPlugin matter_home;
bool              matter_nuclear_reset_requested = false;
unsigned long     last_matter_update             = 0;

// ============================================================
// STATUSMATRIX — v4.0
// 16×16 WS2812B op IO4 via Pixel-line (shield R3=33Ω aanwezig)
// Voeding: 5V rechtstreeks op de matrix (NIET via shield PTC 500mA)
//
// Serpentine adressering (standaard AliExpress 16×16 panel):
//   Datadraden = onderaan → pixel 0 = links-onder
//   Rij 0 (onder): pixels 0→15 links→rechts
//   Rij 1: pixels 31→16 rechts→links
//   etc.
// Logisch rij 0 = bovenaan scherm = fysieke rij 15
//
// ⚠️ Als pixels gespiegeld zijn: stel MATRIX_FLIP_H = true in
// ============================================================
#define MATRIX_PIN    4
#define MATRIX_WIDTH  16
#define MATRIX_HEIGHT 16
#define MATRIX_LEDS   256
#define MATRIX_FLIP_H false

Adafruit_NeoPixel matrix(MATRIX_LEDS, MATRIX_PIN, NEO_GRB + NEO_KHZ800);
uint8_t           matrix_brightness  = 60;
unsigned long     last_matrix_update = 0;

// ============================================================
// MATRIX RIJ-MAPPING — automatische ESP32/Photon fallback
// ============================================================
struct MatrixRowDef {
  int  esp_idx;
  int  photon_idx;
  int  sys_idx;
};

const MatrixRowDef MROW[MATRIX_HEIGHT] = {
  {  -1,  -1,   0 }, // rij  0: S-HVAC
  {  -1,  -1,   1 }, // rij  1: S-ECO
  {  -1,  -1,  -1 }, // rij  2: S-OUTSIDE — gereserveerd
  {  -1,  -1,  -1 }, // rij  3: S-ACCESS  — gereserveerd
  {  -1,  -1,  -2 }, // rij  4: separator
  {   6,  14,  -1 }, // rij  5: R-BandB  / P-BandB
  {   7,  15,  -1 }, // rij  6: R-BADK   / P-Badkamer
  {   8,  16,  -1 }, // rij  7: R-INKOM  / P-Inkom
  {   9,  17,  -1 }, // rij  8: R-KEUKEN / P-Keuken
  {  10,  18,  -1 }, // rij  9: R-WASPL  / P-Waspl
  {  11,  19,  -1 }, // rij 10: R-EETPL  / P-Eetpl
  {  12,  20,  -1 }, // rij 11: R-ZITPL  / P-Zitpl
  {  -1,  -1,  -1 }, // rij 12–15: leeg
  {  -1,  -1,  -1 },
  {  -1,  -1,  -1 },
  {  -1,  -1,  -1 },
};

// ============================================================
// HVAC JSON KEYS — geverifieerd 26 maart 2026 via v1.19 sketch
// ============================================================
#define HVAC_KEY_RSSI     "ac"
#define HVAC_KEY_CIRCUIT1 "p"
#define HVAC_KEY_CIRCUIT2 "q"
#define HVAC_KEY_CIRCUIT3 "r"
#define HVAC_KEY_CIRCUIT4 "s"
#define HVAC_KEY_CIRCUIT5 "t"
#define HVAC_KEY_CIRCUIT6 "u"
#define HVAC_KEY_CIRCUIT7 "v"
#define HVAC_KEY_PUMP_SCH "y"
#define HVAC_KEY_PUMP_WON "aa"
#define HVAC_KEY_VENT     "x"
#define HVAC_KEY_TEMP1    "h"
#define HVAC_KEY_HEAP     "ae"

// ============================================================
// NVS LADEN / OPSLAAN
// ============================================================
void loadNVS() {
  preferences.begin(NVS_NS, true);
  wifi_ssid         = preferences.getString(NVS_WIFI_SSID, "");
  wifi_pass         = preferences.getString(NVS_WIFI_PASS, "");
  poll_minutes      = preferences.getInt(NVS_POLL_MIN, 10);
  room_script_url   = preferences.getString(NVS_ROOM_SCRIPT, "");
  home_mode_global  = preferences.getBool(NVS_HOME_GLOBAL, false);
  matrix_brightness = preferences.getUChar(NVS_MATRIX_BRIGHT, 60);
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    char ka[12], ku[12];
    snprintf(ka, sizeof(ka), "c%d_act", i);
    snprintf(ku, sizeof(ku), "c%d_url", i);
    controllers[i].active     = preferences.getBool(ka, controllers[i].active);
    controllers[i].sheets_url = preferences.getString(ku, "");
    controllers[i].status     = controllers[i].active ? STATUS_PENDING : STATUS_INACTIVE;
  }
  preferences.end();
}

void saveNVS() {
  preferences.begin(NVS_NS, false);
  preferences.putString(NVS_WIFI_SSID,      wifi_ssid);
  preferences.putString(NVS_WIFI_PASS,      wifi_pass);
  preferences.putInt   (NVS_POLL_MIN,       poll_minutes);
  preferences.putString(NVS_ROOM_SCRIPT,    room_script_url);
  preferences.putUChar (NVS_MATRIX_BRIGHT,  matrix_brightness);
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    char ka[12], ku[12];
    snprintf(ka, sizeof(ka), "c%d_act", i);
    snprintf(ku, sizeof(ku), "c%d_url", i);
    preferences.putBool  (ka, controllers[i].active);
    preferences.putString(ku, controllers[i].sheets_url);
  }
  preferences.end();
}

// ============================================================
// WIFI VERBINDEN
// ============================================================
void connectWiFi() {
  if (wifi_ssid.length() == 0) {
    Serial.println("Geen WiFi geconfigureerd → AP mode");
    startAP();
    return;
  }
  Serial.print("WiFi verbinden met " + wifi_ssid);
  WiFi.mode(WIFI_STA);
  IPAddress ip(192,168,0,60), gw(192,168,0,1), sn(255,255,255,0), dns(8,8,8,8);
  WiFi.config(ip, gw, sn, gw, dns);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500); Serial.print("."); retries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    esp_wifi_set_ps(WIFI_PS_NONE);
    wifi_config_t wifi_cfg;
    esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);
    wifi_cfg.sta.listen_interval = 1;
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_pm_config_t pm_cfg = { .max_freq_mhz=160, .min_freq_mhz=160, .light_sleep_enable=false };
    esp_pm_configure(&pm_cfg);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    Serial.println("✓ WiFi verbonden: " + WiFi.localIP().toString());
    Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
    ap_mode = false;
  } else {
    Serial.println("✗ WiFi mislukt → AP mode");
    startAP();
  }
}

// ============================================================
// AP MODE + CAPTIVE PORTAL
// ============================================================
void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Zarlar-Setup");
  IPAddress ap_ip = WiFi.softAPIP();
  dnsServer.start(53, "*", ap_ip);
  ap_mode = true;
  Serial.println("AP mode: SSID=Zarlar-Setup  IP=" + ap_ip.toString());
}

// ============================================================
// POLL ÉÉN ESP32 CONTROLLER
// v5.2: HTTPClient stream → direct lezen in char last_json[700].
//       Geen String-allocatie meer — elimineert heap-fragmentatie.
// ============================================================
void pollESP32Controller(int i) {
  if (!controllers[i].active) return;
  if (strlen(controllers[i].ip) == 0) return;
  String url = "http://" + String(controllers[i].ip) + "/json";
  Serial.printf("[Poll] %s → %s\n", controllers[i].name, url.c_str());
  HTTPClient http;
  http.begin(url);
  http.setTimeout(4000);
  http.setConnectTimeout(2000);
  int code = http.GET();
  if (code == 200) {
    WiFiClient* stream = http.getStreamPtr();
    size_t n = stream->readBytes(controllers[i].last_json,
                                 sizeof(controllers[i].last_json) - 1);
    controllers[i].last_json[n] = '\0';
    controllers[i].status    = STATUS_ONLINE;
    controllers[i].last_poll = millis();
    Serial.printf("  ✓ %s online (%d bytes)\n", controllers[i].name, (int)n);
    http.end();
    delay(500);
    logControllerToSheets(i);
  } else {
    controllers[i].status = STATUS_OFFLINE;
    controllers[i].last_json[0] = '\0';
    Serial.printf("  ✗ %s offline (HTTP %d)\n", controllers[i].name, code);
    http.end();
  }
}

// ============================================================
// POLL PHOTON CONTROLLER via Cloudflare Worker
// v5.2: HTTPClient stream → direct lezen in char last_json[700].
// ============================================================
#define PHOTON_WORKER_HOST "controllers-diagnose.filip-delannoy.workers.dev"
#define PHOTON_WORKER_PATH "/sensor?id="

void pollPhotonController(int i) {
  if (!controllers[i].active) return;
  if (strlen(controllers[i].photon_id) == 0) return;

  if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 30000) {
    Serial.printf("  [Photon] %s overgeslagen — heap te laag\n", controllers[i].name);
    return;
  }

  String url = "https://" + String(PHOTON_WORKER_HOST) + String(PHOTON_WORKER_PATH)
               + String(controllers[i].photon_id);
  Serial.printf("[Poll] %s → worker/%s\n", controllers[i].name, controllers[i].photon_id);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(6000);
  http.setConnectTimeout(4000);
  int code = http.GET();
  if (code == 200) {
    WiFiClient* stream = http.getStreamPtr();
    size_t n = stream->readBytes(controllers[i].last_json,
                                 sizeof(controllers[i].last_json) - 1);
    controllers[i].last_json[n] = '\0';
    http.end();
    if (strstr(controllers[i].last_json, "\"online\":1") != nullptr) {
      controllers[i].status    = STATUS_ONLINE;
      controllers[i].last_poll = millis();
      Serial.printf("  ✓ %s online (%d bytes)\n", controllers[i].name, (int)n);
    } else {
      controllers[i].status = STATUS_OFFLINE;
      controllers[i].last_json[0] = '\0';
      Serial.printf("  ✗ %s offline (Photon niet verbonden)\n", controllers[i].name);
    }
  } else {
    http.end();
    controllers[i].status = STATUS_OFFLINE;
    controllers[i].last_json[0] = '\0';
    Serial.printf("  ✗ %s worker fout (HTTP %d)\n", controllers[i].name, code);
  }
}

void setAllRoomsHomeMode(int mode) {
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    if (!controllers[i].active) continue;
    if (controllers[i].type != TYPE_ROOM) continue;
    if (strlen(controllers[i].ip) == 0) continue;
    char url[48];
    snprintf(url, sizeof(url), "http://%s/set_home?v=%d", controllers[i].ip, mode);
    HTTPClient http;
    http.begin(url);
    http.setTimeout(2000);
    http.setConnectTimeout(1000);
    int code = http.GET();
    Serial.printf("[HOME] %s → %s (HTTP %d)\n", controllers[i].name, mode ? "HOME" : "UIT", code);
    http.end();
  }
}

// ============================================================
// LOG NAAR GOOGLE SHEETS
// v5.2: snprintf POST-body naar char buf op stack — geen String-allocatie.
// ============================================================
void logControllerToSheets(int i) {
  const char* url = (controllers[i].type == TYPE_ROOM)
                    ? room_script_url.c_str()
                    : controllers[i].sheets_url.c_str();
  if (url[0] == '\0') return;
  if (controllers[i].last_json[0] == '\0') return;

  // Bouw POST-body: {"room":"naam", rest van json zonder openende {
  // last_json begint met '{' — skip die en vervang door onze header
  char postbuf[750];
  snprintf(postbuf, sizeof(postbuf), "{\"room\":\"%s\",%s",
           controllers[i].name,
           controllers[i].last_json + 1);  // +1: sla '{' over

  Serial.printf("  [Sheets] POST %s...\n", controllers[i].name);
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);
  int code = http.POST((uint8_t*)postbuf, strlen(postbuf));
  Serial.printf("  [Sheets] %s %s\n", controllers[i].name,
                (code == 200 || code == 302) ? "OK ✓" : "fout");
  http.end();
}

// ============================================================
// POLL CYCLUS — gespreid 2s per controller
// ============================================================
void handlePolling() {
  if (ap_mode) return;
  unsigned long now = millis();
  if (!polling_active) {
    if (now - last_poll_cycle >= (unsigned long)poll_minutes * 60000UL) {
      last_poll_cycle = now;
      polling_active  = true;
      poll_index      = 0;
      poll_step_timer = now;
      Serial.println("\n=== START POLL CYCLUS ===");
    }
    return;
  }
  if (now - poll_step_timer < 2000) return;
  poll_step_timer = now;
  while (poll_index < NUM_CONTROLLERS) {
    if (controllers[poll_index].active) {
      if (controllers[poll_index].type != TYPE_PHOTON &&
          strlen(controllers[poll_index].ip) > 0) {
        pollESP32Controller(poll_index);
        poll_index++;
        return;
      }
      if (controllers[poll_index].type == TYPE_PHOTON &&
          strlen(controllers[poll_index].photon_id) > 0) {
        pollPhotonController(poll_index);
        poll_index++;
        return;
      }
    }
    poll_index++;
  }
  polling_active = false;
  Serial.println(F("=== POLL CYCLUS KLAAR === (typ 'status' voor diagnostiek)\n"));
  updateMatrix();
}

// ============================================================
// MATTER — Nuclear reset (settings bewaard, Matter-NVS gewist)
// ============================================================
void matterNuclearReset() {
  Serial.println(F("\n=== MATTER NUCLEAR RESET ==="));
  preferences.begin(NVS_NS, true);
  String bk_ssid   = preferences.getString(NVS_WIFI_SSID,    "");
  String bk_pass   = preferences.getString(NVS_WIFI_PASS,    "");
  int    bk_poll   = preferences.getInt   (NVS_POLL_MIN,     10);
  String bk_script = preferences.getString(NVS_ROOM_SCRIPT,  "");
  bool   bk_home   = preferences.getBool  (NVS_HOME_GLOBAL,  false);
  uint8_t bk_mbr   = preferences.getUChar (NVS_MATRIX_BRIGHT, 60);
  bool   bk_act[NUM_CONTROLLERS];
  String bk_url[NUM_CONTROLLERS];
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    char ka[12], ku[12];
    snprintf(ka, sizeof(ka), "c%d_act", i);
    snprintf(ku, sizeof(ku), "c%d_url", i);
    bk_act[i] = preferences.getBool  (ka, controllers[i].active);
    bk_url[i] = preferences.getString(ku, "");
  }
  preferences.end();
  nvs_flash_erase();
  nvs_flash_init();
  preferences.begin(NVS_NS, false);
  preferences.putString(NVS_WIFI_SSID,     bk_ssid);
  preferences.putString(NVS_WIFI_PASS,     bk_pass);
  preferences.putInt   (NVS_POLL_MIN,      bk_poll);
  preferences.putString(NVS_ROOM_SCRIPT,   bk_script);
  preferences.putBool  (NVS_HOME_GLOBAL,   bk_home);
  preferences.putUChar (NVS_MATRIX_BRIGHT, bk_mbr);
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    char ka[12], ku[12];
    snprintf(ka, sizeof(ka), "c%d_act", i);
    snprintf(ku, sizeof(ku), "c%d_url", i);
    preferences.putBool  (ka, bk_act[i]);
    preferences.putString(ku, bk_url[i]);
  }
  preferences.end();
  Serial.println(F("  Settings teruggeschreven. Herstart..."));
  delay(300);
  ESP.restart();
}

// ============================================================
// WIFI TESTER
// ============================================================
int wtPing() {
  WiFiClient c;
  unsigned long t = millis();
  bool ok = c.connect(IPAddress(192,168,0,1), 53, 500);
  int ms = ok ? (int)(millis() - t) : -1;
  if (ok) c.stop();
  return ms;
}

void handleWifiJson() {
  char buf[22];
  int r = ap_mode ? 0 : WiFi.RSSI();
  int q = ap_mode ? 0 : constrain(2 * (r + 100), 0, 100);
  snprintf(buf, sizeof(buf), "{\"r\":%d,\"q\":%d}", r, q);
  server.send(200, "application/json", buf);
}

void handleWifiSnap() {
  if (ap_mode) { server.send(200, "application/json", "{\"status\":\"ap\"}"); return; }
  if (server.hasArg("start")) {
    wt_snap_rssi = WiFi.RSSI();
    wt_snap_ping = wtPing();
    if (!wt_scan_pending) { WiFi.scanNetworks(true); wt_scan_pending = true; }
    server.send(200, "application/json", "{\"status\":\"scanning\"}");
    return;
  }
  if (!wt_scan_pending) { server.send(200, "application/json", "{\"status\":\"idle\"}"); return; }
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) { server.send(200, "application/json", "{\"status\":\"scanning\"}"); return; }
  String j = "{\"status\":\"ok\",\"rssi\":";
  j += wt_snap_rssi; j += ",\"ping\":"; j += wt_snap_ping; j += ",\"nets\":[";
  String mySsid = WiFi.SSID();
  int found = 0;
  if (n > 0) {
    for (int k = 0; k < n && found < 4; k++) {
      if (WiFi.SSID(k) == mySsid) continue;
      if (found++) j += ",";
      j += "{\"s\":\""; j += WiFi.SSID(k); j += "\",\"r\":"; j += WiFi.RSSI(k); j += "}";
    }
  }
  j += "]}";
  WiFi.scanDelete();
  wt_scan_pending = false;
  server.send(200, "application/json", j);
}

// ============================================================
// STATUSMATRIX — helper functies (v4.0)
// ============================================================
int matPxIdx(int row, int col) {
  if (MATRIX_FLIP_H) col = (MATRIX_WIDTH - 1) - col;
  int phys_row = (MATRIX_HEIGHT - 1) - row;
  if (phys_row % 2 == 0) return phys_row * MATRIX_WIDTH + col;
  else                   return phys_row * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - col);
}

void matPx(int row, int col, uint8_t r, uint8_t g, uint8_t b) {
  if (row < 0 || row >= MATRIX_HEIGHT || col < 0 || col >= MATRIX_WIDTH) return;
  matrix.setPixelColor(matPxIdx(row, col), r, g, b);
}

void matRow(int row, uint8_t r, uint8_t g, uint8_t b) {
  for (int c = 0; c < MATRIX_WIDTH; c++) matPx(row, c, r, g, b);
}

void statusPx(int row, int col, int status) {
  switch (status) {
    case STATUS_ONLINE:   matPx(row, col, 0,   200, 0);   break;
    case STATUS_OFFLINE:  matPx(row, col, 200, 0,   0);   break;
    case STATUS_PENDING:  matPx(row, col, 200, 140, 0);   break;
    default:              matPx(row, col, 12,  12,  12);  break;
  }
}

void tempPx(int row, int col, float t, float cold=18.0, float warm=22.0, float hot=26.0) {
  if (t < 1.0 || t > 80.0) { matPx(row, col, 20, 0, 20); return; }
  if (t < cold) { matPx(row, col, 0, 0, 200); return; }
  if (t < warm) {
    float f = (t - cold) / (warm - cold);
    matPx(row, col, 0, (uint8_t)(200*f), (uint8_t)(200*(1-f)));
    return;
  }
  if (t < hot) {
    float f = (t - warm) / (hot - warm);
    matPx(row, col, (uint8_t)(200*f), 200, 0);
    return;
  }
  matPx(row, col, 200, 0, 0);
}

void boilerTempPx(int row, int col, float t) {
  tempPx(row, col, t, 40.0, 60.0, 75.0);
}

void rssiPx(int row, int col, int rssi) {
  if      (rssi >= -60) matPx(row, col, 0,   200, 0);
  else if (rssi >= -70) matPx(row, col, 150, 200, 0);
  else if (rssi >= -80) matPx(row, col, 200, 80,  0);
  else                  matPx(row, col, 200, 0,   0);
}

void heapPx(int row, int col, float kb) {
  if      (kb > 35) matPx(row, col, 0,   200, 0);
  else if (kb > 25) matPx(row, col, 180, 150, 0);
  else              matPx(row, col, 200, 0,   0);
}

void cyanLevel(int row, int col, int pct_0_100) {
  if (pct_0_100 <= 0) { matPx(row, col, 8, 8, 8); return; }
  uint8_t v = (uint8_t)map(pct_0_100, 1, 100, 40, 200);
  matPx(row, col, 0, v/2, v);
}

// ============================================================
// JSON key extractor — v5.2: const char* i.p.v. const String&
// Werkt direct op char last_json[] via strstr/atof/atoi.
// Geen String-kopie, geen heap-allocatie.
// ============================================================
static float jF(const char* j, const char* key, float def = 0.0f) {
  char buf[28];
  snprintf(buf, sizeof(buf), "\"%s\":", key);
  const char* pos = strstr(j, buf);
  if (!pos) return def;
  return (float)atof(pos + strlen(buf));
}
static int jI(const char* j, const char* key, int def = 0) {
  char buf[28];
  snprintf(buf, sizeof(buf), "\"%s\":", key);
  const char* pos = strstr(j, buf);
  if (!pos) return def;
  return atoi(pos + strlen(buf));
}

// ============================================================
// MATRIX — ROOM rij renderer (v4.0)
// v5.2: const char* j i.p.v. const String& j
// ============================================================
void renderRoomRow(int row, int ci) {
  const char* j = controllers[ci].last_json;
  bool online    = (controllers[ci].status == STATUS_ONLINE);

  statusPx(row, 0, controllers[ci].status);

  if (!online) {
    for (int c = 1; c < MATRIX_WIDTH; c++) matPx(row, c, 25, 0, 0);
    return;
  }

  // Col 1: HOME
  int home_sw = jI(j, "v");
  matPx(row, 1, home_sw ? 0 : 15, home_sw ? 30 : 15, home_sw ? 180 : 15);

  // Col 2: Verwarming AAN
  int heat = jI(j, "b");
  matPx(row, 2, heat ? 200 : 10, heat ? 30 : 10, 10);

  // Col 3: Temperatuur DHT22
  tempPx(row, 3, jF(j, "e", 0.0f));

  // Col 4: Vochtigheid
  int humi = jI(j, "h");
  if      (humi < 10)  matPx(row, 4, 20, 0, 20);
  else if (humi < 50)  matPx(row, 4, 0,  200, 0);
  else if (humi < 70)  matPx(row, 4, 180, 180, 0);
  else if (humi < 85)  matPx(row, 4, 200, 60,  0);
  else                 matPx(row, 4, 200, 0,   0);

  // Col 5: CO2
  int co2 = jI(j, "k");
  if      (co2 == 0)    matPx(row, 5, 15, 15, 15);
  else if (co2 < 800)   matPx(row, 5, 0,  200, 0);
  else if (co2 < 1200)  matPx(row, 5, 180, 180, 0);
  else if (co2 < 1500)  matPx(row, 5, 200, 80,  0);
  else                  matPx(row, 5, 200, 0,   0);

  // Col 6: MOV1
  int m1 = jI(j, "y");
  matPx(row, 6, m1 ? 200 : 12, m1 ? 180 : 12, m1 ? 120 : 12);

  // Col 7: MOV2
  int m2 = jI(j, "z");
  matPx(row, 7, m2 ? 200 : 12, m2 ? 180 : 12, m2 ? 120 : 12);

  // Col 8: TSTAT
  int tstat = jI(j, "d");
  matPx(row, 8, tstat ? 200 : 10, tstat ? 60 : 10, 10);

  // Col 9: Dauw alert
  int dew = jI(j, "j");
  matPx(row, 9, dew ? 200 : 0, 0, dew ? 0 : 20);

  // Col 10: NeoPixel kamerkleur
  {
    int pr = jI(j, "q"), pg = jI(j, "r"), pb = jI(j, "s");
    if (pr + pg + pb > 0) matPx(row, 10, (uint8_t)(pr/2), (uint8_t)(pg/2), (uint8_t)(pb/2));
    else                  matPx(row, 10, 10, 10, 10);
  }

  // Col 11: Dag / Nacht
  {
    int night = jI(j, "o");
    if (night) matPx(row, 11, 40, 0, 60);
    else       matPx(row, 11, 180, 150, 0);
  }

  // Col 12: LDR1 omgevingslicht
  {
    int ldr = jI(j, "m");
    int br = (100 - constrain(ldr, 0, 100)) * 2;
    matPx(row, 12, (uint8_t)(br), (uint8_t)(br * 0.75f), 0);
  }

  // Col 13: Aantal actieve pixels (key "t" = pixel_on_str, bv. "P=00111")
  // v5.2: strstr/strchr i.p.v. String.indexOf/substring — geen heap-allocatie
  {
    uint8_t br = 0;
    const char* eq = strstr(j, "\"t\":\"");
    if (eq) {
      const char* start = eq + 5;           // na "t":"
      const char* end   = strchr(start, '"');
      if (end && end > start) {
        const char* sep = strchr(start, '=');
        if (sep && sep < end) {
          sep++;
          int total = (int)(end - sep);
          int ones  = 0;
          for (const char* p = sep; p < end; p++) if (*p == '1') ones++;
          if (total > 0) br = (uint8_t)((ones * 220) / total);
        }
      }
    }
    matPx(row, 13, br, br, br);
  }

  // Col 14: Heap largest block KB
  heapPx(row, 14, jF(j, "ae", 0.0f));

  // Col 15: WiFi RSSI
  rssiPx(row, 15, jI(j, "ac", -100));
}

// ============================================================
// MATRIX — PHOTON rij renderer (v4.7)
// v5.2: const char* j i.p.v. const String& j
// ============================================================
void renderPhotonRow(int row, int ci) {
  const char* j = controllers[ci].last_json;
  bool online    = (controllers[ci].status == STATUS_ONLINE);

  statusPx(row, 0, controllers[ci].status);

  if (!online) {
    for (int c = 1; c < MATRIX_WIDTH; c++) matPx(row, c, 25, 0, 0);
    return;
  }

  // Col 1: niet beschikbaar
  matPx(row, 1, 0, 0, 0);

  // Col 2: TSTATon (key "l")
  int tstat = jI(j, "l");
  matPx(row, 2, tstat ? 200 : 10, tstat ? 60 : 10, 10);

  // Col 3: Temp1 DHT22 (key "g")
  tempPx(row, 3, jF(j, "g", 0.0f));

  // Col 4: Vochtigheid (key "d")
  int humi = jI(j, "d");
  if      (humi < 10)  matPx(row, 4, 20, 0, 20);
  else if (humi < 50)  matPx(row, 4, 0,  200, 0);
  else if (humi < 70)  matPx(row, 4, 180, 180, 0);
  else if (humi < 85)  matPx(row, 4, 200, 60,  0);
  else                 matPx(row, 4, 200, 0,   0);

  // Col 5: CO2 (key "a")
  int co2 = jI(j, "a");
  if      (co2 == 0)   matPx(row, 5, 15, 15, 15);
  else if (co2 < 800)  matPx(row, 5, 0,  200, 0);
  else if (co2 < 1200) matPx(row, 5, 180, 180, 0);
  else if (co2 < 1500) matPx(row, 5, 200, 80,  0);
  else                 matPx(row, 5, 200, 0,   0);

  // Col 6: MOV1 (key "i")
  int m1 = jI(j, "i");
  matPx(row, 6, m1 ? 200 : 12, m1 ? 180 : 12, m1 ? 120 : 12);

  // Col 7: MOV2 (key "j")
  int m2 = jI(j, "j");
  matPx(row, 7, m2 ? 200 : 12, m2 ? 180 : 12, m2 ? 120 : 12);

  // Col 8: niet beschikbaar
  matPx(row, 8, 0, 0, 0);

  // Col 9: DewAlert (key "k")
  int dew = jI(j, "k");
  matPx(row, 9, dew ? 200 : 0, 0, dew ? 0 : 20);

  // Col 10: RGB pixels (keys "s","t","u")
  {
    int pr = jI(j, "s"), pg = jI(j, "t"), pb = jI(j, "u");
    if (pr + pg + pb > 0) matPx(row, 10, (uint8_t)(pr/2), (uint8_t)(pg/2), (uint8_t)(pb/2));
    else                  matPx(row, 10, 10, 10, 10);
  }

  // Col 11: Night (key "q")
  {
    int night = jI(j, "q");
    if (night) matPx(row, 11, 40, 0, 60);
    else       matPx(row, 11, 180, 150, 0);
  }

  // Col 12: LDR licht (key "e")
  {
    int ldr = jI(j, "e");
    int br = (100 - constrain(ldr, 0, 100)) * 2;
    matPx(row, 12, (uint8_t)(br), (uint8_t)(br * 0.75f), 0);
  }

  // Col 13: niet beschikbaar
  matPx(row, 13, 0, 0, 0);

  // Col 14: FreeMem% (key "x")
  {
    int fm = jI(j, "x");
    if      (fm > 35) matPx(row, 14,  0, 180,  0);
    else if (fm > 20) matPx(row, 14, 180, 180,  0);
    else              matPx(row, 14, 200,   0,  0);
  }

  // Col 15: RSSI niet beschikbaar
  matPx(row, 15, 0, 0, 0);
}

// ============================================================
// MATRIX — ECO rij renderer (v4.5)
// v5.2: const char* j i.p.v. const String& j
// ============================================================
void renderEcoRow(int row, int ci) {
  const char* j = controllers[ci].last_json;
  bool online    = (controllers[ci].status == STATUS_ONLINE);

  statusPx(row, 0, controllers[ci].status);

  if (!online) {
    for (int c = 1; c < MATRIX_WIDTH; c++) matPx(row, c, 25, 0, 0);
    return;
  }

  // Col 1: Tsun collector temp
  float tsun = jF(j, "l", 0.0f);
  if      (tsun < 1.0f)  matPx(row, 1, 15, 15, 15);
  else if (tsun < 20.0f) matPx(row, 1,  0,  0, 200);
  else if (tsun < 50.0f) matPx(row, 1,  0, 200, 100);
  else if (tsun < 80.0f) matPx(row, 1, 200, 120,  0);
  else                   matPx(row, 1, 200,   0,  0);

  // Col 2: dT rendement
  float dt = jF(j, "m", 0.0f);
  if      (dt > 10) matPx(row, 2,   0, 200,   0);
  else if (dt >  5) matPx(row, 2, 150, 180,   0);
  else if (dt >  2) matPx(row, 2, 100,  60,   0);
  else              matPx(row, 2,  10,  10,  10);

  // Col 3-8: zes boilertemperaturen
  boilerTempPx(row, 3, jF(j, "b", 0.0f));
  boilerTempPx(row, 4, jF(j, "c", 0.0f));
  boilerTempPx(row, 5, jF(j, "d", 0.0f));
  boilerTempPx(row, 6, jF(j, "e", 0.0f));
  boilerTempPx(row, 7, jF(j, "f", 0.0f));
  boilerTempPx(row, 8, jF(j, "g", 0.0f));

  // Col 9: EAv gemiddelde boilertemp
  boilerTempPx(row, 9, jF(j, "h", 0.0f));

  // Col 10: PWM pomp
  cyanLevel(row, 10, jI(j, "n") * 100 / 255);

  // Col 11: yield_today kWh
  float yt = jF(j, "k", 0.0f);
  if (yt <= 0)  matPx(row, 11, 10, 10, 10);
  else {
    uint8_t v = (uint8_t)constrain((int)(yt * 20), 20, 200);
    matPx(row, 11, v*2/3, v, 0);
  }

  // Col 12: EQtot energie-inhoud
  float qtot = jF(j, "i", 0.0f);
  if (qtot <= 0)  matPx(row, 12, 10, 10, 10);
  else {
    uint8_t v = (uint8_t)constrain((int)(qtot * 10), 20, 200);
    matPx(row, 12, v, v*3/4, 0);
  }

  // Col 13: dEQ delta kWh
  float deq = jF(j, "j", 0.0f);
  if      (deq > 0.5f) matPx(row, 13,  0, 200,  0);
  else if (deq > 0)    matPx(row, 13,  0,  80,  0);
  else                 matPx(row, 13, 10,  10, 10);

  // Col 14: FreeHeap%
  int heapp = jI(j, "q");
  if      (heapp > 35) matPx(row, 14,  0, 180,  0);
  else if (heapp > 20) matPx(row, 14, 180, 180,  0);
  else                 matPx(row, 14, 200,   0,  0);

  // Col 15: RSSI — ⚠️ key "p" voor ECO (niet "ac"!)
  rssiPx(row, 15, jI(j, "p", -100));
}

// ============================================================
// MATRIX — HVAC rij renderer (v4.1)
// v5.2: const char* j i.p.v. const String& j
// ============================================================
void renderHvacRow(int row, int ci) {
  const char* j = controllers[ci].last_json;
  bool online    = (controllers[ci].status == STATUS_ONLINE);

  statusPx(row, 0, controllers[ci].status);

  if (!online) {
    for (int c = 1; c < MATRIX_WIDTH; c++) matPx(row, c, 25, 0, 0);
    return;
  }

  // Circuits 1-7
  const char* ckeys[7] = {
    HVAC_KEY_CIRCUIT1, HVAC_KEY_CIRCUIT2, HVAC_KEY_CIRCUIT3,
    HVAC_KEY_CIRCUIT4, HVAC_KEY_CIRCUIT5, HVAC_KEY_CIRCUIT6,
    HVAC_KEY_CIRCUIT7
  };
  for (int k = 0; k < 7; k++) {
    int on = jI(j, ckeys[k]);
    matPx(row, 1+k, on ? 20 : 8, on ? 200 : 8, on ? 20 : 8);
  }

  // Col 8: Pomp SCH
  int psch = jI(j, HVAC_KEY_PUMP_SCH);
  matPx(row, 8, 0, psch ? 40 : 10, psch ? 200 : 10);

  // Col 9: Pomp WON
  int pwon = jI(j, HVAC_KEY_PUMP_WON);
  matPx(row, 9, 0, pwon ? 40 : 10, pwon ? 200 : 10);

  // Col 10: Ventilatie %
  cyanLevel(row, 10, jI(j, HVAC_KEY_VENT));

  // Col 11: KSAv gemiddelde boilertemp
  boilerTempPx(row, 11, jF(j, HVAC_KEY_TEMP1, 0.0f));

  // Col 12-13: gereserveerd
  matPx(row, 12, 8, 8, 8);
  matPx(row, 13, 8, 8, 8);

  // Col 14: Heap largest block KB
  heapPx(row, 14, jF(j, HVAC_KEY_HEAP, 0.0f));

  // Col 15: RSSI
  rssiPx(row, 15, jI(j, HVAC_KEY_RSSI, -100));
}

// ============================================================
// MATRIX — updateMatrix() (v5.0)
// v5.2: strlen() i.p.v. last_json.length()
// ============================================================
void updateMatrix() {
  matrix.clear();

  for (int row = 0; row < MATRIX_HEIGHT; row++) {
    const MatrixRowDef& rd = MROW[row];

    if (rd.sys_idx == -2) continue;
    if (rd.sys_idx == -1 && rd.esp_idx == -1 && rd.photon_idx == -1) continue;
    if (rd.sys_idx >= 0) {
      if (!controllers[rd.sys_idx].active) continue;
      if      (strcmp(controllers[rd.sys_idx].name, "S-ECO")  == 0) renderEcoRow(row,  rd.sys_idx);
      else if (strcmp(controllers[rd.sys_idx].name, "S-HVAC") == 0) renderHvacRow(row, rd.sys_idx);
      else { statusPx(row, 0, controllers[rd.sys_idx].status); }
      continue;
    }

    bool esp_ok = (rd.esp_idx >= 0)
               && controllers[rd.esp_idx].active
               && strlen(controllers[rd.esp_idx].last_json) > 5;

    if (esp_ok) {
      renderRoomRow(row, rd.esp_idx);
      continue;
    }

    bool pho_ok = (rd.photon_idx >= 0)
               && controllers[rd.photon_idx].active
               && strlen(controllers[rd.photon_idx].last_json) > 5;

    if (pho_ok) {
      renderPhotonRow(row, rd.photon_idx);
      continue;
    }

    if (rd.esp_idx >= 0 && controllers[rd.esp_idx].active) {
      statusPx(row, 0, controllers[rd.esp_idx].status);
      continue;
    }
  }

  matrix.show();
  last_matrix_update = millis();
  Serial.printf("[Matrix] Bijgewerkt | heap largest: %d bytes\n",
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

// ============================================================
// MATRIX — boot-animatie (v4.2)
// ============================================================
void matrixBootAnimation() {
  matrix.clear();
  matrix.show();
  delay(150);

  const int sys_rows[] = {0, 1};
  for (int ri = 0; ri < 2; ri++) {
    int row = sys_rows[ri];
    for (int c = 0; c < MATRIX_WIDTH; c++) {
      matrix.clear();
      matPx(row, c, 220, 0, 0);
      if (c >= 1) matPx(row, c-1, 60, 0, 0);
      if (c >= 2) matPx(row, c-2, 20, 0, 0);
      matrix.show();
      delay(28);
    }
    matrix.clear();
    matrix.show();
    delay(80);
  }

  delay(200);

  const int room_rows[] = {5, 6, 7, 8, 9, 10, 11};
  for (int ri = 0; ri < 7; ri++) {
    int row = room_rows[ri];
    for (int c = 0; c < MATRIX_WIDTH; c++) {
      matrix.clear();
      matPx(row, c, 0, 0, 220);
      if (c >= 1) matPx(row, c-1, 0, 0, 60);
      if (c >= 2) matPx(row, c-2, 0, 0, 20);
      matrix.show();
      delay(28);
    }
    matrix.clear();
    matrix.show();
    delay(60);
  }

  for (int c = 0; c < MATRIX_WIDTH; c++) {
    matPx(0, c, 30, 30, 30);
    matPx(1, c, 30, 30, 30);
    for (int r = 5; r <= 11; r++) matPx(r, c, 30, 30, 30);
  }
  matrix.show();
  delay(250);
  matrix.clear();
  matrix.show();
  Serial.println(F("[Matrix] Boot-animatie klaar"));
}

// ============================================================
// MATRIX — testpatroon (v4.0)
// ============================================================
void matrixTestPattern() {
  matrix.clear();

  const uint8_t rowcols[12][3] = {
    {200, 0,   0  }, // rij 0: HVAC → rood
    {200, 80,  0  }, // rij 1: ECO  → oranje
    {80,  0,   80 }, // rij 2: OUTSIDE → paars
    {80,  0,   80 }, // rij 3: ACCESS  → paars
    {60,  30,  0  }, // rij 4: separator → amber
    {0,   200, 0  }, // rij 5: R-BandB → groen
    {0,   180, 20 }, // rij 6: R-BADK
    {0,   160, 40 }, // rij 7: R-INKOM
    {0,   140, 60 }, // rij 8: R-KEUKEN
    {0,   120, 80 }, // rij 9: R-WASPL
    {0,   100, 100}, // rij 10: R-EETPL
    {0,   80,  120}, // rij 11: R-ZITPL
  };

  for (int row = 0; row < 12; row++) {
    for (int c = 0; c < MATRIX_WIDTH; c++) {
      uint8_t r = rowcols[row][0], g = rowcols[row][1], b = rowcols[row][2];
      if (c == 0 || c == MATRIX_WIDTH-1) matPx(row, c, 200, 200, 200);
      else                               matPx(row, c, r/3, g/3, b/3);
    }
  }
  matPx(0, 0,  255, 255, 255);
  matPx(0, 15, 255, 255, 255);
  matPx(11,0,  255, 255, 255);
  matPx(11,15, 255, 255, 255);

  matrix.show();
  Serial.println("[Matrix] Testpatroon actief — controleer orientatie!");
  Serial.println("  Rij 0 (rood)   = HVAC = BOVENSTE rij matrix");
  Serial.println("  Rij 11 (blauw) = R-ZITPL = ONDERSTE aktieve rij");
  Serial.println("  Hoekpixels wit = voor orientatiecheck");
  Serial.println("  Na verificatie: normaal updaten via updateMatrix()");
}

// ============================================================
// WIFI TESTER — HTML pagina
// ============================================================
String getWifiPage() {
  String h = F("<!DOCTYPE html><html lang='nl'><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Zarlar WiFi</title>"
    "<style>"
    "body{margin:0;background:#111;color:#eee;font-family:monospace;font-size:13px}"
    ".hdr{background:#1a1200;border-bottom:2px solid #f0a500;padding:10px 16px}"
    ".hdr-t{color:#f0a500;font-size:18px;letter-spacing:2px}"
    ".nav{background:#161b22;padding:8px 16px;display:flex;gap:6px;flex-wrap:wrap;"
         "border-bottom:1px solid #333}"
    ".nav a{color:#888;text-decoration:none;padding:4px 10px;border:1px solid #333;"
            "border-radius:3px;font-size:11px}"
    ".nav a.cur,.nav a:hover{color:#f0a500;border-color:#f0a500}"
    ".wrap{padding:14px 16px}"
    "#big{font-size:18pt;font-weight:bold;text-align:center;padding:14px 0 4px}"
    ".bw{margin:0 0 16px;background:#222;border-radius:3px;height:8px}"
    ".bi{height:8px;border-radius:3px;transition:width .5s,background .5s}"
    "table{border-collapse:collapse;width:100%}"
    "tr{border-bottom:1px solid #1a1a1a}"
    "td{padding:5px 6px;font-size:12px;vertical-align:middle}"
    ".cn{cursor:pointer;color:#aaa;white-space:nowrap;width:110px}"
    ".cn:hover{color:#f0a500}"
    ".g{color:#2ecc40}.y{color:#f0c040}.o{color:#e05a00}.r{color:#e74c3c}"
    ".dim{color:#444}"
    "</style></head><body>"
    "<div class='hdr'><span class='hdr-t'>⬡ ZARLAR</span></div>"
    "<div class='nav'>"
    "<a href='/'>← Dashboard</a>"
    "<a href='/settings'>⚙ Settings</a>"
    "<a href='/wifi' class='cur'>📶 WiFi</a></div>"
    "<div class='wrap'>"
    "<div id='big'>—</div>"
    "<div class='bw'><div class='bi' id='bar' style='width:0'></div></div>"
    "<table id='tbl'></table>"
    "</div>"
    "<script>"
    "var C=");
  h += "[";
  bool first = true;
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    if (controllers[i].type == TYPE_PHOTON) continue;
    if (!first) h += ",";
    h += "{i:"; h += i; h += ",n:\""; h += controllers[i].name; h += "\"}";
    first = false;
  }
  h += "];";
  h += F(
    "var S={};"
    "function rc(r){return r>=-60?'g':r>=-70?'y':r>=-80?'o':'r';}"
    "function pc(p){return p<0?'r':p<8?'g':p<20?'y':p<40?'o':'r';}"
    "var QC=['#2ecc40','#f0c040','#e05a00','#e74c3c'];"
    "function build(){var h='';C.forEach(function(c){"
    "  var d=S[c.i];"
    "  var sd='<span class=\"dim\">—</span>';"
    "  if(d===null)sd='<span class=\"dim\">…</span>';"
    "  else if(d){sd='<span class=\"'+rc(d.rssi)+'\">'+d.rssi+'</span>';"
    "    var ps=d.ping>=0?'<span class=\"'+pc(d.ping)+'\">'+d.ping+'</span>':'<span class=\"r\">—</span>';"
    "    sd+=' '+ps;"
    "    d.nets.forEach(function(n){sd+=' <span style=\"color:#ccc\">'+n.s+'</span><span class=\"'+rc(n.r)+'\"> '+n.r+'</span>';});}"
    "  h+='<tr><td class=\"cn\" onclick=\"tog('+c.i+')\">'+c.n+'</td><td>'+sd+'</td></tr>';"
    "});document.getElementById('tbl').innerHTML=h;}"
    "function tog(idx){if(S[idx]!==undefined){delete S[idx];build();return;}"
    "  S[idx]=null;build();"
    "  fetch('/wifi_snap?start=1').then(r=>r.json()).then(d=>{"
    "    if(S[idx]===undefined)return;"
    "    if(d.status==='scanning')poll(idx);else done(idx,d);});}"
    "function poll(idx){setTimeout(function(){"
    "    if(S[idx]===undefined)return;"
    "    fetch('/wifi_snap').then(r=>r.json()).then(d=>{"
    "      if(S[idx]===undefined)return;"
    "      if(d.status==='scanning')poll(idx);else done(idx,d);});},600);}"
    "function done(idx,d){if(S[idx]===undefined)return;"
    "  if(d.status==='ok')S[idx]={rssi:d.rssi,ping:d.ping,nets:d.nets||[]};"
    "  else delete S[idx];build();}"
    "function updR(){fetch('/wifi_json').then(r=>r.json()).then(d=>{"
    "    var ci=d.q>=70?0:d.q>=40?1:d.q>=20?2:3;var c=QC[ci];"
    "    var el=document.getElementById('big');el.textContent=d.r+' dBm';el.style.color=c;"
    "    var b=document.getElementById('bar');b.style.width=d.q+'%';b.style.background=c;"
    "  }).catch(function(){})}"
    "build();setInterval(updR,1000);updR();"
    "</script></body></html>");
  return h;
}

// ============================================================
// STATUS JSON
// v5.2: strstr/atoi i.p.v. String.indexOf/substring
// ============================================================
String getStatusJson() {
  String j = "[";
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    if (i > 0) j += ",";
    int rssi = 0;
    if (controllers[i].type != TYPE_PHOTON && controllers[i].last_json[0] != '\0') {
      const char* key = (strcmp(controllers[i].name, "S-ECO") == 0) ? "\"p\":" : "\"ac\":";
      const char* pos = strstr(controllers[i].last_json, key);
      if (pos) rssi = atoi(pos + strlen(key));
    }
    j += "{\"n\":\"" + String(controllers[i].name) + "\","
         "\"t\":" + String(controllers[i].type) + ","
         "\"ip\":\"" + String(controllers[i].ip) + "\","
         "\"pid\":\"" + String(controllers[i].photon_id) + "\","
         "\"a\":" + (controllers[i].active ? "1" : "0") + ","
         "\"s\":" + String(controllers[i].status) + ","
         "\"r\":" + String(rssi) + "}";
  }
  j += "]";
  return j;
}

// ============================================================
// HOOFDPAGINA
// ============================================================
String getMainPage() {
  String h = F("<!DOCTYPE html><html lang='nl'><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Zarlar</title>"
    "<style>"
    "body{margin:0;background:#111;color:#eee;font-family:monospace;font-size:14px;-webkit-text-size-adjust:none}"
    ".hdr{background:#1a1200;border-bottom:2px solid #f0a500;padding:10px 16px;"
         "display:flex;align-items:center;justify-content:space-between}"
    ".hdr-t{color:#f0a500;font-size:18px;letter-spacing:2px}"
    ".hdr-r{color:#888;font-size:11px}"
    ".nav{background:#161b22;padding:8px 16px;display:flex;gap:6px;flex-wrap:wrap;"
         "border-bottom:1px solid #333}"
    ".nav a{color:#888;text-decoration:none;padding:4px 10px;border:1px solid #333;"
            "border-radius:3px;font-size:11px}"
    ".nav a:hover{color:#f0a500;border-color:#f0a500}"
    ".wrap{padding:14px 16px;max-width:1200px}"
    ".sec{background:#161b22;border:1px solid #333;border-radius:6px;margin-bottom:14px}"
    ".sec-t{padding:8px 14px;font-size:11px;color:#f0a500;letter-spacing:2px;"
            "border-bottom:1px solid #333;text-transform:uppercase}"
    ".grid{display:flex;flex-wrap:wrap;gap:7px;padding:12px 14px}"
    ".btn{display:flex;align-items:center;gap:7px;padding:7px 12px;"
         "background:#1c2128;border:1px solid #333;border-radius:5px;"
         "cursor:pointer;font-family:monospace;font-size:12px;color:#ccc;"
         "min-width:150px;transition:border-color .15s}"
    ".btn:hover{border-color:#f0a500;color:#f0a500}"
    ".btn.off{opacity:.4;cursor:default}"
    ".btn.off:hover{border-color:#333;color:#ccc}"
    ".dot{width:8px;height:8px;border-radius:50%;flex-shrink:0}"
    "#portal{padding:0}"
    "#portal iframe{width:100%;height:calc(100vh - 84px);border:none;display:block;background:#fff}"
    "#portal .empty{padding:40px;text-align:center;color:#555}"
    "@media(max-width:600px){.btn{min-width:100px}}"
    ".hbtn{padding:4px 10px;border-radius:3px;font-size:11px;cursor:pointer;font-family:monospace}"
    ".hbtn.on{background:#1a3a1a;color:#2ecc40;border:1px solid #2ecc40}"
    ".hbtn.off{background:#1c2128;color:#666;border:1px solid #444}"
    "</style></head><body>"
    "<div class='hdr'><span class='hdr-t'>⬡ ZARLAR</span>"
    "<span class='hdr-r' id='clk'>192.168.0.60</span></div>"
    "<div class='nav'>"
    "<a href='/'>Dashboard</a>"
    "<a href='/settings'>⚙ Settings</a>"
    "<a href='/wifi'>📶 WiFi</a>"
    "<button id='hb' class='hbtn ");
  h += home_mode_global ? "on' onclick='setH(0)'>● HOME" : "off' onclick='setH(1)'>○ UIT";
  h += F("</button></div>"
    "<div class='wrap'>"
    "<div class='sec'><div class='sec-t'>▸ Systeem</div>"
    "<div class='grid' id='gs'></div></div>"
    "<div class='sec'><div class='sec-t'>▸ Rooms</div>"
    "<div class='grid' id='gr'></div></div>"
    "<div class='sec'><div class='sec-t'>☁ Photon</div>"
    "<div class='grid' id='gp'></div></div>"
    "<div class='sec'><div class='sec-t' id='pt'>▸ Portal</div>"
    "<div id='portal'><div class='empty'>Kies een controller</div></div></div>"
    "</div>"
    "<script>"
    "var C=[];"
    "var PL={'datum':'Time','a':'CO2','b':'Dust','c':'Dew','d':'Humi','e':'Light',"
    "'f':'SUNLight','g':'Temp1','h':'Temp2','i':'MOV1','j':'MOV2',"
    "'k':'DewAlert','l':'TSTATon','m':'MOV1light','n':'MOV2light',"
    "'o':'BEAMvalue','p':'BEAMalert','q':'Night','r':'Bed','s':'R',"
    "'t':'G','u':'B','v':'Strength','w':'Quality','x':'FreeMem'};"
    "var SC=['#555','#f0c040','#2ecc40','#e74c3c'];"
    "function rc(r){return r>=-60?'#2ecc40':r>=-70?'#f0c040':r>=-80?'#e05a00':'#e74c3c';}"
    "function dot(s){return '<span class=\"dot\" style=\"background:'+SC[s]+';box-shadow:0 0 5px '+SC[s]+';\"></span>';}"
    "function build(){"
    "  var gs=document.getElementById('gs');"
    "  var gr=document.getElementById('gr');"
    "  var gp=document.getElementById('gp');"
    "  gs.innerHTML=gr.innerHTML=gp.innerHTML='';"
    "  C.forEach(function(c,i){"
    "    var b=document.createElement('button');"
    "    b.className='btn'+(c.a?'':' off');"
    "    b.id='b'+i;"
    "    var rs=c.r!==0?'<span class=\"rs\" style=\"color:'+rc(c.r)+';font-size:11px\">'+Math.abs(c.r)+'</span>':'';"
    "    b.innerHTML=dot(c.s)+c.n+rs;"
    "    b.title=c.ip||c.pid||'';"
    "    if(c.a)b.onclick=function(){open(i);};"
    "    if(c.t===0)gp.appendChild(b);"
    "    else if(c.t===1)gs.appendChild(b);"
    "    else gr.appendChild(b);"
    "  });"
    "}"
    "function updDots(){"
    "  C.forEach(function(c,i){"
    "    var b=document.getElementById('b'+i);"
    "    if(!b)return;"
    "    var d=b.querySelector('.dot');"
    "    if(d)d.style.background=SC[c.s];"
    "    var rs=b.querySelector('.rs');"
    "    if(rs&&c.r!==0){rs.textContent=Math.abs(c.r);rs.style.color=rc(c.r);}"
    "  });"
    "}"
    "function open(i){"
    "  var c=C[i];"
    "  var pu=c.t===0?'':`http://${c.ip}/`;"
    "  document.getElementById('pt').innerHTML=pu?"
    "    `<a href='${pu}' target='_blank' style='color:#f0a500;text-decoration:none'>▸ ${c.n} ↗</a>`:"
    "    '▸ '+c.n;"
    "  var p=document.getElementById('portal');"
    "  if(c.t===0){"
    "    p.innerHTML='<div class=\"empty\" style=\"color:#f0c040\">⏳ Laden '+c.n+'...</div>';"
    "    fetchPhoton(i,c.pid,c.n);"
    "  } else {"
    "    p.innerHTML='<iframe src=\"http://'+c.ip+'/\" title=\"'+c.n+'\"></iframe>';"
    "  }"
    "  document.querySelector('.sec:last-child').scrollIntoView({behavior:'smooth'});"
    "}"
    "async function fetchPhoton(i,id,name){"
    "  var p=document.getElementById('portal');"
    "  p.innerHTML='<div class=\"empty\" style=\"color:#f0c040\">&#x23F3; Laden '+name+'...</div>';"
    "  try{"
    "    var r=await fetch('https://controllers-diagnose.filip-delannoy.workers.dev/sensor?id='+id,{cache:'no-store'});"
    "    if(!r.ok)throw new Error('HTTP '+r.status);"
    "    var obj=await r.json();"
    "    if(!obj.online)throw new Error('Photon offline');"
    "    C[i].s=2;updDots();"
    "    var h='<div style=\"padding:14px;color:#2ecc40\">&#x2713; '+name+' online ('+obj.last_min+'m geleden)</div>';"
    "    h+='<div style=\"padding:0 14px 14px\">';"
    "    for(var k in obj){if(k==='online'||k==='last_min')continue;"
    "      var v=obj[k];var l=PL[k]||k;"
    "      h+='<div style=\"display:flex;justify-content:space-between;padding:5px 0;"
    "border-bottom:1px solid #333\"><span style=\"color:#f0a500\">'+l+'</span>"
    "<span>'+v+'</span></div>';}"
    "    h+='</div>';"
    "    p.innerHTML=h;"
    "  }catch(e){C[i].s=3;updDots();"
    "    p.innerHTML='<div class=\"empty\" style=\"color:#e74c3c\">&#x274C; '+e.message+'</div>';}"
    "}"
    "function pollStatus(){"
    "  fetch('/status_json').then(function(r){return r.json();})"
    "  .then(function(d){C=d;updDots();}).catch(function(){});"
    "}"
    "fetch('/status_json').then(function(r){return r.json();})"
    ".then(function(d){C=d;build();});"
    "setInterval(pollStatus,15000);"
    "setInterval(function(){document.getElementById('clk').textContent="
    "'192.168.0.60 | '+new Date().toLocaleTimeString('nl-BE');},1000);"
    "function setH(v){fetch('/set_home_global?v='+v).then(function(){var b=document.getElementById('hb');b.className='hbtn '+(v?'on':'off');b.textContent=v?'● HOME':'○ UIT';b.onclick=function(){setH(v?0:1);};});}"
    "</script></body></html>");
  return h;
}

// ============================================================
// SETTINGS PAGINA (v5.2: reboot-knop toegevoegd)
// ============================================================
String getSettingsPage() {
  String h = "<!DOCTYPE html><html lang='nl'><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Zarlar Settings</title>"
    "<style>"
    "body{margin:0;background:#111;color:#eee;font-family:monospace;font-size:13px}"
    ".hdr{background:#1a1200;border-bottom:2px solid #f0a500;padding:10px 16px}"
    ".hdr-t{color:#f0a500;font-size:18px;letter-spacing:2px}"
    ".nav{background:#161b22;padding:8px 16px;display:flex;gap:6px;border-bottom:1px solid #333}"
    ".nav a{color:#888;text-decoration:none;padding:4px 10px;border:1px solid #333;border-radius:3px;font-size:11px}"
    ".nav a:hover{color:#f0a500;border-color:#f0a500}"
    ".wrap{padding:14px 16px;max-width:900px}"
    ".sec{background:#161b22;border:1px solid #333;border-radius:6px;margin-bottom:14px;overflow:hidden}"
    ".sec-t{padding:8px 14px;font-size:11px;color:#f0a500;letter-spacing:2px;border-bottom:1px solid #333;text-transform:uppercase}"
    "table{width:100%;border-collapse:collapse}"
    "td{padding:8px 14px;border-bottom:1px solid #222;vertical-align:middle}"
    "td:first-child{color:#888;width:200px}"
    "input[type=text],input[type=password],input[type=number]"
    "{background:#0d1117;border:1px solid #333;color:#eee;padding:5px 8px;"
    "border-radius:3px;font-size:12px;width:100%;font-family:monospace}"
    "input:focus{outline:none;border-color:#f0a500}"
    "input[type=checkbox]{accent-color:#f0a500;width:16px;height:16px}"
    "input[type=range]{accent-color:#f0a500;width:160px;vertical-align:middle}"
    ".badge{font-size:10px;padding:1px 5px;border-radius:2px;margin-left:4px}"
    ".bs{background:#e05a0022;color:#e05a00;border:1px solid #e05a0044}"
    ".br{background:#2ecc4022;color:#2ecc40;border:1px solid #2ecc4044}"
    ".bp{background:#0077ff22;color:#4af;border:1px solid #0077ff44}"
    ".save{background:#f0a500;color:#000;border:none;padding:10px 28px;"
          "border-radius:5px;font-size:13px;font-weight:bold;cursor:pointer;margin:14px 14px}"
    ".save:hover{background:#ffd000}"
    ".rbtn{background:#1c2128;color:#e05a00;border:1px solid #e05a00;padding:10px 28px;"
          "border-radius:5px;font-size:13px;font-weight:bold;cursor:pointer;margin:14px 0 14px 0}"
    ".rbtn:hover{background:#e05a00;color:#000}"
    ".mbtn{background:#1c2128;color:#f0a500;border:1px solid #f0a500;padding:6px 14px;"
           "border-radius:4px;font-size:12px;cursor:pointer;font-family:monospace}"
    ".mbtn:hover{background:#f0a500;color:#000}"
    "</style></head><body>"
    "<div class='hdr'><span class='hdr-t'>⬡ ZARLAR SETTINGS</span></div>"
    "<div class='nav'><a href='/'>← Dashboard</a>"
    "<a href='/settings'>⚙ Settings</a>"
    "<a href='/wifi'>📶 WiFi</a></div>"
    "<div class='wrap'><form action='/save_settings' method='get'>"
    "<div class='sec'><div class='sec-t'>▸ Netwerk</div><table>"
    "<tr><td>WiFi SSID</td><td><input type='text' name='wifi_ssid' value='";
  h += wifi_ssid;
  h += "'></td></tr><tr><td>WiFi Wachtwoord</td>"
       "<td><input type='password' name='wifi_pass' value='";
  h += wifi_pass;
  h += "'></td></tr></table></div>"
       "<div class='sec'><div class='sec-t'>▸ Logging</div><table>"
       "<tr><td>Poll interval (min)</td><td><input type='number' name='poll_min' min='1' max='60' value='";
  h += String(poll_minutes);
  h += "'></td></tr><tr><td>Room Script URL</td>"
       "<td><input type='text' name='room_script' value='";
  h += room_script_url;
  h += "' placeholder='https://script.google.com/...'></td></tr></table></div>"
       "<div class='sec'><div class='sec-t'>▸ Controllers</div><table>"
       "<tr style='background:#1a1a1a'>"
       "<td style='color:#f0a500;font-size:11px'>Controller</td>"
       "<td style='color:#f0a500;font-size:11px'>IP / ID</td>"
       "<td style='color:#f0a500;font-size:11px;text-align:center;width:60px'>Actief</td>"
       "<td style='color:#f0a500;font-size:11px'>Sheets URL</td></tr>";
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    String tb = (controllers[i].type == TYPE_PHOTON) ? "bp" :
                (controllers[i].type == TYPE_SYSTEM) ? "bs" : "br";
    String tn = (controllers[i].type == TYPE_PHOTON) ? "P" :
                (controllers[i].type == TYPE_SYSTEM) ? "S" : "R";
    String ipid = strlen(controllers[i].ip) > 0 ?
                  String(controllers[i].ip) : String(controllers[i].photon_id).substring(0,12) + "...";
    h += "<tr><td>" + String(controllers[i].name) +
         "<span class='badge " + tb + "'>" + tn + "</span></td>";
    h += "<td style='color:#555;font-size:11px'>" + ipid + "</td>";
    h += "<td style='text-align:center'><input type='checkbox' name='act_" + String(i) +
         "' value='1'" + (controllers[i].active ? " checked" : "") + "></td>";
    if (controllers[i].type == TYPE_ROOM || controllers[i].type == TYPE_PHOTON) {
      h += "<td style='color:#444;font-size:11px'>— gedeeld script —</td>";
    } else {
      h += "<td><input type='text' name='url_" + String(i) +
           "' value='" + controllers[i].sheets_url +
           "' placeholder='https://script.google.com/...'></td>";
    }
    h += "</tr>";
  }
  h += "</table></div>"
       "<button type='submit' class='save'>💾 Opslaan</button>"
       "</form>";

  // ── Statusmatrix ─────────────────────────────────────────────────────────
  h += "<div class='sec'><div class='sec-t'>▸ Statusmatrix 16×16</div><table>"
       "<tr><td>Helderheid</td><td>"
       "<input type='range' id='mbr' min='5' max='200' step='5' value='";
  h += String(matrix_brightness);
  h += "' oninput=\"document.getElementById('mbv').textContent=this.value;"
       "fetch('/matrix_bright?v='+this.value);\">"
       " <span id='mbv'>";
  h += String(matrix_brightness);
  h += "</span><span style='color:#555'>/200</span></td></tr>"
       "<tr><td>Testpatroon</td><td>"
       "<button class='mbtn' onclick=\"fetch('/matrix_test').then(function(){"
       "document.getElementById('mts').textContent='✓ Testpatroon actief';})\">"
       "🔆 Activeer test</button>"
       " <button class='mbtn' onclick=\"fetch('/matrix_update').then(function(){"
       "document.getElementById('mts').textContent='✓ Matrix bijgewerkt';})\">"
       "↺ Live data</button>"
       " <span id='mts' style='color:#f0a500;font-size:11px'></span></td></tr>"
       "<tr><td>Oriëntatie</td><td style='color:#555;font-size:11px'>"
       "MATRIX_FLIP_H = ";
  h += MATRIX_FLIP_H ? "true" : "false";
  h += " — pas define aan in sketch als pixels gespiegeld zijn</td></tr>"
       "</table></div>";

  // ── Matter ───────────────────────────────────────────────────────────────
  h += "<div class='sec'><div class='sec-t'>▸ Matter</div><table>"
       "<tr><td>Status</td><td id='ms'>—</td></tr>"
       "<tr><td>Pairing code</td><td id='mp' style='color:#f0a500;letter-spacing:2px'>—</td></tr>"
       "</table>"
       "<div style='padding:10px 14px'>"
       "<button class='save' style='margin:0;background:#e05a00' onclick='if(confirm(\"Matter reset uitvoeren?\"))fetch(\"/matter_reset\").then(()=>setTimeout(()=>location.reload(),4000))'>↺ Reset Matter</button>"
       "</div></div>"
       "<script>"
       "fetch('/matter_json').then(r=>r.json()).then(d=>{"
       "  document.getElementById('ms').textContent=d.commissioned?'✓ Gepaard':'Niet gepaard';"
       "  document.getElementById('ms').style.color=d.commissioned?'#2ecc40':'#f0c040';"
       "  var pc=document.getElementById('mp');"
       "  if(!d.commissioned&&d.code){pc.textContent=d.code;}else{pc.textContent='—';pc.style.color='#555';}"
       "});"
       "</script>";

  // ── Systeem info ─────────────────────────────────────────────────────────
  h += "<div class='sec'><div class='sec-t'>▸ Systeem info</div><table>"
       "<tr><td>IP adres</td><td>";
  h += ap_mode ? "AP " + WiFi.softAPIP().toString() : WiFi.localIP().toString();
  h += "</td></tr><tr><td>Mode</td><td>";
  h += ap_mode ? "AP (Zarlar-Setup)" : "STA";
  h += "</td></tr><tr><td>WiFi RSSI</td><td>";
  h += String(WiFi.RSSI()) + " dBm";
  h += "</td></tr><tr><td>Uptime</td><td>";
  h += String(millis() / 1000) + " s";
  h += "</td></tr><tr><td>Free heap</td><td>";
  h += String(ESP.getFreeHeap()) + " bytes";
  h += "</td></tr><tr><td>Largest block</td><td>";
  h += String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) + " bytes";
  h += "</td></tr><tr><td>Poll interval</td><td>";
  h += String(poll_minutes) + " min";
  h += "</td></tr></table>"
       // ── Reboot knop (v5.2) ────────────────────────────────────────────
       "<div style='padding:10px 14px'>"
       "<button class='rbtn' onclick='if(confirm(\"Dashboard herstarten?\")){"
       "this.textContent=\"⟳ Herstart...\";this.disabled=true;"
       "fetch(\"/reboot\");}'>⟳ Herstart dashboard</button>"
       "</div></div>"
       "</div></body></html>";
  return h;
}

// ============================================================
// WEB SERVER
// ============================================================
void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html; charset=utf-8", getMainPage());
  });
  server.on("/status_json", HTTP_GET, []() {
    server.send(200, "application/json", getStatusJson());
  });
  server.on("/settings", HTTP_GET, []() {
    server.send(200, "text/html; charset=utf-8", getSettingsPage());
  });
  server.on("/save_settings", HTTP_GET, []() {
    if (server.hasArg("wifi_ssid"))    wifi_ssid       = server.arg("wifi_ssid");
    if (server.hasArg("wifi_pass"))    wifi_pass       = server.arg("wifi_pass");
    if (server.hasArg("poll_min"))     poll_minutes    = server.arg("poll_min").toInt();
    if (server.hasArg("room_script"))  room_script_url = server.arg("room_script");
    for (int i = 0; i < NUM_CONTROLLERS; i++) {
      controllers[i].active = server.hasArg("act_" + String(i));
      controllers[i].status = controllers[i].active ? STATUS_PENDING : STATUS_INACTIVE;
      String uk = "url_" + String(i);
      if (server.hasArg(uk)) controllers[i].sheets_url = server.arg(uk);
    }
    saveNVS();
    server.send(200, "text/html",
      "<html><body style='background:#111;color:#f0a500;font-family:monospace;"
      "padding:40px;text-align:center'>"
      "<h2>✓ Opgeslagen!</h2><p>Herstart...</p>"
      "<script>setTimeout(()=>location.href='/',2500);</script>"
      "</body></html>");
    delay(2000);
    ESP.restart();
  });

  // v5.2: reboot endpoint
  server.on("/reboot", HTTP_GET, []() {
    server.send(200, "text/html",
      "<html><body style='background:#111;color:#f0a500;font-family:monospace;"
      "padding:40px;text-align:center'>"
      "<h2>⟳ Herstart...</h2><p>Dashboard herstart. Even wachten...</p>"
      "<script>setTimeout(()=>location.href='/settings',9000);</script>"
      "</body></html>");
    delay(500);
    ESP.restart();
  });

  server.on("/set_home_global", HTTP_GET, []() {
    int v = server.hasArg("v") ? constrain(server.arg("v").toInt(), 0, 1) : 0;
    home_mode_global = (v == 1);
    preferences.begin(NVS_NS, false);
    preferences.putBool(NVS_HOME_GLOBAL, home_mode_global);
    preferences.end();
    matter_home.setOnOff(home_mode_global);
    server.send(200, "text/plain", "OK");
    setAllRoomsHomeMode(v);
    Serial.printf("[HOME] → %s\n", v ? "THUIS" : "WEG");
  });

  server.on("/matter_json", HTTP_GET, []() {
    bool commissioned = Matter.isDeviceCommissioned();
    String code = commissioned ? "" : Matter.getManualPairingCode();
    char buf[80];
    snprintf(buf, sizeof(buf), "{\"commissioned\":%s,\"code\":\"%s\"}",
             commissioned ? "true" : "false", code.c_str());
    server.send(200, "application/json", buf);
  });

  server.on("/matter_reset", HTTP_GET, []() {
    server.send(200, "text/plain", "Matter reset gestart...");
    matter_nuclear_reset_requested = true;
  });

  server.on("/matrix_bright", HTTP_GET, []() {
    if (server.hasArg("v")) {
      int v = constrain(server.arg("v").toInt(), 5, 200);
      matrix_brightness = (uint8_t)v;
      matrix.setBrightness(matrix_brightness);
      matrix.show();
      preferences.begin(NVS_NS, false);
      preferences.putUChar(NVS_MATRIX_BRIGHT, matrix_brightness);
      preferences.end();
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "{\"bright\":%d}", matrix_brightness);
    server.send(200, "application/json", buf);
  });

  server.on("/matrix_test", HTTP_GET, []() {
    matrixTestPattern();
    server.send(200, "text/plain", "Testpatroon actief");
  });

  server.on("/matrix_update", HTTP_GET, []() {
    updateMatrix();
    server.send(200, "text/plain", "Matrix bijgewerkt");
  });

  server.on("/wifi", HTTP_GET,      []() { server.send(200, "text/html; charset=utf-8", getWifiPage()); });
  server.on("/wifi_json", HTTP_GET, []() { handleWifiJson(); });
  server.on("/wifi_snap", HTTP_GET, []() { handleWifiSnap(); });

  auto cpRedirect = []() {
    if (ap_mode) {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/settings");
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "404");
    }
  };
  server.on("/hotspot-detect.html",       HTTP_GET, cpRedirect);
  server.on("/library/test/success.html", HTTP_GET, cpRedirect);
  server.on("/generate_204",              HTTP_GET, cpRedirect);
  server.on("/connecttest.txt",           HTTP_GET, cpRedirect);
  server.onNotFound(cpRedirect);

  server.begin();
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n\n╔══════════════════════════════════════╗");
  Serial.println("║  Zarlar Dashboard v5.2               ║");
  Serial.println("║  192.168.0.60 — Statusmatrix 16×16  ║");
  Serial.println("╚══════════════════════════════════════╝\n");

  loadNVS();

  matrix.begin();
  matrix.setBrightness(matrix_brightness);
  matrix.clear();
  matrix.show();
  Serial.printf("[Matrix] Init: %d LEDs, helderheid %d/255\n",
                MATRIX_LEDS, matrix_brightness);
  Serial.printf("[Matrix] Heap na init: %d bytes (largest: %d)\n",
                ESP.getFreeHeap(),
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  connectWiFi();
  matrixBootAnimation();

  if (!ap_mode) {
    Serial.println(F("\n── Matter initialisatie ──────────────────"));
    matter_home.begin();
    matter_home.setOnOff(home_mode_global);
    matter_home.onChangeOnOff([](bool on_off) -> bool {
      home_mode_global = on_off;
      preferences.begin(NVS_NS, false);
      preferences.putBool(NVS_HOME_GLOBAL, home_mode_global);
      preferences.end();
      setAllRoomsHomeMode(on_off ? 1 : 0);
      Serial.printf("[Matter] HOME → %s\n", on_off ? "THUIS" : "WEG");
      return true;
    });
    uint32_t hp = ESP.getFreeHeap();
    Matter.begin();
    delay(200);
    Serial.printf("[HEAP] Matter kost: -%d bytes  (nu: %u  largest: %u)\n",
                  (int)(hp - ESP.getFreeHeap()), ESP.getFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    if (!Matter.isDeviceCommissioned() && Matter.getManualPairingCode().length() < 5) {
      Serial.println(F("Matter NVS corrupt → nuclear reset"));
      matterNuclearReset();
    }
    if (!Matter.isDeviceCommissioned()) {
      Serial.println(F("Matter: nog niet gepaard."));
      Serial.println("Code: " + Matter.getManualPairingCode());
      Serial.println(F("→ http://192.168.0.60/settings"));
    } else {
      Serial.println(F("Matter: al gepaard."));
    }
    Serial.println(F("──────────────────────────────────────────\n"));
  }

  last_poll_cycle = millis() - (unsigned long)(poll_minutes * 60000UL) + 30000UL;
  setupWebServer();
  Serial.println("READY — http://192.168.0.60/\n");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  if (ap_mode) dnsServer.processNextRequest();
  server.handleClient();
  handlePolling();

  // ── Matter nuclear reset ──────────────────────────────────────────────────
  if (matter_nuclear_reset_requested) {
    matter_nuclear_reset_requested = false;
    delay(200);
    matterNuclearReset();
  }

  // ── Matter update elke 5s ─────────────────────────────────────────────────
  if (!ap_mode && millis() - last_matter_update > 5000) {
    last_matter_update = millis();
    matter_home.setOnOff(home_mode_global);
  }

  // ── Matrix periodieke refresh elke 60s ───────────────────────────────────
  if (millis() - last_matrix_update > 60000UL) {
    updateMatrix();
  }

  // ── Automatische herstart bij persistente heap-uitputting (v5.2) ─────────
  // Als largest_block > 60s aaneensluitend onder 25KB daalt → herstart.
  // Vangt de root cause op als failsafe, mocht fragmentatie toch optreden.
  {
    static unsigned long low_heap_since = 0;
    uint32_t lb = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (lb < 25000) {
      if (low_heap_since == 0) low_heap_since = millis();
      else if (millis() - low_heap_since > 60000UL) {
        Serial.printf("[HEAP] Largest block %u bytes < 25KB voor >60s — herstart\n", lb);
        delay(200);
        ESP.restart();
      }
    } else {
      low_heap_since = 0;
    }
  }

  // ── Serial commando's ─────────────────────────────────────────────────────
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("reset-matter")) {
      Serial.println(F("Matter nuclear reset..."));
      matterNuclearReset();
    }
    if (cmd.equalsIgnoreCase("reset-all")) {
      Serial.println(F("Alles wissen + reboot..."));
      preferences.begin(NVS_NS, false);
      preferences.clear();
      preferences.end();
      nvs_flash_erase();
      delay(300);
      ESP.restart();
    }
    if (cmd.equalsIgnoreCase("status")) {
      Serial.printf("\n=== Zarlar Dashboard v5.2 | Uptime: %lu s ===\n", millis()/1000);
      Serial.printf("IP: %s  RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      Serial.printf("Heap free: %u  Largest: %u\n",
                    ESP.getFreeHeap(),
                    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
      Serial.printf("Matter: %s\n", Matter.isDeviceCommissioned() ? "gepaard" : "niet gepaard");
      Serial.printf("HOME: %s  Matrix brightness: %d\n",
                    home_mode_global ? "THUIS" : "WEG", matrix_brightness);
      Serial.println(F("\n--- Actieve controllers (idx | naam | status | json?) ---"));
      for (int i = 0; i < 22; i++) {
        if (controllers[i].active) {
          Serial.printf("  idx %2d | %-16s | %s | json=%s\n",
            i,
            controllers[i].name,
            controllers[i].status == STATUS_ONLINE  ? "ONLINE " :
            controllers[i].status == STATUS_OFFLINE ? "OFFLINE" :
            controllers[i].status == STATUS_PENDING ? "PENDING" : "INACTF ",
            strlen(controllers[i].last_json) > 5 ? "ja" : "nee");
        }
      }
      Serial.println(F("--- MROW matrix-rij mapping (ESP32 heeft voorrang op Photon) ---"));
      for (int r = 0; r < MATRIX_HEIGHT; r++) {
        const MatrixRowDef& rd = MROW[r];
        if (rd.sys_idx == -2) { Serial.printf("  rij %2d → separator\n", r); continue; }
        if (rd.sys_idx == -1 && rd.esp_idx == -1 && rd.photon_idx == -1) continue;

        if (rd.sys_idx >= 0) {
          Serial.printf("  rij %2d → SYS  idx %2d (%s) actief=%s\n",
            r, rd.sys_idx, controllers[rd.sys_idx].name,
            controllers[rd.sys_idx].active ? "JA" : "NEE");
          continue;
        }

        bool esp_ok = (rd.esp_idx >= 0) && controllers[rd.esp_idx].active
                   && strlen(controllers[rd.esp_idx].last_json) > 5;
        bool pho_ok = (rd.photon_idx >= 0) && controllers[rd.photon_idx].active
                   && strlen(controllers[rd.photon_idx].last_json) > 5;

        const char* esp_name = (rd.esp_idx >= 0) ? controllers[rd.esp_idx].name : "—";
        const char* pho_name = (rd.photon_idx >= 0) ? controllers[rd.photon_idx].name : "—";

        if (esp_ok) {
          Serial.printf("  rij %2d → ESP32 idx %2d (%s) ✓ actief\n",
            r, rd.esp_idx, esp_name);
        } else if (pho_ok) {
          Serial.printf("  rij %2d → Photon idx %2d (%s) ✓ fallback [ESP32 %s inactief]\n",
            r, rd.photon_idx, pho_name, esp_name);
        } else {
          Serial.printf("  rij %2d → leeg  [ESP32 %s / Photon %s]\n",
            r, esp_name, pho_name);
        }
      }
    }
    if (cmd.equalsIgnoreCase("matrix-test")) {
      matrixTestPattern();
    }
    if (cmd.equalsIgnoreCase("matrix-update")) {
      updateMatrix();
      Serial.println(F("[Matrix] Manueel bijgewerkt"));
    }
  }

  delay(2);
}
