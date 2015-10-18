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
  int scale = 2;
  
  for (int16_t x = startx; x < endx; x += 2)
  {
    int bar = abs(myFFT.output[x * 8 / 10]) / scale;
    if (bar > 100) bar = 100;
    if (x != 80)
    {
      tft.fillRect(x * 2, 100 - bar, 3, bar, CYAN);
      tft.fillRect(x * 2, 0, 3, 100 - bar, BLACK);
    }
  }
  startx += 16;
  if (startx >= 160) startx = 0;

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

    uint8_t value = map(waterfall[xx][w_index], 0, 200, 0, 89);
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

  tft.fillRect(0, 101, 319, 4, BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 210);
  tft.setTextColor(GREEN, BLACK);

  switch (filtermode)	{

    case LSB_NARROW:

      tft.fillRect(146, 101, 15, 4, RED);

      tft.print("BW: 700 Hz ");
      break;

    case LSB_WIDE:

      tft.fillRect(121, 101, 40, 4, RED);
      tft.print("BW: 2.7 kHz");
      break;

    case USB_NARROW:

      tft.fillRect(160, 101, 15, 4, RED);
      tft.print("BW: 700 Hz ");
      break;

    case USB_WIDE:

      tft.fillRect(160, 101, 40, 4, RED);
      tft.print("BW: 2.7 kHz");
      break;

    case DSB:

      tft.fillRect(121, 101, 80, 4, RED);
      tft.print("BW: 5.4 kHz");

      break;

    case USB_3700:

      tft.fillRect(160, 101, 110, 4, RED);
      tft.print("BW: 3.7 kHz");
      break;

    case USB_1700:

      tft.fillRect(160, 101, 36, 4, RED);
      tft.print("BW: 1.7 kHz");
      break;

    case LSB_3700:

      tft.fillRect(51, 101, 110, 4, RED);
      tft.print("BW: 3.7 kHz");
      break;

    case LSB_1700:

      tft.fillRect(125, 101, 36, 4, RED);
      tft.print("BW: 1.7 kHz");
      break;





  }
  tft.setTextSize(2);
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
  tft.setCursor(170, 200);
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
  tft.setCursor(200, 220);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);
  tft.print(string);
  tft.setTextSize(2);
  //tft.setFont(Arial_14);
}

