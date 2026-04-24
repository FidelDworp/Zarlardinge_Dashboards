# Zarlar Portal — Projectdocument
*Bijgewerkt april 2026 — Filip Delannoy (FiDel)*

---

## 1. Infrastructuur & toegang

### 1.1 RPi server

| Component | Detail |
|---|---|
| Hardware | Raspberry Pi, vaste IP `192.168.0.50` |
| Software | Node.js v18 + Express, poort 3000 |
| Repo lokaal | `/home/fidel/repo/ESP32_Zarlar/zarlar-rpi/` |
| Public map | `/home/fidel/repo/ESP32_Zarlar/zarlar-rpi/public/` |
| Autostart | systemd `zarlar.service` ✅ |
| SSH lokaal | `ssh fidel@192.168.0.50` (wachtwoord: zarlar) |
| SSH overal | `ssh fidel@100.123.74.113` (via Tailscale) |

### 1.2 GitHub repository

**Repo:** `FidelDworp/Zarlardinge_Dashboards/ESP32_Zarlar/zarlar-rpi/`

```
zarlar-rpi/
├── server.js               ← Node.js server v2.0
├── update.sh               ← RPi sync script
├── deploy.sh               ← Deploy script (ook actief op Mac als ~/deploy.sh)
├── README.md
├── Zarlar_Portal_Plan.md   ← Dit document
└── public/
    ├── index.html          ← Portal NIVO 1 (synoptisch overzicht)
    ├── eco.html            ← ECO Boiler NIVO 2 detail pagina
    ├── epex-grafiek.html   ← EPEX energie grafiek
    ├── zarlar.css          ← Gedeelde stijl
    └── zarlar.js           ← Gedeelde functies
```

### 1.3 Deploy workflow

**Alles in één commando op Mac:**
```bash
bash ~/deploy.sh "omschrijving van wijziging"
```

`deploy.sh` doet automatisch:
1. Detecteert bestanden in `~/Downloads`
   - `.html/.css/.js` → `public/`
   - `.sh/.md` → root van zarlar-rpi/
2. `git add + commit`
3. `git pull --rebase` (voorkomt conflicten)
4. `git push`
5. SSH naar RPi via Tailscale → `bash ~/update.sh`
6. Ruimt Downloads op

**Werkt van overal** — thuis én buitenshuis via Tailscale.

**`deploy.sh` zelf updaten op Mac:**
```bash
cp ~/Downloads/deploy.sh ~/deploy.sh && chmod +x ~/deploy.sh && bash ~/deploy.sh "..."
```

### 1.4 Remote toegang via Tailscale

| Apparaat | Tailscale IP | Status |
|---|---|---|
| RPi | `100.123.74.113` | ✅ Online, autostart |
| MacBook | `100.89.205.22` | ✅ Online |
| iPhone Filip + Mireille | `100.104.215.18` | ✅ Online |
| Maarten | — | ⬜ Nog uit te nodigen |
| Céline | — | ⬜ Nog uit te nodigen |

- **Gratis plan:** 3 gebruikers (Filip/Mireille = 1 Apple account, Maarten, Céline)
- **Filip + Mireille** delen Apple account → beide iPhones automatisch verbonden
- **Uitnodigen:** Tailscale dashboard → Add device → Share → e-mailadres
- **Portal bereikbaar overal:** `http://100.123.74.113:3000`

---

## 2. Netwerk controllers

```
192.168.0.50  → RPi Zarlar Portal (Node.js poort 3000)
192.168.0.60  → ESP32 Dashboard v5.7 (bron van waarheid, matrix 16×16)
192.168.0.70  → ESP32 HVAC v1.19
192.168.0.71  → ESP32 ECO Boiler v1.23
192.168.0.73  → ESP32 Smart Energy (in ontwikkeling)
192.168.0.80  → ESP32 Room v2.21 (Eetplaats/Testroom)
192.168.0.75–.81 → Toekomstige Room controllers
```

### 2.1 Architectuurprincipes

**Dashboard controller (192.168.0.60) = bron van waarheid**
- Registreert alle controllers in het systeem
- Pollt alle controllers elke 15 seconden
- Stuurt data naar Google Sheets — BLIJFT ZO, RPi neemt dit NOOIT over
- Serveert /json met volledige systeemstatus
- Bestuurt 16×16 RGB pixel matrix

**RPi = display + controle laag**
- Leest /json van Dashboard controller (lijst van alle controllers)
- Haalt /json en /capabilities op van elke controller
- Toont rijke UI zonder heap beperkingen
- Leest Google Sheets (read-only) voor historiek
- Voert scenes en automatisaties uit via /api/set/
- Neemt GEEN dataverzameling of logging taken over

