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

// Wifi Settings
const char * 	ssid 		= "Wifi-SSD"; 		// your network SSID (name)
const char * 	pass 		= "Wifi-PWD";  		// your network password
// MQTT Settings
const char* 	MQTTServer	= "MQTT-Server"; 	// your MQTT-Servers name
// Hardware Settings
const uint16_t 	PixelCount 	= 60; // best effect with n*60 LEDs, but other numbers will work too
const uint8_t 	PixelPin 	= 2;  // make sure to set this to the correct pin, ignored for Esp8266
const uint16_t 	TimerLimit 	= 600;//timer shows only if left time is < TimerLimit
///NTP Settings
IPAddress timeServer(134, 95, 192, 172); // Uni Köln
const int timeZone = 1;     // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)
// Color Settings (leave to default)
#define colorSaturation 128
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1Ws2812xMethod > strip(PixelCount, PixelPin);
RgbColor Csecond (map(255, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation));
RgbColor Cminute (map(0, 0, 255, 0, colorSaturation), map(255, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation));
RgbColor Chour (map(0, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation), map(255, 0, 255, 0, colorSaturation));
RgbColor Csegment5 (map(10, 0, 255, 0, colorSaturation), map(10, 0, 255, 0, colorSaturation), map(10, 0, 255, 0, colorSaturation));
RgbColor Csegment15 (map(200, 0, 255, 0, colorSaturation), map(200, 0, 255, 0, colorSaturation), map(200, 0, 255, 0, colorSaturation));
RgbColor Ctimer (map(100, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation));
RgbColor black(0);

///Animation Settings (leave to default)
const uint16_t AnimCount = PixelCount / 5 * 2 + 1; // we only need enough animations for the tail and one extra
const uint16_t PixelFadeDuration = 300; // third of a second
// one second divide by the number of pixels = loop once a second
const uint16_t NextPixelMoveDuration = 1000 / PixelCount; // how fast we move through the pixels
NeoGamma<NeoGammaTableMethod> colorGamma; // for any fade animations, best to correct gamma
const uint16_t AniTime = 10; //how long will the animation be shown (seconds)
///


//input RGB from mqtt
int inRed 	= 	50;		//0..255
int inGreen = 	50;		//0..255
int inBlue 	= 	50;		//0..255

//Hand positions
time_t 	EndTime 	=	0;
int 	RestTime	=	0;

int status = WL_IDLE_STATUS;  // the Wifi radio's status


// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
//NTP
WiFiClient net;
PubSubClient  client(net);
void AnimationSelect (int value);
void connect() {
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  while (!client.connect("arduinoClient")) {
    delay(1000);
  }
  client.subscribe("/Clock/colorRGB/set");
  client.subscribe("/Clock/effect/set");
  // client.subscribe("/Clock/timer/set");
  client.subscribe("/Clock/alarm/set");
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
    AnimationSelect(i);
  }

  //Start Timer
  if (strcmp(topic, "/Clock/timer/set") == 0) {
    int i = Payload.substring(0, length).toInt();
  }

  //Set Alarm
  if (strcmp(topic, "/Clock/alarm/set") == 0) {
    tmElements_t EndTime_e;
    // * Example for Payload -> "2019-11-26T19:30:00"
    if (1 == 2) { //(payload[10] != 'T') {
      EndTime = now(); //timer canceled
    } else {
      EndTime_e.Year   =  CalendarYrToTm(Payload.substring(0, Payload.indexOf('-')).toInt());
      EndTime_e.Month  =  Payload.substring(Payload.indexOf('-') + 1, Payload.lastIndexOf('-')).toInt();
      EndTime_e.Day  =  Payload.substring(Payload.lastIndexOf('-') + 1, Payload.indexOf('T')).toInt();
      EndTime_e.Hour  = Payload.substring(Payload.indexOf('T') + 1, Payload.indexOf(':')).toInt();
      EndTime_e.Minute  = Payload.substring(Payload.indexOf(':') + 1, Payload.lastIndexOf(':')).toInt();
      EndTime_e.Second  = Payload.substring(Payload.lastIndexOf(':') + 1).toInt();

      EndTime = makeTime(EndTime_e);
    }
  }
}

