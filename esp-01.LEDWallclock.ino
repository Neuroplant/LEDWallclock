/*
  LED Wall Clock
  ESP-01 WS2812B with 60 LED (or more) on pin 2
  include NeoPixelBus libary
  
  Features:
  - get time from NTP (instead of buying a RTC-Modul)
  - use free pixels as Color light (MQTT /Clock/colorRGB/set)
  - Timer (seconds max 24h) (MQTT /Clock/timer/set)
  - Animation-Alarm (MQTT /Clock/effect/set)

  Parts:
  - PCB (60mm*40mm)
  - DC DC Converter (set to 5V output) (e.g. for 60 LEDs this will be okay, for more LEDs take something bigger:
  https://www.amazon.de/AZDelivery-LM2596S-Netzteil-Adapter-Arduino/dp/B07DP3JX2X/ref=sr_1_4?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=37NAW85CZCOXT&keywords=dcdc+wandler&qid=1574410661&sprefix=dc+dc%2Caps%2C169&sr=8-4)
  - Level Shifter (e.g. https://www.amazon.de/XCSOURCE®-Logisches-Konverter-Bi-Direktional-TE291/dp/B0148BLZGE/ref=sr_1_6?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=3PXRUEETCBDR9&keywords=level+shifter&qid=1574411353&sprefix=level+%2Caps%2C182&sr=8-6)
  - Resistor 470Ohm
  - Capacitor 1000µF/6.3V
  - DC Powersupply
  -	ESP8266-01
*/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <NeoPixelBus.h>
#include <stdlib.h>
#include <NeoPixelAnimator.h>
const uint16_t PixelCount = 60; // best effect with 60 LEDs, but other numbers will work too
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266
///Animation
const uint16_t AnimCount = PixelCount / 5 * 2 + 1; // we only need enough animations for the tail and one extra
const uint16_t PixelFadeDuration = 300; // third of a second
// one second divide by the number of pixels = loop once a second
const uint16_t NextPixelMoveDuration = 1000 / PixelCount; // how fast we move through the pixels
NeoGamma<NeoGammaTableMethod> colorGamma; // for any fade animations, best to correct gamma
const uint16_t AniTime = 10; //how long will the animation be shown (seconds)
///Amimation

#define colorSaturation 128
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1Ws2812xMethod > strip(PixelCount, PixelPin);
// Colors for hands
RgbColor Csecond (map(255, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation));
RgbColor Cminute (map(0, 0, 255, 0, colorSaturation), map(255, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation));
RgbColor Chour (map(0, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation), map(255, 0, 255, 0, colorSaturation));
RgbColor Csegment5 (map(10, 0, 255, 0, colorSaturation), map(10, 0, 255, 0, colorSaturation), map(10, 0, 255, 0, colorSaturation));
RgbColor Csegment15 (map(200, 0, 255, 0, colorSaturation), map(200, 0, 255, 0, colorSaturation), map(200, 0, 255, 0, colorSaturation));
RgbColor Ctimer (map(255, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation));

RgbColor black(0);

//input RGB from mqtt
int inRed = 50;		//0..255
int inGreen = 50;	//0..255
int inBlue = 50;		//0..255

int pos_second_hand;
int pos_minute_hand;
int pos_hour_hand;
int timer = 0;

#ifndef STASSID
#define STASSID "WIFI_NAME"// please change
#define STAPSK  "WIFI_PWD"// please change
#endif

const char * ssid = STASSID; // your network SSID (name)
const char * pass = STAPSK;  // your network password

const char* MQTTServer = "MQTT_SERVER";
int status = WL_IDLE_STATUS;  // the Wifi radio's status

///NTP
IPAddress timeServer(134, 130, 4, 17); // time-a.timefreq.bldrdoc.gov
const int timeZone = 2;     // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)
// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
//NTP
WiFiClient net;
PubSubClient  client(net);

void connect() {
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  while (!client.connect("arduinoClient")) {
    delay(1000);
  }
  client.subscribe("/Clock/colorRGB/set");
  client.subscribe("/Clock/effect/set");
  client.subscribe("/Clock/timer/set");
}
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = 0;
  String Payload = String((char*)payload);

  // change Color
  if (strcmp(topic, "/Clock/colorRGB/set") == 0) {
    inRed = Payload.substring(0, Payload.indexOf(',')).toInt();
    inGreen = Payload.substring(Payload.indexOf(',') + 1, Payload.lastIndexOf(',')).toInt();
    inBlue = Payload.substring(Payload.lastIndexOf(',') + 1).toInt();
  }
  //Start Animation
  if (strcmp(topic, "/Clock/effect/set") == 0) {
    int i = Payload.substring(0, length).toInt();
    Anima(i);
  }
  //Start Timer
  if (strcmp(topic, "/Clock/timer/set") == 0) {
    int i = Payload.substring(0, length).toInt();
    if (i > 10) Timer(i, Ctimer);
  }
}
struct MyAnimationState
{
  RgbColor StartingColor;
  RgbColor EndingColor;
  uint16_t IndexPixel; // which pixel this animation is effecting
};
NeoPixelAnimator animations(AnimCount); // NeoPixel animation management object
MyAnimationState animationState[AnimCount];
uint16_t frontPixel = 0;  // the front of the loop
RgbColor frontColor;  // the color at the front of the loop
void SetRandomSeed()
{
  uint32_t seed;
  // random works best with a seed that can use 31 bits
  // analogRead on a unconnected pin tends toward less than four bits
  seed = analogRead(0);
  delay(1);
  for (int shifts = 3; shifts < 31; shifts += 3)
  {
    seed ^= analogRead(0) << shifts;
    delay(1);
  }
  randomSeed(seed);
}
void FadeOutAnimUpdate(const AnimationParam& param)
{
  // this gets called for each animation on every time step
  // progress will start at 0.0 and end at 1.0
  // we use the blend function on the RgbColor to mix
  // color based on the progress given to us in the animation
  RgbColor updatedColor = RgbColor::LinearBlend(
                            animationState[param.index].StartingColor,
                            animationState[param.index].EndingColor,
                            param.progress);
  // apply the color to the strip
  strip.SetPixelColor(animationState[param.index].IndexPixel,
                      colorGamma.Correct(updatedColor));
}
void LoopAnimUpdate(const AnimationParam& param)
{
  // wait for this animation to complete,
  // we are using it as a timer of sorts
  if (param.state == AnimationState_Completed)
  {
    // done, time to restart this position tracking animation/timer
    animations.RestartAnimation(param.index);
    // pick the next pixel inline to start animating
    //
    frontPixel = (frontPixel + 1) % PixelCount; // increment and wrap
    if (frontPixel == 0)
    {
      // we looped, lets pick a new front color
      frontColor = HslColor(random(360) / 360.0f, 1.0f, 0.25f);
    }
    uint16_t indexAnim;
    // do we have an animation available to use to animate the next front pixel?
    // if you see skipping, then either you are going to fast or need to increase
    // the number of animation channels
    if (animations.NextAvailableAnimation(&indexAnim, 1))
    {
      animationState[indexAnim].StartingColor = frontColor;
      animationState[indexAnim].EndingColor = RgbColor(0, 0, 0);
      animationState[indexAnim].IndexPixel = frontPixel;
      animations.StartAnimation(indexAnim, PixelFadeDuration, FadeOutAnimUpdate);
    }
  }
}


