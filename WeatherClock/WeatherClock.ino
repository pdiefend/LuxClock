/*

   Plan:


   List of higher-level tasks:
  - 1) Figure out how to perform timed tasks
  -   a) Update weather forecast at 45mins
  -   b) Update display at 0mins and 30mins
    2) Merge NTP into weather code
      a) add way to keep trying until time RX'd first time
      b) count sequential failures and retry if failed x times in a row.
    3) Set up NTP and weather updates to reconnect to Wifi if lost
  - 4) Parse hour data from weather forecast to sync DST
  - 5) Figure out display cycle timing
  -   a) propbably need 2 timers or one and loop
  -   b) one timer for next event, need FSM
  -   c) other timer for cycling LEDS, less complicated FSM
  -   d) actually timers aren't necessary. We can use an FSM in loop. Less overhead, simpiler
    6) Merge LED code into rest of code
    7) Add not-connected state with LED display mode
    8) Figure out color mapping for

  Notes:
  packed color is uint32_t
  r = c[16:23]
  g = c[8:15]
  b = c[0:7]
*/

#include <TimeLib.h>
//#include <TimeAlarms.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include "PASSWORDS.h"

// Definitions and constants
#define NUM_TEMPS 36
#define NUM_LEDS 48
#define Weather_DL_Minute 56
#define LED_Cycle_Time 5 // can't be 1
#define DAYLIGHT_MODE 0
#define TEMPERATURE_MODE 1
#define POP_MODE 2
#define TRANS_STEPS 45

#define NTP_PACKET_SIZE 48

#define WS2812_PIN 2

// Forward Declarations
void updateWeatherForecast();
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
void updateDisplay();
void advanceDisplay();
void convertWeatherToColor();
int getNextIntFromString(String str, int startIdx);
void convertWeatherToColor();
void displayData();

const char request[] =
  "GET /api/" API_Key "/astronomy/hourly/q/" Zipcode ".json HTTP/1.1\r\n"
  "User-Agent: ESP8266/0.1\r\n"
  "Accept: */*\r\n"
  "Host: api.wunderground.com\r\n"
  "Connection: close\r\n"
  "\r\n";
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = -5;  // Eastern Standard Time (USA)
const unsigned int localPort = 8888;  // local port to listen for UDP packets

// Globals
static char respBuf[4096];
byte packetBuffer[NTP_PACKET_SIZE];

int temperatures[NUM_TEMPS]; // must be signed... it gets cold here sometimes
int pops[NUM_TEMPS]; // could be reduced to bytes
int hours[NUM_TEMPS]; // cound be reduced to bytes
int sunrise = 0;
int sunset = 0;
byte tempidx = 0;
byte popidx = 0;
byte hoursidx = 0;
WiFiUDP Udp;

byte cycleNum = 0;
//uint32_t coloredWeather[3][NUM_LEDS+2];
byte red[3][NUM_LEDS + 2];
byte blue[3][NUM_LEDS + 2];
byte green[3][NUM_LEDS + 2];
Adafruit_NeoPixel ring = Adafruit_NeoPixel(NUM_LEDS, WS2812_PIN, NEO_GRB + NEO_KHZ800);

// Scheduling flags
byte tempFlag = 0;
byte WeatherDataFresh = 0;
byte LEDs_Advanced = 0;


void setup() {
  Serial.begin(115200);
  ring.begin();
  ring.setBrightness(25);
  for (int i = 0; i < NUM_LEDS; i++) {
    ring.setPixelColor(i, 0);
  }
  ring.show();

  Serial.print("Reset Timeout");
  for (int i = 0; i < 3; i++) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\n");

  Serial.print("Connecting");

  WiFi.begin(mySSID, myPASS);

  int waitCounter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    ring.setPixelColor(waitCounter, ring.Color(50, 0, 0));
    ring.show();
    waitCounter++;
    delay(500);
    Serial.print(F("."));
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    ring.setPixelColor(i, ring.Color(0, 50, 0));
  }
  ring.show();
  Serial.print("Connected, My IP is: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  /*
    while(timeStatus() == timeNotSet){
      delay(10000);
      setTime(getNtpTime());
      //yield();
    }
  */
  delay(1000);
  yield();
  delay(1000);
  yield();

  for (int i = 0; i < NUM_LEDS; i++) {
    ring.setPixelColor(i, 0);
  }
  ring.show();

  updateWeatherForecast();
  WeatherDataFresh = 1;
  convertWeatherToColor();
}




