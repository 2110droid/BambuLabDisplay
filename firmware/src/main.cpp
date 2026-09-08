/*
  Bambu Lab P2S Round Status Display
  Multi-board web-configurable firmware

  Supported build targets in this repository:
    - ESP32-C3 Super Mini
    - ESP32-S3 DevKitC-1 / generic S3 DevKit
    - ESP32 DevKit / WROOM

  User-specific settings are NOT compiled into this firmware.
  They are stored in ESP32 Preferences/NVS and can be written from the
  GitHub Pages installer over USB Serial after flashing.

  Required libraries:
    - Arduino_GFX
    - U8g2
    - PubSubClient
    - ArduinoJson

  The GC9A01 pinout is selected at compile time for each board target.
*/


#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <math.h>

// ============================================================================
// BOARD-SPECIFIC CONFIGURATION
// ============================================================================

#if defined(BOARD_C3_SUPERMINI)

  static const char* BOARD_ID   = "c3-supermini";
  static const char* BOARD_NAME = "ESP32-C3 Super Mini";

  static const int TFT_CS   = 7;
  static const int TFT_DC   = 6;
  static const int TFT_MOSI = 5;
  static const int TFT_SCK  = 4;
  static const int TFT_BL   = 3;
  static const int TFT_RST  = 10;

#elif defined(BOARD_S3_DEVKIT)

  static const char* BOARD_ID   = "s3-devkit";
  static const char* BOARD_NAME = "ESP32-S3 DevKit";

  static const int TFT_CS   = 9;
  static const int TFT_DC   = 10;
  static const int TFT_MOSI = 11;
  static const int TFT_SCK  = 12;
  static const int TFT_BL   = 13;
  static const int TFT_RST  = 14;

#elif defined(BOARD_ESP32_WROOM)

  static const char* BOARD_ID   = "esp32-wroom";
  static const char* BOARD_NAME = "ESP32 DevKit / WROOM";

  // Safe, commonly available pins on classic ESP32 DevKit/WROOM boards.
  static const int TFT_CS   = 17;
  static const int TFT_DC   = 16;
  static const int TFT_MOSI = 23;
  static const int TFT_SCK  = 18;
  static const int TFT_BL   = 21;
  static const int TFT_RST  = 19;

#else
  #error "No supported board target selected."
#endif

// ============================================================================
// STORED USER CONFIGURATION
// ============================================================================

Preferences preferences;

String wifiSsid;
String wifiPassword;
String deviceHostname;
String printerIp;
String printerSerial;
String printerAccessCode;

// POSIX timezone string used by configTzTime().
String localTimezone = "CET-1CEST,M3.5.0,M10.5.0/3";

static const uint16_t MQTT_PORT = 8883;
static const char* MQTT_USERNAME = "bblp";

static const char* PREF_NAMESPACE = "p2sdisplay";

// ============================================================================
// DISPLAY
// ============================================================================

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED
);

Arduino_GFX *gfx = new Arduino_GC9A01(
  bus, TFT_RST, 0, true
);

// ============================================================================
// NETWORK
// ============================================================================

WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

// ============================================================================
// COLORS
// ============================================================================

static const uint16_t COLOR_BLACK      = 0x0000;
static const uint16_t COLOR_WHITE      = 0xFFFF;
static const uint16_t COLOR_PANEL      = 0x1082;
static const uint16_t COLOR_PANEL_ALT  = 0x18C3;
static const uint16_t COLOR_BLUE       = 0x04FF;
static const uint16_t COLOR_CYAN       = 0x07FF;
static const uint16_t COLOR_GREEN      = 0x07E0;
static const uint16_t COLOR_LIME       = 0xBFE0;
static const uint16_t COLOR_YELLOW     = 0xFFE0;
static const uint16_t COLOR_ORANGE     = 0xFD20;
static const uint16_t COLOR_RED        = 0xF800;
static const uint16_t COLOR_MAGENTA    = 0xF81F;
static const uint16_t COLOR_GREY       = 0x8410;
static const uint16_t COLOR_GREY_DARK  = 0x3186;

// ============================================================================
// UI GEOMETRY
// ============================================================================

static const int RING_CENTER_X = 120;
static const int RING_CENTER_Y = 120;
static const int RING_INNER_RADIUS = 108;
static const int RING_OUTER_RADIUS = 115;
static const float ARC_STEP_DEG = 0.5f;

static const int NOZZLE_PANEL_X = 48;
static const int NOZZLE_PANEL_Y = 160;
static const int NOZZLE_PANEL_W = 64;
static const int NOZZLE_PANEL_H = 36;

