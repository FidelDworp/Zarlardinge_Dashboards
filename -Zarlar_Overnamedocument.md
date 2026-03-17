# Zarlar Thuisautomatisering — Master Overnamedocument
**ESP32-C6 · Arduino IDE · Matter · Google Sheets**
*Filip Delannoy — Zarlardinge (BE) — bijgewerkt 17 maart 2026*

---

## 1. Systeemoverzicht

### 1.1 Wat is Zarlar?

Een volledig zelfgebouwd thuisautomatiseringssysteem op basis van drie ESP32-C6 controllers, elk met een eigen AsyncWebServer en Matter-integratie via WiFi. Een vierde apparaat — het **Zarlar Dashboard** op 192.168.0.60 — fungeert als centrale dataverzamelaar: het pollt de JSON-endpoints van alle controllers en POST de data naar Google Sheets via Google Apps Script.

```
[ROOM 192.168.0.80] ──┐
[HVAC 192.168.0.70] ──┼──→ Zarlar Dashboard 192.168.0.60 ──→ Google Sheets
[ECO  192.168.0.71] ──┘
         │
         └──→ Apple Home (via Matter/WiFi)
```

**Belangrijk leermoment:** de HVAC-controller deed vroeger zelf de HTTPS POST naar Google Sheets. Dit mislukte structureel (te lang, heap-druk). De POST is nu volledig gedelegeerd aan het Zarlar Dashboard. Elke controller publiceert alleen zijn `/json` endpoint — het dashboard doet de rest.

### 1.2 Controllers — huidige staat

| Controller | IP | MAC | Versie | Status |
|---|---|---|---|---|
| **HVAC** | 192.168.0.70 | 58:8C:81:32:29:54 | v1.18 | ✅ Productie, stabiel |
| **ECO Boiler** | 192.168.0.71 | 58:8C:81:32:2B:D4 | v1.22 | ✅ Productie, stabiel |
| **ROOM** | 192.168.0.80 | 58:8C:81:5D:B0:88 | v2.10 | ✅ Matter + heap stabiel |
| **Zarlar Dashboard** | 192.168.0.60 | — | — | Verzamelt JSON, POST naar Google |

### 1.3 Partitietabel (identiek voor alle drie)

Bestand: `partitions_16mb.csv` — plaatsen naast het `.ino` bestand in de schetsmap.

| Naam | Type | Offset | Grootte |
|------|------|--------|---------|
| nvs | data/nvs | 0x9000 | 20 KB |
| otadata | data/ota | 0xe000 | 8 KB |
| app0 | app/ota_0 | 0x10000 | 6 MB |
| app1 | app/ota_1 | 0x610000 | 6 MB |
| spiffs | data/spiffs | 0xC10000 | ~4 MB |

⚠️ **Nooit `huge_app` gebruiken** — had maar één app-slot en brak OTA. `app0` + `app1` elk 6 MB → OTA volledig werkend.

### 1.4 Arduino IDE instellingen

| Instelling | Waarde |
|---|---|
| Board | ESP32C6 Dev Module |
| Partition Scheme | Custom → `partitions_16mb.csv` uit schetsmap |
| Flash Size | 16 MB |
| Upload (eerste keer) | USB |
| Upload (daarna) | OTA via Arduino IDE → Sketch → Upload via OTA |

---

## 2. Gemeenschappelijke regels — van toepassing op ALLE controllers

### 2.1 Verplichte sketch-header

Elke sketch begint met:
```cpp
// ⚠️ Verplicht voor ESP32-C6 (RISC-V) — vóór alle #include statements
#define Serial Serial0
```
**Positie is kritiek:** staat hij ná de `#include` statements → 100+ cascade-compilatiefouten.

De **versieheader** als blokcommentaar bevat datum, versienummer en beschrijving per wijziging. Let op: schrijf **`* /`** (met spatie) als je `*/` bedoelt in de tekst — anders breekt de blokcommentaar.

### 2.2 Heap — basisregels

