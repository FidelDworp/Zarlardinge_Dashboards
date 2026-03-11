/* ============================================================
   Zarlar Dashboard v2.0
   ESP32 WROOM-32 @ 192.168.0.60 — http://zarlar.local
   Filip Delannoy

   11mar26  v2.0  Volledig herschreven:
     - 16 controllers (S- systeem + R- room + Photon)
     - Status indicatoren per controller (grijs/geel/groen/rood)
     - Google Sheets logging per controller (NVS)
     - Room controllers: gedeeld Apps Script
     - Systeem controllers: eigen Apps Script URL per stuk
     - Max rijen limiet per sheet (NVS, standaard 10.000)
     - iFrame zonder titelbalk
     - Settings volledig in NVS
   ============================================================ */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// ============================================================
// CONTROLLER TYPES
// ============================================================
#define TYPE_PHOTON  0   // Particle Photon — via cloud API
#define TYPE_SYSTEM  1   // S- controller — eigen JSON structuur
#define TYPE_ROOM    2   // R- controller — gedeelde JSON structuur

// ============================================================
// STATUS CODES
// ============================================================
#define STATUS_INACTIVE  0   // Niet geconfigureerd (grijs)
#define STATUS_PENDING   1   // Nog niet gepolld (geel)
#define STATUS_ONLINE    2   // Online (groen)
#define STATUS_OFFLINE   3   // Offline/onbereikbaar (rood)

// ============================================================
// CONTROLLER STRUCT
// ============================================================
struct Controller {
  const char* name;       // Weergavenaam
  const char* ip;         // IP adres (leeg voor Photon)
  const char* photon_id;  // Particle device ID (leeg voor ESP32)
  int         type;       // TYPE_PHOTON / TYPE_SYSTEM / TYPE_ROOM
  bool        active;     // Standaard actief ja/nee
  int         status;     // Runtime status
  String      sheets_url; // Google Sheets URL (uit NVS)
  String      last_json;  // Laatste ontvangen JSON
  unsigned long last_poll; // Tijdstip laatste poll
};

// ============================================================
// CONTROLLER TABEL — 16 controllers + 8 Photon
// ============================================================
#define NUM_CONTROLLERS 24

Controller controllers[NUM_CONTROLLERS] = {
  // Naam          IP               Photon ID                         Type         Active
  {"S-HVAC",    "192.168.0.70", "",                                TYPE_SYSTEM, true,  STATUS_PENDING, "", "", 0},
  {"S-ECO",     "192.168.0.71", "",                                TYPE_SYSTEM, true,  STATUS_PENDING, "", "", 0},
  {"S-OUTSIDE", "192.168.0.72", "",                                TYPE_SYSTEM, false, STATUS_INACTIVE,"", "", 0},
  {"FUT1",      "192.168.0.73", "",                                TYPE_SYSTEM, false, STATUS_INACTIVE,"", "", 0},
  {"FUT2",      "192.168.0.74", "",                                TYPE_SYSTEM, false, STATUS_INACTIVE,"", "", 0},
  {"S-ACCESS",  "192.168.0.82", "",                                TYPE_SYSTEM, false, STATUS_INACTIVE,"", "", 0},
  {"R-TESTROOM","192.168.0.80", "",                                TYPE_ROOM,   true,  STATUS_PENDING, "", "", 0},
  {"R-BandB",   "192.168.0.75", "",                                TYPE_ROOM,   false, STATUS_INACTIVE,"", "", 0},
  {"R-BADK",    "192.168.0.76", "",                                TYPE_ROOM,   false, STATUS_INACTIVE,"", "", 0},
  {"R-INKOM",   "192.168.0.77", "",                                TYPE_ROOM,   false, STATUS_INACTIVE,"", "", 0},
  {"R-KEUKEN",  "192.168.0.78", "",                                TYPE_ROOM,   false, STATUS_INACTIVE,"", "", 0},
  {"R-WASPL",   "192.168.0.79", "",                                TYPE_ROOM,   false, STATUS_INACTIVE,"", "", 0},
  {"R-EETPL",   "192.168.0.80", "",                                TYPE_ROOM,   false, STATUS_INACTIVE,"", "", 0},
  {"R-ZITPL",   "192.168.0.81", "",                                TYPE_ROOM,   false, STATUS_INACTIVE,"", "", 0},
  // Particle Photon controllers
  {"P-BandB",   "", "30002c000547343233323032",                    TYPE_PHOTON, true,  STATUS_PENDING, "", "", 0},
  {"P-Badkamer","", "5600420005504b464d323520",                    TYPE_PHOTON, true,  STATUS_PENDING, "", "", 0},
  {"P-Inkom",   "", "420035000e47343432313031",                    TYPE_PHOTON, true,  STATUS_PENDING, "", "", 0},
  {"P-Keuken",  "", "310017001647373335333438",                    TYPE_PHOTON, true,  STATUS_PENDING, "", "", 0},
  {"P-Waspl",   "", "33004f000e504b464d323520",                    TYPE_PHOTON, true,  STATUS_PENDING, "", "", 0},
  {"P-Eetpl",   "", "210042000b47343432313031",                    TYPE_PHOTON, true,  STATUS_PENDING, "", "", 0},
  {"P-Zitpl",   "", "410038000547353138383138",                    TYPE_PHOTON, true,  STATUS_PENDING, "", "", 0},
  {"P-TESTROOM","", "200033000547373336323230",                    TYPE_PHOTON, true,  STATUS_PENDING, "", "", 0},
  // Extra vrije slots
  {"(vrij)",    "", "",                                            TYPE_SYSTEM, false, STATUS_INACTIVE,"", "", 0},
  {"(vrij)",    "", "",                                            TYPE_SYSTEM, false, STATUS_INACTIVE,"", "", 0},
};

