
# Zarlar Thuisautomatisering — Master Overnamedocument
**ESP32-C6 · Arduino IDE · Matter · Google Sheets · Node.js · Tailscale**
*Filip Delannoy — Zarlardinge (BE) — bijgewerkt 26 april 2026*

---

## 1. Systeemoverzicht

### 1.1 Architectuur en dataflow
Een volledig zelfgebouwd thuisautomatiseringssysteem op basis van ESP32-C6 controllers,
elk met een eigen webserver en Matter-integratie via WiFi. Het **Zarlar Dashboard (192.168.0.60)**
fungeert als centrale dataverzamelaar: het ontvangt JSON van alle controllers en POST de data
naar Google Sheets via Google Apps Script. Controllers doen nooit zelf HTTPS-calls naar Google.

De **Raspberry Pi (192.168.0.50)** vormt de rijke portaallaag — bereikbaar van overal via Tailscale.

```
[HVAC   192.168.0.70] ──┐
[ECO    192.168.0.71] ──┤
[SENRG  192.168.0.73] ──┼──→ Zarlar Dashboard 192.168.0.60 ──→ Google Sheets
[ROOM   192.168.0.80] ──┤         │
[Photons via Worker]  ──┘         └──→ Apple Home (via Matter/WiFi)
                                  
RPi 192.168.0.50 ←── pollt alle controllers ──→ Tailscale ──→ Overal
```

**Drie lagen:**
- **Laag 1 — ESP32 controllers:** sensoren, regelen, `/json` publiceren, Matter
- **Laag 2 — Dashboard ESP32:** pollt alles, matrix 16×16, Google Sheets logging
- **Laag 3 — RPi portal:** rijke UI, EPEX grafieken, matrix replica, overal bereikbaar

**Kritisch leermoment:** HTTPS POST vanuit de ESP32 zelf mislukte structureel door heap-druk.
Elke controller publiceert enkel zijn `/json` endpoint — het Dashboard doet de rest.

### 1.2 Controllers — huidige staat (26 april 2026)

| Controller | Naam | IP | MAC | Board | Versie | Status |
|---|---|---|---|---|---|---|
| **HVAC** | ESP32_HVAC | 192.168.0.70 | 58:8C:81:32:2B:90 | 32-pin clone | v1.19 | ✅ Productie stabiel |
| **ECO Boiler** | ESP32_ECO Boiler | 192.168.0.71 | 58:8C:81:32:2B:D4 | 32-pin clone | v1.23 | ✅ Productie stabiel |
| **Smart Energy** | ESP32_C6_ENERGY | 192.168.0.73 | — | 32-pin clone | v1.26 | ✅ Actief 26/04/2026 |
| **ROOM / Eetplaats** | ESP32_EETPLAATS | 192.168.0.80 | 58:8C:81:32:2F:48 | 32-pin clone | v2.21 | ✅ Matter + heap stabiel |
| **Zarlar Dashboard** | ESP32_ZARLAR | 192.168.0.60 | A8:42:E3:4B:FA:BC | 30-pin clone | v5.8 | ✅ Matter + Matrix 16×16 |
| **RPi Portal** | zarlar-rpi | 192.168.0.50 | — | Raspberry Pi | Node.js v2.0 | ✅ Actief (Tailscale) |

⚠️ **MAC-wissel HVAC:** experimenteerbord (MAC `58:8C:81:32:29:54`) is ook als HVAC gebruikt.
Productie-HVAC draait op `58:8C:81:32:2B:90`. Bij twijfel: check MAC in serial boot-output.

⚠️ **MAC-wissel HVAC:** experimenteerbord (MAC `58:8C:81:32:29:54`) is ook als HVAC gebruikt.
Productie-HVAC draait op `58:8C:81:32:2B:90`. Bij twijfel: check MAC in serial boot-output.

### 1.3 Particle Photon controllers (transitiefase)
Tijdens de migratie van Particle Photon naar ESP32 draaien de Photon-controllers nog in productie.
Het Dashboard pollt hun data via een **Cloudflare Worker** die de Particle Cloud API afschermt.

| Photon | Naam | Device ID (kort) | Status | Equivalent ESP32 |
|---|---|---|---|---|
| P-BandB | R1-BandB | 30002c... | ⚫ Offline | R-BandB (idx 6) |
| P-Badkamer | R2-BADK | 560042... | ✅ Online | R-BADK (idx 7) |
| P-Inkom | R3-INKOM | 420035... | ✅ Online | R-INKOM (idx 8) |
| P-Keuken | R4-KEUK | 310017... | ✅ Online | R-KEUKEN (idx 9) |
| P-Waspl | R5-WASPL | 33004f... | ✅ Online | R-WASPL (idx 10) |
| P-Eetpl | R6-EETPL | 210042... | ✅ Online | **R-EETPL ESP32 actief** (idx 11) |
| P-Zitpl | R7-ZITPL | 410038... | ✅ Online | R-ZITPL (idx 12) |

**Cloudflare Worker:** `https://controllers-diagnose.filip-delannoy.workers.dev`
⚠️ Particle token zit veilig in de Worker — niet in de browser en niet in de ESP32 sketch.

### 1.4 Geplande controllers
| # | Naam | IP | MAC | Functie |
|---|---|---|---|---|
| 3 | S-OUTSIDE | 192.168.0.72 | 58:8C:81:32:2F:B0 | Weerstation |
| 6 | R-BandB | 192.168.0.75 | 58:8C:81:32:28:8C | Room |
| 7 | R-BADK | 192.168.0.76 | 58:8C:81:32:2B:80 | Room |
| 8 | R-INKOM | 192.168.0.77 | 58:8C:81:32:29:3C | Room |
| 9 | R-KEUKEN | 192.168.0.78 | 58:8C:81:32:2B:AC | Room |
| 10 | R-WASPL | 192.168.0.79 | 58:8C:81:32:2F:9C | Room |
| 12 | R-ZITPL | 192.168.0.81 | 58:8C:81:32:2D:14 | Room |
| 13 | S-ACCESS | 192.168.0.82 | 58:8C:81:32:26:9C | Buitenverlichting/toegang |

---

## 2. Gemeenschappelijke hardware

### 2.1 ESP32-C6 module & boards
Alle Zarlar-controllers draaien op de **ESP32-C6-WROOM-1N16** (Espressif):
- 16 MB flash, WiFi 6 (2.4 GHz), BLE 5, Zigbee/Thread
- 3.3V IO — **niet 5V-tolerant**
- Ingebouwde PCB-antenne (standaard) of WROOM-1U met U.FL-connector voor externe antenne

**Dev boards in gebruik:**

| Type | Pins | Prijs | Aantal | Gebruik |
|---|---|---|---|---|
| 32-pin clone | 32 | €2.52/stuk (AliExpress dec 2025) | 10 stuks | **Productie** — HVAC, ECO, ROOM, SENRG |
| 30-pin clone | 30 | €9/stuk | 3 stuks | Dashboard + reserve |

⚠️ **Clone-boards:** clones, niet officieel Espressif. Werken identiek maar hebben soms
afwijkende pinlabels. Gebruik altijd de pinout uit de controllersecties van dit document.

### 2.2 Partitietabel (identiek voor ALLE controllers)
Bestand **`partitions_16mb.csv`** naast het `.ino` bestand plaatsen
(exacte naam vereist door Arduino IDE — anders werkt Custom partition scheme niet).

| Naam | Type | Offset | Grootte |
|---|---|---|---|
| nvs | data/nvs | 0x9000 | 20 KB |
| otadata | data/ota | 0xe000 | 8 KB |
| app0 | app/ota_0 | 0x10000 | 6 MB |
| app1 | app/ota_1 | 0x610000 | 6 MB |
| spiffs | data/spiffs | 0xC10000 | ~4 MB |

⚠️ **Nooit `huge_app`** — had maar één app-slot en brak OTA.
⚠️ **Nooit een 4MB controller** voor het Dashboard — bootloop (`partition size 0x600000 exceeds flash chip size 0x400000`).

### 2.3 Strapping pins — nooit als input
| Pin | Reden |
|---|---|
| IO8 | Strapping pin — LEEG LATEN |
| IO9 | Strapping pin — LEEG LATEN |
| IO0 | Boot pin — alleen als output of met sterke pull-up |
| IO15 | Alleen als output |

⚠️ **IO14 bestaat niet** op het 32-pin devboard — staat in de SoC datasheet maar niet uitgebroken.

