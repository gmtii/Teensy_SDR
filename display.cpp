// code for the TFT display

#include "display.h"
#include <Audio.h>
#include "ILI9341_t3.h"
#include "font_Arial.h"
#include "fft_iq.h"

extern AudioAnalyzeFFT256IQ     FFT256IQ;
extern AudioAnalyzeFFT256       demod_FFT;

#define TFT_DC      21
#define TFT_CS      20
#define TFT_RST    255  // 255 = unused, connect to 3.3V
#define TFT_MOSI     7
#define TFT_SCLK    14
#define TFT_MISO    12

#define WATERFALL_SIZE 69

#define WATERFALL_BASE 121+WATERFALL_SIZE

void box (uint8_t number, String text, bool selected );
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);

const uint16_t gradient[] = {
  0x0
  , 0x1
  , 0x2
  , 0x3
  , 0x4
  , 0x5
  , 0x6
  , 0x7
  , 0x8
  , 0x9
  , 0x10
  , 0x1F
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
  , 0xF87E
  , 0xF83E
  , 0xF83E
  , 0xF83E
  , 0xF83E
  , 0xF85E
  , 0xF85E
  , 0xF85E
  , 0xF85E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
  , 0xF87E
};
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

  //tft.fillRect(160, 0, 4, 105, RED);

  tft.drawFastHLine(63, 120, 319, WHITE);
  tft.drawFastHLine(0, 190, 319, WHITE);

  tft.drawFastVLine(192, 190, 50, WHITE);
  tft.drawFastVLine(63, 0, 190, WHITE);

  tft.drawFastVLine(63, 117, 3, WHITE);
  tft.drawFastVLine(128, 117, 3, WHITE);
  tft.drawFastVLine(192, 114, 6, WHITE);
  tft.drawFastVLine(256, 117, 3, WHITE);
  tft.drawFastVLine(319, 117, 3, WHITE);

  tft.drawFastHLine(0, 0, 63, WHITE);
  tft.drawFastHLine(0, 38, 63, WHITE);
  tft.drawFastHLine(0, 76, 63, WHITE);
  tft.drawFastHLine(0, 114, 63, WHITE);
  tft.drawFastHLine(0, 152, 63, WHITE);

}

// draw the spectrum display
// this version draws 1/10 of the spectrum per call but we run it 10x the speed
// this allows other stuff to run without blocking for so long

void show_spectrum(void) {

  static int startx = 0, endx;
  endx = startx + 64;
  int bar;

  for (int16_t x = startx; x < endx; x += 1)
  {
    bar = abs(FFT256IQ.output[x]); // / scale;
    if (bar > 100) bar = 98;
    if (bar < 2) bar = 2;
    tft.drawFastVLine(x + 64 , 98 - bar, 2, 0x37F);
    tft.drawFastVLine(x + 64 , 100 - bar, bar - 2, 0x11F);
    tft.drawFastVLine(x + 64 , 0, 98 - bar, 0x3);
  }
  startx += 64;
  if (startx >= 319) startx = 0;
}

void show_spectrum_base(void) {
  static int startx = 0, endx;
  endx = startx + 64;
  int scale = 1;

  for (int16_t x = startx; x < endx; x += 1)
  {
    int bar = abs(demod_FFT.output[x * 8 / 10]) / scale;
    //if (bar > 100) bar = 100;
    tft.fillRect(x * 2 + 64 , 100 - bar, 2, bar, GREEN);
    tft.fillRect(x * 2 + 64, 0, 2, 100 - bar, BLACK);
  }
  startx += 64;
  if (startx >= 192) startx = 0;
}

void show_waterfall(void) {

  static uint16_t waterfall[128][WATERFALL_SIZE];  // 64x60 array for simple waterfall display
  static uint8_t w_index = 0;
  uint8_t value, p, x, xx = 0;
  uint16_t xx_temp, xx_temp2;

  while (xx < 127 )
  {
    waterfall[xx][w_index] = 1;

    for (uint8_t y = xx * 2; y < (xx * 2 + 1); y++)               // sum of bin powers near cursor - usb only
      waterfall[xx][w_index] += (uint8_t)(abs(FFT256IQ.output[y]));  // store bin power readings in circular buffer

    uint8_t value = map(waterfall[xx][w_index], 0, 125, 0, 80);
    waterfall[xx][w_index] = gradient[ value ];

    int8_t p = w_index;
    xx_temp = xx * 2 + 64;
    uint8_t x = WATERFALL_SIZE;

    while (x > 0)
    {
      tft.drawFastHLine(xx_temp, WATERFALL_BASE - x, 2, waterfall[xx][p]);
      if (--p < 0 ) p = WATERFALL_SIZE - 1;
      x--;
    }
    xx++;
  }
  if (++w_index >= WATERFALL_SIZE) w_index = 0;
}