// ============================================================
// NVS SLEUTELS
// ============================================================
Preferences preferences;
const char* NVS_NS          = "zarlar";
const char* NVS_WIFI_SSID   = "wifi_ssid";
const char* NVS_WIFI_PASS   = "wifi_pass";
const char* NVS_POLL_MIN    = "poll_min";
const char* NVS_MAX_ROWS    = "max_rows";
const char* NVS_ROOM_SCRIPT = "room_script"; // Gedeeld Apps Script voor R- controllers

// ============================================================
// GLOBALS
// ============================================================
WebServer server(80);

String wifi_ssid       = "Delannoy";
String wifi_pass       = "kampendaal,34";
int    poll_minutes    = 10;
int    max_rows        = 10000;
String room_script_url = "";   // Gedeeld Apps Script voor alle R- rooms

unsigned long last_poll_cycle = 0;
int           poll_index      = 0;   // Welke controller wordt nu gepolld
bool          polling_active  = false;
unsigned long poll_step_timer = 0;

// Photon token cache
String photon_token = "";

// ============================================================
// NVS LADEN
// ============================================================
void loadNVS() {
  preferences.begin(NVS_NS, true);
  wifi_ssid       = preferences.getString(NVS_WIFI_SSID, "Delannoy");
  wifi_pass       = preferences.getString(NVS_WIFI_PASS, "kampendaal,34");
  poll_minutes    = preferences.getInt(NVS_POLL_MIN, 10);
  max_rows        = preferences.getInt(NVS_MAX_ROWS, 10000);
  room_script_url = preferences.getString(NVS_ROOM_SCRIPT, "");

  // Per controller: active flag + sheets URL
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    String key_act = "c" + String(i) + "_act";
    String key_url = "c" + String(i) + "_url";
    bool stored_active = preferences.getBool(key_act.c_str(), controllers[i].active);
    controllers[i].active = stored_active;
    controllers[i].sheets_url = preferences.getString(key_url.c_str(), "");
    controllers[i].status = stored_active ? STATUS_PENDING : STATUS_INACTIVE;
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
    String key_act = "c" + String(i) + "_act";
    String key_url = "c" + String(i) + "_url";
    preferences.putBool(key_act.c_str(), controllers[i].active);
    preferences.putString(key_url.c_str(), controllers[i].sheets_url);
  }
  preferences.end();
}

// ============================================================
// HELPER: STATUS KLEUR CSS
// ============================================================
String statusColor(int status) {
  switch (status) {
    case STATUS_INACTIVE: return "#888";
    case STATUS_PENDING:  return "#f0c040";
    case STATUS_ONLINE:   return "#2ecc40";
    case STATUS_OFFLINE:  return "#e74c3c";
  }
  return "#888";
}

