#!/bin/bash

set +e

G=/sys/kernel/config/usb_gadget/composite

if [ ! -d "$G" ]; then
    echo "Gadget existiert nicht."
    exit 0
fi

cd "$G"

# Vom UDC trennen
echo "" > UDC 2>/dev/null

sleep 1

# Symlinks entfernen
rm -f configs/c.1/hid.usb0
rm -f configs/c.1/hid.usb1

# Funktionen entfernen
rmdir functions/hid.usb0 2>/dev/null
rmdir functions/hid.usb1 2>/dev/null

# Konfiguration entfernen
rmdir configs/c.1/strings/0x409 2>/dev/null
rmdir configs/c.1 2>/dev/null

# Strings entfernen
rmdir strings/0x409 2>/dev/null

cd ..

# Gadget-Verzeichnis entfernen
rmdir composite 2>/dev/null

echo "Composite HID Gadget gestoppt."
