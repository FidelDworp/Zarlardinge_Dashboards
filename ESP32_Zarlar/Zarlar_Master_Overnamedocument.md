
# Zarlar Thuisautomatisering — Master Overnamedocument
**ESP32-C6 · Arduino IDE · Google Sheets · Node.js · Matter (op RPi) · Tailscale**
*Filip Delannoy — Zarlardinge (BE) — bijgewerkt 16 oktober 2026*

> **Review 16 oktober 2026:**
> - **Matter** verhuist volledig van de ESP32's naar de RPi; Apple Home praat enkel nog met de RPi (§1.1, §16).
> - **S-ENERGY** bijgewerkt naar v1.29 volgens de sketch: LED op IO4, echte `/json`-keys, nieuwe Tesla bridge (§6.10).
> - **Dubbele inhoud** over RPi, energie, afrekening en batterij is vervangen door verwijzingen naar `Zarlar_Portal_Plan.md` (§10, §12–15).
> - **Rechtgezet:**
>   - pinoverzicht §2.7: IO2/IO4 bij ROOM en SENRG;
>   - naam van de partitietabel;
>   - het SSH-wachtwoord is verwijderd.
>
> **Nog te verifiëren door FiDel:**
> - **V1** Dashboard: draait v5.8 of v5.9?
> - **V2** S0-interfaceprintje: 4,7 kΩ + 1 kΩ (sketch-header) of 10 kΩ + 10 nF (eerdere tekst)?

---

## 1. Systeemoverzicht

### 1.1 Architectuur en dataflow
Een volledig zelfgebouwd thuisautomatiseringssysteem op basis van ESP32-C6 controllers,
elk met een eigen webserver. **Matter/Apple Home wordt gecentraliseerd op de RPi** (beslist oktober 2026, §16).
Tijdens de migratie hebben ROOM, HVAC en Dashboard nog hun eigen Matter-endpoints. Het **Zarlar Dashboard (192.168.0.60)**
fungeert als centrale dataverzamelaar: het ontvangt JSON van alle controllers en POST de data
naar Google Sheets via Google Apps Script. Controllers doen nooit zelf HTTPS-calls naar Google.

De **Raspberry Pi (192.168.0.50)** vormt de rijke portaallaag, de Matter-bridge naar Apple Home en de plek waar energiebeslissingen genomen worden. Ze is overal bereikbaar via Tailscale.

```
[HVAC   192.168.0.70] ──┐
[ECO    192.168.0.71] ──┤
[SENRG  192.168.0.73] ──┼──→ Zarlar Dashboard 192.168.0.60 ──→ Google Sheets
[ROOMs  192.168.0.75-81]┤
[Photons via Worker]  ──┘

RPi 192.168.0.50 ←── pollt alle controllers (/json)
   ├──→ Apple Home (Matter-bridge, doel §16)
   ├──→ S-ENERGY /tesla_cmd ──→ tesla-key-esp32 ──BLE──→ Tesla (§6.10)
   └──→ Tailscale ──→ Overal
```

**Drie lagen:**
- **Laag 1 — ESP32 controllers:** sensoren, regelen, `/json` publiceren, commando's uitvoeren
- **Laag 2 — Dashboard ESP32:** pollt alles, matrix 16×16, Google Sheets logging
- **Laag 3 — RPi portal:** rijke UI, Matter-bridge naar Apple Home, energiebeslissingen (tarieven, batterij, laadkeuzes), overal bereikbaar

**Kritisch leermoment:** HTTPS POST vanuit de ESP32 zelf mislukte structureel door heap-druk.
Elke controller publiceert enkel zijn `/json` endpoint — het Dashboard doet de rest.

### 1.2 Controllers — huidige staat (oktober 2026)

