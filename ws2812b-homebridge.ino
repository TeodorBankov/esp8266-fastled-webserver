
#include <FastLED.h>
FASTLED_USING_NAMESPACE

extern "C" {
#include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <EEPROM.h>
#include "GradientPalletes.h";

const char *ssid = "YOUR WIFI NAME GOES HERE;
const char *password = "your WIFI PASSWORD GOES HERE"; 

ESP8266WebServer server(80);

#define DATA_PIN      2	        // data pin on the esp
#define LED_TYPE      WS2812B	// define your model of LEDS here
#define COLOR_ORDER   GRB	// color order
#define NUM_LEDS      300	// number of LEDs

#define FRAMES_PER_SECOND  60 

CRGB ledsPrev[NUM_LEDS];
CRGB leds[NUM_LEDS];

uint8_t patternIndex = 0;

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#define SECONDS_PER_PALETTE 30

extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );

uint8_t currentPatternIndex = 0; // Index number of which pattern is current
bool autoplayEnabled = false;

uint8_t autoPlayDurationSeconds = 10;
unsigned int autoPlayTimeout = 0;

uint8_t currentPaletteIndex = 0;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

CRGB solidColor = CRGB::Black;

uint8_t power = 0;

IPAddress Ip(192,168,5,160); 
IPAddress Gateway(192,168,5,1); 
IPAddress Subnet(255,255,255,0);

const uint8_t brightnessCount = 5;
uint8_t brightnessMap[brightnessCount] = { 16, 32, 64, 128, 255 };
int brightnessIndex = 0;
uint8_t brightness = brightnessMap[brightnessIndex];