### 2.4 Voeding
| Situatie | Voeding |
|---|---|
| Testopstelling | 5V via USB-C connector devboard |
| Productie (Zarlar shield) | 5V via VIN-pin, beveiligd met PTC-zekering (500 mA) |

De module heeft een ingebouwde 3.3V LDO.

### 2.5 Shield — connectoroverzicht
| Connector | Pins | Voeding | ROOM | HVAC | ECO | Dashboard | Opmerking |
|---|---|---|---|---|---|---|---|
| **Roomsense** (RJ45) | 8 | 5V + 3V3 + GND | ✅ | — | — | — | DHT22, PIR, Sharp dust, LDR1 |
| **OPTION** (RJ45) | 8 | 5V + 3V3 + GND | ✅ | — | — | — | PIR2, CO2 PWM, TSTAT, LDR2 |
| **T-BUS** (3-pin) | 3 | 3V3 + GND | ✅ | ✅ | ✅ | — | DS18B20 OneWire |
| **Pixel-line** (3-pin) | 3 | 5V + GND | ✅ | — | — | ✅ | NeoPixel data + 5V voeding |
| **I2C** (4-pin) | 4 | 3V3 + GND | ✅ | ✅ | — | — | SDA + SCL, 4.7k pull-ups naar 3.3V |
| **SPI** (6-pin) | 6 | 3V3 + GND | — | — | ✅ | — | MAX31865 PT1000 |
| **Relay OUT** (2-pin) | 2 | — | — | — | ✅ | — | IO1: pomprelais ECO |
| **PWM OUT** (2-pin) | 2 | — | — | ✅ | ✅ | — | HVAC: ventilator / ECO: pomp |

> **Smart Energy** gebruikt de **Roomsense** of **Option** RJ45-bus op het shield voor de
> verbinding naar de S0-interface PCB (zie §6.2).

### 2.6 Arduino IDE instellingen
| Instelling | Waarde |
|---|---|
| Board | ESP32C6 Dev Module |
| Flash Size | **16 MB** |
| Partition Scheme | Custom → `partitions_16mb.csv` uit schetsmap |
| USB CDC On Boot | **Enabled** (verplicht voor Serial over USB-C) |
| Upload (eerste keer) | USB |
| Upload (daarna) | OTA via Arduino IDE → Sketch → Upload via OTA |

### 2.7 Pinout-snel-referentie alle controllers
Gedetailleerde pinout per controller: zie §4.1 (HVAC), §5.1 (ECO), §6.1 (SENRG), §7.1 (ROOM), §8.1 (Dashboard).

| Pin | HVAC | ECO | SENRG | ROOM | Dashboard |
|---|---|---|---|---|---|
| IO1 | — | Pomprelais | — | LDR1 analog | — |
| IO3 | DS18B20 | DS18B20 | — | DS18B20 | — |
| IO4 | — | — | **LED matrix DIN** | NeoPixels | NeoPixels matrix |
| IO5 | — | PWM pomp | **S0 Solar** | PIR MOV1 | — |
| IO6 | — | — | **S0 SCH afname** | DHT22 | — |
| IO7 | — | — | **S0 SCH injectie** | — | — |
| IO10 | — | — | — | TSTAT | — |
| IO11 | I2C SCL | — | — | I2C SCL | — |
| IO13 | I2C SDA | — | — | I2C SDA | — |
| IO20 | PWM vent. | SPI CS | — | — | — |

> ⚠️ **SENRG pinnen bevestigd 26/04/2026:** LED matrix op **IO4** (Pixel-line connector),
> S0-kanalen op IO5/IO6/IO7 (Roomsense/Option RJ45). IO10 is vrij op SENRG.

---

## 3. Gemeenschappelijke softwareregels
Van toepassing op **alle** Zarlar-controllers tenzij expliciet anders aangegeven.

### 3.1 Verplichte sketch-header
```cpp
// ⚠️ Verplicht voor ESP32-C6 (RISC-V) — vóór alle #include statements
#define Serial Serial0
```

⚠️ **Positie is kritiek:** ná de `#include` → 100+ cascade-compilatiefouten.
⚠️ **Niet aanwezig zonder Matter** → stuurt output naar UART0 (fysieke pins), Serial monitor leeg.

De **versieheader** als blokcommentaar: schrijf **`* /`** (met spatie) als je `*/` bedoelt in tekst.

### 3.2 Heap — basisregels
ESP32-C6 heeft 512 KB SRAM. Matter + WiFi reserveren ~130–140 KB.

| Largest free block | Status | Actie |
|---|---|---|
| > 35 KB | 🟢 Comfortabel | Geen actie |
| 25–35 KB | 🟡 Werkbaar | Opvolgen |
| < 25 KB | 🔴 Instabiel | STOP — evalueer |

**Regels:**
- Alle webpagina's via **chunked streaming** (`AsyncResponseStream`) — nooit `html.reserve(N)`
- `String` globals → **`char[]` + `strlcpy`**
- `String(i)` voor NVS-keys → **`snprintf` naar `char buf[]`**
- ArduinoJson v7: `StaticJsonDocument<N>` alloceert ALTIJD op heap. Gebruik globale `JsonDocument` met `clear()`.
- `http.getString()` → **`http.getStream()` + `DeserializationOption::Filter`**

`WebServer` vs `AsyncWebServer`: Dashboard gebruikt `WebServer` (blocking) — bewust.
Minder heap (~10KB verschil), minder complex, bewezen stabiel voor Dashboard-gebruik.

### 3.3 DS18B20 / OneWireNg
| Regel | Detail |
|---|---|
| `CONVERT_ALL` broadcast | SKIP ROM 0xCC + 0x44 eenmalig, daarna individueel uitlezen |
| `delay(750)` na conversie | Enige geautoriseerde lange delay — WDT-safe via `vTaskDelay` |
| Leesfrequentie ≥ 60s | Elke 2s lezen → 30× meer WDT-exposure. 60s is ruim voldoende. |
| Nooit per sensor in loop | Stapelt interrupt-blocking → `Interrupt WDT timeout on CPU0` |

### 3.4 Matter — gemeenschappelijke regels
| Regel | Detail |
|---|---|
| `#define Serial Serial0` vóór `#include` | Verplicht, zie §3.1 |
| `Matter.begin()` returns `void` | Geen `if(Matter.begin())` — compilatiefout |
| mDNS verwijderen | Matter start eigen mDNS. `MDNS.begin()` → conflicten. Volledig verwijderen. |
| Max 12 endpoints | Praktische limiet op ESP32-C6 met volledige webUI |
| Updates via `loop()` | Centrale `update_matter()`, interval 5–10s via `millis()` |
| `nvs_flash_erase()` niet in async handler | Gebruik vlag → uitvoeren in `loop()` |
| Matter reset bij endpoint-wijziging | Type of volgorde wijzigen → pairing wissen vóór herpairing |
| Pairing code niet alleen via Serial | Toon altijd ook in webUI `/settings` |

**Auto-recovery bij corrupt NVS:**
```cpp
Matter.begin();
delay(200);
if (!Matter.isDeviceCommissioned() && Matter.getManualPairingCode().length() < 5) {
  nvs_flash_erase(); nvs_flash_init(); ESP.restart();
}
```

### 3.5 AsyncWebServer handlers
- Nooit blocking I/O in handlers — sensordata lezen in `loop()`, niet in handler
- Grote pagina's: chunked streaming via `AsyncResponseStream`
- `/json` endpoint: pure `snprintf` naar `char buf[]`, direct naar response

### 3.6 IO-pinnen — onmiddellijke reactie
Elke IO-actie via webUI of Matter moet **direct** de pin aansturen:
```cpp
server.on("/actie", HTTP_GET, [](AsyncWebServerRequest *request) {
  circuits[idx].override_active = true;
  mcp.digitalWrite(idx, LOW);             // 1. Pin direct aansturen
  ignore_callbacks = true;
  matter_circuit[idx].setOnOff(true);     // 2. Matter synchroon bijwerken
  ignore_callbacks = false;
  request->send(200, "text/plain", "OK");
});
```
**Bewuste code-duplicatie** op drie plaatsen (webUI / Matter callback / override cancel) is OK — niet samenvoegen tenzij vierde pad bijkomt.

