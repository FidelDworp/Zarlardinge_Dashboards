#!/bin/bash
# ─────────────────────────────────────────────────────
# deploy.sh — Zarlar bestanden deployen vanuit Downloads
# Gebruik: bash ~/deploy.sh "omschrijving van wijziging"
# ─────────────────────────────────────────────────────

REPO="$HOME/Zarlardinge_Dashboards/ESP32_Zarlar/zarlar-rpi"
DOWNLOADS="$HOME/Downloads"
BERICHT="${1:-update}"

# Zarlar bestanden die we herkennen
PUBLIC_EXT=("html" "css" "js")
ROOT_FILES=("server.js" "update.sh" "README.md")
ROOT_EXT=("md" "sh")

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " Zarlar deploy — $(date '+%d/%m/%Y %H:%M')"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

cd "$REPO" || { echo "✗ Repo niet gevonden: $REPO"; exit 1; }

# Houdt bij welke bestanden gekopieerd werden
GEKOPIEERD=()
GEVONDEN=false

# 1. Zoek Zarlar bestanden in Downloads
for bestand in "$DOWNLOADS"/*; do
  naam=$(basename "$bestand")
  ext="${naam##*.}"

  # Sla mappen over
  [ -f "$bestand" ] || continue

  # Controleer of het een bekend Zarlar bestand is
  IN_ROOT=false
  IN_PUBLIC=false

  # Root bestanden op naam
  for root in "${ROOT_FILES[@]}"; do
    [ "$naam" = "$root" ] && IN_ROOT=true && break
  done

  # Root bestanden op extensie (.sh, .md)
  for e in "${ROOT_EXT[@]}"; do
    [ "$ext" = "$e" ] && IN_ROOT=true && break
  done

  # Public bestanden op extensie
  for e in "${PUBLIC_EXT[@]}"; do
    [ "$ext" = "$e" ] && IN_PUBLIC=true && break
  done

  # Kopieer naar juiste map
  if $IN_ROOT; then
    cp "$bestand" "$REPO/$naam"
    echo "→ $naam → root/"
    GEKOPIEERD+=("$bestand")
    GEVONDEN=true
  elif $IN_PUBLIC; then
    cp "$bestand" "$REPO/public/$naam"
    echo "→ $naam → public/"
    GEKOPIEERD+=("$bestand")
    GEVONDEN=true
  fi
done

# Niets gevonden?
if ! $GEVONDEN; then
  echo "⚠ Geen Zarlar bestanden gevonden in Downloads"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  exit 0
fi

# 2. Rommel opruimen (bestanden die niet in repo horen)
git rm -f "zarlar-update commands" 2>/dev/null || true

# 3. Git pull --rebase (voorkomt conflict als GitHub verder staat)
echo "→ git pull (rebase)..."
git pull --rebase

# 4. Git commit + push
echo "→ git add + commit..."
git add .
git commit -m "$BERICHT"
echo "→ git push..."
git push

# 3. RPi bijwerken
echo "→ RPi updaten..."
ssh fidel@192.168.0.50 'bash /home/fidel/update.sh'

# 4. Downloads opruimen
echo "→ Downloads opruimen..."
for bestand in "${GEKOPIEERD[@]}"; do
  rm "$bestand"
  echo "✓ Verwijderd: $(basename "$bestand")"
done

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " Klaar!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