void setup(void) {
  

  Serial.begin(115200);

  delay(100);
  Serial.setDebugOutput(true);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);         // for WS2812 (Neopixel)
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness);

  fill_solid(leds, NUM_LEDS, solidColor);
  FastLED.show();

  EEPROM.begin(512);
  loadSettings();

  FastLED.setBrightness(brightness);

  Serial.println();
  Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
  Serial.print( F("Boot Vers: ") ); Serial.println(system_get_boot_version());
  Serial.print( F("CPU: ") ); Serial.println(system_get_cpu_freq());
  Serial.print( F("SDK: ") ); Serial.println(system_get_sdk_version());
  Serial.print( F("Chip ID: ") ); Serial.println(system_get_chip_id());
  Serial.print( F("Flash ID: ") ); Serial.println(spi_flash_get_id());
  Serial.print( F("Flash Size: ") ); Serial.println(ESP.getFlashChipRealSize());
  Serial.print( F("Vcc: ") ); Serial.println(ESP.getVcc());
  Serial.println("");

  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
    }
    Serial.printf("\n");
  } 

    Serial.printf("Connecting to %s\n", ssid);
    WiFi.hostname("Bed RGB-strip");
    WiFi.config(Ip, Gateway, Subnet);
    WiFi.begin(ssid, password);
    Serial.println("");

    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
    }

    Serial.print("Connected! Open http://");
    Serial.print(WiFi.localIP());
    Serial.println(" in your browser");

    server.on("/all", HTTP_GET, []() {
    sendAll();
  });

  server.on("/power", HTTP_GET, []() {
    sendPower();
  });

  server.on("/power", HTTP_POST, []() {
    String value = server.arg("value");
    setPower(value.toInt());
    sendPower();
  });

  server.on("/powerOn", HTTP_GET, []() {
    setPower(1);
    sendPower();
  });
  server.on("/powerOff", HTTP_GET, []() {
    setPower(0);
    sendPower();
  });
  server.on("/solidColor", HTTP_GET, []() {
    sendSolidColor();
  });

  server.on("/solidColor", HTTP_POST, []() {
    String r = server.arg("r");
    String g = server.arg("g");
    String b = server.arg("b");
    setSolidColor(r.toInt(), g.toInt(), b.toInt());
    sendSolidColor();
  });
  
  server.on("/solidHexColor", HTTP_GET, []() {
    sendSolidHexColor();
  });

  server.on("/solidHexColor", HTTP_POST, []() {
    String hex = server.arg("value");
    long number = strtol( &hex[0], NULL, 16);
    //Serial.println(number);
    // Split them up into r, g, b values730011

    long r = number >> 16;
    long g = number >> 8 & 0xFF;
    long b = number & 0xFF;
    Serial.println(r);
    Serial.println(g);
    Serial.println(b);
    setPower(1);
    setSolidColor(r, g, b);
    sendSolidHexColor();
  });

  server.on("/pattern", HTTP_GET, []() {
    sendPattern();
  });

  server.on("/pattern", HTTP_POST, []() {
    String value = server.arg("value");
    setPattern(value.toInt());
    sendPattern();
  });

  server.on("/patternUp", HTTP_POST, []() {
    adjustPattern(true);
    sendPattern();
  });

  server.on("/patternDown", HTTP_POST, []() {
    adjustPattern(false);
    sendPattern();
  });

  server.on("/brightness", HTTP_GET, []() {
    sendBrightness();
  });

  server.on("/brightness", HTTP_POST, []() {
    String value = server.arg("value");
    setBrightness(value.toInt());
    sendBrightness();
  });

  server.on("/brightnessPercent", HTTP_GET, []() {
    sendBrightnessPercent();
  });

  server.on("/brightnessPercent", HTTP_POST, []() {
    String percent = server.arg("value");
    int value = 255 * (percent.toFloat() / 100);
    setBrightness(value);
    sendBrightnessPercent();
  });


  server.on("/brightnessUp", HTTP_POST, []() {
    adjustBrightness(true);
    sendBrightness();
  });

  server.on("/brightnessDown", HTTP_POST, []() {
    adjustBrightness(false);
    sendBrightness();
  });

  server.on("/palette", HTTP_GET, []() {
    sendPalette();
  });

  server.on("/palette", HTTP_POST, []() {
    String value = server.arg("value");
    setPalette(value.toInt());
    sendPalette();
  });


  server.serveStatic("/index.html", SPIFFS, "/index.html");
  server.serveStatic("/fonts", SPIFFS, "/fonts", "max-age=86400");
  server.serveStatic("/js", SPIFFS, "/js");
  server.serveStatic("/css", SPIFFS, "/css", "max-age=86400");
  server.serveStatic("/images", SPIFFS, "/images", "max-age=86400");
  server.serveStatic("/", SPIFFS, "/index.html");

  server.begin();

  Serial.println("HTTP server started");

  autoPlayTimeout = millis() + (autoPlayDurationSeconds * 1000);
}