**Gouden regels:**
- /json endpoint op elke controller wordt nooit gewijzigd
- RPi neemt nooit Google Sheets logging over
- Elke fase levert direct tastbaar voordeel op

**Gouden regel:** Browser gebruikt NOOIT lokale IPs — alles via `/api/` op RPi.
```javascript
// FOUT — werkt niet extern
fetch('http://192.168.0.71/json')
// CORRECT — werkt overal
fetch('/api/poll/eco')
```

---

## 3. server.js v2.0 — API endpoints

| Endpoint | Methode | Functie |
|---|---|---|
| `/api/settings` | GET/POST | Persistente instellingen |
| `/api/epex` | GET | EPEX België spotprijzen (cache) |
| `/api/poll/:naam` | GET | Poll ESP32 `/json` |
| `/api/capabilities/:naam` | GET | Controller capabilities (cache 6u) |
| `/api/set/:naam` | POST | Commando naar controller |
| `/api/status` | GET | Status alle controllers |
| `/api/scenes` | GET/POST/DELETE | Scènes beheren |
| `/api/scenes/:naam/run` | POST | Scène uitvoeren |
| `/api/controllers` | GET | Lijst van controllers |
| `/api/history/:naam` | GET | Google Sheets historiek (read-only) |

---

## 3b. Self-describing API — /capabilities

Elke controller beschrijft zichzelf volledig. De RPi bouwt de UI automatisch
op basis hiervan — geen hardcoded kennis nodig.

```json
{
  "controller": "ROOM1",
  "naam": "Woonkamer",
  "versie": "2.10",
  "sensoren": [
    { "key": "t",  "naam": "Temperatuur", "eenheid": "C",   "type": "float" },
    { "key": "h",  "naam": "Vochtigheid", "eenheid": "%",   "type": "float" },
    { "key": "co", "naam": "CO2",         "eenheid": "ppm", "type": "int"   }
  ],
  "bedieningen": [
    { "key": "p0", "naam": "Salon links", "type": "toggle",
      "endpoint": "/set", "body_aan": {"p0": 1}, "body_uit": {"p0": 0} },
    { "key": "dim", "naam": "Dimmer salon", "type": "slider",
      "min": 0, "max": 100, "eenheid": "%",
      "endpoint": "/set", "body": {"dim": "{waarde}"} }
  ],
  "instellingen": [
    { "key": "naam",   "naam": "Kamer naam",     "type": "text" },
    { "key": "pixels", "naam": "Actieve pixels", "type": "int", "min": 0, "max": 16 }
  ]
}
```

UI types die de portal automatisch genereert:

| Type | UI element |
|---|---|
| `toggle` | Aan/uit schakelaar |
| `slider` | Schuifbalk met min/max |
| `float` / `int` | Getal met eenheid |
| `text` | Tekstveld |
| `color` | Kleurpicker |

**Cachen op RPi:** in `controller-configs.json`
Opgehaald bij: RPi opstart, elke 6u, manuele refresh, bij scene aanmaken.
Op de controller: statische JSON string in PROGMEM of NVS — minimale heap impact.

---

## 4. Portal architectuur & visie

### 4.1 Filosofie

**Apple-like · KISS · Grafisch-eerst**

- Zo weinig mogelijk tekst, zo veel mogelijk visuele beelden
- Werkt perfect op telefoon én desktop
- Intuïtief voor iedereen in het gezin zonder uitleg
- Donker thema
- SVG symbolen als visuele taal

### 4.2 Prioriteit van informatie

**Niveau A — Alarmen (altijd bovenaan):**
Vocht/dauwpunt alert, temperatuuralarm, onverwachte beweging, controller offline

**Niveau B — Actuele toestand (standaard):**
Per controller type: de meest relevante data in de concrete situatie

**Niveau C — Details op aanvraag:**
Via klik naar NIVO 2 pagina

### 4.3 Drie lagen

```
┌─────────────────────────────────────────────┐
│  NIVO 1 — Synoptisch overzicht (index.html) │
│  Donker canvas · SVG cirkels per controller │
│  [Overzicht] [💡 Lichten] [🌡️ HVAC] [🔒 Sec]│
└──────────────┬──────────────────────────────┘
               │ klik op cirkel
┌──────────────▼──────────────────────────────┐
│  NIVO 2a — Controller detail                │
│  Grote SVG visualisatie + cijfers           │
│  Instellingen onderaan                      │
├─────────────────────────────────────────────┤
│  NIVO 2b — Bedieningen tabs                 │
│  💡 Lichten  🌡️ HVAC  🔒 Security           │
└─────────────────────────────────────────────┘
```

