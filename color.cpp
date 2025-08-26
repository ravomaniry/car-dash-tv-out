/*
  ===================================================================
   ESP32 Color Composite Dashboard (Function-Based + Icons)
   --------------------------------------------------------
   Reads:
     - Oil pressure switch (digital, HIGH = critical, LOW = OK)
     - Coolant temperature (analog sender on ADC1)
     - Fuel level (analog sender on ADC1)

   Displays:
     - Text + gauges on a PAL composite TV in COLOR
     - Icons next to each reading (oil can, thermometer, fuel pump)
     - Gauges transition from green (OK) → red (critical)
     - Screen flashes red if any parameter is critical

   ----------------------
   HARDWARE CONNECTIONS
   ----------------------
   ESP32 Video out:
     - GPIO25 (DAC1) → 75Ω resistor → RCA video IN (TV)
     - GND → RCA ground

   Sensors:
     - Oil pressure switch → GPIO2
       (connect switch between GPIO2 and GND, use INPUT_PULLUP)
     - Coolant temp sender → GPIO32 (ADC1_CH4)
     - Fuel level sender   → GPIO33 (ADC1_CH5)

   ----------------------
   LIBRARIES TO INSTALL
   ----------------------
   - ESP32CompositeColorVideo
     (https://github.com/marciot/ESP32CompositeColorVideo)

   ===================================================================
*/

#include <CompositeGraphics.h>
#include <CompositeVideo.h>
#include <Arduino.h>

// --- Video setup (PAL, 128x96 pixels) ---
CompositeGraphics graphics(CompositeVideo::PAL, 128, 96);

// --- Sensor pins ---
const int oilPin     = 2;
const int coolantPin = 32;
const int fuelPin    = 33;

// --- Calibration values (tune for your sensors) ---
const int coolantADCMin = 100;
const int coolantADCMax = 900;
const int coolantCMin = 0;
const int coolantCMax = 120;
const int coolantCriticalC = 100;

const int fuelADCMin = 80;
const int fuelADCMax = 900;
const int fuelLitersMin = 0;
const int fuelLitersMax = 50;
const int fuelCriticalLiters = 5;

// --- Flash settings ---
unsigned long lastFlash = 0;
bool flashState = true;
const unsigned long flashInterval = 500; // ms

// -------------------------------------------------------------------
// Icon bitmaps (8x8 pixels, monochrome)
// -------------------------------------------------------------------
const unsigned char oilIcon [] PROGMEM = {
  0b00011000,
  0b00111100,
  0b01111110,
  0b00111100,
  0b01111110,
  0b00111100,
  0b00011000,
  0b00000000
};

const unsigned char tempIcon [] PROGMEM = {
  0b00011000,
  0b00011000,
  0b00011000,
  0b00011000,
  0b00111100,
  0b00111100,
  0b00011000,
  0b00000000
};

const unsigned char fuelIcon [] PROGMEM = {
  0b00111100,
  0b01111110,
  0b01000010,
  0b01011010,
  0b01011010,
  0b01000010,
  0b01111110,
  0b00111100
};

// -------------------------------------------------------------------
// Conversion functions
// -------------------------------------------------------------------
int adcToCoolantC(int adc) {
  return constrain(
    map(adc, coolantADCMin, coolantADCMax, coolantCMin, coolantCMax),
    coolantCMin, coolantCMax
  );
}

int adcToFuelLiters(int adc) {
  return constrain(
    map(adc, fuelADCMin, fuelADCMax, fuelLitersMin, fuelLitersMax),
    fuelLitersMin, fuelLitersMax
  );
}

// -------------------------------------------------------------------
// Utility
// -------------------------------------------------------------------
bool shouldFlash() {
  unsigned long now = millis();
  if (now - lastFlash > flashInterval) {
    flashState = !flashState;
    lastFlash = now;
  }
  return flashState;
}

// -------------------------------------------------------------------
// Drawing functions
// -------------------------------------------------------------------
void drawBackground(bool warningMode, bool flash) {
  if (warningMode && flash) {
    graphics.setHue(1); // red
    graphics.fillScreen(40);
  } else {
    graphics.setHue(0); // grayscale background
    graphics.fillScreen(0);
  }
}

void drawOilStatus(bool oilCritical) {
  graphics.setHue(0);
  graphics.drawBitmap(0, 10, oilIcon, 8, 8, 40);

  graphics.setCursor(15, 10);
  graphics.setHue(oilCritical ? 1 : 5); // red if critical, green if ok
  graphics.print(oilCritical ? "OIL WARN" : "OIL OK");
}

void drawCoolantGauge(int coolantC) {
  graphics.setHue(0);
  graphics.drawBitmap(0, 30, tempIcon, 8, 8, 40);

  int barWidth = map(coolantC, coolantCMin, coolantCMax, 0, 40);
  int hue = map(coolantC, coolantCMin, coolantCMax, 5, 1); // green→red

  graphics.setHue(0);
  graphics.drawRect(20, 30, 42, 10, 40);

  graphics.setHue(hue);
  graphics.fillRect(21, 31, barWidth, 8, 40);

  graphics.setCursor(70, 30);
  graphics.setHue(0);
  graphics.print(String(coolantC) + "C");
}

void drawFuelGauge(int fuelLiters) {
  graphics.setHue(0);
  graphics.drawBitmap(0, 50, fuelIcon, 8, 8, 40);

  int barWidth = map(fuelLiters, fuelLitersMin, fuelLitersMax, 0, 40);
  int hue = map(fuelLiters, fuelLitersMin, fuelLitersMax, 5, 1); // green→red

  graphics.setHue(0);
  graphics.drawRect(20, 50, 42, 10, 40);

  graphics.setHue(hue);
  graphics.fillRect(21, 51, barWidth, 8, 40);

  graphics.setCursor(70, 50);
  graphics.setHue(0);
  graphics.print(String(fuelLiters) + "L");
}

// -------------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------------
void setup() {
  pinMode(oilPin, INPUT_PULLUP);
  graphics.begin();
  graphics.setFont(0);
}

void loop() {
  bool flash = shouldFlash();

  // --- Read sensors ---
  bool oilCritical = (digitalRead(oilPin) == HIGH);
  int coolantADC = analogRead(coolantPin);
  int fuelADC = analogRead(fuelPin);

  int coolantC = adcToCoolantC(coolantADC);
  int fuelLiters = adcToFuelLiters(fuelADC);

  bool coolantCritical = (coolantC >= coolantCriticalC);
  bool fuelCritical = (fuelLiters <= fuelCriticalLiters);
  bool warningMode = oilCritical || coolantCritical || fuelCritical;

  // --- Draw screen ---
  drawBackground(warningMode, flash);
  drawOilStatus(oilCritical);
  drawCoolantGauge(coolantC);
  drawFuelGauge(fuelLiters);

  delay(50);
}
