#!/bin/bash
echo "========================================"
echo " Homebridge verwijderen"
echo "========================================"
echo "Stoppen..."
sudo systemctl stop homebridge 2>/dev/null
sudo systemctl stop homebridge-config-ui-x 2>/dev/null
echo "Uitschakelen..."
sudo systemctl disable homebridge 2>/dev/null
sudo systemctl disable homebridge-config-ui-x 2>/dev/null
echo "NPM packages verwijderen..."
sudo npm uninstall -g homebridge 2>/dev/null
sudo npm uninstall -g homebridge-config-ui-x 2>/dev/null
sudo npm uninstall -g @homebridge/hap-nodejs 2>/dev/null
echo "Service bestanden verwijderen..."
sudo rm -f /etc/systemd/system/homebridge.service 2>/dev/null
sudo rm -f /etc/systemd/system/homebridge-config-ui-x.service 2>/dev/null
echo "Config map verwijderen (optioneel)..."
read -p "Homebridge config ook verwijderen? (/var/lib/homebridge) [j/N]: " antw
if [[ "$antw" == "j" || "$antw" == "J" ]]; then
  sudo rm -rf /var/lib/homebridge
  echo "Config verwijderd."
else
  echo "Config bewaard."
fi
sudo systemctl daemon-reload
echo ""
echo "Homebridge verwijderd!"
echo "========================================"
