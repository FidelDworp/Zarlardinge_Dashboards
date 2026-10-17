// ─────────────────────────────────────────────────────────────────────────────
// matterbridge-zarlar v0.3 — Zarlar Photon-knoppen in Apple Home
// Filip Delannoy — Portal Plan §16.9 (F4a)
//
// Apple Home ──► Matterbridge (deze plugin) ──► server.js (RPi) ──► Particle ──► Photon
//
// v0.3 (17 okt 2026):
//   - soort "kleur" → Matter extendedColorLight (server.js ≥ v2.8). Tint en verzadiging gaan als
//     {"aan","hue":0-360,"sat":0-100} naar de portal. Apple kan kleuren sturen als tint/verzadiging,
//     als XY of als kleurtemperatuur: XY wordt omgerekend naar tint, kleurtemperatuur = wit (sat 0).
//     Helderheid wordt genegeerd (de Photon kent alleen de kleuren). Zelfde verzamelvenster van 400 ms.
//
// v0.2 (17 okt 2026):
//   - soort "dimmer" → Matter dimmableLight (server.js ≥ v2.7). Aan/uit en niveau (0-100 %)
//     gaan samen naar POST /api/photon/:knop {"aan","niveau"}. Apple stuurt vaak "on" en
//     "moveToLevel" vlak na elkaar: per dimmer 400 ms verzamelen, dan één commando.
//     Het antwoord zet de schuif op het niveau van de gekozen stand (bv. 33/67/100 %).
//
// v0.1 (17 okt 2026):
//   - Haalt de knoppen op bij GET /api/knoppen van de Zarlar Portal (server.js ≥ v2.5).
//     Geen eigen lijst: server.js is de enige bron.
//   - soort "lamp" → Matter onOffLight, soort "schakelaar" → onOffPlugInUnit (stopcontact).
//   - Vaste id per knop (bv. "trap-rood") → Apple Home houdt kamer en naam bij herstart.
//   - Klik → POST /api/photon/:knop {"aan":true|false}. Het antwoord bevat de toestand
//     van de hele groep; die wordt overgenomen (standen exclusief, actieve stand springt
//     terug aan). Geen of fout antwoord → schakelaar springt terug.
//   - Portal onbereikbaar bij start → elke 10 s opnieuw, nooit registreren met een lege
//     lijst (anders verdwijnen de toestellen uit Apple Home).
//   - Portal-adres: config "portal", standaard http://192.168.0.50:3000
//     (niet localhost: dat behandelt server.js als Funnel-verkeer).
// ─────────────────────────────────────────────────────────────────────────────
import { MatterbridgeDynamicPlatform, MatterbridgeEndpoint, onOffLight, onOffPlugInUnit, dimmableLight, extendedColorLight } from 'matterbridge';
import { OnOff, LevelControl, ColorControl } from 'matterbridge/matter/clusters';

const VERSIE  = '0.3.0';
const PORTAL  = 'http://192.168.0.50:3000';
const wacht   = ms => new Promise(r => setTimeout(r, ms));
const naarPct = lvl => Math.max(1, Math.min(100, Math.round((lvl ?? 254) * 100 / 254)));   // Matter 1-254 → %
const naarLvl = pct => Math.max(1, Math.min(254, Math.round((pct ?? 100) * 254 / 100)));   // % → Matter 1-254
const hueGr   = h => Math.round((h ?? 0) * 360 / 254);    // Matter tint 0-254 → graden
const satPct  = s => Math.round((s ?? 0) * 100 / 254);    // Matter verzadiging 0-254 → %

