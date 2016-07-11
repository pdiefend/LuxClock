/*

   Plan:


   List of higher-level tasks:
    1) Figure out how to perform timed tasks
      a) Update weather forecast at 45mins
      b) Update display at 0mins and 30mins
    2) Merge NTP into weather code
      a) add way to keep trying until time RX'd first time
      b) count sequential failures and retry if failed x times in a row.
    3) Set up NTP and weather updates to reconnect to Wifi if lost
    4) Parse hour data from weather forecast to sync DST
    5) Figure out display cycle timing
      a) propbably need 2 timers or one and loop
      b) one timer for next event, need FSM
      c) other timer for cycling LEDS, less complicated FSM
      d) actually timers aren't necessary. We can use an FSM in loop. Less overhead, simpiler
    6) Merge LED code in rest of code

*/

#include <TimeLib.h>
//#include <TimeAlarms.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "PASSWORDS.h"

// Definitions and constants
#define NUM_TEMPS 36
#define DEBUGGING
#define Weather_DL_Minute 22
#define LED_Cycle_Time 5 // can't be 1

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

int temperatures[NUM_TEMPS];
int pops[NUM_TEMPS];
int sunrise = 0;
int sunset = 0;
byte tempidx = 0;
byte popidx = 0;
WiFiUDP Udp;


// Forward Declarations
void updateWeatherForecast();
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
void updateDisplay();

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

  //  Alarm.timerRepeat(5, updateDisplay);
  /*
    while(timeStatus() == timeNotSet){
      delay(10000);
      setTime(getNtpTime());
      //yield();
    }
  */
  delay(3000);
}


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
    updateWeatherForecast();
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

  // Wait 5 minutes
  //delay(300000); // this will trigger the WDT
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
      line = line.substring(23); // TODO Find a better way to find the number I want
      temperatures[tempidx] = line.toInt();
      //Serial.println(line);
      tempidx += 1;
    } else if (line.indexOf("pop") != -1) {
      line = line.substring(10); // TODO Find a better way to find the number I want
      pops[popidx] = line.toInt();
      //Serial.println(line);
      popidx += 1;
    } else if (line.indexOf("sunrise") != -1) {
      line = httpclient.readStringUntil('}');
      idx = line.indexOf("hour");
      line = line.substring(idx + 7); // TODO Find a better way, but this is better
      sunrise = line.toInt() * 60;

      idx = line.indexOf("minute");
      line = line.substring(idx + 9); // TODO Find a better way, but this is better
      sunrise = sunrise + line.toInt();
    } else if (line.indexOf("sunset") != -1) {
      line = httpclient.readStringUntil('}');
      idx = line.indexOf("hour");
      line = line.substring(idx + 7); // TODO Find a better way, but this is better
      sunset = line.toInt() * 60;

      idx = line.indexOf("minute");
      line = line.substring(idx + 9); // TODO Find a better way, but this is better
      sunset = sunset + line.toInt();
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

// NTP Functions from TimeNTP_ESP8266WiFi.ino ===============================================
// Todo trim these down and ifdef print statement

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

  Serial.print("Sunrise: ");
  Serial.println(sunrise);

  Serial.print("Sunset: ");
  Serial.println(sunset);
}
#endif