void show_waterfall_base(void) {


  static uint16_t waterfall[64][WATERFALL_SIZE];  // 64x60 array for simple waterfall display
  static uint8_t w_index = 0;
  uint8_t value, p, x, xx = 0;
  uint16_t xx_temp, xx_temp2;


  while (xx < 64 )
  {

    waterfall[xx][w_index] = 1;

    for (uint8_t y = xx * 2; y < (xx * 2 + 1); y++)               // sum of bin powers near cursor - usb only
      waterfall[xx][w_index] += (uint8_t)(abs(demod_FFT.output[y]));  // store bin power readings in circular buffer

    uint8_t value = map(waterfall[xx][w_index], 0, 200, 0, 80);
    waterfall[xx][w_index] = gradient[ value ];

    int8_t p = w_index;
    xx_temp = xx * 5 + 64;
    uint8_t x = WATERFALL_SIZE;

    for (uint8_t x = WATERFALL_SIZE; x > 0; x-- ) {
      tft.drawFastHLine(xx_temp, WATERFALL_BASE - x, 5, waterfall[xx][p]);
      if (--p < 0 ) p = WATERFALL_SIZE - 1;
    }

    xx++;

  }
  if (++w_index >= WATERFALL_SIZE) w_index = 0;

}

// indicate filter bandwidth on spectrum display
void show_bandwidth(int filtermode, uint16_t IF_FREQ) {

  tft.fillRect(192, 101, 127, 4, BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 210);
  tft.setTextColor(GREEN, BLACK);

  uint16_t posX = map(IF_FREQ, 0, 22000, 192, 319);

  switch (filtermode)  {

    case LSB_NARROW:
      tft.fillRect(posX - 5, 101, 5, 4, RED);
      tft.print("BW: 700 Hz ");
      break;
    case USB_NARROW:
      tft.fillRect(posX, 101, 5, 4, RED);
      tft.print("BW: 700 Hz ");
      break;

    case LSB_WIDE:
      tft.fillRect(posX - 20, 101, 20, 4, RED);
      tft.print("BW: 2.7 kHz");
      break;

    case USB_WIDE:
      tft.fillRect(posX, 101, 20, 4, RED);
      tft.print("BW: 2.7 kHz");
      break;

    case DSB:
      tft.fillRect(posX - 26, 101, 52, 4, RED);
      tft.print("BW: 7.4 kHz");
      break;

    case USB_3700:
      tft.fillRect(posX, 101, 26, 4, RED);
      tft.print("BW: 3.7 kHz");
      break;
    case LSB_3700:
      tft.fillRect(posX - 26, 101, 26, 4, RED);
      tft.print("BW: 3.7 kHz");
      break;

    case USB_1700:
      tft.fillRect(posX, 101, 12, 4, RED);
      tft.print("BW: 1.7 kHz");
      break;
    case LSB_1700:
      tft.fillRect(posX - 12, 101, 12, 4, RED);
      tft.print("BW: 1.7 kHz");
      break;

    case AM_BPF:
      tft.fillRect(160, 101, 64, 4, RED);
      tft.print("BW: 10 kHz");
      break;



  }
  tft.setTextSize(2);
}

// show frequency step
void show_steps(String name) {
  tft.setTextSize(1);
  tft.setTextColor(GREEN, BLACK);
  tft.setCursor(60, 220);
  tft.print(name);
  tft.setTextSize(2);
}

// show radio mode
void show_radiomode(String mode) {
  box (0, mode, 0 );
}

void show_band(String bandname) {  // show band
  box (1, bandname, 0 );
}

// show frequency
void show_frequency(long int freq) {

  char string[20];   // print format stuff

  if (freq < 10e6)
    sprintf(string, " %d.%03d.%03d", freq / 1000000, (freq - freq / 1000000 * 1000000) / 1000,
            freq % 1000 );
  else
    sprintf(string, "%d.%03d.%03d", freq / 1000000, (freq - freq / 1000000 * 1000000) / 1000,
            freq % 1000 );

  //tft.setFont(Arial_14);
  tft.setCursor(200, 220);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);
  tft.print(string);
  tft.setTextSize(2);
  //tft.setFont(Arial_14);
}

void box (uint8_t number, String text, bool selected )
{

  uint8_t posY = 38 * number + 1;

  if (selected)
  {
    tft.setTextColor(BLACK, ILI9341_CYAN);
    tft.fillRect(1, posY, 62, 37, ILI9341_CYAN);

  }
  else {
    tft.setTextColor(WHITE, BLACK);
    tft.fillRect(1, posY, 62, 37, BLACK);
  }

  if ( text.length() < 4)
    tft.setCursor(10, posY + 10);
  else
    tft.setCursor(5, posY + 10);

  tft.setFont(Arial_14);

  tft.setTextSize(2);
  tft.print(text);
  tft.setFontAdafruit();
}