### 3.7 NVS — namespaces en crashlog
| Namespace | Eigenaar | Mag aanraken? |
|---|---|---|
| `zarlar` | Dashboard | ✅ |
| `room-config` | ROOM | ✅ |
| `hvac-config` | HVAC | ✅ |
| `eco-config` | ECO | ✅ |
| `senrg-config` | Smart Energy | ✅ |
| `crash-log` | Alle sketches | ✅ |
| `chip-factory` / `chip-config` / `chip-counters` | Matter intern | ❌ Niet aanraken |

**Crashlog feedback loop (geleerd 12 april 2026, opgelost v2.20):**
```cpp
static bool crash_logged_this_episode = false;
if (lb < 25000 && !crash_logged_this_episode) {
    crash_logged_this_episode = true;
    // crashPrefs.begin() etc. — slechts 1× per episode
} else if (lb >= 25000) {
    crash_logged_this_episode = false;
}
```

### 3.8 Serial commando's (alle sketches)
| Commando | Effect |
|---|---|
| `reset-matter` | Wist alleen Matter-koppeling — instellingen blijven |
| `reset-all` | Wist alles: NVS + Matter-koppeling |
| `status` | Uitgebreid statusrapport in Serial Monitor |

### 3.9 Serial monitor — bekende valkuilen ESP32-C6
- **Serial leeg na boot:** `USB CDC On Boot` staat op Disabled → zetten op Enabled
- **Serial leeg na boot (2):** `#define Serial Serial0` aanwezig zonder Matter → verwijderen
- **Serial mist boot-berichten:** sketch print te snel → `delay(3000)` na `Serial.begin(115200)`

### 3.10 JSON key synchronisatie
Bij hernoemen van JSON-keys **falen alle consumers stil** (Google Sheets, Dashboard, HVAC).
Bij elke JSON-structuurwijziging meteen nalopen:
1. Consumerende sketches (HVAC poll-code + filter-doc)
2. Zarlar Dashboard matrix-rendering
3. Google Apps Script kolommen

**Geen nieuwe JSON-keys toevoegen** tenzij expliciete toestemming — bestaande consumers breken stil.

### 3.11 WebUI JavaScript — lessen
- **Nooit volledige JS-block in één `str_replace`** — gebruik altijd kleine, chirurgische ingrepen
- **`DOMContentLoaded`** betrouwbaarder dan `window.addEventListener('load')` voor inline scripts
- **Klok onafhankelijk van JSON-fetch** — aparte `updateClock()` met eigen `setInterval`
- **Slider niet overschrijven** terwijl gebruiker hem versleept — check `document.activeElement`
- **PIR triggers direct herberekenen** na `pushEvent()` via `countRecent()` in loop
- **`RSSI !== 0`** als conditie — niet `c.r` (is truthy als negatief)
- **iOS Safari:** font-size min. 11px, `-webkit-text-size-adjust:none` op body

### 3.12 WiFi scan — lessen
- `WiFi.channel(k)` kan `0` teruggeven → geen channel-filter op nul
- ESP32-C6 is **2.4GHz-only** — geen channel-filter nodig
- TCP-ping: gebruik **port 53 (DNS)** — altijd open, niet 80

### 3.13 Statusmatrix — lessen
- **WS2812B 5V voeding apart** — bij volle helderheid matrix >3A. Nooit via shield PTC (max 500 mA).
- **Serpentine adressering** — `matPxIdx()` converteert logisch naar fysiek adres
- **MROW-volgorde** moet exact overeenkomen met SVG-labelsheet — verificeer via `status` commando
- **Largest free block** is de echte heap-metric, niet total free heap
- **`sed -i` op `*/`** corrupteert versieheader-blokcommentaar → compileerfout. Nooit globaal vervangen.

---

## 4. HVAC Controller (192.168.0.70)

### 4.1 Hardware & pinout
| Component | Detail |
|---|---|
| Board | ESP32-C6 32-pin clone, MAC `58:8C:81:32:2B:90` |
| Static IP | 192.168.0.70 |
| I/O expander | MCP23017 op I2C (SDA=IO13, SCL=IO11) |
| Temperatuursensoren | 6× DS18B20 op OneWire (IO3) — SCH boiler |
| Ventilator | PWM op IO20 via `ledcWrite` (0–255 → 0–100%) |

| ESP32-C6 Pin | `#define` | Functie |
|---|---|---|
| IO3 | `ONE_WIRE_PIN 3` | DS18B20 OneWire — 6× SCH boiler |
| IO11 | `I2C_SCL 11` | I2C SCL → MCP23017 |
| IO13 | `I2C_SDA 13` | I2C SDA → MCP23017 |
| IO20 | `VENT_FAN_PIN 20` | PWM ventilator (1 kHz, 8-bit) |

**MCP23017 poortindeling:**

| MCP pin | Richting | Functie |
|---|---|---|
| 0–6 | OUTPUT | Relay circuits 1–7 (actief-laag) |
| 7 | INPUT_PULLUP | Pomp feedback |
| 8 | OUTPUT | Distributiepomp SCH |
| 9 | OUTPUT | Distributiepomp WON |
| 10–12 | INPUT_PULLUP | TSTAT inputs circuits |
| 13–15 | INPUT_PULLUP | Reserve TSTAT-slots |

### 4.2 Libraries
| Library | Gebruik |
|---|---|
| `OneWireNg_CurrentPlatform` | DS18B20 — C6-compatibel |
| `Adafruit_MCP23X17` | I/O expander relais + TSTAT |
| `AsyncTCP` + `ESPAsyncWebServer` | Webserver |
| `ArduinoJson` | JSON polling room controllers + ECO |
| `Preferences` | NVS opslag |

### 4.3 /json endpoint
| Key | Sheet | Label | Eenheid |
|---|---|---|---|
| `a` | B | uptime_sec | s |
| `b`–`g` | C–H | KST1–KST6 (boilertemperaturen) | °C |
| `h` | I | KSAv (boilergemiddelde) | °C |
| `i`–`o` | J–P | duty_4h C1–C7 | int |
| `p`–`v` | Q–W | heating_on C1–C7 | 0/1 |
| `w` | X | total_power | kW |
| `x` | Y | vent_percent | % |
| `y` | Z | sch_on (pomp SCH) | 0/1 |
| `z` | AA | last_sch_pump.kwh_pumped | kWh |
| `aa` | AB | won_on (pomp WON) | 0/1 |
| `ab` | AC | last_won_pump.kwh_pumped | kWh |
| `ac` | AD | RSSI | dBm |
| `ad` | AE | FreeHeap% | % |
| `ae` | AF | LargestBlock | KB |

Circuitnamen instelbaar via `/settings`. Default: C1–C7. Productie: BB, WP, BK, ZP, EP, KK, IK.

### 4.4 Matter endpoints
| # | Type | Variabele | Opmerking |
|---|---|---|---|
| EP1 | MatterTemperatureSensor | `sch_temps[0]` | Boiler top |
| EP2–EP8 | MatterOnOffPlugin | `circuits[0..6]` | Kringen 1–7 |
| EP9 | MatterFan | `vent_percent` | Ventilatie % |

### 4.5 Heap-baseline
```
Setup:   free=~180 KB  largest=~55 KB
Runtime: largest_block stabiel >35 KB  ✅
```

### 4.6 Versiehistorie
| Versie | Wijziging |
|---|---|
| v1.19 | Matter `onChangeOnOff`: `mcp.digitalWrite()` onmiddellijk — relais reageren direct vanuit Apple Home |
| v1.18 | ECO JSON keys hernoemd: ETopH→b, EBotL→g, EAv→h, EQtot→i |

### 4.7 Openstaande punten
- **kWh-berekening**: echte `Q = m × Cp × ΔT / 3600` per pompbeurt implementeren
- **HTML compressie**: witte pagina op iPhone bij ventilatieslider → heap-krapte bij page reload

---

## 5. ECO Boiler Controller (192.168.0.71)

### 5.1 Hardware & pinout
| Component | Detail |
|---|---|
| Board | ESP32-C6 32-pin clone, MAC `58:8C:81:32:2B:D4` |
| Static IP | 192.168.0.71 |
| Temperatuursensoren | 6× DS18B20 op OneWire (IO3) — 2 per boilerlaag: Top/Mid/Bot × H/L |
| Zonnecollector | PT1000 via MAX31865 SPI (CS=IO20, MOSI=IO21, MISO=IO22, SCK=IO23) |
| Pomprelais | IO1 — digitaal aan/uit (actief-laag) |
| Circulatiepomp | PWM op IO5 (0–255), freq 1 kHz, 8-bit |

