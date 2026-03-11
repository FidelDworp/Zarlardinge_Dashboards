/* ============================================================
   Zarlar Dashboard v2.3
   ESP32-C6 (30-pin) @ 192.168.0.60 — http://zarlar.local
   Filip Delannoy

   BOARD INSTELLINGEN (Arduino IDE):
     Board        : ESP32-C6 Dev Module (espressif:arduino-esp32-master)
     Upload Speed : 921600
     Flash Size   : 4MB
     Partition    : Huge APP (3MB No OTA)

   EERSTE GEBRUIK / FACTORY RESET:
     1. Upload sketch → verbindt automatisch met WiFi
     2. Bij geen WiFi → AP "Zarlar-Setup" verschijnt
     3. Verbind met "Zarlar-Setup" → browser opent /settings automatisch
     4. Vul WiFi SSID + wachtwoord in, sla op → herstart
     5. Daarna: open http://192.168.0.60/settings voor controllers

   NETWERK:
     Fixed IP  : 192.168.0.60
     DNS       : 8.8.8.8 (Google — vereist voor HTTPS naar Google Sheets)
     mDNS      : http://zarlar.local
     AP SSID   : Zarlar-Setup (bij geen WiFi)

   11mar26 15:00  v2.3  WiFi sleep uitgeschakeld (betere bereikbaarheid Safari/iPhone).
                        AP + captive portal toegevoegd voor setup zonder flash.
                        HTML vereenvoudigd voor minder heap gebruik.
                        DNS fix: 8.8.8.8 (router gaf 0.0.0.0 voor Google).
   11mar26 11:00  v2.2  DNS fix ingevoerd.
   11mar26 09:00  v2.0  Volledig herschreven (16 controllers, status dots,
                        Google Sheets logging, iFrame portal, NVS settings).
   ============================================================ */

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

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
// ============================================================
struct Controller {
  const char* name;
  const char* ip;
  const char* photon_id;
  int         type;
  bool        active;
  int         status;
  String      sheets_url;
  String      last_json;
  unsigned long last_poll;
};

// ============================================================
// CONTROLLER TABEL
// ============================================================
#define NUM_CONTROLLERS 24

