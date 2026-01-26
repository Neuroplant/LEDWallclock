/*
  LED Wall Clock
  ESP-01 WS2812B with 60 LEDs (or more) on pin 2
  Additional Libraries:
	- FS / SPIFFS
	- WiFiManager
	- WiFi / WiFiUDP
	- TimeLib (Time)
	- ArduinoJson (v6.x)
	- NeoPixelBus
	- NeoPixelAnimator
	- PubSubClient
  
  Features:
  - Get time from NTP (instead of buying an RTC module)
  - Use free pixels as colour light (MQTT /Clock/colorRGB/set)
  - Timer (seconds, max 24h) (MQTT /Clock/timer/set)
  - Animation alarm (MQTT /Clock/effect/set)
  
  Parts:
   - ESP8266-01 (ESP-01)
   - ESP8266 ESP-01/01S RGB-LED-Controller-Adapter. 
   - 5V Powersupply (or something like this https://www.amazon.de/dp/B0F9Y5LQJB?ref=ppx_yo2ov_dt_b_fed_asin_title)
   
   [when I created this Project it was nessesary to build your own PCB but now you can buy one assembled with USB-C Connector
    - PCB (60mm * 40mm)
  - DC-DC Converter (set to 5V output) (e.g., for 60 LEDs this will be sufficient; for more LEDs, use a bigger one)
    https://www.amazon.de/AZDelivery-LM2596S-Netzteil-Adapter-Arduino/dp/B07DP3JX2X/
  - Level Shifter (e.g., https://www.amazon.de/XCSOURCE®-Logisches-Konverter-Bi-Direktional-TE291/dp/B0148BLZGE/)
  - Resistor 470 Ohm
  - Capacitor 1000µF / 6.3V
  - DC Power Supply
  ]
*/

#include <FS.h>               // File system for SPIFFS
#include <WiFiManager.h>      // For WiFi configuration via captive portal
#include <WiFiUdp.h>          // For NTP (UDP)
#include <TimeLib.h>          // Time functions (time_t, now(), etc.)
#include <stdlib.h>           // Standard C++ functions (e.g., map, random)

#ifdef ESP32
  #include <SPIFFS.h>
#endif

#include <ArduinoJson.h>      // https://github.com/bblanchon/ArduinoJson
#include <NeoPixelBus.h>      // https://github.com/Makuna/NeoPixelBus
#include <NeoPixelAnimator.h> // Animations (fade, blend, etc.)

// WiFi Settings

// MQTT Settings
char MQTTServer[40];
char MQTTClient[20];
char MQTTPW[20];

// Flag for saving configuration
bool shouldSaveConfig = false;

// Callback notifying us of the need to save configuration
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Hardware Settings
const uint16_t PixelCount = 120;  // Best effect with multiples of 60 LEDs, other numbers also work
const uint8_t PixelPin = 2;       // Ensure this is the correct pin; ignored for ESP8266
int TimerLimit = 1800;            // Timer shows only if remaining time < TimerLimit
int Rotation = 0;                 // If the first pixel is not at "up"

// NTP Settings
IPAddress timeServer(134, 95, 192, 172);  // University of Cologne
const int timeZone = 1;                   // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

// Colour Settings (leave default)
#define colorSaturation 255
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1Ws2812xMethod> strip(PixelCount, PixelPin);
RgbColor Csecond(map(255, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation));
RgbColor Cminute(map(0, 0, 255, 0, colorSaturation), map(255, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation));
RgbColor Chour(map(0, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation), map(255, 0, 255, 0, colorSaturation));
RgbColor Csegment5(map(10, 0, 255, 0, colorSaturation), map(10, 0, 255, 0, colorSaturation), map(10, 0, 255, 0, colorSaturation));
RgbColor Csegment15(map(20, 0, 255, 0, colorSaturation), map(20, 0, 255, 0, colorSaturation), map(20, 0, 255, 0, colorSaturation));
RgbColor Ctimer(map(100, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation), map(0, 0, 255, 0, colorSaturation));
RgbColor black(0);