De ESP32-C6 heeft 512 KB SRAM. Matter + WiFi reserveren ~130–140 KB. De 16 MB flash is **niet** beschikbaar als heap.

**Drempelwaarden:**

| Largest free block | Status | Actie |
|---|---|---|
| > 35 KB | 🟢 Comfortabel | Geen actie |
| 25–35 KB | 🟡 Werkbaar | Opvolgen bij volgende endpoint-toevoeging |
| < 25 KB | 🔴 Instabiel | STOP — evalueer endpoint-schrapping |

**Regels:**
- Alle webpagina's via **chunked streaming** (`AsyncResponseStream`) — nooit meer `html.reserve(N)` of grote String-opbouw
- `String` globals → **`char[]` + `strlcpy`** — elimineer permanente heap-fragmentatie
- `String(i)` voor NVS-keys → **`snprintf` naar `char buf[]`** op de stack
- **ArduinoJson v7**: `StaticJsonDocument<N>` alloceert ALTIJD op heap (niet stack zoals v6). Gebruik globale `JsonDocument` met `clear()` hergebruik.
- `http.getString()` → **`http.getStream()` + `DeserializationOption::Filter`** — elimineert ~1 KB String-allocatie per poll
- `buildJson()` / log-JSON → **pure `snprintf` naar static `char buf[]`** — nul heap-alloc

### 2.3 DS18B20 / OneWireNg

| Regel | Detail |
|---|---|
| `CONVERT_ALL` broadcast | Stuur conversie eenmalig naar alle sensoren (SKIP ROM 0xCC + 0x44). Daarna individueel uitlezen. |
| `delay(750)` na conversie | De enige geautoriseerde lange delay — WDT-safe via FreeRTOS `vTaskDelay` |
| Leesfrequentie ≥ 60s | Elke 2s lezen → 30× meer WDT-exposure. 60s is ruim voldoende. |
| Nooit per sensor in loop | Stapelt interrupt-blocking op → `Interrupt WDT timeout on CPU0` |

### 2.4 Matter — gemeenschappelijke regels

| Regel | Detail |
|---|---|
| `#define Serial Serial0` vóór `#include` | Verplicht, zie §2.1 |
| `Matter.begin()` returns `void` | Geen `if(Matter.begin())` — geeft compilatiefout |
| mDNS verwijderen | Matter start eigen interne mDNS-stack. `MDNS.begin()` → conflicten. Volledig verwijderen, toegang via statisch IP. |
| Max 12 endpoints | Praktische limiet op ESP32-C6 met volledige webUI |
| Updates via `loop()` | Centrale `update_matter()` functie, interval 5–10s via `millis()`. Nooit blocking `while(!commissioned)`. |
| `nvs_flash_erase()` niet vanuit async handler | Gebruik vlag: `matter_nuclear_reset_requested = true` → uitvoeren in `loop()` |
| Matter reset bij endpoint-wijziging | Type of volgorde wijzigen → pairing wissen vóór herpairing |
| `MatterOnOffLight` vs `MatterOnOffPlugin` | Lichten: `OnOffLight`. Logische schakelaars: `OnOffPlugin`. |

**Auto-recovery bij corrupt NVS** (toepassen bij Matter-initialisatie):
```cpp
Matter.begin();
delay(200);
if (!Matter.isDeviceCommissioned() && Matter.getManualPairingCode().length() < 5) {
  nvs_flash_erase();
  nvs_flash_init();
  ESP.restart();
}
```

### 2.5 AsyncWebServer handlers

- Alle handlers: **nooit blocking I/O** — sensordata lezen in `loop()`, niet in handler
- Grote pagina's: **chunked streaming** via `AsyncResponseStream`
- `/json` endpoint: **pure `snprintf`** naar `char buf[]`, direct naar response

### 2.6 IO-pinnen — onmiddellijke reactie