// Matter XY (0-65535) → tint (graden) en verzadiging (%), via lineair sRGB
function xyNaarHs(cx, cy) {
  const x = cx / 65535, y = Math.max(cy / 65535, 0.0001);
  const X = x / y, Z = (1 - x - y) / y;
  const rgb = [ 3.2406 * X - 1.5372 - 0.4986 * Z, -0.9689 * X + 1.8758 + 0.0415 * Z, 0.0557 * X - 0.2040 + 1.0570 * Z ]
    .map(v => Math.max(0, v));
  const [r, g, b] = rgb, max = Math.max(r, g, b), min = Math.min(r, g, b), d = max - min;
  if (max === 0 || d === 0) return { hue: 0, sat: 0 };
  let h = max === r ? ((g - b) / d) % 6 : max === g ? (b - r) / d + 2 : (r - g) / d + 4;
  h = Math.round(h * 60); if (h < 0) h += 360;
  return { hue: h, sat: Math.round(d / max * 100) };
}

export default function initializePlugin(matterbridge, log, config) {
  return new ZarlarPlatform(matterbridge, log, config);
}

export class ZarlarPlatform extends MatterbridgeDynamicPlatform {
  constructor(matterbridge, log, config) {
    super(matterbridge, log, config);
    if (typeof this.verifyMatterbridgeVersion !== 'function' || !this.verifyMatterbridgeVersion('3.10.10')) {
      throw new Error(`matterbridge-zarlar vereist Matterbridge ≥ 3.10.10 (nu ${this.matterbridge.matterbridgeVersion})`);
    }
    this.portal     = config.portal || PORTAL;
    this.toestellen = new Map();   // knop-id → MatterbridgeEndpoint
    this.dimmers    = new Set();   // knop-id's van dimmers
    this.kleuren    = new Set();   // knop-id's van kleurenlampen
    this.wachtrij   = new Map();   // dimmer/kleur-id → { aan, niveau|hue|sat, timer }
    this.gestopt    = false;
    this.log.info(`Zarlar v${VERSIE} — portal ${this.portal}`);
  }

  // Lijst ophalen; blijft proberen tot het lukt (of tot shutdown)
  async haalKnoppen() {
    for (let poging = 1; !this.gestopt; poging++) {
      try {
        const r = await fetch(this.portal + '/api/knoppen', { signal: AbortSignal.timeout(5000) });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        const lijst = await r.json();
        if (!Array.isArray(lijst) || lijst.length === 0) throw new Error('lege lijst');
        return lijst;
      } catch (e) {
        this.log.warn(`Portal niet bereikbaar (poging ${poging}): ${e.message} — opnieuw over 10 s`);
        await wacht(10000);
      }
    }
    return [];
  }

