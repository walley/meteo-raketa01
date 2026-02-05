#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <WiFiClient.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

extern "C" {
  uint16 readvdd33(void);
}

#define ONE_WIRE_BUS 2
#define REDLED 14
#define GREENLED 12
#define BLUELED 13
//#define DOSLEEP
#define LEDBLINK

// Color definitions
#define AGFX_BLACK 0x0000
#define AGFX_BLUE 0x001F
#define AGFX_RED 0xF800
#define AGFX_GREEN 0x07E0
#define AGFX_CYAN 0x07FF
#define AGFX_MAGENTA 0xF81F
#define AGFX_YELLOW 0xFFE0
#define AGFX_WHITE 0xFFFF

ADC_MODE(ADC_VCC); //vcc read

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// HTTP config server (AP mode)
ESP8266WebServer configServer(80);
volatile bool config_saved = false;

static inline void drawPixelSafe(Adafruit_SSD1306 &disp, int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0) return;
  if (x >= disp.width() || y >= disp.height()) return;
  disp.drawPixel(x, y, color);
}

// Rectangle: outline or filled.
// x,y = top-left corner, w,h = size. If showNow is true, disp.display() is called.
void drawRectManual(Adafruit_SSD1306 &disp, int16_t x, int16_t y, int16_t w, int16_t h,
                    uint16_t color, bool filled = false, bool showNow = false) {
  if (w <= 0 || h <= 0) return;

  // Clip coordinates to display bounds (basic clipping)
  int16_t maxX = disp.width()  - 1;
  int16_t maxY = disp.height() - 1;

  int16_t xEnd = x + w - 1;
  int16_t yEnd = y + h - 1;

  if (!filled) {
    // top and bottom
    for (int16_t xi = x; xi <= xEnd; ++xi) {
      drawPixelSafe(disp, xi, y, color);
      drawPixelSafe(disp, xi, yEnd, color);
    }
    // left and right
    for (int16_t yi = y; yi <= yEnd; ++yi) {
      drawPixelSafe(disp, x, yi, color);
      drawPixelSafe(disp, xEnd, yi, color);
    }
  } else {
    // filled rectangle
    for (int16_t yi = y; yi <= yEnd; ++yi) {
      for (int16_t xi = x; xi <= xEnd; ++xi) {
        drawPixelSafe(disp, xi, yi, color);
      }
    }
  }

  if (showNow) disp.display();
}

// Midpoint circle algorithm (outline or filled).
// cx,cy = center, r = radius. If showNow is true, disp.display() is called.
void drawCircleManual(Adafruit_SSD1306 &disp, int16_t cx, int16_t cy, int16_t r,
                      uint16_t color, bool filled = false, bool showNow = false) {
  if (r <= 0) return;

  int16_t x = 0;
  int16_t y = r;
  int16_t d = 1 - r;

  auto plotCirclePoints = [&](int16_t px, int16_t py) {
    // 8-way symmetry
    drawPixelSafe(disp,  cx + px, cy + py, color);
    drawPixelSafe(disp,  cx - px, cy + py, color);
    drawPixelSafe(disp,  cx + px, cy - py, color);
    drawPixelSafe(disp,  cx - px, cy - py, color);
    drawPixelSafe(disp,  cx + py, cy + px, color);
    drawPixelSafe(disp,  cx - py, cy + px, color);
    drawPixelSafe(disp,  cx + py, cy - px, color);
    drawPixelSafe(disp,  cx - py, cy - px, color);
  };

  if (!filled) {
    // Outline only
    plotCirclePoints(x, y);
    while (x < y) {
      ++x;
      if (d < 0) {
        d += 2 * x + 1;
      } else {
        --y;
        d += 2 * (x - y) + 1;
      }
      plotCirclePoints(x, y);
    }
  } else {
    // Filled circle: draw horizontal spans between symmetric points
    // Draw center line first
    for (int16_t xi = cx - r; xi <= cx + r; ++xi) {
      drawPixelSafe(disp, xi, cy, color);
    }

    while (x < y) {
      ++x;
      if (d < 0) {
        d += 2 * x + 1;
      } else {
        --y;
        d += 2 * (x - y) + 1;
      }
      // For each pair of symmetric y rows, draw horizontal spans
      int16_t left1  = cx - x;
      int16_t right1 = cx + x;
      int16_t left2  = cx - y;
      int16_t right2 = cx + y;

      for (int16_t xi = left1; xi <= right1; ++xi) {
        drawPixelSafe(disp, xi, cy + y, color);
        drawPixelSafe(disp, xi, cy - y, color);
      }
      for (int16_t xi = left2; xi <= right2; ++xi) {
        drawPixelSafe(disp, xi, cy + x, color);
        drawPixelSafe(disp, xi, cy - x, color);
      }
    }
  }

  if (showNow) disp.display();
}

const char * essids[10];
const char * passwords[10];
const byte PIN_CLK = 12;   // define CLK pin (any digital pin)
const byte PIN_DIO = 14;   // define DIO pin (any digital pin)
WiFiClient wifi_client;
//SevenSegmentFun    display(PIN_CLK, PIN_DIO);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
uint16_t display_width;
uint16_t display_height;

void ledblink(int led)
{
#ifdef LEDBLINK
  digitalWrite(led, HIGH);
  delay(100);
  digitalWrite(led, LOW);
#endif
}

