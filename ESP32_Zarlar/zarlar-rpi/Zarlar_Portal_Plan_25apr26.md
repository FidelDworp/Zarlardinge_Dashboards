# Zarlar Portal — Projectdocument
*Bijgewerkt 25 april 2026 — Filip Delannoy (FiDel)*

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

**Injectietarief (teruglevering aan het net):**

Bij het **dynamisch Ecopower-tarief** wordt injectie vergoed aan de EPEX-marktprijs
minus een onbalanstoeslag ≈ **5,25 ct/kWh gemiddeld** (feb. 2026). Injectie aan 0% BTW.
Bij negatieve EPEX-prijzen wordt er géén vergoeding betaald — dan kost injectie zelfs geld.

Bij het **vast Ecopower-tarief** geldt een vaste injectievergoeding van ca. **5,0 ct/kWh**,
ongeacht het uur of de marktprijs.

> ⚠️ In beide gevallen is de injectievergoeding veel lager dan de afnameprijs.
> Een thuisbatterij is pas financieel interessant na de komst van de digitale meter (~2028),
> omdat daarna de saldering stopt en injectie niet langer 1-op-1 verrekend wordt.

**Injectieteller (nieuw v2.0 epex-grafiek.html):**
- Derde info-kaart: "☀️ Injectie vandaag" — toont kWh + € opbrengst
- Prefereert live S-ENERGY data (`/api/poll/senrg`, key `k`) als beschikbaar en < 90s oud
- Valt terug op simulatieberekening — toont badge `LIVE` of `SIM`
- € berekend per EPEX-slot: injectieprijs = EPEX − onbalansafslag (instelbaar, standaard 0,67 ct/kWh)

**Tarieftabel (nieuw derde kolom "INJECTIE"):**
- Energieprijs: EPEX spotprijs (zelfde als dynamisch)
- Afnametarief, GSC, WKK, heffingen: niet van toepassing (0%)
- BTW: 0% (conform factuur — tegenover 6% op afname)
- Onbalansafslag: instelbaar via nieuw veld (standaard 0,67 ct/kWh)
- Totaal: EPEX − onbalansafslag ct/kWh aan 0% BTW

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

**server.js v2.0:**
- Alle room controllers toegevoegd (room75–room81, 192.168.0.75–.81)
- Dashboard controller toegevoegd (192.168.0.60)
- `/api/photon/:id` proxy endpoint — Cloudflare Worker via RPi (geen CORS issues)
- `/api/matrix` endpoint — haalt alle ESP32 + Photon data parallel op in één call
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
| 11 | Smart Energy controller — sketch v0.1 schrijven | ✅ Klaar (v1.25 SIM) |
| 12 | S-ENERGY: maandelijkse pieken loggen naar stats bestand op RPi | ⬜ Later |
| 13 | Afrekenpagina WON/SCH zichtbaar op portal voor Maarten en Céline | ⬜ Later |
| 14 | S-ENERGY: echte S0 bekabeling + overschakelen van SIM naar LIVE | ⬜ Later |
| 15 | Smart Energy — P1 dongle integratie (na digitale meter ~2028) | ⬜ Toekomst |
| 16 | Thuisbatterij(en) — beslissing + integratie in sturing (~2028) | ⬜ Toekomst |

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
| AP11 | S-ENERGY: sketch v1.26 flashen op ESP32-C6 (192.168.0.73) |
| AP12 | S-ENERGY: SIM_S0 + SIM_P1 valideren via /json badges op epex-pagina |
| AP13 | S-ENERGY: maandpiek per huis loggen (ps, pw, pt) naar stats-bestand RPi |
| AP14 | Portal: afrekenpagina WON/SCH bouwen (zie §13) |
| AP15 | S-ENERGY: S0 bekabeling aansluiten → sim_mode uitschakelen → v1.26 LIVE |
| AP16 | S-ENERGY: WON afname + injectie toevoegen via P1-dongle (~2028) |

---

## 11. Smart Energy Controller (S-ENERGY) — Hardware

> **Status: sketch v1.25 SIM beschikbaar — klaar om te flashen.**
> IP: `192.168.0.73` · Board: ESP32-C6 32-pin

### 11.0 Versiehistorie sketch

| Versie | Status | Wijziging |
|---|---|---|
| v1.24 | Archief | Eerste productieversie — live S0 ISR |
| v1.25 SIM | Archief | Enkelvoudige SIMULATION_MODE — vervangen door v1.26 |
| **v1.26** | **Actief** | Twee onafhankelijke simulatievlaggen SIM_S0 + SIM_P1 · HomeWizard P1 integratie |

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