Controller controllers[NUM_CONTROLLERS] = {
  {"S-HVAC",    "192.168.0.70", "", TYPE_SYSTEM, true,  STATUS_PENDING,  "", "", 0},
  {"S-ECO",     "192.168.0.71", "", TYPE_SYSTEM, true,  STATUS_PENDING,  "", "", 0},
  {"S-OUTSIDE", "192.168.0.72", "", TYPE_SYSTEM, false, STATUS_INACTIVE, "", "", 0},
  {"FUT1",      "192.168.0.73", "", TYPE_SYSTEM, false, STATUS_INACTIVE, "", "", 0},
  {"FUT2",      "192.168.0.74", "", TYPE_SYSTEM, false, STATUS_INACTIVE, "", "", 0},
  {"S-ACCESS",  "192.168.0.82", "", TYPE_SYSTEM, false, STATUS_INACTIVE, "", "", 0},
  {"R-TESTROOM","192.168.0.80", "", TYPE_ROOM,   true,  STATUS_PENDING,  "", "", 0},
  {"R-BandB",   "192.168.0.75", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", "", 0},
  {"R-BADK",    "192.168.0.76", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", "", 0},
  {"R-INKOM",   "192.168.0.77", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", "", 0},
  {"R-KEUKEN",  "192.168.0.78", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", "", 0},
  {"R-WASPL",   "192.168.0.79", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", "", 0},
  {"R-EETPL",   "192.168.0.80", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", "", 0},
  {"R-ZITPL",   "192.168.0.81", "", TYPE_ROOM,   false, STATUS_INACTIVE, "", "", 0},
  {"P-BandB",   "", "30002c000547343233323032", TYPE_PHOTON, true,  STATUS_PENDING,  "", "", 0},
  {"P-Badkamer","", "5600420005504b464d323520", TYPE_PHOTON, true,  STATUS_PENDING,  "", "", 0},
  {"P-Inkom",   "", "420035000e47343432313031", TYPE_PHOTON, true,  STATUS_PENDING,  "", "", 0},
  {"P-Keuken",  "", "310017001647373335333438", TYPE_PHOTON, true,  STATUS_PENDING,  "", "", 0},
  {"P-Waspl",   "", "33004f000e504b464d323520", TYPE_PHOTON, true,  STATUS_PENDING,  "", "", 0},
  {"P-Eetpl",   "", "210042000b47343432313031", TYPE_PHOTON, true,  STATUS_PENDING,  "", "", 0},
  {"P-Zitpl",   "", "410038000547353138383138", TYPE_PHOTON, true,  STATUS_PENDING,  "", "", 0},
  {"P-TESTROOM","", "200033000547373336323230", TYPE_PHOTON, true,  STATUS_PENDING,  "", "", 0},
  {"(vrij)",    "", "", TYPE_SYSTEM, false, STATUS_INACTIVE, "", "", 0},
  {"(vrij)",    "", "", TYPE_SYSTEM, false, STATUS_INACTIVE, "", "", 0},
};

// ============================================================
// NVS
// ============================================================
Preferences preferences;
const char* NVS_NS          = "zarlar";
const char* NVS_WIFI_SSID   = "wifi_ssid";
const char* NVS_WIFI_PASS   = "wifi_pass";
const char* NVS_POLL_MIN    = "poll_min";
const char* NVS_MAX_ROWS    = "max_rows";
const char* NVS_ROOM_SCRIPT = "room_script";

// ============================================================
// GLOBALS
// ============================================================
WebServer  server(80);
DNSServer  dnsServer;

String wifi_ssid       = "";
String wifi_pass       = "";
int    poll_minutes    = 10;
int    max_rows        = 10000;
String room_script_url = "";

bool          ap_mode          = false;
unsigned long last_poll_cycle  = 0;
int           poll_index       = 0;
bool          polling_active   = false;
unsigned long poll_step_timer  = 0;

// ============================================================
// NVS LADEN / OPSLAAN
// ============================================================
void loadNVS() {
  preferences.begin(NVS_NS, true);
  wifi_ssid       = preferences.getString(NVS_WIFI_SSID, "");
  wifi_pass       = preferences.getString(NVS_WIFI_PASS, "");
  poll_minutes    = preferences.getInt(NVS_POLL_MIN, 10);
  max_rows        = preferences.getInt(NVS_MAX_ROWS, 10000);
  room_script_url = preferences.getString(NVS_ROOM_SCRIPT, "");
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    String ka = "c" + String(i) + "_act";
    String ku = "c" + String(i) + "_url";
    controllers[i].active     = preferences.getBool(ka.c_str(), controllers[i].active);
    controllers[i].sheets_url = preferences.getString(ku.c_str(), "");
    controllers[i].status     = controllers[i].active ? STATUS_PENDING : STATUS_INACTIVE;
  }
  preferences.end();
}

void saveNVS() {
  preferences.begin(NVS_NS, false);
  preferences.putString(NVS_WIFI_SSID, wifi_ssid);
  preferences.putString(NVS_WIFI_PASS, wifi_pass);
  preferences.putInt(NVS_POLL_MIN, poll_minutes);
  preferences.putInt(NVS_MAX_ROWS, max_rows);
  preferences.putString(NVS_ROOM_SCRIPT, room_script_url);
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    String ka = "c" + String(i) + "_act";
    String ku = "c" + String(i) + "_url";
    preferences.putBool(ka.c_str(), controllers[i].active);
    preferences.putString(ku.c_str(), controllers[i].sheets_url);
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
  IPAddress ip(192,168,0,60);
  IPAddress gw(192,168,0,1);
  IPAddress sn(255,255,255,0);
  IPAddress dns(8,8,8,8);
  WiFi.config(ip, gw, sn, gw, dns);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500); Serial.print("."); retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    // Sleep volledig uitschakelen voor maximale bereikbaarheid
    esp_wifi_set_ps(WIFI_PS_NONE);
    wifi_config_t wifi_cfg;
    esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);
    wifi_cfg.sta.listen_interval = 1;
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_pm_config_t pm_cfg = {
      .max_freq_mhz      = 160,
      .min_freq_mhz      = 160,
      .light_sleep_enable = false
    };
    esp_pm_configure(&pm_cfg);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    Serial.println("✓ WiFi verbonden: " + WiFi.localIP().toString());
    Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
    Serial.println("✓ Sleep uitgeschakeld (altijd bereikbaar)");

    if (MDNS.begin("zarlar")) Serial.println("  mDNS: http://zarlar.local");
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
  Serial.println("AP mode actief: SSID=Zarlar-Setup");
  Serial.println("IP: " + ap_ip.toString());
  Serial.println("→ Verbind met Zarlar-Setup en open browser");
}

// ============================================================
// POLL ÉÉN CONTROLLER
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
    controllers[i].last_json = http.getString();
    controllers[i].status    = STATUS_ONLINE;
    controllers[i].last_poll = millis();
    Serial.printf("  ✓ %s online (%d bytes)\n", controllers[i].name, controllers[i].last_json.length());
    http.end();
    delay(500);
    logControllerToSheets(i);
  } else {
    controllers[i].status = STATUS_OFFLINE;
    Serial.printf("  ✗ %s offline (HTTP %d)\n", controllers[i].name, code);
    http.end();
  }
}

// ============================================================
// LOG NAAR GOOGLE SHEETS
// ============================================================
void logControllerToSheets(int i) {
  String url = (controllers[i].type == TYPE_ROOM) ? room_script_url : controllers[i].sheets_url;
  if (url.length() == 0) return;
  if (controllers[i].last_json.length() == 0) return;

  Serial.printf("  [Sheets] POST %s...\n", controllers[i].name);

  HTTPClient http;
  http.begin(url.c_str());
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);
  int code = http.POST(controllers[i].last_json);
  if (code == 200 || code == 302) {
    Serial.printf("  [Sheets] %s OK ✓\n", controllers[i].name);
  } else {
    Serial.printf("  [Sheets] %s fout: %d\n", controllers[i].name, code);
  }
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
    if (controllers[poll_index].active &&
        controllers[poll_index].type != TYPE_PHOTON &&
        strlen(controllers[poll_index].ip) > 0) {
      pollESP32Controller(poll_index);
      poll_index++;
      return;
    }
    poll_index++;
  }
  polling_active = false;
  Serial.println("=== POLL CYCLUS KLAAR ===\n");
}

