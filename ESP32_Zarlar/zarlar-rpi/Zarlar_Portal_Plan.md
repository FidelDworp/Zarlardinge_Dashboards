# Zarlar Portal — Projectdocument
*Bijgewerkt 18 oktober 2026 — Filip Delannoy (FiDel)*

> **Rolverdeling documenten:**
> - Dit document: alles op de **RPi** (server, pagina's, deploy, Tailscale, Matter-bridge §16, energiebeslissingen §17, tarieven, afrekening, batterij).
> - `ESP32_Zarlar/Zarlar_Master_Overnamedocument.md`: systeemoverzicht en alle **ESP32-controllers**.
>
> **Review 16 oktober 2026.** De punten V1–V4 van 15 okt zijn opgelost op basis van de sketches:
> - **V1:** S0 op IO5/IO6/IO7, LED op IO4 (§11).
> - **V2:** `pw`/`ps` bestaan nog niet (§12, §14).
> - **V3:** de RPi bewaart instellingen, keuzes en stats, en neemt energiebeslissingen. Google Sheets-logging blijft op het Dashboard (§2.1).
> - **V4:** Dashboard v5.9, ROOM v2.21.
>
> Nieuw: §17 Energiebeslissingen + Tesla bridge.
>
> **17 oktober 2026:** Tailscale-sleutel verlopen en hersteld; de Matterbridge is verhuisd naar de woning **Zarlardinge**; de oude HomeSpan-bridge voor de Photons is gewist. Daarom start F4 nu met de **Photon-toestellen** (§16.8, §16.9).
>
> **18 oktober 2026:** de **ECO-boiler heeft wél Matter** (5 endpoints sinds v1.18) — rechtgezet in §16.1, §16.3 en §16.11 (bron: sketch v1.23, Master §5.4). HVAC-plan zonder Matter staat in `ESP32-HVAC/README.md` §10.

---

## 1. Infrastructuur & toegang

### 1.1 RPi server

| Component | Detail |
|---|---|
| Hardware | Raspberry Pi 3B+ (±1 GB RAM, microSD), vaste IP `192.168.0.50` |
| OS | Debian 13 (Trixie), aarch64 |
| Software | Node.js + Express, poort 3000 (v18 in april 2026; actuele versie checken met `node -v`, zie §16 F0) |
| Repo lokaal | `/home/fidel/repo/ESP32_Zarlar/zarlar-rpi/` |
| Public map | `/home/fidel/repo/ESP32_Zarlar/zarlar-rpi/public/` |
| Autostart | systemd `zarlar.service` ✅ |
| SSH lokaal | `ssh fidel@192.168.0.50` (wachtwoord: niet in dit publieke document) |
| SSH overal | `ssh fidel@100.123.74.113` (via Tailscale) |

### 1.2 GitHub repository

**Repo:** `FidelDworp/ZarlarApp/ESP32_Zarlar/zarlar-rpi/` (oude naam `Zarlardinge_Dashboards` stuurt door; lokale map op de Mac in `deploy.sh` heet nog `~/Zarlardinge_Dashboards`)

> ⚠️ Na de hernoeming wijzigt het GitHub Pages-adres naar `fideldworp.github.io/ZarlarApp/`. `public/outside.html` verwijst nog naar `fideldworp.github.io/Zarlardinge_Dashboards/` → nakijken of dat nog laadt.

```
zarlar-rpi/
├── server.js               ← Node.js server v2.1
├── update.sh               ← RPi sync script
├── deploy.sh               ← Deploy script (ook actief op Mac als ~/deploy.sh)
├── 1-check.sh … 4-static-ip.sh ← Eenmalige installatiescripts
├── README.md
├── Zarlar_Portal_Plan.md   ← Dit document
└── public/
    ├── index.html          ← Portal NIVO 1 (synoptisch overzicht)
    ├── eco.html            ← ECO Boiler NIVO 2
    ├── hvac.html           ← HVAC NIVO 2
    ├── room.html           ← Room NIVO 2 (Eetplaats)
    ├── outside.html        ← Buitensensor
    ├── matrix.html         ← 16×16 matrix replica (Dashboard)
    ├── 12x4_matrix.html    ← S-ENERGY matrix replica
    ├── verlichting.html    ← Verlichting alle rooms
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
192.168.0.60  → ESP32 Dashboard v5.9 (bron van waarheid, matrix 16×16)
192.168.0.70  → ESP32 HVAC v1.19
192.168.0.71  → ESP32 ECO Boiler v1.23
192.168.0.73  → ESP32 Smart Energy (S-ENERGY v1.29 + Tesla bridge, zie §11)
(LAN)         → tesla-key-esp32 (BLE-proxy naar Tesla1, host in S-ENERGY NVS)
192.168.0.80  → ESP32 Room v2.24 (Eetplaats/Testroom)
192.168.0.75–.81 → Room controllers (in server.js v2.1):
                   .75 B&B · .76 Badkamer · .77 Inkomhal · .78 Keuken
                   .79 Wasplaats · .80 Eetplaats · .81 Zitplaats
                   (vervangen stap voor stap de Photons P-*)
```

### 2.1 Architectuurprincipes

**Dashboard controller (192.168.0.60) = bron van waarheid**
- Registreert alle controllers in het systeem
- Pollt alle controllers elke 15 seconden
- Stuurt data naar Google Sheets — BLIJFT ZO, RPi neemt dit NOOIT over
- Serveert /json met volledige systeemstatus
- Bestuurt 16×16 RGB pixel matrix

**RPi = display + controle + beslissingslaag**
- Leest /json van Dashboard controller (lijst van alle controllers)
- Haalt /json en /capabilities op van elke controller
- Toont rijke UI zonder heap beperkingen
- Leest Google Sheets (read-only) voor historiek
- Voert scenes en automatisaties uit via /api/set/
- **Enige Matter-laag naar Apple Home** (§16)
- **Neemt energiebeslissingen** (tarieven, batterijplanning, laadkeuzes) en stuurt die naar S-ENERGY en de Tesla bridge (§17)
- Bewaart eigen gegevens: instellingen, scenes, keuzes, energie-stats (`stats-energie.json`) en actielog
- Neemt de **sensordata-logging naar Google Sheets NIET** over (blijft Dashboard)

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

## 3. server.js v2.9 — API endpoints

> **Toegang (v2.3):** LAN (`192.168.0.x`) en Tailscale (`100.64.0.0/10`) zonder login. Via **Funnel** (komt binnen als `127.0.0.1`) is voor alles een login nodig (HTTP Basic Auth). Gegevens in `/home/fidel/zarlar-dashboard/auth.json` op de Pi, niet in GitHub. Zonder dat bestand is Funnel volledig geblokkeerd. 10 foute pogingen geven 15 min blokkade.

| Endpoint | Methode | Functie |
|---|---|---|
| `/api/settings` | GET/POST | Persistente instellingen |
| `/api/epex` | GET | EPEX België spotprijzen. Slimme cache (v2.2, §6.1), optioneel `?force=1`, extra velden `slots_vandaag`/`slots_morgen` |
| `/api/poll/:naam` | GET | Poll ESP32 `/json` |
| `/api/capabilities/:naam` | GET | Controller capabilities (cache 6u) |
| `/api/set/:naam` | POST | Commando naar controller. Rooms (v2.9): `sw1`/`sw2`/`sw3` (gevraagde toestand, niet togglen), `pN` (pixel N togglen), `setpoint`, `home` of `hm`, `r`/`g`/`b` |
| `/api/status` | GET | Status alle controllers |
| `/api/scenes` | GET/POST/DELETE | Scènes beheren |
| `/api/scenes/:naam/run` | POST | Scène uitvoeren |
| `/api/controllers` | GET | Lijst van controllers |
| `/api/photon/:id` | GET | Photon-data via Cloudflare Worker (geen CORS) |
| `/api/rooms-apple` | GET | v2.9: rooms in Apple Home (vaste lijst `ROOMS_APPLE`): naam, online, mov2, pixel-nicknames. Laatst gekende info in `room-info.json`. Alleen LAN/Tailscale |
| `/api/knoppen` | GET | v2.4+: de Photon-knoppen voor Apple Home (id, naam, Photon, soort, toestand). Alleen LAN/Tailscale. Zie §16.9 |
| `/api/photon/:knop` | POST | v2.4+: knop schakelen via Particle. Body `{"aan":bool}`, dimmer ook `niveau` (0–100), kleur ook `hue`/`sat`. Token uit `particle.json` op de Pi. Alleen LAN/Tailscale |
| `/api/matrix` | GET | Alle ESP32 + Photon data parallel — **kritiek, nooit verwijderen** |
| `/api/history/:naam` | GET | Google Sheets historiek (read-only) — ⬜ gepland, nog niet in server.js |

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
| HVAC | `drawheating` | ✅ /hvac.html |
| ECO Boiler | Custom boiler SVG (6 lagen) | ✅ /eco.html |
| **S-ENERGY** | **`drawelectricenergy`** | **✅ /12x4_matrix.html (29/04)** |
| Eetplaats | `drawhome` | ✅ /room.html |
| Buiten | `drawSunlight` | ✅ /outside.html |
| **Dashboard** | **`drawwifi`** | **✅ /matrix.html (29/04)** |

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

### 4.6 NIVO 2b — S-ENERGY Matrix 12×4 (12x4_matrix.html) ✅ 29/04/2026

**Live replica fysieke WS2812B matrix (48 pixels):**

- 12 kolommen × 4 rijen = 48 pixels
- Serpentine layout: rechts→links, afwisselend ↑↓ (verticaal per kolom)
- Lightbar rendering: vult van onderaan (rij 3) naar boven (rij 0)

**Kolommen:**
1. ☀️ SOL (0-6kW groen)
2. ⚡ SCH↓ (0-10kW rood afname)
3. ⚡ SCH↑ (0-6kW groen injectie)
4. 🏠 WON↓ (0-10kW rood afname)
5. 🏠 WON↑ (0-6kW groen injectie, ~2028)
6. 📊 PIEK (15kW status, groen OK / rood alarm)
7. 💰 ct/kWh (0-40ct gradient cyaan→groen→geel→rood)
8. 💡 HUIS (advies groen=goed / rood=duur)
9. 🔋 BAT (dim paars toekomst)
10. ❤️ HEAP (60KB ESP32 geheugen)
11. 📶 WiFi (RSSI signaalsterkte)
12. 🔴 SIM (S0/P1 status **bovenaan rij 0+1**, rood=SIM / groen=LIVE)

**Features:**
- Status info: badges (S0:SIM/LIVE · P1:SIM/LIVE), solar W, EPEX ct/kWh
- Auto-refresh elke 15s via `/api/poll/senrg`
- Legenda met kleurcodes
- Gebruikt gedeelde `zarlar.css` + `zarlar.js`
- Glow effect op actieve pixels

**Link vanaf:**
- index.html → S-ENERGY tegel → `/12x4_matrix.html`
- epex-grafiek.html → "🔲 Bekijk live matrix 12×4 →" knop

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

**Energiebeheer (ECO boiler, EVs, WPs, batterij):** beslissingen worden op de **RPi** genomen
(keuzes van Maarten en Filip, tarieven, planning). S-ENERGY en de Tesla bridge voeren uit. Zie §17.
*(Wijziging okt 2026: vroeger stond hier dat de SENRG-controller alles zelf beheert.)*

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
4. Matrix pagina — exacte replica 16×16 RGB matrix, live via /json van .60 ✅
5. Per controller rijke UI — automatisch gegenereerd vanuit /capabilities
6. Verlichting pagina — alle room controllers gecombineerd, scenes knoppen ✅ (scenes-knoppen nog te bevestigen)
7. Scenes en Automatisaties — visuele editor, log van uitvoeringen

**Publieke mapstructuur:** plat gehouden (alle pagina's direct in `public/`, zie §1.2).
Nog te bouwen: `scenes.html` (scenes en automatisaties) ⬜.

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
minus een onbalansafslag van 0,67 ct/kWh; dat gaf gemiddeld ≈ **5,25 ct/kWh** (feb. 2026). Injectie aan 0% BTW.
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

### 6.1 Probleem: prijzen van morgen verschijnen te laat (analyse 16 okt 2026)

> **Status: ✅ opgelost in `server.js` v2.2, live sinds 16 okt 2026 14:43.** Eerste opvraging na deploy: `bron: live`, `slots_vandaag: 96`, `slots_morgen: 96`. Opvolgen kan met `journalctl -u zarlar -f` (regels `[EPEX] fetch …`).

**Symptoom:** de prijzen van morgen verschijnen vaak niet in de grafiek, soms pas de volgende ochtend. EPEX publiceert de day-ahead prijzen normaal rond 12:55–13:05.

**Oorzaak:** `server.js` v2.1, `/api/epex`:
```js
if (cache && Date.now() - cache.ts < 26 * 3600000)
  return res.json({ ...cache, bron: 'cache' });
```
De cache blijft **26 uur geldig, ook als de prijzen van morgen er niet in zitten**. Gebeurt de eerste opvraging vóór ±13:00, dan wordt een cache zonder "morgen" een volle dag lang teruggegeven. Vaker pollen vanuit de browser helpt dus niet. Ook S-ENERGY (die `/api/epex` gebruikt) krijgt zo te laat de prijzen van morgen.

**Voorstel (alleen `server.js`, chirurgisch, pas na GO):**
| Situatie | Gedrag |
|---|---|
| Cache bevat vandaag + morgen (volledig) | Geldig tot morgen ±13:00 |
| Vóór ±13:00, cache bevat vandaag | Geldig (morgen bestaat nog niet) |
| Na ±13:00, morgen ontbreekt | Opnieuw ophalen bij energy-charts, **maximaal om de 10 min** |
| Ophalen mislukt | Laatste cache teruggeven met `bron: 'cache_fallback'` (zoals nu) |
| `?force=1` (optioneel, voor refresh-knop) | Meteen ophalen, begrensd tot 1× per minuut |

Het veld `morgen_beschikbaar` bestaat al in de respons. Daarna eventueel een kleine aanpassing in `epex-grafiek.html`: een statusregel "morgen nog niet gepubliceerd (±13u)" en een refresh-knop. Verder niets.

**Later, optioneel:**
- **Tweede bron als fallback**, bv. ENTSO-E Transparency (gratis token).
- **Logregel per fetch** in de RPi-log: tijd, aantal slots vandaag/morgen, bron.

*Voorstellen van Grok/ChatGPT (mei 2026) zijn verworpen. Ze vervingen het volledige script (met lege stubs of weggelaten functies) en zochten de oorzaak in de browser in plaats van in de servercache.*

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

**server.js v2.1:**
- Alle room controllers toegevoegd (room75–room81, 192.168.0.75–.81)
- Dashboard controller toegevoegd (192.168.0.60)
- `/api/photon/:id` proxy endpoint — Cloudflare Worker via RPi (geen CORS issues)
- `/api/matrix` endpoint — haalt alle ESP32 + Photon data parallel op in één call
- **ESP32:** C6, Arduino IDE, `#define Serial Serial0` verplicht
- **ESPAsyncWebServer** — geen MQTT, geen Home Assistant *op de ESP32's* (op de RPi komt MQTT voor Zigbee, zie §16)
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
| 6 | HVAC NIVO 2 pagina | 🟡 Eerste versie (29/04) |
| 7 | Room NIVO 2 pagina | 🟡 Eerste versie (30/04) |
| 8 | Lichten tab verfijnen (pixel nicknames via NVS) | ⬜ Open |
| 9 | Maarten + Céline uitnodigen op Tailscale | ⬜ Open |
| 10 | `/capabilities` endpoint op ESP32 controllers | ⬜ Later |
| **11** | **S-ENERGY controller — sketch v1.27 (matrix 12×4 definitief)** | **✅ Klaar 28/04** |
| **11b** | **S-ENERGY portal pagina 12x4_matrix.html** | **✅ Klaar 29/04** |
| 12 | S-ENERGY: maandelijkse pieken loggen naar stats bestand op RPi | ⬜ Later |
| 13 | Afrekenpagina WON/SCH zichtbaar op portal voor Maarten en Céline | ⬜ Later |
| 14 | S-ENERGY: echte S0 bekabeling + overschakelen van SIM naar LIVE | ⬜ Later |
| 15 | Smart Energy — P1 dongle integratie (na digitale meter ~2028) | ⬜ Toekomst |
| 16 | Thuisbatterij(en) — beslissing + integratie in sturing (~2028) | ⬜ Toekomst |
| 17 | Matter centraliseren op RPi (Matterbridge) + Zigbee-knoppen — zie §16 | ⬜ Plan |
| 18 | Energiebeslissingen op RPi + Tesla-bediening via portal — zie §17 | ⬜ Plan |
| 19 | EPEX-cache fix: prijzen van morgen tijdig binnenhalen — zie §6.1 | ✅ Live 16/10 (v2.2) |

---

## 10. Openstaande actiepunten

| # | Actie |
|---|---|
| AP1 | HVAC NIVO 2 pagina afwerken (eerste versie staat) |
| AP2 | Room NIVO 2 pagina afwerken + generiek maken voor alle rooms |
| AP3 | NIVO 1: alarmen tonen als prioriteit |
| AP4 | Maarten uitnodigen op Tailscale |
| AP5 | Céline uitnodigen op Tailscale |
| AP6 | `/capabilities` endpoint op Room sketch |
| AP7 | `/capabilities` endpoint op HVAC sketch |
| AP8 | `/capabilities` endpoint op ECO sketch |
| ~~AP9~~ | ~~S-ENERGY: GPIO-pinnen toewijzen~~ ✅ IO5/IO6/IO7 via Roomsense RJ45 |
| AP10 | S-ENERGY: S0-interfaceprintje bouwen volgens variant C (Master §6.2) + 1 kanaal testen |
| **AP11** | **S-ENERGY: sketch v1.27 — matrix 12×4 definitief** | **✅ Klaar 28/04** |
| **AP12** | **S-ENERGY: SIM_S0 + SIM_P1 valideren via /json badges** | **✅ Klaar** |
| **AP12b** | **Portal: 12x4_matrix.html live replica** | **✅ Klaar 29/04** |
| AP13 | S-ENERGY: maandpiek per huis loggen (ps, pw, pt) naar stats-bestand RPi |
| AP14 | Portal: afrekenpagina WON/SCH bouwen (zie §13) |
| AP15 | S-ENERGY: S0 bekabeling aansluiten → SIM_S0 uitschakelen → LIVE |
| AP16 | S-ENERGY: WON afname + injectie toevoegen via P1-dongle (~2028) |
| AP17 | Matter → RPi: F0 inventaris starten + beslissingen B1–B4 (zie §16) |
| AP18 | Energie: E0 inventaris + vragen Q1–Q3 beantwoorden (zie §17) |
| ~~AP19~~ | ✅ `server.js` v2.2 live (16/10) |
| ~~AP21~~ | ✅ `server.js` v2.3 live (16/10): login voor Funnel, `auth.json` op de Pi. Funnel-URL is nu `https://rpi-raspberrypi-zarlar.tail3c7f42.ts.net` (Pi hernoemd in Tailscale; oude naam bestond publiek niet meer, opgelost met `tailscale funnel reset`). iOS: beginscherm-icoon als web-app toont wit blad bij Basic Auth → icoon openen in Safari. Eventueel later: eigen loginpagina met cookie (v2.4). |
| AP22 | Portal-Node upgraden van v18 (EOL) naar 22/24 LTS, met nieuwe NodeSource-bron. Apart traject, met backup en test van `server.js`. |
| AP23 | `apt upgrade` van de 95 openstaande pakketten, apart en met verse backup |
| ~~AP24~~ | ✅ F4a afgewerkt (17/10): 26 Photon-knoppen in Apple Home via `server.js` v2.8 + plugin `matterbridge-zarlar` v0.3 (§16.9–16.10) |
| ~~AP25~~ | Particle-token: **bewust behouden** (beslissing FiDel 17/10), want vernieuwen vraagt aanpassingen in te veel apps en pagina's. Risico gekend: het token staat leesbaar in `Lights.html` op de publieke GitHub Pages |
| AP26 | Testwoning "Matter Home" verwijderen in de Woning-app. **Pas na F6:** de eigen Matter-tegels van room80 staan nog in "Matter Home" en dienen als vergelijking tijdens de parallelle test (§16.11). De openstaande uitnodiging voor Mireille intrekken: niet nodig, want FiDel en Mireille gebruiken dezelfde Apple-account. |
| ~~AP27~~ | ✅ Plugin `matterbridge-zarlar` staat in de repo (17/10), nu v0.6 |
| AP28 | F4 rooms: ✅ `server.js` v2.9 + plugin v0.6 (17/10). Volgende: F6 room80 zonder Matter (ROOM-sketch v3.0), §16.11 |
| AP29 | Onderdelen van "Eetplaats" één keer hernoemen in Apple Home (Apple toont geen namen voor onderdelen, §16.11); daarna Matterbridge herstarten en controleren dat de namen blijven |
| AP30 | room80: nickname pixel 0 "TV achtergrondverlichting" is te lang voor het Matter-label (16 tekens) — eventueel inkorten in de room-instellingen (alleen van belang voor Google Home, F10) |
| ~~AP20~~ | ✅ `deploy.sh` v2 (16/10): stopt bij Rosetta-, git- of SSH-fout, en ruimt Downloads alleen op als alles lukte |

---

## 11. Smart Energy Controller (S-ENERGY) — samenvatting

> **Controllerdetail staat in `Zarlar_Master_Overnamedocument.md` §6:** hardware, pinout, S0-printje (variant C), `/json`-keys, matrix 12×4 en Tesla bridge (§6.10). Hieronder staat alleen wat de portal nodig heeft. Bij twijfel geldt de sketch.

| Item | Waarde |
|---|---|
| Sketch | `ESP32_C6_ENERGY_v1_29.ino` (24 sep 2026), IP `192.168.0.73` |
| Pinnen | S0 op **IO5/IO6/IO7** (Roomsense RJ45), matrix 12×4 op **IO4** |
| Status | S0 nog in SIM (printje nog te bouwen), P1 in SIM tot ~2028 |
| Portal-pagina's | `12x4_matrix.html`, `epex-grafiek.html` (via `/api/poll/senrg`) |
| EPEX | S-ENERGY haalt `/api/epex` van de RPi (`{unix_seconds, price}` in EUR/MWh) |

**`/json`-keys die de portal gebruikt** (volledige lijst in Master §6.4):
`a` solar W · `b` WON W · `c`/`d` SCH afname/injectie W · `e` netto SCH W · `h`/`j`/`k` solar, SCH afname, SCH injectie dag Wh · `i`/`vw` WON afname/injectie dag Wh · `n`/`n2` EPEX nu/+1u (ct/kWh × 100) · `pt` maandpiek W · `sim_s0`/`sim_p1` · `ac`/`ae` · `ver`.
`pw` en `ps` bestaan **nog niet**.

**Simulatievlaggen `SIM_S0` / `SIM_P1`:** onafhankelijk van elkaar, alleen via `/settings` van de controller. ⚠️ **Nooit automatisch omschakelen**, ook niet vanuit de portal. De portal toont ze enkel als badge (`S0:SIM · P1:SIM`).

**Tesla bridge (sinds v1.28):** S-ENERGY biedt `/tesla_json`, `/tesla_cfg`, `/tesla_save` en `/tesla_cmd` aan, en stuurt via de `tesla-key-esp32`-proxy (BLE) naar de auto. Bediening vanuit de portal: zie §17.

**HomeWizard P1 (WON, ~2028):** HWE-P1-RJ12, lokale API `GET http://<P1_IP>/api/v1/data`. Wordt door S-ENERGY gepolld, niet door de RPi. De P1-velden worden omgezet naar de keys `b`, `i` en `vw`.

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
| Maandpiek | berekend | Nu | `pt` (W) — momentaan en enkel SCH; kwartierpiek gepland |
| Maandpiek SCH individueel | berekend | Gepland | `ps` (W) — bestaat nog niet |
| Maandpiek WON individueel | P1-dongle | ~2028 | `pw` (W) — bestaat nog niet |
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
- JSON key `pt` (maandpiek) bestaat. `pw` en `ps` (individuele pieken WON/SCH) moeten nog toegevoegd worden.
- EPEX-arbitrage simulatie draait al in de grafiekpagina
- Piekbeheer-algoritme in de EPEX-pagina houdt al rekening met batterij

---

## 15. Geleerde lessen & Best Practices (30 april 2026)

### 15.1 Verlichting pagina development — Auto-discovery & Optimistic UI

**Deliverable:** `/verlichting.html` — Beheer verlichting in alle rooms via de switch-keys sw1/sw2/sw3 (zelfde logica als de Matter-switches op de ROOM)

**Architectuur:**
- Auto-discovery via `/api/status` → filter alleen `online: true` rooms
- State tracking in JavaScript `roomStates` object
- Optimistic UI updates → toggle/color veranderen direct visueel
- Rollback bij POST failure
- iPhone fix: zichtbare `<input type="color">` ipv hidden

**ESP32 ROOM endpoints:**
- `/toggle_pixel_mode?idx=0` → Pixel 0 (MOV override)
- `/toggle_pixel_mode?idx=1` → Pixel 1
- `/toggle_pixel?idx=2..N` → Pixels 2+
- `/setcolor?r=R&g=G&b=B` → RGB alle pixels
- `/capabilities` → Pixel nicknames array

**server.js proxy (v2.1):**
```javascript
// SW1 → pixel_mode[0]
// SW2 → pixel_mode[1]  
// SW3 → AAN/UIT logica (NIET toggle!)
if (body.sw3 !== undefined) {
  const targetState = body.sw3 === 1;
  // Poll huidige state, toggle alleen pixels die moeten veranderen
  for (let i = 2; i < pixelCount; i++) {
    if (currentState[i] !== targetState) {
      await fetch(`/toggle_pixel?idx=${i}`);
    }
  }
}
```

**KRITIEKE LES:** SW3 moet AAN/UIT zijn, NIET blind toggle van alle pixels!

---

### 15.2 Matrix endpoint recovery — /api/matrix ontbrak

**Probleem:** Na commit `9ebef44` ("v2.1: ROOM controller proxy") verdween `/api/matrix` endpoint.

**Impact:**
- matrix.html kon geen 16×16 matrix data ophalen
- Dashboard controller iframe werkte nog, maar matrix niet zichtbaar

**Oplossing:**
1. Identificeer missing endpoint via `grep -n "/api/matrix" server.js`
2. Git history: `git log --all -S "/api/matrix" --oneline`
3. Restore vanuit commit `005ba9e`
4. **KRITIEK:** Start ALTIJD van bestaande server.js, voeg chirurgisch toe

**Herstelde code:**
```javascript
const PHOTON_WORKER = 'https://controllers-diagnose.filip-delannoy.workers.dev/sensor?id=';
const PHOTON_IDS = { ... };

app.get('/api/photon/:id', async (req, res) => { ... });
app.get('/api/matrix', async (req, res) => {
  // Poll alle ESP32 + Photons parallel
  // Return combined result voor matrix.html
});
```

---

### 15.3 Deploy workflow — Kritieke regels

**NOOIT doen:**
- Uitroepteken (`!`) in commit messages → breekt bash
- Nieuwe server.js maken zonder bestaande te checken
- Handmatig files kopiëren → gebruik deploy.sh

**WEL doen:**
```bash
bash ~/deploy.sh "Beschrijving zonder uitroepteken"
```

**Mac-vereiste (16/10/2026):** Terminal mag **niet** via Rosetta draaien. Anders faalt git met een `xcrun ... need 'x86_64'`-fout. Controle: `arch` moet `arm64` geven. Oplossing: Finder → Hulpprogramma's → Terminal → Cmd-I → "Open met Rosetta" uitvinken.

**Welk document via welke weg (17/10/2026):**
- `Zarlar_Portal_Plan.md` → **alleen via `deploy.sh`**. Nooit via de GitHub-website uploaden: dat gaf op 16/10 een merge-conflict bij de volgende deploy.
- `Zarlar_Master_Overnamedocument.md` → via de GitHub-website (geen deploy nodig).
- Bij een conflict toch: `cp ~/Downloads/Zarlar_Portal_Plan.md <repo>/ESP32_Zarlar/zarlar-rpi/` → `git add` → `GIT_EDITOR=true git rebase --continue` → opnieuw `deploy.sh`.

**Deploy.sh flow:**
1. Detecteert files in ~/Downloads
2. Git add + commit + pull --rebase + push
3. SSH naar RPi → git pull + rsync public/
4. Als server.js wijzigt → kopieer + restart zarlar.service
5. Ruimt Downloads op

**Regressie voorkomen:**
- Upload HUIDIGE server.js voordat wijzigingen maken
- Gebruik `str_replace` voor chirurgische edits
- Test na deployment

---

### 15.4 Browser caching issues

**Symptomen:**
- Nieuwe code deployed, maar browser toont oude versie
- Hard refresh (Cmd+Shift+R) helpt niet altijd
- Mobiel vs desktop: verschillend cache gedrag

**Fixes:**
1. **Cache-busting URL:** `?v=2`, `?v=3` toevoegen
2. **Incognito mode** voor clean test
3. **iPhone Safari:** Settings → Clear History and Website Data
4. **Verify server:** `curl http://localhost:3000/file.html` op RPi

---

### 15.5 Optimistic UI pattern

**Principe:** UI update VOOR server response, rollback bij failure.

```javascript
async function togglePixel(roomId, switchType) {
  const newState = !roomStates[roomId][switchType];
  
  // 1. Optimistic update
  roomStates[roomId][switchType] = newState;
  toggleEl.classList.toggle('on', newState);
  
  // 2. POST to server
  const resp = await fetch('/api/set/...', { ... });
  
  // 3. Rollback on failure
  if (!resp.ok) {
    roomStates[roomId][switchType] = !newState;
    toggleEl.classList.toggle('on', !newState);
  }
}
```

**Voordelen:**
- Instant feedback
- Geen reload nodig
- Graceful degradation bij netwerk errors

---

### 15.6 iPhone kleurpicker fix

**Probleem:** iOS triggert geen events op `opacity: 0` inputs.

**Fout:**
```html
<div class="color-preview" onclick="...click hidden input...">
<input type="color" style="opacity:0; width:0; height:0;">
```

**Fix:**
```html
<input type="color" 
       class="color-picker-input"
       style="width:60px; height:60px; border-radius:8px;"
       oninput="setColor(...)"
       onchange="setColor(...)">
```

**Resultaat:** Native iOS color picker werkt, ziet er netjes uit.

---

### 15.7 Auto-discovery pattern

**Principe:** Toon ALLEEN online controllers, poll GEEN offline controllers.

```javascript
async function discoverRooms() {
  const resp = await fetch('/api/status');
  const status = await resp.json();
  const rooms = [];
  
  // Filter ALLEEN online rooms
  for (const [name, info] of Object.entries(status)) {
    if (name.startsWith('room') && info.online) {
      rooms.push({ id: parseInt(name.replace('room', '')), ... });
    }
  }
  return rooms;
}

// Poll ALLEEN deze online rooms
for (const room of onlineRooms) {
  await fetch(`/api/poll/room${room.id}`);
}
```

**Voordelen:**
- Snelle load tijd (geen 3s timeouts op offline rooms)
- Automatische updates wanneer rooms online komen
- Clean UX: geen "offline" placeholders

---

## 16. Matter centraliseren op de RPi (+ Zigbee-knoppen)

*Toegevoegd 15 oktober 2026. Status: **plan, nog niets geïnstalleerd**. Basis: overnamedocument "Plan Matter naar RPi" van 2 oktober 2026 (§16.2), aangevuld met review, stappenplan en werking (§16.3 tot §16.8).*

### 16.1 Doel

- Matter verdwijnt uit de ESP32-C6-controllers (ROOM, HVAC, ECO, Dashboard; beslist okt 2026, zie Master §16). Dat bespaart 80 tot 120 KB heap per controller (zie §15) en maakt de sketches eenvoudiger.
- De ESP32's doen alleen nog I/O, lokale realtime-berekeningen, JSON serveren en de matrix aansturen, volgens de bestaande architectuurregel.
- De RPi wordt de enige Matter-laag naar Apple Home, en later eventueel naar Google Home.
- De 4 Aqara Opple 6-knops schakelaars (WXCJKG13LM, Zigbee 3.0) worden lokaal bruikbaar. De Chinese cloud-hub werkt niet meer.
- Alles wat nu op de RPi draait blijft behouden: `zarlar.service`, Tailscale (+ Funnel) en desktop/VNC.

### 16.2 Uitgangssituatie (uit overnamedocument 2 okt 2026)

| Item | Waarde |
|---|---|
| Hardware | Raspberry Pi 3B+, ±1 GB RAM, microSD 14 GB (5,0 GB vrij) |
| OS | Debian 13 (Trixie) 13.7, aarch64, kernel 6.18.50+rpt-rpi-v8 |
| Belasting | load 0.00, CPU ±99% idle, 605 MiB available, swap 175 MiB gebruikt |
| Docker | niet geïnstalleerd |
| Processen | zarlar-dashboard (Node, ±48 MB), tailscaled, wayvnc, labwc, wf-panel-pi, pcmanfm, piclone, squeekboard |
| Apple Home | 2× HomePod mini, 2× Apple TV 4K, iPhones (Thread Border Routers aanwezig) |
| Tweede RPi | Sjalay (Recht), via Tailscale. Valt buiten deze fase. |

Ontwikkelprincipes uit het overnamedocument, die onverkort gelden:
- **Niet-destructief.** De bestaande installatie blijft werken.
- **Eén stap tegelijk:** commando → output bekijken → interpreteren → volgende kleine stap.
- **Eerst meten, dan wijzigen.**
- **Tailscale niet herinstalleren of vervangen.**
- **Backup vóór elke structurele wijziging.**
- Geen ander OS (geen Home Assistant OS) en geen nieuwe hardware zolang de Pi 3B+ niet getest is.
- Actuele officiële documentatie gebruiken voor installatie-instructies.

Het overnamedocument vergeleek twee architecturen:
- **A.** Alleen een Matter Server.
- **B.** Home Assistant (Container) + Matter Server, in Docker op de bestaande Debian.

Het voorlopige voorstel daar was B.

### 16.3 Review: wat bijgestuurd moet worden

1. **A en B halen het doel niet.** In beide architecturen blijven de ESP32's Matter-devices die de Matter Server commissioneert. Matter verhuist dan niet naar de Pi, er komt alleen een extra controller bij. Om Matter echt uit de ESP32's te halen is een **Matter-*bridge*** op de Pi nodig. Die bridge leest de bestaande HTTP/JSON-endpoints (via `server.js`) en biedt die zelf als Matter-accessoires aan Apple Home aan.
2. **Het startmechanisme van zarlar-dashboard is al bekend.** Volgens §1.1 is dat systemd `zarlar.service`. `systemctl status zarlar-dashboard` vond niets omdat de unit anders heet. Dit moet bevestigd worden met `systemctl status zarlar` (zie F0).
3. **Node-versie.** De portal draait op Node.js v18. Matterbridge vraagt Node 20.19+, 22 of 24 (24 LTS aanbevolen). De systeem-Node mag niet blind geüpgraded worden, want dat raakt `server.js`. Daarom draait Matterbridge in Docker, of met een eigen Node, apart van de portal.
4. **Home Assistant op een Pi 3B+.** Home Assistant ondersteunt alleen nog OS- en Container-installaties. aarch64 blijft ondersteund, maar 1 GB RAM is krap voor HA + Matter Server samen. Voor dashboards en historiek heeft Zarlar al de eigen portal, dus HA is voor het doel niet nodig.
5. **Welke controllers hebben Matter?** Volgens de Master (§16): ROOM (8 endpoints), HVAC (9 endpoints), **ECO (5 endpoints: zonnecollector, boiler top, boiler bodem, vulling %, pomp)** en Dashboard (1 endpoint HOME/UIT). Alleen S-ENERGY heeft geen Matter. *(Rechtgezet 18 okt 2026: hier stond eerst dat ook ECO geen Matter had; de ECO-sketch heeft Matter sinds v1.18.)* Het overnamedocument sprak van ±12 controllers, inclusief de geplande rooms .75–.81. Die krijgen meteen een sketch zonder Matter.
6. **Apple Home-scènes.** Zodra een ESP32 zonder Matter geflasht wordt, verdwijnt zijn oude accessoire uit Apple Home. Scènes en automaties die daarnaar verwijzen (o.a. die van Mireille) moeten dan opnieuw ingesteld worden op het bridge-accessoire. Dit gebeurt dus per kamer en gepland.

### 16.4 Voorgestelde architectuur (C): Matterbridge + Zarlar-plugin

```text
ESP32-C6 controllers (géén Matter meer: alleen HTTP/JSON)
        │  /json, /toggle_pixel, ...
        ▼
server.js (portal, zarlar.service)  ← enig toegangspunt naar de controllers
        │  /api/poll, /api/set, /api/scenes
        ▼
Matterbridge (Docker, Node 24, network_mode: host)
   ├─ plugin "zarlar"  (eigen dynamic platform → rooms als Matter-devices)
   └─ plugin "zigbee2mqtt" → Zigbee2MQTT ← Mosquitto ← USB Zigbee-dongle ← 4× Opple
        │
        ▼
Apple Home (wired Apple TV als voorkeurshub)  /  later Google Home
```

Waarom deze keuze:
- **Licht.** Volgens de projectdocumentatie draait Matterbridge vanaf 512 MB RAM. Dat maakt de Pi 3B+ realistisch, in tegenstelling tot HA.
- **Past in de stack.** Matterbridge is Node.js, net als de portal, en ondersteunt eigen plugins (accessory- en dynamic platform).
- **Eén toegangspunt.** De plugin praat alleen met `server.js`, niet rechtstreeks met de ESP32's. De Dashboard-controller (.60) blijft de bron van waarheid (§2.1); de portal is de enige laag die naar buiten praat, en controllers blijven geïsoleerd van de buitenwereld.
- **Zigbee komt er gratis bij.** De bestaande zigbee2mqtt-plugin stelt de Opple-knoppen beschikbaar in Apple Home. Via MQTT kan `server.js` dezelfde knopacties ook gebruiken voor Zarlar-scènes.
- **Reversibel.** De Docker-container stoppen herstelt de vorige toestand. Er zijn geen wijzigingen aan Debian-pakketten van de portal.

Aandachtspunten:
- IPv6 en mDNS moeten op het LAN werken.
- Apple Home-limiet: 150 accessoires per bridge.
- Wifi-HomePods kunnen soms zendfouten geven, dus een bekabelde Apple TV als hub verdient de voorkeur.

### 16.5 Stappenplan

Elke fase heeft een **meetpunt** (wanneer is ze geslaagd) en een **terugweg**. Pas na expliciete GO naar de volgende fase. Binnen een fase: één commando per keer.

| Fase | Wat | Meetpunt | Terugweg |
|---|---|---|---|
| **F0** | Inventaris, niets wijzigen: `ip -br addr`, `ip -6 addr`, `ip route`, `ip -6 route`, `tailscale status`, `systemctl status zarlar`, `node -v`, `free -h`, `df -h`. Daarnaast een lijst welke ESP32's nu Matter hebben. | IPv6 actief op LAN-interface, autostart portal bevestigd, Matter-lijst compleet | n.v.t. |
| **F1** | Backup: SD-image (piclone naar USB/SD) + check dat de repo volledig in GitHub staat | Image opstartbaar getest | n.v.t. |
| **F2** | Docker installeren (officiële Debian-instructies) | Portal, Tailscale en VNC werken nog; RAM gemeten | `apt purge docker-*` |
| **F3** | Matterbridge in Docker (`network_mode: host`), zonder plugins; koppelen met Apple Home | Bridge zichtbaar in Woning-app; RAM < ±150 MB extra; 24 u stabiel | container stoppen, bridge uit Woning verwijderen |
| **F4a** | **Photon-toestellen** naar de RPi-bridge (code pas na GO): de 26 knoppen van de gewiste HomeSpan-bridge terug in Apple Home via de Zarlar-plugin → `server.js` → Particle. Zie §16.9 | ✅ 17/10: 26 knoppen werken, ook via 4G | plugin uitschakelen |
| **F4** | (herzien 17/10, §16.11) `server.js` v2.9 + plugin v0.4–v0.6: **room80 met alle tegels** die de room nu zelf via Matter aanbiedt (thermostaat, vocht, beweging, kleur, SW1–SW3) | ✅ 17/10: room80 als één toestel "Eetplaats" in Zarlardinge, waarden gelijk aan portal | plugin terug naar v0.3 |
| **F5** | (herzien 17/10) **Parallelle test 48 u:** oude tegels van de ESP32 (woning "Matter Home") naast de nieuwe van de RPi (Zarlardinge) | 17/10 avond: alles werkt volgens verwachting (FiDel). Duur van de test: beslist FiDel vóór F6 | plugin terug naar v0.3 |
| **F6** | Die ene ESP32 flashen **zonder Matter** (nieuwe sketchversie, na GO); oud accessoire uit Apple Home; scènes van die kamer opnieuw instellen | Heap-winst gemeten; kamer volledig bedienbaar via bridge | vorige sketch terugflashen |
| **F7** | Opschalen 1 → 2 → 4 → alle rooms, telkens met dezelfde check | RAM, CPU, swap, mDNS en Apple Home stabiel na elke stap | laatste groep terugdraaien |
| **F8** | Zigbee: USB-dongle + Mosquitto + Zigbee2MQTT (Docker) + matterbridge-zigbee2mqtt; 1 Opple koppelen, `operation_mode: event` | Alle 6 knoppen (single/double/long) zien in Z2M en Apple Home | containers stoppen |
| **F9** | Optioneel: Opple-acties ook in portal (`server.js` via MQTT → bestaande scenes) | Knop start Zarlar-scene | feature uitzetten |
| **F10** | Optioneel: Google Home als tweede fabric | Devices in beide apps | fabric verwijderen |

**Wanneer naar een Pi 4/5 (4 GB)?** Bij aanhoudend swapgebruik, RAM-tekort, instabiele mDNS/Matter-discovery, trage reacties of minder dan 2 GB vrije opslag. De Pi 3B+ kan dan portal, Tailscale en VNC blijven draaien.

### 16.6 Open beslissingen

| # | Vraag |
|---|---|
| B1 | Architectuur C (Matterbridge) akkoord, of toch HA (B) testen? (de richting "Matter naar RPi" zelf is beslist) |
| B2 | Welke Zigbee-dongle (bv. Sonoff ZBDongle-E / SLZB-06)? |
| ~~B3~~ | ✅ room80 (R-EETPL), de enige room die nu online is (beslist 17/10) |
| B4 | Google Home echt nodig, of enkel Apple Home? |

### 16.7 Werking bridge & dynamische kamers (toegevoegd 16 okt 2026)

**Koppelen met Apple Home: één keer, niet per kamer**
- De RPi is één **Matter-bridge** (vergelijkbaar met een Hue-bridge). Alle kamers zijn accessoires *achter* die bridge.
- **Wanneer:** stap F3. Eerst wordt de lege bridge gekoppeld, die 24 u stabiel moet blijven. Pas daarna komen er kamers bij (F4).
- **Hoe:** de Matterbridge-webpagina op de Pi (`http://192.168.0.50:8283`) toont QR-code en koppelcode. Woning-app → **+** → Accessoire toevoegen → QR scannen of code intypen.
- Later toegevoegde kamers verschijnen vanzelf in Apple Home, zonder opnieuw te koppelen.
- **Opvolgen:** de Matterbridge-pagina toont de koppelstatus, de fabrics (Apple Home, later evt. Google Home), de apparaten en een live log. Gepland: een portaltegel "Apple Home-bridge" met status (gekoppeld / aantal accessoires) en een link naar die pagina.
- Voorkeurshub in Apple Home: bij voorkeur een bekabelde Apple TV (wifi-HomePods kunnen soms zendfouten geven).

**Het bestaande lichtensysteem blijft het hart**
```
Apple Home ──► Matterbridge (plugin "zarlar") ──► server.js ──► room-ESP32
Portal     ─────────────────────────────────────► server.js ──► room-ESP32
Dashboard  ◄──────────────────────────────────── (pollt de ESP32's, zoals nu)
```
- De plugin gebruikt **dezelfde calls** als `verlichting.html`: `/api/status`, `/api/poll/roomNN`, `/api/set/roomNN` (sw1/sw2/sw3) en `/setcolor`. Er komt geen tweede bedieningslogica.
- **Eén toestand:** die zit op de ESP32. Een lamp in de portal schakelen is binnen enkele seconden zichtbaar in Apple Home (de plugin pollt), en omgekeerd.
- Dashboard en 16×16-matrix blijven ongewijzigd.

**Dynamische kamers** — ⚠️ *herzien 17/10 (§16.11, R2): géén automatische ontdekking, maar een **vaste lijst** in `server.js`. Een room die offline is bij het herstarten van de plugin zou anders uit Apple Home verdwijnen en scènes breken. Onderstaande tekst blijft als achtergrond.*
- De plugin ontdekt rooms zoals `verlichting.html` dat doet: `/api/status` filteren op `online: true` voor `room*`. De namen komen uit `/capabilities` (pixel-nicknames uit NVS van de room).
- Een nieuwe room op het wifi wordt een nieuw accessoire in Apple Home, met de namen uit `/settings` van die room.
- **Een offline room wordt NIET verwijderd** uit Apple Home, maar getoond als "geen reactie". Verwijderen zou scènes en automaties (o.a. van Mireille) breken. Er worden alleen rooms toegevoegd, nooit automatisch verwijderd.
- **Te testen in F4:** kan Matterbridge een room toevoegen terwijl de bridge draait, of pas na een herstart van de plugin? Beide zijn bruikbaar. In het tweede geval herstart de plugin zichzelf (bv. hooguit om de 15 min) wanneer er een nieuwe room ontdekt is.

**Matter-accessoires per room (voorstel, zelfde functies als room80 nu zelf via Matter aanbiedt)**
| Functie | Matter-type | Bron in server.js / room |
|---|---|---|
| SW1 — pixel 0 (MOV-override) | Lamp aan/uit | `/api/set` `sw1` |
| SW2 — pixel 1 | Lamp aan/uit | `/api/set` `sw2` |
| SW3 — pixels 2+ (AAN/UIT, geen toggle) | Lamp aan/uit | `/api/set` `sw3` |
| Kleur alle pixels | Kleurlamp | `/setcolor` |
| Thuis/Weg + setpoint | Thermostaat | bestaande room-endpoints (`/toggle_home`, `/set_setpoint`) |
| Temperatuur, vochtigheid | Sensoren | `/json` keys `e`/`f`, `h` |
| Beweging MOV1/MOV2 | Bewegingssensoren | `/json` keys `w`/`x` |

### 16.8 Voortgang

| Fase | Datum | Resultaat |
|---|---|---|
| **F0** inventaris | 16/10/2026 | ✅ `zarlar.service` actief (startmechanisme bevestigd). Node **v18.20.8** → Matterbridge komt in Docker. RAM 905 MiB (641 MiB beschikbaar vóór Docker). Schijf 14 GB, 4,7 GB vrij. Alleen **wifi** (`eth0` down). IPv6 + ULA `fd78:…` in orde. Thread-routers aanwezig (Aqara camerahub G3 / deurbel). |
| **F1** backup | 16/10/2026 | ✅ SD-clone (16 GB → 32 GB) met SD Card Copier, na de deploy van v2.3 |
| **F2** Docker | 16/10/2026 | ✅ Docker **29.9.1** + Compose **v5.6.0** uit de officiële Debian-bron (trixie/arm64). `hello-world` OK. zarlar/tailscaled/docker actief, portal 200. RAM beschikbaar **544 MiB** (−97 MiB), schijf 4,4 GB vrij. |
| **F3** Matterbridge | 16/10/2026 | ✅ Matterbridge **3.10.14** in Docker (`~/matterbridge/docker-compose.yml`, image `luligu/matterbridge:latest`, host-netwerk, loglimiet 3×10 MB). Interne Node 24. Frontend `http://192.168.0.50:8283` met wachtwoord. **Gekoppeld met Apple Home** (2 fabrics = Woning + sleutelhanger, normaal). Extra RAM ±80 MiB → 462 MiB beschikbaar. "Virtual mode" uitgezet. 24-uurstest: 17 u healthy, daarna onderbroken door de Pi-herstart en het Tailscale-probleem (zie 17/10). Stabiliteit voldoende bevonden. |
| **F3b** verhuis | 17/10/2026 | ✅ Bridge verwijderd uit de testwoning "Matter Home" en opnieuw gekoppeld in **Zarlardinge** als **"Testkamer RPi Matter Bridge"**. Hub: Keuken Geel Bolleke. |
| **F4** room80 | 17/10/2026 | ✅ `server.js` v2.9 + plugin v0.6: room80 als één samengesteld toestel "Eetplaats" (8 onderdelen) in Zarlardinge; oude tegels van de ESP32 blijven ter vergelijking in "Matter Home" (F5). Heap room80 **vóór F6**: vrij 17 %, grootste blok 31 KB, minimum ooit 7 KB. |
| **F4a** Photons | 17/10/2026 | ✅ 26 Photon-knoppen in Apple Home: `server.js` v2.4 → v2.8 + plugin `matterbridge-zarlar` v0.1 → v0.3 (§16.9–16.10). De ESP32-WROOM-32D met HomeSpan is niet meer nodig. |

**Lessen F3 (16/10):**
- **Nooit updaten via de virtuele "Update"-schakelaar of via de webpagina.** In Docker brak dat de container (`matterbridge: not found`, herstartlus). Herstel: `docker compose up -d --force-recreate`. De koppeling bleef intact (staat in `~/.matterbridge`).
- **Updaten = alleen zo:** `ssh -t fidel@100.123.74.113 'cd ~/matterbridge && sudo docker compose pull && sudo docker compose up -d'`
- `docker stats` toont 0 B: de Pi-kernel houdt geen geheugen per container bij (memory-cgroup uit). Meten via `free -h` volstaat.
- De log toont UTC-tijd. Optioneel later `TZ=Europe/Brussels` in de compose.

**Gebeurtenissen 17/10:**
- **Tailscale "Logged out":** de sleutel van de Pi was verlopen. Daardoor was er geen toegang van buitenaf (Termius, portal, Funnel). Herstel op de Pi: `sudo tailscale up --accept-dns=false --advertise-routes=192.168.0.0/24` → inloggen via de getoonde URL. Daarna in de Tailscale-adminconsole **key expiry uitgeschakeld voor alle 5 toestellen**. Controle: `tailscale status`.
- **Wifi en Pi herstart** (FiDel, ±21:00 de dag ervoor). Portal, Tailscale en Matterbridge kwamen automatisch terug (`restart: always`).
- **Les: een Matter-bridge hoort in de woning met de hubs.** De bridge stond in de testwoning "Matter Home", die geen woninghub heeft. Gevolg: alleen bedienbaar via wifi, niet van buitenaf. Alle woninghubs (HomePods, Apple TV's) horen bij **Zarlardinge**.
- **Verhuizen van een bridge:** na "Verwijder bridge uit woning" blijft vaak de fabric **AppleKeyChain** (vendor 4996) achter. Matterbridge toont dan geen QR-code. Oplossing: die fabric verwijderen op de Matterbridge-pagina, waarna de QR-code terugkomt.
- **De HomeSpan-bridge voor de Photons is per vergissing gewist** uit Apple Home ("Testkamer Zarlardinge Bridge", ESP32-WROOM-32D, `Full_Particle_Bridge_Compliant.ino`), nadat Claude die verkeerd aangeduid had als room80. Beslissing (FiDel): **niet opnieuw koppelen, maar meteen migreren naar de RPi-bridge** (F4a, §16.9). De scènes worden daarna opnieuw gemaakt.
- De testwoning "Matter Home" is nu leeg en kan verwijderd worden.

**Lessen F4a (17/10):**
- **Na elke wijziging in de knoppenlijst (nieuwe of verdwenen tegels): de woninghub Keuken Geel Bolleke herstarten** (stekker uit/in). Matterbridge meldt nieuwe toestellen correct, maar Apple Home verwerkt verdwenen tegels meteen en nieuwe pas na een herstart van de hub.
- De Woning-app heeft **geen zoekveld**. Nieuwe tegels vind je via de bridge (tandwiel → lijst accessoires) of in de kamer van de bridge.
- **Apple Home toont geen meerstandenschakelaar** (Matter Mode Select wordt niet getoond). Oplossing: dimbare lamp voor standen met een volgorde, kleurenlamp voor kleuren, één schakelaar voor twee standen.
- De plugin praat met `http://192.168.0.50:3000`, **niet met localhost**: voor `server.js` is `127.0.0.1` Funnel-verkeer (login vereist).
- Elke tegel heeft een vaste id (bv. `trap`). Een nieuwe id = een nieuwe tegel: kamer en scènes opnieuw instellen.

**Lessen F4 (17/10):**
- **Losse toestellen of één samengesteld toestel?** Elk apart toestel achter de bridge heeft een eigen naam (de nickname komt automatisch mee), maar geeft een losse tegel. Een samengesteld toestel (één accessoire met onderdelen, zoals de ESP32 zelf) geeft één tegel met in Apple de keuze "Toon als afzonderlijke tegels", maar **Apple toont geen namen voor de onderdelen**: ze heten allemaal zoals het toestel. Gekozen: samengesteld, onderdelen één keer zelf hernoemen (AP29).
- **Apple negeert het Matter-label (FixedLabel)** voor de naam van een onderdeel (getest met v0.6). De labels blijven staan voor Google Home (F10). Een label is maximaal 16 tekens.
- De ESP32 stuurt zelf ook geen namen voor zijn onderdelen: de namen in "Matter Home" staan in Apple, niet in de room.
- Nieuwe tegels verschenen deze keer ook zonder herstart van de woninghub. Bij een wissel van losse tegels naar één toestel was de herstart wel nuttig.
- `room80` staat bewust op **Weg** (thermostaat "Uit").

**Bijvangst (los van Matter):**
- De NodeSource-bron `node_18.x` wordt door apt geweigerd (SHA1-sleutel ongeldig sinds 02/2026). Geen gevolg voor de werking, maar Node 18 is EOL → zie AP22.
- `apt` meldt 95 upgradebare pakketten. Bewust uitgesteld: later apart, met verse backup (AP23).
- Aanbeveling: netwerkkabel naar de Pi vóór F3/F4 (stabielere mDNS/multicast voor Matter). IP `192.168.0.50` moet dan behouden blijven.

### 16.9 F4a — Photon-toestellen naar de RPi-bridge (✅ 17/10/2026)

**Status:** ✅ afgewerkt op 17/10/2026. Vervangt `Full_Particle_Bridge_Compliant.ino` (HomeSpan 2.1.5 op ESP32-WROOM-32D), die per vergissing uit Apple Home gewist was.

**Weg van een klik:**
```
Apple Home ──► Matterbridge (plugin "zarlar") ──► server.js /api/photon/:knop ──► Particle Cloud ──► Photon (functie "manual")
```
- De plugin heeft **geen eigen lijst**: hij leest `GET /api/knoppen` uit `server.js` (constante `KNOPPEN`). Eén bron.
- Het Particle-token staat alleen in `/home/fidel/zarlar-dashboard/particle.json` op de Pi (`{"token":"..."}`, chmod 600, niet in GitHub). `deploy.sh` en `update.sh` raken het niet aan. Ontbreekt het, dan antwoordt `/api/photon/:knop` met 503. Het token wordt bewust niet vernieuwd (AP25).
- De Photons melden hun stand niet terug. `server.js` onthoudt de laatst gestuurde toestand **in RAM**: na een herstart van de Pi staat alles op "uit" tot je iets kiest.
- Mislukt een commando (Photon offline, timeout), dan springt de tegel in Apple Home terug.
- Het getal dat de Photon terugstuurt (bv. 100) wordt niet geïnterpreteerd (beslist 17/10: niet nodig).

**Beslissingen (17/10/2026, FiDel):**
| # | Beslissing |
|---|---|
| P1 | Device-ID's zoals in console.particle.io. **BandB vervalt** (Photon defect, wordt later een ESP32). Buro en Bed via R6-EETPL; ECO ⇒ SCHUUR/WONING via S-HVAC |
| P2 | Eerst één schakelaar per stand, daarna vereenvoudigd: dimmer voor standen met een volgorde, kleurenlamp voor de Trap, één schakelaar voor twee standen |
| P3 | Token in `particle.json` op de Pi |
| — | Weergave: lampen als **lamp**, de rest als **stopcontact** (schakelaar), zodat "alle lampen uit" de HVAC en ECO niet raakt |

**Photons (8):**
| Photon (console) | ID | Ook in `PHOTON_IDS` (matrix)? |
|---|---|---|
| R7-ZITPL | `200033000547373336323230` | ja |
| R2-BADK | `5600420005504b464d323520` | ja |
| R3-INKOM | `2c0026000747343232363230` | ja |
| R4-KEUK | `310017001647373335333438` | ja |
| R5-WASPL | `33004f000e504b464d323520` | ja |
| R6-EETPL | `3c0030000a47353138383138` | ja |
| S-OUTSIDE | `3e003f001447343338333633` | nee |
| S-HVAC | `290044000147353138383138` | nee |

**Knoppen (26, server.js v2.8):** 16 lampen, 7 schakelaars, 2 dimmers, 1 kleurenlamp.
| Photon | Lampen (aan/uit → `<tag>on` / `<tag>off`) | Schakelaars | Dimmer / kleur |
|---|---|---|---|
| R7-ZITPL | Muurspots (`muurspots`), TV Backlight (`tvback`) | Zomertijd: aan = `summer`, uit = `winter` | **Klok helderheid** (dimmer): 1–33 % `l`, 34–66 % `m`, 67–100 % `h`; kan niet uit |
| R2-BADK | Badkamerlicht (`badkamer`), Leeslampjes (`leeslampjes`) | — | — |
| R3-INKOM | Inkom (`inkom`), Dressing (`dressing`) | — | **Trap** (kleur): uit `trapoff`, wit `trapon`, rood `traproodon`, groen `trapgroenon`, blauw `trapblauwon` (dichtste tint; verzadiging < 25 % = wit; helderheid genegeerd) |
| R4-KEUK | Keuken Spots (`spots`) | — | — |
| R5-WASPL | Wasplaats (`wasplaats`), Toilet/Vestiaire (`toilet`) | — | — |
| R6-EETPL | Tafel (`tafel`), Bib (`bib`), Buro (`buro`), Bed (`bed`) | — | **Ramen** (dimmer): uit `ramenlowoff`, 1–33 % `ramenlowon`, 34–66 % `ramenmedon`, 67–100 % `ramenhion` |
| S-OUTSIDE | Terras (`S`), Veldkant (`W2`), Voordeur (`W1`) | Sunlight auto/dag/nacht (`auto`/`day`/`night`, onderling exclusief) | — |
| S-HVAC | — | ECO => SCHUUR (`sch`), ECO => WONING (`won`), HVAC Home: aan = `home`, uit = `out` | — |

- De schuif van een dimmer springt na een keuze naar het midden van de stand (33/67/100 %), zodat je ziet wat actief is. De Trap springt naar zuiver wit/rood/groen/blauw.
- Sunlight blijft drie exclusieve schakelaars: drie standen zonder volgorde passen in geen enkel Apple-type. De actieve stand uitzetten doet niets.

**Versies `server.js` (17/10):** v2.4 knoppen + Particle · v2.5 soort lamp/schakelaar · v2.6 Zomertijd en HVAC als één schakelaar · v2.7 Ramen en Klok als dimmer · v2.8 Trap als kleurenlamp.

### 16.10 Plugin `matterbridge-zarlar`

| | |
|---|---|
| Versie | **v0.7** (21/10/2026): + HVAC als samengesteld toestel (§16.12). Historiek: v0.1 lampen/schakelaars · v0.2 dimmers · v0.3 kleurenlamp · v0.4 room-tegels (los) · v0.5 room = één toestel · v0.6 namen als FixedLabel (Apple toont ze niet: onderdelen één keer zelf hernoemen) · v0.7 HVAC-toestel |
| Bestanden | `package.json`, `module.js`, `matterbridge-zarlar.config.json` — gewoon JavaScript, geen buildstap |
| Op de Pi | `/home/fidel/Matterbridge/matterbridge-zarlar/` (in de container: `/root/Matterbridge/...`, gekoppeld volume → overleeft updates en `--force-recreate`) |
| In de repo | `ESP32_Zarlar/zarlar-rpi/matterbridge-zarlar/` ✅ |
| Portal-adres | config `portal`, standaard `http://192.168.0.50:3000` |

**Werking:**
- Bij de start: `GET /api/knoppen` → één Matter-toestel per knop, met de knop-id als vaste id (Apple Home houdt kamer en naam).
- Portal onbereikbaar bij de start → elke 10 s opnieuw. Er wordt **nooit een lege lijst** geregistreerd (anders verdwijnen de tegels en scènes).
- Commando's worden niet afgewacht in Matter (Apple krijgt meteen antwoord). Dimmer en kleur: 400 ms verzamelen (Apple stuurt vaak `on` + `moveToLevel`/kleur kort na elkaar), dan één commando.
- Het antwoord van `server.js` bevat de toestand van de knop (of de hele groep); die wordt overgenomen.

**Eerste installatie (17/10, gebeurd):**
```
scp ~/Downloads/matterbridge-zarlar.tar.gz fidel@100.123.74.113:/tmp/ && ssh -t fidel@100.123.74.113 'sudo tar -xzf /tmp/matterbridge-zarlar.tar.gz -C /home/fidel/Matterbridge && sudo docker exec -w /root/Matterbridge/matterbridge-zarlar matterbridge npm link matterbridge'
ssh -t fidel@100.123.74.113 'sudo docker exec matterbridge matterbridge -add /root/Matterbridge/matterbridge-zarlar; cd ~/matterbridge && sudo docker compose restart'
```
- `npm link matterbridge` maakt `node_modules/matterbridge` → `/usr/local/lib/node_modules/matterbridge` in de container. Eenmalig; blijft staan bij updates van de plugin.

**Plugin bijwerken:** `.tar.gz` in Downloads (nooit via `deploy.sh`: die zou `module.js` in `public/` zetten). Eerst in de repo:
```
cd ~/Zarlardinge_Dashboards/ESP32_Zarlar/zarlar-rpi && tar -xzf ~/Downloads/matterbridge-zarlar.tar.gz && git add matterbridge-zarlar && git commit -m "Plugin matterbridge-zarlar vX.Y" && git pull --rebase && git push
```
Dan op de Pi:
```
scp ~/Downloads/matterbridge-zarlar.tar.gz fidel@100.123.74.113:/tmp/ && ssh -t fidel@100.123.74.113 'sudo tar -xzf /tmp/matterbridge-zarlar.tar.gz -C /home/fidel/Matterbridge && cd ~/matterbridge && sudo docker compose restart && sleep 40 && sudo docker logs --since 1m matterbridge 2>&1 | grep -iE "Zarlar v|knoppen geregistreerd|error"'
```
Daarna **Keuken Geel Bolleke herstarten** als er tegels bijkomen of verdwijnen.

**Plugin in de repo zetten (AP27, eenmalig, ✅ 17/10):**
```
cd ~/Zarlardinge_Dashboards/ESP32_Zarlar/zarlar-rpi && tar -xzf ~/Downloads/matterbridge-zarlar.tar.gz && git add matterbridge-zarlar && git commit -m "Plugin matterbridge-zarlar v0.3" && git pull --rebase && git push && rm ~/Downloads/matterbridge-zarlar.tar.gz
```
Daarna staat de plugin via `update.sh` ook in `/home/fidel/repo/ESP32_Zarlar/zarlar-rpi/matterbridge-zarlar/` op de Pi. Een latere update kan dan: bestanden in de repo-map op de Mac zetten → committen/pushen → op de Pi kopiëren uit `~/repo` en Matterbridge herstarten.

### 16.11 F4–F6 — Rooms naar de RPi-bridge (F4 ✅ 17/10/2026)

**Beslissingen (17/10/2026, FiDel):**
| # | Beslissing |
|---|---|
| R1 | Testkamer: **room80** (R-EETPL, 192.168.0.80), de enige room die nu online is |
| R2 | De rooms in Apple Home staan in een **vaste lijst in `server.js`**, niet automatisch ontdekt (zie §16.7) |
| R3 | Kamernaam uit diezelfde lijst (room80 = "Eetplaats"). De lampnamen komen uit de pixel-nicknames (`/capabilities`) |
| R4 | **Eerst parallel testen:** de oude tegels van de ESP32 blijven in de woning "Matter Home", de nieuwe van de RPi komen in Zarlardinge. Pas na 48 u stabiel: Matter uit de room-sketch (F6) |
| R5 | Toestand opvragen om de **10 s** (om te beginnen) |
| R6 | (17/10, na test) Elke room = **één samengesteld toestel** ("Eetplaats" met 8 onderdelen), zoals de ESP32 zelf. Onderdelen één keer zelf hernoemen in Apple Home |

**Tegels per room** — dezelfde als room80 nu zelf via Matter aanbiedt (ROOM-sketch v2.24, nagekeken in repo `ESP32C6_ROOMS`):
| # | Apple Home-tegel | Bron in de room (`/json`-key, endpoint) |
|---|---|---|
| 1 | Thermostaat: temperatuur, setpoint, Verwarmen = Thuis / Uit = Weg | temp `f` (DS18B20, reserve `e`), setpoint `c`, thuis `v`; `/set_setpoint?setpoint=`, `/set_home?v=` |
| 2 | Vochtigheid | `h` |
| 3 | Beweging MOV1 | `y` |
| 4 | Beweging MOV2 (alleen als ingeschakeld) | `z` |
| 5 | Kleurpicker alle pixels (altijd aan) | `q`/`r`/`s`; `/setcolor?r=&g=&b=` |
| 6–8 | SW1 (pixel 0), SW2 (pixel 1), SW3 (pixels 2+) | `t` (`P=...`); `/toggle_pixel_mode?idx=`, `/toggle_pixel?idx=` |

**Uitvoering F4 (17/10):**
- `server.js` v2.9: `ROOMS_APPLE` (room80 = Eetplaats), `GET /api/rooms-apple`, room-proxy sw1/sw2 zonder blind togglen, `setpoint`, `home`/`hm`, `pN`. **MOV2** wordt gelezen uit het vinkje `name="mov2"` op `/settings` van de room (tot `/capabilities` het meldt). Bijvangst: de pixel- en Thuis/Weg-knoppen in `index.html` werken nu.
- Plugin v0.6: room80 = 1 toestel, 8 onderdelen (Thermostaat, Vochtigheid, Beweging 1, Beweging 2, Kleur, SW1 "TV achtergrondverlichting", SW2 "Muurspots oost", SW3 "Eetplaats lampen"). Opvragen elke 10 s; offline → toestel "geen reactie".
- Getest op de echte room80: `/api/rooms-apple` (mov2 = true, 5 nicknames) en `sw3` aan.

**Stappen (elk na GO):**
1. ✅ **`server.js` v2.9:** room-proxy `/api/set/roomNN` aanvullen. SW1/SW2 gebruiken nu een *toggle*: als Apple "aan" stuurt terwijl de pixel al aan is, gaat hij uit. Nieuw: eerst de toestand lezen en alleen schakelen als het nodig is (zoals SW3 al doet; ook beter voor `verlichting.html`). Setpoint en Thuis/Weg erbij. Vaste lijst `ROOMS_APPLE` met kamernamen.
2. ✅ **Plugin v0.4 → v0.6:** room-tegels. Elke 10 s `/api/poll/room80`, klikken via `/api/set/room80`.
3. **Parallelle test** (F5): gestart 17/10 ±20:15. Duur: FiDel beslist.
4. **F6:** room80 zonder Matter flashen (ROOM-sketch v3.0, repo `ESP32C6_ROOMS`), oude tegels verdwijnen, scènes opnieuw instellen, heapwinst meten. Daarna AP26 (testwoning "Matter Home" verwijderen). Zie plan hieronder.
5. **F7:** volgende rooms = één regel in de lijst, geen nieuwe code.

HVAC (9 tegels), ECO (5 tegels) en de HOME/UIT-knop van het Dashboard hebben ook nog eigen Matter; die volgen in latere fasen. **HVAC: plan in §16.12.** Volgorde per controller zoals bij room80: heap meten → bridge biedt de controller aan → parallelle test → pas dan een sketch zonder Matter (HVAC: `ESP32-HVAC/README.md` §10).

**Plan F6: ROOM-sketch v3.0 zonder Matter (code pas na GO; zelfde plan als Master §7.9):**
| | |
|---|---|
| Vertrekpunt | `TESTROOM/ESP32_C6_ROOM.ino` v2.24 (8 mei 2026), repo `ESP32C6_ROOMS`. ✅ 19/10: room80 antwoordt op `/capabilities` (5 pixelnamen) → **minstens v2.22**. De exacte versie (v2.22/23/24) is niet zichtbaar in de UI; niet nodig voor F6, terugweg = v2.24 uit de repo. Versie zichtbaar maken = ROOM v3.4 (Master §7.9) |
| Eruit | Matter-includes en de 8 endpoints, `Matter.begin()`, callbacks en het doorgeven van waarden naar Matter, `/matter`, `/matter_reset`, `matterNuclearReset()`, Matter-links in de menu's, seriële Matter-commando's |
| Erbij | `"mov2":true/false` in `/capabilities` (`server.js` v2.9 gebruikt dat dan i.p.v. de HTML) |
| Blijft | Alle endpoints die `server.js` gebruikt, webpagina's, NVS-instellingen, heapmeting aan het einde van `setup()`, partitietabel |
| Meten | Vóór (17/10, v2.24 met Matter): **vrij 17 %, grootste blok 31 KB, minimum ooit 7 KB** (`/json` `ad`/`ae`/`af`). Na: dezelfde drie waarden |
| Flashen | OTA via `/update` van room80, of Arduino IDE |
| Terugweg | v2.24 terugflashen |
| Daarna | Oude tegels in "Matter Home" verdwijnen → AP26 (testwoning verwijderen) |

---

### 16.12 HVAC (ESP32 .70) naar de RPi-bridge — plan (19 okt 2026)

*Status: **H1 ✅ en H2 ✅ (21/10/2026)** — toestel "HVAC" staat in Zarlardinge. Volgende: tegels één voor één testen, dan H3 (parallelle test). Code pas na GO, één stap tegelijk.*

**Vertrekpunt**
- De **Photon S-HVAC stuurt nog de echte verwarming**. De ESP32-HVAC (.70, sketch v1.19) is een **testopstelling** (kringen met testinstellingen, alle op room80).
- De drie Photon-knoppen van S-HVAC (§16.9) **blijven ongewijzigd**: *ECO => SCHUUR*, *ECO => WONING* en *HVAC Home*. FiDel hernoemt *HVAC Home* zelf in de Woning-app naar **"HOME"** en zet ze in de kamer **Inkom** (id `hvac-mode` blijft, geen code).
- De ESP32-HVAC staat met zijn eigen Matter (9 tegels) in de woning **"Matter Home"**. Die blijft tijdens de test.
- `server.js` v2.9 kent de HVAC al (`CONTROLLERS.hvac`), `/api/poll/hvac` werkt. **Schakelen werkt nog niet:** de algemene tak stuurt `POST /set`, maar de HVAC heeft enkel GET-endpoints.

**Doel:** één **gecombineerd toestel "HVAC"** in de woning **Zarlardinge**, zoals room80 — met alle onderdelen en metingen van de controller. Daarna parallel testen met de oude ESP32-tegels.

**Onderdelen van het toestel "HVAC"**

| Onderdeel (label, max 16 bytes) | Matter-type | Uit `/json` | Klikken → HVAC (GET) |
|---|---|---|---|
| Boiler boven | temperatuursensor | `b` | — |
| Boiler gem. | temperatuursensor | `h` | — |
| Boiler onder | temperatuursensor | `g` | — |
| 7 kringnamen uit de HVAC-instellingen (B-H2) | 7× stopcontact (`onOffPlugInUnit`) | `p` … `v` | aan → `/circuit_override_on?circuit=N`, uit → `/circuit_override_off?circuit=N` (override 10 min, daarna weer automatisch) |
| Ventilatie | ventilator (FanControl, %) | `x` | % → `/set_vent?vent=%` (minimum 3 u); 0 % of modus uit → `/set_vent?vent=0` (automatisch); aan zonder % → 30 % (zoals de ESP32 nu) |
| ECO => SCHUUR | stopcontact | `y` | aan → `/pump_sch_on` (60 s geforceerd, daarna vanzelf uit); uit → `/pump_sch_cancel` (terug naar automatisch) |
| ECO => WONING | stopcontact | `aa` | aan → `/pump_won_on`; uit → `/pump_won_cancel` |

Gedrag om te weten: de tegel toont altijd de **werkelijke** toestand uit `/json`. Draait een pomp of kring automatisch, dan staat de tegel aan; "uit" tikken tijdens een automatische pompbeurt heft enkel een geforceerde stand op — bij de volgende poll springt de tegel weer aan zolang de automatiek pompt.

**Werkverdeling**

*`server.js` v2.10*
- `HVAC_APPLE = { id: 'hvac', naam: 'HVAC' }` (vaste definitie, zoals `ROOMS_APPLE`).
- `GET /api/hvac-apple` → `{ id, naam, online, kringen: [7 namen] }`. Namen uit `/json_settings` van de HVAC (**enkel** `circuits[].name` overnemen), laatst gekende waarden in `hvac-info.json`, 6 u cache — zodat een offline HVAC zijn tegels en namen houdt. Alleen vanaf het eigen netwerk (zoals `/api/rooms-apple`).
- `POST /api/set/hvac` (nieuwe tak vóór de algemene `/set`-tak): `{k1..k7: 1|0}`, `{vent: 0-100}`, `{sch: 1|0}`, `{won: 1|0}` → de GET-endpoints hierboven.

*Plugin `matterbridge-zarlar` v0.7*
- `maakHvac()`: één toestel (`bridgedNode` + `powerSource`) met de onderdelen als kinderen, vaste id `hvac` → Apple onthoudt kamer en namen.
- Elke 10 s `/api/poll/hvac` → tegels bijwerken; HVAC offline → "geen reactie".
- Klikken → `POST /api/set/hvac`, daarna meteen opnieuw pollen. Eigen updates niet terugsturen (zelfde `context.fabric`-aanpak als bij de rooms).

**Stappenplan**

| Stap | Wat | Meetpunt | Terugweg |
|---|---|---|---|
| **H1** | `server.js` v2.10 — ✅ 21/10: gedeployed; `curl`-test geslaagd (7 namen "Een"…"Zeven", `k1:1` → `p=1`, `"auto"` → cancel) | Vanaf de Mac: `curl` naar `/api/hvac-apple` geeft 7 namen; `POST /api/set/hvac {"k1":1}` → relais 1 klikt, `/json` `p=1`; `{"k1":0}`; `{"vent":40}` → `x=40`; `{"vent":0}`; `{"sch":1}` → `y=1`, na 60 s weer 0 | vorige `server.js` (git) |
| **H2** | Plugin v0.7 — ✅ 21/10: toestel "HVAC" met 13 onderdelen geregistreerd, HVAC online, alles verschenen in Zarlardinge (zonder herstart van de hub). Apple toont de FixedLabel-namen **niet**: alle onderdelen kregen dezelfde naam en zijn door FiDel **eenmalig zelf hernoemd** (blijft bewaard dankzij de vaste id's). ⏳ Nog: elke tegel één keer schakelen | Toestel "HVAC" in Zarlardinge; elke tegel getest; waarden gelijk aan de webpagina van de HVAC; HVAC uit → "geen reactie" | plugin v0.6 |
| **H3** | Parallelle test met de ESP32-tegels in "Matter Home" | Duur beslist FiDel | plugin v0.6 |
| daarna | HVAC-sketch **v2.0 zonder Matter** (`ESP32-HVAC/README.md` §10 stap 3); oude tegels uit "Matter Home" verwijderen | heap-winst gemeten | v1.19 terugflashen |
| later | Overgang naar productie (ESP32 vervangt Photon S-HVAC): Photon-knoppen ECO => SCHUUR/WONING uit `KNOPPEN`; "HOME" naar de HOME/UIT van het Dashboard (§16, aparte fase) | — | — |

**Beslissingen**

| # | Vraag | Beslissing (FiDel, 19 okt 2026) |
|---|---|---|
| B-H1 | Boilertemperaturen: 3 (boven, gem., onder) of alle 6 lagen? | ✅ **3** |
| B-H2 | Kringnamen: vast "Kring 1-7" of uit de HVAC-instellingen (nu "Een"-"Zeven", later BB, WP, …)? | ✅ **uit de HVAC-instellingen** |

**Aandachtspunt:** `/json_settings` van de HVAC geeft ook het WiFi-wachtwoord leesbaar terug. De RPi neemt daar enkel de kringnamen uit over. Het wachtwoord uit dat endpoint halen = aparte, kleine HVAC-versie later (geen nieuwe JSON-key, wel één veld minder).

## 17. Energiebeslissingen op de RPi (+ Tesla bridge)

*Toegevoegd 16 oktober 2026. Status: **plan**. Beslissing FiDel: alles rond energietarieven, batterijplanning en laadkeuzes komt op de RPi-interface. Keuzes van de gebruikers (Maarten en Filip) worden naar de controllers gestuurd die ze uitvoeren.*

### 17.1 Rolverdeling

```text
Gebruikers (Maarten, Filip) ──► RPi portal  (energie-pagina)
                                   │  bewaart instellingen + keuzes + log
                                   │  beslist (handmatig nu, later automatisch)
                                   ▼
                             server.js  /api/energy/...
                                   │  HTTP (LAN)
                     ┌─────────────┴──────────────┐
                     ▼                            ▼
          S-ENERGY .73 (meet, toont)     S-ENERGY /tesla_cmd
          /json, matrix 12×4                 │ HTTP
                                             ▼
                                   tesla-key-esp32 ──BLE──► Tesla1
```

| Laag | Doet | Doet niet |
|---|---|---|
| **RPi** | Tarieven en vaste opslag bewaren (`settings.json`, bestaat al via `/api/settings`) · laadkeuzes en planning bewaren · beslissen · commando's sturen · alles loggen (wie, wat, wanneer, resultaat) | Geen S0/P1 meten. `SIM_S0`/`SIM_P1` nooit omschakelen. |
| **S-ENERGY** | Meten, `/json` serveren, matrix tonen, Tesla-commando's doorgeven aan de proxy | Geen eigen laadlogica (bewuste regel sinds v1.28) |
| **tesla-key-esp32** | BLE-sessie met de auto | Geen logica |

### 17.2 Ontwerpregels
1. **Eén weg naar de auto:** RPi → S-ENERGY `/tesla_cmd`. De RPi praat niet rechtstreeks met de proxy, zodat S-ENERGY de grens `t1_maxa` blijft bewaken.
2. **Veilig bij uitval:** valt de RPi weg, dan gebeurt er niets nieuws. De auto blijft in zijn laatste toestand en S-ENERGY blijft meten.
3. **Begrenzing op twee plaatsen:** de RPi kiest de ampère binnen de grenzen, en S-ENERGY knipt nog eens op `t1_maxa` (5–32 A).
4. **Toegang:** Funnel blijft beschikbaar als eigen toegang zonder VPN, maar achter een login (`server.js` v2.3, zie §3). LAN en Tailscale zonder login. Stuurcommando's (Tesla, `/api/set`) vallen daar automatisch onder. *(Beslist 16/10: geen alleen-lezen-modus voor gasten, want er wordt niet verhuurd en gasten in huis gebruiken het lokale IP.)*
5. **Transparant:** elke actie komt in een log op de RPi (`energy-log.json`), zichtbaar op de pagina, met wie het vroeg (Maarten/Filip).
6. **Tarieven op één plek:** de vaste opslag (nu hardcoded `VAST_CT_KWH` 14,32 in S-ENERGY) komt uit `/api/settings` van de RPi. Dit staat al als openstaand punt in de sketch.

### 17.3 Stappenplan (telkens pas na GO)

| Stap | Wat | Meetpunt |
|---|---|---|
| **E0** | Inventaris: welke energie-instellingen bestaan al in `settings.json` / `epex-grafiek.html`? Welke beslissingen willen Maarten en Filip kunnen nemen? | Lijst akkoord |
| **E1** | `server.js`: read-only proxy `/api/tesla` → S-ENERGY `/tesla_json` + toegangsregel voor stuurcommando's (regel 4) | Tesla-data zichtbaar in portal, Funnel kan niet sturen |
| **E2** | Portal-pagina `energie.html`: Tesla-status + handmatig start/stop/ampère via `/api/tesla/cmd` → S-ENERGY `/tesla_cmd`, met log | Bediening werkt vanaf iPhone (Tailscale) |
| **E3** | Tarieven/vaste opslag centraal: S-ENERGY leest de vaste opslag van `/api/settings` (sketchwijziging, nieuwe versie) | Zelfde ct/kWh op matrix en portal |
| **E4** | Planning: laadvenster per gebruiker (bv. "vóór 7:00 80%"), en goedkoopste EPEX-kwartieren kiezen. De RPi stuurt start/stop. | Laadt in goedkope uren, log klopt |
| **E5** | Automatisch: solar-overschot en piekbewaking (15 kW) → ampère bijsturen | Geen piekoverschrijding, overschot benut |
| **E6** | Later: thuisbatterij (§14) en P1 WON (~2028) in dezelfde beslislaag | — |

### 17.4 Open vragen
| # | Vraag |
|---|---|
| Q1 | Laadt alleen Tesla1 hier, of komt er een tweede EV (EV-lader 2 Maarten, merk/type nog open)? |
| Q2 | Wie mag wat: mogen Maarten en Filip beiden de Tesla bedienen, of elk alleen de eigen auto/verbruik? |
| Q3 | Moet de afrekening (§13) de Tesla-laadenergie apart tonen (solar→Tesla, `t1_`-tellers)? |

---

*Zarlar Portal Projectdocument — Filip Delannoy — 16 oktober 2026 (updated)*

