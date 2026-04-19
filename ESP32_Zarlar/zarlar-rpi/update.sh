#!/bin/bash
# ─────────────────────────────────────────────────────
# Zarlar update.sh — synchroniseert RPi met GitHub
# Gebruik: ssh fidel@192.168.0.50 'bash ~/update.sh'
# ─────────────────────────────────────────────────────

REPO="/home/fidel/repo"
ZARLAR="/home/fidel/zarlar-dashboard"
SUBMAP="ESP32_Zarlar/zarlar-rpi"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " Zarlar update — $(date '+%d/%m/%Y %H:%M')"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 1. Git pull (rebase voorkomt merge-conflicten)
cd "$REPO"
echo "→ git pull..."
git fetch origin
git rebase origin/main

# 2. Was server.js gewijzigd?
CHANGED=$(git diff HEAD@{1} HEAD --name-only 2>/dev/null | grep "$SUBMAP/server.js")

# 3. Public bestanden kopiëren
echo "→ public/ synchroniseren..."
rsync -av "$REPO/$SUBMAP/public/" "$ZARLAR/public/"

# 4. server.js kopiëren indien gewijzigd
if [ -n "$CHANGED" ]; then
  echo "→ server.js gewijzigd — kopiëren en herstarten..."
  cp "$REPO/$SUBMAP/server.js" "$ZARLAR/server.js"
  sudo systemctl restart zarlar
  echo "✓ Zarlar herstart"
else
  echo "→ server.js ongewijzigd — geen herstart nodig"
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " Klaar!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