### 4.4 NIVO 1 — Synoptisch overzicht

**Huidige implementatie (index.html):**
- 6 klikbare controller-cirkels met Filip's SVG iconen
- Tab navigatie: Overzicht / Lichten / HVAC / Security
- Live data via `/api/status` — auto-refresh 15s
- Demodata als server niet bereikbaar

| Controller | SVG icoon | Link |
|---|---|---|
| HVAC | `drawheating` | ⬜ /hvac.html |
| ECO Boiler | Custom boiler SVG (6 lagen) | ✅ /eco.html |
| Energie | `drawelectricenergy` | ✅ /epex-grafiek.html |
| Eetplaats | `drawhome` | ⬜ /room.html |
| Zon | `drawSunlight` | ⬜ later |
| Dashboard | `drawwifi` | ⬜ later |

### 4.5 NIVO 2a — ECO Boiler (eco.html) ✅

- Boiler SVG van Filip: 6 gekleurde lagen (HSL kleurschaal)
- Zonnecollector slang (rood→blauw gradient)
- Warmwaterslang — geanimeerd als pomp draait
- Temperatuurlabels rechts per laag
- Energie inhoud met voortgangsbalk (0→25 kWh)
- Collector temperatuur (Tsun) + ΔT badge
- PWM balk voor pompdebiet
- Laag tabel: kleurbolletje + mini-balk + °C
- Live via `/api/poll/eco` — demodata als offline

---

## 5. SVG iconen collectie (Filip Delannoy)

Beschikbaar als `-ALL-Icons-nightstyle.html` en `-ALL-Icons.html` in project.

**Alle functies:**
`drawalertsign` · `drawBreathingair` · `drawdust` · `drawelectricenergy` · `drawheating`
`drawhome` · `drawlighting` · `drawman` · `drawwoman` · `drawmemory` (5 niveaus)
`drawnightmoon` · `drawraindrop` · `drawshower` · `drawSolarheat` · `drawSolarPV`
`drawSunlight` · `drawtapwater` · `drawwifi` · `drawwoodfire` · `drawbedtime`
`drawLDR` · `drawHUMI` · `drawDEW` · `drawTEMP` · `drawCHESTALERT` · `drawCHESTLIGHT`
`drawTSTAT` · `drawWATERPUMP` · `drawRAINTANK` (5 niveaus)

**Kleurschaal universeel:**
- Koud/laag: `#4da6ff` (blauw)
- Goed/normaal: `#00d18c` (groen)
- Warm/let op: `#ffb030` (amber)
- Heet/alarm: `#ff4555` (rood)

---

## 5b. Room controllers & scenes

**Room controllers:** alle rooms hebben dezelfde sketch.
NVS variabelen bepalen: hoeveel pixels actief, nickname per pixel, kamer naam.
RPi haalt config op via /capabilities — geen hardcoded configuratie nodig.

**Scenes combineren lampen van meerdere room controllers:**
```json
{
  "naam": "Filmavond",
  "icoon": "TV",
  "acties": [
    { "controller": "room1", "key": "p0",      "waarde": 1  },
    { "controller": "room1", "key": "dim",      "waarde": 30 },
    { "controller": "room2", "key": "p0",       "waarde": 0  },
    { "controller": "hvac",  "key": "setpoint", "waarde": 20 }
  ]
}
```

**Voorbeeldscènes:**

| Scene | Wat er gebeurt |
|---|---|
| Filmavond | Salon 30% dim, TV licht aan, gang uit, slaapkamer uit |
| Diner | Tafel vol, salon 50%, keuken aan, rest uit |
| Ochtend | Slaapkamer aan, badkamer aan, rest uit |
| Nacht | Alles uit |
| Welkom | Gang aan, salon aan, rest uit |
| Lezen | Salon 80%, leeslamp aan, rest uit |

**Energiebeheer (ECO boiler, EVs, WPs, batterij)** wordt volledig beheerd door de
SENRG controller — het portal interfereert NOOIT met energieautomatisering.

**Automatisaties — enkel voor verlichting:**

| Trigger | Actie |
|---|---|
| Zonsondergang | Scene Avond |
| 23:00 | Scene Nacht |
| 07:00 weekdag | Scene Ochtend |
| Bewegingsdetectie | Scene Welkom |

**Geplande portal pagina's:**
1. Systeem overzicht (index.html) — alle controllers, online/offline status ✅
2. ECO Boiler detail (eco.html) ✅
3. EPEX energie grafiek (epex-grafiek.html) ✅
4. Matrix pagina — exacte replica 16×16 RGB matrix, live via /json van .60
5. Per controller rijke UI — automatisch gegenereerd vanuit /capabilities
6. Verlichting pagina — alle room controllers gecombineerd, scenes knoppen
7. Scenes en Automatisaties — visuele editor, log van uitvoeringen