| Controller | Naam | IP | MAC | Board | Versie | Status |
|---|---|---|---|---|---|---|
| **HVAC** | ESP32_HVAC | 192.168.0.70 | 58:8C:81:32:2B:90 | 32-pin clone | v1.19 | ✅ Productie stabiel |
| **ECO Boiler** | ESP32_ECO Boiler | 192.168.0.71 | 58:8C:81:32:2B:D4 | 32-pin clone | v1.23 | ✅ Productie stabiel |
| **Smart Energy** | ESP32_C6_ENERGY | 192.168.0.73 | — | 32-pin clone | v1.29 | ✅ Actief, incl. Tesla bridge (sep 2026) |
| **Tesla-proxy** | tesla-key-esp32 | via NVS `t1_host` | — | ESP32 (apart) | — | ✅ BLE-proxy naar Tesla1 (§6.10) |
| **ROOM / Eetplaats** | ESP32_EETPLAATS | 192.168.0.80 | 58:8C:81:32:2F:48 | 32-pin clone | v2.21 | ✅ Matter + heap stabiel |
| **Zarlar Dashboard** | ESP32_ZARLAR | 192.168.0.60 | A8:42:E3:4B:FA:BC | 30-pin clone | v5.8 (V1) | ✅ Matter + Matrix 16×16 |
| **RPi Portal** | zarlar-rpi | 192.168.0.50 | — | Raspberry Pi 3B+ | server.js v2.1 | ✅ Actief (Tailscale) |

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
Bestand **`partitions.csv`** (zo staat het in de repo: `ESP32_Zarlar/partitions.csv`) in de schetsmap naast het `.ino`-bestand plaatsen.
Arduino IDE gebruikt een bestand met **exact deze naam** automatisch. Sketch-headers die nog `partitions_16mb.csv` vermelden, zijn op dat punt achterhaald.

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

> **Smart Energy** gebruikt de **Roomsense** RJ45-bus op het shield voor de
> verbinding naar de S0-interface PCB (zie §6.2), en de **Pixel-line** voor de 12×4 matrix.

### 2.6 Arduino IDE instellingen
| Instelling | Waarde |
|---|---|
| Board | ESP32C6 Dev Module |
| Flash Size | **16 MB** |
| Partition Scheme | `partitions.csv` in de schetsmap (wordt automatisch gebruikt, zie §2.2) |
| USB CDC On Boot | **Enabled** (verplicht voor Serial over USB-C) |
| Upload (eerste keer) | USB |
| Upload (daarna) | OTA via Arduino IDE → Sketch → Upload via OTA |

### 2.7 Pinout-snel-referentie alle controllers
Gedetailleerde pinout per controller: zie §4.1 (HVAC), §5.1 (ECO), §6.1 (SENRG), §7.1 (ROOM), §8.1 (Dashboard).

| Pin | HVAC | ECO | SENRG | ROOM | Dashboard |
|---|---|---|---|---|---|
| IO1 | — | Pomprelais | — | LDR1 analog | — |
| IO3 | DS18B20 | DS18B20 | — | DS18B20 | — |
| IO2 | — | — | — | LDR2 analog (beam) | — |
| IO4 | — | — | **LED matrix 12×4 DIN** | NeoPixels | Matrix 16×16 |
| IO5 | — | PWM pomp | **S0 Solar** | PIR MOV1 | — |
| IO6 | — | — | **S0 SCH afname** | DHT22 | — |
| IO7 | — | — | **S0 SCH injectie** | — | — |
| IO10 | — | — | — | TSTAT | — |
| IO11 | I2C SCL | — | — | I2C SCL | — |
| IO13 | I2C SDA | — | — | I2C SDA | — |
| IO20 | PWM vent. | SPI CS | — | — | — |

> ⚠️ **SENRG pinnen volgens sketch v1.29:** LED matrix **12×4 (48 pixels)** op **IO4** (Pixel-line, via 330 Ω),
> S0-kanalen op IO5/IO6/IO7 (Roomsense RJ45). IO2 en IO10 zijn vrij op SENRG.

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
> ⏳ **Overgangsregels.** Ze gelden alleen zolang een controller nog Matter heeft. Na migratie naar de RPi-bridge (§16) vervallen ze voor die controller.

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
> ⏳ Te migreren naar de RPi-bridge (§16).

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
> **Status: ✅ Productie, sketch v1.29 actief (24 sep 2026), S0 nog in SIM-modus.**
> Referentie: `ESP32_C6_ENERGY_v1_29.ino`. Bij verschil tussen dit document en de sketch geldt de sketch.

---

### 6.1 Hardware & pinout
| Component | Detail |
|---|---|
| Board | ESP32-C6 32-pin clone, Zarlar shield |
| Static IP | 192.168.0.73 |
| Locatie | Kastje inkomhal Maarten, naast Telenet router |
| Voeding | 5V/2A extern. Matrix NIET via shield-PTC. |
| LED matrix | **12×4 WS2812B (48 pixels)** op **IO4**, via 330 Ω, Pixel-line connector, eigen 5V |
| S0-interface | Via **Roomsense** RJ45 naar interface-PCB (zie §6.2) |

