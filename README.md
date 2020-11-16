# heat_can
CAN bus interface firmware for my Logamatic heating.

This is the firmware of an ESP8266 board with a MCP2515 CAN controller.
It's purpose is to access the CAN bus of a Logamatic heating controller remotely via MQTT.
It's just a wireless CAN interface, no magic here ;)

It contains the [Arduino CAN library](https://github.com/sandeepmistry/arduino-CAN) as a subtree, because it required some patches for my ESP8266 setup.
