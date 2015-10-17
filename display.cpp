// code for the TFT display

#include "display.h"
#include <Audio.h>
#include "ILI9341_t3.h"
#include "font_Arial.h"

extern AudioAnalyzeFFT256  myFFT;      // FFT for Spectrum Display

#define TFT_DC      21
#define TFT_CS      20
#define TFT_RST    255  // 255 = unused, connect to 3.3V
#define TFT_MOSI     7
#define TFT_SCLK    14
#define TFT_MISO    12

ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);

static const uint16_t gradient[] = {

  0x1F
  , 0x1F
  , 0x1F
  , 0x1F
  , 0x1F
  , 0x1F
  , 0x1F
  , 0x1F
  , 0x9F
  , 0x11F
  , 0x19F
  , 0x23F
  , 0x2BF
  , 0x33F
  , 0x3BF
  , 0x43F
  , 0x4BF
  , 0x53F
  , 0x5BF
  , 0x63F
  , 0x6BF
  , 0x73F
  , 0x7FE
  , 0x7FA
  , 0x7F5
  , 0x7F0
  , 0x7EB
  , 0x7E6
  , 0x7E2
  , 0x17E0
  , 0x3FE0
  , 0x67E0
  , 0x8FE0
  , 0xB7E0
  , 0xD7E0
  , 0xFFE0
  , 0xFFC0
  , 0xFF80
  , 0xFF20
  , 0xFEE0
  , 0xFE80
  , 0xFE40
  , 0xFDE0
  , 0xFDA0
  , 0xFD40
  , 0xFD00
  , 0xFCA0
  , 0xFC60
  , 0xFC00
  , 0xFBC0
  , 0xFB60
  , 0xFB20
  , 0xFAC0
  , 0xFA80
  , 0xFA20
  , 0xF9E0
  , 0xF980
  , 0xF940
  , 0xF8E0
  , 0xF8A0
  , 0xF840
  , 0xF800
  , 0xF802
  , 0xF804
  , 0xF806
  , 0xF808
  , 0xF80A
  , 0xF80C
  , 0xF80E
  , 0xF810
  , 0xF812
  , 0xF814
  , 0xF816
  , 0xF818
  , 0xF81A
  , 0xF81C
  , 0xF81E
  , 0xF81E
  , 0xF81E
  , 0xF81E
  , 0xF83E
  , 0xF83E
  , 0xF83E
  , 0xF83E
  , 0xF85E
  , 0xF85E
  , 0xF85E
  , 0xF85E
  , 0xF87E
};




uint16_t color_scale ( uint16_t value );

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
  tft.setCursor(0, 150);
  //tft.setFont(Arial_14);
  //tft.print("Teensy SDR - Radio");

  // Show mid screen tune position

  tft.fillRect(160, 0, 4, 105, RED);

  tft.drawFastHLine(0, 120, 319, WHITE);
  tft.drawFastHLine(0, 190, 319, WHITE);
  tft.drawFastVLine(155, 190, 50, WHITE);


  tft.setTextSize(1);

  tft.setCursor(0, 110);
  tft.print("-10");

  tft.setCursor(79, 110);
  tft.print("-5");

  tft.setCursor(160 , 110);
  tft.print("0");

  tft.setCursor(239 , 110);
  tft.print("+5");

  tft.setCursor(300 , 110);
  tft.print("+10");

  tft.setTextSize(2);


}

// draw the spectrum display
// this version draws 1/10 of the spectrum per call but we run it 10x the speed
// this allows other stuff to run without blocking for so long

void show_spectrum(void) {
  static int startx = 0, endx;
  endx = startx + 16;
  int scale = 1;
  //digitalWrite(DEBUG_PIN,1); // for timing measurements

  for (int16_t x = startx; x < endx; x += 2)
  {
    int bar = abs(myFFT.output[x * 8 / 10]) / scale;
    if (bar > 80) bar = 100;
    if (x != 80)
    {
      //tft.drawFastVLine(x*2, 60-bar,bar, GREEN);
      //tft.drawFastVLine(x*2, 0, 60-bar, BLACK);

      tft.fillRect(x * 2, 100 - bar, 4, bar, CYAN);
      tft.fillRect(x * 2, 0, 4, 100 - bar, BLACK);

    }
  }
  startx += 16;
  if (startx >= 160) startx = 0;
  //digitalWrite(DEBUG_PIN,0); //
}