| ESP32-C6 Pin | `#define` | Functie |
|---|---|---|
| IO4 | `LED_PIN 4` | WS2812B matrix DIN (48 pixels, via 330 Ω) |
| IO5 | `S0_SOL_PIN 5` | S0 Zonnepanelen A14, forward productie (klem 18/19) |
| IO6 | `S0_SCHF_PIN 6` | S0 Schuur A5, forward afname (klem 18/19) |
| IO7 | `S0_SCHR_PIN 7` | S0 Schuur A5, reverse injectie (klem 20/21) |

### 6.2 Interface PCB — S0 aansluiting
**S0-uitgang Inepro PRO380-S:** 0,1 Wh/imp (10.000 imp/kWh), directe verbinding zonder optocoupler.

**Schema per kanaal, zoals in de sketch-header v1.29:**
```
3,3V ─[4,7kΩ]─┬─ GPIO (INPUT, geen interne pull-up)
              [1kΩ] serie
               │
              S0+ klem 18/20 (Inepro PRO380-S)
              S0– klem 19/21 ──── GND
```
> ⚠️ **V2:** een eerdere versie van dit document beschreef een 40×40 mm SMD-PCB met 10 kΩ pull-up + 10 nF, 4 kanalen. Welk printje werkelijk gebouwd of besteld is, moet nog bevestigd worden.

**RJ45 Roomsense pinout (sketch v1.29):**

| RJ45 pin | Signaal | GPIO |
|---|---|---|
| 1 | GND | GND |
| 2 | 3,3V | 3,3V |
| 3 | S0 Solar | IO5 |
| 4 | S0 SCH afname | IO6 |
| 5 | S0 SCH injectie | IO7 |
| 6 | vrij | — |
| 7 | GND | GND |
| 8 | 5V | 5V |

### 6.3 Libraries
| Library | Gebruik |
|---|---|
| `AsyncTCP` + `ESPAsyncWebServer` | Webserver |
| `Adafruit NeoPixel` | WS2812B matrix 12×4 |
| `Preferences` | NVS (namespace `senrg`): dagcumulatieven, instellingen, Tesla-config |
| `HTTPClient` | P1-dongle (stream, zonder `getString()`) |
| `WiFiClient` (eigen minimale HTTP/1.0-client) | EPEX via RPi `/api/epex` + Tesla-proxy (heap-zuinig) |
| `ArduinoJson` | Stream-parse met filter (EPEX, P1, Tesla) |
| `DNSServer` | Captive portal in AP-modus |

### 6.4 /json endpoint (v1.29)
Dit is de **enige referentie** voor de Smart Energy JSON-keys. Bij een wijziging: Dashboard matrix-rij 2, GAS-script ENERGY en de RPi-pagina's (`12x4_matrix.html`, `epex-grafiek.html`) nalopen.

| Key | Inhoud | Eenheid |
|---|---|---|
| `a` | Solar vermogen | W |
| `b` | WON vermogen (P1, + = afname) | W |
| `c` | SCH afname | W |
| `d` | SCH injectie | W |
| `e` | Netto SCH (+ = injectie) | W |
| `h` | Solar dag | Wh |
| `i` | WON afname dag | Wh |
| `j` | SCH afname dag | Wh |
| `k` | SCH injectie dag | Wh |
| `vw` | WON injectie dag | Wh |
| `n` | EPEX nu, all-in | ct/kWh × 100 |
| `n2` | EPEX +1 u, all-in | ct/kWh × 100 |
| `pt` | Piek maand (momentaan, enkel SCH, zie §6.9) | W |
| `ac` | RSSI | dBm |
| `ae` | Heap largest block | bytes |
| `sim_s0` / `sim_p1` | Simulatievlaggen | 0/1 |
| `ver` | Firmwareversie | tekst |

> Keys `pw` en `ps` (individuele maandpiek WON/SCH) bestaan **nog niet**. Ze staan op de openstaande lijst. De keys `q`, `qv`, `r`, `rv`, `s`, `sv`, `nv`, `f`, `g`, `o`, `eod` en `v` uit eerdere versies van dit document zijn evenmin aanwezig. Tesla-data zit apart in `/tesla_json` (§6.10).