Elke IO-actie via de webUI of Matter moet **direct** de pin aansturen — nooit wachten op een pollcyclus. Patroon:
```cpp
server.on("/actie", HTTP_GET, [](AsyncWebServerRequest *request) {
  // 1. State bijwerken
  circuits[idx].override_active = true;
  // 2. Pin ONMIDDELLIJK schakelen
  mcp.digitalWrite(idx, LOW);
  // 3. Matter synchroon bijwerken
  ignore_callbacks = true;
  matter_circuit[idx].setOnOff(true);
  ignore_callbacks = false;
  request->send(200, "text/plain", "OK");
});
```

### 2.7 JSON key synchronisatie — kritiek leermoment

Wanneer een JSON-structuur in een controller hernoemd wordt (bijv. van lange namen naar compacte a/b/c-keys), **falen alle consumers (HVAC, Zarlar, Google Script) stil** — JSON-keys retourneren gewoon 0 als ze niet gevonden worden, zonder foutmelding.

**Regel:** bij elke JSON-structuurwijziging meteen alle consumers nalopen:
1. HVAC sketch (poll-code + filter-doc)
2. Zarlar Dashboard
3. Google Apps Script

### 2.8 Crash-logging (in alle drie sketches aanwezig)

```cpp
// setup(): vorige crash lezen
preferences.begin("crash-log", false);
// loop(): heap bewaken
if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 25000) {
  // opslaan in NVS
}
```
Crash-info tonen in `/settings`: laatste reden + teller + resetknop.

### 2.9 Serial commando's (alle drie sketches)

| Commando | Effect |
|---|---|
| `reset-matter` | Wist alleen Matter/HomeKit koppeling — instellingen blijven intact |
| `reset-all` | Wist alles: instellingen (NVS) + Matter-koppeling |
| `status` | Uitgebreid statusrapport in Serial Monitor |

### 2.10 NVS namespaces

| Namespace | Eigenaar | Mag aanraken? |
|---|---|---|
| `room-config` | ROOM sketch | ✅ |
| `hvac-config` | HVAC sketch | ✅ |
| `eco-config` | ECO sketch | ✅ |
| `crash-log` | Alle sketches | ✅ |
| `chip-factory` | Matter intern | ❌ Niet aanraken |
| `chip-config` | Matter intern | ❌ Niet aanraken |
| `chip-counters` | Matter intern | ❌ Niet aanraken |

---

## 3. HVAC Controller — specifiek

### 3.1 Hardware

| Component | Detail |
|---|---|
| Static IP | 192.168.0.70 |
| I/O expander | MCP23017 I2C — pins 0..6 relay circuits, 7 pump feedback, 8..9 distributiepompen, 10..12 TSTAT inputs (INPUT_PULLUP) |
| Temperatuursensoren | 6× DS18B20 op OneWire pin 3 (SCH boiler) |
| Ventilator | PWM op GPIO 20 via `ledcWrite` (0–255 → 0–100%) |

### 3.2 Heap-baseline (v1.18)

```
Runtime: ~20% free    Largest block: ~32 KB  ✅ acceptabel
```

### 3.3 Verwarmingslogica — beslissingsvolgorde per circuit

```
1. Override actief (< 10 min)?         → relay = override_state, stop
2. Circuit OFFLINE?                     → relay = TSTAT only
3. Iemand THUIS (home_status = 1)?      → relay = TSTAT OR HTTP
4. Niemand thuis                        → relay = HTTP only
```

### 3.4 Ventilatie

`effective_vent = max(vent_rooms, vent_override_active ? vent_override_percent : 0)`

Deze formule staat op **4 plaatsen** — altijd consistent houden:
1. `pollRooms()` → PWM-pin
2. `buildLogJson()` → JSON key `x`
3. `/set_vent` handler
4. `update_matter_sensors()`

Override vervalt automatisch na 3 uur. Bij vervallen: terugval naar `vent_percent` (rooms), niet naar 0.

### 3.5 TSTAT snelcheck

`checkTstatPins()` in `loop()` elke 100ms: leest MCP-pins 10/11/12, flankdetectie via `tstat_last_state[7]`, relay onmiddellijk bij wijziging. Override heeft voorrang.

### 3.6 Sliding window duty% (v1.16)