**Publieke mapstructuur (doel):**
```
public/
├── index.html          ← Systeem overzicht ✅
├── eco.html            ← ECO Boiler detail ✅
├── epex-grafiek.html   ← Energie grafiek ✅
├── hvac.html           ← HVAC detail ⬜
├── room.html           ← Room detail ⬜
├── matrix/
│   └── index.html      ← 16×16 matrix replica ⬜
├── verlichting/
│   └── index.html      ← Gecombineerde verlichting ⬜
├── scenes/
│   └── index.html      ← Scenes en automatisaties ⬜
└── assets/
    ├── zarlar.css
    └── zarlar.js
```

---

## 6. EPEX grafiek

**Werkend in epex-grafiek.html:**
- EPEX spotprijzen + alle vaste kosten gestapeld
- Solar productie gesimuleerd (later S0 meting)
- Batterij laden/ontladen gesimuleerd
- NU lijn met actuele reële prijs
- Morgen label: alleen zichtbaar als morgen data beschikbaar
- Solar: volledige dag, toekomst 30% alpha

**Tariefvergelijking (instellingen tab):**
- Dynamisch: Geert Van Leuven, Fluvius Imewo, feb. 2026
- Vast: Filip Delannoy, Fluvius West, nov. 2025
- Vast totaal auto-berekend → voedt witte stippellijn

**Tarieven Fluvius Imewo (gebruikt voor beide):**

| Component | ct/kWh |
|---|---|
| Afnametarief | 5.23 |
| GSC | 1.10 |
| WKK | 0.39 |
| Heffingen | 4.94 |
| Energieprijs vast | 12.80 (instelbaar) |
| BTW | 6% |
| Abo + Cap | berekend op maandpiek |

**GitHub Pages testversie:**
`https://fideldworp.github.io/Zarlardinge_Dashboards/TEST_epex-grafiek.html`

---

## 7. ECO Boiler JSON keys

| Key | Naam | Eenheid |
|---|---|---|
| `b` | ETopH (bovenste sensor hoog) | °C |
| `c` | ETopL | °C |
| `d` | EMidH | °C |
| `e` | EMidL | °C |
| `f` | EBotH | °C |
| `g` | EBotL (onderste sensor laag) | °C |
| `h` | EAv (gemiddelde) | °C |
| `i` | EQtot (energie inhoud) | kWh |
| `j` | dEQ (delta energie) | kWh |
| `k` | yield_today | kWh |
| `l` | Tsun (collector) | °C |
| `m` | dT (Tsun - Tboil) | °C |
| `n` | PWM | 0-255 |
| `o` | Pomp aan/uit | 0/1 |
| `p` | RSSI | dBm |

---

## 8. Technische context

- **RPi:** Node.js v18 + Express + node-fetch@2
- **ESP32:** C6, Arduino IDE, `#define Serial Serial0` verplicht
- **ESPAsyncWebServer** — geen MQTT, geen Home Assistant
- **Nooit** `huge_app` partitie — gebruik `partitions_16mb.csv`
- **ECO gebruikt RSSI key** `p`, andere controllers `ac`
- **Dashboard gebruikt** `WebServer` (blocking), niet AsyncWebServer
- **Fonts portal:** DM Sans + DM Mono
- **Kleurthema:** donker, `--bg:#0a0d12`, `--card:#111620`

---

## 9. Fasering

| Fase | Wat | Status |
|---|---|---|
| 1 | RPi server + GitHub workflow + deploy automation | ✅ Klaar |
| 2 | EPEX grafiek + tariefvergelijking | ✅ Klaar |
| 3 | Tailscale remote toegang (overal) | ✅ Klaar |
| 4 | Portal NIVO 1: SVG cirkels, alle tabs | ✅ Klaar |
| 5 | ECO Boiler NIVO 2 pagina | ✅ Klaar |
| 6 | HVAC NIVO 2 pagina | ⬜ Open |
| 7 | Room NIVO 2 pagina | ⬜ Open |
| 8 | Lichten tab verfijnen (pixel nicknames via NVS) | ⬜ Open |
| 9 | Maarten + Céline uitnodigen op Tailscale | ⬜ Open |
| 10 | `/capabilities` endpoint op ESP32 controllers | ⬜ Later |
| 11 | Smart Energy controller — sketch v0.1 schrijven | ⬜ Later |
| 12 | Smart Energy — P1 dongle integratie (na digitale meter ~2028) | ⬜ Toekomst |