static const int BED_PANEL_X = 128;
static const int BED_PANEL_Y = 160;
static const int BED_PANEL_W = 64;
static const int BED_PANEL_H = 36;

// ============================================================================
// PRINTER DATA
// ============================================================================

enum PrinterState
{
  PRINTER_OFFLINE,
  PRINTER_IDLE,
  PRINTER_PRINTING,
  PRINTER_PAUSED,
  PRINTER_FINISHED,
  PRINTER_ERROR,
  PRINTER_UNKNOWN
};

PrinterState printerState = PRINTER_OFFLINE;
PrinterState previousPrinterState = PRINTER_UNKNOWN;

int progressPercent = 0;
int previousProgressPercent = -1;

int currentLayer = 0;
int previousLayer = -1;

int totalLayers = 0;
int previousTotalLayers = -1;

int remainingMinutes = 0;
int previousRemainingMinutes = -1;

float nozzleTemp = 0.0f;
int previousNozzleDisplay = -999;

float bedTemp = 0.0f;
int previousBedDisplay = -999;

String printName = "";
String previousPrintName = "";

bool mqttHasReceivedData = false;

// ============================================================================
// TIMING
// ============================================================================

unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long lastDataReceived = 0;

static const unsigned long WIFI_RETRY_INTERVAL_MS = 5000;
static const unsigned long MQTT_RETRY_INTERVAL_MS = 5000;
static const unsigned long DATA_TIMEOUT_MS = 30000;

// NTP / local time
static const char* NTP_SERVER_1 = "pool.ntp.org";
static const char* NTP_SERVER_2 = "time.google.com";

bool timeSyncStarted = false;

// ============================================================================
// TEXT HELPERS
// ============================================================================

void drawCenteredText(
  const String &text,
  int baselineY,
  const uint8_t *font,
  uint16_t color
)
{
  gfx->setFont(font);
  gfx->setTextColor(color);
  gfx->setTextWrap(false);

  int16_t x1, y1;
  uint16_t width, height;

  gfx->getTextBounds(text, 0, baselineY, &x1, &y1, &width, &height);

  int cursorX =
    ((240 - static_cast<int>(width)) / 2) - x1;

  gfx->setCursor(cursorX, baselineY);
  gfx->print(text);
}

void drawCenteredTextInBox(
  const String &text,
  int boxX,
  int boxW,
  int baselineY,
  const uint8_t *font,
  uint16_t color
)
{
  gfx->setFont(font);
  gfx->setTextColor(color);
  gfx->setTextWrap(false);

  int16_t x1, y1;
  uint16_t width, height;

  gfx->getTextBounds(text, 0, baselineY, &x1, &y1, &width, &height);

  int cursorX =
    boxX + ((boxW - static_cast<int>(width)) / 2) - x1;

  gfx->setCursor(cursorX, baselineY);
  gfx->print(text);
}

void drawLargePercentValue(
  int percent,
  int baselineY,
  uint16_t color
)
{
  percent = constrain(percent, 0, 100);

  String numberText = String(percent);
  String percentText = "%";

  int16_t x1Num, y1Num;
  uint16_t wNum, hNum;

  int16_t x1Pct, y1Pct;
  uint16_t wPct, hPct;

  // Large numeric-only font for the digits.
  gfx->setFont(u8g2_font_logisoso24_tn);
  gfx->getTextBounds(
    numberText,
    0,
    baselineY,
    &x1Num,
    &y1Num,
    &wNum,
    &hNum
  );

  // Keep the percent sign in the already proven Helvetica 18 font.
  gfx->setFont(u8g2_font_helvB18_tr);
  gfx->getTextBounds(
    percentText,
    0,
    baselineY,
    &x1Pct,
    &y1Pct,
    &wPct,
    &hPct
  );

  const int spacing = 3;

  int totalWidth =
    static_cast<int>(wNum) +
    spacing +
    static_cast<int>(wPct);

  int left =
    (240 - totalWidth) / 2;

  gfx->setTextColor(color);
  gfx->setTextWrap(false);

  gfx->setFont(u8g2_font_logisoso24_tn);
  gfx->setCursor(
    left - x1Num,
    baselineY
  );
  gfx->print(numberText);

  gfx->setFont(u8g2_font_helvB18_tr);
  gfx->setCursor(
    left +
    static_cast<int>(wNum) +
    spacing -
    x1Pct,
    baselineY
  );
  gfx->print(percentText);
}

String formatRemainingTime(int minutes)
{
  if (minutes < 0) minutes = 0;

  int hours = minutes / 60;
  int mins = minutes % 60;

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", hours, mins);

  return String(buffer);
}

