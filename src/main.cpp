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
    Serial.print(") ");
    Serial.print(WiFi.SSID(thisNet));
    Serial.print("\tSignal: ");
    Serial.print(WiFi.RSSI(thisNet));
    Serial.print(" dBm");
    Serial.print("\tEncryption: ");
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

void find_wifi()
{
  int i,ii;

  for (i = 0; i < 4; i++) {
    Serial.print("trying ");
    Serial.print(essids[i]);
    Serial.print(":");
    Serial.println(passwords[i]);

    WiFi.begin(essids[i], passwords[i]);

    for (ii = 0; ii < 100; ii++) {
      Serial.println(get_wifi_status(WiFi.status()));
      delay(20);
    }

    if ((WiFi.waitForConnectResult() == WL_CONNECTED)) {
      Serial.println("FOUND");
      return;
    } else {
      Serial.println("NOPE");
      WiFi.disconnect();
    }
  }

  Serial.println("couldn't find a shit");
}

void turn_off_pins()
{
  //turnOff(10);
  //turnOff(9);
  //turnOff(16);
  //turnOff(5);
  //turnOff(4);
  //turnOff(0);
  //turnOff(2);
  //turnOff(14);
  //turnOff(12);
  //turnOff(13);
  //turnOff(15);
  //turnOff(3);
  //turnOff(1);
}


void infoled_setup()
{
  pinMode(GREENLED, OUTPUT);
  pinMode(BLUELED, OUTPUT);
  pinMode(REDLED, OUTPUT);

  digitalWrite(GREENLED, LOW);
  digitalWrite(REDLED, LOW);
  digitalWrite(BLUELED, LOW);
}

void wifi_essids_setup()
{
  essids[0] = "wa-v101f";
  essids[1] = "HZSOL-WRK";
  essids[2] = "waredmi";
  essids[3] = "3GWiFi_55357C";

  passwords[0] = "aaaaaaaaa";
  passwords[1] = "HZSOL231wpa";
  passwords[2] = "aaaaaaaaa";
  passwords[3] = "1245678";
}

void setup_display()
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
  }

  display_width = display.width();
  display_height = display.height();
}

void setup_sensors()
{
  sensors.begin();
}

float get_sensors()
{
  float t;

  sensors.requestTemperatures();
  t = sensors.getTempCByIndex(0);
  Serial.println(t);
  return t;
}

void show_sensors()
{
  float t;
  t = get_sensors();
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print("temperature");
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 25);
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.print(t);
  display.print((char)247);
  display.print("C");
  display.display();
}

void do_stuff()
{
  char tstr[20];
  float t;
  int tt;
  char data[50];

  say("----");

  float vdd = ESP.getVcc() / 1000.0;
  dtostrf(vdd, 2, 2, tstr);
  say("voltage:");
  say(tstr);

//  Serial.println(readvdd33());

  delay(500);

  say("----");
  say("temperature");

  sensors.requestTemperatures();
  t = sensors.getTempCByIndex(0);
//  dtostrf(t, 2, 2, tstr);
//  say(tstr);

  //display.setColonOn(true);
  byte  rawData;
  tt = t * 100;
  //display.print(tt);
  // display degree symbol on position 3 plus set lower colon
  rawData = B11100011;
  //display.printRaw(rawData, 3);
  delay(1000);
  //display.setColonOn(false);

  create_line("x","meteotest", data, t);
  Serial.println(data);

  if ((WiFi.status() == WL_CONNECTED)) {
    //send_data(data);
    Serial.print("connected to ");
    Serial.println(WiFi.SSID());
    ledblink(GREENLED);
  } else {
    Serial.println("not connected");
    ledblink(REDLED);
  }
}

void setup()
{
  int i;

  Serial.begin(9600);

  infoled_setup();
  setup_display();
  setup_sensors();

  display.clearDisplay();
  display.setCursor(0, 40);
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  display.print("+29");
  display.print((char)274);
  display.print("C");
  //display.drawChar(0, 0, 'x', AGFX_RED, AGFX_WHITE, 1);
  display.fillRect(0, 0, 64, 20, AGFX_RED);
  display.display();

  for (i=0; i<display_width; i+=4) {
    display.drawLine(0, display_height-1, i, 0, SSD1306_WHITE);
    display.display();
    delay(1);
  }

  /*  wifi_essids_setup();
      turn_off_pins();
      sensors.begin();

    //  WiFi.persistent(false); //disables the storage to flash.

      WiFi.mode(WIFI_STA);

      if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin();
      }

      delay(100);

      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("start: not connected");
        find_wifi();
      } else {
        Serial.println("start: connected");
      }

      Serial.println(WiFi.macAddress());

      //list_networks();

      delay(1000);

      do_stuff();

    #ifdef DOSLEEP
      ESP.deepSleep(59000000);
    #endif
    */
}

void loop()
{
  //do_stuff();
  show_sensors();

  analogWrite(REDLED,   random(256));
  analogWrite(GREENLED, random(256));
  analogWrite(BLUELED,  random(256));
  get_sensors();

  delay(1000); // keep the color 1 second
}