// Animation Settings (leave default)
int AniTime = 10;       // How long the animation is shown (seconds)
int NextAnimation = 1;  // Animation selected for alarm

// Input RGB from MQTT
long FrameRed = 50;    // 0..255
long FrameGreen = 50;  // 0..255
long FrameBlue = 50;   // 0..255

// Alarm
time_t EndTime = now();      // Alarm triggers at EndTime
unsigned long RestTime = 0;  // Seconds remaining until alarm

// Time when the digital clock was last displayed
time_t prevDisplay = 0;

// WiFi radio status
int status = WL_IDLE_STATUS;

// UDP instance for sending/receiving packets
WiFiUDP Udp;
unsigned int localPort = 8888;  // Local port for UDP
WiFiClient net;                  // NTP client

// Rotate by [Rotation] pixels to maintain clock orientation
int Protate(int i) {
  i = i + Rotation;
  if (i >= PixelCount) {
    i = i - PixelCount;
  }
  return i;
}

// Timer storage
constexpr size_t maxTimerStored = 16;
time_t TimerList[maxTimerStored] {0};

// Store new timer in the list
bool StoreTimer(time_t value) {
  const time_t current = now();

  // Remove expired timers
  for (int i = 0; i < maxTimerStored; i++) {
    if (TimerList[i] <= current) {
      TimerList[i] = 0;
    }
  }

  // Add new timer
  for (int i = 0; i < maxTimerStored; i++) {
    if (TimerList[i] == 0) {
      TimerList[i] = value;
      return true;
    }
  }

  return false; // No free slot
}

// Get next timer from the list
time_t QueryTimer() {
  time_t value = LONG_MAX;
  for (int i = 0; i < maxTimerStored; i++) {
    if ((TimerList[i] != 0) && (TimerList[i] < value)) {
      value = TimerList[i];
    }
  }
  return value;
}

// MQTT client
PubSubClient client(net);
void AnimationSelect(int value);

// Connect to MQTT and subscribe
void connect() {
  while (WiFi.status() != WL_CONNECTED) delay(1000);
  while (!client.connect(MQTTServer, MQTTClient, MQTTPW)) delay(1000);

  // Subscribe to relevant topics
  client.subscribe("/Uhr/colorRGB/set");
  client.subscribe("/Uhr/effect/set");
  client.subscribe("/Uhr/timer/set");
  client.subscribe("/Uhr/alarm/set");
  client.subscribe("/Uhr/TimerLimit/set");
  client.subscribe("/Uhr/Rotation/set");
  client.subscribe("/Uhr/Csecond/set");
  client.subscribe("/Uhr/Cminute/set");
  client.subscribe("/Uhr/Chour/set");
  client.subscribe("/Uhr/Csegment5/set");
  client.subscribe("/Uhr/Csegment15/set");
  client.subscribe("/Uhr/Ctimer/set");
  client.subscribe("/Uhr/NextAnimation/set");
  client.subscribe("/Uhr/AniTime/set");
}