String formatEstimatedFinishTime(int minutesRemaining)
{
  if (minutesRemaining < 0)
    minutesRemaining = 0;

  time_t now = time(nullptr);

  // If NTP has not synchronized yet, Unix time will still be very small.
  if (now < 1000000000)
    return "--:--";

  time_t finishTime =
    now + static_cast<time_t>(minutesRemaining) * 60;

  struct tm localFinish;

  if (!localtime_r(&finishTime, &localFinish))
    return "--:--";

  char buffer[8];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02d:%02d",
    localFinish.tm_hour,
    localFinish.tm_min
  );

  return String(buffer);
}

// ============================================================================
// STATE HELPERS
// ============================================================================

PrinterState decodePrinterState(const String &state)
{
  String s = state;
  s.toUpperCase();

  if (s == "RUNNING" || s == "PRINTING")
    return PRINTER_PRINTING;

  if (s == "PAUSE" || s == "PAUSED")
    return PRINTER_PAUSED;

  if (
    s == "FINISH" ||
    s == "FINISHED" ||
    s == "COMPLETE" ||
    s == "COMPLETED"
  )
    return PRINTER_FINISHED;

  if (s == "IDLE" || s == "READY")
    return PRINTER_IDLE;

  if (s == "FAILED" || s == "ERROR")
    return PRINTER_ERROR;

  return PRINTER_UNKNOWN;
}

String printerStateToString(PrinterState state)
{
  switch (state)
  {
    case PRINTER_OFFLINE:  return "OFFLINE";
    case PRINTER_IDLE:     return "READY";
    case PRINTER_PRINTING: return "PRINTING";
    case PRINTER_PAUSED:   return "PAUSED";
    case PRINTER_FINISHED: return "COMPLETE";
    case PRINTER_ERROR:    return "ERROR";
    default:               return "UNKNOWN";
  }
}

uint16_t printerStateColor(PrinterState state)
{
  switch (state)
  {
    case PRINTER_OFFLINE:  return COLOR_GREY;
    case PRINTER_IDLE:     return COLOR_CYAN;
    case PRINTER_PRINTING: return COLOR_BLUE;
    case PRINTER_PAUSED:   return COLOR_ORANGE;
    case PRINTER_FINISHED: return COLOR_GREEN;
    case PRINTER_ERROR:    return COLOR_RED;
    default:               return COLOR_WHITE;
  }
}


uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return
    ((r & 0xF8) << 8) |
    ((g & 0xFC) << 3) |
    (b >> 3);
}

uint16_t interpolateColor(
  uint16_t colorA,
  uint16_t colorB,
  float t
)
{
  t = constrain(t, 0.0f, 1.0f);

  uint8_t rA = ((colorA >> 11) & 0x1F) << 3;
  uint8_t gA = ((colorA >> 5)  & 0x3F) << 2;
  uint8_t bA = ( colorA        & 0x1F) << 3;

  uint8_t rB = ((colorB >> 11) & 0x1F) << 3;
  uint8_t gB = ((colorB >> 5)  & 0x3F) << 2;
  uint8_t bB = ( colorB        & 0x1F) << 3;

  uint8_t r = rA + static_cast<int>((rB - rA) * t);
  uint8_t g = gA + static_cast<int>((gB - gA) * t);
  uint8_t b = bA + static_cast<int>((bB - bA) * t);

  return rgb565(r, g, b);
}

uint16_t progressGradientColor(int percent)
{
  percent = constrain(percent, 0, 100);

  if (percent <= 50)
  {
    // Red -> Yellow
    return interpolateColor(
      COLOR_RED,
      COLOR_YELLOW,
      percent / 50.0f
    );
  }

  // Yellow -> Green
  return interpolateColor(
    COLOR_YELLOW,
    COLOR_GREEN,
    (percent - 50) / 50.0f
  );
}

// ============================================================================
// PROGRESS RING
// ============================================================================

void getPolarPoint(float angleDeg, int radius, int &x, int &y)
{
  float angleRad = angleDeg * DEG_TO_RAD;

  x = RING_CENTER_X +
      static_cast<int>(roundf(cosf(angleRad) * radius));

  y = RING_CENTER_Y +
      static_cast<int>(roundf(sinf(angleRad) * radius));
}