**Simulatievlaggen:** `SIM_S0` (3× S0-kanalen) en `SIM_P1` (HomeWizard P1, WON) zijn onafhankelijk, en elk wordt via `/settings` ingesteld. ⚠️ **Nooit automatisch omschakelen.**

**HomeWizard P1 (WON, ~2028):** model HWE-P1-RJ12, `GET http://<P1_IP>/api/v1/data` (plain HTTP, geen auth). Lokale API aanzetten in de HomeWizard-app. Dagcumulatieven via een midnight-snapshot (sinds v1.29 gereset bij dagwissel).

### 6.5 LED matrix 12×4 (48 pixels WS2812B)

**Layout:** 12 kolommen × 4 rijen. Adressering via `pxIdx(col,row)`.
> ⚠️ Het commentaarblok bij `MATRIX` in de sketch (rond regel 95) beschrijft nog een oudere kolomindeling. De tabel hieronder (v1.27) is de werkelijke rendering.

**Kolomindeling v1.27 (definitief):**

| Col | Sym | Label | Bereik | Kleurlogica |
|---|---|---|---|---|
| 0 | ☀️ | SOL | 0-6kW | Groen lightbar (onder→boven) |
| 1 | ⚡ | SCH↓ | 0-10kW | Rood lightbar (afname) |
| 2 | ⚡ | SCH↑ | 0-6kW | Groen lightbar (injectie) |
| 3 | 🏠 | WON↓ | 0-10kW | Rood lightbar (afname) |
| 4 | 🏠 | WON↑ | 0-6kW | Groen lightbar (injectie, ~2028) |
| 5 | 📊 | PIEK | 15kW | Groen OK / rood alarm |
| 6 | 💰 | ct/kWh | 0-40ct | Cyaan→groen→geel→oranje→rood |
| 7 | 💡 | HUIS | OK | Groen=goed moment / rood=duur |
| 8 | 🔋 | BAT | ~2028 | Dim paars (toekomst) |
| 9 | ❤️ | HEAP | 60KB | Groen>50% / amber / rood |
| 10 | 📶 | WiFi | -60dB | Groen≥-60 / amber / rood |
| 11 | 🔴 | SIM | S0·P1 | **Rij 0 = S0, rij 1 = P1**: rood=SIM / groen=LIVE. Rij 2-3 vrij (voorzien voor Tesla1). |

**Kleurprincipes:** groen = € opbrengst / goed, rood = € kosten / slecht. Lightbars vullen van onderaan (rij 3) naar boven.

**Web-replica:** `/12x4_matrix.html` op de RPi.

### 6.6 Matter
Geen Matter, nu niet en in de toekomst niet (§16).

### 6.7 Heap-baseline
**Gemeten v1.27 (28/04/2026):** largest free block ~262 KB (zonder Matter) ✅.
v1.29: heap-robuustheid verbeterd (geen `http.getString()`, geen `String` in serial), maar nog niet opnieuw gemeten.

### 6.8 Versiehistorie
| Versie | Datum | Inhoud |
|---|---|---|
| v1.24 | 24 apr 2026 | Eerste productieversie, live S0 ISR |
| v1.25 | 25 apr 2026 | Enkelvoudige SIMULATION_MODE (archief) |
| v1.26 | 25 apr 2026 | Twee onafhankelijke simulatievlaggen SIM_S0 / SIM_P1 |
| v1.27 | 28 apr 2026 | Matrix layout definitief, `pxIdx()` correct, SIM col 11 bovenaan |
| v1.28 | 24 sep 2026 | **Tesla1**: pagina `/tesla`, data volgen + handmatig start/stop/wake/ampère via proxy. Middernacht-reset elke tick + dag in NVS, `configTzTime` CET/CEST. |
| v1.29 | 24 sep 2026 | Heap/robuustheid: P1 + EPEX zonder `getString()`, P1 midnight-snapshot reset, escapes in `/tesla_json`, default max_piek 15 kW. Tesla-gedrag ongewijzigd. |

### 6.9 Openstaande actiepunten