| ESP32-C6 Pin | `#define` | Functie |
|---|---|---|
| IO1 | `RELAY_PIN 1` | Pomprelais (actief-laag) |
| IO3 | `ONEWIRE_PIN 3` | DS18B20 OneWire (6× boiler) |
| IO5 | `PWM_PIN 5` | PWM circulatiepomp (0–255) |
| IO20 | `SPI_CS 20` | SPI CS → MAX31865 |
| IO21 | `SPI_MOSI 21` | SPI MOSI |
| IO22 | `SPI_MISO 22` | SPI MISO |
| IO23 | `SPI_SCK 23` | SPI SCK |

### 5.2 Libraries
| Library | Gebruik |
|---|---|
| `OneWireNg_CurrentPlatform` | DS18B20 — C6-compatibel |
| `Adafruit_MAX31865` | PT1000 via SPI |
| `AsyncTCP` + `ESPAsyncWebServer` | Webserver |
| `Preferences` | NVS opslag |

### 5.3 /json endpoint
⚠️ **ECO gebruikt key `p` voor RSSI** — alle andere controllers gebruiken `ac`. Kritiek voor Dashboard!

| Key | Sheet | Label | Eenheid |
|---|---|---|---|
| `a` | B | uptime_sec | s |
| `b`–`g` | C–H | ETopH / ETopL / EMidH / EMidL / EBotH / EBotL | °C |
| `h` | I | EAv (boilergemiddelde) | °C |
| `i` | J | EQtot (energie-inhoud) | kWh |
| `j` | K | dEQ (delta kWh) | kWh |
| `k` | L | yield_today | kWh |
| `l` | M | Tsun (collector) | °C |
| `m` | N | dT (Tsun−Tboiler) | °C |
| `n` | O | pwm_value | 0–255 |
| `o` | P | pump_relay | 0/1 |
| `p` | Q | **RSSI** ⚠️ | dBm |
| `q` | R | FreeHeap% | % |
| `r` | S | MaxAllocHeap | KB |
| `s` | T | MinFreeHeap | KB |

### 5.4 Matter endpoints
Matter is **niet geïntegreerd** in de ECO-boiler sketch.

### 5.5 Heap-baseline
Nog te meten — zie openstaande punten §5.7.

### 5.6 Versiehistorie
| Versie | Wijziging |
|---|---|
| v1.23 | Huidige productieversie — stabiel |

### 5.7 Openstaande punten
- **Heap-analyse**: baseline meten, ArduinoJson v7 check, `String(i)` NVS-keys → `snprintf`
- **kWh-berekening**: echte `Q = m × Cp × ΔT / 3600` per pompbeurt
- **Reactietijden**: IO-pinnen direct aansturen vanuit webUI-handlers
- **Versieheader**: `* /` met spatie in commentaar

---

## 6. Smart Energy Controller (192.168.0.73)
> **Status: in ontwikkeling — sketch v0.0 nog te schrijven.**
> Volledig technisch detail: zie **`Energy_Management_System_v1_5.md`**

---

### 6.1 Hardware & pinout
| Component | Detail |
|---|---|
| Board | ESP32-C6 32-pin clone |
| Static IP | 192.168.0.73 |
| Locatie | Kastje inkomhal Maarten, naast Telenet router |
| Voeding | 5V via shield of USB-C |
| LED-strip | 12× WS2812B op IO10, 5V voeding apart (niet via shield PTC) |
| S0-interface | Via RJ45 naar interface PCB (zie §6.2) |

| ESP32-C6 Pin | Signaal | Functie |
|---|---|---|
| IO3 | S0 Solar | S0-puls interrupt FALLING |
| IO4 | LED matrix (48 pixels)
| IO5 | S0 Solar | S0-puls interrupt FALLING |
| IO6 | S0 SCH afname | S0-puls interrupt FALLING |
| IO7 | S0 SCH injectie
| IO10 | LED-strip data | WS2812B DIN (via 330Ω serie-weerstand) |

### 6.2 Interface PCB — S0 aansluiting
**S0-uitgang = passief (spanningloos) contact** (IEC 62053-31) → directe verbinding zonder optocoupler.

**Schema per S0-kanaal (4× identiek, 40×40mm SMD PCB):**
```
  3.3V
    |
  [R 10kΩ 0805]  ← pull-up
    |
    +──────────────────── S0+ klem (teller)
    |                     S0- klem (teller) ──── GND
    |
  [C 10nF 0805]  ← HF-filter
    |
   GND      → middenknoop → GPIO (INPUT, geen interne pull-up nodig)
```

**RJ45 pinout naar ESP32 shield (T568B):**

| RJ45 pin | Kleur | Signaal | GPIO |
|---|---|---|---|
| 1 | Oranje-wit | GND | GND |
| 2 | Oranje | 3.3V | 3.3V |
| 3 | Groen-wit S0 Solar (IO5)
Pin 4 (Blauw)      → S0 SCH afname (IO6)
Pin 5 (Blauw-wit)  → S0 SCH injectie (IO7)
| 6 | Groen | S0 reserve | IO6 |

### 6.3 Libraries
| Library | Gebruik |
|---|---|
| `AsyncTCP` + `ESPAsyncWebServer` | Webserver (conform andere Zarlar-controllers) |
| `Adafruit NeoPixel` | WS2812B LED-strip (12 pixels) |
| `Preferences` | NVS opslag (dagcumulatieven, EPEX-cache, instellingen) |
| `HTTPClient` | EPEX ophalen (energy-charts.info) + ntfy.sh push |
| `ArduinoJson` | EPEX JSON parsing |

### 6.4 /json endpoint
Dit is de **enige referentie** voor de Smart Energy JSON-keys.
Bij wijziging: Dashboard matrix-rij 2 en GAS-script S-ENERGY nalopen.

| Key | Label | Eenheid | Opmerking |
|---|---|---|---|
| `a` | Solar vermogen | W | Real-time |
| `b` | Verbruik WON | W | Real-time |
| `c` | Verbruik SCH | W | Real-time |
| `d` | Overschot (a−b−c) | W | Positief = injectie |
| `h` | Solar dag | Wh | Dagcumulatief |
| `i` | WON dag bruto | Wh | Dagcumulatief |
| `j` | SCH dag bruto | Wh | Dagcumulatief |
| `v` | Injectie dag | Wh | Dagcumulatief |
| `q` | Kost WON dag — dynamisch | EUR×100 | |
| `qv` | Kost WON dag — vast | EUR×100 | |
| `r` | Kost SCH dag — dynamisch | EUR×100 | |
| `rv` | Kost SCH dag — vast | EUR×100 | |
| `s` | Solar opbrengst dag — dynamisch | EUR×100 | |
| `sv` | Solar opbrengst dag — vast | EUR×100 | |
| `n` | EPEX prijs huidig kwartier | EUR/kWh×1000 | |
| `n2` | EPEX prijs volgend kwartier | EUR/kWh×1000 | |
| `nv` | Vast tarief geconfigureerd | EUR/kWh×1000 | |
| `pt` | Piek gecombineerde afname maand | W | Basis Fluvius-tarief |
| `pw` | Piek WON individueel maand | W | Gedragsanalyse |
| `ps` | Piek SCH individueel maand | W | Gedragsanalyse |
| `e` | ECO-boiler aan/uit | 0/1 | |
| `f` | Tesla laden aan/uit | 0/1 | |
| `g` | Override actief | 0/1 | |
| `o` | LED-strip helderheid | 0–100 | Instelbaar via /settings |
| `eod` | End-of-day vlag | 0/1 | Midnight trigger GAS |
| `ac` | RSSI | dBm | Conform andere controllers |
| `ae` | Heap largest block | bytes | |

