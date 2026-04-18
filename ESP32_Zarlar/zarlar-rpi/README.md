# ZARLAR Dashboard — RPi Installatie

## Voorbereiding op je Mac

1. Download alle bestanden uit deze map
2. Maak een map `zarlar-setup` op een USB stick
3. Kopieer alle bestanden daarin
4. Steek de stick in de RPi

## SSH verbinden vanuit Mac Terminal

```bash
ssh fidel@192.168.0.180    # huidig IP — wijzigt na herstart zonder vast IP
```
Wachtwoord: jouw RPi wachtwoord  
Je ziet niets terwijl je typt — dat is normaal!

## Bestanden kopiëren van stick naar RPi

```bash
cp /media/fidel/ZARLAR/zarlar-setup/* /home/fidel/
cd /home/fidel
chmod +x *.sh
```

## Installatiestappen

```bash
./1-check.sh              # controleer huidige toestand
./2-remove-homebridge.sh  # verwijder Homebridge
./3-setup-zarlar.sh       # installeer Zarlar Dashboard
./4-static-ip.sh          # stel vast IP in (192.168.0.50) + herstart
```

Na herstart bereikbaar op: **http://192.168.0.50:3000**

## Handige commando's

```bash
sudo systemctl status zarlar    # status
sudo systemctl restart zarlar   # herstart
sudo journalctl -u zarlar -f    # live log
```

## Mappenstructuur op de RPi

```
/home/fidel/zarlar-dashboard/
├── server.js
├── settings.json        ← automatisch aangemaakt
├── epex-cache.json      ← automatisch aangemaakt
└── public/
    ├── epex-grafiek.html
    └── index.html       ← symlink naar epex-grafiek.html
```

## API endpoints

| Endpoint | Methode | Wat |
|---|---|---|
| `/` | GET | Webpagina |
| `/api/settings` | GET/POST | Instellingen lezen/schrijven |
| `/api/epex` | GET | EPEX data (gecached 26u) |
| `/api/poll/senrg` | GET | Smart Energy controller |
| `/api/poll/eco` | GET | ECO Boiler |
| `/api/poll/hvac` | GET | HVAC controller |
| `/api/status` | GET | Status alle controllers |
| `/api/set/senrg` | POST | Commando naar controller |

## Netwerk

```
192.168.0.50  → RPi Zarlar Dashboard (na vast IP)
192.168.0.60  → ESP32 Zarlar Dashboard (oud)
192.168.0.70  → ESP32 HVAC
192.168.0.71  → ESP32 ECO Boiler
192.168.0.73  → ESP32 Smart Energy
192.168.0.80  → ESP32 Room
```

---
*Zarlar Dashboard v1.0 — April 2026*
