#include <TVout.h>
#include <fontALL.h>

TVout TV;

// Pins and calibration
const int oilPin = 2;
const int coolantPin = A0;
const int fuelPin = A1;

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

// Flash settings
unsigned long lastFlash = 0;
bool flashState = true;
const unsigned long flashInterval = 500; // ms

// --- Helpers ---
int adcToCoolantC(int adc) {
  return constrain(map(adc, coolantADCMin, coolantADCMax, coolantCMin, coolantCMax), coolantCMin, coolantCMax);
}
int adcToFuelLiters(int adc) {
  return constrain(map(adc, fuelADCMin, fuelADCMax, fuelLitersMin, fuelLitersMax), fuelLitersMin, fuelLitersMax);
}
bool shouldFlash() {
  unsigned long now = millis();
  if (now - lastFlash > flashInterval) {
    flashState = !flashState;
    lastFlash = now;
  }
  return flashState;
}

// --- Drawing ---
void drawDegreeSymbol(int x, int y, bool color) {
  TV.set_pixel(x, y, color);
  TV.set_pixel(x+1, y, color);
  TV.set_pixel(x, y+1, color);
  TV.set_pixel(x+1, y+1, color);
}

void drawOilWarning(int oilState, bool flash, bool color) {
  bool critical = (oilState == HIGH);
  if (!(critical && !flash)) {
    if (critical) {
      TV.print(10, 10, "OIL WARN", color);
    } else {
      TV.print(10, 10, "OIL OK", color);
    }
  }
}

void drawCoolant(int tempC, bool flash, bool color) {
  bool critical = (tempC >= coolantCriticalC);
  int barWidth = map(tempC, coolantCMin, coolantCMax, 0, 40);
  if (!(critical && !flash)) {
    TV.print(0, 30, "TEMP", color);
    TV.draw_rect(40, 30, 42, 10, color);
    TV.fill_rect(41, 31, barWidth, 8, color);
    TV.print(90, 30, String(tempC).c_str(), color);
    drawDegreeSymbol(105, 30, color);
    TV.print(110, 30, "C", color);
  }
}

void drawFuel(int liters, bool flash, bool color) {
  bool critical = (liters <= fuelCriticalLiters);
  int barWidth = map(liters, fuelLitersMin, fuelLitersMax, 0, 40);
  if (!(critical && !flash)) {
    TV.print(0, 50, "FUEL", color);
    TV.draw_rect(40, 50, 42, 10, color);
    TV.fill_rect(41, 51, barWidth, 8, color);
    TV.print(90, 50, String(liters).c_str(), color);
    TV.print(110, 50, "L", color);
  }
}

void setup() {
  pinMode(oilPin, INPUT);
  TV.begin(PAL, 120, 96);
  TV.select_font(font4x6);
}

void loop() {
  bool flash = shouldFlash();
  
  int oilState = digitalRead(oilPin);
  int coolantADC = analogRead(coolantPin);
  int fuelADC = analogRead(fuelPin);
  int coolantC = adcToCoolantC(coolantADC);
  int fuelLiters = adcToFuelLiters(fuelADC);

  bool warningMode = (oilState == HIGH) || (coolantC >= coolantCriticalC) || (fuelLiters <= fuelCriticalLiters);

  if (warningMode) {
    // Fill screen ON (bright)
    TV.fill(1);
  } else {
    // Fill screen OFF (dark)
    TV.fill(0);
  }

  // Decide text color so it's always white against background:
  // White on black: color=1
  // White on bright: invert logic so text stands out, color=0
  bool textColor = warningMode ? 0 : 1;

  drawOilWarning(oilState, flash, textColor);
  drawCoolant(coolantC, flash, textColor);
  drawFuel(fuelLiters, flash, textColor);

  delay(50);
}