void loop() {

  // updateWeatherForecast();
  //displayData();

  // advance LEDs
  if (((minute() == 0) || (minute() == 30)) && LEDs_Advanced == 0) { // 0 minutes might not be needed
    LEDs_Advanced = 1;
    //advanceDisplay();
    // advance the half hour LEDs
  } else if ((minute() == 1) || (minute() == 31)) {
    LEDs_Advanced = 0;
  }

  // cycle display modes
  if (((second() % LED_Cycle_Time) == 0) && tempFlag == 0) {
    Serial.printf("%02d", hour());
    Serial.print(":");
    Serial.printf("%02d", minute());
    Serial.print(":");
    Serial.printf("%02d", second());
    Serial.println();
    tempFlag = 1;
    updateDisplay();


  } else if ((second() % LED_Cycle_Time) != 0) {
    tempFlag = 0;
  }

  // Download Weather Data
  if (minute() == Weather_DL_Minute && WeatherDataFresh == 0) {
    //updateWeatherForecast();
    WeatherDataFresh = 1;
    displayData();
  } else if (minute() == (Weather_DL_Minute + 1)) {
    WeatherDataFresh = 0;
  }

  




  /*
    Serial.println("\nEnd");
    // DEV purposes, all stop
    while(1){
      yield(); // needs to be yeild or else WDT expires
    }
  */
}

// Weather Download Functions =========================================================

void updateWeatherForecast() {
  // TODO check if wifi is still connected

  Serial.println("Downloading Forecast");

  // Open socket
  WiFiClient httpclient;
  if (!httpclient.connect("api.wunderground.com", 80)) {
    Serial.println("[Weather download]: connection failed");
    while (1);
    return;
    // TODO fail more gracefully and recover
  }

  //Serial.print(request);
  httpclient.print(request);
  httpclient.flush();

  // TODO get cloud cover (sky)?, others?
  // How should I display these. I don't want more than 3 cycles
  // maybe find a way to show cloud cover and sunrise/sunset in same cycle

  String line = "";
  int idx = 0;
  while (httpclient.connected() || httpclient.available()) {
    line = httpclient.readStringUntil('\n');
    if (line.indexOf("temp") != -1) {
      temperatures[tempidx] = getNextIntFromString(line, line.indexOf("english"));
      tempidx += 1;
    } else if (line.indexOf("pop") != -1) {
      pops[popidx] = getNextIntFromString(line, line.indexOf("pop"));
      popidx += 1;
    } else if (line.indexOf("hour_padded") != -1) {
      hours[hoursidx] = getNextIntFromString(line, line.indexOf("hour"));
      hoursidx += 1;
    } else if (line.indexOf("sunrise") != -1) {
      line = httpclient.readStringUntil('}');
      sunrise = getNextIntFromString(line, line.indexOf("hour")) * 60;
      sunrise += getNextIntFromString(line, line.indexOf("minute"));
    } else if (line.indexOf("sunset") != -1) {
      line = httpclient.readStringUntil('}');
      sunset = getNextIntFromString(line, line.indexOf("hour")) * 60;
      sunset += getNextIntFromString(line, line.indexOf("minute"));
    }
  }

  tempidx = 0;
  popidx = 0;
  line = "";

  Serial.println();
  Serial.println("Closing connection\n");

  httpclient.stop();
}

// Display Functions ===============================================================

