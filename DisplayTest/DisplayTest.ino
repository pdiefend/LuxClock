#include <Adafruit_NeoPixel.h>

#define NUM_TEMPS 36
#define NUM_LEDS 48
#define PIN 2

#define DAYLIGHT_MODE 0
#define TEMPERATURE_MODE 1
#define POP_MODE 2

void convertWeatherToColor();

// Globals
int temperatures[NUM_TEMPS] = { -5,   0,   5,  10,  15,  20, 
                                25,  30,  35,  40,  45,  50,
                                55,  60,  65,  70,  75,  80,
                                85,  90,  95, 100, 105, 110,
                               115,   0,   0,   0,   0,   0,
                                 0,   0,   0,   0,   0,   0};
int pops[NUM_TEMPS] = {0,  5,  10, 15, 20, 25,
                      30, 35,  40, 45, 50, 55,
                      60, 65,  70, 75, 80, 85,
                      90, 95, 100,  80,  60,  40,
                       20,  0,   0,  0,  0,  0,
                       0,  0,   0,  0,  0,  0};
int hours[NUM_TEMPS] = { 0,  1,  2,  3,  4,  5,
                         6,  7,  8,  9, 10, 11,
                        12, 13, 14, 15, 16, 17,
                        18, 19, 20, 21, 22, 23,
                         0,  1,  2,  3,  4,  5,
                         6,  7,  8,  9, 10, 11};
int sunrise = 366;
int sunset = 1185;

uint32_t coloredWeather[3][NUM_LEDS+2];

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(60, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);

  Serial.print("Reset Timeout");
  for (int i = 0; i < 3; i++) {
    Serial.print(".");
    delay(1000);
  }

  pixels.begin();
  pixels.setBrightness(50);

  // zero out my 1m (60 pixel) test strip
for(int i=0;i<60;i++){
      
      // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
      pixels.setPixelColor(i, 0);
      
    }
    pixels.show();
  
  convertWeatherToColor();
}

void loop() {
  // put your main code here, to run repeatedly:
  for(int j=0; j<3; j++){
    
    if(j == DAYLIGHT_MODE){
      Serial.println("Daylight");
    } else if (j== TEMPERATURE_MODE){
      Serial.println("Temperature");
    } else if (j== POP_MODE){
      Serial.println("Precip");
    }
    
    for(int i=0;i<NUM_LEDS;i++){
      
      // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
      pixels.setPixelColor(i, coloredWeather[j][i]);
      
    }
    pixels.show(); // This sends the updated pixel color to the hardware.
    delay(3000);
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