- 12 slots × 20 min = **4u rollend gemiddelde** per circuit
- `duty_4h` → naar JSON/Sheets (keys `i`..`o`)
- `duty_cycle` (cumulatief since boot) → live UI weergave
- `dc_last_poll` voor correcte delta-meting (niet cumulatief)
- `dc_slots_filled` voor correcte noemer bij lege ring

### 3.7 ECO pomplogica state machine (in HVAC)

| State | Duur | Overgang |
|---|---|---|
| `ECO_IDLE` | — | → `ECO_PUMP_WON` of `ECO_PUMP_SCH` bij `should_start` |
| `ECO_PUMP_SCH` | 1 min | → `ECO_WAIT_SCH` na timeout, of `IDLE` bij `should_stop` |
| `ECO_WAIT_SCH` | 1 min | → `ECO_PUMP_WON` als nog `should_start`, anders `IDLE` |
| `ECO_PUMP_WON` | 1 min | → `ECO_WAIT_WON` na timeout, of `IDLE` bij `should_stop` |
| `ECO_WAIT_WON` | 2 min | → `ECO_PUMP_SCH` als nog `should_start`, anders `IDLE` |

- **Start (OR):** `ETopH > eco_max_temp` (90°C) OF `EQtot > eco_threshold` (15 kWh)
- **Stop (OR):** `ETopH < eco_min_temp` (80°C) OF `EQtot < (eco_threshold − eco_hysteresis)`
- **Fair share:** `last_pump_was_sch` wisselt startvolgorde automatisch

⚠️ **TODO:** `kwh_transferred` is hardcoded `0.5` kWh per pompbeurt — echte berekening ontbreekt.

### 3.8 Matter endpoints (v1.18, 9 van max 12)

| # | Type | Variabele |
|---|---|---|
| EP1 | TemperatureSensor | SCH boiler top |
| EP2–7 | OnOffPlugin | circuits[0..5] |
| EP8 | OnOffPlugin | circuit[6] |
| EP9 | Fan | ventilatie (snelheid + aan/uit) |

### 3.9 HVAC /json output (keys a..ae → naar Zarlar → Google Sheets)

| Key | Sheet | Label | Eenheid |
|-----|-------|-------|---------|
| `a` | B | uptime_sec | s |
| `b` | C | ETopH_sch | °C |
| `c` | D | ETopL_sch | °C |
| `d` | E | EMidH_sch | °C |
| `e` | F | EMidL_sch | °C |
| `f` | G | EBotH_sch | °C |
| `g` | H | EBotL_sch | °C |
| `h` | I | EAv_sch | °C |
| `i`..`o` | J..P | BB/WP/BK/ZP/EP/KK/IK duty4h | % |
| `p`..`v` | Q..W | R1..R7 relay | 0/1 |
| `w` | X | HeatDem | kW |
| `x` | Y | Vent effectief | % |
| `y` | Z | SCH_pomp | 0/1 |
| `z` | AA | SCH_kWh ⚠️ TODO | kWh |
| `aa` | AB | WON_pomp | 0/1 |
| `ab` | AC | WON_kWh ⚠️ TODO | kWh |
| `ac` | AD | RSSI | dBm |
| `ad` | AE | FreeHeap% | % |
| `ae` | AF | LargestBlock | KB |

### 3.10 Wat HVAC leest van andere controllers

**Van Room (via HTTP poll):**

| JSON key | HVAC variabele | Betekenis |
|---|---|---|
| `b` | `heat_request` | Kamer vraagt verwarming (Heating_on) |
| `g` | `vent_request` | Gevraagd ventilatie% |
| `c` | `setpoint` | Ingesteld setpoint °C |
| `e` | `room_temp` | Kamertemperatuur °C (Temp1 DHT22) |
| `v` | `home_status` | 1 = iemand thuis |

**Van ECO (via HTTP poll):**

| JSON key | HVAC variabele | Betekenis |
|---|---|---|
| `b` | `eco_boiler.temp_top` | ETopH — bovenste sensor hoog °C |
| `g` | `eco_boiler.temp_bottom` | EBotL — onderste sensor laag °C |
| `h` | `eco_boiler.temp_avg` | EAv — gemiddelde °C |
| `i` | `eco_boiler.qtot` | EQtot — energieinhoud kWh |

