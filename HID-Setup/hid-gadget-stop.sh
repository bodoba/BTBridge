#!/bin/bash

# Disable immediate script termination on errors.
# This is intentional because cleanup operations may fail
# if some resources have already been removed.
set +e

# Path to the USB composite gadget configuration.
G=/sys/kernel/config/usb_gadget/composite

# Verify that the gadget exists.
# If the directory is missing, there is nothing to clean up.
if [ ! -d "$G" ]; then
    echo "USB gadget does not exist."
    exit 0
fi

# Enter the gadget configuration directory.
cd "$G"

###############################################################################
# DETACH THE GADGET FROM THE USB DEVICE CONTROLLER
###############################################################################

# Unbind the gadget from the USB Device Controller (UDC).
#
# This immediately disconnects the virtual USB device from the
# host computer (PC, Mac, KVM, USB switch, etc.).
#
# The host operating system will see the keyboard and mouse
# disappear as if they were physically unplugged.
echo "" > UDC 2>/dev/null

# Give the kernel a moment to properly detach the device.
sleep 1

###############################################################################
# REMOVE CONFIGURATION LINKS
###############################################################################

# Remove symbolic links connecting the HID functions
# to configuration c.1.
#
# USB gadget functions must be detached from the configuration
# before the functions themselves can be removed.
rm -f configs/c.1/hid.usb0
rm -f configs/c.1/hid.usb1
rm -f configs/c.1/hid.usb2

###############################################################################
# REMOVE HID FUNCTIONS
###############################################################################

# Remove the virtual keyboard function.
#
# This deletes the HID keyboard definition from ConfigFS.
rmdir functions/hid.usb0 2>/dev/null

# Remove the virtual mouse function.
#
# This deletes the HID mouse definition from ConfigFS.
rmdir functions/hid.usb1 2>/dev/null

# Remove the virtual consumer control function.
#
# This deletes the HID consumer control definition from ConfigFS.
rmdir functions/hid.usb2 2>/dev/null

###############################################################################
# REMOVE USB CONFIGURATION
###############################################################################

# Remove the configuration's language string directory.
rmdir configs/c.1/strings/0x409 2>/dev/null

# Remove configuration c.1 itself.
rmdir configs/c.1 2>/dev/null

###############################################################################
# REMOVE DEVICE STRING DESCRIPTORS
###############################################################################

# Remove manufacturer, product, and serial-number strings.
rmdir strings/0x409 2>/dev/null

###############################################################################
# REMOVE GADGET DIRECTORY
###############################################################################

# Move one level up in ConfigFS.
cd ..

# Remove the gadget root directory.
#
# At this point all functions, configurations and strings
# should already have been removed.
rmdir composite 2>/dev/null

# Final status message.
echo "Composite HID Gadget stopped successfully."