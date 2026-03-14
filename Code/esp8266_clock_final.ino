#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <time.h>

// Pin definitions
#define PIN_CLK 14      // D5 - SCK
#define PIN_DATA 13     // D7 - MOSI
#define PIN_CS 15       // D8 - CS
#define PIN_DC 2        // D4 - DC
#define PIN_RST 0       // D3 - RST

#define DHTPIN 12       // D6 - DHT11
#define BUTTON_PIN 5    // D1 - Button (J5 Pin 1)
#define BUZZER_PIN 4    // D2 - Buzzer

#define DHTTYPE DHT11

// External 1kΩ pull-up to 3.3V on J5 Pin 1
// Rest = HIGH, Pressed = LOW
#define BTN_PRESSED  LOW
#define BTN_RELEASED HIGH

// Display setup
U8G2_SH1107_PIMORONI_128X128_F_4W_SW_SPI u8g2(U8G2_R0, PIN_CLK, PIN_DATA, PIN_CS, PIN_DC, PIN_RST);

// Objects
DHT dht(DHTPIN, DHTTYPE);
WiFiManager wifiManager;

// Display modes
enum DisplayMode {
  MODE_CLOCK,
  MODE_SENSORS,
  MODE_ALARM_MENU,
  MODE_SET_HOUR,
  MODE_SET_MINUTE
};

// State
DisplayMode currentMode = MODE_CLOCK;
float temperature = 0;
float humidity = 0;
bool useCelsius = true;
bool show24Hour = true;

// Alarm settings
bool alarmEnabled = false;
int alarmHour = 7;
int alarmMinute = 0;
bool alarmTriggered = false;
int tempHour = 7;
int tempMinute = 0;

// Time
const char* ntpServer = "pool.ntp.org";
long gmtOffset_sec = 19800;
int daylightOffset_sec = 0;

// Forward declarations
void showMessage(const char* line1, const char* line2 = "", const char* line3 = "");
void updateDisplay();
void readSensors();
void printTime();
void checkAlarm();
void playAlarmSound();
void playBeep(int frequency, int duration);
void handleButton();
void handleShortPress();
void handleLongPress();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n================================"));
  Serial.println(F("ESP8266 WiFi Clock + Alarm"));
  Serial.println(F("External pull-up button build"));
  Serial.println(F("================================\n"));

  // External 1kΩ pull-up handles biasing, just use INPUT
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println(F("Button: D1 (J5), Buzzer: D2"));

  // Initialize OLED
  u8g2.begin();
  Serial.println(F("OLED: OK"));

  playBeep(1000, 100);

  showMessage("WiFi Clock", "Starting...");
  delay(1500);

  // WiFi Setup
  showMessage("WiFi Setup", "Connect to:", "ClockSetup");
  Serial.println(F("WiFi setup starting..."));

  WiFiManagerParameter custom_timezone("tz", "Timezone", "5.5", 6,
    "<br/>5.5 (Sri Lanka/India), 0 (UK), -5 (USA EST)");

  wifiManager.addParameter(&custom_timezone);
  wifiManager.setConfigPortalTimeout(180);

  if (!wifiManager.autoConnect("ClockSetup")) {
    Serial.println(F("Failed, restarting..."));
    showMessage("WiFi Failed", "Restarting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println(F("\nWiFi Connected!"));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());

  String tzValue = custom_timezone.getValue();
  float tzHours = tzValue.toFloat();
  gmtOffset_sec = (long)(tzHours * 3600);

  Serial.print(F("Timezone: UTC"));
  if (tzHours >= 0) Serial.print(F("+"));
  Serial.println(tzHours);

  showMessage("WiFi OK!", WiFi.SSID().c_str(), "Syncing...");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  Serial.print(F("Syncing"));
  int attempts = 0;
  while (time(nullptr) < 100000 && attempts < 20) {
    Serial.print(F("."));
    delay(500);
    attempts++;
  }
  Serial.println();

  if (time(nullptr) < 100000) {
    Serial.println(F("Time sync failed!"));
    showMessage("Time Sync", "Failed!");
    delay(2000);
  } else {
    Serial.println(F("Time synced!"));
    printTime();
    showMessage("Time Synced!", "Ready!");
    playBeep(1500, 100);
    delay(1500);
  }

  dht.begin();
  Serial.println(F("DHT11: OK"));
  delay(2000);
  readSensors();

  Serial.println(F("\n=== CONTROLS ==="));
  Serial.println(F("Short press : Next screen"));
  Serial.println(F("Long press  : Alarm ON/OFF (Clock/Sensors)"));
  Serial.println(F("Long press  : Set time (Alarm Menu)"));
  Serial.println(F("================\n"));
}

void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastTimeSync = 0;

  handleButton();

  if (alarmEnabled && !alarmTriggered &&
      currentMode != MODE_SET_HOUR && currentMode != MODE_SET_MINUTE) {
    checkAlarm();
  }

  if (alarmTriggered) {
    playAlarmSound();
  }

  if (millis() - lastTimeSync >= 3600000) {
    lastTimeSync = millis();
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println(F("Time re-synced"));
  }

  readSensors();

  if (millis() - lastUpdate >= 500) {
    lastUpdate = millis();
    updateDisplay();
  }

  delay(10);
}