---

## 10. Openstaande actiepunten

| # | Actie |
|---|---|
| AP1 | HVAC NIVO 2 pagina bouwen |
| AP2 | Room NIVO 2 pagina bouwen |
| AP3 | NIVO 1: alarmen tonen als prioriteit |
| AP4 | Maarten uitnodigen op Tailscale |
| AP5 | Céline uitnodigen op Tailscale |
| AP6 | `/capabilities` endpoint op Room sketch |
| AP7 | `/capabilities` endpoint op HVAC sketch |
| AP8 | `/capabilities` endpoint op ECO sketch |
| AP9 | S-ENERGY: GPIO-pinnen toewijzen via OPTION RJ45 connector |
| AP10 | S-ENERGY: interface printje met pull-up + serieweerstanden bouwen |
| AP11 | S-ENERGY: sketch v0.1 schrijven (S0 pulstelling + LED matrix 12×4) |

---

## 11. Smart Energy Controller (S-ENERGY) — Hardware

> **Status: in ontwikkeling — sketch nog te schrijven.**
> IP: `192.168.0.73` · Board: ESP32-C6 32-pin

### 11.1 Energiemeters

| Meter | Label | Type | Locatie kast |
|---|---|---|---|
| Zonnepanelen | A14 | Inepro PRO380-S | Kast bij Maarten |
| Schuur | A5 | Inepro PRO380-S | Kast bij Maarten |

**S0 pulsrate:** `R_L = 0,1 Wh/imp` → **10.000 pulsen per kWh**
```cpp
#define PULSEN_PER_KWH 10000
// 1 puls = 0,1 Wh
float wh = (float)pulsTeller / PULSEN_PER_KWH * 1000.0;
// Vermogen (W) via interval tussen pulsen (ms):
float watt = 0.1 / (intervalMs / 3600000.0);
```

### 11.2 S0 kanalen

| Kanaal | Meter | Klemmen | Richting | GPIO |
|---|---|---|---|---|
| S0-1 | Zonnepanelen (A14) | 18/19 | Forward (productie) | nader te bepalen |
| S0-2 | Schuur (A5) | 18/19 | Forward (afname) | nader te bepalen |
| S0-3 | Schuur (A5) | 20/21 | Reverse (injectie) | nader te bepalen |

**WON-verbruik:** niet gemeten in fase 1 (nog analoge meter, uitzondering exclusief nachttarief).
JSON key `b` = 0 tot digitale meter beschikbaar (~2028). Dan via P1-dongle.

### 11.3 S0 interfaceschema (per kanaal, 3×)

```
3,3V
 |
[4,7kΩ pull-up]
 |
 +──────────── GPIO (INPUT, interne pull-up uit)
 |
[1kΩ serie]
 |
Klem 18 of 20 (S0+) ←── 5V extern (gedeeld met ESP32 voeding)
Klem 19 of 21 (S0−) ──── GND (gedeeld met ESP32 GND)
```

Geen optocoupler — S0-uitgang van PRO380-S is zelf al optisch geïsoleerd.
Verbinding via **OPTION RJ45** connector op het Zarlar shield.

### 11.4 LED matrix

**Hardware:** 12×4 WS2812B matrix = **48 pixels**, serpentine datavolgorde.
Connector: JST SM 3-pin (wit=DI, rood=+5V, blauw=GND).
Voeding: aparte 5V (niet via shield PTC).

| Pixel | Functie | Kleurlogica |
|---|---|---|
| 1 | ☀️ Solar vermogen | Uit→geel dim→groen helder |
| 2 | 💰 EPEX prijs | Groen=goedkoop / geel=normaal / rood=duur |
| 3 | ⚖️ Netto balans | Groen=injectie / rood=afname |
| 4 | 🔋 Batterij (toekomst) | SOC kleurschaal |
| 5 | ♨️ ECO boiler | Groen=aan / zwart=uit |
| 6 | 🚙 EV WON | Groen gradient op laadvermogen |
| 7 | 🚗 EV SCH | Idem |
| 8 | 🏠 WP WON | Groen=aan / zwart=uit |
| 9 | 🏚️ WP SCH | Groen=aan / zwart=uit |
| 10 | 🍳 Koken? | Groen=goed moment / rood=duur of piek |
| 11 | 👕 Wassen? | Zelfde logica |
| 12 | 📊 Piek | Groen→geel→oranje→rood vs MAX_PIEK |

*Pixels 10–11 voor Céline en Mireille: groen = goed moment om te koken/wassen.*

---

*Zarlar Portal Projectdocument — Filip Delannoy — april 2026*