String statusLabel(int status) {
  switch (status) {
    case STATUS_INACTIVE: return "Niet actief";
    case STATUS_PENDING:  return "Wacht op poll";
    case STATUS_ONLINE:   return "Online";
    case STATUS_OFFLINE:  return "Offline";
  }
  return "?";
}

// ============================================================
// POLL ÉÉN ESP32 CONTROLLER
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
    controllers[i].last_json  = http.getString();
    controllers[i].status     = STATUS_ONLINE;
    controllers[i].last_poll  = millis();
    Serial.printf("  ✓ %s online (%d bytes)\n", controllers[i].name, controllers[i].last_json.length());

    // Log naar Google Sheets
    logControllerToSheets(i);

  } else {
    controllers[i].status = STATUS_OFFLINE;
    Serial.printf("  ✗ %s offline (HTTP %d)\n", controllers[i].name, code);
  }

  http.end();
}

// ============================================================
// LOG NAAR GOOGLE SHEETS
// ============================================================
void logControllerToSheets(int i) {
  String url = "";

  if (controllers[i].type == TYPE_ROOM) {
    url = room_script_url;
  } else {
    url = controllers[i].sheets_url;
  }

  if (url.length() == 0) return;
  if (controllers[i].last_json.length() == 0) return;

  Serial.printf("  [Sheets] POST %s...\n", controllers[i].name);

  HTTPClient http;
  http.begin(url);
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
// POLL CYCLUS — gespreid, 2s per controller
// ============================================================
void handlePolling() {
  unsigned long now = millis();

  // Start nieuwe cyclus na poll_minutes
  if (!polling_active) {
    if (now - last_poll_cycle >= (unsigned long)poll_minutes * 60000UL) {
      last_poll_cycle  = now;
      polling_active   = true;
      poll_index       = 0;
      poll_step_timer  = now;
      Serial.println("\n=== START POLL CYCLUS ===");
    }
    return;
  }

  // Wacht 2s tussen polls
  if (now - poll_step_timer < 2000) return;
  poll_step_timer = now;

  // Sla Photon over (die worden client-side gepolld via JS)
  // Sla inactieve over
  while (poll_index < NUM_CONTROLLERS) {
    if (controllers[poll_index].active &&
        controllers[poll_index].type != TYPE_PHOTON &&
        strlen(controllers[poll_index].ip) > 0) {
      pollESP32Controller(poll_index);
      poll_index++;
      return;  // Volgende stap over 2s
    }
    poll_index++;
  }

  // Cyclus klaar
  polling_active = false;
  Serial.println("=== POLL CYCLUS KLAAR ===\n");
}

// ============================================================
// STATUS JSON ENDPOINT — voor browser polling
// ============================================================
String getStatusJson() {
  String json = "[";
  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"name\":\"" + String(controllers[i].name) + "\",";
    json += "\"type\":" + String(controllers[i].type) + ",";
    json += "\"ip\":\"" + String(controllers[i].ip) + "\",";
    json += "\"photon_id\":\"" + String(controllers[i].photon_id) + "\",";
    json += "\"active\":" + String(controllers[i].active ? "true" : "false") + ",";
    json += "\"status\":" + String(controllers[i].status);
    json += "}";
  }
  json += "]";
  return json;
}

