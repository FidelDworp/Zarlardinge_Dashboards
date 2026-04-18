#!/bin/bash
echo "========================================"
echo " Zarlar Dashboard installeren"
echo "========================================"

# Gebruiker en paden automatisch detecteren
GEBRUIKER=$(whoami)
HOME_DIR="/home/$GEBRUIKER"
ZARLAR_DIR="$HOME_DIR/zarlar-dashboard"
PUBLIC_DIR="$ZARLAR_DIR/public"

echo "Gebruiker: $GEBRUIKER"
echo "Installatiemap: $ZARLAR_DIR"

# Bestanden staan naast dit script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "Bronmap: $SCRIPT_DIR"
echo ""

# Maak mappenstructuur
echo "Mappen aanmaken..."
mkdir -p "$PUBLIC_DIR"

# Kopieer bestanden
echo "Bestanden kopiëren..."
cp "$SCRIPT_DIR/server.js"         "$ZARLAR_DIR/" || { echo "FOUT: server.js niet gevonden"; exit 1; }
cp "$SCRIPT_DIR/epex-grafiek.html" "$PUBLIC_DIR/" || { echo "FOUT: epex-grafiek.html niet gevonden"; exit 1; }

# Index symlink zodat / de pagina toont
ln -sf epex-grafiek.html "$PUBLIC_DIR/index.html"
echo "✓ index.html symlink aangemaakt"

# NPM packages installeren
echo "NPM packages installeren..."
cd "$ZARLAR_DIR"
npm init -y --quiet
npm install express node-fetch@2 --quiet

# Service bestand aanmaken (dynamisch — correct pad en gebruiker)
echo "Service bestand aanmaken..."
sudo tee /etc/systemd/system/zarlar.service > /dev/null << SERVICE
[Unit]
Description=Zarlar Dashboard Server
After=network.target

[Service]
ExecStart=/usr/bin/node $ZARLAR_DIR/server.js
WorkingDirectory=$ZARLAR_DIR
Restart=always
RestartSec=5
User=$GEBRUIKER
Environment=NODE_ENV=production

[Install]
WantedBy=multi-user.target
SERVICE

# Service starten
echo "Service installeren en starten..."
sudo systemctl daemon-reload
sudo systemctl enable zarlar
sudo systemctl start zarlar

# Wacht even en check
sleep 3
echo ""
if systemctl is-active --quiet zarlar; then
  IP=$(hostname -I | awk '{print $1}')
  echo "✓ Zarlar draait!"
  echo "  Open in browser: http://$IP:3000"
else
  echo "✗ Probleem — log:"
  sudo journalctl -u zarlar -n 20 --no-pager
fi
echo "========================================"