| Actie | Wie | Status |
|---|---|---|
| S0 bekabeling aansluiten → SIM_S0 uit (bewust, via /settings) | Filip + Maarten | Open |
| UTP-kabel trekken verdeelkast → inkomhal | Filip + Maarten | Open |
| `pt` wordt een echte kwartierpiek (nu momentaan en enkel SCH) | Filip | Open |
| `pw` (piek WON) zodra P1-dongle actief is | Filip | ~2028 |
| Vaste opslag ophalen via RPi `/api/settings` (nu hardcoded `VAST_CT_KWH` 14,32) | Filip | Open |
| ntfy.sh push bij piekdrempel (app installeren Filip + Maarten) | Filip + Maarten | Open |
| **Tesla1:** dag-/totaaltellers solar→Tesla, `t1_`-keys in `/json` (+ GAS + Dashboard), matrix col 11 rij 2-3 | Filip | Open |
| **Tesla1 automatische sturing:** beslissing op RPi, uitvoering via `/tesla_cmd` (§6.10) | Filip | Open |
| **Dashboard matrix-rij 2** afstemmen op de v1.29-keys (zie §8.6) | Filip | Open |
| EV-lader 2: merk/type opzoeken | Maarten | Open |
| CZ-TAW1 WP WON reset + herregistratie | Filip | Open |
| Portal 12×4 matrix kleuren debuggen (sommige fout) | Filip | Open |

### 6.10 Tesla bridge (Tesla1, sinds v1.28)

**Keten:**
```
RPi portal ──(gepland)──► S-ENERGY .73  /tesla_cmd · /tesla_json · /tesla_cfg
                               │  HTTP/1.0, poort 80
                               ▼
                    tesla-key-esp32 proxy  (aparte ESP32, host + VIN in NVS)
                               │  Bluetooth LE (lokaal, geen cloud)
                               ▼
                            Tesla (Tesla1)
```

| Item | Detail |
|---|---|
| Proxy | `tesla-key-esp32`: aparte ESP32 die Fleet-API-achtige REST-calls lokaal via BLE naar de auto stuurt |
| Config (NVS) | `t1_host` (proxy host/IP), `t1_vin` (VIN), `t1_maxa` (max laadstroom, 5–32 A, default 16) |
| Lezen | `GET /api/1/vehicles/<VIN>/vehicle_data` → stream-parse van alleen `charge_state` (15 keys: SOC, laadstatus, A, V, fasen, kWh, minuten, bereik, laadpoort) |
| Commando's | `POST /api/1/vehicles/<VIN>/command/{charge_start, charge_stop, wake_up, set_charging_amps}` |
| Poll-interval | 15 s als `/tesla` open staat · 5 min als niemand kijkt · 60 s na een fout |
| Timeouts | lezen 10 s · commando 12 s (wake-up duurt 5 tot 15 s) |

**Endpoints op S-ENERGY:**

| Endpoint | Methode | Functie |
|---|---|---|
| `/tesla` | GET | Bedieningspagina (PROGMEM) |
| `/tesla_json` | GET | Laatste Tesla-data + status |
| `/tesla_cfg` | GET | `{"host","vin","maxa"}` |
| `/tesla_save?host=&vin=&maxa=` | GET | Config opslaan in NVS |
| `/tesla_cmd?c=` | POST | Commando (start/stop/wake/ampère, begrensd door `t1_maxa`) |

**Ontwerpregels:**
- **Geen automatische sturing in de sketch.** Laadbeslissingen (solar-overschot, EPEX, piek, keuzes van Maarten en Filip) worden op de RPi genomen en via `/tesla_cmd` uitgevoerd.
- Heap-zuinig: eigen minimale HTTP-client, geen `String`, ArduinoJson-filter, vaste buffers.
- Blokkerend in `loop()` met deadline. De S0-ISR blijft pulsen tellen en de webserver draait apart.

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
> ⏳ Te migreren naar de RPi-bridge (§16). Het bridge-accessoire moet dezelfde functies aanbieden: thermostaat Thuis/Weg, vochtigheid, bewegingssensoren, kleurlicht en SW1–SW3.

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

**Apple Home koppeling (huidig):** Thermostat UIT = `home_mode=0` (Weg), HEAT = `home_mode=1` (Thuis).
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
2. ~~Aparte tegels Apple Home~~: vervalt, wordt geregeld in de RPi-bridge (§16)
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
> ⏳ Te migreren naar de RPi-bridge (§16).

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
| **2** | **S-ENERGY** | **Systeem — ✅ Actief (S-ENERGY v1.29)** |
| 3 | S-OUTSIDE | Gereserveerd |
| 4 | Separator | "ROOMS" |
| 5–11 | R-BandB…R-ZITPL | Rooms |
| 12–15 | — | Leeg |

