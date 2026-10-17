// ─────────────────────────────────────────────────────────────────────────────
// matterbridge-zarlar v0.5 — Zarlar Photon-knoppen en rooms in Apple Home
// Filip Delannoy — Portal Plan §16.9 (F4a)
//
// Apple Home ──► Matterbridge (deze plugin) ──► server.js (RPi) ──► Particle ──► Photon
//
// v0.5 (17 okt 2026): elke room = één samengesteld toestel (keuze B), zoals de ESP32 zelf:
//   één accessoire "Eetplaats" met de functies als onderdelen. In Apple Home kies je dan zelf
//   "Toon als afzonderlijke tegels" aan/uit. Nieuwe id per room (bv. "room80"); de losse
//   tegels van v0.4 (room80-thermostaat, …) verdwijnen. Bereikbaar/onbereikbaar op het toestel.
//
// v0.4 (17 okt 2026): rooms (Portal Plan §16.11, server.js ≥ v2.9)
//   - Rooms uit GET /api/rooms-apple (vaste lijst in server.js). Per room dezelfde tegels als
//     de room nu zelf via Matter aanbiedt: thermostaat (verwarmen = Thuis, uit = Weg),
//     vochtigheid, beweging MOV1 (+ MOV2 als ingeschakeld), kleur, SW1/SW2/SW3.
//   - Elke 10 s /api/poll/roomNN → tegels bijwerken. Room offline → tegels "geen reactie".
//   - Klikken → POST /api/set/roomNN (sw1/sw2/sw3, setpoint, home, r/g/b), daarna meteen opnieuw pollen.
//   - Wijzigingen van Apple Home herkend aan context.fabric; eigen updates worden niet teruggestuurd.
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
import { MatterbridgeDynamicPlatform, MatterbridgeEndpoint, onOffLight, onOffPlugInUnit, dimmableLight, extendedColorLight,
         thermostat, humiditySensor, occupancySensor, bridgedNode, powerSource } from 'matterbridge';
import { OnOff, LevelControl, ColorControl, Thermostat, RelativeHumidityMeasurement, OccupancySensing,
         BridgedDeviceBasicInformation } from 'matterbridge/matter/clusters';

const VERSIE  = '0.5.0';
const POLL_MS = 10000;   // R5: rooms elke 10 s opvragen
const PORTAL  = 'http://192.168.0.50:3000';
const wacht   = ms => new Promise(r => setTimeout(r, ms));
const naarPct = lvl => Math.max(1, Math.min(100, Math.round((lvl ?? 254) * 100 / 254)));   // Matter 1-254 → %
const naarLvl = pct => Math.max(1, Math.min(254, Math.round((pct ?? 100) * 254 / 100)));   // % → Matter 1-254
const hueGr   = h => Math.round((h ?? 0) * 360 / 254);    // Matter tint 0-254 → graden
const satPct  = s => Math.round((s ?? 0) * 100 / 254);    // Matter verzadiging 0-254 → %

