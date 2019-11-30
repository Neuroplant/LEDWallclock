# LEDWallclock
very low cost Wallclock based on ESP01 and WS2812B Leds 

  include NeoPixelBus libary
  
  # Features:
  - get current time from NTP (instead of buying a RTC-Modul)
  - use free pixels as Color light (MQTT /Clock/colorRGB/set)
  - use Alarm-time (MQTT /Clock/alarm/set)
  - Animation-Alarm (MQTT /Clock/effect/set)

  # Parts:
  - PCB (60mm*40mm)
  - DC DC Converter (set to 5V output) (e.g. for 60 LEDs this will be okay, for more LEDs take something bigger:
  https://www.amazon.de/AZDelivery-LM2596S-Netzteil-Adapter-Arduino/dp/B07DP3JX2X/ref=sr_1_4?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=37NAW85CZCOXT&keywords=dcdc+wandler&qid=1574410661&sprefix=dc+dc%2Caps%2C169&sr=8-4)
  - Level Shifter (e.g. https://www.amazon.de/XCSOURCE®-Logisches-Konverter-Bi-Direktional-TE291/dp/B0148BLZGE/ref=sr_1_6?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=3PXRUEETCBDR9&keywords=level+shifter&qid=1574411353&sprefix=level+%2Caps%2C182&sr=8-6)
  - Resistor 470Ohm
  - Capacitor 1000µF/6.3V
  - DC Powersupply
  -	ESP8266-01