void drawSolidArc(float startAngle, float endAngle, uint16_t color)
{
  if (endAngle <= startAngle) return;

  for (float angle = startAngle; angle < endAngle; angle += ARC_STEP_DEG)
  {
    float nextAngle = angle + ARC_STEP_DEG;

    if (nextAngle > endAngle)
      nextAngle = endAngle;

    int outer1X, outer1Y;
    int outer2X, outer2Y;
    int inner1X, inner1Y;
    int inner2X, inner2Y;

    getPolarPoint(angle,     RING_OUTER_RADIUS, outer1X, outer1Y);
    getPolarPoint(nextAngle, RING_OUTER_RADIUS, outer2X, outer2Y);
    getPolarPoint(angle,     RING_INNER_RADIUS, inner1X, inner1Y);
    getPolarPoint(nextAngle, RING_INNER_RADIUS, inner2X, inner2Y);

    gfx->fillTriangle(
      outer1X, outer1Y,
      outer2X, outer2Y,
      inner1X, inner1Y,
      color
    );

    gfx->fillTriangle(
      inner1X, inner1Y,
      outer2X, outer2Y,
      inner2X, inner2Y,
      color
    );
  }
}

int displayedProgress()
{
  if (printerState == PRINTER_FINISHED)
    return 100;

  if (
    printerState == PRINTER_IDLE ||
    printerState == PRINTER_OFFLINE
  )
    return 0;

  return constrain(progressPercent, 0, 100);
}

void drawFullRing()
{
  drawSolidArc(
    -90.0f,
    270.0f,
    COLOR_GREY_DARK
  );

  int progress = displayedProgress();

  if (progress <= 0)
    return;

  // At 100%, replace the complete gradient with a solid green ring.
  if (progress >= 100)
  {
    drawSolidArc(
      -90.0f,
      270.0f,
      COLOR_GREEN
    );

    return;
  }

  // During printing the active ring transitions:
  // red -> yellow -> green.
  for (int p = 0; p < progress; p++)
  {
    float startAngle =
      -90.0f + (360.0f * p / 100.0f);

    float endAngle =
      -90.0f + (360.0f * (p + 1) / 100.0f);

    drawSolidArc(
      startAngle,
      endAngle,
      progressGradientColor(p + 1)
    );
  }
}

void updateRingIncrementally()
{
  int oldProgress = previousProgressPercent;

  if (
    previousPrinterState == PRINTER_IDLE ||
    previousPrinterState == PRINTER_OFFLINE
  )
    oldProgress = 0;

  if (previousPrinterState == PRINTER_FINISHED)
    oldProgress = 100;

  int newProgress = displayedProgress();

  // When the print reaches 100%, redraw the whole ring so the previous
  // red/yellow/green gradient becomes one continuous solid green ring.
  if (newProgress >= 100 && oldProgress < 100)
  {
    drawFullRing();
    return;
  }

  if (
    printerState != previousPrinterState ||
    newProgress < oldProgress ||
    previousProgressPercent < 0
  )
  {
    drawFullRing();
    return;
  }

  if (newProgress == oldProgress)
    return;

  float startAngle =
    -90.0f + (360.0f * oldProgress / 100.0f);

  float endAngle =
    -90.0f + (360.0f * newProgress / 100.0f);

  drawSolidArc(
    startAngle,
    endAngle,
    progressGradientColor(newProgress)
  );
}

// ============================================================================
// UI
// ============================================================================

void drawStaticUI()
{
  gfx->fillScreen(COLOR_BLACK);

  drawFullRing();

  gfx->fillRoundRect(72, 22, 96, 25, 12, COLOR_PANEL);

  gfx->fillRoundRect(
    NOZZLE_PANEL_X,
    NOZZLE_PANEL_Y,
    NOZZLE_PANEL_W,
    NOZZLE_PANEL_H,
    9,
    COLOR_PANEL
  );

  gfx->fillRoundRect(
    BED_PANEL_X,
    BED_PANEL_Y,
    BED_PANEL_W,
    BED_PANEL_H,
    9,
    COLOR_PANEL
  );
}

void updateStatus(bool force = false)
{
  if (!force && printerState == previousPrinterState)
    return;

  gfx->fillRoundRect(72, 22, 96, 25, 12, COLOR_PANEL);

  drawCenteredText(
    printerStateToString(printerState),
    40,
    u8g2_font_helvB10_tr,
    printerStateColor(printerState)
  );
}

