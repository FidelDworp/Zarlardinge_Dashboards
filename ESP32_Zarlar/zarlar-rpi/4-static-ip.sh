#!/bin/bash
echo "========================================"
echo " Vast IP instellen voor Zarlar RPi"
echo "========================================"

VASTE_IP="192.168.0.50"
GATEWAY="192.168.0.1"
DNS="192.168.0.1"

# Detecteer actieve interface
if ip link show eth0 2>/dev/null | grep -q "state UP"; then
  INTERFACE="eth0"
elif ip link show wlan0 2>/dev/null | grep -q "state UP"; then
  INTERFACE="wlan0"
else
  echo "Actieve interface niet gevonden. Beschikbare interfaces:"
  ip link show | grep -E "^[0-9]+:" | awk '{print $2}' | tr -d ':'
  read -p "Voer interface in (bijv. eth0 of wlan0): " INTERFACE
fi

echo "Interface: $INTERFACE"
echo "Huidig IP: $(hostname -I | awk '{print $1}')"
echo "Nieuw IP:  $VASTE_IP"
echo ""

# Check of al ingesteld
if grep -q "$VASTE_IP" /etc/dhcpcd.conf 2>/dev/null; then
  echo "Vast IP $VASTE_IP al ingesteld — niets te doen."
  exit 0
fi

# Schrijf naar dhcpcd.conf
sudo tee -a /etc/dhcpcd.conf > /dev/null << CONF

# Zarlar Dashboard — vast IP (ingesteld $(date))
interface $INTERFACE
static ip_address=$VASTE_IP/24
static routers=$GATEWAY
static domain_name_servers=$DNS
CONF

echo "✓ Vast IP ingesteld: $VASTE_IP"
echo "  Gateway:   $GATEWAY"
echo "  Interface: $INTERFACE"
echo ""
echo "Na herstart bereikbaar op: http://$VASTE_IP:3000"
echo ""
read -p "Nu herstarten? [j/N]: " antw
if [[ "$antw" == "j" || "$antw" == "J" ]]; then
  echo "Herstarten..."
  sudo reboot
else
  echo "Herstart later met: sudo reboot"
fi
echo "========================================"
