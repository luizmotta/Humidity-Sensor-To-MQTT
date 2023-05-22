# This is a Wemos D1 ESP WROOM 2 sketch that reads a Capacitive Soil Moisture v1.2 and uploads realtime to a MQTT server
 
1) Connect sensor to 3V3 / GND / A0
2) Copy config.h.sample to config.h
3) Edit your newly created config.h to add your wifi, MQTT, input pin and sleep time info (in minutes)
4) Upload to your board and wait for the information to be published every sleep_time

Obs: The board goes to deep sleep between runs.