void updateMainProgress(bool force = false)
{
  if (
    !force &&
    progressPercent == previousProgressPercent &&
    currentLayer == previousLayer &&
    totalLayers == previousTotalLayers &&
    printerState == previousPrinterState
  )
    return;

  gfx->fillRect(42, 56, 156, 70, COLOR_BLACK);

  if (printerState == PRINTER_OFFLINE)
  {
    drawCenteredText(
      "OFFLINE",
      88,
      u8g2_font_helvB18_tr,
      COLOR_GREY
    );

    drawCenteredText(
      "Waiting for printer",
      112,
      u8g2_font_helvR08_tr,
      COLOR_GREY
    );

    return;
  }

  if (printerState == PRINTER_IDLE)
  {
    drawCenteredText(
      "READY",
      88,
      u8g2_font_helvB18_tr,
      COLOR_CYAN
    );

    drawCenteredText(
      "Printer available",
      112,
      u8g2_font_helvR08_tr,
      COLOR_GREY
    );

    return;
  }

  if (printerState == PRINTER_FINISHED)
  {
    drawLargePercentValue(
      100,
      96,
      COLOR_GREEN
    );

    drawCenteredText(
      "Print finished",
      118,
      u8g2_font_helvR08_tr,
      COLOR_GREY
    );

    return;
  }

  if (printerState == PRINTER_ERROR)
  {
    drawCenteredText(
      "ERROR",
      88,
      u8g2_font_helvB18_tr,
      COLOR_RED
    );

    drawCenteredText(
      "Check printer",
      112,
      u8g2_font_helvR08_tr,
      COLOR_GREY
    );

    return;
  }

  uint16_t mainColor =
    printerState == PRINTER_PAUSED
      ? COLOR_ORANGE
      : COLOR_WHITE;

  drawLargePercentValue(
    progressPercent,
    96,
    mainColor
  );

  if (totalLayers > 0)
  {
    drawCenteredText(
      "Layer " + String(currentLayer) + " / " + String(totalLayers),
      118,
      u8g2_font_helvR08_tr,
      COLOR_GREY
    );
  }
  else if (currentLayer > 0)
  {
    drawCenteredText(
      "Layer " + String(currentLayer),
      118,
      u8g2_font_helvR08_tr,
      COLOR_GREY
    );
  }
}

void updateRemainingTime(bool force = false)
{
  if (
    !force &&
    remainingMinutes == previousRemainingMinutes &&
    printerState == previousPrinterState
  )
    return;

  gfx->fillRect(45, 122, 150, 37, COLOR_BLACK);

  if (
    printerState == PRINTER_PRINTING ||
    printerState == PRINTER_PAUSED ||
    printerState == PRINTER_UNKNOWN
  )
  {
    drawCenteredText(
      formatRemainingTime(remainingMinutes),
      139,
      u8g2_font_helvB12_tr,
      COLOR_WHITE
    );

    drawCenteredText(
      "TIME LEFT (HH:MM)",
      150,
      u8g2_font_helvR08_tr,
      COLOR_GREY
    );

    drawCenteredText(
      "DONE " + formatEstimatedFinishTime(remainingMinutes),
      159,
      u8g2_font_helvR08_tr,
      COLOR_CYAN
    );
  }
  else if (printerState == PRINTER_IDLE)
  {
    drawCenteredText(
      "NO ACTIVE JOB",
      145,
      u8g2_font_helvR08_tr,
      COLOR_GREY
    );
  }
  else if (printerState == PRINTER_FINISHED)
  {
    drawCenteredText(
      "PRINT COMPLETE",
      145,
      u8g2_font_helvR08_tr,
      COLOR_GREEN
    );
  }
}

void updateTemperatures(bool force = false)
{
  int nozzleValue = static_cast<int>(roundf(nozzleTemp));
  int bedValue = static_cast<int>(roundf(bedTemp));

  if (force || nozzleValue != previousNozzleDisplay)
  {
    gfx->fillRoundRect(
      NOZZLE_PANEL_X,
      NOZZLE_PANEL_Y,
      NOZZLE_PANEL_W,
      NOZZLE_PANEL_H,
      9,
      COLOR_PANEL_ALT
    );

    drawCenteredTextInBox(
      "NOZZLE",
      NOZZLE_PANEL_X,
      NOZZLE_PANEL_W,
      172,
      u8g2_font_helvR08_tr,
      COLOR_GREY
    );

    drawCenteredTextInBox(
      String(nozzleValue) + "C",
      NOZZLE_PANEL_X,
      NOZZLE_PANEL_W,
      191,
      u8g2_font_helvB12_tr,
      COLOR_ORANGE
    );

    previousNozzleDisplay = nozzleValue;
  }

  if (force || bedValue != previousBedDisplay)
  {
    gfx->fillRoundRect(
      BED_PANEL_X,
      BED_PANEL_Y,
      BED_PANEL_W,
      BED_PANEL_H,
      9,
      COLOR_PANEL_ALT
    );

    drawCenteredTextInBox(
      "BED",
      BED_PANEL_X,
      BED_PANEL_W,
      172,
      u8g2_font_helvR08_tr,
      COLOR_GREY
    );

    drawCenteredTextInBox(
      String(bedValue) + "C",
      BED_PANEL_X,
      BED_PANEL_W,
      191,
      u8g2_font_helvB12_tr,
      COLOR_CYAN
    );

    previousBedDisplay = bedValue;
  }
}

