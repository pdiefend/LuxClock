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
#include "PASSWORDS.h"

// Definitions and constants
#define NUM_TEMPS 36
#define NUM_LEDS 48
#define DEBUGGING
#define Weather_DL_Minute 56
#define LED_Cycle_Time 5 // can't be 1
#define DAYLIGHT_MODE 0
#define TEMPERATURE_MODE 1
#define POP_MODE 2

#define NTP_PACKET_SIZE 48

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
uint32_t coloredWeather[3][NUM_LEDS+2];


// Forward Declarations
void updateWeatherForecast();
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
void updateDisplay();
int getNextIntFromString(String str, int startIdx);
void convertWeatherToColor();


#ifdef DEBUGGING
void displayData();
#endif


void setup() {
  Serial.begin(115200);

  Serial.print("Reset Timeout");
  for (int i = 0; i < 3; i++) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\n");

  Serial.print("Connecting");

  WiFi.begin(mySSID, myPASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(F("."));
  }

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
  delay(3000);
}


// Scheduling flags
byte tempFlag = 0;
byte WeatherDataFresh = 0;
byte LEDs_Advanced = 0;

void loop() {

  // updateWeatherForecast();
#ifdef DEBUGGING
  //displayData();
#endif

  // cycle display modes
  if (((second() % LED_Cycle_Time) == 0) && tempFlag == 0) {
    updateDisplay();
    Serial.printf("%02d", hour());
    Serial.print(":");
    Serial.printf("%02d", minute());
    Serial.print(":");
    Serial.printf("%02d", second());
    Serial.println();
    tempFlag = 1;

#ifdef DEBUGGING
    //displayData();
#endif

  } else if ((second() % LED_Cycle_Time) == 1) {
    tempFlag = 0;
  }

  // Download Weather Data
  if (minute() == Weather_DL_Minute && WeatherDataFresh == 0) {
    //updateWeatherForecast();
    WeatherDataFresh = 1;

#ifdef DEBUGGING
    displayData();
#endif

  } else if (minute() == (Weather_DL_Minute + 1)) {
    WeatherDataFresh = 0;
  }

  // advance LEDs
  if (((minute() == 0) || (minute() == 30)) && LEDs_Advanced == 0) {
    LEDs_Advanced = 1;
    advanceDisplay();
    // advance the half hour LEDs
  } else if ((minute() == 1) || (minute() == 31)) {
    LEDs_Advanced = 0;
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

#ifdef DEBUGGING
  Serial.println("Downloading Forecast");
#endif

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
      /*
        line = line.substring(23); // TODO Find a better way to find the number I want
        temperatures[tempidx] = line.toInt();
        //Serial.println(line);
      */
      temperatures[tempidx] = getNextIntFromString(line, line.indexOf("english"));
      tempidx += 1;
    } else if (line.indexOf("pop") != -1) {
      /*line = line.substring(10); // TODO Find a better way to find the number I want
        pops[popidx] = line.toInt();
        //Serial.println(line);
      */
      pops[popidx] = getNextIntFromString(line, line.indexOf("pop"));
      popidx += 1;
    } else if (line.indexOf("hour_padded") != -1) {
      /*
        line = line.substring(11); // TODO Find a better way to find the number I want
        hours[hoursidx] = line.toInt();
        //Serial.println(line);
      */
      hours[hoursidx] = getNextIntFromString(line, line.indexOf("hour"));
      hoursidx += 1;


    } else if (line.indexOf("sunrise") != -1) {
      line = httpclient.readStringUntil('}');
      /*
        idx = line.indexOf("hour");
        line = line.substring(idx + 7); // TODO Find a better way, but this is better
        sunrise = line.toInt() * 60;

        idx = line.indexOf("minute");
        line = line.substring(idx + 9); // TODO Find a better way, but this is better
        sunrise = sunrise + line.toInt();
      */
      sunrise = getNextIntFromString(line, line.indexOf("hour")) * 60;
      sunrise += getNextIntFromString(line, line.indexOf("minute"));

    } else if (line.indexOf("sunset") != -1) {
      line = httpclient.readStringUntil('}');
      /*
        idx = line.indexOf("hour");
        line = line.substring(idx + 7); // TODO Find a better way, but this is better
        sunset = line.toInt() * 60;

        idx = line.indexOf("minute");
        line = line.substring(idx + 9); // TODO Find a better way, but this is better
        sunset = sunset + line.toInt();
      */
      sunset = getNextIntFromString(line, line.indexOf("hour")) * 60;
      sunset += getNextIntFromString(line, line.indexOf("minute"));
    }
  }

  tempidx = 0;
  popidx = 0;
  line = "";

#ifdef DEBUGGING
  Serial.println();
  Serial.println("Closing connection\n");
#endif

  httpclient.stop();
}

// Display Functions ===============================================================

void updateDisplay() {
  // update LED Display
}

void advanceDisplay() {
  // advance LED Display
}

void convertWeatherToColor() {
  /*
    packed color is uint32_t
    r = c[16:23]
    g = c[8:15]
    b = c[0:7]
  */
  // uint32_t coloredWeather[3][NUM_LEDS+2]; // 3x50

  byte r, g, b = 0;
  for (int idx = 0; idx < (NUM_LEDS+2); idx++) {
    // Daylight
    r = 0; g = 0; b = 0;
    int timeOfIDX = hours[idx/2] * 60;
    if((idx % 2) == 1){
      // bottom of the hour, add 30 minutes
      timeOfIDX += 30;
    }
    if((abs(timeOfIDX - sunrise) < 16) || (abs(timeOfIDX - sunset) < 16)){
      // first or last light
      r = 75;
      g = 75;
      b = 200;
    } else if((timeOfIDX > sunrise) && (timeOfIDX < sunset)){
      // if daylight
      r = 128;
      g = 128;
    } else {
      // nighttime
      b = 75;
    }
    coloredWeather[DAYLIGHT_MODE][idx] = (r<<16)+(g<<8)+b;

    // Temperature
    r = 0; g = 0; b = 0;
    byte tmp = 0; // temporary not temperature
    if(temperatures[idx/2] <= 0){
      r=128; g=128; b=255;
    } else if(temperatures[idx/2] <= 30) {
      tmp = map(temperatures[idx/2], 0, 30, 0, 128);
      r = (128-tmp); g = (128-tmp); b = 255;
    } else if(temperatures[idx/2] <= 60) {
      tmp = map(temperatures[idx/2], 30, 60, 0, 255);
      g = tmp; b = (255-tmp);
    } else if(temperatures[idx/2] <= 90) {
      tmp = map(temperatures[idx/2], 60, 90, 0, 255);
      r = tmp; g = (255-tmp);
    } else if(temperatures[idx/2] <= 105) {
      tmp = map(temperatures[idx/2], 90, 105, 0, 128);
      r = (255-tmp); b = tmp;
    } else {
      r = 128; b = 128;
    }
    coloredWeather[TEMPERATURE_MODE][idx] = (r<<16)+(g<<8)+b;

    // Precip
    r = 0; g = 0; b = 0;
    b = map(pops[idx/2], 0, 100, 0, 255);
    coloredWeather[POP_MODE][idx] = (r<<16)+(g<<8)+b;
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

#ifdef DEBUGGING
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
#endif