**S-ENERGY matrix-rij (rij 2) — kolomindeling:**

> ⚠️ **Loopt niet meer gelijk met S-ENERGY v1.29** (§6.4): `d` is nu SCH-injectie (niet de balans), `e` is netto W (niet de ECO-boiler), en `f`/`g` bestaan niet meer. Deze tabel beschrijft de Dashboard-code v5.8. Rendering aanpassen staat in de openstaande punten van §6.9.

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

**Heap-druk door Matter:** opgelost door Matter naar de RPi te verhuizen (§16). Het idee van een aangepaste partitietabel (april 2026) is vervallen: een partitietabel verdeelt de flash, niet het RAM.

**OTA testen op Dashboard:** Nog niet getest na v5.8 update

**Matrix kolommen 6/7 ROOM aanpassen naar bewegingsdetectie:**
- **Huidige logica:** Col 6/7 tonen PIR beweging ALLEEN als licht aan is (`w>0 AND x>0`)
- **Gewenste logica:** Col 6/7 tonen ALTIJD beweging als PIR triggert (`w>0` en `x>0` apart)
- **Reden:** Bewegingshistorie blijft zichtbaar ook als licht uit is
- **Impact:** Betere debugging van PIR sensoren, gedragspatronen herkennen
- **TODO:** ROOM sketch aanpassen en testen

---

## 9. Schimmelbescherming via vochtigheid
Elke ROOM controller meet lokaal de luchtvochtigheid. Bij overschrijding van een drempelwaarde
vraagt de room controller ventilatie via JSON key `g` (Vent_percent). De HVAC neemt de hoogste
ventilatievraag van alle zones en stuurt de centrale ventilator aan.

De volledige schimmelbeschermingslogica zit in de room controllers — de HVAC is enkel uitvoerder.

---

## 10. Raspberry Pi Portal — samenvatting

> **Volledig detail:** `zarlar-rpi/Zarlar_Portal_Plan.md`. Dat document is de enige referentie voor alles op de RPi: server, API, pagina's, deploy, Tailscale, Matter-bridge en energiebeslissingen. Hieronder staat alleen wat je nodig hebt om het geheel te begrijpen.

| Item | Waarde |
|---|---|
| Hardware | Raspberry Pi 3B+ (±1 GB), Debian 13, vaste IP `192.168.0.50` |
| Software | Node.js + Express, `server.js` v2.1, poort 3000, systemd `zarlar.service` |
| Toegang | Tailscale `http://100.123.74.113:3000` · Funnel (tijdelijk) `https://raspberrypi.tail3c7f42.ts.net/` |
| Deploy | `bash ~/deploy.sh "omschrijving"` op de Mac (geen `!` in de boodschap) |

**Gouden regel:** de browser gebruikt NOOIT lokale IP's. Alles loopt via `/api/` op de RPi.

### 10.1 Taakverdeling ESP32 vs RPi (bijgewerkt okt 2026)

| Taak | ESP32 | RPi |
|---|---|---|
| Sensordata, I/O, lokale regeling | ✅ Meet, regelt, publiceert `/json` | Pollt en toont |
| Google Sheets logging | ✅ Dashboard doet dit | ❌ Nooit overnemen |
| Statusmatrices (16×16, 12×4) | ✅ Fysieke matrix | Web-replica |
| **Matter / Apple Home** | ❌ Verdwijnt van de ESP32's (§16) | ✅ Enige Matter-laag (bridge) |
| **Energiebeslissingen** (tarieven, batterijplanning, laadkeuzes Maarten/Filip) | Voert uit (S-ENERGY, Tesla bridge §6.10) | ✅ Bewaart instellingen en keuzes, beslist, stuurt door |
| EPEX-grafieken, afrekening WON/SCH | ❌ Te zwaar | ✅ |
| Historiek + trends | ❌ | ✅ (Google Sheets read-only + eigen stats) |
| Toegang van buitenaf | Photons via Cloudflare Worker | ✅ Tailscale + Funnel |

---

