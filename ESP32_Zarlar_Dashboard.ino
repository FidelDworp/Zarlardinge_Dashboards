/* ESP32_Zarlar_Dashboard = Server voor alle ROOM pages op ESP32 WROOM controller
 * Author: Fidel Dworp
 * Rev.Date: 26 feb 2026 20:00
 * 
 * Standalone webserver voor Zarlar dashboard
 * - Particle Photon: JSON fetch met correcte labels
 * - ESP32 controllers: iFrame embedded UI
 * Use Fixed IP: 192.168.0.60 of http://zarlar.local/
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ============== CONFIGURATIE ==============
// PAS DEZE AAN VOOR JOUW NETWERK!
const char* WIFI_SSID = "Delannoy";        // ← AANPASSEN!
const char* WIFI_PASS = "kampendaal,34";  // ← AANPASSEN!
const char* MDNS_NAME = "zarlar";          // → http://zarlar.local/

// ============== ESP32 CONTROLLER URLS ==============
// Kies tussen IP of .local (of beide proberen)
// TIP: Stel statische DHCP reservering in op je router voor beste resultaten

// Optie 1: Statische IP adressen (werkt altijd, indien correct geconfigureerd)
const char* ECO_URL_IP = "http://192.168.0.71/";      // Huidig DHCP adres
const char* TESTROOM_URL_IP = "http://192.168.0.80/"; // Huidig DHCP adres  
const char* HVAC_URL_IP = "http://192.168.0.70/";     // Huidig DHCP adres

// Optie 2: Bonjour/mDNS namen (werkt niet in alle browsers!)
const char* ECO_URL_MDNS = "http://eco.local/";
const char* TESTROOM_URL_MDNS = "http://eetplaats.local/";
const char* HVAC_URL_MDNS = "http://hvac.local/";

// GEBRUIK DEZE (kies hieronder welke je wilt gebruiken)
const char* ECO_URL = ECO_URL_IP;           // Verander naar ECO_URL_MDNS als je .local wilt
const char* TESTROOM_URL = TESTROOM_URL_IP; // Verander naar TESTROOM_URL_MDNS als je .local wilt
const char* HVAC_URL = HVAC_URL_IP;         // Verander naar HVAC_URL_MDNS als je .local wilt

// ============== GLOBALS ==============
WebServer server(80);

// ============== PHOTON DATA LABELS ==============
// Mapping van JSON keys naar leesbare labels
struct LabelMap {
  const char* key;
  const char* label;
};

const LabelMap PHOTON_LABELS[] = {
  {"datum", "Time"},
  {"a", "CO2"},
  {"b", "Dust"},
  {"c", "Dew"},
  {"d", "Humi"},
  {"e", "Light"},
  {"f", "SUNLight"},
  {"g", "Temp1"},
  {"h", "Temp2"},
  {"i", "MOV1"},
  {"j", "MOV2"},
  {"k", "DewAlert"},
  {"l", "TSTATon"},
  {"m", "MOV1light"},
  {"n", "MOV2light"},
  {"o", "BEAMvalue"},
  {"p", "BEAMalert"},
  {"q", "Night"},
  {"r", "Bed"},
  {"s", "R"},
  {"t", "G"},
  {"u", "B"},
  {"v", "Strength"},
  {"w", "Quality"},
  {"x", "FreeMem"}
};

String getLabel(String key) {
  for (int i = 0; i < sizeof(PHOTON_LABELS) / sizeof(PHOTON_LABELS[0]); i++) {
    if (key == PHOTON_LABELS[i].key) {
      return String(PHOTON_LABELS[i].label);
    }
  }
  return key; // Als geen label gevonden, return originele key
}

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔══════════════════════════════╗");
  Serial.println("║  ESP32 Zarlar Dashboard v1.1 ║");
  Serial.println("║  iFrame Edition              ║");
  Serial.println("║  24 januari 2026             ║");
  Serial.println("╚══════════════════════════════╝\n");
  
  // WiFi verbinden met FIXED IP!
  WiFi.mode(WIFI_STA);
  IPAddress static_ip(192, 168, 0, 60);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(static_ip, gateway, subnet, gateway);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Verbinden met WiFi");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi verbonden!");
    Serial.print("  IP adres: ");
    Serial.println(WiFi.localIP());
    Serial.print("  RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    // mDNS
    if (MDNS.begin(MDNS_NAME)) {
      Serial.print("  mDNS: http://");
      Serial.print(MDNS_NAME);
      Serial.println(".local/");
    }
  } else {
    Serial.println("✗ WiFi verbinding mislukt!");
    Serial.println("  Check SSID en wachtwoord in sketch!");
    Serial.println("  Restart ESP32 na aanpassen...");
    while(1) delay(1000); // Stop hier
  }
  
  // Web server setup
  setupWebServer();
  server.begin();
  
  Serial.println("\n╔══════════════════════════════╗");
  Serial.println("║      DASHBOARD READY!        ║");
  Serial.println("╚══════════════════════════════╝");
  Serial.println("\nOpen in browser:");
  Serial.print("  → http://");
  Serial.println(WiFi.localIP());
  Serial.print("  → http://");
  Serial.print(MDNS_NAME);
  Serial.println(".local/\n");
}

// ============== LOOP ==============
void loop() {
  server.handleClient();
  delay(2);
}

// ============== WEB SERVER ==============
void setupWebServer() {
  
  // Root endpoint - Zarlar Dashboard
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getZarlarHTML());
  });
  
  // Info endpoint (voor debugging)
  server.on("/info", HTTP_GET, []() {
    String info = "ESP32 Zarlar Dashboard v1.1\n";
    info += "═══════════════════════════\n";
    info += "IP: " + WiFi.localIP().toString() + "\n";
    info += "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
    info += "Uptime: " + String(millis()/1000) + " sec\n";
    info += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
    server.send(200, "text/plain", info);
  });
  
  // 404 handler
  server.onNotFound([]() {
    server.send(404, "text/plain", "404 - Not Found");
  });
}

// ============== HTML GENERATOR ==============
String getZarlarHTML() {
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="nl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Zarlar Dashboard</title>
<style>
* {box-sizing:border-box;margin:0;padding:0}
body {font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;min-height:100vh}
.header {text-align:center;padding:30px 20px;background:rgba(0,0,0,0.2)}
.header h1 {font-size:32px;text-shadow:0 2px 4px rgba(0,0,0,0.3);margin-bottom:10px}
.header p {opacity:0.9;font-size:14px}
.container {max-width:1200px;margin:0 auto;padding:20px}
.section {background:rgba(255,255,255,0.95);border-radius:12px;padding:20px;margin-bottom:20px;color:#222;box-shadow:0 4px 12px rgba(0,0,0,0.15)}
.section h2 {margin:0 0 15px 0;color:#667eea;font-size:20px;border-bottom:2px solid #667eea;padding-bottom:10px}
.buttons {display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}
.btn {padding:15px;border:none;border-radius:8px;font-size:15px;font-weight:600;cursor:pointer;transition:all 0.2s;color:#fff}
.btn:hover {transform:translateY(-2px);box-shadow:0 4px 8px rgba(0,0,0,0.2)}
.btn:active {transform:translateY(0)}
.btn-photon {background:#007bff}
.btn-photon:hover {background:#0056b3}
.btn-esp {background:#28a745}
.btn-esp:hover {background:#218838}
.btn-hvac {background:#ff6600}
.btn-hvac:hover {background:#e65c00}
#box {background:#fff;border-radius:8px;padding:20px;min-height:200px}
.iframe-container {position:relative;width:100%;background:#fff;border-radius:8px;overflow:hidden}
.iframe-header {background:#667eea;color:#fff;padding:15px;font-weight:600;font-size:18px;display:flex;justify-content:space-between;align-items:center}
.iframe-close {background:rgba(255,255,255,0.2);border:none;color:#fff;padding:5px 15px;border-radius:5px;cursor:pointer;font-size:14px}
.iframe-close:hover {background:rgba(255,255,255,0.3)}
iframe {width:100%;height:700px;border:none;display:block}
.row {display:flex;justify-content:space-between;align-items:center;padding:10px 0;border-bottom:1px solid #eee}
.row:last-child {border-bottom:none}
.lbl {font-weight:600;color:#667eea;flex:0 0 140px}
.val {font-family:monospace;font-weight:700;padding:6px 12px;border-radius:6px;background:#f0f0f0;text-align:center;min-width:100px}
.loading {text-align:center;padding:60px 20px;color:#999;font-size:16px}
.spinner {display:inline-block;width:40px;height:40px;border:4px solid #f3f3f3;border-top:4px solid #667eea;border-radius:50%;animation:spin 1s linear infinite;margin-bottom:20px}
@keyframes spin {0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}
.error {color:#dc3545;background:#ffe6e6;padding:15px;border-radius:8px;border-left:4px solid #dc3545}
.success {color:#28a745;background:#e6f9e6;padding:15px;border-radius:8px;border-left:4px solid #28a745;margin-bottom:15px}
@media(max-width:600px){.lbl{flex:0 0 100px;font-size:13px}.val{min-width:80px;font-size:13px}.buttons{grid-template-columns:1fr}iframe{height:500px}}
</style>
</head>
<body>
<div class="header">
<h1>🏠 Zarlar Dashboard</h1>
<p>Lokaal Netwerk Monitoring - ESP32 Hosted</p>
</div>

<div class="container">

<div class="section">
<h2>☁️ Particle Photon Controllers</h2>
<div class="buttons">
<button class="btn btn-photon" onclick="goPhoton('30002c000547343233323032','BandB')">BandB</button>
<button class="btn btn-photon" onclick="goPhoton('5600420005504b464d323520','Badkamer')">Badkamer</button>
<button class="btn btn-photon" onclick="goPhoton('420035000e47343432313031','Inkom')">Inkom</button>
<button class="btn btn-photon" onclick="goPhoton('310017001647373335333438','Keuken')">Keuken</button>
<button class="btn btn-photon" onclick="goPhoton('33004f000e504b464d323520','Wasplaats')">Wasplaats</button>
<button class="btn btn-photon" onclick="goPhoton('210042000b47343432313031','Eetplaats')">Eetplaats</button>
<button class="btn btn-photon" onclick="goPhoton('410038000547353138383138','Zitplaats')">Zitplaats</button>
<button class="btn btn-photon" onclick="goPhoton('200033000547373336323230','TESTROOM')">TESTROOM (Photon)</button>
</div>
</div>

<div class="section">
<h2>🔧 ESP32 Controllers</h2>
<div class="buttons">
<button class="btn btn-esp" onclick="goESP32('ECO_URL','🌞 ECO Boiler')">🌞 ECO Boiler</button>
<button class="btn btn-esp" onclick="goESP32('TESTROOM_URL','🏠 TESTROOM ESP32')">🏠 TESTROOM ESP32</button>
<button class="btn btn-hvac" onclick="goESP32('HVAC_URL','🔥 HVAC')">🔥 HVAC</button>
</div>
</div>

<div class="section">
<h2>📊 Controller Data</h2>
<div id="box">
<div class="loading">
<div class="spinner"></div><br>
Kies een controller hierboven
</div>
</div>
</div>

</div>

<script>
var TOKEN=null;

// ============================================================================
// PARTICLE PHOTON CONTROLLERS (zoals altijd)
// ============================================================================
async function getToken(){
if(TOKEN)return TOKEN;
try{
var r=await fetch('https://controllers-diagnose.filip-delannoy.workers.dev/token');
var d=await r.json();
TOKEN=d.token;
return TOKEN;
}catch(e){console.error("Token error:",e);return null}
}

async function goPhoton(id,name){
var box=document.getElementById('box');
box.innerHTML='<div class="loading"><div class="spinner"></div><br>Laden '+name+'...</div>';
var token=await getToken();
if(!token){box.innerHTML='<div class="error">❌ Geen Particle token</div>';return}
try{
var r=await fetch('https://api.particle.io/v1/devices/'+id+'/JSON_status?access_token='+token,{cache:'no-store'});
if(!r.ok)throw new Error('HTTP '+r.status);
var d=await r.json();
var p=parseResult(d.result||(d.body&&d.body.result)||null);
if(!p)throw new Error('Parse error');
showPhotonData(box,name,p);
}catch(e){
console.error('Error:',e);
box.innerHTML='<div class="error">❌ Fout: '+e.message+'</div>';
}
}

function parseResult(r){
if(!r)return null;
if(typeof r==='object')return r;
if(typeof r==='string'){
try{return JSON.parse(r)}catch(e){
try{return JSON.parse(decodeURIComponent(r))}catch(e2){return null}
}
}
return null;
}

function showPhotonData(box,name,data){
var h='<div class="success">✓ '+name+' - Verbonden</div>';



// Label mapping (sync met ESP32 sketch)
var labels={
'datum':'Time','a':'CO2','b':'Dust','c':'Dew','d':'Humi','e':'Light',
'f':'SUNLight','g':'Temp1','h':'Temp2','i':'MOV1','j':'MOV2',
'k':'DewAlert','l':'TSTATon','m':'MOV1light','n':'MOV2light',
'o':'BEAMvalue','p':'BEAMalert','q':'Night','r':'Bed','s':'R',
't':'G','u':'B','v':'Strength','w':'Quality','x':'FreeMem'
};
for(var k in data){
var v=data[k];
var lbl=labels[k]||k; // Gebruik label of originele key
if(typeof v==='object'){try{v=JSON.stringify(v)}catch(e){v='Object'}}
h+='<div class="row"><span class="lbl">'+lbl+':</span><span class="val">'+v+'</span></div>';
}



box.innerHTML=h;
}

// ============================================================================
// ESP32 CONTROLLERS (via iFrame)
// ============================================================================
function goESP32(url,name){
var box=document.getElementById('box');
box.innerHTML='<div class="iframe-container">'+
'<div class="iframe-header">'+
'<span>'+name+'</span>'+
'<button class="iframe-close" onclick="closeFrame()">✕ Sluiten</button>'+
'</div>'+
'<iframe src="'+url+'" title="'+name+'"></iframe>'+
'</div>';
}

function closeFrame(){
var box=document.getElementById('box');
box.innerHTML='<div class="loading">Kies een controller hierboven</div>';
}
</script>
</body>
</html>
)rawliteral";

  // Vervang URL placeholders
  html.replace("ECO_URL", ECO_URL);
  html.replace("TESTROOM_URL", TESTROOM_URL);
  html.replace("HVAC_URL", HVAC_URL);
  
  return html;
}