// ============================================================
// HOOFDPAGINA HTML
// ============================================================
String getMainPage() {
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="nl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Zarlar Dashboard</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');
:root {
  --bg:      #0d1117;
  --panel:   #161b22;
  --border:  #30363d;
  --accent:  #f0a500;
  --accent2: #e05a00;
  --txt:     #e6edf3;
  --txt2:    #8b949e;
  --green:   #2ecc40;
  --red:     #e74c3c;
  --yellow:  #f0c040;
  --grey:    #555;
}
* { box-sizing:border-box; margin:0; padding:0; }
body { background:var(--bg); color:var(--txt); font-family:'Exo 2',sans-serif; min-height:100vh; }

/* HEADER */
.header {
  background: linear-gradient(90deg, #0d1117 0%, #1a1200 50%, #0d1117 100%);
  border-bottom: 2px solid var(--accent);
  padding: 16px 24px;
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.header-title {
  font-family:'Share Tech Mono',monospace;
  font-size: 22px;
  color: var(--accent);
  letter-spacing: 3px;
  text-shadow: 0 0 20px rgba(240,165,0,0.4);
}
.header-right {
  font-size: 11px;
  color: var(--txt2);
  text-align: right;
  font-family:'Share Tech Mono',monospace;
}
.nav {
  display: flex;
  gap: 6px;
  padding: 10px 24px;
  background: var(--panel);
  border-bottom: 1px solid var(--border);
  flex-wrap: wrap;
}
.nav a {
  color: var(--txt2);
  text-decoration: none;
  font-size: 12px;
  padding: 5px 12px;
  border-radius: 4px;
  border: 1px solid var(--border);
  transition: all 0.2s;
  font-family:'Share Tech Mono',monospace;
}
.nav a:hover { color:var(--accent); border-color:var(--accent); }

/* LAYOUT */
.container { padding: 20px 24px; max-width: 1400px; }

/* SECTIES */
.section {
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  margin-bottom: 20px;
  overflow: hidden;
}
.section-title {
  padding: 10px 16px;
  font-size: 12px;
  font-weight: 600;
  letter-spacing: 2px;
  text-transform: uppercase;
  color: var(--accent);
  background: rgba(240,165,0,0.06);
  border-bottom: 1px solid var(--border);
  font-family:'Share Tech Mono',monospace;
}

/* CONTROLLER GRID */
.ctrl-grid {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  padding: 14px 16px;
}
.ctrl-btn {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 14px;
  background: #1c2128;
  border: 1px solid var(--border);
  border-radius: 6px;
  cursor: pointer;
  font-family:'Exo 2',sans-serif;
  font-size: 13px;
  font-weight: 600;
  color: var(--txt);
  transition: all 0.15s;
  min-width: 130px;
}
.ctrl-btn:hover { border-color: var(--accent); color: var(--accent); background: #1f2430; }
.ctrl-btn.inactive { opacity: 0.4; cursor: default; }
.ctrl-btn.inactive:hover { border-color: var(--border); color: var(--txt); background: #1c2128; }
.status-dot {
  width: 9px; height: 9px;
  border-radius: 50%;
  flex-shrink: 0;
  box-shadow: 0 0 6px currentColor;
}

/* FRAME */
.frame-wrap {
  padding: 0;
}
.frame-wrap iframe {
  width: 100%;
  height: 750px;
  border: none;
  display: block;
  background: #fff;
}
.frame-empty {
  padding: 60px 20px;
  text-align: center;
  color: var(--txt2);
  font-size: 14px;
}
.frame-empty span {
  display: block;
  font-size: 32px;
  margin-bottom: 12px;
  opacity: 0.3;
}

/* PHOTON DATA */
.photon-data { padding: 16px; }
.photon-row {
  display: flex;
  justify-content: space-between;
  padding: 7px 0;
  border-bottom: 1px solid var(--border);
  font-size: 13px;
}
.photon-row:last-child { border-bottom: none; }
.photon-lbl { color: var(--accent); font-weight: 600; }
.photon-val { font-family:'Share Tech Mono',monospace; color: var(--txt); }
.msg-ok   { color:var(--green); padding:12px 16px; font-size:13px; }
.msg-err  { color:var(--red);   padding:12px 16px; font-size:13px; }
.msg-load { color:var(--yellow);padding:12px 16px; font-size:13px; }

@media(max-width:600px){
  .ctrl-btn { min-width: 110px; font-size:12px; }
  .frame-wrap iframe { height:500px; }
}
</style>
</head>
<body>

<div class="header">
  <div class="header-title">⬡ ZARLAR DASHBOARD</div>
  <div class="header-right" id="hdr-time">192.168.0.60 &nbsp;|&nbsp; zarlar.local</div>
</div>

<div class="nav">
  <a href="/">Dashboard</a>
  <a href="/settings">⚙ Settings</a>
  <a href="/info">Info</a>
</div>

<div class="container">

  <!-- SYSTEEM CONTROLLERS -->
  <div class="section">
    <div class="section-title">▸ Systeem Controllers</div>
    <div class="ctrl-grid" id="grid-system"></div>
  </div>

  <!-- ROOM CONTROLLERS -->
  <div class="section">
    <div class="section-title">▸ Room Controllers</div>
    <div class="ctrl-grid" id="grid-room"></div>
  </div>

  <!-- PARTICLE PHOTON -->
  <div class="section">
    <div class="section-title">☁ Particle Photon Controllers</div>
    <div class="ctrl-grid" id="grid-photon"></div>
  </div>

  <!-- PORTAL -->
  <div class="section" id="portal-section">
    <div class="section-title" id="portal-title">▸ Controller Portal</div>
    <div id="portal-box">
      <div class="frame-empty"><span>⬡</span>Kies een controller hierboven</div>
    </div>
  </div>

</div>

<script>
// ============================================================
// DATA van server
// ============================================================
var CONTROLLERS = [];
var photonToken = null;

var PHOTON_LABELS = {
  'datum':'Time','a':'CO2','b':'Dust','c':'Dew','d':'Humi','e':'Light',
  'f':'SUNLight','g':'Temp1','h':'Temp2','i':'MOV1','j':'MOV2',
  'k':'DewAlert','l':'TSTATon','m':'MOV1light','n':'MOV2light',
  'o':'BEAMvalue','p':'BEAMalert','q':'Night','r':'Bed','s':'R',
  't':'G','u':'B','v':'Strength','w':'Quality','x':'FreeMem',
  'y':'HeatReq','z':'VentReq','aa':'Setpoint','ab':'RoomTemp','af':'HomeStatus'
};

// Status dot HTML
function dot(status) {
  var colors = ['#555','#f0c040','#2ecc40','#e74c3c'];
  var labels = ['Niet actief','Wacht op poll','Online','Offline'];
  var c = colors[status] || '#555';
  var l = labels[status] || '?';
  return '<span class="status-dot" style="background:'+c+';color:'+c+';" title="'+l+'"></span>';
}

// Bouw controller knoppen op
function buildGrids() {
  var sys = document.getElementById('grid-system');
  var room = document.getElementById('grid-room');
  var phot = document.getElementById('grid-photon');
  sys.innerHTML = ''; room.innerHTML = ''; phot.innerHTML = '';

  CONTROLLERS.forEach(function(c, i) {
    var inactive = !c.active ? ' inactive' : '';
    var btn = document.createElement('button');
    btn.className = 'ctrl-btn' + inactive;
    btn.id = 'btn-' + i;
    btn.innerHTML = dot(c.status) + c.name;
    btn.title = c.name + ' — ' + (c.ip || c.photon_id);
    if (c.active) {
      btn.onclick = function() { openController(i); };
    }
    if (c.type === 0) phot.appendChild(btn);
    else if (c.type === 1) sys.appendChild(btn);
    else room.appendChild(btn);
  });
}

// Update enkel de status dots
function updateDots() {
  CONTROLLERS.forEach(function(c, i) {
    var btn = document.getElementById('btn-' + i);
    if (!btn) return;
    var old = btn.querySelector('.status-dot');
    if (old) old.outerHTML = dot(c.status);
  });
}

// Open controller in portal
function openController(i) {
  var c = CONTROLLERS[i];
  var box = document.getElementById('portal-box');
  var title = document.getElementById('portal-title');
  title.textContent = '▸ ' + c.name;

  if (c.type === 0) {
    // Photon: data ophalen
    box.innerHTML = '<div class="msg-load">⏳ Laden ' + c.name + '...</div>';
    fetchPhoton(i, c.photon_id, c.name);
  } else {
    // ESP32: iFrame
    var url = 'http://' + c.ip + '/';
    box.innerHTML = '<div class="frame-wrap"><iframe src="' + url + '" title="' + c.name + '"></iframe></div>';
  }

  // Scroll naar portal
  document.getElementById('portal-section').scrollIntoView({behavior:'smooth'});
}

// ============================================================
// PHOTON
// ============================================================
async function getToken() {
  if (photonToken) return photonToken;
  try {
    var r = await fetch('https://controllers-diagnose.filip-delannoy.workers.dev/token');
    var d = await r.json();
    photonToken = d.token;
    return photonToken;
  } catch(e) { return null; }
}

async function fetchPhoton(i, id, name) {
  var box = document.getElementById('portal-box');
  var token = await getToken();
  if (!token) {
    box.innerHTML = '<div class="msg-err">❌ Geen Particle token</div>';
    CONTROLLERS[i].status = 3;
    updateDots();
    return;
  }
  try {
    var r = await fetch('https://api.particle.io/v1/devices/'+id+'/JSON_status?access_token='+token, {cache:'no-store'});
    if (!r.ok) throw new Error('HTTP ' + r.status);
    var d = await r.json();
    var raw = d.result || (d.body && d.body.result) || null;
    var p = parsePhoton(raw);
    if (!p) throw new Error('Parse fout');
    CONTROLLERS[i].status = 2;
    updateDots();
    showPhotonData(box, name, p);
  } catch(e) {
    CONTROLLERS[i].status = 3;
    updateDots();
    box.innerHTML = '<div class="msg-err">❌ ' + e.message + '</div>';
  }
}

function parsePhoton(r) {
  if (!r) return null;
  if (typeof r === 'object') return r;
  if (typeof r === 'string') {
    try { return JSON.parse(r); } catch(e) {
      try { return JSON.parse(decodeURIComponent(r)); } catch(e2) { return null; }
    }
  }
  return null;
}

function showPhotonData(box, name, data) {
  var h = '<div class="msg-ok">✓ ' + name + ' — Online</div><div class="photon-data">';
  for (var k in data) {
    var v = data[k];
    var lbl = PHOTON_LABELS[k] || k;
    if (typeof v === 'object') { try { v = JSON.stringify(v); } catch(e) { v = 'Object'; } }
    h += '<div class="photon-row"><span class="photon-lbl">' + lbl + '</span><span class="photon-val">' + v + '</span></div>';
  }
  h += '</div>';
  box.innerHTML = h;
}

// ============================================================
// STATUS POLLING (elke 15s van server)
// ============================================================
function fetchStatus() {
  fetch('/status_json')
    .then(function(r){ return r.json(); })
    .then(function(data){
      CONTROLLERS = data;
      updateDots();
    })
    .catch(function(){});
}

// ============================================================
// INIT
// ============================================================
fetch('/status_json')
  .then(function(r){ return r.json(); })
  .then(function(data){
    CONTROLLERS = data;
    buildGrids();
  });

setInterval(fetchStatus, 15000);

// Header klok
function updateClock(){
  var n = new Date();
  var t = n.toLocaleTimeString('nl-BE');
  document.getElementById('hdr-time').textContent = '192.168.0.60  |  ' + t;
}
setInterval(updateClock, 1000);
updateClock();
</script>
</body>
</html>
)rawliteral";
  return html;
}

// ============================================================
// SETTINGS PAGINA
// ============================================================
String getSettingsPage() {
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="nl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Zarlar Settings</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');
:root{--bg:#0d1117;--panel:#161b22;--border:#30363d;--accent:#f0a500;--txt:#e6edf3;--txt2:#8b949e;}
*{box-sizing:border-box;margin:0;padding:0;}
body{background:var(--bg);color:var(--txt);font-family:'Exo 2',sans-serif;min-height:100vh;}
.header{background:linear-gradient(90deg,#0d1117,#1a1200,#0d1117);border-bottom:2px solid var(--accent);padding:16px 24px;display:flex;align-items:center;justify-content:space-between;}
.header-title{font-family:'Share Tech Mono',monospace;font-size:22px;color:var(--accent);letter-spacing:3px;}
.nav{display:flex;gap:6px;padding:10px 24px;background:var(--panel);border-bottom:1px solid var(--border);}
.nav a{color:var(--txt2);text-decoration:none;font-size:12px;padding:5px 12px;border-radius:4px;border:1px solid var(--border);font-family:'Share Tech Mono',monospace;}
.nav a:hover{color:var(--accent);border-color:var(--accent);}
.container{padding:20px 24px;max-width:900px;}
.section{background:var(--panel);border:1px solid var(--border);border-radius:8px;margin-bottom:20px;overflow:hidden;}
.section-title{padding:10px 16px;font-size:12px;font-weight:600;letter-spacing:2px;text-transform:uppercase;color:var(--accent);background:rgba(240,165,0,0.06);border-bottom:1px solid var(--border);font-family:'Share Tech Mono',monospace;}
table{width:100%;border-collapse:collapse;}
td{padding:9px 16px;border-bottom:1px solid var(--border);font-size:13px;vertical-align:middle;}
td:first-child{color:var(--txt2);width:220px;white-space:nowrap;}
input[type=text],input[type=password],input[type=number]{background:#0d1117;border:1px solid var(--border);color:var(--txt);padding:6px 10px;border-radius:4px;font-size:13px;width:100%;font-family:'Share Tech Mono',monospace;}
input:focus{outline:none;border-color:var(--accent);}
input[type=checkbox]{width:18px;height:18px;cursor:pointer;accent-color:var(--accent);}
.ctrl-row td:first-child{font-weight:600;font-family:'Share Tech Mono',monospace;color:var(--txt);}
.type-badge{font-size:10px;padding:2px 7px;border-radius:3px;margin-left:6px;font-weight:600;}
.type-s{background:#e05a0022;color:#e05a00;border:1px solid #e05a0044;}
.type-r{background:#2ecc4022;color:#2ecc40;border:1px solid #2ecc4044;}
.type-p{background:#0077ff22;color:#4af;border:1px solid #0077ff44;}
.btn-save{background:var(--accent);color:#000;border:none;padding:12px 32px;border-radius:6px;font-size:14px;font-weight:800;cursor:pointer;font-family:'Exo 2',sans-serif;letter-spacing:1px;margin:20px 16px;}
.btn-save:hover{background:#ffd000;}
</style>
</head>
<body>
<div class="header">
  <div class="header-title">⬡ ZARLAR SETTINGS</div>
</div>
<div class="nav">
  <a href="/">← Dashboard</a>
  <a href="/settings">⚙ Settings</a>
  <a href="/info">Info</a>
</div>
<div class="container">
<form action="/save_settings" method="get">

  <div class="section">
    <div class="section-title">▸ Netwerk</div>
    <table>
      <tr><td>WiFi SSID</td><td><input type="text" name="wifi_ssid" value=")rawliteral";
  html += wifi_ssid;
  html += R"rawliteral("></td></tr>
      <tr><td>WiFi Wachtwoord</td><td><input type="password" name="wifi_pass" value=")rawliteral";
  html += wifi_pass;
  html += R"rawliteral("></td></tr>
    </table>
  </div>

  <div class="section">
    <div class="section-title">▸ Logging</div>
    <table>
      <tr><td>Poll interval (minuten)</td><td><input type="number" name="poll_min" min="1" max="60" value=")rawliteral";
  html += String(poll_minutes);
  html += R"rawliteral("></td></tr>
      <tr><td>Max rijen per sheet</td><td><input type="number" name="max_rows" min="100" max="100000" value=")rawliteral";
  html += String(max_rows);
  html += R"rawliteral("></td></tr>
      <tr><td>Room Script URL<br><small style="color:#8b949e;">(gedeeld voor alle R- controllers)</small></td>
          <td><input type="text" name="room_script" value=")rawliteral";
  html += room_script_url;
  html += R"rawliteral(" placeholder="https://script.google.com/macros/s/.../exec"></td></tr>
    </table>
  </div>

  <div class="section">
    <div class="section-title">▸ Controllers</div>
    <table>
      <tr style="background:rgba(240,165,0,0.04);">
        <td style="color:var(--accent);font-size:11px;font-weight:600;">Controller</td>
        <td style="color:var(--accent);font-size:11px;font-weight:600;">IP</td>
        <td style="color:var(--accent);font-size:11px;font-weight:600;width:60px;text-align:center;">Actief</td>
        <td style="color:var(--accent);font-size:11px;font-weight:600;">Sheets URL (S- en R-systeem)</td>
      </tr>
)rawliteral";

  String type_names[] = {"S","R","S"}; // photon=0→S display, system=1→S, room=2→R
  String type_styles[] = {"type-p","type-s","type-r"};

  for (int i = 0; i < NUM_CONTROLLERS; i++) {
    String tname = (controllers[i].type == TYPE_PHOTON) ? "P" :
                   (controllers[i].type == TYPE_SYSTEM) ? "S" : "R";
    String tstyle = (controllers[i].type == TYPE_PHOTON) ? "type-p" :
                    (controllers[i].type == TYPE_SYSTEM) ? "type-s" : "type-r";

    html += "<tr class='ctrl-row'>";
    html += "<td>" + String(controllers[i].name) +
            "<span class='type-badge " + tstyle + "'>" + tname + "</span></td>";
    html += "<td><small style='color:#8b949e;font-family:monospace;'>" +
            String(strlen(controllers[i].ip) > 0 ? controllers[i].ip : controllers[i].photon_id) +
            "</small></td>";
    html += "<td style='text-align:center;'><input type='checkbox' name='act_" + String(i) +
            "' value='1'" + String(controllers[i].active ? " checked" : "") + "></td>";

    // URL veld: niet voor Photon (client-side), niet voor R- (gedeeld script)
    if (controllers[i].type == TYPE_ROOM || controllers[i].type == TYPE_PHOTON) {
      html += "<td><small style='color:#555;'>— gedeeld script —</small></td>";
    } else {
      html += "<td><input type='text' name='url_" + String(i) +
              "' value='" + controllers[i].sheets_url +
              "' placeholder='https://script.google.com/...'></td>";
    }
    html += "</tr>";
  }

  html += R"rawliteral(
    </table>
  </div>

  <button type="submit" class="btn-save">💾 Opslaan &amp; Herstart</button>
</form>
</div>
</body>
</html>
)rawliteral";
  return html;
}

// ============================================================
// INFO PAGINA
// ============================================================
String getInfoPage() {
  String html = "<html><head><meta charset='utf-8'><title>Info</title>";
  html += "<style>body{background:#0d1117;color:#e6edf3;font-family:monospace;padding:24px;}</style></head><body>";
  html += "<h2 style='color:#f0a500;'>Zarlar Dashboard v2.0 — Info</h2><br>";
  html += "IP: " + WiFi.localIP().toString() + "<br>";
  html += "RSSI: " + String(WiFi.RSSI()) + " dBm<br>";
  html += "Uptime: " + String(millis() / 1000) + " s<br>";
  html += "Free heap: " + String(ESP.getFreeHeap()) + " bytes<br>";
  html += "Poll interval: " + String(poll_minutes) + " min<br>";
  html += "Max rijen: " + String(max_rows) + "<br><br>";
  html += "<a href='/' style='color:#f0a500;'>← Dashboard</a>";
  html += "</body></html>";
  return html;
}

// ============================================================
// WEB SERVER SETUP
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
    if (server.hasArg("wifi_ssid")) wifi_ssid    = server.arg("wifi_ssid");
    if (server.hasArg("wifi_pass")) wifi_pass    = server.arg("wifi_pass");
    if (server.hasArg("poll_min"))  poll_minutes = server.arg("poll_min").toInt();
    if (server.hasArg("max_rows"))  max_rows     = server.arg("max_rows").toInt();
    if (server.hasArg("room_script")) room_script_url = server.arg("room_script");

    for (int i = 0; i < NUM_CONTROLLERS; i++) {
      controllers[i].active = server.hasArg("act_" + String(i));
      controllers[i].status = controllers[i].active ? STATUS_PENDING : STATUS_INACTIVE;
      String url_key = "url_" + String(i);
      if (server.hasArg(url_key)) {
        controllers[i].sheets_url = server.arg(url_key);
      }
    }

    saveNVS();
    server.send(200, "text/html",
      "<html><body style='background:#0d1117;color:#f0a500;font-family:monospace;padding:40px;text-align:center;'>"
      "<h2>✓ Opgeslagen!</h2><p>Herstart over 2 seconden...</p>"
      "<script>setTimeout(()=>location.href='/',2500);</script>"
      "</body></html>");
    delay(2000);
    ESP.restart();
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "404 Not Found");
  });

  server.begin();
  Serial.println("Web server gestart.");
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n╔══════════════════════════════════╗");
  Serial.println("║  Zarlar Dashboard v2.0           ║");
  Serial.println("║  192.168.0.60 — zarlar.local     ║");
  Serial.println("╚══════════════════════════════════╝\n");

  loadNVS();

  // WiFi met fixed IP
  WiFi.mode(WIFI_STA);
  IPAddress ip(192,168,0,60);
  IPAddress gw(192,168,0,1);
  IPAddress sn(255,255,255,0);
  WiFi.config(ip, gw, sn, gw);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

  Serial.print("WiFi verbinden");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500); Serial.print("."); retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi verbonden: " + WiFi.localIP().toString());
    Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
    if (MDNS.begin("zarlar")) Serial.println("  mDNS: http://zarlar.local");
  } else {
    Serial.println("✗ WiFi mislukt — controleer SSID/wachtwoord in /settings");
  }

  // Eerste poll direct na 30s (niet meteen — WiFi stabiliseren)
  last_poll_cycle = millis() - (unsigned long)(poll_minutes * 60000UL) + 30000UL;

  setupWebServer();
  Serial.println("\nREADY — http://192.168.0.60/\n");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  server.handleClient();
  handlePolling();
  delay(2);
}
