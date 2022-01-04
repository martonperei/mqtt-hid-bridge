# HID-BLE bridge

A modular application to translate USB events from a Logitech Unifying receiver through MQTT into Bluetooth commands.
Tested on a Raspberry Pi Zero W 2 and Apple TV.

* usb-hid-mqtt Reads USB events from a Logitech Unifying receiver and forwards them to MQTT
* mqtt-ble-hid Reads bluetooth commands from MQTT and forwards them to the paired bluetooth device

## Requirements

* sudo apt install libpaho-mqtt-dev liblua5.3-dev libusb-1.0-0-dev libudev-dev
* sudo systemctl disable hciuart --now
* sudo systemctl disable bthelper --now
* sudo systemctl disable bluetooth --now

## Building

* git clone --recursive git@github.com:martonperei/mqtt-hid-bridge.git
* cd mqtt-hid-bridge && sudo make install

## Usage

* /usr/local/bin/usb-hid-mqtt -a tcp://mqtt.local.lan -t usb-hid -u user -p pass
* /usr/local/bin/mqtt-ble-hid -a tcp://mqtt.local.lan -t ble-hid -u user -p pass
* There are also service files included for both executables, edit them to include your MQTT config
* Pair your Android / Apple TV to the Raspberry Pi
* See the included Node Red flow on how to parse the USB events from MQTT and translate those to Bluetooth events
