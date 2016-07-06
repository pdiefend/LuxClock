#include <ESP8266WiFi.h>
#include "PASSWORDS.h"

#define NUM_TEMPS 36

const char request[] =
    "GET /api/" API_Key "/astronomy/hourly/q/" Zipcode ".json HTTP/1.1\r\n"
    "User-Agent: ESP8266/0.1\r\n"
    "Accept: */*\r\n"
    "Host: api.wunderground.com\r\n"
    "Connection: close\r\n"
    "\r\n";
static char respBuf[4096];
int temperatures[NUM_TEMPS];
int pops[NUM_TEMPS];
int sunrise = 0;
int sunset = 0;
byte tempidx = 0;
byte popidx = 0;

void setup() {
  Serial.begin(115200);

  Serial.print("Reset Timeout");
  for(int i = 0; i < 3; i++){
    Serial.print(".");
    delay(1000);
  }
  
  Serial.println("\n");
  
  Serial.print("Connecting");

  WiFi.begin(SSID, PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(F("."));
  }

  Serial.print("Connected, My IP is: ");
  Serial.println(WiFi.localIP());

  delay(3000);
}



void loop()
{
  // TODO check for disconnect from AP

  // Open socket
  Serial.println("Downloading Forecast");

  WiFiClient httpclient;
  if (!httpclient.connect("api.wunderground.com", 80)) {
    Serial.println("connection failed");
    while(1);
    return;
    // TODO fail more gracefully and recover
  }

  //Serial.print(request);
  httpclient.print(request);
  httpclient.flush();
  
// TODO get sunrise, sunset, cloud cover (sky)?, others?
// How should I display these. I don't want more than 3 cycles
// maybe find a way to show cloud cover and sunrise/sunset in same cycle
  
  String line = "";
  int idx = 0;
  while (httpclient.connected() || httpclient.available()) {
    line = httpclient.readStringUntil('\n');
    if(line.indexOf("temp") != -1){
      line = line.substring(23); // TODO Find a better way to find the number I want
      temperatures[tempidx] = line.toInt();
      //Serial.println(line);
      tempidx += 1;
    } else if (line.indexOf("pop") != -1){
      line = line.substring(10); // TODO Find a better way to find the number I want
      pops[popidx] = line.toInt();
      //Serial.println(line);
      popidx += 1;
    } else if (line.indexOf("sunrise") != -1){
      line = httpclient.readStringUntil('}');
      idx = line.indexOf("hour");
      line = line.substring(idx + 7); // TODO Find a better way, but this is better
      sunrise = line.toInt() * 60;

      idx = line.indexOf("minute");
      line = line.substring(idx + 9); // TODO Find a better way, but this is better
      sunrise = sunrise + line.toInt();
    } else if (line.indexOf("sunset") != -1){
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
  
  Serial.println();
  Serial.println("Closing connection\n");
  httpclient.stop();

  Serial.println("Temperatures: ");
  for(int i = 0; i < (NUM_TEMPS-1); i++){
    Serial.print(temperatures[i]);
    Serial.print(", ");
  }
  Serial.println(temperatures[NUM_TEMPS-1]);

  Serial.println("Percent Of Precip: ");
  for(int i = 0; i < (NUM_TEMPS-1); i++){
    Serial.print(pops[i]);
    Serial.print(", ");
  }
  Serial.println(pops[NUM_TEMPS-1]);

  Serial.print("Sunrise: ");
  Serial.println(sunrise);

  Serial.print("Sunset: ");
  Serial.println(sunset);
  
  Serial.println("\nEnd");

  // DEV purposes, all stop
  while(1){
    yield(); // needs to be yeild or else WDT expires
  }
  
  // Wait 5 minutes
  //delay(300000);
}