### 6.5 LED-strip (12 pixels WS2812B)
| # | Sym | Groep | Kleurlogica |
|---|---|---|---|
| 1 | ☀️ Solar | Energie | Uit→geel dim→groen helder |
| 2 | 💰 Prijs | Energie | Lime=negatief / groen=goedkoop / geel=normaal / rood=duur |
| 3 | ⚖️ Netto | Energie | Groen=injectie / rood=afname |
| 4 | 🔋 Batterij | Batterij | SOC kleurschaal (toekomstig) |
| 5 | ♨️ ECO | Groot | Groen=aan / zwart=uit |
| 6 | 🚙 EV WON | Groot | Groen gradient op laadvermogen |
| 7 | 🚗 EV SCH | Groot | Idem |
| 8 | 🏠 WP WON | Groot | Groen=aan / zwart=uit |
| 9 | 🏚️ WP SCH | Groot | Groen=aan / zwart=uit |
| 10 | 🍳 Koken? | Advies | Groen=goed moment / rood=duur of piek vol |
| 11 | 👕 Wassen? | Advies | Zelfde logica |
| 12 | 📊 Piek | Piek | Groen→geel→oranje→rood vs MAX_PIEK |

Pixels 10–11 (🍳👕) zijn speciaal voor Céline en Mireille — groen = goed moment, rood = wacht.
Testpagina: https://fideldworp.github.io/ZarlarApp/epex-grafiek.html

### 6.6 Matter endpoints
Matter is **niet actief in fase 1** (heap-overhead). Optioneel later toe te voegen.

### 6.7 Heap-baseline
Nog te meten bij eerste werkende sketch (v0.1).

### 6.8 Versiehistorie
| Versie | Datum | Inhoud |
|---|---|---|
| v0.0 | — | Nog te bouwen — zie fasering in EMS §16.10 |

### 6.9 Openstaande actiepunten

| Actie | Wie | Status |
|---|---|---|
ntfy.sh app installeren | Filip + Maarten | Open |
EV-lader 2 merk/type opzoeken | Maarten | Open |
UTP kabel trekken verdeelkast → inkomhal | Filip + Maarten | Open |
CZ-TAW1 WP WON reset + herregistratie | Filip | Open |
NIEUW: sketch v1.27 getest 28 april 2026

---

## 7. ROOM Controller (192.168.0.80)

### 7.1 Hardware & pinout
| Component | Detail |
|---|---|
| Board | ESP32-C6 32-pin clone (MAC `58:8C:81:32:2F:48` productie / `58:8C:81:32:29:54` experimenteerbord) |
| Static IP | 192.168.0.80 |
| Sensoren | DHT22, DS18B20, MH-Z19 CO2, Sharp GP2Y dust, TSL2561 lux, LDR |
| PIR sensoren | **AM312** (3.3V natively, push-pull active HIGH — vervangt HC-SR501) |
| Actuatoren | NeoPixel strip (tot 30 pixels) |

| ESP32-C6 Pin | `#define` | Functie |
|---|---|---|
| IO1 | `LDR_ANALOG 1` | LDR1 analog (⚠️ 10k pull-up IO1→3V3 op shield!) |
| IO2 | `OPTION_LDR 2` | LDR2 analog (beam) |
| IO3 | `ONE_WIRE_PIN 3` | DS18B20 OneWire |
| IO4 | `NEOPIXEL_PIN 4` | NeoPixels data |
| IO5 | `PIR_MOV1 5` | MOV1 PIR (AM312) |
| IO6 | `DHT_PIN 6` | DHT22 data |
| IO7 | `SHARP_ANALOG 7` | Sharp dust analog |
| IO10 | `TSTAT_PIN 10` | TSTAT switch (GND = AAN) |
| IO11 | — | I2C SCL → TSL2561 |
| IO12 | `SHARP_LED 12` | Sharp dust LED |
| IO13 | — | I2C SDA → TSL2561 |
| IO18 | `CO2_PWM 18` | CO2 PWM input (MH-Z19 — ⚠️ 5V voeding!) |
| IO19 | `PIR_MOV2 19` | MOV2 PIR (AM312) |

⚠️ **AM312:** active HIGH, push-pull. `== HIGH`, geen `INPUT_PULLUP` nodig.
⚠️ **MH-Z19:** heeft aparte 5V voeding — PWM-signaal zelf is 3.3V-compatibel.

### 7.2 Libraries
| Library | Gebruik |
|---|---|
| `DHT` | DHT22 |
| `OneWireNg_CurrentPlatform` | DS18B20 |
| `Adafruit_TSL2561_U` | Luxmeter I2C |
| `Adafruit_NeoPixel` | NeoPixel strip |
| `AsyncTCP` + `ESPAsyncWebServer` | Webserver |
| `Preferences` | NVS opslag |

### 7.3 /json endpoint
| Key | Sheet | Label | Eenheid |
|---|---|---|---|
| `a` | B | uptime_sec | s |
| `b` | D | Heating_on | 0/1 |
| `c` | E | Heating_setpoint | °C |
| `d` | F | TSTATon | 0/1 |
| `e` | G | Temp1 DHT22 | °C |
| `f` | H | Temp2 DS18B20 | °C |
| `g` | I | Vent_percent | % |
| `h` | J | Humi DHT22 | % |
| `i` | K | Dew (dauwpunt) | °C |
| `j` | L | DewAlert | 0/1 |
| `k` | M | CO2 | ppm |
| `l` | N | Dust | — |
| `m` | O | Light LDR | 0–100 |
| `n` | P | SUNLight lux | lux |
| `o` | Q | Night | 0/1 |
| `p` | R | Bed switch | 0/1 |
| `q`–`s` | S–U | NeoPixel R/G/B | 0–255 |
| `t` | V | Pixel_on_str | tekst |
| `u` | W | Pixel_mode_str | tekst |
| `v` | X | Home switch | 0/1 |
| `w`–`x` | Y–Z | MOV1/MOV2 triggers/min | /min |
| `y`–`z` | AA–AB | MOV1/MOV2 lamp aan | 0/1 |
| `aa` | AC | BEAMvalue | 0–100 |
| `ab` | AD | BEAMalert | 0/1 |
| `ac` | AE | RSSI | dBm |
| `ad` | AF | FreeHeap% | % |
| `ae` | AG | LargestBlock | KB |
| `af` | AH | MinFreeHeap | KB |
| `ag` | AI | ds_count | — |
| `ah` | AJ | Tds2 | °C |
| `ai` | AK | Tds3 | °C |

### 7.4 Matter endpoints
| # | Type | Variabele | Opmerking |
|---|---|---|---|
| EP1 | MatterThermostat | `room_temp` + `heating_setpoint` | UIT=Weg / HEAT=Thuis |
| EP2 | MatterHumiditySensor | `humi` | |
| EP3 | MatterOccupancySensor | `mov1_light` | MOV1 PIR |
| EP4 | MatterOccupancySensor | `mov2_light` | Als `mov2_enabled` |
| EP5 | MatterColorLight | `neo_r/g/b` | `HsvColor_t` API |
| EP6 | MatterOnOffLight | `pixel_mode[0]` | SW1: MOV-override |
| EP7 | MatterOnOffLight | `pixel_on[1]` | SW2: pixel 1 |
| EP8 | MatterOnOffLight | `pixel_on[2..N]` | SW3: pixels 2..N samen |

**Apple Home koppeling:** Thermostat UIT = `home_mode=0` (Weg), HEAT = `home_mode=1` (Thuis).
NVS-persistent. Volledig synchroon met "Thuis" toggle in webUI.

### 7.5 Heap-baseline
```
Setup:   23% free (62.936 bytes)  largest=~45 KB
Runtime: 20% free (~74 KB)        largest=~31 KB  ✅
```
Matter kost ~214 KB heap. Crashdrempel 25 KB — marge 6 KB.

### 7.6 Specifieke features (v2.21)
**Verwarmingslogica:**
```
Thuis (home_mode=1) + TSTAT → volg hardware thermostaat pin
Weg  (home_mode=0)          → setpoint vs kamertemp + dauwpuntbeveiliging
```
Anti-condensbeveiliging altijd actief: `effective_setpoint = max(setpoint, dew + margin)`

**Ventilatielogica:**
```
co2_enabled && co2 > 0  → vent_percent = map(co2, 400–800 ppm, 0–100%)
Anders                  → vent_percent = slider-waarde
```

**Dot-cirkels:** binaire waarden als gekleurde `.dot` cirkels (14×14px).
Verwarming/TSTAT/Thuis/MOV1/MOV2/Dauw/CO2 tonen live kleurstatus.

**Crash-analyse (12 april 2026):** twee crashes vastgesteld. Geen directe OOM — heap_block
was normaal op crashmoment → waarschijnlijk WDT-crash. Heap_min van 2 KB bereikt door
NVS crashlog feedback loop (opgelost v2.20). CO2 `pulseIn()` blokkeert 400ms per 60s
als sensor niet leest — samen met DS18B20 delay: main loop elke minuut ~1.150ms geblokkeerd.

