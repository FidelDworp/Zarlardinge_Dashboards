// ─────────────────────────────────────────────────────────────────────────────
// Zarlar Dashboard Server v2.2 — Raspberry Pi
// Node.js + Express — draait op poort 3000
//
// v2.2 (16 okt 2026):
//   - /api/epex: cache enkel nog geldig als ze "compleet" is.
//     Vroeger bleef de cache 26 u geldig, ook zonder de prijzen van morgen
//     → prijzen van morgen verschenen soms pas de volgende ochtend.
//     Nu: vóór 12:45 volstaat vandaag; vanaf 12:45 moet morgen erin zitten,
//     anders opnieuw ophalen (max. 1× per 10 min). Optioneel ?force=1 (max. 1×/min).
//   - Nieuw veld in respons: slots_vandaag, slots_morgen. Bestaande velden ongewijzigd.
//
// v2.1 (01 mei 2026):
//   - ROOM controller proxy in /api/set endpoint
//   - Vertaalt Matter switches (sw1/sw2/sw3) naar ROOM GET endpoints
//   - Room75-81 controllers geactiveerd
// ─────────────────────────────────────────────────────────────────────────────
const express = require('express');
const fetch   = require('node-fetch');
const fs      = require('fs');
const path    = require('path');
const app     = express();

app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

const SETTINGS_FILE = path.join(__dirname, 'settings.json');
const EPEX_CACHE    = path.join(__dirname, 'epex-cache.json');
const SCENES_FILE   = path.join(__dirname, 'scenes.json');
const CAPS_CACHE    = path.join(__dirname, 'controller-configs.json');

// ── ESP32 controllers op het lokale netwerk ───────────────────────────────────
const CONTROLLERS = {
  zarlar: 'http://192.168.0.60',  // Zarlar Dashboard (bron van waarheid, matrix)
  hvac:   'http://192.168.0.70',  // HVAC
  eco:    'http://192.168.0.71',  // ECO Boiler
  senrg:  'http://192.168.0.73',  // Smart Energy
  room:   'http://192.168.0.80',  // Alias voor room80
  room1:  'http://192.168.0.80',  // Alias (backwards compatibility)
  // Room controllers (v2.1):
  room75: 'http://192.168.0.75',  // B&B
  room76: 'http://192.168.0.76',  // Badkamer
  room77: 'http://192.168.0.77',  // Inkomhal
  room78: 'http://192.168.0.78',  // Keuken
  room79: 'http://192.168.0.79',  // Wasplaats
  room80: 'http://192.168.0.80',  // Eetplaats
  room81: 'http://192.168.0.81',  // Zitplaats
};

// ── Photon controllers (via Cloudflare Worker proxy) ──────────────────────────
const PHOTON_WORKER = 'https://controllers-diagnose.filip-delannoy.workers.dev/sensor?id=';

const PHOTON_IDS = {
  'P-BandB':    '30002c000547343233323032',
  'P-Badkamer': '5600420005504b464d323520',
  'P-Inkom':    '2c0026000747343232363230',
  'P-Keuken':   '310017001647373335333438',
  'P-Waspl':    '33004f000e504b464d323520',
  'P-Eetpl':    '3c0030000a47353138383138',
  'P-Zitpl':    '200033000547373336323230',
};

// ── Standaard instellingen ────────────────────────────────────────────────────
const DEFAULT_SETTINGS = {
  vast_prijs:      28.0,
  bat_kwh:         10,
  soc_start:       50,
  solar_max_kw:    8,
  max_piek:        15,
  cap_tar:         4.517,
  abo_eur:         5.00,
  databeheer:      1.49,
  maand_kwh:       1300,
  t_fluvius:       5.23,
  t_elia:          1.10,
  t_hernieuwbaar:  0.39,
  t_heffingen:     4.94,
  t_btw_pct:       6.0,
};

// ── Hulpfunctie: bestand lezen of null ───────────────────────────────────────
function readJSON(filePath, fallback = null) {
  try {
    if (fs.existsSync(filePath)) return JSON.parse(fs.readFileSync(filePath, 'utf8'));
  } catch(e) { /* corrupt of leeg */ }
  return fallback;
}

