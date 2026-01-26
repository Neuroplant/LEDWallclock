# LEDWallclock
very low cost Wallclock based on ESP01 and WS2812B Leds 

  # Features:
  - get current time from NTP (instead of buying a RTC-Modul)
  - use free pixels as Color light (MQTT /Clock/colorRGB/set)
  - use Alarm-time (MQTT /Clock/alarm/set)
  - Animation-Alarm (MQTT /Clock/effect/set)

  # MQTT
  you will need an  MQTT Broker (I tested with Mosquitto)
  Topics are:
  - ("/Uhr/colorRGB/set")
  - ("/Uhr/effect/set")
  - ("/Uhr/timer/set")
  - ("/Uhr/alarm/set")
  - ("/Uhr/TimerLimit/set")
  - ("/Uhr/Rotation/set")
  - ("/Uhr/Csecond/set")
  - ("/Uhr/Cminute/set")
  - ("/Uhr/Chour/set")
  - ("/Uhr/Csegment5/set")
  - ("/Uhr/Csegment15/set")
  - ("/Uhr/Ctimer/set")
  - ("/Uhr/NextAnimation/set")
  - ("/Uhr/AniTime/set")

# Parts:
 in addition to the WS2812B strip:
   - ESP8266-01 (ESP-01)
   - ESP8266 ESP-01/01S RGB-LED-Controller-Adapter. 
   - 5V Powersupply (or something like this https://www.amazon.de/dp/B0F9Y5LQJB?ref=ppx_yo2ov_dt_b_fed_asin_title)
   
  *when I created this Project it was nessesary to build your own PCB but now you can buy one assembled with, I will keep everything here in case someone wants to solder it himself
    - PCB (60mm * 40mm)
  - DC-DC Converter (set to 5V output) (e.g., for 60 LEDs this will be sufficient; for more LEDs, use a bigger one)
    https://www.amazon.de/AZDelivery-LM2596S-Netzteil-Adapter-Arduino/dp/B07DP3JX2X/
  - Level Shifter (e.g., https://www.amazon.de/XCSOURCE®-Logisches-Konverter-Bi-Direktional-TE291/dp/B0148BLZGE/)
  - Resistor 470 Ohm
  - Capacitor 1000µF / 6.3V
  - DC Power Supply
  *
  # Libraries
  
  - FS / SPIFFS
  - WiFiManager
  - WiFi / WiFiUDP
  - TimeLib (Time)
  - ArduinoJson (v6.x)
  - NeoPixelBus
  - NeoPixelAnimator
  - PubSubClient

|Bibliothek|SPIFFS (File System)|
| ------------- |:-------------:|
 | Benötigt für: | Speichern von Konfigurationsdateien und Daten auf dem ESP8266. |
 | Installieren: |Für ESP8266: FS ist standardmäßig enthalten, SPIFFS ebenfalls.|
| |Für ESP32: SPIFFS über Bibliotheksmanager installieren.|

| Bibliothek| WiFiManager|
| ------------- |:-------------:|
|Autor: |tzapu|
|Benötigt für: |WLAN-Konfiguration über Captive Portal (falls kein festes WLAN in Code).|
|Arduino IDE Installation:|
|Menü: |Sketch → Bibliothek einbinden → Bibliotheken verwalten → Suche: WiFiManager → Installieren|
|GitHub: |https://github.com/tzapu/WiFiManager|

|Bibliothek: |WiFi (inkl. WiFiUDP)|
| ------------- |:-------------:|
|Benötigt für: |WLAN-Verbindung und NTP (UDP) Kommunikation.|
|Hinweis: |Für ESP8266 ist standardmäßig enthalten. 
||Für ESP32: WiFi über Bibliotheksmanager.|

|Bibliothek: |Time (TimeLib)|
| ------------- |:-------------:|
|Autor: |Michael Margolis|
|Benötigt für: |Zeitfunktionen wie now(), hour(), minute(), etc.|
|Arduino IDE Installation: |Bibliotheksmanager → Suche: TimeLib → Installieren|
|GitHub: https://github.com/PaulStoffregen/Time|

|Bibliothek: |ArduinoJson|
| ------------- |:-------------:|
|Autor: |Benoit Blanchon|
|Benötigt für: |JSON-Parsing für MQTT Konfigurationen.|
|Hinweis: |Version 6.x empfohlen|
|Arduino IDE Installation: |Bibliotheksmanager → Suche: ArduinoJson → Installieren
|GitHub: |https://github.com/bblanchon/ArduinoJson|

|Bibliothek: |NeoPixelBus|
| ------------- |:-------------:|
|Autor: |Makuna|
|Benötigt für: |Steuerung der WS2812B LEDs|
|Arduino IDE Installation: |Bibliotheksmanager → Suche: NeoPixelBus → Installieren|
|GitHub: |https://github.com/Makuna/NeoPixelBus|

|Bibliothek: |NeoPixelAnimator|
| ------------- |:-------------:|
|Benötigt für: |Animationen / Fade / Blending der LEDs|
|Hinweis: |Wird in der Regel automatisch mit NeoPixelBus mitinstalliert, sonst separat installieren|
|Arduino IDE Installation: |Bibliotheksmanager → Suche: NeoPixelAnimator → Installieren|

|Bibliothek: |PubSubClient|
| ------------- |:-------------:|
|Autor: |Nick O'Leary|
|Benötigt für: |MQTT-Kommunikation|
|Arduino IDE Installation: |Bibliotheksmanager → Suche: PubSubClient → Installieren|
|GitHub: |https://github.com/knolleary/pubsubclient|