## 11. Bestanden

> ⚠️ Van de sketches staat alleen `ESP32_C6_Zarlar_Dashboard-Matrix_MATTER.ino` in de GitHub-repo. De andere bestanden staan in het Claude-project of lokaal op de Mac.

### 11.1 ESP32 sketches
| Bestand | Beschrijving |
|---|---|
| `ESP32_C6_MATTER_HVAC_v1.19.ino` | HVAC productieversie |
| `ESP32_C6_MATTER_ECO-boiler_22mar_2200.ino` | ECO Boiler v1.23 productieversie |
| `ESP32-C6_MATTER_ROOM_13apr_v221.ino` | ROOM v2.21 productie |
| `ESP32_C6_Zarlar_Dashboard_MATTER_v5_8.ino` | Dashboard v5.8 (v5.9 bestaat mogelijk, zie V1) |
| `ESP32_C6_ENERGY_v1_29.ino` | **Smart Energy v1.29**: Tesla1-bridge + heap-robuustheid (24 sep 2026) |
| `Oude_MATTER_ROOM_3mar.ino` | Referentie: werkende Matter endpoint-volgorde |

### 11.2 Google Apps Scripts
| Bestand | Beschrijving |
|---|---|
| `HVAC_GoogleScript_v4.gs` | GAS HVAC: 31 kolommen A–AE |
| `ECO_GoogleScript_v2.gs` | GAS ECO: 20 kolommen A–T |
| `ROOM_GoogleScript_v1_4.gs` | GAS ROOM: 37 kolommen A–AK |
| `ENERGY_GoogleScript_v1.gs` | GAS ENERGY: 19 kolommen A–S, sim-vlaggen gelogd |

### 11.3 RPi Portal bestanden
Zie `zarlar-rpi/Zarlar_Portal_Plan.md` §1.2.

### 11.4 Documentatie
| Bestand | Beschrijving |
|---|---|
| `ESP32_Zarlar/Zarlar_Master_Overnamedocument.md` | Dit document: systeem + alle ESP32-controllers |
| `ESP32_Zarlar/zarlar-rpi/Zarlar_Portal_Plan.md` | RPi portal, Matter-bridge, energie (tarieven, afrekening, batterij) |
| `Energy_Management_System_v1_8.md` | Oud werkdocument Smart Energy. Inhoud gaat naar Portal Plan (energie) en §6 (controller). Daarna archiveren. |

### 11.5 Overige
| Bestand | Beschrijving |
|---|---|
| `partitions.csv` | Custom partitietabel alle controllers (§2.2) |
| `Zarlar_Matrix_Labels_v5.svg` | Matrix transparant voor kleurlaser-opdruk |
| `worker-status.js` | Cloudflare Worker v2.0: Photon proxy |
| `Smart_Energy_Zarlardinge_v1.1.docx` | Smart Energy promotiedocument (voor Maarten) |

---

## 12. Smart Energy — verwijzing
Vroeger stond hier een tweede samenvatting van de S-ENERGY-controller. Die spreekt §6 tegen en is verwijderd. **De enige referentie is §6.**

## 13. EPEX & Energietarieven — verwijzing
Verhuisd naar `Zarlar_Portal_Plan.md` §6 (tarieven, injectie: EPEX − 0,67 ct/kWh onbalansafslag, 0% BTW). Energie-instellingen en -beslissingen leven op de RPi.

## 14. Onderlinge Afrekening WON / SCH — verwijzing
Verhuisd naar `Zarlar_Portal_Plan.md` §13.

## 15. Thuisbatterij — verwijzing
Verhuisd naar `Zarlar_Portal_Plan.md` §14.

---

## 16. Matter — Beleid (beslist oktober 2026)

**Beslissing:** Matter verdwijnt van alle ESP32-controllers en wordt gecentraliseerd op de RPi. **Apple Home praat alleen nog met de RPi.** Uitwerking en stappenplan: `Zarlar_Portal_Plan.md` §16.

| Controller | Matter nu | Na migratie |
|---|---|---|
| ROOM controllers | ✅ 8 endpoints (§7.4) | ❌ Verwijderd. Rooms verschijnen via de RPi-bridge. |
| HVAC | ✅ 9 endpoints (§4.4) | ❌ Verwijderd. Kringen en ventilatie via de RPi-bridge. |
| Dashboard | ✅ 1 endpoint HOME/UIT (§8.5) | ❌ Verwijderd. HOME/UIT via de RPi-bridge. |
| ECO Boiler | ❌ (§5.4) | ❌ |
| S-ENERGY | ❌ | ❌ |