void updatePrintName(bool force = false)
{
  // Intentionally disabled.
  // Long print/subtask names do not fit cleanly on the round 240x240 UI.
}

void updateDashboard(bool force = false)
{
  updateRingIncrementally();
  updateStatus(force);
  updateMainProgress(force);
  updateRemainingTime(force);
  updateTemperatures(force);

  previousPrinterState = printerState;
  previousProgressPercent = progressPercent;
  previousLayer = currentLayer;
  previousTotalLayers = totalLayers;
  previousRemainingMinutes = remainingMinutes;
}

// ============================================================================
// MQTT
// ============================================================================

String mqttReportTopic()
{
  return String("device/") + printerSerial + "/report";
}

void parsePrinterPayload(byte* payload, unsigned int length)
{
  DynamicJsonDocument doc(32768);

  DeserializationError error =
    deserializeJson(doc, payload, length);

  if (error)
  {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  if (!doc["print"].is<JsonObject>())
    return;

  JsonObject printData = doc["print"].as<JsonObject>();

  if (!printData["gcode_state"].isNull())
  {
    String state =
      String(printData["gcode_state"].as<const char*>());

    printerState = decodePrinterState(state);
  }

  if (!printData["mc_percent"].isNull())
    progressPercent =
      constrain(printData["mc_percent"].as<int>(), 0, 100);

  if (!printData["mc_remaining_time"].isNull())
    remainingMinutes =
      max(0, printData["mc_remaining_time"].as<int>());

  if (!printData["layer_num"].isNull())
    currentLayer =
      max(0, printData["layer_num"].as<int>());

  if (!printData["total_layer_num"].isNull())
    totalLayers =
      max(0, printData["total_layer_num"].as<int>());

  if (!printData["nozzle_temper"].isNull())
    nozzleTemp =
      printData["nozzle_temper"].as<float>();

  if (!printData["bed_temper"].isNull())
    bedTemp =
      printData["bed_temper"].as<float>();

  if (!printData["subtask_name"].isNull())
    printName =
      String(printData["subtask_name"].as<const char*>());

  mqttHasReceivedData = true;
  lastDataReceived = millis();

  updateDashboard();
}

void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length
)
{
  Serial.print("MQTT message received: ");
  Serial.print(length);
  Serial.println(" bytes");

  parsePrinterPayload(payload, length);
}

bool connectMQTT()
{
  if (WiFi.status() != WL_CONNECTED)
    return false;

  secureClient.setInsecure();
  secureClient.setTimeout(15);

  mqttClient.setServer(printerIp.c_str(), MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(32768);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(15);

  uint64_t chipId = ESP.getEfuseMac();

  char clientId[48];

  snprintf(
    clientId,
    sizeof(clientId),
    "P2S-%04X%08X",
    static_cast<uint16_t>(chipId >> 32),
    static_cast<uint32_t>(chipId)
  );

  Serial.println("Connecting to P2S MQTT...");

  bool connected =
    mqttClient.connect(
      clientId,
      MQTT_USERNAME,
      printerAccessCode.c_str()
    );

  if (!connected)
  {
    Serial.print("MQTT failed, state=");
    Serial.println(mqttClient.state());

    printerState = PRINTER_OFFLINE;
    updateDashboard();

    return false;
  }

  String topic = mqttReportTopic();

  bool subscribed =
    mqttClient.subscribe(topic.c_str(), 0);

  Serial.print("Subscribed: ");
  Serial.println(subscribed ? "YES" : "NO");

  if (subscribed && !mqttHasReceivedData)
  {
    printerState = PRINTER_UNKNOWN;
    updateDashboard();
  }

  return subscribed;
}

// ============================================================================
// WI-FI
// ============================================================================

void startWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (!WiFi.setHostname(deviceHostname.c_str()))
  {
    Serial.println("Warning: Could not set Wi-Fi hostname.");
  }

  WiFi.begin(
    wifiSsid.c_str(),
    wifiPassword.c_str()
  );

  lastWiFiAttempt = millis();

  Serial.print("Board: ");
  Serial.println(BOARD_NAME);

  Serial.print("Hostname: ");
  Serial.println(deviceHostname);

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(wifiSsid);
}