| Kanaal | Meter | Klemmen | Richting | GPIO | Inhoud |
|---|---|---|---|---|---|
| S0-1 | Zonnepanelen (A14) | 18/19 | Forward | IO3 | Solar productie |
| S0-2 | Schuur (A5) | 18/19 | Forward | IO5 | SCH afname van net |
| S0-3 | Schuur (A5) | 20/21 | Reverse | IO6 | SCH injectie naar net (incl. batterij later) |

> ⚠️ **S0-3 toekomstige uitbreiding:** zodra SCH een thuisbatterij plaatst,
> registreert S0-3 niet alleen zonnepaneeloverschot maar ook batterijontlading
> richting het net. Dit kanaal moet dan als gecombineerde "export SCH" beschouwd
> worden — de opsplitsing solar vs. batterij gebeurt op basis van vermogensvergelijking.

**WON-verbruik:** niet gemeten in fase 1 (nog analoge meter).
JSON key `b` = 0 tot digitale meter beschikbaar (~2028). Dan via P1-dongle op het netwerk.

**WON toekomstige meting via P1-dongle (~2028):**
Wanneer WON een digitale meter krijgt, plaatst een P1-dongle de verbruiksdata op het
lokale netwerk. De RPi leest deze data en registreert:
- WON afname van net (kWh en momentaan vermogen)
- WON injectie naar net (kWh — incl. eventuele thuisbatterij)

### 11.3 Simulatiemodi — twee onafhankelijke vlaggen (v1.26)

De sketch bevat twee **volledig onafhankelijke** simulatievlaggen, elk apart instelbaar via `/settings` of serieel commando. **Nooit automatisch omschakelen** — elke overgang is een bewuste handeling om valse data in de logs te voorkomen.

| Vlag | Default | Betekenis | Omschakelen naar LIVE |
|---|---|---|---|
| `SIM_S0` | `true` | Simuleert 3× S0-kanalen (solar, SCH afname, SCH injectie) | Na S0-bekabeling: checkbox uit → reboot |
| `SIM_P1` | `true` | Simuleert P1-dongle (WON afname + injectie) | Na HomeWizard plaatsing (~2028): checkbox uit + P1-IP invullen → reboot |

**Serieel commando's:** `sim s0 on/off` · `sim p1 on/off` · `status` · `help`

**In `/json`:** `"sim_s0":1` en `"sim_p1":1` als aparte indicatoren — de RPi en epex-pagina tonen per bron de status (badge `S0:SIM · P1:SIM`, `S0:LIVE · P1:SIM`, enz.)

**Simulatieprofiel S0 (april, België):**
- Solar: sinus-curve 07:00–19:00, piek 4000 W, ±10% ruis (bewolking)
- SCH afname: 500 W basis + ochtendspits (+1800 W), middag (+600 W), avondspits (+2200 W), nacht (+800 W)
- SCH injectie: max(0, solar − afname) → overschot zonne-energie

**Simulatieprofiel P1 (april, België):**
- WON afname: 400 W basis + ochtend-/avondspits, geen eigen productie in fase 1
- WON injectie: 0 Wh (geen solar bij WON in simulatie)

**Matrix-indicator:** col 0 rij 0 knippert rood als SIM_S0 actief · col 1 rij 0 als SIM_P1 actief · groen sweep bij boot als beide LIVE

### 11.4 HomeWizard P1 Meter — HWE-P1-RJ12

**Hardware:**
- Model: HomeWizard P1 Meter HWE-P1-RJ12
- Aansluiting: RJ12 op P1-poort digitale slimme meter (WON, na ~2028)
- Voeding: 5V 500mA via P1-poort zelf (geen externe voeding nodig)
- WiFi: 2.4 GHz, verbindt via HomeWizard Energy app

**Activatie lokale API:**
1. Installeer de HomeWizard Energy app
2. Koppel de P1 Meter aan je WiFi-netwerk
3. Ga naar: Settings → Meters → jouw meter → **Local API: AAN**
4. Noteer het IP-adres en vul dit in bij S-ENERGY `/settings` → P1 IP-adres

**API endpoint:**
```
GET http://<P1_IP>/api/v1/data
```
Plain HTTP, geen authenticatie, geen cloud vereist. Documentatie: https://api-documentation.homewizard.com/docs/introduction/

**JSON response (relevante velden voor S-ENERGY):**
```json
{
  "smr_version": 50,
  "active_power_w": 997,
  "total_power_import_t1_kwh": 19055.287,
  "total_power_import_t2_kwh": 19505.815,
  "total_power_export_t1_kwh": 0.002,
  "total_power_export_t2_kwh": 0.007
}
```