// MQTT callback handler
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length-1] = 0;
  String Payload = String((char*)payload);

  // Change colour
  if (strcmp(topic, "/Uhr/colorRGB/set") == 0) {
    FrameRed = Payload.substring(0, Payload.indexOf(',')).toInt();
    FrameGreen = Payload.substring(Payload.indexOf(',') + 1, Payload.lastIndexOf(',')).toInt();
    FrameBlue = Payload.substring(Payload.lastIndexOf(',') + 1).toInt();
  }

  // Start animation
  if (strcmp(topic, "/Uhr/effect/set") == 0) {
    int i = Payload.substring(0, length).toInt();
    AnimationSelect(i);
  }

  // Start timer
  if (strcmp(topic, "/Uhr/timer/set") == 0) {
    int i = Payload.substring(0, length).toInt();
    StoreTimer(i + now());
  }

  // Set alarm
  if (strcmp(topic, "/Uhr/alarm/set") == 0) {
    if (Payload != "UNDEF") {
      tm timetm;
      if (Payload.substring(10, 11)=="T") {
        strptime(Payload.c_str(), "%Y-%m-%dT%H:%M:%S", &timetm);
      } else {
        strptime(Payload.c_str(), "%Y-%m-%d %H:%M:%S", &timetm);
      }
      StoreTimer(mktime(&timetm));
    }
  }

  // Set timer limit
  if (strcmp(topic, "/Uhr/TimerLimit/set") == 0) {
    int i = Payload.substring(0, length).toInt();
    TimerLimit = i;
    char restate[length];
    String(TimerLimit).toCharArray(restate, length);
    client.publish("/Uhr/TimerLimit/", restate);
  }

  // Set rotation
  if (strcmp(topic, "/Uhr/Rotation/set") == 0) {
    int i = Payload.substring(0, length).toInt();
    Rotation = i;
    char restate[length];
    String(Rotation).toCharArray(restate, length);
    client.publish("/Uhr/Rotation/", restate);
  }

  // Set second hand colour
  if (strcmp(topic, "/Uhr/Csecond/set") == 0) {
    long TRed = Payload.substring(0, Payload.indexOf(',')).toInt();
    long TGreen = Payload.substring(Payload.indexOf(',') + 1, Payload.lastIndexOf(',')).toInt();
    long TBlue = Payload.substring(Payload.lastIndexOf(',') + 1).toInt();
    Csecond = RgbColor(map(TRed, 0, 255, 0, colorSaturation),
                        map(TGreen, 0, 255, 0, colorSaturation),
                        map(TBlue, 0, 255, 0, colorSaturation));
    char restate[length];
    String i = String(TRed) + "," + String(TGreen) + "," + String(TBlue);
    i.toCharArray(restate, length);
    client.publish("/Uhr/Csecond/", restate);
  }

 //"/Uhr/Cminute/set");
  if (strcmp(topic, "/Uhr/Cminute/set") == 0) {
    long TRed = Payload.substring(0, Payload.indexOf(',')).toInt();
    long TGreen = Payload.substring(Payload.indexOf(',') + 1, Payload.lastIndexOf(',')).toInt();
    long TBlue = Payload.substring(Payload.lastIndexOf(',') + 1).toInt();
    Cminute = RgbColor(map(TRed, 0, 255, 0, colorSaturation), map(TGreen, 0, 255, 0, colorSaturation), map(TBlue, 0, 255, 0, colorSaturation));
    char restate[length];
    String i = (String(TRed) + "," + String(TGreen) + "," + String(TBlue));
    i.toCharArray(restate, length);
    client.publish("/Uhr/Cminute/", restate);
  }
  //"/Uhr/Chour/set");
  if (strcmp(topic, "/Uhr/Chour/set") == 0) {
    long TRed = Payload.substring(0, Payload.indexOf(',')).toInt();
    long TGreen = Payload.substring(Payload.indexOf(',') + 1, Payload.lastIndexOf(',')).toInt();
    long TBlue = Payload.substring(Payload.lastIndexOf(',') + 1).toInt();
    Chour = RgbColor(map(TRed, 0, 255, 0, colorSaturation), map(TGreen, 0, 255, 0, colorSaturation), map(TBlue, 0, 255, 0, colorSaturation));
    char restate[length];
    String i = (String(TRed) + "," + String(TGreen) + "," + String(TBlue));
    i.toCharArray(restate, length);
    client.publish("/Uhr/Chour/", restate);
  }
  //"/Uhr/Csegment5/set");
  if (strcmp(topic, "/Uhr/Csegment5/set") == 0) {
    long TRed = Payload.substring(0, Payload.indexOf(',')).toInt();
    long TGreen = Payload.substring(Payload.indexOf(',') + 1, Payload.lastIndexOf(',')).toInt();
    long TBlue = Payload.substring(Payload.lastIndexOf(',') + 1).toInt();
    Csegment5 = RgbColor(map(TRed, 0, 255, 0, colorSaturation), map(TGreen, 0, 255, 0, colorSaturation), map(TBlue, 0, 255, 0, colorSaturation));
    char restate[length];
    String i = (String(TRed) + "," + String(TGreen) + "," + String(TBlue));
    i.toCharArray(restate, length);
    client.publish("/Uhr/Csegment5/", restate);
  }
  //"/Uhr/Csegment15/set");
  if (strcmp(topic, "/Uhr/Csegment15/set") == 0) {
    long TRed = Payload.substring(0, Payload.indexOf(',')).toInt();
    long TGreen = Payload.substring(Payload.indexOf(',') + 1, Payload.lastIndexOf(',')).toInt();
    long TBlue = Payload.substring(Payload.lastIndexOf(',') + 1).toInt();
    Csegment15 = RgbColor(map(TRed, 0, 255, 0, colorSaturation), map(TGreen, 0, 255, 0, colorSaturation), map(TBlue, 0, 255, 0, colorSaturation));
    char restate[length];
    String i = (String(TRed) + "," + String(TGreen) + "," + String(TBlue));
    i.toCharArray(restate, length);
    client.publish("/Uhr/Csegment15/", restate);
  }
  //"/Uhr/Ctimer/set");
  if (strcmp(topic, "/Uhr/Ctimer/set") == 0) {
    long TRed = Payload.substring(0, Payload.indexOf(',')).toInt();
    long TGreen = Payload.substring(Payload.indexOf(',') + 1, Payload.lastIndexOf(',')).toInt();
    long TBlue = Payload.substring(Payload.lastIndexOf(',') + 1).toInt();
    Ctimer = RgbColor(map(TRed, 0, 255, 0, colorSaturation), map(TGreen, 0, 255, 0, colorSaturation), map(TBlue, 0, 255, 0, colorSaturation));
    char restate[length];
    String i = (String(TRed) + "," + String(TGreen) + "," + String(TBlue));
    i.toCharArray(restate, length);
    client.publish("/Uhr/Ctimer/", restate);
  }
  //"/Uhr/NextAnimation/set");
  if (strcmp(topic, "/Uhr/NextAnimation/set") == 0) {
    int i = Payload.substring(0, length).toInt();
    NextAnimation = i;
    char restate[length];
    String(NextAnimation).toCharArray(restate, length);
    client.publish("/Uhr/NextAnimation/", restate);
  }
  //"/Uhr/AniTime/set");
  if (strcmp(topic, "/Uhr/AniTime/set") == 0) {
    int i = Payload.substring(0, length).toInt();
    AniTime = i;
    char restate[length];
    String(AniTime).toCharArray(restate, length+1);
    client.publish("/Uhr/AniTime/", restate);
  }
}