void printEncryptionType(int thisType)
{
  // read the encryption type and print out the name:
  switch (thisType) {
    case ENC_TYPE_WEP:
      Serial.println("WEP");
      break;

    case ENC_TYPE_TKIP:
      Serial.println("WPA");
      break;

    case ENC_TYPE_CCMP:
      Serial.println("WPA2");
      break;

    case ENC_TYPE_NONE:
      Serial.println("None");
      break;

    case ENC_TYPE_AUTO:
      Serial.println("Auto");
      break;
  }
}

void list_networks()
{
  // scan for nearby networks:
  Serial.println("** Scan Networks **");
  int numSsid = WiFi.scanNetworks();

  if (numSsid == -1) {
    Serial.println("Couldn't get a wifi connection");

    while (true);
  }

  // print the list of networks seen:
  Serial.print("number of available networks:");
  Serial.println(numSsid);

  // print the network number and name for each network found:
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Serial.print(thisNet);
    Serial.print(") " );
    Serial.print(WiFi.SSID(thisNet));
    Serial.print("\tSignal: " );
    Serial.print(WiFi.RSSI(thisNet));
    Serial.print(" dBm");
    Serial.print("\tEncryption: " );
    printEncryptionType(WiFi.encryptionType(thisNet));
  }
}

void say(char* str)
{
  //display.print(str);
  delay(500);
}

char * xstrcpy(char * dest, String src)
{
  int i;

  for (i = 0; i < src.length(); i++) {
    dest[i] = src[i];
  }

  dest[i] = '\0';
  return dest;
}

char * get_ip()
{
  char ip[16];
  xstrcpy(ip, WiFi.localIP().toString());
  return ip;
}

void turnOff(int pin)
{
  pinMode(pin, OUTPUT);
  digitalWrite(pin, 1);
}

int send_data(char * data)
{
  HTTPClient http;

  Serial.print("[HTTP] begin\n");
  http.begin(wifi_client, "http://grezl.eu/wiot/v1/sensor");

  Serial.print("[HTTP] POST\n");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int http_code = http.POST(data);
  //http.writeToStream(&Serial);

  Serial.printf("[HTTP] http code: %d\n", http_code);

  if (http_code > 0) {
    // Handle successful connection (non-negative returned code is an HTTP status or similar)
    // Print and handle common HTTP statuses:
    if (http_code == HTTP_CODE_OK || http_code == HTTP_CODE_CREATED) {
      // 200 OK or 201 Created - read the response payload
      String payload = http.getString();
      Serial.println("[HTTP] Success payload:");
      Serial.println(payload);
      ledblink(GREENLED);
    } else if (http_code >= 300 && http_code < 400) {
      Serial.println("[HTTP] Redirection");
      // Optionally you could inspect "Location" header here
      ledblink(BLUELED);
    } else if (http_code >= 400 && http_code < 500) {
      Serial.println("[HTTP] Client error");
      // Print server response if available
      String payload = http.getString();
      if (payload.length()) {
        Serial.println("[HTTP] Error payload:");
        Serial.println(payload);
      }
      ledblink(REDLED);
    } else if (http_code >= 500) {
      Serial.println("[HTTP] Server error");
      String payload = http.getString();
      if (payload.length()) {
        Serial.println("[HTTP] Error payload:");
        Serial.println(payload);
      }
      ledblink(REDLED);
    } else {
      // Other HTTP codes
      String payload = http.getString();
      if (payload.length()) {
        Serial.println("[HTTP] Response payload:");
        Serial.println(payload);
      }
    }
  } else {
    // http_code <= 0 indicates a connection/transport error
    Serial.printf("[HTTP] failed, error: %s\n", http.errorToString(http_code).c_str());
    ledblink(REDLED);
  }

  http.end();
  return http_code;
}

void create_line(char * type, char * sn, char * data, float value)
{
  char buf[12];

  strcpy(data, "type:METEO,");
  strcat(data, "sn:");
  strcat(data, sn);
  strcat(data, ",");
  strcat(data, "temp:");
  dtostrf(value, 5, 2, buf);
  strcat(data, buf);
  strcat(data, ",ip:");
  strcat(data, get_ip());
  strcat(data, ";");
}

char * get_wifi_status(int m)
{
  switch (m) {
    case 0:
      return "WL_IDLE_STATUS";

    case 1:
      return "WL_NO_SSID_AVAIL";

    case 2:
      return "WL_SCAN_COMPLETED";

    case 3:
      return "WL_CONNECTED";

    case 4:
      return "WL_CONNECT_FAILED";

    case 5:
      return "WL_CONNECTION_LOST";

    case 6:
      return "WL_DISCONNECTED";
  }
}

// ----- CONFIG AP / HTTP SERVER HANDLERS -----

// Simple HTML form served to configure one SSID/password (it writes into essids[0]/passwords[0])
const char CONFIG_FORM[] PROGMEM = R)rawliteral(
<!DOCTYPE html>
<html>
  <head><meta charset="utf-8"><title>Configure WiFi</title></head>
  <body>
    <h2>Configure WiFi</h2>
    <form method="POST" action="/save">
      <label for="essid">SSID:</label><br>
      <input type="text" id="essid" name="essid" required><br><br>
      <label for="password">Password:</label><br>
      <input type="password" id="password" name="password"><br><br>
      <input type="submit" value="Save">
    </form>
  </body>
</html>
)rawliteral