| P1 JSON key | Betekenis | → S-ENERGY /json key |
|---|---|---|
| `active_power_w` | Momentaan vermogen WON (+ = afname, − = injectie) | `b` (W) |
| `total_power_import_t1_kwh + t2_kwh` | Cumulatieve afname (dag via delta midnight) | `i` (Wh) |
| `total_power_export_t1_kwh + t2_kwh` | Cumulatieve injectie (dag via delta midnight) | `vw` (Wh) |

**Update-frequentie:** elke seconde bij DSMR 5.0, elke 10s bij oudere meters. S-ENERGY pollt elke 5s — ruim voldoende.

**GitHub library (referentie, niet gebruikt in sketch):**
https://github.com/jvandenaardweg/homewizard-energy-api
(Node.js, type-safe, toont volledige JSON structuur en polling aanpak)

**Artikel integratie energiemanagementsysteem:**
https://engineering360.nl/homewizard-p1-meter-koppelen-aan-je-energiemanagement

> 💡 **Nota:** `total_power_import/export_kwh` zijn **cumulatieve tellers** die nooit resetten. De sketch berekent dagcumulatieven via een delta tov een midnight-snapshot — identiek aan hoe een klassieke teller werkt.

### 11.5 S0 interfaceschema (per kanaal, 3×)

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

## 12. Energiemeting & Statistieken — Overzicht

### 12.1 Wat er gemeten en bijgehouden wordt

| Grootheid | Bron | Fase | JSON key |
|---|---|---|---|
| Solar productie | S0-1 | Nu | `a` (W), `h` (Wh dag) |
| SCH afname van net | S0-2 | Nu | `c` (W), `j` (Wh dag) |
| SCH injectie naar net | S0-3 | Nu | `d` (W), `k` (Wh dag) |
| SCH batterij ontlading naar net | S0-3 (zelfde kanaal) | Na batterijplaatsing | idem |
| WON afname van net | P1-dongle HomeWizard | ~2028 | `b` (W), `i` (Wh dag) |
| WON injectie naar net | P1-dongle HomeWizard | ~2028 | `vw` (Wh dag) |
| Gecombineerde maandpiek | berekend | Nu | `pt` (W) |
| Maandpiek SCH individueel | berekend | Nu | `ps` (W) — TODO v1.27 |
| Maandpiek WON individueel | P1-dongle | ~2028 | `pw` (W) — TODO v1.27 |
| S0 simulatie actief | vlag | Nu | `sim_s0` (0/1) |
| P1 simulatie actief | vlag | Nu | `sim_p1` (0/1) |

### 12.2 Statistieken opslaan op RPi

De RPi slaat maandelijks de hoogste pieken op in een lokaal JSON-bestand
(`stats-energie.json`). Dit is de basis voor de onderlinge afrekening (zie §13).

```json
{
  "2026-04": {
    "piek_gecombineerd_kw": 12.4,
    "piek_sch_kw": 9.1,
    "piek_won_kw": 6.2,
    "kwh_solar": 420,
    "kwh_sch_afname": 310,
    "kwh_sch_injectie": 185,
    "kwh_won_afname": 0,
    "kwh_won_injectie": 0
  }
}
```

---

## 13. Onderlinge Afrekening WON / SCH

> **Voor Maarten en Céline — hoe werkt dit?**

### 13.1 De situatie uitgelegd

WON (Maarten & Céline) en SCH (Filip & Mireille) gebruiken **één gezamenlijke
elektriciteitsaansluiting en één factuur** bij Ecopower. De kosten worden achteraf
intern verrekend op basis van gemeten verbruik en een eerlijke verdeelsleutel.

### 13.2 Wat er op de factuur staat

Elke maand betaalt één persoon de volledige Ecopower-factuur. Die factuur bestaat uit:

1. **Energiekost** — afhankelijk van hoeveel kWh jullie samen afgenomen hebben,
   tegen de dynamische EPEX-prijs (of vaste prijs).
2. **Injectievergoeding** — wat jullie terugkrijgen voor zonnepanelen die terugleveren
   aan het net. Dit verlaagt de factuur.
3. **Capaciteitstarief** — een maandelijks bedrag per kW gebaseerd op de **hoogste
   gecombineerde piek** van die maand (minimum 2,5 kW).
4. **Abonnement + databeheer** — vaste kosten: ~€6,49/maand.

### 13.3 Hoe de kosten verdeeld worden

**Energiekost en injectievergoeding:**
Elk huis betaalt naar verbruik. De S-ENERGY controller meet dit continu:
- SCH afname en injectie: via S0-kanalen op de Inepro-meter
- WON afname en injectie: via P1-dongle (beschikbaar na digitale meter ~2028)

**Abonnement + databeheer:**
Wordt 50/50 gedeeld — dit is een vaste kost ongeacht verbruik.