void maintainConnections()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    if (
      millis() - lastWiFiAttempt >=
      WIFI_RETRY_INTERVAL_MS
    )
    {
      Serial.println("Retrying Wi-Fi...");

      WiFi.disconnect();
      delay(100);

      WiFi.setHostname(deviceHostname.c_str());

      WiFi.begin(
        wifiSsid.c_str(),
        wifiPassword.c_str()
      );

      lastWiFiAttempt = millis();

      printerState = PRINTER_OFFLINE;
      updateDashboard();
    }

    return;
  }

  if (!timeSyncStarted)
  {
    configTzTime(
      localTimezone.c_str(),
      NTP_SERVER_1,
      NTP_SERVER_2
    );

    timeSyncStarted = true;

    Serial.println("NTP time synchronization started.");
  }

  if (!mqttClient.connected())
  {
    if (
      millis() - lastMQTTAttempt >=
      MQTT_RETRY_INTERVAL_MS
    )
    {
      lastMQTTAttempt = millis();
      connectMQTT();
    }

    return;
  }

  mqttClient.loop();

  if (
    mqttHasReceivedData &&
    millis() - lastDataReceived > DATA_TIMEOUT_MS &&
    printerState != PRINTER_OFFLINE
  )
  {
    printerState = PRINTER_OFFLINE;
    updateDashboard();
  }
}

// ============================================================================
// STARTUP / CONFIGURATION
// ============================================================================

void drawStartupScreen()
{
  gfx->fillScreen(COLOR_BLACK);

  drawCenteredText(
    "P2S",
    104,
    u8g2_font_helvB24_tr,
    COLOR_BLUE
  );

  drawCenteredText(
    "STATUS DISPLAY",
    136,
    u8g2_font_helvR08_tr,
    COLOR_GREY
  );

  drawCenteredText(
    BOARD_ID,
    158,
    u8g2_font_helvR08_tr,
    COLOR_GREY_DARK
  );
}

void drawConfigRequiredScreen()
{
  gfx->fillScreen(COLOR_BLACK);

  drawCenteredText(
    "CONFIG",
    101,
    u8g2_font_helvB18_tr,
    COLOR_ORANGE
  );

  drawCenteredText(
    "Connect by USB",
    131,
    u8g2_font_helvR08_tr,
    COLOR_GREY
  );

  drawCenteredText(
    "and save settings",
    148,
    u8g2_font_helvR08_tr,
    COLOR_GREY
  );
}

void loadConfiguration()
{
  preferences.begin(PREF_NAMESPACE, true);

  wifiSsid =
    preferences.getString("wifi_ssid", "");

  wifiPassword =
    preferences.getString("wifi_pass", "");

  deviceHostname =
    preferences.getString(
      "hostname",
      "Bambu-P2S-Display"
    );

  printerIp =
    preferences.getString("printer_ip", "");

  printerSerial =
    preferences.getString("serial", "");

  printerAccessCode =
    preferences.getString("access_code", "");

  localTimezone =
    preferences.getString(
      "timezone",
      "CET-1CEST,M3.5.0,M10.5.0/3"
    );

  preferences.end();
}

bool configLooksValid()
{
  return
    wifiSsid.length() > 0 &&
    deviceHostname.length() > 0 &&
    printerIp.length() > 0 &&
    printerSerial.length() > 0 &&
    printerAccessCode.length() > 0;
}

void sendConfigStatus()
{
  StaticJsonDocument<512> reply;

  reply["ok"] = true;
  reply["event"] = "config";
  reply["board_id"] = BOARD_ID;
  reply["board_name"] = BOARD_NAME;

  reply["ssid"] = wifiSsid;
  reply["hostname"] = deviceHostname;
  reply["printer_ip"] = printerIp;
  reply["printer_serial"] = printerSerial;
  reply["timezone"] = localTimezone;

  // Never return secrets to the browser.
  reply["wifi_password_set"] =
    wifiPassword.length() > 0;

  reply["access_code_set"] =
    printerAccessCode.length() > 0;

  serializeJson(reply, Serial);
  Serial.println();
}

