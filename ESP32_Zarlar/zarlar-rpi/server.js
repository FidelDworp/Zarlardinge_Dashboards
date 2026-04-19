// ─────────────────────────────────────────────────────────────────────────────
// Zarlar Dashboard Server v2.0 — Raspberry Pi
// Node.js + Express — draait op poort 3000
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
  senrg:  'http://192.168.0.73',  // Smart Energy (toekomstig)
  room:   'http://192.168.0.80',  // Room controller (Testroom)
  room1:  'http://192.168.0.80',  // Alias voor Testroom
  // Toekomstige rooms:
  // bandb:  'http://192.168.0.75',
  // badk:   'http://192.168.0.76',
  // inkom:  'http://192.168.0.77',
  // keuken: 'http://192.168.0.78',
  // waspl:  'http://192.168.0.79',
  // eetpl:  'http://192.168.0.80',  // wordt .80 als testroom verhuist
  // zitpl:  'http://192.168.0.81',
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

app.get('/api/epex', async (req, res) => {
  try {
    if (fs.existsSync(EPEX_CACHE)) {
      const cache = readJSON(EPEX_CACHE);
      if (cache) {
        const nu          = new Date();
        const ouderDan26u = Date.now() - cache.ts > 26 * 3600000;
        const na13u       = nu.getHours() >= 13;
        const mismorgen   = !cache.morgen_beschikbaar;
        const cachedag    = new Date(cache.ts);
        const daggewisseld = cachedag.getDate()  !== nu.getDate() ||
                             cachedag.getMonth() !== nu.getMonth();
        const cacheGeldig = !ouderDan26u && !(na13u && mismorgen) && !daggewisseld;
        if (cacheGeldig) return res.json({ ...cache, bron: 'cache' });
      }
    }

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
    res.json(result);

  } catch(e) {
    const cache = readJSON(EPEX_CACHE);
    if (cache) return res.json({ ...cache, bron: 'cache_fallback' });
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
// ═══════════════════════════════════════════════════════════════════════════════

app.post('/api/set/:controller', async (req, res) => {
  const baseUrl = CONTROLLERS[req.params.controller];
  if (!baseUrl) return res.status(404).json({ error: 'Onbekende controller' });
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
    if (naam === 'room1') return;
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
  console.log(`✓ Zarlar Portal v2.0 draait op http://192.168.0.50:${PORT}`);
  console.log(`  Controllers: ${Object.keys(CONTROLLERS).filter(k=>k!=='room1').join(', ')}`);
  console.log(`  Settings:    ${SETTINGS_FILE}`);
  console.log(`  Scenes:      ${SCENES_FILE}`);
  console.log(`  Caps cache:  ${CAPS_CACHE}`);
});