### 7.7 Versiehistorie
| Versie | Datum | Wijziging |
|---|---|---|
| v2.21 | 13 apr 2026 | KISS: `heating_mode` + `vent_mode` verwijderd. Matter `onChangeMode` → `home_mode`. CO2 dot. |
| v2.20 | 13 apr 2026 | NVS crashlog feedback loop fix, Matter interval 5s→30s, CO2 timeout 400ms→50ms |
| v2.19 | 12 apr 2026 | Dim snelheid + Licht tijd: JS leest slider DOM-waarde direct |
| v2.18 | 12 apr 2026 | MOV triggers direct herberekend bij PIR-event via `countRecent()` |
| v2.12–v2.17 | 12 apr 2026 | UI dot-cirkels, JS timer fixes, JSON stabilisatie |
| v2.11 | 16 mrt 2026 | `/set_home` endpoint voor Dashboard HOME/UIT broadcast |
| v2.10 | 15 mrt 2026 | Matter fixes + heap-optimalisatie (String → char[]) |

### 7.8 Openstaande punten
1. **AM312 integratie**: controleer sketch-logica active HIGH vs LOW bij overgang
2. **Aparte tegels Apple Home**: thermostat naar EP9, losse MatterTemperatureSensor EP1
3. **Dashboard matrix**: kolommen 6/7 aanpassen van `y`/`z` naar `w>0`/`x>0`
4. **CO2 `pulseIn()`**: vervanging door non-blocking aanpak
5. **Gedeeld CSS endpoint**: geschatte winst ~2–3 KB fragmentatie per request

---

## 8. Zarlar Dashboard (192.168.0.60)

### 8.1 Hardware & pinout
| Component | Detail |
|---|---|
| Board | ESP32-C6 **30-pin** clone, MAC `A8:42:E3:4B:FA:BC` |
| Static IP | 192.168.0.60 |
| WebServer | `WebServer` (blocking) — bewust, niet `AsyncWebServer` |
| Statusmatrix | 16×16 WS2812B op IO4 |

⚠️ **30-pin vs 32-pin:** pinvolgorde verschilt van 32-pin — gebruik pinout van 30-pin board.
⚠️ **Matrix voeding:** 16×16 matrix bij volle helderheid >3A — **aparte 5V, niet via shield PTC**.

### 8.2 Functies
- Pollt alle actieve ESP32-controllers via HTTP GET `/json`
- POST data naar Google Sheets via Google Apps Script
- Stuurt HOME/UIT broadcast naar alle ROOM controllers
- Toont live systeemstatus op 16×16 WS2812B statusmatrix
- Matter HOME/UIT toggle (1 endpoint: `MatterOnOffPlugin`)
- WiFi Strength Tester op `/wifi`

### 8.3 NVS namespace `zarlar`
| Key | Type | Inhoud |
|---|---|---|
| `wifi_ssid` / `wifi_pass` | String | WiFi credentials |
| `poll_min` | Int | Poll interval in minuten |
| `room_script` | String | Google Apps Script URL ROOM |
| `home_global` | Bool | HOME/UIT toestand (persistent) |
| `c0_act`..`c21_act` | Bool | Controller actief/inactief |
| `c0_url`..`c21_url` | String | Google Sheets URL per S-controller |

### 8.4 Controller polling & RSSI keys
| Controller | Type | RSSI key |
|---|---|---|
| S-HVAC | TYPE_SYSTEM | `ac` |
| S-ECO | TYPE_SYSTEM | **`p`** ← afwijkend! |
| S-ENERGY | TYPE_SYSTEM | `ac` |
| R-* (alle rooms) | TYPE_ROOM | `ac` |
| P-* (Photon) | TYPE_PHOTON | — |

### 8.5 Matter endpoint
| # | Type | Variabele | Opmerking |
|---|---|---|---|
| EP1 | MatterOnOffPlugin | `home_mode_global` | HOME=aan, WEG=uit |

Synchronisatie: boot + Apple Home callback + webUI handler + loop sync elke 5s.

### 8.6 Statusmatrix 16×16
**Rij-indeling:**

| Rij | Controller | Type |
|---|---|---|
| 0 | S-HVAC | Systeem |
| 1 | S-ECO | Systeem |
| **2** | **S-ENERGY** | **Systeem — toe te voegen** |
| 3 | S-OUTSIDE | Gereserveerd |
| 4 | Separator | "ROOMS" |
| 5–11 | R-BandB…R-ZITPL | Rooms |
| 12–15 | — | Leeg |

**S-ENERGY matrix-rij (rij 2) — kolomindeling:**

| Col | Key | Label | Kleurlogica |
|---|---|---|---|
| 0 | — | Status | Groen=online, rood=offline |
| 1 | `a` | Solar | Geel dim→groen helder |
| 2 | `d` | Balans | Rood=afname / groen=injectie |
| 3 | `n` | EPEX nu | Groen<€0,10 / geel / oranje / rood |
| 4 | `n2` | EPEX +1u | Zelfde schaal |
| 5 | `e` | ECO-boiler | Grijs=uit / oranje=aan |
| 6 | `f` | EV-lader | Grijs=uit / groen=laden |
| 7 | `g` | Override | Rood=override / dim=auto |
| 8–13 | — | Reserve | — |
| 14 | `ae` | Heap | Groen>35KB / geel / rood |
| 15 | `ac` | RSSI | Groen≥-60 / oranje / rood |

**Boot-animatie (v4.2+):** rood staartje rij 0–1, blauw staartje rijen 5→11, witte flash → live data.

**Automatische ESP32/Photon fallback:**
```
1. ESP32 actief + json aanwezig → renderRoomRow()   (definitief)
2. ESP32 inactief of geen data  → renderPhotonRow() (tijdelijk)
3. Geen controller              → zwart
```

### 8.7 WiFi Strength Tester
- `/wifi` — HTML pagina
- `/wifi_json` — `{"r":-62,"q":76}` (~22 bytes, `char buf` op stack)
- TCP-ping via port 53 (DNS) — altijd open

### 8.8 Captive portal (OS-specifieke handlers)
```cpp
server.on("/hotspot-detect.html",       HTTP_GET, cpRedirect); // macOS/iOS
server.on("/library/test/success.html", HTTP_GET, cpRedirect); // macOS ouder
server.on("/generate_204",              HTTP_GET, cpRedirect); // Android
server.on("/connecttest.txt",           HTTP_GET, cpRedirect); // Windows
server.onNotFound(cpRedirect);
```

### 8.9 Versiehistorie
| Versie | Wijziging |
|---|---|
| v5.0 | Matter HOME/UIT + Matrix 16×16 + Photon fallback + push-architectuur stabiel |
| v5.8 | S-ENERGY controller toegevoegd (idx 4, rij 2) · `renderEnergyRow()` 16 kolommen · sim_s0/sim_p1 oranje pixels |

### 8.10 Openstaande punten
- **Matter verwijderen** uit HVAC, ECO en Dashboard sketches → heap besparing ~80–120 KB per controller (Matter enkel nodig op ROOM controllers voor Apple Home)
- **OTA testen** op Dashboard
- **Matrix kolommen 6/7 ROOM** aanpassen naar `w>0`/`x>0` (beweging ongeacht licht)

---

## 9. Schimmelbescherming via vochtigheid
Elke ROOM controller meet lokaal de luchtvochtigheid. Bij overschrijding van een drempelwaarde
vraagt de room controller ventilatie via JSON key `g` (Vent_percent). De HVAC neemt de hoogste
ventilatievraag van alle zones en stuurt de centrale ventilator aan.

De volledige schimmelbeschermingslogica zit in de room controllers — de HVAC is enkel uitvoerder.

---

## 10. Raspberry Pi Portal — Node.js v2.0

### 10.1 Aanleiding en architectuurkeuze

Op 15 april 2026 werd de drie-laags architectuur geïmplementeerd:
- ESP32 controllers doen wat ze goed kunnen: meten, regelen, `/json` publiceren
- Dashboard ESP32 blijft de bron van waarheid voor Google Sheets logging
- RPi vormt de rijke portaallaag: geen heap-limieten, volledige UI vrijheid

**Technologiekeuze: Node.js + Express** (ipv Python/Flask of nginx/cloudflared)
- Dezelfde taal als de browser-kant (JavaScript)
- `node-fetch` voor controller polling
- Eén process, lage overhead op RPi
- Deploy via GitHub → `deploy.sh` → SSH → `update.sh`

