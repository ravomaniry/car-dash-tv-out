/*
  ===================================================================
   ESP32 Color Composite Dashboard with Smooth Gradient
   ----------------------------------------------------
   Features:
   - Reads oil pressure switch, coolant temp, fuel level
   - Displays pixel-art icons + gauges on PAL composite TV
   - Coolant temp: orange < normal, green normal, red > critical
     with smooth gradient
   - Fuel: red → yellow → green smooth gradient depending on liters
   - Oil: icon green if OK, red if LOW PRESSURE (text shows only when critical)
   - Background flashes white when any sensor critical
   - Text contrasts with background
  ===================================================================
*/

#include <CompositeGraphics.h>
#include <CompositeVideo.h>
#include <Arduino.h>

// --- Video setup ---
CompositeGraphics graphics(CompositeVideo::PAL, 128, 96);

// --- Pins ---
const int oilPin     = 2;
const int coolantPin = 32;
const int fuelPin    = 33;

// --- Calibration ---
const int coolantADCMin = 100;
const int coolantADCMax = 900;
const int coolantCMin = 0;
const int coolantCMax = 120;
const int coolantCriticalC = 100;
const int coolantNormalMin = 70; // normal operating temp

const int fuelADCMin = 80;
const int fuelADCMax = 900;
const int fuelLitersMin = 0;
const int fuelLitersMax = 50;
const int fuelCriticalLiters = 5;

// --- Flash ---
unsigned long lastFlash = 0;
bool flashState = true;
const unsigned long flashInterval = 500;

// --- Colors ---
uint16_t DARKBLUE = 10; // normal background
uint16_t WHITE     = 40; // flash
uint16_t BLACK     = 0;

// -------------------------------------------------------------------
// Pixel-art icons (16x16)
const unsigned char oilIcon[32] PROGMEM = {
  0b00001111,0b00000000,0b00011111,0b10000000,
  0b00111111,0b11000000,0b00110011,0b11000000,
  0b00111111,0b11000000,0b00011111,0b10000000,
  0b00001111,0b00000000,0b00000110,0b00000000,
  0b00000110,0b00000000,0b00001111,0b00000000,
  0b00011111,0b10000000,0b00111111,0b11000000,
  0b00110011,0b11000000,0b00111111,0b11000000,
  0b00011111,0b10000000,0b00001111,0b00000000
};

const unsigned char tempIcon[32] PROGMEM = {
  0b00000110,0b00000000,0b00000110,0b00000000,
  0b00000110,0b00000000,0b00000110,0b00000000,
  0b00000110,0b00000000,0b00000110,0b00000000,
  0b00000110,0b00000000,0b00000110,0b00000000,
  0b00001111,0b00000000,0b00011111,0b10000000,
  0b00111111,0b11000000,0b00111111,0b11000000,
  0b00011111,0b10000000,0b00001111,0b00000000,
  0b00000110,0b00000000,0b00000110,0b00000000
};

const unsigned char fuelIcon[32] PROGMEM = {
  0b00111111,0b11000000,0b00100000,0b01000000,
  0b00101111,0b11000000,0b00101000,0b01000000,
  0b00101111,0b11000000,0b00101000,0b01000000,
  0b00101111,0b11000000,0b00100000,0b01000000,
  0b00111111,0b11000000,0b00000011,0b10000000,
  0b00000001,0b10000000,0b00000001,0b10000000,
  0b00000001,0b10000000,0b00000011,0b10000000,
  0b00111111,0b11000000,0b00111111,0b11000000
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

void drawOilStatus(bool oilCritical, bool warningMode, bool flash) {
  uint16_t iconColor = oilCritical ? 1 : 5; // red if critical, green if OK
  uint16_t textColor = (warningMode && flash) ? BLACK : WHITE;

  graphics.drawBitmap(0,10,oilIcon,16,16,iconColor);
  if(oilCritical){
    graphics.setCursor(20,12);
    graphics.setHue(textColor);
    graphics.print("LOW PRESSURE");
  }
}

void drawCoolantGauge(int tempC, bool warningMode, bool flash){
  uint16_t textColor = (warningMode && flash) ? BLACK : WHITE;
  uint16_t hue;

  // Smooth gradient logic
  if(tempC < coolantNormalMin) hue = 30; // orange
  else if(tempC <= coolantCriticalC) hue = map(tempC, coolantNormalMin, coolantCriticalC, 120, 0); // green→red
  else hue = 0; // red

  graphics.drawBitmap(0,30,tempIcon,16,16,hue);

  int barWidth = map(tempC, coolantCMin, coolantCMax, 0,40);
  graphics.drawRect(20,30,42,10,0);
  graphics.fillRect(21,31,barWidth,8,hue);

  graphics.setCursor(70,30);
  graphics.setHue(textColor);
  graphics.print(String(tempC)+"C");
}

void drawFuelGauge(int fuelLiters, bool warningMode, bool flash){
  uint16_t textColor = (warningMode && flash) ? BLACK : WHITE;
  uint16_t hue;

  if(fuelLiters <= fuelCriticalLiters) hue = 0; // red
  else hue = map(fuelLiters, fuelCriticalLiters, fuelLitersMax, 30, 120); // yellow→green

  graphics.drawBitmap(0,50,fuelIcon,16,16,hue);

  int barWidth = map(fuelLiters, fuelLitersMin, fuelLitersMax, 0,40);
  graphics.drawRect(20,50,42,10,0);
  graphics.fillRect(21,51,barWidth,8,hue);

  graphics.setCursor(70,50);
  graphics.setHue(textColor);
  graphics.print(String(fuelLiters)+"L");
}

// -------------------------------------------------------------------
// Setup & loop
void setup() {
  pinMode(oilPin, INPUT_PULLUP);
  graphics.begin();
  graphics.setFont(0);
}

void loop() {
  bool flash = shouldFlash();

  // Read sensors
  bool oilCritical = (digitalRead(oilPin) == HIGH);
  int coolantADC = analogRead(coolantPin);
  int fuelADC = analogRead(fuelPin);

  int coolantC = adcToCoolantC(coolantADC);
  int fuelLiters = adcToFuelLiters(fuelADC);

  bool coolantCritical = (coolantC > coolantCriticalC);
  bool fuelCritical = (fuelLiters <= fuelCriticalLiters);
  bool warningMode = oilCritical || coolantCritical || fuelCritical;

  // Draw
  drawBackground(warningMode, flash);
  drawOilStatus(oilCritical, warningMode, flash);
  drawCoolantGauge(coolantC, warningMode, flash);
  drawFuelGauge(fuelLiters, warningMode, flash);

  delay(50);
}