//Animation
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
///
NeoPixelAnimator animations02(PixelCount); // NeoPixel animation management object
MyAnimationState animationState02[PixelCount];
///
const uint8_t AnimationChannels = 1; // we only need one as all the pixels are animated at once
NeoPixelAnimator animations03(AnimationChannels); // NeoPixel animation management object
MyAnimationState animationState03[AnimationChannels];
boolean fadeToColor = true;  // general purpose variable used to store effect state


void SetRandomSeed(void)
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
void FadeOutAnimUpdate(const AnimationParam & param)
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
void LoopAnimUpdate(const AnimationParam & param)
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
void BlendAnimUpdate02(const AnimationParam & param)
{
  // this gets called for each animation on every time step
  // progress will start at 0.0 and end at 1.0
  // we use the blend function on the RgbColor to mix
  // color based on the progress given to us in the animation
  RgbColor updatedColor = RgbColor::LinearBlend(
                            animationState02[param.index].StartingColor,
                            animationState02[param.index].EndingColor,
                            param.progress);
  // apply the color to the strip
  strip.SetPixelColor(param.index, updatedColor);
}
void BlendAnimUpdate03(const AnimationParam & param)
{
  // this gets called for each animation on every time step
  // progress will start at 0.0 and end at 1.0
  // we use the blend function on the RgbColor to mix
  // color based on the progress given to us in the animation
  RgbColor updatedColor = RgbColor::LinearBlend(
                            animationState03[param.index].StartingColor,
                            animationState03[param.index].EndingColor,
                            param.progress);

  // apply the color to the strip
  for (uint16_t pixel = 0; pixel < PixelCount; pixel++)
  {
    strip.SetPixelColor(pixel, updatedColor);
  }
}
void PickRandom(float luminance)
{
  // pick random count of pixels to animate
  uint16_t count = random(PixelCount);
  while (count > 0)
  {
    // pick a random pixel
    uint16_t pixel = random(PixelCount);

    // pick random time and random color
    // we use HslColor object as it allows us to easily pick a color
    // with the same saturation and luminance
    uint16_t time = random(100, 400);
    animationState02[pixel].StartingColor = strip.GetPixelColor(pixel);
    animationState02[pixel].EndingColor = HslColor(random(360) / 360.0f, 1.0f, luminance);

    animations02.StartAnimation(pixel, time, BlendAnimUpdate02);

    count--;
  }
}
void FadeInFadeOutRinseRepeat(float luminance)
{
  if (fadeToColor)
  {
    // Fade upto a random color
    // we use HslColor object as it allows us to easily pick a hue
    // with the same saturation and luminance so the colors picked
    // will have similiar overall brightness
    RgbColor target = HslColor(random(360) / 360.0f, 1.0f, luminance);
    uint16_t time = random(800, 2000);

    animationState03[0].StartingColor = strip.GetPixelColor(0);
    animationState03[0].EndingColor = target;

    animations03.StartAnimation(0, time, BlendAnimUpdate03);
  }
  else
  {
    // fade to black
    uint16_t time = random(600, 700);

    animationState03[0].StartingColor = strip.GetPixelColor(0);
    animationState03[0].EndingColor = RgbColor(0);

    animations03.StartAnimation(0, time, BlendAnimUpdate03);
  }

  // toggle to the next effect state
  fadeToColor = !fadeToColor;
}

void AnimationSelect (int value) {
  switch (value) {
    case 0 :
      break;
    case 1 : {
        client.publish("/Clock/effect", "1");
        strip.ClearTo(black);

        animations.StartAnimation(0, NextPixelMoveDuration, LoopAnimUpdate);
        int i = now();
        while (i + AniTime  > now()) {
          animations.UpdateAnimations();
          strip.Show();
        }
        break;
      }
//I have no idea, what I do wrong with switch..case any help welcome
      /*  case 2 : {
            client.publish("/Clock/effect", "2");
            strip.ClearTo(black);

            PickRandom(0.2f); // 0.0 = black, 0.25 is normal, 0.5 is bright
            int i = now();
            while (i + AniTime  > now()) {
              animations02.UpdateAnimations();
              strip.Show();
            }
            break;
          }
        case 3 : {
            client.publish("/Clock/effect", "3");
            strip.ClearTo(black);

            FadeInFadeOutRinseRepeat(0.2f); // 0.0 = black, 0.25 is normal, 0.5 is bright
            int i = now();
            while (i + AniTime  > now()) {
              animations03.UpdateAnimations();
              strip.Show();
            }
            break;
          }*/
  }
  client.publish("/Clock/effect", "0", true);
  client.publish("/Clock/effect/set", "0", true);
}

