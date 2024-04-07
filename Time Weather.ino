#include <Arduino.h>
#include <ESPWiFi.h>
#include <ESPHTTPClient.h>
#include <JsonListener.h> 
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "OpenWeatherMapCurrent.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "DHT.h"

const char* WIFI_SSID = "####";
const char* WIFI_PWD = "####";

#define TZ              -4       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries

// Setup
const int UPDATE_INTERVAL_SECS = 10 * 60; // Update every 30 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D5;
const int SDC_PIN = D6;
#else
const int SDA_PIN = 5; //D3;
const int SDC_PIN = 4; //D4;
#endif

String OPEN_WEATHER_MAP_APP_ID = "####";
String OPEN_WEATHER_MAP_LOCATION_ID = "#####";

String OPEN_WEATHER_MAP_LANGUAGE = "pt";

const boolean IS_METRIC = true;

const String WDAY_NAMES[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
const String MONTH_NAMES[] = {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};
/***************************
 * End Settings
 **************************/
 SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
 OLEDDisplayUi   ui( &display );

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapCurrent currentWeatherClient;

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
time_t now;

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

long timeSinceLastWUpdate = 0;

//declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawTemp(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();

FrameCallback frames[] = { drawTemp };
int numberOfFrames = 1;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

DHT dht = DHT(D3, DHT22, 6);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  dht.begin();
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();

    counter++;
  }

  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");

  ui.setTargetFPS(30);

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(TOP);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_DOWN);

  ui.setFrames(frames, numberOfFrames);

  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();

  Serial.println("");

  updateData(&display);
}

void loop() {
  
  if (millis() - timeSinceLastWUpdate > (1000L*UPDATE_INTERVAL_SECS)) {
    setReadyForWeatherUpdate();
    timeSinceLastWUpdate = millis();
  }

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }

}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Starting...");
  drawProgress(display, 30, "Updating Time...");
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID);
  drawProgress(display, 50, "Updating Weather...");
  
  uint8_t allowedHours[] = {12};

  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(500);
}

void drawTemp(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  
  float temperature = dht.readTemperature();

  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = WDAY_NAMES[timeInfo->tm_wday];
  sprintf_P(buff, PSTR("%s, %02d/%02d/%04d"), WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  display->drawString(64 + x, 5 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_CENTER);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_24);
  display->drawString(84 + x, 15 + y, String(temperature,1)+("°C"));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);

  int humidity = dht.readHumidity();
  
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(88 + x, 15 + y, String(humidity)+("%"));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
 
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 48, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(128, 48, temp);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}
