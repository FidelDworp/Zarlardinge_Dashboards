# Zarlar Portal — Projectdocument
*April 2026 — Filip Delannoy (FiDel)*

---

## 1. Wat is gerealiseerd (april 2026)

### 1.1 Infrastructuur RPi

| Component | Detail |
|---|---|
| Hardware | Raspberry Pi, vaste IP `192.168.0.50` |
| Software | Node.js + Express, poort 3000 |
| Autostart | systemd `zarlar.service` |
| SSH toegang | `ssh fidel@192.168.0.50` (wachtwoord: zarlar) |
| Remote toegang | Tailscale VPN — `http://100.123.74.113:3000` |

### 1.2 GitHub workflow

**Repository:** `FidelDworp/Zarlardinge_Dashboards/ESP32_Zarlar/zarlar-rpi/`

**Mapstructuur op GitHub:**
```
zarlar-rpi/
├── server.js          ← Node.js server
├── update.sh          ← RPi sync script
├── deploy.sh          ← Mac deploy script (lokaal op Mac, niet in repo)
├── README.md
└── public/
    ├── index.html     ← Portal overzicht (NIVO 1)
    ├── zarlar.css     ← Gedeelde stijl
    ├── zarlar.js      ← Gedeelde functies
    └── epex-grafiek.html
```

**Deploy workflow (alles in één commando vanuit Mac Terminal):**
```bash
bash ~/deploy.sh "omschrijving van wijziging"
```

`deploy.sh` detecteert bestanden in Downloads, kopieert naar juiste map
(`.html/.css/.js` → `public/`, `.sh/.js` rootbestanden → root),
doet git commit + push, triggert RPi update, ruimt Downloads op.

**RPi update:**
```bash
# Automatisch via deploy.sh, of manueel:
ssh fidel@192.168.0.50 'bash /home/fidel/update.sh'
# Of via alias op Mac:
zarlar-update
```

### 1.3 Remote toegang via Tailscale

| Apparaat | Tailscale IP | Status |
|---|---|---|
| RPi | `100.123.74.113` | ✅ Online, autostart |
| MacBook | `100.89.205.22` | ✅ Online |
| iPhone (Filip + Mireille) | `100.104.215.18` | ✅ Online |
| Maarten | — | ⬜ Nog uit te nodigen |
| Céline | — | ⬜ Nog uit te nodigen |

**Uitnodigen:** Tailscale dashboard → Add device → Share → e-mailadres
**Gratis plan:** 3 gebruikers (Filip/Mireille = 1 account, Maarten, Céline)

### 1.4 Werkende pagina's

| URL | Pagina | Status |
|---|---|---|
| `http://100.123.74.113:3000/` | Portal overzicht | ✅ Werkend |
| `http://100.123.74.113:3000/epex-grafiek.html` | EPEX energie grafiek | ✅ Werkend |

### 1.5 server.js v2.0 — API endpoints

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

### 1.6 EPEX grafiek — fixes april 2026

- **NU lijn:** filter op vandaag 00:00 verwijderd — data intact doorgeven
- **Solar grafiek:** volledige dag tonen, toekomst 30% alpha
- **Morgen label:** alleen tonen als morgen data beschikbaar is
- **Cache:** 3 vernieuwingscondities:
  1. Ouder dan 26 uur
  2. Na 13:00 én morgen data nog niet beschikbaar
  3. Dag gewisseld (cache van gisteren)
- **NU prijs indicator:** totale reële prijs bij NU lijn onderaan grafiek
- **Tariefvergelijking:** Dynamisch (Geert/Imewo/feb.2026) vs Vast (Filip/West/nov.2025)
  in instellingen tab — vast contract auto-berekend uit componenten

---

## 2. Architectuur — controllers & data

### 2.1 Netwerk

```
192.168.0.50  → RPi Zarlar Portal (Node.js)
192.168.0.60  → ESP32 Dashboard (bron van waarheid, matrix 16×16)
192.168.0.70  → ESP32 HVAC
192.168.0.71  → ESP32 ECO Boiler
192.168.0.73  → ESP32 Smart Energy (in ontwikkeling)
192.168.0.80  → ESP32 Room (Testroom/Eetplaats)
192.168.0.75–.81 → Toekomstige Room controllers
Photon controllers → via Cloudflare Worker (legacy, transitie)
```

### 2.2 Dataflow

```
Browser (overal via Tailscale)
       ↓ /api/...
RPi 192.168.0.50
       ↓ lokaal HTTP
ESP32 controllers (192.168.0.xx)
```