typedef void (*Pattern)();
typedef struct {
  Pattern pattern;
  String name;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

// List of patterns to cycle through. Each is defined as a separate function below.
PatternAndNameList patterns = {
  { colorwaves, "Color Waves" },
  { palettetest, "Pallete test" },
  { pride, "pride" },
  { rainbow, "rainbow" },
  { rainbowWithGlitter, "Rainbow With Glitter" },
  { confetti, "Confetti" },
  { sinelon, "Sinelon" },
  { juggle, "Juggle" },
  { bpm, "BPM" },
  { showSolidColor, "Solid Color" },
  };

const uint8_t patternCount = ARRAY_SIZE(patterns);

typedef struct {
  CRGBPalette16 palette;
  String name;
} PaletteAndName;
typedef PaletteAndName PaletteAdnNameList[];

const CRGBPalette16 palettes[] = {
  RainbowColors_p,
  RainbowStripeColors_p,
  CloudColors_p,
  LavaColors_p,
  OceanColors_p,
  ForestColors_p,
  PartyColors_p,
  HeatColors_p
};

const uint8_t paletteCount = ARRAY_SIZE(palettes);

const String paletteNames[paletteCount] = {
  "Rainbow",
  "Rainbow Stripe",
  "Cloud",
  "Lava",
  "Ocean",
  "Forest",
  "Party",
  "Heat",
};

void loop(void) {
  
  random16_add_entropy(random(65535));
  
  server.handleClient();

  if(power == 0){
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    FastLED.delay(15);
    return;
  }

  EVERY_N_MILLISECONDS( 20 ) {
    gHue++; //slow cycle to the next color
  }

   EVERY_N_SECONDS( SECONDS_PER_PALETTE ) {
    gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
    gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
  }

   EVERY_N_MILLISECONDS(40) {
    nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 16);
  }

  if (autoplayEnabled && millis() > autoPlayTimeout) {
    adjustPattern(true);
    autoPlayTimeout = millis() + (autoPlayDurationSeconds * 1000);
  }

  // Call the current pattern function once, updating the 'leds' array
  patterns[currentPatternIndex].pattern();

  // only update if theres a change, cuts out chance of flicker
  if(leds != ledsPrev){
    memcpy(ledsPrev, leds, NUM_LEDS);
    FastLED.show();
  }

  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void loadSettings()
{
  brightness = EEPROM.read(0);

  currentPatternIndex = EEPROM.read(1);
  if (currentPatternIndex < 0)
    currentPatternIndex = 0;
  else if (currentPatternIndex >= patternCount)
    currentPatternIndex = patternCount - 1;

  byte r = EEPROM.read(2);
  byte g = EEPROM.read(3);
  byte b = EEPROM.read(4);

  if (r == 0 && g == 0 && b == 0)
  {
  }
  else
  {
    solidColor = CRGB(r, g, b);
  }

  currentPaletteIndex = EEPROM.read(5);
  if (currentPaletteIndex < 0)
    currentPaletteIndex = 0;
  else if (currentPaletteIndex >= paletteCount)
    currentPaletteIndex = paletteCount - 1;
}

void sendAll()
{
  String json = "{";

  json += "\"power\":" + String(power) + ",";
  json += "\"brightness\":" + String(brightness) + ",";

  json += "\"currentPattern\":{";
  json += "\"index\":" + String(currentPatternIndex);
  json += ",\"name\":\"" + patterns[currentPatternIndex].name + "\"}";

  json += ",\"currentPalette\":{";
  json += "\"index\":" + String(currentPaletteIndex);
  json += ",\"name\":\"" + paletteNames[currentPaletteIndex] + "\"}";

  json += ",\"solidColor\":{";
  json += "\"r\":" + String(solidColor.r);
  json += ",\"g\":" + String(solidColor.g);
  json += ",\"b\":" + String(solidColor.b);
  json += "}";

  json += ",\"patterns\":[";
  for (uint8_t i = 0; i < patternCount; i++)
  {
    json += "\"" + patterns[i].name + "\"";
    if (i < patternCount - 1)
      json += ",";
  }
  json += "]";

  json += ",\"palettes\":[";
  for (uint8_t i = 0; i < paletteCount; i++)
  {
    json += "\"" + paletteNames[i] + "\"";
    if (i < paletteCount - 1)
      json += ",";
  }
  json += "]";

  json += "}";

  server.send(200, "text/json", json);
  json = String();
}

void sendPower()
{
  String json = String(power);
  server.send(200, "text/json", json);
  json = String();
}

void sendPattern()
{
  String json = "{";
  json += "\"index\":" + String(currentPatternIndex);
  json += ",\"name\":\"" + patterns[currentPatternIndex].name + "\"";
  json += "}";
  server.send(200, "text/json", json);
  json = String();
}

void sendPalette()
{
  String json = "{";
  json += "\"index\":" + String(currentPaletteIndex);
  json += ",\"name\":\"" + paletteNames[currentPaletteIndex] + "\"";
  json += "}";
  server.send(200, "text/json", json);
  json = String();
}

void sendBrightness()
{
  String json = String(brightness);
  server.send(200, "text/json", json);
  json = String();
}

void sendBrightnessPercent()
{
  int percent = (brightness / 255.0) * 100;
  String json = String(percent);
  server.send(200, "text/json", json);
  json = String();
}

void sendSolidColor()
{
  String json = "{";
  json += "\"r\":" + String(solidColor.r);
  json += ",\"g\":" + String(solidColor.g);
  json += ",\"b\":" + String(solidColor.b);
  json += "}";
  server.send(200, "text/json", json);
  json = String();
}

void sendSolidHexColor()
{
  char hexout[6];
  sprintf(hexout, "%02x%02x%02x", solidColor.r, solidColor.g, solidColor.b);
  String json = String(hexout);
  server.send(200, "text/json", json);
  json = String();
}

void setPower(uint8_t value)
{
  power = value == 0 ? 0 : 1;
}

void setSolidColor(CRGB color)
{
  setSolidColor(color.r, color.g, color.b);
}

void setSolidColor(uint8_t r, uint8_t g, uint8_t b)
{
  solidColor = CRGB(r, g, b);

  EEPROM.write(2, r);
  EEPROM.write(3, g);
  EEPROM.write(4, b);

  setPattern(patternCount - 1);
}

// increase or decrease the current pattern number, and wrap around at the ends
void adjustPattern(bool up)
{
  if (up)
    currentPatternIndex++;
  else
    currentPatternIndex--;

  // wrap around at the ends
  if (currentPatternIndex < 0)
    currentPatternIndex = patternCount - 1;
  if (currentPatternIndex >= patternCount)
    currentPatternIndex = 0;

  if (autoplayEnabled) {
    EEPROM.write(1, currentPatternIndex);
    EEPROM.commit();
  }
}

void setPattern(int value)
{
  // don't wrap around at the ends
  if (value < 0)
    value = 0;
  else if (value >= patternCount)
    value = patternCount - 1;

  currentPatternIndex = value;

  if (autoplayEnabled == 0) {
    EEPROM.write(1, currentPatternIndex);
    EEPROM.commit();
  }
}

void setPalette(int value)
{
  // don't wrap around at the ends
  if (value < 0)
    value = 0;
  else if (value >= paletteCount)
    value = paletteCount - 1;

  currentPaletteIndex = value;

  EEPROM.write(5, currentPaletteIndex);
  EEPROM.commit();
}

// adjust the brightness, and wrap around at the ends
void adjustBrightness(bool up)
{
  if (up)
    brightnessIndex++;
  else
    brightnessIndex--;

  // wrap around at the ends
  if (brightnessIndex < 0)
    brightnessIndex = brightnessCount - 1;
  else if (brightnessIndex >= brightnessCount)
    brightnessIndex = 0;

  brightness = brightnessMap[brightnessIndex];

  FastLED.setBrightness(brightness);

  EEPROM.write(0, brightness);
  EEPROM.commit();
}

void setBrightness(int value)
{
  // don't wrap around at the ends
  if (value > 255)
    value = 255;
  else if (value < 0) value = 0;

  brightness = value;

  FastLED.setBrightness(brightness);

  EEPROM.write(0, brightness);
  EEPROM.commit();
}

void showSolidColor()
{
  fill_solid(leds, NUM_LEDS, solidColor);
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 10);
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += ColorFromPalette(palettes[currentPaletteIndex], gHue + random8(64));
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13, 0, NUM_LEDS - 1);
  leds[pos] += ColorFromPalette(palettes[currentPaletteIndex], gHue, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = palettes[currentPaletteIndex];
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void juggle()
{
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++)
  {
    leds[beatsin16(i + 7, 0, NUM_LEDS)] |= ColorFromPalette(palettes[currentPaletteIndex], dothue);
    dothue += 32;
  }
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void pride() {
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    nblend(leds[i], newcolor, 64);
  }
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if ( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette(gCurrentPalette, index, bri8);

    nblend(leds[i], newcolor, 128);
  }
}

// Alternate rendering function just scrolls the current palette
// across the defined LED strip.
void palettetest()
{
  static uint8_t startindex = 0;
  startindex--;
  fill_palette( leds, NUM_LEDS, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, 255, LINEARBLEND);
}
