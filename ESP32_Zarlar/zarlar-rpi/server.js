// ─────────────────────────────────────────────────────────────────────────────
// Zarlar Dashboard Server v1.0 — Raspberry Pi
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

// ESP32 controllers op het lokale netwerk
const CONTROLLERS = {
  senrg: 'http://192.168.0.73',  // Smart Energy controller
  eco:   'http://192.168.0.71',  // ECO Boiler
  hvac:  'http://192.168.0.70',  // HVAC
  room:  'http://192.168.0.80',  // Room controller
};

// ── Standaard instellingen ────────────────────────────────────────────────────
const DEFAULT_SETTINGS = {
  vast_prijs:    28.0,
  bat_kwh:       10,
  soc_start:     50,
  solar_max_kw:  8,
  max_piek:      15,
  cap_tar:       4.517,
  abo_eur:       5.00,
  databeheer:    1.49,
  maand_kwh:     1300,
  t_fluvius:     5.23,
  t_elia:        1.10,
  t_hernieuwbaar:0.39,
  t_heffingen:   4.94,
  t_btw_pct:     6.0,
};

// ── Settings ──────────────────────────────────────────────────────────────────
app.get('/api/settings', (req, res) => {
  try {
    if (!fs.existsSync(SETTINGS_FILE)) {
      fs.writeFileSync(SETTINGS_FILE, JSON.stringify(DEFAULT_SETTINGS, null, 2));
    }
    res.json(JSON.parse(fs.readFileSync(SETTINGS_FILE, 'utf8')));
  } catch(e) {
    res.json(DEFAULT_SETTINGS);
  }
});

app.post('/api/settings', (req, res) => {
  try {
    // Merge met bestaande instellingen zodat nieuwe keys bewaard blijven
    let current = DEFAULT_SETTINGS;
    if (fs.existsSync(SETTINGS_FILE)) {
      current = { ...current, ...JSON.parse(fs.readFileSync(SETTINGS_FILE,'utf8')) };
    }
    const merged = { ...current, ...req.body };
    fs.writeFileSync(SETTINGS_FILE, JSON.stringify(merged, null, 2));
    res.json({ ok: true });
  } catch(e) {
    res.status(500).json({ error: e.message });
  }
});

// ── EPEX data (server-side fetch, geen CORS-proxy nodig) ──────────────────────
app.get('/api/epex', async (req, res) => {
  try {
    // Cache check (26 uur geldig)
    if (fs.existsSync(EPEX_CACHE)) {
      const cache = JSON.parse(fs.readFileSync(EPEX_CACHE,'utf8'));
      if (Date.now() - cache.ts < 26 * 3600000) {
        return res.json({ ...cache, bron: 'cache' });
      }
    }
    // Haal vandaag + morgen op
    const nu     = new Date();
    const morgen = new Date(nu); morgen.setDate(nu.getDate()+1);
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

    const unix_seconds = [
      ...(vandaag.unix_seconds || []),
      ...(morgenD?.unix_seconds || [])
    ];
    const price = [
      ...(vandaag.price || []),
      ...(morgenD?.price || [])
    ];

    const result = { unix_seconds, price, ts: Date.now(), bron: 'live',
                     morgen_beschikbaar: !!morgenD?.price?.length };
    fs.writeFileSync(EPEX_CACHE, JSON.stringify(result));
    res.json(result);

  } catch(e) {
    // Geef gecachte data terug als fallback
    if (fs.existsSync(EPEX_CACHE)) {
      const cache = JSON.parse(fs.readFileSync(EPEX_CACHE,'utf8'));
      return res.json({ ...cache, bron: 'cache_fallback' });
    }
    res.status(503).json({ error: 'EPEX niet beschikbaar', detail: e.message });
  }
});

// ── Poll ESP32 controller ─────────────────────────────────────────────────────
app.get('/api/poll/:controller', async (req, res) => {
  const baseUrl = CONTROLLERS[req.params.controller];
  if (!baseUrl) return res.status(404).json({ error: 'Onbekende controller' });
  try {
    const r    = await fetch(baseUrl + '/json', { timeout: 3000 });
    const data = await r.json();
    res.json({ ...data, online: true, ts: Date.now() });
  } catch(e) {
    res.status(503).json({ online: false, error: 'Controller offline', detail: e.message });
  }
});

// ── Stuur commando naar controller ────────────────────────────────────────────
app.post('/api/set/:controller', async (req, res) => {
  const baseUrl = CONTROLLERS[req.params.controller];
  if (!baseUrl) return res.status(404).json({ error: 'Onbekende controller' });
  try {
    const r = await fetch(baseUrl + '/set', {
      method:  'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify(req.body),
      timeout: 3000
    });
    res.json(await r.json());
  } catch(e) {
    res.status(503).json({ error: 'Commando mislukt', detail: e.message });
  }
});

// ── Status alle controllers ───────────────────────────────────────────────────
app.get('/api/status', async (req, res) => {
  const resultaten = {};
  await Promise.all(Object.entries(CONTROLLERS).map(async ([naam, url]) => {
    try {
      const r = await fetch(url + '/json', { timeout: 2000 });
      const d = await r.json();
      resultaten[naam] = { online: true, ip: url, ...d };
    } catch {
      resultaten[naam] = { online: false, ip: url };
    }
  }));
  res.json(resultaten);
});

const PORT = 3000;
app.listen(PORT, '0.0.0.0', () => {
  console.log(`✓ Zarlar Dashboard draait op http://192.168.0.50:${PORT}`);
  console.log(`  Settings: ${SETTINGS_FILE}`);
  console.log(`  EPEX cache: ${EPEX_CACHE}`);
});
