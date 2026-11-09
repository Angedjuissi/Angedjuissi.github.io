// Rekonstruktion des Arduino-Projekts für das HTW-MySignals-Kit (Mega 2560).
// Quellen:
// https://moodle.htw-berlin.de/mod/page/view.php?id=1343997
// https://moodle.htw-berlin.de/mod/resource/view.php?id=1352898 (TftGui-Beispiel, Heiko Huebert)
// https://moodle.htw-berlin.de/mod/resource/view.php?id=1343958 (MySignals-Sensorbeispiele)

#include <MySignals.h>
#include <MySignals_Extender.h>
#include <Adafruit_GFX_AS.h>
#include <Adafruit_ILI9341_AS.h>
#include <UTouch.h>
#include <TftGui.h>
#include <Wire.h>
#include <math.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

Adafruit_ILI9341_AS tft(TFT_CS, TFT_DC);
UTouch touchDevice(TOUCH_CLK, TOUCH_CS, TOUCH_DIN, TOUCH_DOUT, TOUCH_IRQ);
guiTouch touch(touchDevice, 300, 1, 0, 0);
TftGui gui(tft, touch);

// A0 ist im MySignals-Aufbau für den Touch-Controller belegt.
const uint8_t pulseSensorPin = A8;
volatile bool pulseSampleDue = false;
float latestPulse = 0;
bool ledEnabled = false;
char normalLabel[] = "NORM";
char highLabel[] = "HIGH";
char unknownLabel[] = "?";

void setupScreen() {}
void updateHome();
void updatePulse();
void updateTemperature();
void openPulse();
void openTemperature();
void openHome();
void toggleLed();

guiScreen home("Display 1", 500, setupScreen, updateHome);
guiScreen pulse("Herzschlag", 10, setupScreen, updatePulse);
guiScreen temperature("Display 2", 100, setupScreen, updateTemperature);
guiStatusBar homeBar("Display 1", 0, 0);
guiStatusBar pulseBar("Herzschlag", 0, 0);
guiStatusBar temperatureBar("Display 2", 0, 0);
guiButton pulseButton("Screen3", 20, 180, openPulse);
guiButton temperatureButton("Screen2", 20, 100, openTemperature);
guiButton ledButton("LED ON/OFF", 20, 30, toggleLed);
guiSmallButton pulseBack("back", 180, 2, openHome);
guiSmallButton temperatureBack("back", 260, 2, openHome);
guiTextBox temperatureBox("Temp", 200, 50);
guiIndicator statusIndicator("LED", 250, 175);
guiDiagramFloat pulseGraph("KY-039 ADC", 0, 30, 0.0f, 1023.0f, true);
guiDiagramFloat temperatureGraph("Temp C", 0, 30, 20.0f, 30.0f, true);

// Der Interrupt setzt nur ein Flag. SPI und GUI bleiben in loop().
ISR(TIMER3_COMPA_vect) {
  pulseSampleDue = true;
}

void startPulseTimer() {
  // 100 Hz bei 16 MHz und einem Vorteiler von 64.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    TCCR3B = 0;
    TCCR3A = 0;
    TCNT3 = 0;
    OCR3A = F_CPU / 64UL / 100UL - 1;
    TIFR3 = _BV(OCF3A);
    TIMSK3 |= _BV(OCIE3A);
    TCCR3B = _BV(WGM32) | _BV(CS31) | _BV(CS30);
  }
}

void setup() {
  MySignals.begin();
  bitSet(MySignals.expanderState, EXP_TOUCH_CS);
  MySignals.expanderWrite(MySignals.expanderState);
  Wire.begin();
  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, LOW);

  home.add(homeBar);
  home.add(pulseButton);
  home.add(temperatureButton);
  home.add(ledButton);
  home.add(temperatureBox);
  home.add(statusIndicator);
  pulse.add(pulseBar);
  pulse.add(pulseBack);
  pulse.add(pulseGraph);
  temperature.add(temperatureBar);
  temperature.add(temperatureBack);
  temperature.add(temperatureGraph);
  gui.init(home);
  startPulseTimer();
}

void loop() {
  bool readPulse;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    readPulse = pulseSampleDue;
    pulseSampleDue = false;
  }
  if (readPulse) {
    latestPulse = analogRead(pulseSensorPin);
  }
  gui.run();
}

void updateHome() {
  const float degrees = MySignals.getTemperature();
  temperatureBox.updateLabel(String(degrees, 1));
  // Nur die zwei im Video eindeutig belegten Anzeigewerte nachbilden.
  const long tenths = lroundf(degrees * 10.0f);
  if (tenths == 181) {
    statusIndicator.update(ILI9341_GREEN, normalLabel, 2, ILI9341_BLACK);
  } else if (tenths == 284) {
    statusIndicator.update(ILI9341_RED, highLabel, 2, ILI9341_BLACK);
  } else {
    statusIndicator.update(ILI9341_YELLOW, unknownLabel, 2, ILI9341_BLACK);
  }
}

void updatePulse() {
  // Das ADC-Signal für die Kurve begrenzen.
  pulseGraph.updateValue(constrain(latestPulse, 0.0f, 1023.0f));
  // ADC-Zahl getrennt vom Diagramm und von der Zurück-Schaltfläche anzeigen.
  static unsigned long lastRefresh = 0;
  const unsigned long now = millis();
  const int value = static_cast<int>(latestPulse);
  if (now - lastRefresh >= 200) {
    tft.fillRect(260, 2, 60, 22, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.drawNumber(value, 265, 4, 2);
    lastRefresh = now;
  }
}

void updateTemperature() {
  temperatureGraph.updateValue(constrain(MySignals.getTemperature(), 20.0f, 30.0f));
}

void openHome() { gui.setScreen(home); }
void openPulse() { gui.setScreen(pulse); }
void openTemperature() {
  gui.setScreen(temperature);
}

void toggleLed() {
  ledEnabled = !ledEnabled;
  digitalWrite(LED1, ledEnabled ? HIGH : LOW);
  updateHome();
}