  async onStart(reason) {
    this.log.info(`Start (${reason ?? '-'})`);
    await this.ready;
    const knoppen = await this.haalKnoppen();

    for (const k of knoppen) {
      const lamp  = k.soort === 'lamp';
      const dimmr = k.soort === 'dimmer';
      const kleur = k.soort === 'kleur';
      const type  = kleur ? extendedColorLight : dimmr ? dimmableLight : lamp ? onOffLight : onOffPlugInUnit;
      const model = kleur ? 'Zarlar kleur' : dimmr ? 'Zarlar dimmer' : lamp ? 'Zarlar lamp' : 'Zarlar schakelaar';
      let dev = new MatterbridgeEndpoint(type, { id: k.id })
        .createDefaultIdentifyClusterServer()
        .createDefaultBridgedDeviceBasicInformationClusterServer(
          k.naam, 'ZARLAR-' + k.id, this.matterbridge.aggregatorVendorId,
          'Delannoy', model, 1, VERSIE)
        .createDefaultOnOffClusterServer(!!k.aan);
      if (dimmr) dev = dev.createDefaultLevelControlClusterServer(naarLvl(k.niveau));
      if (kleur) dev = dev.createDefaultLevelControlClusterServer(254)
                          .createDefaultColorControlClusterServer(undefined, undefined, Math.round((k.hue ?? 0) * 254 / 360), Math.round((k.sat ?? 0) * 254 / 100));
      dev = dev.createDefaultPowerSourceWiredClusterServer().addRequiredClusters();

      if (kleur) {
        this.kleuren.add(k.id);
        const hs = () => ({ hue: hueGr(dev.getAttribute(ColorControl, 'currentHue')), sat: satPct(dev.getAttribute(ColorControl, 'currentSaturation')) });
        dev.addCommandHandler('on',     () => this.dim(k.id, { aan: true }));
        dev.addCommandHandler('off',    () => this.dim(k.id, { aan: false }));
        dev.addCommandHandler('toggle', () => this.dim(k.id, { aan: !dev.getAttribute(OnOff, 'onOff') }));
        dev.addCommandHandler('moveToLevel',            () => {});   // helderheid: niet ondersteund
        dev.addCommandHandler('moveToLevelWithOnOff',   ({ request: { level } }) => this.dim(k.id, { aan: level > 0 }));
        dev.addCommandHandler('moveToHueAndSaturation', ({ request: { hue, saturation } }) => this.dim(k.id, { hue: hueGr(hue), sat: satPct(saturation) }));
        dev.addCommandHandler('moveToHue',              ({ request: { hue } }) => this.dim(k.id, { ...hs(), hue: hueGr(hue) }));
        dev.addCommandHandler('moveToSaturation',       ({ request: { saturation } }) => this.dim(k.id, { ...hs(), sat: satPct(saturation) }));
        dev.addCommandHandler('moveToColor',            ({ request: { colorX, colorY } }) => this.dim(k.id, xyNaarHs(colorX, colorY)));
        dev.addCommandHandler('moveToColorTemperature', () => this.dim(k.id, { hue: 0, sat: 0 }));   // wit
      } else if (dimmr) {
        this.dimmers.add(k.id);
        const lvl = () => naarPct(dev.getAttribute(LevelControl, 'currentLevel'));
        dev.addCommandHandler('on',     () => this.dim(k.id, { aan: true,  niveau: lvl() }));
        dev.addCommandHandler('off',    () => this.dim(k.id, { aan: false }));
        dev.addCommandHandler('toggle', () => this.dim(k.id, { aan: !dev.getAttribute(OnOff, 'onOff'), niveau: lvl() }));
        dev.addCommandHandler('moveToLevel',          ({ request: { level } }) => this.dim(k.id, { niveau: naarPct(level) }));
        dev.addCommandHandler('moveToLevelWithOnOff', ({ request: { level } }) => this.dim(k.id, level > 0 ? { aan: true, niveau: naarPct(level) } : { aan: false }));
      } else {
        dev.addCommandHandler('on',     () => this.klik(k.id, true));
        dev.addCommandHandler('off',    () => this.klik(k.id, false));
        dev.addCommandHandler('toggle', () => this.klik(k.id, !dev.getAttribute(OnOff, 'onOff')));
      }

      await this.registerDevice(dev);
      this.toestellen.set(k.id, dev);
    }
    this.log.info(`${this.toestellen.size} knoppen geregistreerd`);
  }

  async onConfigure() {
    await super.onConfigure();
    // Toestand van de portal overnemen (na herstart van de Pi staat alles "uit")
    try {
      const r = await fetch(this.portal + '/api/knoppen', { signal: AbortSignal.timeout(5000) });
      if (r.ok) for (const s of await r.json()) await this.zet(s);
    } catch (e) {
      this.log.warn(`Toestand niet opgehaald: ${e.message}`);
    }
  }

  // Niet wachten in de command handler: Apple Home krijgt meteen antwoord,
  // het Particle-commando loopt op de achtergrond.
  klik(id, aan) {
    this.stuur(id, aan).catch(e => this.log.error(`${id}: ${e.message}`));
  }

