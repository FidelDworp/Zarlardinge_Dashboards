#!/bin/bash
# ─────────────────────────────────────────────────────
# deploy.sh — Zarlar bestanden deployen vanuit Downloads
# Gebruik: bash ~/deploy.sh "omschrijving van wijziging"
#
# v2 (16 okt 2026): stopt bij elke fout (Rosetta, git commit/pull/push, SSH).
#   Downloads wordt ALLEEN opgeruimd als alles gelukt is.
#   Aanleiding: op 16/10 faalde git (xcrun/Rosetta), het script liep door
#   en verwijderde server.js uit Downloads.
# ─────────────────────────────────────────────────────

REPO="$HOME/Zarlardinge_Dashboards/ESP32_Zarlar/zarlar-rpi"
DOWNLOADS="$HOME/Downloads"
BERICHT="${1:-update}"

# Zarlar bestanden die we herkennen
PUBLIC_EXT=("html" "css" "js")
ROOT_FILES=("server.js" "update.sh" "README.md")
ROOT_EXT=("md" "sh")

# Stoppen met duidelijke melding — Downloads blijft onaangeroerd
stop() {
  echo "✗ $1"
  echo "  Downloads NIET opgeruimd — je bestanden staan er nog."
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  exit 1
}

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " Zarlar deploy — $(date '+%d/%m/%Y %H:%M')"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 0. Terminal mag niet via Rosetta draaien (git faalt dan met xcrun-fout)
[ "$(arch)" = "i386" ] && stop "Terminal draait via Rosetta (arch = i386). Finder → Hulpprogramma's → Terminal → Cmd-I → 'Open met Rosetta' uitvinken, Terminal herstarten."

cd "$REPO" || stop "Repo niet gevonden: $REPO"
git status >/dev/null 2>&1 || stop "git werkt niet op deze Mac (zie foutmelding hierboven)." 

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

# 3. Git commit
echo "→ git add + commit..."
git add . || stop "git add mislukt."
if git diff --cached --quiet; then
  echo "  (geen wijzigingen t.o.v. vorige commit — commit overgeslagen)"
else
  git commit -m "$BERICHT" || stop "git commit mislukt."
fi

# 4. Git pull --rebase (voorkomt conflict als GitHub verder staat)
echo "→ git pull (rebase)..."
git pull --rebase || stop "git pull --rebase mislukt (conflict?). Los op met 'git status' in $REPO, of vraag hulp."

# 5. Git push
echo "→ git push..."
git push || stop "git push mislukt."

# 6. RPi bijwerken via Tailscale (werkt overal)
echo "→ RPi updaten..."
ssh -o ConnectTimeout=15 fidel@100.123.74.113 'bash /home/fidel/update.sh' \
  || stop "RPi niet bereikbaar of update.sh faalde. GitHub is wel bijgewerkt: later opnieuw deployen volstaat."

# 7. Downloads opruimen (alleen bereikt als alles gelukt is)
echo "→ Downloads opruimen..."
for bestand in "${GEKOPIEERD[@]}"; do
  rm "$bestand"
  echo "✓ Verwijderd: $(basename "$bestand")"
done

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " Klaar!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
