#!/bin/bash

# Stop HID Device now
systemctl stop hid-gadget.service

# disable service
systemctl disable hid-gadget.service

# remove service file
rm /etc/systemd/system/hid-gadget.service

# reload systemd to pick remove sevice
systemctl daemon-reload

# remove HID Start/Stop scripts from /usr/local/bin
ubin=/usr/local/bin
rm ${ubin}/hid-gadget-stop.sh 
rm ${ubin}/hid-gadget-start.sh