  // Dimmer: commando's 400 ms verzamelen (Apple stuurt vaak on + moveToLevel kort na elkaar)
  dim(id, wens) {
    const dev = this.toestellen.get(id);
    const w = this.wachtrij.get(id) || { aan: dev ? !!dev.getAttribute(OnOff, 'onOff') : true };
    if (wens.aan !== undefined) w.aan = wens.aan;
    for (const veld of ['niveau', 'hue', 'sat']) {
      if (wens[veld] !== undefined) { w[veld] = wens[veld]; if (wens.aan === undefined) w.aan = true; }
    }
    clearTimeout(w.timer);
    w.timer = setTimeout(() => {
      this.wachtrij.delete(id);
      const extra = {};
      for (const veld of ['niveau', 'hue', 'sat']) if (w[veld] !== undefined) extra[veld] = w[veld];
      this.stuur(id, w.aan, extra).catch(e => this.log.error(`${id}: ${e.message}`));
    }, 400);
    this.wachtrij.set(id, w);
  }

  async stuur(id, aan, extra = {}) {
    let d = null;
    try {
      const r = await fetch(`${this.portal}/api/photon/${encodeURIComponent(id)}`, {
        method:  'POST',
        headers: { 'Content-Type': 'application/json' },
        body:    JSON.stringify({ aan, ...extra }),
        signal:  AbortSignal.timeout(12000),
      });
      d = await r.json().catch(() => null);
      const info = aan && Object.keys(extra).length ? ' ' + JSON.stringify(extra) : '';
      if (r.ok) this.log.info(`${id} → ${aan ? 'aan' : 'uit'}${info}${d && d.verstuurd === false ? ' (niet verstuurd)' : ''}`);
      else      this.log.warn(`${id} → ${aan ? 'aan' : 'uit'} mislukt: ${d?.error ?? 'HTTP ' + r.status} ${d?.detail ?? ''}`);
    } catch (e) {
      this.log.warn(`${id} → portal niet bereikbaar: ${e.message}`);
    }

    await wacht(300);   // Matter eerst zijn eigen statuswijziging laten afronden
    if (d && Array.isArray(d.knoppen)) {
      for (const s of d.knoppen) await this.zet(s);
    } else {
      await this.zet({ id, aan: !aan });   // geen bruikbaar antwoord → terugspringen
    }
  }

  // Toestand { id, aan, niveau?, hue?, sat? } overnemen in Matter (alleen wat verschilt)
  async zet(s) {
    const dev = this.toestellen.get(s.id);
    if (!dev) return;
    if (dev.getAttribute(OnOff, 'onOff') !== s.aan) {
      await dev.setAttribute(OnOff, 'onOff', s.aan, this.log);
    }
    if (this.dimmers.has(s.id) && typeof s.niveau === 'number') {
      const lvl = naarLvl(s.niveau);
      if (dev.getAttribute(LevelControl, 'currentLevel') !== lvl) await dev.setAttribute(LevelControl, 'currentLevel', lvl, this.log);
    }
    if (this.kleuren.has(s.id) && typeof s.hue === 'number' && typeof s.sat === 'number') {
      const h = Math.round(s.hue * 254 / 360), sa = Math.round(s.sat * 254 / 100);
      if (dev.getAttribute(ColorControl, 'colorMode') !== ColorControl.ColorMode.CurrentHueAndCurrentSaturation)
        await dev.setAttribute(ColorControl, 'colorMode', ColorControl.ColorMode.CurrentHueAndCurrentSaturation, this.log);
      if (dev.getAttribute(ColorControl, 'currentHue') !== h)         await dev.setAttribute(ColorControl, 'currentHue', h, this.log);
      if (dev.getAttribute(ColorControl, 'currentSaturation') !== sa) await dev.setAttribute(ColorControl, 'currentSaturation', sa, this.log);
    }
  }

  async onShutdown(reason) {
    this.gestopt = true;
    for (const w of this.wachtrij.values()) clearTimeout(w.timer);
    await super.onShutdown(reason);
    this.log.info(`Stop (${reason ?? '-'})`);
    if (this.config.unregisterOnShutdown) await this.unregisterAllDevices();
  }
}