void saveConfiguration(JsonObjectConst root)
{
  String newSsid =
    root["ssid"] | "";

  String newWifiPassword =
    root["wifi_password"] | "";

  String newHostname =
    root["hostname"] | "Bambu-P2S-Display";

  String newPrinterIp =
    root["printer_ip"] | "";

  String newPrinterSerial =
    root["printer_serial"] | "";

  String newAccessCode =
    root["access_code"] | "";

  String newTimezone =
    root["timezone"] |
    "CET-1CEST,M3.5.0,M10.5.0/3";

  if (
    newSsid.length() == 0 ||
    newHostname.length() == 0 ||
    newPrinterIp.length() == 0 ||
    newPrinterSerial.length() == 0 ||
    newAccessCode.length() == 0
  )
  {
    Serial.println(
      "{\"ok\":false,\"event\":\"config_saved\",\"error\":\"missing_required_field\"}"
    );
    return;
  }

  preferences.begin(PREF_NAMESPACE, false);

  preferences.putString(
    "wifi_ssid",
    newSsid
  );

  preferences.putString(
    "wifi_pass",
    newWifiPassword
  );

  preferences.putString(
    "hostname",
    newHostname
  );

  preferences.putString(
    "printer_ip",
    newPrinterIp
  );

  preferences.putString(
    "serial",
    newPrinterSerial
  );

  preferences.putString(
    "access_code",
    newAccessCode
  );

  preferences.putString(
    "timezone",
    newTimezone
  );

  preferences.end();

  Serial.println(
    "{\"ok\":true,\"event\":\"config_saved\",\"restart\":true}"
  );

  Serial.flush();
  delay(700);
  ESP.restart();
}

void clearConfiguration()
{
  preferences.begin(PREF_NAMESPACE, false);
  preferences.clear();
  preferences.end();

  Serial.println(
    "{\"ok\":true,\"event\":\"config_cleared\",\"restart\":true}"
  );

  Serial.flush();
  delay(700);
  ESP.restart();
}

void processSerialCommand(const String &line)
{
  if (line.length() == 0)
    return;

  StaticJsonDocument<1024> doc;

  DeserializationError error =
    deserializeJson(doc, line);

  if (error)
  {
    Serial.println(
      "{\"ok\":false,\"event\":\"error\",\"error\":\"invalid_json\"}"
    );
    return;
  }

  String command =
    doc["cmd"] | "";

  if (command == "ping")
  {
    StaticJsonDocument<256> reply;

    reply["ok"] = true;
    reply["event"] = "pong";
    reply["board_id"] = BOARD_ID;
    reply["board_name"] = BOARD_NAME;
    reply["configured"] = configLooksValid();

    serializeJson(reply, Serial);
    Serial.println();

    return;
  }

  if (command == "read")
  {
    sendConfigStatus();
    return;
  }

  if (command == "config")
  {
    saveConfiguration(
      doc.as<JsonObjectConst>()
    );
    return;
  }

  if (command == "clear")
  {
    clearConfiguration();
    return;
  }

  Serial.println(
    "{\"ok\":false,\"event\":\"error\",\"error\":\"unknown_command\"}"
  );
}

void handleSerialConfiguration()
{
  static String inputLine;

  while (Serial.available() > 0)
  {
    char c =
      static_cast<char>(Serial.read());

    if (c == '\r')
      continue;

    if (c == '\n')
    {
      processSerialCommand(inputLine);
      inputLine = "";
      continue;
    }

    if (inputLine.length() < 1536)
    {
      inputLine += c;
    }
    else
    {
      inputLine = "";
      Serial.println(
        "{\"ok\":false,\"event\":\"error\",\"error\":\"line_too_long\"}"
      );
    }
  }
}

// ============================================================================
// SETUP
// ============================================================================

void setup()
{
  Serial.begin(115200);
  delay(700);

  loadConfiguration();

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);

  if (!gfx->begin())
  {
    Serial.println(
      "{\"ok\":false,\"event\":\"display_error\"}"
    );

    while (true)
    {
      handleSerialConfiguration();
      delay(10);
    }
  }

  gfx->setUTF8Print(true);

  gfx->fillScreen(COLOR_BLACK);
  digitalWrite(TFT_BL, HIGH);

  drawStartupScreen();

  Serial.println();
  Serial.println("Bambu P2S Display firmware started.");
  Serial.print("Board: ");
  Serial.println(BOARD_NAME);
  Serial.println(
    "USB config commands: ping, read, config, clear (JSON lines)"
  );

  if (!configLooksValid())
  {
    delay(700);
    drawConfigRequiredScreen();

    Serial.println(
      "{\"ok\":true,\"event\":\"config_required\"}"
    );

    return;
  }

  delay(700);

  printerState = PRINTER_OFFLINE;

  drawStaticUI();
  updateDashboard(true);

  mqttClient.setCallback(mqttCallback);

  startWiFi();
}

// ============================================================================
// LOOP
// ============================================================================

void loop()
{
  // USB configuration remains available even while the display is running.
  handleSerialConfiguration();

  if (configLooksValid())
  {
    maintainConnections();
  }

  delay(5);
}
