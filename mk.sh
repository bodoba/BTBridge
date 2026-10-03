#!/bin/sh
rsync -avr . --exclude .git --exclude .DS_Store BTBridge:/home/pi/AddOn/BTBridge
ssh BTBridge -x "( cd /home/pi/AddOn/BTBridge/bt2usbd && make )"