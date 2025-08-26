/*
  ===================================================================
   ESP32 Color Composite Dashboard with Glow Plug
   -----------------------------------------------------------
   Features:
   - Each metric (oil, coolant, fuel) has its own function
   - Reads sensors, calculates values, and draws gauge/icon
   - Flashing background for critical values
   - Glow plug control with countdown timer and icon
   - No afterglow

  Libraries Required:
  -------------------
  1. CompositeVideo:
     - Provides the ability to output PAL/NTSC composite video from the ESP32.
     - Handles the low-level video timing and signal generation.
     - GitHub / installation: https://github.com/bitluni/esp32-color-pal
     
  2. CompositeGraphics:
     - Built on top of CompositeVideo.
     - Provides simple graphics primitives (drawRect, fillRect, drawBitmap, text, setHue, etc.).
     - Handles drawing pixel-art icons and bars on the composite video screen.
     - Usually included in the same repository as CompositeVideo.

  3. Arduino core for ESP32:
     - Make sure you have the ESP32 board definitions installed in Arduino IDE.
     - Needed to compile and upload code to ESP32 boards.

  Pin Assignments:
  ----------------
  - Oil pressure switch input: GPIO 2 (digital input)
    HIGH = low pressure / warning
    LOW  = OK

  - Coolant temperature sensor: GPIO 32 (analog input)
    Read with analogRead(), mapped to degrees Celsius

  - Fuel level sensor: GPIO 33 (analog input)
    Read with analogRead(), mapped to liters

  - Glow plug button: GPIO 15 (digital input, active LOW)
    Press button to start glow plug sequence

  - Glow plug MOSFET control: GPIO 16 (digital output)
    HIGH = turn on glow plug
    LOW  = turn off glow plug

  Other Notes:
  ------------
  - TV output: connect ESP32 DAC pins (usually GPIO 25 or 26) to TV composite input with proper resistor network if needed.
  - Ensure proper power supply for ESP32 and glow plug circuit (MOSFET rated for current).
  - Screen will display:
      - Oil, coolant, and fuel icons with color-coded gauges
      - Flashing background if any value is critical
      - Glow plug countdown when activated
  - Each metric has its own function for reading sensor and drawing the gauge for modularity.

  ===================================================================
*/

#include <CompositeGraphics.h>
#include <CompositeVideo.h>
#include <Arduino.h>

// --- Video setup ---
CompositeGraphics graphics(CompositeVideo::PAL, 128, 96);

// --- Pins ---
const int oilPin         = 2;
const int coolantPin     = 32;
const int fuelPin        = 33;
const int glowButtonPin  = 15;  // Button to start glow
const int glowPin        = 16;  // MOSFET controlling glow plug

// --- Calibration ---
const int coolantADCMin  = 100;
const int coolantADCMax  = 900;
const int coolantCMin    = 0;
const int coolantCMax    = 120;
const int coolantCriticalC = 100;
const int coolantNormalMin = 70; // normal operating temp

const int fuelADCMin     = 80;
const int fuelADCMax     = 900;
const int fuelLitersMin  = 0;
const int fuelLitersMax  = 50;
const int fuelCriticalLiters = 5;

// --- Glow parameters ---
const int glowMinTime    = 3;  // seconds
const int glowMaxTime    = 8;  // seconds
const int glowTempMin    = 0;  // °C coldest temp
const int glowTempMax    = 70; // °C warm engine

// --- Flash ---
unsigned long lastFlash  = 0;
bool flashState          = true;
const unsigned long flashInterval = 500;

// --- Colors ---
uint16_t DARKBLUE = 10; // normal background
uint16_t WHITE     = 40; // flash
uint16_t BLACK     = 0;

// -------------------------------------------------------------------
// Pixel-art icons (16x16)
const unsigned char oilIcon[32] PROGMEM = { /* same as before */ 
  0b00001111,0b00000000,0b00011111,0b10000000,
  0b00111111,0b11000000,0b00110011,0b11000000,
  0b00111111,0b11000000,0b00011111,0b10000000,
  0b00001111,0b00000000,0b00000110,0b00000000,
  0b00000110,0b00000000,0b00001111,0b00000000,
  0b00011111,0b10000000,0b00111111,0b11000000,
  0b00110011,0b11000000,0b00111111,0b11000000,
  0b00011111,0b10000000,0b00001111,0b00000000
};

const unsigned char tempIcon[32] PROGMEM = { /* same as before */ 
  0b00000110,0b00000000,0b00000110,0b00000000,
  0b00000110,0b00000000,0b00000110,0b00000000,
  0b00000110,0b00000000,0b00000110,0b00000000,
  0b00000110,0b00000000,0b00000110,0b00000000,
  0b00001111,0b00000000,0b00011111,0b10000000,
  0b00111111,0b11000000,0b00111111,0b11000000,
  0b00011111,0b10000000,0b00001111,0b00000000,
  0b00000110,0b00000000,0b00000110,0b00000000
};

const unsigned char fuelIcon[32] PROGMEM = { /* same as before */ 
  0b00111111,0b11000000,0b00100000,0b01000000,
  0b00101111,0b11000000,0b00101000,0b01000000,
  0b00101111,0b11000000,0b00101000,0b01000000,
  0b00101111,0b11000000,0b00100000,0b01000000,
  0b00111111,0b11000000,0b00000011,0b10000000,
  0b00000001,0b10000000,0b00000001,0b10000000,
  0b00000001,0b10000000,0b00000011,0b10000000,
  0b00111111,0b11000000,0b00111111,0b11000000
};