void updateDisplay() {
  // update LED Display
  int nextCycleNum = (cycleNum + 1) % 3;


  //int nextCycleNum = (cycleNum+1) % 3;
  //byte rOld,gOld,bOld = 0;
  //byte rNew,gNew,bNew = 0;
  //int rStep, gStep, bStep = 0;
  int r, g, b = 0;

  for (int jdx = 1; jdx <= TRANS_STEPS; jdx++) {
    // 0 -> NUM_LEDS
    // for each LED calculate the step increment to the next pixel
    for (int idx = 0; idx < NUM_LEDS; idx++) {
      float rStep = (red[nextCycleNum][idx] - red[cycleNum][idx]);
      rStep = rStep * (float)jdx / (float)TRANS_STEPS;

      float gStep = (green[nextCycleNum][idx] - green[cycleNum][idx]);
      gStep = gStep * (float)jdx / (float)TRANS_STEPS;

      float bStep = (blue[nextCycleNum][idx] - blue[cycleNum][idx]);
      bStep = bStep * (float)jdx / (float)TRANS_STEPS;

      r = red[cycleNum][idx] + (int)rStep;
      g = green[cycleNum][idx] + (int)gStep;
      b = blue[cycleNum][idx] + (int)bStep;

      ring.setPixelColor(idx, ring.Color(r, g, b));
    }

    ring.show();

    delay(1500.0 / TRANS_STEPS);
    yield();
  }

  cycleNum = nextCycleNum;

  for (int i = 0; i < NUM_LEDS; i++) {
    ring.setPixelColor(i, ring.Color(red[cycleNum][i], green[cycleNum][i], blue[cycleNum][i]));
  }
  ring.show();
}

void advanceDisplay() {
  // advance LED Display 1/2 hour
  for (int idx = 0; idx < (NUM_LEDS + 1); idx++) {
    //coloredWeather[0][idx] = coloredWeather[0][idx + 1];
    //coloredWeather[1][idx] = coloredWeather[1][idx + 1];
    //coloredWeather[2][idx] = coloredWeather[2][idx + 1];

    red[DAYLIGHT_MODE][idx] = red[DAYLIGHT_MODE][idx+1];
    green[DAYLIGHT_MODE][idx] = green[DAYLIGHT_MODE][idx+1];
    blue[DAYLIGHT_MODE][idx] = blue[DAYLIGHT_MODE][idx+1];

    red[TEMPERATURE_MODE][idx] = red[TEMPERATURE_MODE][idx+1];
    green[TEMPERATURE_MODE][idx] = green[TEMPERATURE_MODE][idx+1];
    blue[TEMPERATURE_MODE][idx] = blue[TEMPERATURE_MODE][idx+1];

    red[POP_MODE][idx] = red[POP_MODE][idx+1];
    green[POP_MODE][idx] = green[POP_MODE][idx+1];
    blue[POP_MODE][idx] = blue[POP_MODE][idx+1];
  }
}