// -------------------------------------------------------
// BUTTON HANDLER
// Rest = HIGH, Pressed = LOW (external 1kΩ pull-up)
// -------------------------------------------------------
void handleButton() {
  static unsigned long lastButtonCheck = 0;
  static bool lastButtonState = BTN_RELEASED;  // HIGH at rest
  static unsigned long buttonPressStart = 0;

  if (millis() - lastButtonCheck < 50) return;
  lastButtonCheck = millis();

  bool buttonState = digitalRead(BUTTON_PIN);

  // Button just pressed (HIGH -> LOW)
  if (buttonState == BTN_PRESSED && lastButtonState == BTN_RELEASED) {
    buttonPressStart = millis();
    delay(50);  // debounce
  }

  // Button just released (LOW -> HIGH)
  if (buttonState == BTN_RELEASED && lastButtonState == BTN_PRESSED) {
    unsigned long pressDuration = millis() - buttonPressStart;

    if (pressDuration < 2000) {
      handleShortPress();
    } else {
      handleLongPress();
    }

    delay(100);  // prevent double-trigger
  }

  lastButtonState = buttonState;
}

void handleShortPress() {
  playBeep(800, 50);
  Serial.println(F("SHORT PRESS"));

  switch (currentMode) {
    case MODE_CLOCK:
      if (alarmTriggered) {
        alarmTriggered = false;
        noTone(BUZZER_PIN);
        digitalWrite(BUZZER_PIN, LOW);
        Serial.println(F("Alarm stopped"));
      } else {
        currentMode = MODE_SENSORS;
        Serial.println(F("-> SENSORS"));
      }
      break;

    case MODE_SENSORS:
      currentMode = MODE_ALARM_MENU;
      Serial.println(F("-> ALARM MENU"));
      break;

    case MODE_ALARM_MENU:
      currentMode = MODE_CLOCK;
      Serial.println(F("-> CLOCK"));
      break;

    case MODE_SET_HOUR:
      tempHour++;
      if (tempHour > 23) tempHour = 0;
      Serial.print(F("Hour: "));
      Serial.println(tempHour);
      break;

    case MODE_SET_MINUTE:
      tempMinute++;
      if (tempMinute > 59) tempMinute = 0;
      Serial.print(F("Minute: "));
      Serial.println(tempMinute);
      break;
  }
}

void handleLongPress() {
  playBeep(1200, 100);
  Serial.println(F("LONG PRESS"));

  switch (currentMode) {
    case MODE_CLOCK:
    case MODE_SENSORS:
      alarmEnabled = !alarmEnabled;
      Serial.print(F("Alarm: "));
      Serial.println(alarmEnabled ? F("ON") : F("OFF"));
      {
        char msg[20];
        sprintf(msg, "Alarm %s", alarmEnabled ? "ON" : "OFF");
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d", alarmHour, alarmMinute);
        showMessage(msg, timeStr);
      }
      playBeep(alarmEnabled ? 1500 : 800, 150);
      delay(1500);
      break;

    case MODE_ALARM_MENU:
      tempHour = alarmHour;
      tempMinute = alarmMinute;
      currentMode = MODE_SET_HOUR;
      Serial.println(F("-> SET HOUR"));
      break;

    case MODE_SET_HOUR:
      currentMode = MODE_SET_MINUTE;
      Serial.println(F("-> SET MINUTE"));
      playBeep(1000, 50);
      delay(50);
      playBeep(1200, 50);
      break;

    case MODE_SET_MINUTE:
      alarmHour = tempHour;
      alarmMinute = tempMinute;
      alarmEnabled = true;
      currentMode = MODE_ALARM_MENU;
      Serial.print(F("Alarm saved: "));
      Serial.print(alarmHour);
      Serial.print(F(":"));
      Serial.println(alarmMinute);
      showMessage("Alarm Set!", "Enabled");
      playBeep(1500, 100);
      delay(100);
      playBeep(1500, 100);
      delay(1000);
      break;
  }
}