void Anima (int value) {
  switch (value) {
    case 0:
      break;
    case 1:
	    strip.ClearTo(black);
		animations.StartAnimation(0, NextPixelMoveDuration, LoopAnimUpdate);
      int i = now();
      while (i + AniTime  > now()) {
		  animations.UpdateAnimations();
		  strip.Show();
		  };
      client.publish("/Clock/effect", "0", true);
      client.publish("/Clock/effect/set", "0", true);
      break;
  }
}
void setup()
{
  strip.Begin();
  strip.Show();
  SetRandomSeed();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);
  client.setServer(MQTTServer, 1883);
  client.setCallback(callback);
  connect();
}
time_t prevDisplay = 0; // when the digital clock was displayed
void loop()
{
  client.loop();
  if (!client.connected()) {
    connect();
  }
  //digitalClockDisplay();
  if (now() != prevDisplay) { //update the display only if time has changed
    prevDisplay = now();
    digitalClockDisplay();
  }
}

void Clockface(int Red, int Green, int Blue) {
  RgbColor Cfill(Red, Green, Blue);
  //delete hands and write all leds in selected color
  strip.ClearTo(Cfill);
  //publish to mqtt as feedback (is this needed or wrong?)
  char restate[12];
  String i = String  (String(Red) + "," + String(Green) + "," + String(Blue));
  i.toCharArray(restate, 12);
  client.publish("/Clock/colorRGB", restate);
  //show 5-minute segments
  if (Red + Green + Blue > 0) {
    for (int i = 0; i < 12; i++) {
      strip.SetPixelColor(i * (PixelCount / 60) * 5, Csegment5);
    }
    //show 15-minute segments
    for (int i = 0; i < 4; i++) {
      strip.SetPixelColor(i * (PixelCount / 60) * 15, Csegment15);
    }
  }
}

void digitalClockDisplay() {
  Clockface(inRed, inGreen, inBlue);
  //show hour hand
  int hours = hour();
  if (hours >= 12) hours -= 12;
  pos_hour_hand = map(((hours * 5) + (minute() / 12)), 0, 59, 0, PixelCount - 1);
  strip.SetPixelColor(pos_hour_hand, Chour);
  //show minute hand
  pos_minute_hand = map(minute(), 0, 59, 0, PixelCount - 1);
  strip.SetPixelColor(pos_minute_hand, Cminute);
  //show second hand
  pos_second_hand = map(second(), 0, 59, 0, PixelCount - 1);
  strip.SetPixelColor(pos_second_hand, Csecond);
  strip.Show();
}
/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
time_t getNtpTime() {
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0; // return 0 if unable to get the time
}
// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress & address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
/*-------- NTP code ----------*/


void Timer(int TimeLeft, RgbColor Tcolor) {
  int Endtime = now() + TimeLeft;
  //Timer loop
  while (Endtime >= now()) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
    }
    TimeLeft = Endtime - now();//update left time to end

    char restate[12];
 	itoa(TimeLeft,restate,10);
    client.publish("/Clock/timer", restate);


    if (TimeLeft >= 3600) {
      // >1 hour
      for (int i = 0; i < map(TimeLeft, 0, 24 * 3600, 0, PixelCount); i ++) {
        strip.SetPixelColor(PixelCount - i, Tcolor);
      }
    }
    if ((TimeLeft > 60) && (TimeLeft < 3600)) {
      // >1 Minute
      for (int i = 0; i <= map(TimeLeft, 0, 3600, 0, PixelCount); i++) {
        strip.SetPixelColor(i, Tcolor);
      }
    }
    if (TimeLeft <= 60) {
      //< 1 Minute
      for (int i = 0; i <= map(TimeLeft, 0, 60, 0, PixelCount); i++) {
        strip.SetPixelColor(i, Tcolor);
      }
    }
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
    }
     delay(200);
	strip.Show();
  }
  //timer reached
  client.publish("/Clock/effect/set", "1", true); //Alarm
  client.publish("/Clock/timer/set", "0", true);
  client.publish("/Clock/timer", "0");
  connect();


  return;
}