### 10.2 RPi infrastructuur

| Component | Detail |
|---|---|
| Hardware | Raspberry Pi, vaste IP `192.168.0.50` |
| Software | Node.js v18 + Express, poort 3000 |
| Autostart | systemd `zarlar.service` |
| Repo lokaal | `/home/fidel/repo/ESP32_Zarlar/zarlar-rpi/` |
| Public map | `/home/fidel/repo/ESP32_Zarlar/zarlar-rpi/public/` |
| SSH lokaal | `ssh fidel@192.168.0.50` (wachtwoord: zarlar) |
| SSH overal | `ssh fidel@100.123.74.113` (via Tailscale) |

### 10.3 Tunnel-aanpak: Tailscale gekozen (april 2026)

> ✅ **Beslissing genomen:** Tailscale, niet Cloudflare nginx/cloudflared.

| Aspect | ~~Cloudflare Tunnel + nginx~~ | **Tailscale (gekozen)** |
|---|---|---|
| Publieke URL | ✅ `controllers.zarlardinge.be` | Via Funnel: `https://raspberrypi.tail3c7f42.ts.net` |
| Toegang externen | Browser, geen app | Tailscale app of Funnel voor gasten |
| Setup | Complex (nginx + cloudflared) | Eén commando |
| Beveiliging | Cloudflare Access policies | Tailscale authenticatie |
| Status | ❌ Niet gerealiseerd | ✅ Actief |

**Vaste toegang (Tailscale members):**
```
http://100.123.74.113:3000
```

**Tijdelijke publieke toegang (Tailscale Funnel):**
```bash
# Aanzetten (SSH naar RPi)
sudo tailscale funnel 3000
# → URL: https://raspberrypi.tail3c7f42.ts.net/

# Op achtergrond (blijft actief na SSH afsluiten)
sudo tailscale funnel --bg 3000

# Uitzetten
sudo tailscale funnel off
```

> ⚠️ Funnel maakt de portal publiek — stop altijd na gebruik!

**Tailscale gebruikers:**

| Gebruiker | Tailscale IP | Status |
|---|---|---|
| RPi | `100.123.74.113` | ✅ Online, autostart |
| MacBook Filip | `100.89.205.22` | ✅ Online |
| iPhone Filip + Mireille | `100.104.215.18` | ✅ Online |
| Maarten | — | ⬜ Nog uit te nodigen |
| Céline | — | ⬜ Nog uit te nodigen |

### 10.4 Deploy workflow

```bash
# Alles in één commando op Mac (bestanden eerst naar ~/Downloads)
bash ~/deploy.sh "omschrijving van wijziging"
```

`deploy.sh` doet automatisch:
1. `.html/.css/.js` → `public/` · `.sh/.md` → root
2. `git add + commit + pull --rebase + push`
3. SSH naar RPi → `update.sh` → rsync → herstart indien `server.js` gewijzigd

**Server.js herstart nodig bij:**
```bash
sudo systemctl restart zarlar  # alleen na server.js wijziging
```

### 10.5 server.js v2.0 — API endpoints

**Gouden regel:** browser gebruikt NOOIT lokale IPs — alles via `/api/` op RPi.

| Endpoint | Functie |
|---|---|
| `GET /api/poll/:naam` | Poll ESP32 `/json` (senrg, eco, hvac, room, dashboard) |
| `GET /api/poll/room:n` | Poll room controller (room75–room81) |
| `GET /api/photon/:id` | Photon proxy via Cloudflare Worker |
| `GET /api/matrix` | Alle controllers parallel (ESP32 + Photons) voor matrix.html |
| `GET /api/epex` | EPEX België spotprijzen (gecached, elk uur bijgewerkt) |
| `GET /api/status` | Status alle controllers |
| `GET /api/settings` | Persistente instellingen laden |
| `POST /api/settings` | Instelling opslaan |

### 10.6 Portal pagina's

| Pagina | URL | Status | Inhoud |
|---|---|---|---|
| Portal overzicht | `/` (index.html) | ✅ Actief | SVG-cirkels alle controllers, tabs |
| ECO Boiler detail | `/eco.html` | ✅ Actief | Boiler SVG 6 lagen, temperaturen |
| EPEX grafiek | `/epex-grafiek.html` | ✅ Actief | Spotprijzen, injectieteller, tariefvergelijking |
| Live matrix | `/matrix.html` | ✅ Actief | 16×16 replica, ESP32 + Photon, auto-refresh |
| HVAC detail | `/hvac.html` | ⬜ Gepland | — |
| Afrekening WON/SCH | `/afrekening` | ⬜ Gepland | Capaciteitstarief verdeling |

### 10.7 Taakverdeling ESP32 vs RPi

| Taak | ESP32 | RPi |
|---|---|---|
| Sensordata | ✅ Meet + publiceert `/json` | Pollt en toont |
| Google Sheets logging | ✅ Dashboard doet dit | ❌ Nooit overnemen |
| Statusmatrix 16×16 | ✅ Blijft ESP32 | Replica in matrix.html |
| Matter/HomeKit | ✅ Blijft ESP32 Room | — |
| EPEX grafieken | ❌ Te zwaar | ✅ RPi |
| Historiek + trends | ❌ Te zwaar | ✅ RPi (Google Sheets read-only) |
| Toegang van buitenaf | Via Cloudflare Worker (Photons) | ✅ Tailscale + Funnel |


---

## 11. Bestanden

### 11.1 ESP32 sketches
| Bestand | Beschrijving |
|---|---|
| `ESP32_C6_MATTER_HVAC_v1.19.ino` | HVAC productieversie |
| `ESP32_C6_MATTER_ECO-boiler_22mar_2200.ino` | ECO Boiler v1.23 productieversie |
| `ESP32-C6_MATTER_ROOM_13apr_v221.ino` | ROOM v2.21 — productie |
| `ESP32_C6_Zarlar_Dashboard_MATTER_v5_8.ino` | Dashboard v5.8 — S-ENERGY rij toegevoegd |
| `ESP32_C6_ENERGY_v1_27.ino`  | Smart Energy v1.27 — matrix layout definitief, actief 28/04/2026 |
| `Oude_MATTER_ROOM_3mar.ino` | Referentie: werkende Matter endpoint-volgorde |

### 11.2 Google Apps Scripts
| Bestand | Beschrijving |
|---|---|
| `HVAC_GoogleScript_v4.gs` | GAS HVAC — 31 kolommen A–AE |
| `ECO_GoogleScript_v2.gs` | GAS ECO — 20 kolommen A–T |
| `ROOM_GoogleScript_v1_4.gs` | GAS ROOM — 37 kolommen A–AK |
| `ENERGY_GoogleScript_v1.gs` | GAS ENERGY — 19 kolommen A–S, sim-vlaggen gelogd |

### 11.3 RPi Portal bestanden
| Bestand | Beschrijving |
|---|---|
| `server.js` | Node.js server v2.0 — alle endpoints + Photon proxy + matrix aggregator |
| `public/index.html` | Portal NIVO 1 — SVG-cirkels, tabs, S-ENERGY tegel |
| `public/eco.html` | ECO Boiler NIVO 2 — boiler SVG 6 lagen |
| `public/epex-grafiek.html` | EPEX grafiek — spotprijzen, injectiekolom, tariefvergelijking |
| `public/matrix.html` | Live 16×16 matrix replica — ESP32 + Photon fallback |
| `deploy.sh` | Mac deploy script — git + SSH + rsync + herstart |
| `update.sh` | RPi sync script |

### 11.4 Documentatie
| Bestand | Beschrijving |
|---|---|
| `Zarlar_Master_Overnamedocument.md` | Dit document — master referentie |
| `Energy_Management_System_v1_7.md` | Smart Energy volledig technisch werkdocument |
| `Zarlar_Portal_Plan_25apr26.md` | RPi portal projectdocument |

### 11.5 Overige
| Bestand | Beschrijving |
|---|---|
| `partitions_16mb.csv` | Custom partitietabel alle controllers |
| `Zarlar_Matrix_Labels_v5.svg` | Matrix transparant — kleurlaser opdruk |
| `worker-status.js` | Cloudflare Worker v2.0 — Photon proxy |
| `Smart_Energy_Zarlardinge_v1.1.docx` | Smart Energy promotiedocument (voor Maarten) |