const unsigned char glowIcon[32] PROGMEM = { /* same as before */ 
  0b00000111,0b00000000,0b00001111,0b10000000,
  0b00011111,0b11000000,0b00111111,0b11100000,
  0b00111111,0b11110000,0b00111111,0b11110000,
  0b00011111,0b11100000,0b00001111,0b10000000,
  0b00000111,0b00000000,0b00001111,0b10000000,
  0b00011111,0b11000000,0b00111111,0b11100000,
  0b00111111,0b11110000,0b00111111,0b11110000,
  0b00011111,0b11100000,0b00001111,0b10000000
};

// -------------------------------------------------------------------
// Conversion functions
int adcToCoolantC(int adc) {
  return constrain(map(adc, coolantADCMin, coolantADCMax, coolantCMin, coolantCMax),
                   coolantCMin, coolantCMax);
}

int adcToFuelLiters(int adc) {
  return constrain(map(adc, fuelADCMin, fuelADCMax, fuelLitersMin, fuelLitersMax),
                   fuelLitersMin, fuelLitersMax);
}

// -------------------------------------------------------------------
// Flash utility
bool shouldFlash() {
  unsigned long now = millis();
  if(now - lastFlash > flashInterval){
    flashState = !flashState;
    lastFlash = now;
  }
  return flashState;
}

// -------------------------------------------------------------------
// Drawing functions
void drawBackground(bool warningMode, bool flash) {
  graphics.fillScreen((warningMode && flash) ? WHITE : DARKBLUE);
}

// -------------------------------------------------------------------
// Metric-specific functions
void handleOilStatus(bool flash) {
  bool oilCritical = (digitalRead(oilPin) == HIGH);
  uint16_t iconColor = oilCritical ? 1 : 5;
  uint16_t textColor = (oilCritical && flash) ? BLACK : WHITE;

  graphics.drawBitmap(0,10,oilIcon,16,16,iconColor);
  if(oilCritical){
    graphics.setCursor(20,12);
    graphics.setHue(textColor);
    graphics.print("LOW PRESSURE");
  }
}

void handleCoolantTemp(bool flash) {
  int coolantADC = analogRead(coolantPin);
  int coolantC = adcToCoolantC(coolantADC);
  bool coolantCritical = (coolantC > coolantCriticalC);

  uint16_t textColor = (coolantCritical && flash) ? BLACK : WHITE;
  uint16_t hue;

  if(coolantC < coolantNormalMin) hue = 30; // orange
  else if(coolantC <= coolantCriticalC) hue = map(coolantC, coolantNormalMin, coolantCriticalC, 120, 0); // green→red
  else hue = 0; // red

  graphics.drawBitmap(0,30,tempIcon,16,16,hue);
  int barWidth = map(coolantC, coolantCMin, coolantCMax, 0,40);
  graphics.drawRect(20,30,42,10,0);
  graphics.fillRect(21,31,barWidth,8,hue);

  graphics.setCursor(70,30);
  graphics.setHue(textColor);
  graphics.print(String(coolantC)+"C");
}

void handleFuelLevel(bool flash) {
  int fuelADC = analogRead(fuelPin);
  int fuelLiters = adcToFuelLiters(fuelADC);
  bool fuelCritical = (fuelLiters <= fuelCriticalLiters);
  uint16_t textColor = (fuelCritical && flash) ? BLACK : WHITE;
  uint16_t hue = fuelCritical ? 0 : map(fuelLiters, fuelCriticalLiters, fuelLitersMax, 30, 120);

  graphics.drawBitmap(0,50,fuelIcon,16,16,hue);
  int barWidth = map(fuelLiters, fuelLitersMin, fuelLitersMax, 0,40);
  graphics.drawRect(20,50,42,10,0);
  graphics.fillRect(21,51,barWidth,8,hue);

  graphics.setCursor(70,50);
  graphics.setHue(textColor);
  graphics.print(String(fuelLiters)+"L");
}

// -------------------------------------------------------------------
// Glow plug handling
void drawGlowScreen(int remainingSeconds) {
  graphics.fillScreen(WHITE);
  graphics.setHue(BLACK);
  graphics.setCursor(50,40);
  graphics.print(remainingSeconds);
  graphics.drawBitmap(110,0,glowIcon,16,16,1);
}

void handleGlowPlug() {
  static bool glowActive = false;
  static unsigned long glowStartTime = 0;
  static int glowDuration = 0;

  // Read coolant for glow duration
  int coolantADC = analogRead(coolantPin);
  int coolantC = adcToCoolantC(coolantADC);

  if(!glowActive && digitalRead(glowButtonPin) == LOW){
    glowActive = true;
    glowDuration = map(coolantC, glowTempMin, glowTempMax, glowMaxTime, glowMinTime);
    glowDuration = constrain(glowDuration, glowMinTime, glowMaxTime);
    glowStartTime = millis();
    digitalWrite(glowPin, HIGH);
  }

  if(glowActive){
    int elapsed = (millis() - glowStartTime)/1000;
    int remaining = glowDuration - elapsed;
    if(remaining <= 0){
      glowActive = false;
      digitalWrite(glowPin, LOW);
    } else {
      drawGlowScreen(remaining);
      delay(50);
    }
  }
}

// -------------------------------------------------------------------
// Setup & loop
void setup() {
  pinMode(oilPin, INPUT_PULLUP);
  pinMode(coolantPin, INPUT);
  pinMode(fuelPin, INPUT);
  pinMode(glowButtonPin, INPUT_PULLUP);
  pinMode(glowPin, OUTPUT);
  digitalWrite(glowPin, LOW);

  graphics.begin();
  graphics.setFont(0);
}

void loop() {
  bool flash = shouldFlash();

  handleGlowPlug();

  if(digitalRead(glowPin) == LOW){ // normal gauges
    drawBackground(flash);
    handleOilStatus(flash);
    handleCoolantTemp(flash);
    handleFuelLevel(flash);
  }

  delay(50);
}