### 3.11 Versiehistorie

| Datum | Versie | Wijziging |
|---|---|---|
| 17 mrt 26 | v1.18 | ECO JSON keys: ETopH→b, EBotL→g, EAv→h, EQtot→i |
| 17 mrt 26 | v1.17 | Room JSON keys: heat_request y→b, vent_request z→g, setpoint aa→c, room_temp h→e, home_status af→v |
| 13 mrt 26 | v1.16 | Sliding window duty% 4u per circuit |
| 13 mrt 26 | v1.15 | TSTAT snelcheck 100ms, ventilatie max()-logica, CSS fix, NVS snprintf |
| 13 mrt 26 | v1.14 | Circuit override: relay onmiddellijk via mcp.digitalWrite |
| 12 mrt 26 | v1.13 | ArduinoJson v7 fix, buildLogJson snprintf, getStream+filter |
| 12 mrt 26 | v1.12 | Matter 14→9 endpoints, /json compacte keys, chunked streaming |
| 12 mrt 26 | v1.9–11 | Heap-optimalisaties: char[], circuits[7], snprintf |

---

## 4. ECO Boiler Controller — specifiek

### 4.1 Hardware

| Component | Detail |
|---|---|
| Static IP | 192.168.0.71 |
| Temperatuursensoren | 6× DS18B20 (2 per laag: Top/Mid/Bot × H/L) |
| Zonnecollector | PT1000 temperatuursensor → `Tsun` |
| Circulatiepomp | PWM-aangestuurd op basis van `dT = Tsun − Tboil` |

### 4.2 ECO /json output (keys a..s → naar Zarlar → Google Sheets)

| Key | Sheet | Label | Eenheid |
|-----|-------|-------|---------|
| `a` | B | uptime_sec | s |
| `b` | C | ETopH | °C |
| `c` | D | ETopL | °C |
| `d` | E | EMidH | °C |
| `e` | F | EMidL | °C |
| `f` | G | EBotH | °C |
| `g` | H | EBotL | °C |
| `h` | I | EAv | °C |
| `i` | J | EQtot | kWh |
| `j` | K | dEQ (delta kWh) | kWh |
| `k` | L | yield_today | kWh |
| `l` | M | Tsun | °C |
| `m` | N | dT (Tsun−Tboil) | °C |
| `n` | O | pwm_value | 0–255 |
| `o` | P | pump_relay | 0/1 |
| `p` | Q | RSSI | dBm |
| `q` | R | FreeHeap% | % |
| `r` | S | MaxAllocHeap | KB |
| `s` | T | MinFreeHeap | KB |

### 4.3 Openstaande punten ECO

- **Heap-analyse**: baseline meten, ArduinoJson v7 check, `String(i)` NVS-keys → `snprintf`
- **kWh-berekening** (⚠️ TODO ook in HVAC): echte `Q = m × Cp × ΔT / 3600` berekening per pompbeurt implementeren in ECO sketch, resultaat doorgeven via `/json`
- **Reactietijden**: IO-pinnen direct aansturen vanuit webUI-handlers
- **Versieheader**: `* /` met spatie in commentaar

---

## 5. ROOM Controller — specifiek

### 5.1 Hardware

| Component | Detail |
|---|---|
| Static IP | 192.168.0.80 |
| Sensoren | DHT22 (temp+vocht), DS18B20, MH-Z19 (CO2), Sharp GP2Y (dust), TSL2561 (lux), LDR, PT1000 |
| Actuatoren | NeoPixel strip (tot 30 pixels), PIR×2, laserbeam LDR |
| Verwarming | TSTAT output + setpoint |

### 5.2 Heap-baseline (v2.10)

```
Setup:   23% free  (62936 bytes)   Largest block: 45 KB
Runtime: 20% free  (~74 KB)        Largest block: 31 KB  ✅
Crashdrempel: 25 KB — marge: 6 KB  ✅
```

