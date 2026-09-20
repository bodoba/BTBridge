#!/bin/bash

ubin=/usr/local/bin

# Copy HID Start/Stop scripts to /usr/local/bin
mkdir -p ${ubin}
install -o root -g root -m 0750 -t ${ubin} HID-Setup/hid-gadget-stop.sh HID-Setup/hid-gadget-start.sh

# Make systemd aware of new service
install -o root -g root -m 0640 -t /etc/systemd/system HID-Setup/hid-gadget.service

# reload systemd to pick up new sevice
systemctl daemon-reload

# Activate service at boot
systemctl enable hid-gadget.service

# Start HID Device now
systemctl start hid-gadget.service