///Amimation

void setup()
{
  strip.Begin();
  strip.Show();
  SetRandomSeed();
  // attempt to connect to WiFi network
  // We start by connecting to a WiFi network
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
  if (now() != prevDisplay) { //update the display only if time has changed
    prevDisplay = now();
    digitalClockDisplay();
  }
}



void ClockFrame(int Red, int Green, int Blue) {
  RgbColor Cfill(Red, Green, Blue);
  //delete hands and write all leds in selected color
  strip.ClearTo(Cfill);
  //publish to mqtt as feedback (is this needed or wrong?)
  char restate[12];
  String i = (String(Red) + "," + String(Green) + "," + String(Blue));
  i.toCharArray(restate, 12);
  client.publish("/Clock/colorRGB", restate);
}

int ClockSegments(RgbColor Cvalue5, RgbColor Cvalue15) {
  for (int i = 1; i <= 12; i++) {
    strip.SetPixelColor(i * (PixelCount / 60) * 5, Cvalue5);
  }
  //show 15-minute segments
  for (int i = 1; i <= 4; i++) {
    strip.SetPixelColor(i * (PixelCount / 60) * 15, Cvalue15);
  }
  return 1;
}


void ClockHands(RgbColor CvalueH, RgbColor CvalueM, RgbColor CvalueS) {

  //show hour hand
  strip.SetPixelColor(map((((hourFormat12() - 1) * 5) + (minute() / 6)), 0, 59, 0, PixelCount - 1), CvalueH);
  //show minute hand
  strip.SetPixelColor(map(minute(), 0, 59, 0, PixelCount - 1), CvalueM);
  //show second hand
  strip.SetPixelColor(map(second(), 0, 59, 0, PixelCount - 1), CvalueS);
}

void ClockTimer(time_t value, RgbColor Cvalue) {
  if (value < now()) {
    RestTime = 0;
  } else {
    RestTime = (int)value - (int)now();
  }
  if (RestTime > 0) {
    char restate[24];
    sprintf(restate, "%d", RestTime);
    client.publish("/Clock/timer", restate,false);

    sprintf(restate, "%04d-%02d-%02dT%02d:%02d:%02d", year(value), month(value), day(value), hour(value), minute(value), second(value));
    // client.publish("/Clock/alarm", restate,false);
   
    if ((RestTime >= 3600) && (RestTime <= TimerLimit)) {
      // >1 hour
      for (int i = 0; i < map(RestTime, 0, 24 * 3600, 0, PixelCount); i ++) {
        strip.SetPixelColor(PixelCount - i, Cvalue);
      }
    }

    if ((RestTime > 60) && (RestTime < 3600) && (RestTime <= TimerLimit)) {
      // >1 Minute
      for (int i = 0; i <= map(RestTime, 0, 3600, 0, PixelCount); i++) {
        strip.SetPixelColor(i, Ctimer);
      }
    }

    if ((RestTime <= 60) && (RestTime <= TimerLimit)) {
      //< 1 Minute
      for (int i = 0; i <= map(RestTime, 0, 60, 0, PixelCount); i++) {
        strip.SetPixelColor(i, Ctimer);
      }
    }
  }
}

void digitalClockDisplay() {

  //Clock Frame alias Light
  ClockFrame(inRed, inGreen, inBlue);
  //Clock Segments, only shown at a minimum light level
  if ((inRed > 10) || (inGreen > 10) || (inBlue > 10)) {
    ClockSegments(Csegment5, Csegment15);
  }
  //Timer
  ClockTimer(EndTime, Ctimer);
  //Clock Hands, only shown if Light is switched on
  if (inRed + inGreen + inBlue > 0) {
    ClockHands(Chour, Cminute, Csecond);
  }
  strip.Show();
  connect();
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