**Waarom:**
- 80 tot 120 KB heap-winst per controller. ROOM heeft nu maar 6 KB marge boven de crashdrempel (§7.5).
- Eenvoudigere sketches: geen `chip-*` NVS, geen pairing, geen endpoint-volgorde meer.
- Eén plek voor Apple Home, en later eventueel Google Home.

**Vervallen strategie (april 2026):** Matter behouden en de heap vergroten met een aangepaste partitietabel. ⚠️ Dat kon technisch niet werken: de partitietabel verdeelt het **flashgeheugen** (16 MB), terwijl de heap in het **SRAM** zit (512 KB). Een andere partitietabel geeft geen extra RAM.

**Migratie per controller** (volgorde in Portal Plan §16.5):
1. De RPi-bridge biedt de controller aan in Apple Home en wordt getest naast het bestaande Matter-device.
2. Nieuwe sketchversie zonder Matter, met bijgewerkte versieheader en nieuwe heap-baseline.
3. Het oude accessoire uit Apple Home verwijderen. Scènes en automaties (o.a. van Mireille) opnieuw koppelen aan het bridge-accessoire.
4. §3.4, het Matter-gedeelte van §3.6 en de Matter-sectie van die controller in dit document laten vervallen.

**`#define Serial Serial0` na migratie:** volgens §3.1 en §3.9 stuurt deze regel zonder Matter de output naar UART0, waardoor de Serial Monitor leeg blijft. Per controller testen of de regel weg moet zodra Matter verwijderd is. S-ENERGY v1.29 (zonder Matter) heeft hem wel staan: nakijken hoe de Serial Monitor zich daar gedraagt.

---

## 17. Instructies nieuwe sessie

1. **Upload dit document**, plus de relevante sketch(es). Voor RPi- of energiesessies ook `Zarlar_Portal_Plan.md`.
2. **Vraag Claude het document samen te vatten** vóór hij iets aanpast.
3. **Eerst een plan.** Claude mag pas coderen na expliciete GO.
4. **Pin-mapping altijd vragen** voor een nieuwe sketch. Claude kent de hardware niet.
5. **Code wint van tekst:** bij twijfel is de actieve sketch de referentie, niet dit document.

**Kritische herinneringen:**
- `#define Serial Serial0` bovenaan, verplicht voor ESP32-C6 RISC-V met Matter (zie §16 voor zonder Matter)
- Versieheader aanpassen bij elke wijziging
- Bij een JSON-structuurwijziging: GAS-script + Dashboard + RPi portal nalopen (§3.10)
- IO-pins altijd onmiddellijk aansturen (§3.6)
- `* /` met spatie in commentaar (nooit `*/` in tekst, dat geeft een compileerfout)
- Nooit IO8 of IO9 als input (strapping pins)
- ECO gebruikt RSSI-key `p`, alle andere controllers `ac` (§5.3)
- Dashboard gebruikt `WebServer` (blocking), niet `AsyncWebServer` (§3.2)
- Nooit een volledig JS-block in één `str_replace`, altijd chirurgisch (§3.11)
- `DOMContentLoaded` gebruiken, niet `window.addEventListener('load')` (§3.11)
- Geen nieuwe JSON-keys zonder expliciete toestemming (§3.10)
- Geen state-variabelen die niet NVS-persistent zijn
- KISS: geen AUTO/MANUEEL-lagen boven sliders die al volledig functioneel zijn
- **Browser gebruikt NOOIT lokale IP's**, alles via `/api/` op de RPi
- **SIM_S0 / SIM_P1 nooit automatisch omschakelen**, altijd een bewuste handeling
- **LED matrix 12×4 SENRG op IO4** (48 pixels, via 330 Ω, Pixel-line connector, bevestigd in sketch v1.29)
- **Matter:** geen nieuwe Matter-code op ESP32's. Alles verhuist naar de RPi (§16).
- **Tesla:** geen automatische sturing in de sketch. Beslissingen komen van de RPi (§6.10).

*Zarlar project — Filip Delannoy — bijgewerkt 16 oktober 2026*
