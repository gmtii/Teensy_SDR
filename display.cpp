// code for the TFT display

#include "display.h"
#include <Audio.h>
#include "ILI9341_t3.h"
extern AudioAnalyzeFFT256  myFFT;      // FFT for Spectrum Display

#define TFT_DC      20
#define TFT_CS      21
#define TFT_RST    255  // 255 = unused, connect to 3.3V
#define TFT_MOSI     7
#define TFT_SCLK    14
#define TFT_MISO    12
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);

void setup_display(void) {
  
  // initialize the LCD display
  SPI.setMOSI(7);
  SPI.setSCK(14);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(BLACK);
  tft.setCursor(0, 220);
  tft.setTextColor(WHITE);
  tft.setTextWrap(true);
  tft.setTextSize(2);
  //tft.print("Teensy SDR -  F:");
  
  // Show mid screen tune position
  tft.drawFastVLine(160, 0,100,RED);
}

// draw the spectrum display
// this version draws 1/10 of the spectrum per call but we run it 10x the speed
// this allows other stuff to run without blocking for so long

void show_spectrum(void) {
  static int startx=0, endx;
  endx=startx+16;
  int scale=1;
//digitalWrite(DEBUG_PIN,1); // for timing measurements
  
  for (int16_t x=startx; x < endx; x+=2) 
  {
    int bar=abs(myFFT.output[x*8/10])/scale;
    if (bar >100) bar=100;
    if(x!=80)
    {
       //tft.drawFastVLine(x*2, 60-bar,bar, GREEN);
       //tft.drawFastVLine(x*2, 0, 60-bar, BLACK); 

       tft.fillRect(x*2,100-bar,4,bar,GREEN);
       tft.fillRect(x*2,0,4,100-bar,BLACK);
          
    }
  }
  startx+=16;
  if(startx >=160) startx=0;
//digitalWrite(DEBUG_PIN,0); // 
}

void show_waterfall(void) {
  // experimental waterfall display for CW -
  // this should probably be on a faster timer since it needs to run as fast as possible to catch CW edges
  //  FFT bins are 22khz/128=171hz wide 
  // cw peak should be around 11.6khz - 
  static uint16_t waterfall[80];  // array for simple waterfall display
  static uint8_t w_index=0,w_avg;
  waterfall[w_index]=0;
  for (uint8_t y=66;y<67;++y)  // sum of bin powers near cursor - usb only
      waterfall[w_index]+=(uint8_t)(abs(myFFT.output[y])); // store bin power readings in circular buffer
  waterfall[w_index]|= (waterfall[w_index]<<5 |waterfall[w_index]<<11); // make it colorful
  int8_t p=w_index;
  for (uint8_t x=158;x>0;x-=2) {
    tft.fillRect(x,65,2,4,waterfall[p]);
    if (--p<0 ) p=79;
  }
  if (++w_index >=80) w_index=0; 
}

// indicate filter bandwidth on spectrum display
void show_bandwidth(int filtermode) { 
  tft.drawFastHLine(80,101,160, BLACK); // erase old indicator
  tft.drawFastHLine(80,102,160, BLACK); // erase old indicator 
  
  switch (filtermode)	{
    case LSB_NARROW:
      tft.drawFastHLine(72+74,101,12, RED);
      tft.drawFastHLine(72+74,102,12, RED);
    break;
    case LSB_WIDE:
      tft.drawFastHLine(61+60,101,40, RED);
      tft.drawFastHLine(61+60,102,40, RED);
    break;
    case USB_NARROW:
      tft.drawFastHLine(83+80,101,12, RED);
      tft.drawFastHLine(83+80,102,12, RED);
    break;
    case USB_WIDE:
      tft.drawFastHLine(80+80,101,40, RED);
      tft.drawFastHLine(80+80,102,40, RED);
    break;
  }
}  

// show frequency step
void show_steps(String name) {
  tft.setTextColor(GREEN,BLACK); 
  tft.setCursor(0, 220);
  tft.print(name);
}

// show radio mode
void show_radiomode(String mode) { 
  tft.setTextColor(GREEN,BLACK);
  tft.setCursor(50, 200);
  tft.print(mode);
}  

void show_band(String bandname) {  // show band
  tft.setTextColor(YELLOW,BLACK);
  tft.setCursor(0, 200);
  tft.print(bandname);
}

// show frequency
void show_frequency(long int freq) { 
    char string[80];   // print format stuff
    sprintf(string,"%d.%03d.%03d",freq/1000000,(freq-freq/1000000*1000000)/1000,
          freq%1000 );
    tft.setCursor(140, 210);
    tft.setTextColor(WHITE,BLACK);
    tft.setTextSize(3);
    tft.print(string);
    tft.setTextSize(2); 
}    



