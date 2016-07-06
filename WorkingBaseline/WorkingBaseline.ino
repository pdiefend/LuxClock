#include <ESP8266WiFi.h>
#include "PASSWORDS.h"

const char request[] =
    "GET /api/" API_Key "/astronomy/hourly/q/" Zipcode ".json HTTP/1.1\r\n"
    "User-Agent: ESP8266/0.1\r\n"
    "Accept: */*\r\n"
    "Host: api.wunderground.com\r\n"
    "Connection: close\r\n"
    "\r\n";

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

static char respBuf[4096];

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
  
  
  // bool skip_headers = true;
  String line = "";
  while (httpclient.connected() || httpclient.available()) {
    line = httpclient.readStringUntil('\n');
    if(line.indexOf("temp") != -1){
      Serial.println(line);
    }
  }

  line = "";
  
  Serial.println();
  Serial.println("closing connection");
  httpclient.stop();

  // DEV purposes, all stop
  while(1){
    yield();
  }
  
  // Wait 5 minutes
  //delay(300000);
}

