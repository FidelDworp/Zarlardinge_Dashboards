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
| 11 | Smart Energy controller + S0 meting | ⬜ Later |

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

---

*Zarlar Portal Projectdocument — Filip Delannoy — april 2026*