**Gouden regel:** Browser gebruikt NOOIT lokale IPs — alles via `/api/` op RPi.

### 2.3 JSON schemas (samenvatting)

Zie Zarlar_Master_Overnamedocument.md §4–§8 voor volledige schemas.

---

## 3. Portal visie & architectuur

### 3.1 Filosofie

**Apple-like · KISS · Grafisch-eerst**

- Zo weinig mogelijk tekst, zo veel mogelijk visuele beelden
- Werkt perfect op telefoon én desktop
- Intuïtief voor iedereen in het gezin zonder uitleg
- Donker thema (consistent met bestaande EPEX pagina)
- SVG symbolen als visuele taal

### 3.2 Drie lagen

```
┌─────────────────────────────────────────────────────┐
│  NIVO 1 — Synoptisch overzicht                       │
│  Donker canvas · SVG cirkels per controller          │
│  [💡 Lichten] [🌡️ HVAC] [🔒 Security] tabs          │
└──────────────┬──────────────────────────────────────┘
               │ klik op cirkel of tab
┌──────────────▼──────────────────────────────────────┐
│  NIVO 2a — Controller detail                         │
│  Grote SVG visualisatie + cijfers + instellingen     │
├─────────────────────────────────────────────────────┤
│  NIVO 2b — Bedieningen (tabs)                        │
│  💡 Lichten  🌡️ HVAC  🔒 Security                   │
└─────────────────────────────────────────────────────┘
```

### 3.3 NIVO 1 — Synoptisch overzicht

Vervangt de huidige `index.html` volledig.

**Layout:** Donker canvas met per controller een **klikbare cirkel** met SVG symbool.

| Controller | SVG symbool | Dynamische kleurlogica |
|---|---|---|
| HVAC | `heating` (radiator) | Blauw→rood op gem. boilertemp |
| ECO Boiler | Boiler SVG (6 lagen) | Elke laag blauw→rood op temp |
| Room (per kamer) | `home` + `temp` | Kleur = kamertemperatuur |
| Smart Energy | `electricenergy` + `SolarPV` | Groen=overschot, rood=afname |
| Dashboard | Miniatuur 4×4 matrix | Live kleuren van echte matrix |
| WiFi/status | `wifi` | Groen/oranje/rood op RSSI |

**Bovenaan:** drie horizontale tab-knoppen voor NIVO 2b:
- 💡 **Lichten** — verlichting per kamer + scènes
- 🌡️ **HVAC** — verwarmingsoverzicht
- 🔒 **Security** — beweging, aanwezigheid, Home/Weg

**Onderaan elke cirkel:** naam van de controller, klein en subtiel.

**Statusindicator:** kleine gekleurde stip (groen/geel/rood) per cirkel.

### 3.4 NIVO 2a — Controller detail pagina's

Per controller een eigen pagina met:

**ECO Boiler (`/controllers/eco/`):**
- Grote SVG boilertekening met 6 benoemde lagen
- Elke laag kleurt van blauw (#4a9eff) → groen (#00c896) → oranje (#ffb830) → rood (#ff4558)
- Collector temperatuur (Tsun) als apart element
- Pomppijl/indicator — animatie als pomp draait
- Cijfers subtiel erbij: °C per laag, EQtot kWh, PWM%
- Onderaan: instellingen (drempelwaarden, tijden)

**HVAC (`/controllers/hvac/`):**
- Plattegrond-stijl: 7 verwarmingskringen als rechthoeken
- Per kring: kleur = aan/uit, balk = duty 4u%
- Ventilatiepijl met snelheid
- 2 pompindicatoren (SCH/WON)
- Boilertemperatuur als horizontale gelaagde balk

**Room (`/controllers/room/`):**
- Kameroverzicht met pixels als gekleurde bolletjes
- Temperatuur + vochtigheid + CO2 als grote getallen
- Bewegingsindicatoren (MOV1/MOV2) — pulseren bij beweging
- Dag/nacht indicator (`nightmoon` / `Sunlight`)
- Dauwpuntalert (`dewpoint`)

**Smart Energy (`/controllers/senrg/`):**
- Huidige EPEX grafiek (`epex-grafiek.html`) verhuist hierheen
- Aangevuld met live S0 metingen (later)

### 3.5 NIVO 2b — Bedieningen tabs

**Tab 💡 Lichten:**
- Bovenaan: scène-knoppen (Filmavond, Diner, Nacht, Welkom, Lezen...)
- Daaronder: kamers als groepen (Apple Home stijl)
- Per kamer: klikbare lichtpunten (cirkels met naam)
- Kleur van lichtpunt = actuele RGB kleur van de pixel
- Klik = toggle aan/uit

**Tab 🌡️ HVAC:**
- Setpoint per kamer instelbaar (slider)
- Home/Weg per kamer of globaal
- Ventilatie slider
- Override per circuit (aan/uit/auto)

**Tab 🔒 Security:**
- Bewegingssensoren per kamer — `man/woman` icoon pulseert bij beweging
- Dag/nacht status per kamer
- Globale Home/Weg schakelaar (broadcast naar alle rooms)
- `home` icoon toont HOME/UIT status

### 3.6 SVG strategie

**Bibliotheek:** Bestaande collectie van Filip (nightstyle + backup versies)

**Beschikbare iconen:**
`alertsign` · `Breathingair` (ventilatie) · `dust` · `electricenergy` · `heating` (radiator)
`home` · `lighting` (lamp) · `man` · `woman` · `memory` (gelaagd, 5 niveaus)
`nightmoon` · `raindrop` (vochtigheid) · `shower` · `Solarheat` · `SolarPV`
`Sunlight` · `tapwater` · `wifi` · `woodfire` · `bedtime` · `roomlight`
`vapour` · `dewpoint` · `temp` (thermometer) · `chestalert` · `chestlight`
`tstat` (thermostaat) · `waterpump` · `raintank` (gelaagd, 5 niveaus)

**Gelaagde iconen (dynamisch inkleuren per niveau):**
- `memory` — 5 niveaus → heap visualisatie
- `raintank` — 5 niveaus → regenwaterreservoir of boilerlaag indicatie

**Technische aanpak:**
- SVG inline in HTML plaatsen (niet als `<img>`)
- Elk inkleurbaal onderdeel krijgt een uniek `id`
- JavaScript past `fill` aan op basis van sensordata
- Kleurschaal universeel:
  - Koud/laag: `#4a9eff` (blauw)
  - Goed/normaal: `#00c896` (groen)
  - Warm/let op: `#ffb830` (amber)
  - Heet/alarm: `#ff4558` (rood)
- SVG schaalt via `width` en `height` parameters — geen kwaliteitsverlies

---

## 4. Fasering

| Fase | Wat | Prioriteit |
|---|---|---|
| **1** | NIVO 1 redesign: SVG cirkels, synoptisch | Hoog |
| **2** | ECO Boiler detail pagina met gelaagde boiler SVG | Hoog |
| **3** | Room detail pagina | Hoog |
| **4** | Tab Lichten (Apple Home stijl) | Hoog |
| **5** | HVAC detail pagina | Middel |
| **6** | Tab HVAC bedieningen | Middel |
| **7** | Tab Security | Middel |
| **8** | Smart Energy detail (EPEX verhuizen) | Laag |
| **9** | Maarten + Céline uitnodigen op Tailscale | Laag |
| **10** | /capabilities endpoint op ESP32 controllers | Later |

---

## 5. Openstaande actiepunten

| # | Actie | Status |
|---|---|---|
| AP1 | SVG boilertekening aanmaken (6 inkleurbale lagen) | ⬜ Open |
| AP2 | NIVO 1 redesign uitwerken | ⬜ Open |
| AP3 | Maarten uitnodigen op Tailscale | ⬜ Open |
| AP4 | Céline uitnodigen op Tailscale | ⬜ Open |
| AP5 | `/capabilities` endpoint op Room sketch | ⬜ Later |
| AP6 | `/capabilities` endpoint op HVAC sketch | ⬜ Later |
| AP7 | `/capabilities` endpoint op ECO sketch | ⬜ Later |

---

## 6. Technische context

- **RPi:** Node.js v18 + Express + node-fetch@2
- **ESP32:** C6, Arduino IDE, ESPAsyncWebServer
- **Fonts portal:** Syne (UI) + IBM Plex Mono (data)
- **Kleurthema:** zie `zarlar.css` CSS variabelen
- **SVG iconen:** collectie Filip Delannoy (nightstyle versie)
- **EPEX data:** energy-charts.info via RPi server (geen CORS probleem)
- **Tailscale:** gratis plan, 3 accounts, onbeperkt apparaten per account

---

*Zarlar Portal Projectdocument — Filip Delannoy — april 2026*