// ============================================================
// STATUS JSON
// ============================================================
String getStatusJson() {
  String j = "[";
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    if (i > 0) j += ",";
    j += "{\"n\":\"" + String(controllers[i].name) + "\","
         "\"t\":" + String(controllers[i].type) + ","
         "\"ip\":\"" + String(controllers[i].ip) + "\","
         "\"pid\":\"" + String(controllers[i].photon_id) + "\","
         "\"a\":" + (controllers[i].active ? "1" : "0") + ","
         "\"s\":" + String(controllers[i].status) + "}";
  }
  j += "]";
  return j;
}

// ============================================================
// HOOFDPAGINA — compact, geen externe fonts
// ============================================================
String getMainPage() {
  String h = F("<!DOCTYPE html><html lang='nl'><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Zarlar</title>"
    "<style>"
    "body{margin:0;background:#111;color:#eee;font-family:monospace;font-size:14px}"
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
         "min-width:120px;transition:border-color .15s}"
    ".btn:hover{border-color:#f0a500;color:#f0a500}"
    ".btn.off{opacity:.4;cursor:default}"
    ".btn.off:hover{border-color:#333;color:#ccc}"
    ".dot{width:8px;height:8px;border-radius:50%;flex-shrink:0}"
    "#portal{padding:0}"
    "#portal iframe{width:100%;height:700px;border:none;display:block;background:#fff}"
    "#portal .empty{padding:40px;text-align:center;color:#555}"
    "@media(max-width:600px){.btn{min-width:100px}#portal iframe{height:450px}}"
    "</style></head><body>"
    "<div class='hdr'><span class='hdr-t'>⬡ ZARLAR</span>"
    "<span class='hdr-r' id='clk'>192.168.0.60</span></div>"
    "<div class='nav'>"
    "<a href='/'>Dashboard</a>"
    "<a href='/settings'>⚙ Settings</a>"
    "<a href='/info'>Info</a></div>"
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
    "var C=[],PT=null;"
    "var PL={'datum':'Time','a':'CO2','b':'Dust','c':'Dew','d':'Humi','e':'Light',"
    "'f':'SUNLight','g':'Temp1','h':'Temp2','i':'MOV1','j':'MOV2',"
    "'k':'DewAlert','l':'TSTATon','m':'MOV1light','n':'MOV2light',"
    "'o':'BEAMvalue','p':'BEAMalert','q':'Night','r':'Bed','s':'R',"
    "'t':'G','u':'B','v':'Strength','w':'Quality','x':'FreeMem'};"
    "var SC=['#555','#f0c040','#2ecc40','#e74c3c'];"
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
    "    b.innerHTML=dot(c.s)+c.n;"
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
    "async function getToken(){"
    "  if(PT)return PT;"
    "  try{var r=await fetch('https://controllers-diagnose.filip-delannoy.workers.dev/token');"
    "  PT=(await r.json()).token;return PT;}catch(e){return null;}"
    "}"
    "async function fetchPhoton(i,id,name){"
    "  var p=document.getElementById('portal');"
    "  var tk=await getToken();"
    "  if(!tk){p.innerHTML='<div class=\"empty\" style=\"color:#e74c3c\">❌ Geen token</div>';C[i].s=3;updDots();return;}"
    "  try{"
    "    var r=await fetch('https://api.particle.io/v1/devices/'+id+'/JSON_status?access_token='+tk,{cache:'no-store'});"
    "    if(!r.ok)throw new Error('HTTP '+r.status);"
    "    var d=await r.json();"
    "    var raw=d.result||(d.body&&d.body.result)||null;"
    "    var obj=null;"
    "    if(raw){try{obj=typeof raw==='object'?raw:JSON.parse(raw);}catch(e){}}"
    "    if(!obj)throw new Error('Parse fout');"
    "    C[i].s=2;updDots();"
    "    var h='<div style=\"padding:14px;color:#2ecc40\">✓ '+name+' online</div>';"
    "    h+='<div style=\"padding:0 14px 14px\">';"
    "    for(var k in obj){var v=obj[k];var l=PL[k]||k;"
    "      h+='<div style=\"display:flex;justify-content:space-between;padding:5px 0;"
    "border-bottom:1px solid #333\"><span style=\"color:#f0a500\">'+l+'</span>"
    "<span>'+v+'</span></div>';}"
    "    h+='</div>';"
    "    p.innerHTML=h;"
    "  }catch(e){C[i].s=3;updDots();"
    "    p.innerHTML='<div class=\"empty\" style=\"color:#e74c3c\">❌ '+e.message+'</div>';}"
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
    "</script></body></html>");
  return h;
}