void show_waterfall_cw(void) {
  // experimental waterfall display for CW -
  // this should probably be on a faster timer since it needs to run as fast as possible to catch CW edges
  //  FFT bins are 22khz/128=171hz wide
  // cw peak should be around 11.6khz -
  static uint16_t waterfall[80];  // array for simple waterfall display
  static uint8_t w_index = 0, w_avg;
  waterfall[w_index] = 0;
  for (uint8_t y = 66; y < 67; ++y) // sum of bin powers near cursor - usb only
    waterfall[w_index] += (uint8_t)(abs(myFFT.output[y])); // store bin power readings in circular buffer
  waterfall[w_index] |= (waterfall[w_index] << 5 | waterfall[w_index] << 11); // make it colorful
  int8_t p = w_index;
  for (uint8_t x = 80; x > 0; x -= 1) {
    tft.fillRect(160, 200 - x, 2, 4, waterfall[p]);
    if (--p < 0 ) p = 79;
  }
  if (++w_index >= 80) w_index = 0;
}


void show_waterfall(void) {


  static uint16_t waterfall[64][50];  // 64x60 array for simple waterfall display
  static uint8_t w_index = 0;
  uint8_t xx = 0;

  while (xx < 64 )
  {

    waterfall[xx][w_index] = 1;

    for (uint8_t y = xx * 2; y < (xx * 2 + 1); y++)               // sum of bin powers near cursor - usb only
      waterfall[xx][w_index] += (uint8_t)(abs(myFFT.output[y]));  // store bin power readings in circular buffer

    uint8_t value = map(waterfall[xx][w_index], 0, 150, 0, 89);
    waterfall[xx][w_index] = gradient[value];

    int8_t p = w_index;

    for (uint8_t x = 50; x > 0; x-- ) {
      tft.drawFastHLine(xx * 5, 171 - x, 5, waterfall[xx][p]);
      if (--p < 0 ) p = 49;
    }

    xx++;

  }
  if (++w_index >= 50) w_index = 0;

}

// indicate filter bandwidth on spectrum display
void show_bandwidth(int filtermode) {
  //tft.drawFastHLine(80, 101, 160, BLACK); // erase old indicator
  //tft.drawFastHLine(80, 102, 160, BLACK); // erase old indicator
  tft.fillRect(120, 101, 80, 4, BLACK);

  switch (filtermode)	{
    case LSB_NARROW:
      //tft.drawFastHLine(72 + 74, 101, 12, RED);
      //tft.drawFastHLine(72 + 74, 102, 12, RED);
      tft.fillRect(146, 101, 12, 4, RED);
      break;
    case LSB_WIDE:
      //tft.drawFastHLine(61 + 60, 101, 40, RED);
      //tft.drawFastHLine(61 + 60, 102, 40, RED);
      tft.fillRect(121, 101, 40, 4, RED);
      break;
    case USB_NARROW:
      //tft.drawFastHLine(83 + 80, 101, 12, RED);
      //tft.drawFastHLine(83 + 80, 102, 12, RED);
      tft.fillRect(163, 101, 12, 4, RED);
      break;
    case USB_WIDE:
      //tft.drawFastHLine(80 + 80, 101, 40, RED);
      //tft.drawFastHLine(80 + 80, 102, 40, RED);
      tft.fillRect(160, 101, 40, 4, RED);
      break;
  }
}

// show frequency step
void show_steps(String name) {
  tft.setTextSize(1);
  tft.setTextColor(GREEN, BLACK);
  tft.setCursor(100, 200);
  tft.print(name);
  tft.setTextSize(2);
}

// show radio mode
void show_radiomode(String mode) {
  tft.setTextSize(1);
  tft.setTextColor(GREEN, BLACK);
  tft.setCursor(0, 200);
  tft.print(mode);
  tft.setTextSize(2);
}

void show_band(String bandname) {  // show band
  tft.setTextSize(1);
  tft.setTextColor(YELLOW, BLACK);
  tft.setCursor(50, 200);
  tft.print(bandname);
  tft.setTextSize(2);
}

// show frequency
void show_frequency(long int freq) {
  char string[80];   // print format stuff

  if (freq < 10e6)
    sprintf(string, " %d.%03d.%03d", freq / 1000000, (freq - freq / 1000000 * 1000000) / 1000,
            freq % 1000 );
  else
    sprintf(string, "%d.%03d.%03d", freq / 1000000, (freq - freq / 1000000 * 1000000) / 1000,
            freq % 1000 );

  //tft.setFont(Arial_28);
  tft.setCursor(0, 220);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);
  tft.print(string);
  tft.setTextSize(2);
  //tft.setFont(Arial_14);
}

