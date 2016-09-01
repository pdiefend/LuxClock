#include <Adafruit_NeoPixel.h>

#define NUM_TEMPS 36
#define NUM_LEDS 48
#define PIN 2

#define DAYLIGHT_MODE 0
#define TEMPERATURE_MODE 1
#define POP_MODE 2
#define TRANS_STEPS 45

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

//uint32_t coloredWeather[3][NUM_LEDS+2];
byte red[3][NUM_LEDS+2];
byte blue[3][NUM_LEDS+2];
byte green[3][NUM_LEDS+2];

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

int cycleNum = 0;

void loop() {
  // put your main code here, to run repeatedly:
  //for(int j=0; j<3; j++){

  int nextCycleNum = (cycleNum+1) % 3;
  
  
    //int nextCycleNum = (cycleNum+1) % 3;
    //byte rOld,gOld,bOld = 0;
    //byte rNew,gNew,bNew = 0;
    //int rStep, gStep, bStep = 0;
    int r,g,b = 0; 
    
  for(int jdx = 1; jdx <= TRANS_STEPS; jdx++){   
    // 0 -> NUM_LEDS
    // for each LED calculate the step increment to the next pixel
    for(int idx = 0; idx < NUM_LEDS; idx++) {
      float rStep = (red[nextCycleNum][idx] - red[cycleNum][idx]);
      rStep = rStep * (float)jdx / (float)TRANS_STEPS;

      float gStep = (green[nextCycleNum][idx] - green[cycleNum][idx]);
      gStep = gStep * (float)jdx / (float)TRANS_STEPS;

      float bStep = (blue[nextCycleNum][idx] - blue[cycleNum][idx]);
      bStep = bStep * (float)jdx / (float)TRANS_STEPS;

      r = red[cycleNum][idx] + (int)rStep;
      g = green[cycleNum][idx] + (int)gStep;
      b = blue[cycleNum][idx] + (int)bStep;
  
      //((coloredWeather[nextCycleNum][idx] & (0xff0000))>>16) - ((coloredWeather[cycleNum][idx] & (0xff0000))>>16);
      //uint32_t rOld = ((coloredWeather[cycleNum][idx] & (0xff0000))>>16);
      //rNew = (byte)(coloredWeather[nextCycleNum][idx] & (0xff0000))>>16;
      //rStep = (rNew - rOld) / TRANS_STEPS;
  
      //uint32_t gOld = ((coloredWeather[cycleNum][idx] & (0x00ff00))>>8);
      //gNew = (byte)(coloredWeather[nextCycleNum][idx] & (0x00ff00))>>8;
      //gStep = (gNew - gOld) / TRANS_STEPS;
  
      //uint32_t bOld = ((coloredWeather[cycleNum][idx] & (0x0000ff)));
      //bNew = (byte)(coloredWeather[nextCycleNum][idx] & (0x000ff));
      //bStep = (bNew - bOld) / TRANS_STEPS;

      //float gStep = ((coloredWeather[nextCycleNum][idx] & (0x00ff00))>>8) - ((coloredWeather[cycleNum][idx] & (0x00ff00))>>8);
      //gStep /= TRANS_STEPS;
      //float bStep = ((coloredWeather[nextCycleNum][idx] & (0x0000ff))) - ((coloredWeather[cycleNum][idx] & (0x0000ff)));
      //bStep /= TRANS_STEPS;
    
      //r = (int)(rOld + (rStep*jdx));
      //g = (int)(gOld + (gStep*jdx));
      //b = (int)(bOld + (bStep*jdx));

      pixels.setPixelColor(idx, pixels.Color(r,g,b));
    }
  
    pixels.show();
    
    delay(1500.0/TRANS_STEPS);
    yield();
  }  
  
cycleNum = nextCycleNum;

  
    
    if(cycleNum == DAYLIGHT_MODE){
      Serial.println("Daylight");
    } else if (cycleNum== TEMPERATURE_MODE){
      Serial.println("Temperature");
    } else if (cycleNum== POP_MODE){
      Serial.println("Precip");
    }
      
    for(int i=0;i<NUM_LEDS;i++){
      
      // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
      pixels.setPixelColor(i, pixels.Color(red[cycleNum][i], green[cycleNum][i], blue[cycleNum][i]));
      
    }
    pixels.show(); // This sends the updated pixel color to the hardware.
    delay(1000);
    yield();
    delay(1000);
    yield();
    delay(1000);
    yield();
    delay(1000);
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
  for (int idx = 0; idx < (NUM_LEDS+2); idx++) {
    // Daylight
    //r = 0; g = 0; b = 0;
    int timeOfIDX = hours[idx/2] * 60;
    if((idx % 2) == 1){
      // bottom of the hour, add 30 minutes
      timeOfIDX += 30;
    }
    if((abs(timeOfIDX - sunrise) < 16) || (abs(timeOfIDX - sunset) < 16)){
      // first or last light
      red[DAYLIGHT_MODE][idx] = 75;
      green[DAYLIGHT_MODE][idx] = 75;
      blue[DAYLIGHT_MODE][idx] = 200;
    } else if((timeOfIDX > sunrise) && (timeOfIDX < sunset)){
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
    if(temperatures[idx/2] <= 0){
      red[TEMPERATURE_MODE][idx] = 128; 
      green[TEMPERATURE_MODE][idx] = 128;
      blue[TEMPERATURE_MODE][idx] = 255;
    } else if(temperatures[idx/2] <= 30) {
      tmp = map(temperatures[idx/2], 0, 30, 0, 128);
      red[TEMPERATURE_MODE][idx] = (128-tmp);
      green[TEMPERATURE_MODE][idx] = (128-tmp);
      blue[TEMPERATURE_MODE][idx] = 255;
    } else if(temperatures[idx/2] <= 60) {
      tmp = map(temperatures[idx/2], 30, 60, 0, 255);
      red[TEMPERATURE_MODE][idx] = 0;
      green[TEMPERATURE_MODE][idx] = tmp;
      blue[TEMPERATURE_MODE][idx] = (255-tmp);
    } else if(temperatures[idx/2] <= 90) {
      tmp = map(temperatures[idx/2], 60, 90, 0, 255);
      red[TEMPERATURE_MODE][idx] = tmp;
      green[TEMPERATURE_MODE][idx] = (255-tmp);
      blue[TEMPERATURE_MODE][idx] = 0;
    } else if(temperatures[idx/2] <= 105) {
      tmp = map(temperatures[idx/2], 90, 105, 0, 128);
      red[TEMPERATURE_MODE][idx] = (255-tmp);
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
    blue[POP_MODE][idx] = map(pops[idx/2], 0, 100, 0, 255);
    //coloredWeather[POP_MODE][idx] = (r<<16)+(g<<8)+b;
  }

}