---

## 12. Smart Energy Controller — samenvatting

> Volledig technisch werkdocument: `Energy_Management_System_v1_7.md`

### 12.1 Controller v1.26 — kernfeatures

**Twee onafhankelijke simulatievlaggen (kernprincipe):**

| Vlag | Default | Omschakelen |
|---|---|---|
| `SIM_S0` | `true` | Na S0-bekabeling: uitvinken `/settings` → reboot |
| `SIM_P1` | `true` | Na HomeWizard dongle (~2028): uitvinken + P1-IP invullen |

⚠️ **Nooit automatisch omschakelen** — altijd bewuste handeling.

**Eerste ingebruikname: 26 april 2026 — 21:05**
- RSSI −49 dBm · Heap 256 KB · EPEX 22,9 ct/kWh live ontvangen
- Matrix rij 2 ✅ · S-ENERGY tegel portal ✅ · Google Sheets logging ✅

### 12.2 Pinnen (bevestigd 26/04/2026)

| Pin | Functie |
|---|---|
| IO4 | WS2812B matrix DIN (via 330Ω, Pixel-line connector) |
| IO5 | S0 Solar — Inepro PRO380-S A14 (klem 18/19) |
| IO6 | S0 SCH afname — Inepro PRO380-S A5 (klem 18/19) |
| IO7 | S0 SCH injectie — Inepro PRO380-S A5 (klem 20/21) |

**LED Matrix definitief ontwerp**
- Kolomindeling samenvatting (0-11)
- Ontwerpprincipes (lightbar richting, kleurschema)
- Print overlay vermelding (SVG 120×35mm beschikbaar)


### 12.3 /json keys (v1.26)

| Key | Inhoud | Eenheid |
|---|---|---|
| `a` | Solar W | W |
| `b` | WON W (P1, + = afname) | W |
| `c` / `d` | SCH afname / injectie W | W |
| `e` | Netto SCH (+ = injectie) | W |
| `h` / `j` / `k` | Solar / SCH afname / injectie dag | Wh |
| `i` / `vw` | WON afname / injectie dag | Wh |
| `n` / `n2` | EPEX nu / +1u all-in | ct/kWh × 100 |
| `pt` | Maandpiek gecombineerd | W |
| `sim_s0` / `sim_p1` | Simulatievlaggen | 0/1 |
| `ac` / `ae` | RSSI / Heap | dBm / bytes |

### 12.4 HomeWizard P1 Meter (~2028)

**Model:** HWE-P1-RJ12 · **Activatie:** HomeWizard app → Settings → Local API AAN
**Endpoint:** `GET http://<P1_IP>/api/v1/data` (plain HTTP, geen auth)
**Docs:** https://api-documentation.homewizard.com/docs/introduction/

### 12.5 LED matrix 12×4 (48 pixels)

Kolomlabels voor behuizing: `SOL W · SOL kWh · SCH AF · SCH INJ · NETTO · WON W · EPEX · EPEX+1 · PIEK% · KOKEN? · WASSEN? · HEAP`

---

## 13. EPEX & Energietarieven

### 13.1 Leverancier

**Ecopower — Dynamische burgerstroom** (Geert Van Leuven, Lede, Fluvius Imewo)
**Vergelijking:** Groene burgerstroom vast (Filip Delannoy, Zarlardinge, Fluvius West)

### 13.2 Afnameprijsopbouw (dynamisch, mrt. 2026)

| Component | ct/kWh |
|---|---|
| EPEX spotprijs | variabel per kwartier |
| Afnametarief Fluvius | 5,23 |
| GSC | 1,10 |
| WKK | 0,39 |
| Heffingen + accijnzen | 4,94 |
| BTW | 6% |
| **Vaste opslag totaal** | **~14,32 ct/kWh** |

### 13.3 Injectietarief (conform factuur feb. 2026)

```
Injectieprijs = max(0, EPEX − 0,67 ct/kWh onbalansafslag)
BTW op injectie = 0%
```

Gemeten feb. 2026: gem. 5,25 ct/kWh ontvangen.
**Geen** afnametarief, GSC, WKK of heffingen bij injectie.

> 💡 Injecteren levert ~5 ct/kWh vs ~28 ct/kWh afnameprijs.
> **Zelf verbruiken of opslaan is altijd beter dan injecteren.**

### 13.4 Capaciteitstarief

Gebaseerd op **hoogste gecombineerde piek** van de maand (minimum 2,5 kW).
WON en SCH delen één meter → verdeling proportioneel naar individuele piek (zie §14).

---

## 14. Onderlinge Afrekening WON / SCH

WON (Maarten & Céline) en SCH (Filip & Mireille) delen één Fluvius-aansluiting en één factuur.

**Verdeling capaciteitstarief:**
```
Aandeel SCH = Piek SCH ÷ (Piek SCH + Piek WON)
Aandeel WON = Piek WON ÷ (Piek SCH + Piek WON)
```

**Tijdlijn:**

| Periode | WON meting | Status |
|---|---|---|
| Nu → ~2028 | Analoge teller — schatting via (totaal − SCH) | ⚠️ Benadering |
| Na digitale meter (~2028) | P1-dongle HomeWizard — exacte meting | ⬜ Gepland |

**Afrekenpagina portal:** `/afrekening` — gepland, nog niet gebouwd.

---

## 15. Thuisbatterij — Strategie

**Nu:** analoge teller → saldering actief → batterij loont financieel niet.
**Na 2028:** digitale meter → saldering stopt → batterij wordt interessant.

**Keuze ~2028:** twee aparte (eenvoudiger) of één gedeelde (financieel optimaler).
De S-ENERGY controller en portal zijn al ontworpen voor beide scenario's.

---

## 16. Matter — Beleid

| Controller | Matter | Reden |
|---|---|---|
| ROOM controllers | ✅ **Verplicht behouden** | Apple Home, scenes Mireille, multi-merk |
| HVAC | ⬜ Verwijderen | Enkel via portal/dashboard |
| ECO Boiler | ⬜ Verwijderen | Enkel via portal/dashboard |
| S-ENERGY | ❌ Nooit toegevoegd | Meetcontroller |
| Dashboard | ⬜ Verwijderen | Infrastructuur |

**Heap winst:** ~80–120 KB per controller na Matter verwijdering.

---

## 17. Instructies nieuwe sessie

1. **Upload dit document** + relevante sketch(es) + `Energy_Management_System_v1_7.md` bij Smart Energy sessies
2. **Vraag Claude het document samen te vatten** vóór hij iets aanpast
3. **Eerst een plan** — Claude mag pas beginnen coderen na expliciete goedkeuring
4. **Pin-mapping altijd vragen** voor een nieuwe sketch — Claude kent de hardware niet

**Kritische herinneringen:**
- `#define Serial Serial0` bovenaan — verplicht voor ESP32-C6 RISC-V
- Versieheader aanpassen bij elke wijziging
- Bij JSON-structuurwijziging: GAS-script + Dashboard + RPi portal nalopen (§3.10)
- IO-pins altijd onmiddellijk aansturen (§3.6)
- `* /` met spatie in commentaar (nooit `*/` in tekst — compileerfout)
- Nooit IO8, IO9 als input (strapping pins)
- ECO gebruikt RSSI key `p`, alle andere controllers `ac` (§5.3)
- Dashboard gebruikt `WebServer` (blocking), niet `AsyncWebServer` (§3.2)
- Pairing code altijd in webUI tonen, niet alleen Serial (§3.4)
- Nooit volledige JS-block in één `str_replace` — altijd chirurgisch (§3.11)
- `DOMContentLoaded` gebruiken, niet `window.addEventListener('load')` (§3.11)
- Geen nieuwe JSON-keys zonder expliciete toestemming (§3.10)
- Geen state-variabelen die niet NVS-persistent zijn
- KISS: geen AUTO/MANUEEL lagen boven sliders die al volledig functioneel zijn
- **Browser gebruikt NOOIT lokale IPs** — alles via `/api/` op RPi
- **SIM_S0 / SIM_P1 nooit automatisch omschakelen** — altijd bewuste handeling
- **LED matrix SENRG op IO4** (niet IO10 — bevestigd 26/04/2026)
- **Matter enkel op ROOM controllers** — verwijderen uit HVAC/ECO/Dashboard

*Zarlar project — Filip Delannoy — bijgewerkt 26 april 2026*