Matter kost ~214 KB heap — niet te vermijden. Alle andere optimalisaties zijn gedaan.

### 5.3 Heap-optimalisaties doorgevoerd (v2.10)

| Maatregel | Winst |
|---|---|
| `pixel_nicknames[30]` String[] → `char[30][32]` BSS | ~1.5 KB heap |
| `ds_nicknames[4]`, `room_id`, `wifi_*`, `static_ip_str` → `char[]` | Permanente heap weg |
| `getFormattedDateTime()` String return → `const char*` static buf | Heap-alloc per call weg |
| Captive portal handlers → AP-only registratie | ~600 bytes handler-heap |
| `MOV_BUF_SIZE` 50 → 20 | 240 bytes BSS |
| N pixel-handlers → 2 universele handlers (`?idx=`) | ~600 bytes handler-heap |
| `mdns_name` en `hsvToRgb()` verwijderd | Flash + BSS |

### 5.4 Matter endpoints (v2.10, werkend)

| # | Type | Variabele | Opmerking |
|---|---|---|---|
| EP1 | MatterThermostat | `room_temp` + `heating_setpoint` | Setpoint instelbaar vanuit Apple Home |
| EP2 | MatterHumiditySensor | `humi` | |
| EP3 | MatterOccupancySensor | `mov1_light` | MOV1 PIR |
| EP4 | MatterOccupancySensor | `mov2_light` | Alleen als `mov2_enabled` |
| EP5 | MatterColorLight | `neo_r/g/b` | `HsvColor_t` API; on/off altijd `true` |
| EP6 | MatterOnOffLight | `pixel_mode[0]` | SW1: pixel 0 MOV-override |
| EP7 | MatterOnOffLight | `pixel_on[1]` | SW2: pixel 1 |
| EP8 | MatterOnOffLight | `pixel_on[2..N]` | SW3: pixels 2..pixels_num-1 samen |

### 5.5 Matter — ROOM-specifieke valkuilen

**"Aparte tegels" in Apple Home — hypothese endpoint-volgorde:**
In de werkende v2.1 (3 maart 2026) stond de thermostat op positie 7 ná 6 sensor-endpoints → Apple Home bood splitsen aan. In v2.10 staat de thermostat op EP1 → geen splitsen.

Te proberen volgende sessie:
```
EP1: MatterTemperatureSensor (los)
EP2: MatterHumiditySensor
EP3: MatterOccupancySensor MOV1
EP4: MatterOccupancySensor MOV2
EP5: MatterColorLight
EP6: MatterOnOffLight SW1
EP7: MatterOnOffLight SW2
EP8: MatterOnOffLight SW3
EP9: MatterThermostat  ← achteraan
```
Let op: Matter reset + herpairing vereist bij endpoint-volgorde wijziging.

**Andere ROOM Matter-valkuilen:**

| Valkuil | Oplossing |
|---|---|
| `MatterEnhancedColorLight` blokkeert aparte tegels | Gebruik `MatterColorLight` |
| `espHsvColor_t` vs `HsvColor_t` | `MatterColorLight` = `HsvColor_t` |
| Pixels buiten `pixels_num` bij boot | `updateLength(30)` + `clear()` + `show()` eerst, dan `updateLength(pixels_num)` |

### 5.6 ROOM /json output (keys a..ai → naar Zarlar → Google Sheets)

