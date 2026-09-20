#!/bin/bash

# Abort immediately if any command exits with a non-zero status.
# This prevents partially configured USB gadgets.
set -e

# Load the Linux USB Composite Gadget framework.
# This kernel module provides the ConfigFS infrastructure
# required to create virtual USB devices.
modprobe libcomposite

# Base path of the USB gadget configuration.
# Everything related to the virtual USB device is created
# inside this ConfigFS directory.
G=/sys/kernel/config/usb_gadget/composite

# Exit if the gadget already exists.
# Prevents duplicate initialization when the script is
# accidentally executed more than once.
if [ -d "$G" ]; then
    exit 0
fi

# Create the gadget directory and enter it.
mkdir -p "$G"
cd "$G"

###############################################################################
# USB DEVICE IDENTIFICATION
###############################################################################

# USB Vendor ID.
# 0x1d6b is assigned to the Linux Foundation and is commonly
# used for Linux USB gadget devices.
echo 0x1d6b > idVendor

# USB Product ID.
# Used by the host operating system to identify the device.
echo 0x0104 > idProduct

# USB specification version (USB 2.0).
echo 0x0200 > bcdUSB

# Device firmware version.
echo 0x0100 > bcdDevice

###############################################################################
# DEVICE STRINGS
###############################################################################

# Create language-specific string directory.
# 0x409 = English (United States).
mkdir -p strings/0x409

# Device serial number.
# Should ideally be unique per device.
echo "00000001" > strings/0x409/serialnumber

# Manufacturer name shown on the host computer.
echo "BB-ZONE" > strings/0x409/manufacturer

# Product name displayed by Windows, Linux or macOS.
echo "Bluetooth K/M Bridge" > strings/0x409/product

###############################################################################
# USB CONFIGURATION
###############################################################################

# Create configuration #1.
# A USB device may expose multiple configurations.
mkdir -p configs/c.1

# Configuration description strings.
mkdir -p configs/c.1/strings/0x409

# Description shown to the host system.
echo "Composite HID" > configs/c.1/strings/0x409/configuration

# Maximum power consumption in mA advertised to the host.
echo 250 > configs/c.1/MaxPower

###############################################################################
# HID FUNCTION #1 : KEYBOARD
###############################################################################

# Create a HID keyboard function.
mkdir -p functions/hid.usb0

# Protocol = 1 -> Keyboard
echo 1 > functions/hid.usb0/protocol

# Boot subclass.
# Required for BIOS/UEFI compatibility.
echo 1 > functions/hid.usb0/subclass

# Length of HID reports sent to the host.
# Standard keyboard reports are 8 bytes.
echo 8 > functions/hid.usb0/report_length

# HID Report Descriptor for a standard 101/104-key keyboard.
#
# Defines:
# - Modifier keys (Ctrl, Shift, Alt, GUI)
# - LED indicators (Caps Lock, Num Lock, etc.)
# - Up to six simultaneously pressed keys
#
# This descriptor is presented to the host system during
# USB enumeration and tells the OS how to interpret reports.
printf '\x05\x01\x09\x06\xa1\x01\x05\x07\x19\xe0\x29\xe7\x15\x00\x25\x01\x75\x01\x95\x08\x81\x02\x95\x01\x75\x08\x81\x01\x95\x05\x75\x01\x05\x08\x19\x01\x29\x05\x91\x02\x95\x01\x75\x03\x91\x01\x95\x06\x75\x08\x15\x00\x25\x65\x05\x07\x19\x00\x29\x65\x81\x00\xc0' \
> functions/hid.usb0/report_desc

###############################################################################
# HID FUNCTION #2 : MOUSE
###############################################################################

# Create a HID mouse function.
mkdir -p functions/hid.usb1

# Protocol = 2 -> Mouse
echo 2 > functions/hid.usb1/protocol

# Boot subclass.
# Allows operation in BIOS environments.
echo 1 > functions/hid.usb1/subclass

# Mouse reports are 4 bytes:
# Byte 0 = buttons
# Byte 1 = X movement
# Byte 2 = Y movement
# Byte 3 = wheel

echo 4 > functions/hid.usb1/report_length

# HID Report Descriptor for a standard USB mouse.
#
# Supports:
# - Left, middle and right button
# - Relative X/Y movement
# - Wheel

printf '\x05\x01\x09\x02\xa1\x01\x09\x01\xa1\x00\x05\x09\x19\x01\x29\x05\x15\x00\x25\x01\x95\x05\x75\x01\x81\x02\x95\x01\x75\x03\x81\x01\x05\x01\x09\x30\x09\x31\x09\x38\x15\x81\x25\x7f\x75\x08\x95\x03\x81\x06\xc0\xc0' > functions/hid.usb1/report_desc

###############################################################################
# HID FUNCTION #3 : CONSUMER CONTROL
###############################################################################

mkdir -p functions/hid.usb2

# Generic HID Device
echo 0 > functions/hid.usb2/protocol
echo 0 > functions/hid.usb2/subclass

# 2-byte report
echo 2 > functions/hid.usb2/report_length

# # HID Report Descriptor for consumer control
printf '\x05\x0C\x09\x01\xA1\x01\x15\x00\x26\xFF\x03\x19\x00\x2A\xFF\x03\x75\x10\x95\x01\x81\x00\xC0' \
> functions/hid.usb2/report_desc


###############################################################################
# ENABLE FUNCTIONS
###############################################################################

# Attach the keyboard function to configuration c.1.
ln -s functions/hid.usb0 configs/c.1/

# Attach the mouse function to configuration c.1.
ln -s functions/hid.usb1 configs/c.1/

# Attach the consumer control function to configuration c.1.
ln -s functions/hid.usb2 configs/c.1/

###############################################################################
# ACTIVATE THE GADGET
###############################################################################

# Give the kernel a moment to finish all ConfigFS operations.
sleep 2

# Bind the gadget to the USB Device Controller (UDC).
#
# On the Orange Pi PC Plus the OTG controller is exposed as
# "musb-hdrc.4.auto".
#
# Once this command succeeds, the host computer will detect
# a new USB composite device containing:
#
# - USB HID Keyboard
# - USB HID Mouse
# - USB HID Consumer Control
#
echo musb-hdrc.4.auto > UDC
 
echo "Composite HID Gadget started successfully."