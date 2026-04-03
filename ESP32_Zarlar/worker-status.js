// worker-status.js — v2.0 met /sensor endpoint voor ESP32 Dashboard matrix
// https://dash.cloudflare.com/6410f9752a55e2ae9964a1e9462c7304/home/overview
// Deployment url: https://controllers-diagnose.filip-delannoy.workers.dev/
const PARTICLE_TOKEN = "ba9d9e1ed9f70cc5db24de2db21764a3a3afe28b";

const DEVICES = [
    { id: "30002c000547343233323032", name: "R1-BandB" },
    { id: "5600420005504b464d323520", name: "R2-BADK" },
    { id: "2c0026000747343232363230", name: "R3-INKOM" },
    { id: "310017001647373335333438", name: "R4-KEUK" },
    { id: "33004f000e504b464d323520", name: "R5-WASPL" },
    { id: "3c0030000a47353138383138", name: "R6-EETPL" },
    { id: "200033000547373336323230", name: "R7-ZITPL" },
    { id: "30002d000747353138383138", name: "R8-ACCESS" },
    { id: "3e003f001447343338333633", name: "S-OUTSIDE" },
    { id: "310049000f47343432313031", name: "S-ECO-boiler" },
    { id: "290044000147353138383138", name: "S-HVAC" }
];

// -----------------------------------------------------------
// Device info ophalen (status/last_seen/firmware/rssi)
// -----------------------------------------------------------
async function getDeviceInfo(deviceId) {
    const url = `https://api.particle.io/v1/devices/${deviceId}?access_token=${PARTICLE_TOKEN}`;
    try {
        const res = await fetch(url);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        const status = data.connected ? 'online' : 'offline';
        const last_seen = data.last_heard ?? null;
        let last_seen_minutes = null;
        if (last_seen) {
            last_seen_minutes = (Date.now() - new Date(last_seen)) / 60000;
        }
        return {
            id: deviceId,
            name: data.name || deviceId,
            status,
            last_seen,
            last_seen_minutes,
            firmware: data.variables?.firmware ?? '--',
            rssi: data.variables?.rssi ?? '--'
        };
    } catch (err) {
        return {
            id: deviceId,
            name: deviceId,
            status: "offline",
            last_seen: "--",
            last_seen_minutes: null,
            firmware: "--",
            rssi: "--"
        };
    }
}

// -----------------------------------------------------------
// Sensor data ophalen via Particle JSON_status variabele
// Geeft de ruwe JSON terug zoals de Photon die publiceert,
// aangevuld met "online" veld (0/1) en "last_seen_minutes".
// -----------------------------------------------------------
async function getSensorData(deviceId) {
    const url = `https://api.particle.io/v1/devices/${deviceId}/JSON_status?access_token=${PARTICLE_TOKEN}`;
    try {
        const res = await fetch(url);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();

        // Particle geeft het resultaat in data.result (string of object)
        let obj = data.result;
        if (typeof obj === 'string') {
            try { obj = JSON.parse(obj); } catch(e) { obj = null; }
        }
        if (!obj || typeof obj !== 'object') {
            return { error: "no_data", online: 0 };
        }

        // Voeg online-vlag en tijdstip toe
        obj.online = 1;
        obj.last_min = Math.round(
            (Date.now() - new Date(data.coreInfo?.last_heard ?? Date.now())) / 60000
        );
        return obj;

    } catch (err) {
        return { error: err.message, online: 0 };
    }
}

// -----------------------------------------------------------
// CORS headers
// -----------------------------------------------------------
function corsHeaders() {
    return {
        "Access-Control-Allow-Origin": "*",
        "Access-Control-Allow-Methods": "GET, OPTIONS",
        "Access-Control-Allow-Headers": "Content-Type",
        "Content-Type": "application/json"
    };
}

// -----------------------------------------------------------
// Worker entrypoint
// -----------------------------------------------------------
export default {
    async fetch(request, env, ctx) {
        const url = new URL(request.url);

        if (request.method === "OPTIONS") {
            return new Response(null, { headers: corsHeaders() });
        }

        // === 1. Diagnose endpoint (root) — ongewijzigd ===
        if (url.pathname === "/" || url.pathname === "") {
            try {
                const results = await Promise.all(
                    DEVICES.map(d => getDeviceInfo(d.id))
                );
                return new Response(JSON.stringify(results), {
                    headers: corsHeaders()
                });
            } catch (err) {
                return new Response(
                    JSON.stringify({ error: err.message }),
                    { status: 500, headers: corsHeaders() }
                );
            }
        }

        // === 2. Token endpoint — ongewijzigd ===
        if (url.pathname === "/token") {
            return new Response(
                JSON.stringify({ token: PARTICLE_TOKEN }),
                { headers: corsHeaders() }
            );
        }

        // === 3. NIEUW: sensor endpoint voor ESP32 matrix ===
        // GET /sensor?id=5600420005504b464d323520
        // Geeft Photon JSON_status terug als platte JSON
        if (url.pathname === "/sensor") {
            const deviceId = url.searchParams.get("id");
            if (!deviceId) {
                return new Response(
                    JSON.stringify({ error: "id parameter verplicht" }),
                    { status: 400, headers: corsHeaders() }
                );
            }
            const data = await getSensorData(deviceId);
            return new Response(JSON.stringify(data), {
                headers: corsHeaders()
            });
        }

        // Alles anders → 404
        return new Response(
            JSON.stringify({ error: "Not found" }),
            { status: 404, headers: corsHeaders() }
        );
    }
};