// -------------------------------------------------------
// ALARM
// -------------------------------------------------------
void checkAlarm() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  if (timeinfo->tm_hour == alarmHour &&
      timeinfo->tm_min  == alarmMinute &&
      timeinfo->tm_sec  == 0) {
    alarmTriggered = true;
    Serial.println(F("ALARM TRIGGERED!"));
  }
}

void playAlarmSound() {
  static unsigned long lastBeep = 0;
  static bool beepState = false;

  if (millis() - lastBeep >= 500) {
    lastBeep = millis();
    beepState = !beepState;
    tone(BUZZER_PIN, beepState ? 2000 : 1000);
  }
}

void playBeep(int frequency, int duration) {
  tone(BUZZER_PIN, frequency);
  delay(duration);
  noTone(BUZZER_PIN);
}

// -------------------------------------------------------
// DISPLAY
// -------------------------------------------------------
void updateDisplay() {
  u8g2.clearBuffer();

  switch (currentMode) {
    case MODE_CLOCK:      drawClock();      break;
    case MODE_SENSORS:    drawSensors();    break;
    case MODE_ALARM_MENU: drawAlarmMenu();  break;
    case MODE_SET_HOUR:   drawSetHour();    break;
    case MODE_SET_MINUTE: drawSetMinute();  break;
  }

  u8g2.sendBuffer();
}

void drawClock() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  int hour    = timeinfo->tm_hour;
  int minute  = timeinfo->tm_min;
  int day     = timeinfo->tm_mday;
  int month   = timeinfo->tm_mon + 1;
  int year    = timeinfo->tm_year + 1900;
  int weekday = timeinfo->tm_wday;
  bool isPM   = false;

  if (!show24Hour) {
    isPM = (hour >= 12);
    if (hour == 0)      hour = 12;
    else if (hour > 12) hour -= 12;
  }

  u8g2.setFont(u8g2_font_logisoso24_tn);
  char timeStr[10];
  sprintf(timeStr, "%02d:%02d", hour, minute);
  int w = u8g2.getStrWidth(timeStr);
  u8g2.drawStr(64 - w / 2, 35, timeStr);

  if (!show24Hour) {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5, 35, isPM ? "PM" : "AM");
  }

  if (WiFi.status() == WL_CONNECTED) {
    u8g2.setFont(u8g2_font_micro_tr);
    u8g2.drawStr(120, 8, "W");
  }

  if (alarmEnabled) {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5, 10, "A");
    u8g2.setFont(u8g2_font_micro_tr);
    char alarmStr[6];
    sprintf(alarmStr, "%02d:%02d", alarmHour, alarmMinute);
    u8g2.drawStr(2, 18, alarmStr);
  }

  u8g2.setFont(u8g2_font_ncenB10_tr);
  char dateStr[20];
  sprintf(dateStr, "%02d/%02d/%04d", day, month, year);
  w = u8g2.getStrWidth(dateStr);
  u8g2.drawStr(64 - w / 2, 55, dateStr);

  u8g2.setFont(u8g2_font_ncenB08_tr);
  const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  w = u8g2.getStrWidth(days[weekday]);
  u8g2.drawStr(64 - w / 2, 70, days[weekday]);

  u8g2.setFont(u8g2_font_micro_tr);
  u8g2.drawStr(15, 120, alarmEnabled ? "Hold: Alarm OFF" : "Hold: Alarm ON");
}

void drawSensors() {
  u8g2.setFont(u8g2_font_ncenB10_tr);
  int w = u8g2.getStrWidth("ROOM DATA");
  u8g2.drawStr(64 - w / 2, 20, "ROOM DATA");

  u8g2.setFont(u8g2_font_logisoso16_tn);
  char tempStr[10];
  sprintf(tempStr, "%.1f", temperature);
  int tempW = u8g2.getStrWidth(tempStr);
  int tempX = 64 - tempW / 2 - 12;
  u8g2.drawStr(tempX, 45, tempStr);
  u8g2.drawCircle(tempX + tempW + 3, 35, 2);

  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(tempX + tempW + 8, 45, useCelsius ? "C" : "F");

  u8g2.setFont(u8g2_font_ncenB08_tr);
  w = u8g2.getStrWidth("Temperature");
  u8g2.drawStr(64 - w / 2, 60, "Temperature");

  u8g2.setFont(u8g2_font_ncenB12_tr);
  char humStr[10];
  sprintf(humStr, "%.0f%%", humidity);
  w = u8g2.getStrWidth(humStr);
  u8g2.drawStr(64 - w / 2, 80, humStr);

  u8g2.setFont(u8g2_font_6x10_tr);
  w = u8g2.getStrWidth("Humidity");
  u8g2.drawStr(64 - w / 2, 95, "Humidity");

  u8g2.setFont(u8g2_font_micro_tr);
  u8g2.drawStr(15, 120, alarmEnabled ? "Hold: Alarm OFF" : "Hold: Alarm ON");
}