// ============================================================
// SETTINGS PAGINA
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
    ".badge{font-size:10px;padding:1px 5px;border-radius:2px;margin-left:4px}"
    ".bs{background:#e05a0022;color:#e05a00;border:1px solid #e05a0044}"
    ".br{background:#2ecc4022;color:#2ecc40;border:1px solid #2ecc4044}"
    ".bp{background:#0077ff22;color:#4af;border:1px solid #0077ff44}"
    ".save{background:#f0a500;color:#000;border:none;padding:10px 28px;"
          "border-radius:5px;font-size:13px;font-weight:bold;cursor:pointer;margin:14px 14px}"
    ".save:hover{background:#ffd000}"
    "</style></head><body>"
    "<div class='hdr'><span class='hdr-t'>⬡ ZARLAR SETTINGS</span></div>"
    "<div class='nav'><a href='/'>← Dashboard</a>"
    "<a href='/settings'>⚙ Settings</a><a href='/info'>Info</a></div>"
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
  h += "'></td></tr><tr><td>Max rijen / sheet</td>"
       "<td><input type='number' name='max_rows' min='100' max='100000' value='";
  h += String(max_rows);
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
       "</form></div></body></html>";
  return h;
}

// ============================================================
// INFO PAGINA
// ============================================================
String getInfoPage() {
  String h = "<html><head><meta charset='utf-8'><title>Info</title>"
    "<style>body{background:#111;color:#eee;font-family:monospace;padding:20px;}"
    "a{color:#f0a500;}</style></head><body>"
    "<h3 style='color:#f0a500'>Zarlar Dashboard v2.3</h3>";
  h += "IP: " + (ap_mode ? "AP " + WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "<br>";
  h += "Mode: " + String(ap_mode ? "AP (Zarlar-Setup)" : "STA") + "<br>";
  h += "RSSI: " + String(WiFi.RSSI()) + " dBm<br>";
  h += "Uptime: " + String(millis()/1000) + " s<br>";
  h += "Free heap: " + String(ESP.getFreeHeap()) + " bytes<br>";
  h += "Poll interval: " + String(poll_minutes) + " min<br>";
  h += "Max rijen: " + String(max_rows) + "<br><br>";
  h += "<a href='/'>← Dashboard</a>";
  h += "</body></html>";
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

  server.on("/info", HTTP_GET, []() {
    server.send(200, "text/html; charset=utf-8", getInfoPage());
  });

  server.on("/settings", HTTP_GET, []() {
    server.send(200, "text/html; charset=utf-8", getSettingsPage());
  });

  server.on("/save_settings", HTTP_GET, []() {
    if (server.hasArg("wifi_ssid"))   wifi_ssid       = server.arg("wifi_ssid");
    if (server.hasArg("wifi_pass"))   wifi_pass       = server.arg("wifi_pass");
    if (server.hasArg("poll_min"))    poll_minutes    = server.arg("poll_min").toInt();
    if (server.hasArg("max_rows"))    max_rows        = server.arg("max_rows").toInt();
    if (server.hasArg("room_script")) room_script_url = server.arg("room_script");
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

  // Captive portal redirects
  server.onNotFound([]() {
    if (ap_mode) {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/settings");
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "404");
    }
  });

  server.begin();
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n╔══════════════════════════════════╗");
  Serial.println("║  Zarlar Dashboard v2.3           ║");
  Serial.println("║  192.168.0.60 — zarlar.local     ║");
  Serial.println("╚══════════════════════════════════╝\n");

  loadNVS();
  connectWiFi();

  // Eerste poll na 30s
  last_poll_cycle = millis() - (unsigned long)(poll_minutes * 60000UL) + 30000UL;

  setupWebServer();
  Serial.println("\nREADY — http://192.168.0.60/\n");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  if (ap_mode) dnsServer.processNextRequest();
  server.handleClient();
  handlePolling();
  delay(2);
}