void convertWeatherToColor() {
  /*
    packed color is uint32_t
    r = c[16:23]
    g = c[8:15]
    b = c[0:7]
  */
  // uint32_t coloredWeather[3][NUM_LEDS+2]; // 3x50

  //byte r, g, b = 0;
  for (int idx = 0; idx < (NUM_LEDS + 2); idx++) {
    // Daylight
    //r = 0; g = 0; b = 0;
    int timeOfIDX = hours[idx / 2] * 60;
    if ((idx % 2) == 1) {
      // bottom of the hour, add 30 minutes
      timeOfIDX += 30;
    }
    if ((abs(timeOfIDX - sunrise) < 16) || (abs(timeOfIDX - sunset) < 16)) {
      // first or last light
      red[DAYLIGHT_MODE][idx] = 75;
      green[DAYLIGHT_MODE][idx] = 75;
      blue[DAYLIGHT_MODE][idx] = 200;
    } else if ((timeOfIDX > sunrise) && (timeOfIDX < sunset)) {
      // if daylight
      red[DAYLIGHT_MODE][idx] = 128;
      green[DAYLIGHT_MODE][idx] = 128;
    } else {
      // nighttime
      blue[DAYLIGHT_MODE][idx] = 75;
    }
    //coloredWeather[DAYLIGHT_MODE][idx] = (r<<16)+(g<<8)+b;

    // Temperature
    //r = 0; g = 0; b = 0;
    byte tmp = 0; // temporary not temperature
    if (temperatures[idx / 2] <= 0) {
      red[TEMPERATURE_MODE][idx] = 128;
      green[TEMPERATURE_MODE][idx] = 128;
      blue[TEMPERATURE_MODE][idx] = 255;
    } else if (temperatures[idx / 2] <= 30) {
      tmp = map(temperatures[idx / 2], 0, 30, 0, 128);
      red[TEMPERATURE_MODE][idx] = (128 - tmp);
      green[TEMPERATURE_MODE][idx] = (128 - tmp);
      blue[TEMPERATURE_MODE][idx] = 255;
    } else if (temperatures[idx / 2] <= 60) {
      tmp = map(temperatures[idx / 2], 30, 60, 0, 255);
      red[TEMPERATURE_MODE][idx] = 0;
      green[TEMPERATURE_MODE][idx] = tmp;
      blue[TEMPERATURE_MODE][idx] = (255 - tmp);
    } else if (temperatures[idx / 2] <= 90) {
      tmp = map(temperatures[idx / 2], 60, 90, 0, 255);
      red[TEMPERATURE_MODE][idx] = tmp;
      green[TEMPERATURE_MODE][idx] = (255 - tmp);
      blue[TEMPERATURE_MODE][idx] = 0;
    } else if (temperatures[idx / 2] <= 105) {
      tmp = map(temperatures[idx / 2], 90, 105, 0, 128);
      red[TEMPERATURE_MODE][idx] = (255 - tmp);
      green[TEMPERATURE_MODE][idx] = 0;
      blue[TEMPERATURE_MODE][idx] = tmp;
    } else {
      red[TEMPERATURE_MODE][idx] = 128;
      green[TEMPERATURE_MODE][idx] = 0;
      blue[TEMPERATURE_MODE][idx] = 128;
    }
    //coloredWeather[TEMPERATURE_MODE][idx] = (r<<16)+(g<<8)+b;

    // Precip
    //r = 0; g = 0; b = 0;
    red[POP_MODE][idx] = 0;
    green[POP_MODE][idx] = 0;
    blue[POP_MODE][idx] = map(pops[idx / 2], 0, 100, 0, 255);
    //coloredWeather[POP_MODE][idx] = (r<<16)+(g<<8)+b;
  }

}

// NTP Functions from TimeNTP_ESP8266WiFi.ino ===============================================
// Todo trim these down and ifdef print statements

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

// Supporting Functions =======================================================

// returns the next int
int getNextIntFromString(String str, int startIdx) {
  if ((startIdx >= str.length()) || (startIdx < 0)) {
    return -1;
  }

  char chr = 0;
  for (int idx = startIdx; idx < str.length(); idx++) {
    chr = str.charAt(idx);
    if (isDigit(chr)) {
      return str.substring(idx).toInt();
    }
  }
  return -1;
}

// Debug Functions ========================================================

void displayData() {
  Serial.println("Temperatures: ");
  for (int i = 0; i < (NUM_TEMPS - 1); i++) {
    Serial.print(temperatures[i]);
    Serial.print(", ");
  }
  Serial.println(temperatures[NUM_TEMPS - 1]);

  Serial.println("Percent Of Precip: ");
  for (int i = 0; i < (NUM_TEMPS - 1); i++) {
    Serial.print(pops[i]);
    Serial.print(", ");
  }
  Serial.println(pops[NUM_TEMPS - 1]);

  Serial.println("Hours: ");
  for (int i = 0; i < (NUM_TEMPS - 1); i++) {
    Serial.print(hours[i]);
    Serial.print(", ");
  }
  Serial.println(hours[NUM_TEMPS - 1]);

  Serial.print("Sunrise: ");
  Serial.println(sunrise);

  Serial.print("Sunset: ");
  Serial.println(sunset);
}