void drawAlarmMenu() {
  u8g2.setFont(u8g2_font_ncenB14_tr);
  int w = u8g2.getStrWidth("ALARM");
  u8g2.drawStr(64 - w / 2, 25, "ALARM");

  u8g2.setFont(u8g2_font_ncenB18_tr);
  const char* status = alarmEnabled ? "ON" : "OFF";
  w = u8g2.getStrWidth(status);
  u8g2.drawStr(64 - w / 2, 50, status);

  u8g2.setFont(u8g2_font_logisoso16_tn);
  char timeStr[10];
  sprintf(timeStr, "%02d:%02d", alarmHour, alarmMinute);
  w = u8g2.getStrWidth(timeStr);
  u8g2.drawStr(64 - w / 2, 75, timeStr);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(8, 95,  "Press: Back to clock");
  u8g2.drawStr(8, 108, "Hold: Change time");
}

void drawSetHour() {
  u8g2.setFont(u8g2_font_ncenB10_tr);
  int w = u8g2.getStrWidth("SET HOUR");
  u8g2.drawStr(64 - w / 2, 25, "SET HOUR");

  u8g2.setFont(u8g2_font_logisoso32_tn);
  char hourStr[5];
  sprintf(hourStr, "%02d", tempHour);
  w = u8g2.getStrWidth(hourStr);
  u8g2.drawStr(64 - w / 2, 65, hourStr);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(25, 95,  "Press: +1");
  u8g2.drawStr(25, 110, "Hold: Next");
}

void drawSetMinute() {
  u8g2.setFont(u8g2_font_ncenB10_tr);
  int w = u8g2.getStrWidth("SET MINUTE");
  u8g2.drawStr(64 - w / 2, 25, "SET MINUTE");

  u8g2.setFont(u8g2_font_logisoso32_tn);
  char minStr[5];
  sprintf(minStr, "%02d", tempMinute);
  w = u8g2.getStrWidth(minStr);
  u8g2.drawStr(64 - w / 2, 65, minStr);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(25, 95,  "Press: +1");
  u8g2.drawStr(25, 110, "Hold: Save");
}

// -------------------------------------------------------
// SENSORS & UTILITIES
// -------------------------------------------------------
void readSensors() {
  static unsigned long lastRead = 0;

  if (millis() - lastRead > 2500) {
    float h = dht.readHumidity();
    float t = dht.readTemperature(!useCelsius);

    if (!isnan(h) && !isnan(t)) {
      temperature = t;
      humidity    = h;
    }
    lastRead = millis();
  }
}

void showMessage(const char* line1, const char* line2, const char* line3) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);

  int w1 = u8g2.getStrWidth(line1);
  u8g2.drawStr(64 - w1 / 2, 30, line1);

  if (strlen(line2) > 0) {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    int w2 = u8g2.getStrWidth(line2);
    u8g2.drawStr(64 - w2 / 2, 50, line2);
  }

  if (strlen(line3) > 0) {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    int w3 = u8g2.getStrWidth(line3);
    u8g2.drawStr(64 - w3 / 2, 65, line3);
  }

  u8g2.sendBuffer();
}

void printTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  Serial.print(F("Time: "));
  Serial.print(timeinfo->tm_year + 1900);
  Serial.print(F("/"));
  Serial.print(timeinfo->tm_mon + 1);
  Serial.print(F("/"));
  Serial.print(timeinfo->tm_mday);
  Serial.print(F(" "));
  Serial.print(timeinfo->tm_hour);
  Serial.print(F(":"));
  Serial.println(timeinfo->tm_min);
}

/*
 * ========================================
 * CONTROLS
 * ========================================
 * CLOCK / SENSORS:
 *   Short press --> Next screen
 *   Long press  --> Toggle alarm ON/OFF
 *
 * ALARM MENU:
 *   Short press --> Back to clock
 *   Long press  --> Change alarm time
 *
 * SETTING HOUR / MINUTE:
 *   Short press --> +1
 *   Long press  --> Confirm / Save
 *
 * ALARM RINGING:
 *   Short press --> Stop alarm
 * ========================================
 */