**Capaciteitstarief — de eerlijke verdeelsleutel:**

Het capaciteitstarief is gebaseerd op de hoogste piek van de maand.
De RPi houdt bij welk huis welke piek veroorzaakte, en verdeelt de kost proportioneel:

```
Aandeel SCH = Piek SCH ÷ (Piek SCH + Piek WON)
Aandeel WON = Piek WON ÷ (Piek SCH + Piek WON)
```

**Voorbeeld voor april 2026:**

| | SCH | WON |
|---|---|---|
| Individuele maandpiek | 9,1 kW | 6,2 kW |
| Aandeel capaciteitskost | 59% | 41% |
| Capaciteitskost totaal (bv. €55,78) | €32,91 | €22,87 |

> 💡 **Waarom is dit eerlijk?** Wie zijn verbruik beter spreidt (bijv. door
> slim laden van de EV of batterijsturing), heeft een lagere individuele piek
> en betaalt dus minder capaciteitskosten. Goed gedrag wordt beloond.

### 13.4 Maandelijkse afrekeningsrapport op de portal

De RPi genereert automatisch een overzichtspagina (`/afrekening`) met:

- Gemeten kWh per huis (afname en injectie)
- Berekende energiekost per huis
- Hoogste individuele piek per huis die maand
- Capaciteitskost verdeling (met % per huis)
- **Totaalbedrag WON** en **totaalbedrag SCH**
- Saldo: wie betaalt wie, hoeveel

Deze pagina is zichtbaar voor iedereen op het Tailscale-netwerk
(Filip, Mireille, Maarten, Céline) via `http://100.123.74.113:3000/afrekening`.

### 13.5 Tijdlijn

| Periode | Wat beschikbaar |
|---|---|
| Nu → ~2028 | SCH volledig gemeten · WON nog schatten of manueel ingeven |
| Na digitale meter WON (~2028) | Alles automatisch en volledig gemeten |
| Na thuisbatterij(en) | Batterijbijdrage zichtbaar in statistieken |

> 📌 **Nota voor de periode vóór 2028:** Zolang WON nog een analoge meter heeft,
> kan het WON-verbruik geschat worden op basis van de gecombineerde meting minus
> het gemeten SCH-verbruik. Dit geeft een goede benadering maar is geen exacte meting.
> Beide partijen stemmen in met deze benadering tot de digitale meter beschikbaar is.

---

## 14. Thuisbatterij — Strategie & Timing

### 14.1 Waarom nu nog niet

Zolang SCH en WON een **analoge terugdraaiende teller** hebben, geldt
volledige **saldering**: elke kWh die teruggeleverd wordt, vermindert de
afname 1-op-1 tegen het volledige retailtarief. Het net is dan een gratis
virtuele batterij met 100% rendement en zonder investering.

Een fysieke thuisbatterij voegt in die situatie weinig financieel voordeel toe
— de terugverdientijd (€6.000–€10.000 investering) is te lang.

### 14.2 Wanneer wél zinvol (~2028)

Zodra de analoge meter vervangen wordt door een **digitale meter** stopt de
saldering. Injectie wordt dan vergoed aan slechts ~5 ct/kWh i.p.v. het volle
retailtarief. Dan is een thuisbatterij wél interessant:
- Zelfverbruik verhogen (solar opslaan en 's avonds gebruiken)
- EPEX-arbitrage: goedkoop laden, duur ontladen
- Piekafvlakking voor lager capaciteitstarief

### 14.3 Gedeelde batterij vs. aparte batterijen

**Twee aparte batterijen (één per huis):**
- Elk huis optimaliseert onafhankelijk
- Eenvoudiger interne afrekening
- Geen discussie over wie wanneer laadt

**Één gedeelde batterij (op de gemeenschappelijke teller):**
- Schaalvoordeel: grotere batterij = goedkoper per kWh
- Hogere benutting door gecombineerd verbruiksprofiel
- Piekbeheersing op de gedeelde teller is efficiënter
- Maar: complexere regeling en interne verrekening

> 💡 **Aanbeveling:** beslissing te nemen rond 2028 op basis van de dan
> beschikbare meetdata en tarieven. De S-ENERGY controller en het portal
> zijn al ontworpen om beide scenario's te ondersteunen.

### 14.4 Infrastructuur al klaar

- S0-3 kanaal (IO6) registreert al reverse-pulsen van SCH
- JSON keys `pw`, `ps`, `pt` voor individuele en gecombineerde pieken bestaan al
- EPEX-arbitrage simulatie draait al in de grafiekpagina
- Piekbeheer-algoritme in de EPEX-pagina houdt al rekening met batterij

---

*Zarlar Portal Projectdocument — Filip Delannoy — 25 april 2026*