function writeJSON(filePath, data) {
  fs.writeFileSync(filePath, JSON.stringify(data, null, 2));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SETTINGS
// ═══════════════════════════════════════════════════════════════════════════════

app.get('/api/settings', (req, res) => {
  try {
    if (!fs.existsSync(SETTINGS_FILE)) writeJSON(SETTINGS_FILE, DEFAULT_SETTINGS);
    res.json(readJSON(SETTINGS_FILE, DEFAULT_SETTINGS));
  } catch(e) { res.json(DEFAULT_SETTINGS); }
});

app.post('/api/settings', (req, res) => {
  try {
    let current = { ...DEFAULT_SETTINGS, ...readJSON(SETTINGS_FILE, {}) };
    writeJSON(SETTINGS_FILE, { ...current, ...req.body });
    res.json({ ok: true });
  } catch(e) { res.status(500).json({ error: e.message }); }
});

// ═══════════════════════════════════════════════════════════════════════════════
// EPEX DATA (server-side fetch, geen CORS-proxy nodig)
// ═══════════════════════════════════════════════════════════════════════════════

// v2.2: slimme cache — zie header. Responsformaat ongewijzigd (+ slots_vandaag/slots_morgen).
const EPEX_REFETCH_MS   = 10 * 60 * 1000;  // max. 1 fetch per 10 min zolang morgen ontbreekt
const EPEX_FORCE_MS     = 60 * 1000;       // ?force=1: max. 1 fetch per minuut
const EPEX_PUBLICATIE_H = 12, EPEX_PUBLICATIE_M = 45;  // EPEX publiceert normaal ±12:55–13:05
let   epexLaatsteFetch  = 0;               // millis van laatste poging bij energy-charts

function epexDagStart(offsetDagen) {
  const d = new Date(); d.setHours(0, 0, 0, 0); d.setDate(d.getDate() + offsetDagen);
  return Math.floor(d.getTime() / 1000);
}
function epexSlotsOpDag(cache, offsetDagen) {
  if (!cache || !Array.isArray(cache.unix_seconds)) return 0;
  const van = epexDagStart(offsetDagen), tot = epexDagStart(offsetDagen + 1);
  return cache.unix_seconds.filter(s => s >= van && s < tot).length;
}
function epexNaPublicatie() {
  const n = new Date();
  return n.getHours() > EPEX_PUBLICATIE_H ||
        (n.getHours() === EPEX_PUBLICATIE_H && n.getMinutes() >= EPEX_PUBLICATIE_M);
}
function epexMetTellers(cache, bron) {
  return { ...cache, bron,
           slots_vandaag: epexSlotsOpDag(cache, 0),
           slots_morgen:  epexSlotsOpDag(cache, 1) };
}

app.get('/api/epex', async (req, res) => {
  const cache       = readJSON(EPEX_CACHE);
  const heeftVandaag = epexSlotsOpDag(cache, 0) >= 23;   // ≥23: werkt voor uur- én kwartierdata (DST-dag 23 u)
  const heeftMorgen  = epexSlotsOpDag(cache, 1) >= 23;
  const force        = req.query.force === '1';
  const sindsFetch   = Date.now() - epexLaatsteFetch;

  // 1) Cache compleet genoeg? → teruggeven
  const cacheGeldig = heeftVandaag && (heeftMorgen || !epexNaPublicatie());
  if (cacheGeldig && !(force && sindsFetch >= EPEX_FORCE_MS))
    return res.json(epexMetTellers(cache, 'cache'));

  // 2) Niet compleet, maar recent nog geprobeerd → cache teruggeven (energy-charts niet overspoelen)
  const wachttijd = force ? EPEX_FORCE_MS : EPEX_REFETCH_MS;
  if (heeftVandaag && sindsFetch < wachttijd)
    return res.json(epexMetTellers(cache, 'cache'));

  // 3) Ophalen bij energy-charts
  epexLaatsteFetch = Date.now();
  try {
    const nu     = new Date();
    const morgen = new Date(nu); morgen.setDate(nu.getDate() + 1);
    const fmt    = d => `${d.getFullYear()}-${String(d.getMonth()+1).padStart(2,'0')}-${String(d.getDate()).padStart(2,'0')}`;

    async function haalDag(datum) {
      const url = `https://api.energy-charts.info/price?bzn=BE&start=${datum}&end=${datum}`;
      const r   = await fetch(url, { timeout: 10000 });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      return r.json();
    }

    const [dV, dM] = await Promise.allSettled([haalDag(fmt(nu)), haalDag(fmt(morgen))]);
    const vandaag  = dV.status === 'fulfilled' ? dV.value : null;
    const morgenD  = dM.status === 'fulfilled' ? dM.value : null;

    if (!vandaag || !vandaag.price?.length) throw new Error('Geen EPEX data');

    const result = {
      unix_seconds: [...(vandaag.unix_seconds||[]), ...(morgenD?.unix_seconds||[])],
      price:        [...(vandaag.price||[]),        ...(morgenD?.price||[])],
      ts:           Date.now(),
      bron:         'live',
      morgen_beschikbaar: !!morgenD?.price?.length,
    };
    writeJSON(EPEX_CACHE, result);
    const uit = epexMetTellers(result, 'live');
    console.log(`[EPEX] fetch ${force ? '(force) ' : ''}OK — vandaag ${uit.slots_vandaag} slots, morgen ${uit.slots_morgen} slots`);
    res.json(uit);

  } catch(e) {
    console.log(`[EPEX] fetch mislukt: ${e.message}`);
    if (cache) return res.json(epexMetTellers(cache, 'cache_fallback'));
    res.status(503).json({ error: 'EPEX niet beschikbaar', detail: e.message });
  }
});

// ═══════════════════════════════════════════════════════════════════════════════
// POLL ESP32 CONTROLLER  →  /json
// ═══════════════════════════════════════════════════════════════════════════════

app.get('/api/poll/:controller', async (req, res) => {
  const baseUrl = CONTROLLERS[req.params.controller];
  if (!baseUrl) return res.status(404).json({ error: 'Onbekende controller', beschikbaar: Object.keys(CONTROLLERS) });
  try {
    const r    = await fetch(baseUrl + '/json', { timeout: 3000 });
    const data = await r.json();
    res.json({ ...data, online: true, ts: Date.now() });
  } catch(e) {
    res.status(503).json({ online: false, error: 'Controller offline', detail: e.message });
  }
});

// ═══════════════════════════════════════════════════════════════════════════════
// PHOTON PROXY  →  /api/photon/:id
// ═══════════════════════════════════════════════════════════════════════════════

app.get('/api/photon/:id', async (req, res) => {
  try {
    const r    = await fetch(PHOTON_WORKER + req.params.id, { timeout: 4000 });
    const data = await r.json();
    res.json(data);
  } catch(e) {
    res.status(503).json({ error: 'Photon niet bereikbaar', detail: e.message });
  }
});

// ═══════════════════════════════════════════════════════════════════════════════
// MATRIX DATA  →  /api/matrix
// ═══════════════════════════════════════════════════════════════════════════════

app.get('/api/matrix', async (req, res) => {
  const result = {};

  // ESP32 controllers parallel ophalen
  const esp32Keys = Object.entries(CONTROLLERS);
  await Promise.all(esp32Keys.map(async ([naam, url]) => {
    try {
      const r = await fetch(url + '/json', { timeout: 2000 });
      result[naam] = { ...(await r.json()), online: true };
    } catch {
      result[naam] = { online: false };
    }
  }));

  // Photons parallel ophalen
  await Promise.all(Object.entries(PHOTON_IDS).map(async ([naam, id]) => {
    try {
      const r    = await fetch(PHOTON_WORKER + id, { timeout: 4000 });
      const data = await r.json();
      result[naam] = { ...data, online: data.online === 1 || data['online'] === true };
    } catch {
      result[naam] = { online: false };
    }
  }));

  result._ts = Date.now();
  res.json(result);
});

// ═══════════════════════════════════════════════════════════════════════════════
// CAPABILITIES  →  /api/capabilities/:naam
// Haalt /capabilities op van controller, cachet in controller-configs.json
// ═══════════════════════════════════════════════════════════════════════════════

app.get('/api/capabilities/:controller', async (req, res) => {
  const naam    = req.params.controller;
  const baseUrl = CONTROLLERS[naam];
  if (!baseUrl) return res.status(404).json({ error: 'Onbekende controller' });

  const force = req.query.refresh === '1';

  // Cache ophalen (6 uur geldig, tenzij force refresh)
  const cache = readJSON(CAPS_CACHE, {});
  if (!force && cache[naam] && (Date.now() - (cache[naam]._ts || 0)) < 6 * 3600000) {
    return res.json({ ...cache[naam], bron: 'cache' });
  }

  try {
    const r    = await fetch(baseUrl + '/capabilities', { timeout: 3000 });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    const data = await r.json();
    cache[naam] = { ...data, _ts: Date.now() };
    writeJSON(CAPS_CACHE, cache);
    res.json({ ...data, bron: 'live' });
  } catch(e) {
    // Geef cache terug als fallback, ook als verlopen
    if (cache[naam]) return res.json({ ...cache[naam], bron: 'cache_fallback' });
    res.status(503).json({ error: 'Capabilities niet beschikbaar', detail: e.message });
  }
});

// ═══════════════════════════════════════════════════════════════════════════════
// STUUR COMMANDO NAAR CONTROLLER  →  /set
// v2.1: ROOM controllers proxy (sw1/sw2/sw3 → GET endpoints)
// ═══════════════════════════════════════════════════════════════════════════════

app.post('/api/set/:controller', async (req, res) => {
  const naam    = req.params.controller;
  const baseUrl = CONTROLLERS[naam];
  if (!baseUrl) return res.status(404).json({ error: 'Onbekende controller' });
  
  const body = req.body;
  
  // ─── ROOM CONTROLLER PROXY (v2.1) ──────────────────────────────────────────
  // ROOM heeft geen /set POST endpoint, alleen GET endpoints:
  //   /toggle_mov?idx=N, /toggle_pixel?idx=N, /setcolor?r=R&g=G&b=B
  // Vertaal Matter switch types (sw1, sw2, sw3, r/g/b) naar deze endpoints
  
  if (naam.startsWith('room')) {
    try {
      const results = [];
      
      // SW1: Pixel 0 (MOV override)
      if (body.sw1 !== undefined) {
        const url = `${baseUrl}/toggle_pixel_mode?idx=0`;
        const resp = await fetch(url, { timeout: 3000 });
        results.push({ sw1: resp.ok });
      }
      
      // SW2: Pixel 1
      if (body.sw2 !== undefined) {
        const url = `${baseUrl}/toggle_pixel_mode?idx=1`;
        const resp = await fetch(url, { timeout: 3000 });
        results.push({ sw2: resp.ok });
      }
      
      // SW3: Pixels 2+ (AAN/UIT, niet toggle)
      if (body.sw3 !== undefined) {
        const targetState = body.sw3 === 1; // true = AAN, false = UIT
        
        // Haal eerst /json op om huidige state te weten
        const jsonResp = await fetch(`${baseUrl}/json`, { timeout: 3000 });
        const data = await jsonResp.json();
        const pixelStr = (data.t || '').replace('P=', '');
        
        let toggled = 0;
        // Toggle alleen pixels die niet in gewenste state zijn
        for (let i = 2; i < pixelStr.length; i++) {
          const currentState = pixelStr[i] === '1';
          if (currentState !== targetState) {
            await fetch(`${baseUrl}/toggle_pixel?idx=${i}`, { timeout: 3000 });
            toggled++;
          }
        }
        results.push({ sw3: true, pixels: pixelStr.length - 2, toggled });
      }
      
      // RGB Color (alle pixels)
      if (body.r !== undefined && body.g !== undefined && body.b !== undefined) {
        const url = `${baseUrl}/setcolor?r=${body.r}&g=${body.g}&b=${body.b}`;
        const resp = await fetch(url, { timeout: 3000 });
        results.push({ color: resp.ok, rgb: [body.r, body.g, body.b] });
      }
      
      return res.json({ ok: true, controller: naam, results });
      
    } catch(e) {
      return res.status(503).json({ error: 'ROOM commando mislukt', detail: e.message });
    }
  }
  
  // ─── ANDERE CONTROLLERS (origineel gedrag) ─────────────────────────────────
  // HVAC, ECO, SENRG hebben wel een /set POST endpoint
  try {
    const r = await fetch(baseUrl + '/set', {
      method:  'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify(req.body),
      timeout: 3000,
    });
    res.json(await r.json());
  } catch(e) {
    res.status(503).json({ error: 'Commando mislukt', detail: e.message });
  }
});

// ═══════════════════════════════════════════════════════════════════════════════
// STATUS ALLE CONTROLLERS
// ═══════════════════════════════════════════════════════════════════════════════

app.get('/api/status', async (req, res) => {
  const resultaten = {};
  await Promise.all(Object.entries(CONTROLLERS).map(async ([naam, url]) => {
    // Sla dubbele aliases over (room1 = room)
    if (naam === 'room1' || naam === 'room') return;
    try {
      const r = await fetch(url + '/json', { timeout: 2000 });
      const d = await r.json();
      resultaten[naam] = { online: true, ip: url.replace('http://',''), ts: Date.now(), ...d };
    } catch {
      resultaten[naam] = { online: false, ip: url.replace('http://',''), ts: Date.now() };
    }
  }));
  res.json(resultaten);
});

// ═══════════════════════════════════════════════════════════════════════════════
// SCENES — lezen / schrijven / uitvoeren
// ═══════════════════════════════════════════════════════════════════════════════

// Alle scenes ophalen
app.get('/api/scenes', (req, res) => {
  res.json(readJSON(SCENES_FILE, []));
});

// Scene toevoegen of bijwerken (op basis van naam)
app.post('/api/scenes', (req, res) => {
  try {
    const scenes = readJSON(SCENES_FILE, []);
    const nieuw  = req.body;
    if (!nieuw.naam) return res.status(400).json({ error: 'naam verplicht' });

    const idx = scenes.findIndex(s => s.naam === nieuw.naam);
    if (idx >= 0) scenes[idx] = nieuw;
    else          scenes.push(nieuw);

    writeJSON(SCENES_FILE, scenes);
    res.json({ ok: true, scenes });
  } catch(e) { res.status(500).json({ error: e.message }); }
});

// Scene verwijderen
app.delete('/api/scenes/:naam', (req, res) => {
  try {
    const scenes  = readJSON(SCENES_FILE, []);
    const gefilterd = scenes.filter(s => s.naam !== req.params.naam);
    writeJSON(SCENES_FILE, gefilterd);
    res.json({ ok: true });
  } catch(e) { res.status(500).json({ error: e.message }); }
});

// Scene uitvoeren
app.post('/api/scenes/:naam/run', async (req, res) => {
  const scenes = readJSON(SCENES_FILE, []);
  const scene  = scenes.find(s => s.naam === req.params.naam);
  if (!scene) return res.status(404).json({ error: 'Scene niet gevonden' });

  const resultaten = [];
  for (const actie of (scene.acties || [])) {
    const baseUrl = CONTROLLERS[actie.controller];
    if (!baseUrl) {
      resultaten.push({ controller: actie.controller, ok: false, error: 'Onbekende controller' });
      continue;
    }
    try {
      const body = { [actie.key]: actie.waarde };
      const r    = await fetch(baseUrl + '/set', {
        method:  'POST',
        headers: { 'Content-Type': 'application/json' },
        body:    JSON.stringify(body),
        timeout: 3000,
      });
      resultaten.push({ controller: actie.controller, key: actie.key, ok: r.ok });
    } catch(e) {
      resultaten.push({ controller: actie.controller, key: actie.key, ok: false, error: e.message });
    }
  }

  // Scene-uitvoering loggen
  scene.laatste_uitvoering = new Date().toISOString();
  const idx = scenes.findIndex(s => s.naam === scene.naam);
  if (idx >= 0) { scenes[idx] = scene; writeJSON(SCENES_FILE, scenes); }

  const succes = resultaten.filter(r => r.ok).length;
  res.json({ ok: succes > 0, scene: scene.naam, acties: resultaten.length, succes });
});

// ═══════════════════════════════════════════════════════════════════════════════
// CONTROLLERS OVERZICHT (voor portal navigatie)
// ═══════════════════════════════════════════════════════════════════════════════

app.get('/api/controllers', (req, res) => {
  const lijst = Object.entries(CONTROLLERS)
    .filter(([naam]) => naam !== 'room1')  // geen aliases
    .map(([naam, url]) => ({ naam, ip: url.replace('http://','') }));
  res.json(lijst);
});

// ═══════════════════════════════════════════════════════════════════════════════
// START
// ═══════════════════════════════════════════════════════════════════════════════

const PORT = 3000;
app.listen(PORT, '0.0.0.0', () => {
  console.log(`✓ Zarlar Portal v2.1 draait op http://192.168.0.50:${PORT}`);
  console.log(`  Controllers: ${Object.keys(CONTROLLERS).filter(k=>k!=='room1').join(', ')}`);
  console.log(`  Settings:    ${SETTINGS_FILE}`);
  console.log(`  Scenes:      ${SCENES_FILE}`);
  console.log(`  Caps cache:  ${CAPS_CACHE}`);
});