| Key | Sheet | Label | Eenheid |
|-----|-------|-------|---------|
| `a` | B | uptime_sec | s |
| `b` | D | Heating_on | 0/1 |
| `c` | E | Heating_setpoint | °C |
| `d` | F | TSTATon | 0/1 |
| `e` | G | Temp1 (DHT22) | °C |
| `f` | H | Temp2 (DS18B20) | °C |
| `g` | I | Vent_percent | % |
| `h` | J | Humi (DHT22) | % |
| `i` | K | Dew (dauwpunt) | °C |
| `j` | L | DewAlert | 0/1 |
| `k` | M | CO2 | ppm |
| `l` | N | Dust | — |
| `m` | O | Light LDR | 0–100 |
| `n` | P | SUNLight lux | lux |
| `o` | Q | Night (licht>50) | 0/1 |
| `p` | R | Bed switch | 0/1 |
| `q`..`s` | S..U | Neopixel R/G/B | 0–255 |
| `t` | V | Pixel_on_str | tekst |
| `u` | W | Pixel_mode_str | tekst |
| `v` | X | Home switch | 0/1 |
| `w`..`x` | Y..Z | MOV1/MOV2 /min | /min |
| `y`..`z` | AA..AB | MOV1/MOV2 light | 0/1 |
| `aa` | AC | BEAMvalue | 0–100 |
| `ab` | AD | BEAMalert | 0/1 |
| `ac` | AE | RSSI | dBm |
| `ad` | AF | FreeHeap% | % |
| `ae` | AG | LargestBlock | KB |
| `af` | AH | MinFreeHeap | KB |
| `ag` | AI | ds_count | — |
| `ah` | AJ | Tds2 | °C |
| `ai` | AK | Tds3 | °C |

### 5.7 Openstaande punten ROOM

**Prioriteit 1 — Aparte tegels:**
Thermostat naar EP9, losse `MatterTemperatureSensor` als EP1. Matter reset vereist. Referentie: `Oude_MATTER_ROOM_3mar.ino`.

**Prioriteit 2 — Gedeeld CSS endpoint:**
```cpp
server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
  AsyncWebServerResponse *r = request->beginResponse(200, "text/css", gedeelde_css);
  r->addHeader("Cache-Control", "public, max-age=86400");
  request->send(r);
});
```
Elke pagina vervangt `<style>...</style>` door `<link rel="stylesheet" href="/style.css">`. Geschatte winst: ~2–3 KB minder fragmentatie per request.

---

## 6. Schimmelbescherming via vochtigheid

Elke ROOM controller meet lokaal de luchtvochtigheid. Bij overschrijding van een drempelwaarde beslist de room controller autonoom om ventilatie te vragen via JSON key `g` (Vent_percent). De HVAC neemt de hoogste ventilatievraag van alle zones en stuurt de centrale ventilator aan. De volledige schimmelbeschermingslogica zit in de room controllers — de HVAC is enkel uitvoerder.

---

## 7. Bestanden

| Bestand | Beschrijving |
|---|---|
| `ESP32_C6_MATTER_HVAC_v1.18.ino` | HVAC productieversie — huidig |
| `HVAC_GoogleScript_v4.gs` | GAS HVAC — 31 kolommen A–AE |
| `ESP32_C6_MATTER_ECO_v1.22.ino` | ECO productieversie — huidig |
| `ECO_GoogleScript.gs` | GAS ECO — 20 kolommen A–T |
| `ESP32-C6_MATTER_ROOM_15mar_2200.ino` | ROOM v2.10 — Matter + heap stabiel |
| `ROOM_GoogleScript_v1_4.gs` | GAS ROOM — 37 kolommen A–AK |
| `Oude_MATTER_ROOM_3mar.ino` | Referentie: werkende Matter endpoint-volgorde (aparte tegels) |
| `partitions_16mb.csv` | Custom partitietabel voor alle controllers |
| `Zarlar_Master_Overnamedocument.md` | Dit document |

---

## 8. Instructies voor nieuwe sessie

1. **Upload** de actuele sketch als bijlage + dit document
2. **Vraag Claude** het document te lezen en samen te vatten vóór hij iets aanpast
3. **Heap-baseline** laten meten als eerste stap
4. **Versie per versie** werken met testmoment ertussen
5. **Herinner Claude** bij aanvang:
   - `* /` met spatie in commentaar (geen `*/` in tekst)
   - Versieheader aanpassen bij elke wijziging
   - Bij JSON-structuurwijziging: alle consumers nalopen (HVAC, Zarlar, Google Script)
   - IO-pins altijd onmiddellijk aansturen — nooit wachten op pollcyclus

---

*Zarlar project — Filip Delannoy — bijgewerkt 17 maart 2026*