// RGB (0-255) ↔ HSV (tint 0-360, verzadiging 0-100, waarde 0-100)
function rgbNaarHsv(r, g, b) {
  const max = Math.max(r, g, b), min = Math.min(r, g, b), d = max - min;
  let h = 0;
  if (d) h = max === r ? ((g - b) / d) % 6 : max === g ? (b - r) / d + 2 : (r - g) / d + 4;
  h = Math.round(h * 60); if (h < 0) h += 360;
  return { hue: h, sat: max ? Math.round(d / max * 100) : 0, val: Math.round(max / 255 * 100) };
}
function hsvNaarRgb(h, s, v) {
  s /= 100; v /= 100;
  const c = v * s, x = c * (1 - Math.abs((h / 60) % 2 - 1)), m = v - c;
  const [r, g, b] = h < 60 ? [c, x, 0] : h < 120 ? [x, c, 0] : h < 180 ? [0, c, x] : h < 240 ? [0, x, c] : h < 300 ? [x, 0, c] : [c, 0, x];
  return { r: Math.round((r + m) * 255), g: Math.round((g + m) * 255), b: Math.round((b + m) * 255) };
}
const kort = t => String(t).slice(0, 32);   // Matter-naam max 32 tekens

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
    this.rooms      = [];          // [{ id, naam, mov2, n (aantal pixels), tegels: {soort → endpoint} }]
    this.pollTimer  = null;
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

    await this.maakRooms();
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
    // Rooms: meteen een eerste keer, daarna elke POLL_MS
    await this.pollRooms();
    this.pollTimer = setInterval(() => this.pollRooms().catch(e => this.log.warn(`Poll rooms: ${e.message}`)), POLL_MS);
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

  // ═══ ROOMS (v0.4) ═══════════════════════════════════════════════════════════

  // Lijst ophalen bij server.js; blijft proberen (nooit registreren zonder lijst)
  async haalRooms() {
    for (let poging = 1; !this.gestopt; poging++) {
      try {
        const r = await fetch(this.portal + '/api/rooms-apple', { signal: AbortSignal.timeout(15000) });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return await r.json();
      } catch (e) {
        this.log.warn(`Rooms niet opgehaald (poging ${poging}): ${e.message} — opnieuw over 10 s`);
        await wacht(10000);
      }
    }
    return [];
  }

  async maakRooms() {
    const lijst = await this.haalRooms();
    for (const room of lijst) {
      const R = room.id, n = (room.pixels || []).length, t = {};
      const naamPixel = i => kort(room.pixels[i] || `${room.naam} pixel ${i}`);

      // Het toestel zelf: één accessoire per room, met de functies als onderdelen
      const toestel = new MatterbridgeEndpoint([bridgedNode, powerSource], { id: R })
        .createDefaultBridgedDeviceBasicInformationClusterServer(kort(room.naam), 'ZARLAR-' + R, this.matterbridge.aggregatorVendorId, 'Delannoy', 'Zarlar room', 1, VERSIE)
        .createDefaultPowerSourceWiredClusterServer()
        .addRequiredClusterServers();

      // 1. Thermostaat: temperatuur + setpoint; verwarmen = Thuis, uit = Weg
      t.thermo = toestel.addChildDeviceType('Thermostaat', thermostat)
        .createDefaultIdentifyClusterServer()
        .createDefaultHeatingThermostatClusterServer(20, 20, 10, 30)
        .addRequiredClusterServers();
      t.thermo.subscribeAttribute(Thermostat, 'occupiedHeatingSetpoint', (nw, oud, ctx) => {
        if (ctx?.fabric === undefined) return;                      // eigen update
        this.roomSet(R, { setpoint: nw / 100 });
      });
      t.thermo.subscribeAttribute(Thermostat, 'systemMode', (nw, oud, ctx) => {
        if (ctx?.fabric === undefined) return;
        this.roomSet(R, { home: nw === Thermostat.SystemMode.Off ? 0 : 1 });
      });

      // 2. Vochtigheid
      t.vocht = toestel.addChildDeviceType('Vochtigheid', humiditySensor)
        .createDefaultIdentifyClusterServer()
        .createDefaultRelativeHumidityMeasurementClusterServer(null)
        .addRequiredClusterServers();

      // 3-4. Beweging MOV1 (+ MOV2)
      t.mov1 = toestel.addChildDeviceType('Beweging 1', occupancySensor)
        .createDefaultIdentifyClusterServer()
        .createDefaultOccupancySensingClusterServer(false)
        .addRequiredClusterServers();
      if (room.mov2) {
        t.mov2 = toestel.addChildDeviceType('Beweging 2', occupancySensor)
          .createDefaultIdentifyClusterServer()
          .createDefaultOccupancySensingClusterServer(false)
          .addRequiredClusterServers();
      }

      // 5. Kleurpicker voor alle pixels (altijd aan, zoals op de room)
      t.kleur = toestel.addChildDeviceType('Kleur', extendedColorLight)
        .createDefaultIdentifyClusterServer()
        .createDefaultOnOffClusterServer(true)
        .createDefaultLevelControlClusterServer(254)
        .createDefaultColorControlClusterServer()
        .addRequiredClusterServers();
      const k = t.kleur;
      const hs = () => ({ hue: hueGr(k.getAttribute(ColorControl, 'currentHue')), sat: satPct(k.getAttribute(ColorControl, 'currentSaturation')) });
      k.addCommandHandler('on',     () => {});
      k.addCommandHandler('off',    () => this.roomAltijdAan(R));
      k.addCommandHandler('toggle', () => this.roomAltijdAan(R));
      k.addCommandHandler('moveToLevel',            ({ request: { level } }) => this.roomKleur(R, { val: naarPct(level) }));
      k.addCommandHandler('moveToLevelWithOnOff',   ({ request: { level } }) => level > 0 ? this.roomKleur(R, { val: naarPct(level) }) : this.roomAltijdAan(R));
      k.addCommandHandler('moveToHueAndSaturation', ({ request: { hue, saturation } }) => this.roomKleur(R, { hue: hueGr(hue), sat: satPct(saturation) }));
      k.addCommandHandler('moveToHue',              ({ request: { hue } }) => this.roomKleur(R, { ...hs(), hue: hueGr(hue) }));
      k.addCommandHandler('moveToSaturation',       ({ request: { saturation } }) => this.roomKleur(R, { ...hs(), sat: satPct(saturation) }));
      k.addCommandHandler('moveToColor',            ({ request: { colorX, colorY } }) => this.roomKleur(R, xyNaarHs(colorX, colorY)));
      k.addCommandHandler('moveToColorTemperature', () => this.roomKleur(R, { hue: 0, sat: 0 }));

      // 6-8. SW1 (pixel 0), SW2 (pixel 1), SW3 (pixels 2+) — onderdeelnaam = pixel-nickname
      const sw = [['sw1', 1, naamPixel(0)], ['sw2', 2, naamPixel(1)], ['sw3', 3, kort(`${room.naam} lampen`)]];
      for (const [key, nodig, naam] of sw) {
        if (n < nodig) continue;
        const d = toestel.addChildDeviceType(naam, onOffLight)
          .createDefaultIdentifyClusterServer()
          .createDefaultOnOffClusterServer(false)
          .addRequiredClusterServers();
        d.addCommandHandler('on',     () => this.roomSet(R, { [key]: 1 }));
        d.addCommandHandler('off',    () => this.roomSet(R, { [key]: 0 }));
        d.addCommandHandler('toggle', () => this.roomSet(R, { [key]: d.getAttribute(OnOff, 'onOff') ? 0 : 1 }));
        t[key] = d;
      }

      await this.registerDevice(toestel);
      this.rooms.push({ id: R, naam: room.naam, mov2: !!room.mov2, n, toestel, tegels: t, kleurBusyTot: 0 });
      this.log.info(`Room ${R} (${room.naam}): 1 toestel met ${Object.keys(t).length} onderdelen geregistreerd${room.online ? '' : ' — room nu offline'}`);
    }
  }

  // Commando naar de room (niet afwachten in Matter), daarna meteen opnieuw pollen
  roomSet(R, body) {
    (async () => {
      try {
        const r = await fetch(`${this.portal}/api/set/${R}`, {
          method: 'POST', headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(body), signal: AbortSignal.timeout(10000),
        });
        if (r.ok) this.log.info(`${R} ← ${JSON.stringify(body)}`);
        else      this.log.warn(`${R} ← ${JSON.stringify(body)} mislukt: HTTP ${r.status}`);
      } catch (e) {
        this.log.warn(`${R} ← ${JSON.stringify(body)}: portal niet bereikbaar (${e.message})`);
      }
      await wacht(800);
      await this.pollRooms(R);
    })().catch(e => this.log.error(`${R}: ${e.message}`));
  }

  // Kleurpicker van de room: altijd aan houden
  roomAltijdAan(R) {
    const room = this.rooms.find(x => x.id === R);
    setTimeout(() => room?.tegels.kleur.setAttribute(OnOff, 'onOff', true, this.log).catch(() => {}), 300);
  }

  // Kleur: 400 ms verzamelen, dan één r/g/b naar de room
  roomKleur(R, wens) {
    const room = this.rooms.find(x => x.id === R);
    if (!room) return;
    const k = room.tegels.kleur;
    const sleutel = 'kleur:' + R;
    const w = this.wachtrij.get(sleutel) || {
      hue: hueGr(k.getAttribute(ColorControl, 'currentHue')),
      sat: satPct(k.getAttribute(ColorControl, 'currentSaturation')),
      val: naarPct(k.getAttribute(LevelControl, 'currentLevel')),
    };
    Object.assign(w, wens);
    room.kleurBusyTot = Date.now() + 5000;      // poll overschrijft de kleur even niet
    clearTimeout(w.timer);
    w.timer = setTimeout(() => {
      this.wachtrij.delete(sleutel);
      this.roomSet(R, hsvNaarRgb(w.hue, w.sat, w.val));
    }, 400);
    this.wachtrij.set(sleutel, w);
  }

  // Alle rooms (of één) opvragen en de tegels bijwerken
  async pollRooms(alleen) {
    for (const room of this.rooms) {
      if (alleen && room.id !== alleen) continue;
      const t = room.tegels;
      let j = null;
      try {
        const r = await fetch(`${this.portal}/api/poll/${room.id}`, { signal: AbortSignal.timeout(5000) });
        j = r.ok ? await r.json() : null;
      } catch { j = null; }

      const online = !!(j && j.online !== false);
      if (room.online !== online) {
        room.online = online;
        await room.toestel.setAttribute(BridgedDeviceBasicInformation, 'reachable', online, this.log).catch(() => {});
        this.log.info(`Room ${room.id} ${online ? 'online' : 'OFFLINE — tegels "geen reactie"'}`);
      }
      if (!online) continue;

      const zet = async (dev, cl, attr, v) => {
        if (dev && v !== undefined && v !== null && dev.getAttribute(cl, attr) !== v) await dev.setAttribute(cl, attr, v, this.log);
      };
      const ds  = Number(j.f), dht = Number(j.e);
      const temp = ds > 0 ? ds : dht > 0 ? dht : null;
      if (temp !== null) await zet(t.thermo, Thermostat, 'localTemperature', Math.round(temp * 100));
      if (Number(j.c) > 0) await zet(t.thermo, Thermostat, 'occupiedHeatingSetpoint', Math.round(Number(j.c) * 100));
      await zet(t.thermo, Thermostat, 'systemMode', Number(j.v) ? Thermostat.SystemMode.Heat : Thermostat.SystemMode.Off);
      const rs = t.thermo.getAttribute(Thermostat, 'thermostatRunningState');
      if (rs && !!rs.heat !== !!Number(j.b)) await t.thermo.setAttribute(Thermostat, 'thermostatRunningState', { ...rs, heat: !!Number(j.b) }, this.log);

      if (Number(j.h) > 0) await zet(t.vocht, RelativeHumidityMeasurement, 'measuredValue', Math.round(Number(j.h) * 100));

      for (const [key, veld] of [['mov1', 'y'], ['mov2', 'z']]) {
        const dev = t[key]; if (!dev) continue;
        const bezet = !!Number(j[veld]);
        const nu = dev.getAttribute(OccupancySensing, 'occupancy');
        if (!nu || !!nu.occupied !== bezet) await dev.setAttribute(OccupancySensing, 'occupancy', { occupied: bezet }, this.log);
      }

      const px = String(j.t || '').replace('P=', '');
      await zet(t.sw1, OnOff, 'onOff', px[0] === '1');
      await zet(t.sw2, OnOff, 'onOff', px[1] === '1');
      await zet(t.sw3, OnOff, 'onOff', px.slice(2).includes('1'));

      if (Date.now() > room.kleurBusyTot && j.q !== undefined) {
        const { hue, sat, val } = rgbNaarHsv(Number(j.q), Number(j.r), Number(j.s));
        await zet(t.kleur, ColorControl, 'colorMode', ColorControl.ColorMode.CurrentHueAndCurrentSaturation);
        await zet(t.kleur, ColorControl, 'currentHue', Math.round(hue * 254 / 360));
        await zet(t.kleur, ColorControl, 'currentSaturation', Math.round(sat * 254 / 100));
        if (val > 0) await zet(t.kleur, LevelControl, 'currentLevel', naarLvl(val));
        await zet(t.kleur, OnOff, 'onOff', true);
      }
    }
  }

  async onShutdown(reason) {
    this.gestopt = true;
    clearInterval(this.pollTimer);
    for (const w of this.wachtrij.values()) clearTimeout(w.timer);
    await super.onShutdown(reason);
    this.log.info(`Stop (${reason ?? '-'})`);
    if (this.config.unregisterOnShutdown) await this.unregisterAllDevices();
  }
}
