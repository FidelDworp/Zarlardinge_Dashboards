/* ═══════════════════════════════════════════════════════
   Zarlar Portal — Gedeelde JavaScript
   April 2026 — FiDel
   ═══════════════════════════════════════════════════════ */

'use strict';

// ── Klok ──────────────────────────────────────────────────────────────────────
function zarlarClock(elementId) {
  const el = document.getElementById(elementId);
  if (!el) return;
  function tick() {
    const now = new Date();
    el.textContent = now.toLocaleTimeString('nl-BE', { hour:'2-digit', minute:'2-digit', second:'2-digit' });
  }
  tick();
  setInterval(tick, 1000);
}

// ── Uptime formatter (seconden → leesbaar) ────────────────────────────────────
function zarlarUptime(sec) {
  sec = parseInt(sec) || 0;
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  if (d > 0) return `${d}d ${h}u`;
  if (h > 0) return `${h}u ${m}m`;
  return `${m}m`;
}

// ── Tijdstempel formatter ─────────────────────────────────────────────────────
function zarlarTs(ts) {
  if (!ts) return '—';
  const d = new Date(ts);
  const now = Date.now();
  const diff = Math.floor((now - d) / 1000);
  if (diff < 10)  return 'nu';
  if (diff < 60)  return `${diff}s geleden`;
  if (diff < 3600) return `${Math.floor(diff/60)}m geleden`;
  return d.toLocaleTimeString('nl-BE', { hour:'2-digit', minute:'2-digit' });
}

// ── RSSI → kwaliteit ──────────────────────────────────────────────────────────
function zarlarRssi(rssi) {
  rssi = parseInt(rssi) || -100;
  const pct = Math.min(100, Math.max(0, 2 * (rssi + 100)));
  const kleur = pct >= 70 ? 'green' : pct >= 40 ? 'amber' : 'red';
  return `<span class="data-value ${kleur}">${rssi} dBm</span>`;
}

// ── Heap kleur ────────────────────────────────────────────────────────────────
function zarlarHeap(pct) {
  pct = parseInt(pct) || 0;
  const kleur = pct >= 50 ? 'green' : pct >= 30 ? 'amber' : 'red';
  return `<span class="data-value ${kleur}">${pct}%</span>`;
}

// ── Temperatuur kleur ─────────────────────────────────────────────────────────
function zarlarTemp(t, unit = '°C') {
  t = parseFloat(t);
  if (isNaN(t)) return '<span class="data-value muted">—</span>';
  const kleur = t > 35 ? 'red' : t > 25 ? 'amber' : t > 15 ? 'green' : 'blue';
  return `<span class="data-value ${kleur}">${t.toFixed(1)} ${unit}</span>`;
}

// ── Boolean indicator ─────────────────────────────────────────────────────────
function zarlarBool(val, aanTekst = 'AAN', uitTekst = 'UIT') {
  const aan = parseInt(val) === 1 || val === true;
  return aan
    ? `<span class="data-value green">${aanTekst}</span>`
    : `<span class="data-value muted">${uitTekst}</span>`;
}

// ── Pixel status uit "P=xxxxxxx" string ───────────────────────────────────────
function zarlarPixels(t_str, neo_r, neo_g, neo_b) {
  if (!t_str || typeof t_str !== 'string') return '<span class="data-value muted">—</span>';
  const bits = t_str.replace('P=', '');
  let html = '<div class="pixel-row">';
  let aanCount = 0;
  for (let i = 0; i < bits.length; i++) {
    const aan = bits[i] === '1';
    if (aan) aanCount++;
    const style = aan
      ? `background:rgb(${neo_r||255},${neo_g||255},${neo_b||255})`
      : '';
    html += `<div class="pixel-dot${aan ? ' on' : ''}" style="${style}" title="Pixel ${i}"></div>`;
  }
  html += '</div>';
  return html + `<div style="margin-top:4px;font-family:var(--font-data);font-size:10px;color:var(--text-muted)">${aanCount}/${bits.length} aan</div>`;
}

// ── Fetch met timeout ─────────────────────────────────────────────────────────
async function zarlarFetch(url, options = {}) {
  const ctrl = new AbortController();
  const tid  = setTimeout(() => ctrl.abort(), options.timeout || 8000);
  try {
    const r = await fetch(url, { ...options, signal: ctrl.signal });
    clearTimeout(tid);
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    return await r.json();
  } catch(e) {
    clearTimeout(tid);
    throw e;
  }
}

// ── Auto-refresh mechanisme ───────────────────────────────────────────────────
// Gebruik: zarlarAutoRefresh(15, () => laadData())
function zarlarAutoRefresh(intervalSec, callback) {
  let timer = null;
  const dotEl = document.getElementById('refresh-dot');

  function setPolling(active) {
    if (dotEl) dotEl.classList.toggle('polling', active);
  }

  async function run() {
    setPolling(true);
    try {
      await callback();
    } finally {
      setPolling(false);
    }
    timer = setTimeout(run, intervalSec * 1000);
  }

  // Manuele refresh knop
  const btn = document.getElementById('refresh-btn');
  if (btn) {
    btn.addEventListener('click', () => {
      if (timer) clearTimeout(timer);
      run();
    });
  }

  run();

  return {
    stop: () => { if (timer) clearTimeout(timer); },
    now:  () => { if (timer) clearTimeout(timer); run(); }
  };
}

// ── Status badge HTML ─────────────────────────────────────────────────────────
function zarlarBadge(online, ts) {
  const oud = ts && (Date.now() - ts) > 30000;
  const klass = !ts ? 'pending' : online ? (oud ? 'pending' : 'online') : 'offline';
  const tekst = !ts ? 'wacht' : online ? (oud ? 'oud' : 'online') : 'offline';
  return `<span class="status-badge ${klass}">
    <span class="status-dot"></span>${tekst}
  </span>`;
}

// ── Mini circuit bars (voor HVAC) ─────────────────────────────────────────────
function zarlarCircuitBars(data, keys) {
  let html = '<div class="mini-bars">';
  for (const key of keys) {
    const aan = parseInt(data[key]) === 1;
    html += `<div class="mini-bar ${aan ? 'on' : 'off'}" title="Circuit ${key}: ${aan ? 'AAN' : 'UIT'}"></div>`;
  }
  html += '</div>';
  return html;
}

// ── Boiler temperatuurband (voor ECO) ────────────────────────────────────────
function zarlarBoilerBand(keys, data) {
  const temps = keys.map(k => parseFloat(data[k]) || 0);
  const max   = 90, min = 20;
  let html = '<div style="display:flex;gap:2px;margin-top:6px;height:28px;align-items:flex-end;">';
  for (const t of temps) {
    const pct = Math.max(5, Math.min(100, (t - min) / (max - min) * 100));
    const r   = Math.floor(Math.min(255, (t - min) / (max - min) * 255));
    const b   = Math.floor(Math.max(0, 255 - r));
    html += `<div style="flex:1;height:${pct}%;background:rgb(${r},80,${b});border-radius:2px 2px 0 0;" title="${t.toFixed(1)}°C"></div>`;
  }
  html += '</div>';
  return html;
}