///Animation
//taken from the examples at NeoPixelBus

//Animaton 1 (NeoPixelFunLoop)
const uint16_t AnimCount = PixelCount / 5 * 2 + 1;  // we only need enough animations for the tail and one extra
const uint16_t PixelFadeDuration = 300;             // third of a second
// one second divide by the number of pixels = loop once a second
const uint16_t NextPixelMoveDuration = 1000 / PixelCount;  // how fast we move through the pixels
NeoGamma<NeoGammaTableMethod> colorGamma;                  // for any fade animations, best to correct gamma

struct MyAnimationState {
  RgbColor StartingColor;
  RgbColor EndingColor;
  uint16_t IndexPixel;  // which pixel this animation is effecting
};
NeoPixelAnimator animations(AnimCount);  // NeoPixel animation management object
MyAnimationState animationState[AnimCount];
uint16_t frontPixel = 0;  // the front of the loop
RgbColor frontColor;      // the color at the front of the loop

void SetRandomSeed(void) {
  uint32_t seed;
  // random works best with a seed that can use 31 bits
  // analogRead on a unconnected pin tends toward less than four bits
  seed = analogRead(0);
  delay(1);
  for (int shifts = 3; shifts < 31; shifts += 3) {
    seed ^= analogRead(0) << shifts;
    delay(1);
  }
  randomSeed(seed);
}
void FadeOutAnimUpdate(const AnimationParam& param) {
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
void LoopAnimUpdate(const AnimationParam& param) {
  // wait for this animation to complete,
  // we are using it as a timer of sorts
  if (param.state == AnimationState_Completed) {
    // done, time to restart this position tracking animation/timer
    animations.RestartAnimation(param.index);
    // pick the next pixel inline to start animating
    //
    frontPixel = (frontPixel + 1) % PixelCount;  // increment and wrap
    if (frontPixel == 0) {
      // we looped, lets pick a new front color
      frontColor = HslColor(random(360) / 360.0f, 1.0f, 0.25f);
    }
    uint16_t indexAnim;
    // do we have an animation available to use to animate the next front pixel?
    // if you see skipping, then either you are going to fast or need to increase
    // the number of animation channels
    if (animations.NextAvailableAnimation(&indexAnim, 1)) {
      animationState[indexAnim].StartingColor = frontColor;
      animationState[indexAnim].EndingColor = RgbColor(0, 0, 0);
      animationState[indexAnim].IndexPixel = frontPixel;
      animations.StartAnimation(indexAnim, PixelFadeDuration, FadeOutAnimUpdate);
    }
  }
}

///Animation 2 (FunRandomChange)
NeoPixelAnimator animations02(PixelCount);  // NeoPixel animation management object

struct MyAnimationState2 {
  RgbColor StartingColor;
  RgbColor EndingColor;
};
MyAnimationState2 animationState02[PixelCount];

void BlendAnimUpdate02(const AnimationParam& param) {
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
void PickRandom02(float luminance) {
  // pick random count of pixels to animate
  uint16_t count = random(PixelCount);
  while (count > 0) {
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


///Animation 3 (FunFadeInOut)

const uint8_t AnimationChannels = 1;               // we only need one as all the pixels are animated at once
NeoPixelAnimator animations03(AnimationChannels);  // NeoPixel animation management object
MyAnimationState animationState03[AnimationChannels];
boolean fadeToColor = true;  // general purpose variable used to store effect state

struct MyAnimationState03 {
  RgbColor StartingColor;
  RgbColor EndingColor;
};


void BlendAnimUpdate03(const AnimationParam& param) {
  // this gets called for each animation on every time step
  // progress will start at 0.0 and end at 1.0
  // we use the blend function on the RgbColor to mix
  // color based on the progress given to us in the animation
  RgbColor updatedColor = RgbColor::LinearBlend(
    animationState03[param.index].StartingColor,
    animationState03[param.index].EndingColor,
    param.progress);

  // apply the color to the strip
  for (uint16_t pixel = 0; pixel < PixelCount; pixel++) {
    strip.SetPixelColor(pixel, updatedColor);
  }
}

void FadeInFadeOutRinseRepeat(float luminance) {
  if (fadeToColor) {
    // Fade upto a random color
    // we use HslColor object as it allows us to easily pick a hue
    // with the same saturation and luminance so the colors picked
    // will have similiar overall brightness
    RgbColor target = HslColor(random(360) / 360.0f, 1.0f, luminance);
    uint16_t time = random(800, 2000);

    animationState03[0].StartingColor = strip.GetPixelColor(0);
    animationState03[0].EndingColor = target;

    animations03.StartAnimation(0, time, BlendAnimUpdate03);
  } else {
    // fade to black
    uint16_t time = random(600, 700);

    animationState03[0].StartingColor = strip.GetPixelColor(0);
    animationState03[0].EndingColor = RgbColor(0);

    animations03.StartAnimation(0, time, BlendAnimUpdate03);
  }

  // toggle to the next effect state
  fadeToColor = !fadeToColor;
}

void AnimationSelect(int value) {
  int i = now();
  switch (value) {
    case 0:
      break;

    case 1:
      client.publish("/Uhr/effect", "1");
      strip.ClearTo(black);
      animations.StartAnimation(0, NextPixelMoveDuration, LoopAnimUpdate);
      //int i = now();
      while (i + AniTime > now()) {
        animations.UpdateAnimations();
        Serial.print(".");
        strip.Show();
		client.loop();

      }

      break;
      //* Don't know what is wrong here, any help welcome
    case 2:
      client.publish("/Uhr/effect", "2");
      strip.ClearTo(black);
      while (i + AniTime > now()) {
        if (animations02.IsAnimating()) {
          animations02.UpdateAnimations();
          strip.Show();
		  client.loop();
        } else {
          PickRandom02(0.2f);  // 0.0 = black, 0.25 is normal, 0.5 is bright
        }
      }
      break;
    case 3:
      client.publish("/Uhr/effect", "3");
      strip.ClearTo(black);
      SetRandomSeed();
      while (i + AniTime > now()) {
        if (animations03.IsAnimating()) {
          animations03.UpdateAnimations();
          strip.Show();
		  client.loop();
        } else {
          FadeInFadeOutRinseRepeat(0.2f);  // 0.0 = black, 0.25 is normal, 0.5 is bright
        }
      }

      break;
  }
  client.publish("/Uhr/effect", "0", true);
  client.publish("/Uhr/effect/set", "0", true);
}

void setup() {
  strip.Begin();
  strip.Show();
  SetRandomSeed();
  Serial.begin(115200);
  
  /*
  // attempt to connect to WiFi network
  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.begin(9600);
  Serial.print("Connecting to ");
  Serial.print(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  */
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

 #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if ( ! deserializeError ) {
#else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
#endif

          Serial.println("\nparsed json");
          strcpy(MQTTServer, json["MQTTServer"]);
          strcpy(MQTTClient, json["MQTTClient"]);
          strcpy(MQTTPW, json["MQTTPW"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_MQTTServer("MQTTServer", "MQTTServer(royser03)", MQTTServer, 40);
  WiFiManagerParameter custom_MQTTClient("Client", "Client(MQDev)", MQTTClient, 6);
  WiFiManagerParameter custom_MQTTPW("Passwort", "Passwort(Client)", MQTTPW, 32);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10, 0, 1, 99), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));

  //add all your parameters here
  wifiManager.addParameter(&custom_MQTTServer);
  wifiManager.addParameter(&custom_MQTTClient);
  wifiManager.addParameter(&custom_MQTTPW);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("ClockLight", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(MQTTServer, custom_MQTTServer.getValue());
  strcpy(MQTTClient, custom_MQTTClient.getValue());
  strcpy(MQTTPW, custom_MQTTPW.getValue());
  Serial.println("The values in the file are: ");
  Serial.println("\tMQTTServer (royser03): " + String(MQTTServer));
  Serial.println("\tMQTTClient (MQDev): " + String(MQTTClient));
  Serial.println("\tMQTTPW (Client): " + String(MQTTPW));

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
 #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif
    json["MQTTServer"] = MQTTServer;
    json["MQTTClient"] = MQTTClient;
    json["MQTTPW"] = MQTTPW;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
    serializeJson(json, Serial);
    serializeJson(json, configFile);
#else
    json.printTo(Serial);
    json.printTo(configFile);
#endif
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Udp.begin(localPort);

  setSyncProvider(getNtpTime);
  setSyncInterval(3600);

  client.setServer(MQTTServer, 1883);
  client.setCallback(callback);

  connect();
}

void loop() {

  client.loop();
  if (!client.connected()) {
    connect();
  }
  if (now() != prevDisplay) {  //update the display only if time has changed
    prevDisplay = now();
    digitalClockDisplay();
  }
}

void ClockFrame(int Red, int Green, int Blue) {
  RgbColor Cfill(Red, Green, Blue);
  //delete hands and write all leds in selected color
  strip.ClearTo(Cfill);
  //publish to mqtt as feedback
  char restate[12];
  String i = (String(Red) + "," + String(Green) + "," + String(Blue));
  i.toCharArray(restate, 12);
  client.publish("/Uhr/colorRGB", restate);
}

int ClockSegments(RgbColor Cvalue5, RgbColor Cvalue15) {
  //show 5-minute segments
  for (int i = 0; i <= 11; i++) {
    strip.SetPixelColor(Protate(i * (PixelCount / 60) * 5), Cvalue5);
    strip.SetPixelColor(Protate(1 + (i * (PixelCount / 60) * 5)), Cvalue5);
  }
  //show 15-minute segments
  for (int i = 0; i <= 3; i++) {
    strip.SetPixelColor(Protate(i * (PixelCount / 60) * 15), Cvalue15);
    strip.SetPixelColor(Protate(1 + (i * (PixelCount / 60) * 15)), Cvalue15);
  }
  return 1;
}

void ClockHands(RgbColor CvalueH, RgbColor CvalueM, RgbColor CvalueS) {
  //show hour hand
  int i = (hourFormat12() * 5) + (int)(minute() / 12 + 0.5);
  if (i > 59) i -= 60;
  strip.SetPixelColor(Protate(map(i, 0, 59, 0, PixelCount - 1)), CvalueH);
  //show minute hand
  strip.SetPixelColor(Protate(map(minute(), 0, 59, 0, PixelCount - 1)), CvalueM);
  //show second hand
  strip.SetPixelColor(Protate(map(second(), 0, 59, 0, PixelCount - 1)), CvalueS);
}

void ClockTimer(time_t value, RgbColor Cvalue) {
  if (value < now()) {
    RestTime = 0;
  } else {
    RestTime = (unsigned long)value - (unsigned long)now();
  }
  if (RestTime > 0) {
    char restate[24];
    sprintf(restate, "%d", RestTime);
    client.publish("/Uhr/timer", restate, false);
    sprintf(restate, "%04d-%02d-%02d %02d:%02d:%02d", year(value), month(value), day(value), hour(value), minute(value), second(value));
    client.publish("/Uhr/alarm", restate, false);
    if ((RestTime >= 3600) && (RestTime <= TimerLimit)) {
      // >1 hour
      for (int i = 0; i < map(RestTime, 0, 24 * 3600, 0, PixelCount); i++) {
        strip.SetPixelColor(Protate(PixelCount - i), Cvalue);
      }
    }

    if ((RestTime > 60) && (RestTime < 3600) && (RestTime <= TimerLimit)) {
      // >1 Minute
      for (int i = 0; i <= map(RestTime, 0, 3600, 0, PixelCount); i++) {
        strip.SetPixelColor(Protate(i), Ctimer);
      }
    }

    if ((RestTime <= 60) && (RestTime <= TimerLimit)) {
      //< 1 Minute
      for (int i = 0; i <= map(RestTime, 0, 60, 0, PixelCount); i++) {
        strip.SetPixelColor(Protate(i), Ctimer); 
      }
    }

    if (RestTime <= 1) {
      AnimationSelect(NextAnimation);
      }
  }
}

void digitalClockDisplay() {
  Serial.println("digitalClockDisplay");



  //Clock Frame alias Light
  ClockFrame(FrameRed, FrameGreen, FrameBlue);
   //Timer
  ClockTimer(QueryTimer(), Ctimer);
//Static markers for 5/15 Minutes
  ClockSegments(Csegment5, Csegment15);
//Clock Hands
   ClockHands(Chour, Cminute, Csecond);
  
  strip.Show();
}
/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;      // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];  //buffer to hold incoming & outgoing packets
time_t getNtpTime() {
  while (Udp.parsePacket() > 0)
    ;  // discard any previously received packets
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0;  // return 0 if unable to get the time
}
// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);  //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
/*-------- NTP code ----------*/														 
