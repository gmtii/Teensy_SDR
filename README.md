# Teensy_SDR
Software defined radio using Teensy 3.1, Teensy Audio shield and Softrock

This is a software defined radio using the Arduino compatible Teensy 3.1 and audio shield from PJRC. 
It takes advantage of the audio library supplied by PJRC to implement the audio signal processing chain.

Dependencies - most of these libraries are available from PJRC and can be installed with the Teensyduino package:

Audio - Teensy Audio library V1.02,
SD (not used but needed for the audio libraries to compile),
SPI,
Wire,
Metro,
Bounce,
Encoder,
SI5351 (github.com/NT7S/Si5351),
Adafruit Graphics library (adafruit.com),
Driver for your display - I used a Banggood knockoff of the AdaFruit 1.8" TFT

Changelog:
Jan 2015 - first public release, RX only
March 2015 - CW and SSB TX mode added, band swiching added, general cleanup of the code
April 9 2015 - added the driver for the Banggood 1.8" display and a slightly modded SI5351 library for the Teensy which has no eeprom
	- move these to your Arduino/libraries folder
April 13 2015 - added wiring diagram

NOTE: code compiles OK with Arduino 1.61 and Teensyduino 1.21 BUT sometimes image is not suppressed as evidenced
by signals moving both ways when you tune. Seems that if you recompile and reload once or twice its fine until
the next time you recompile and flash the code. Tool chain bug ???

TODO:
- fix software AGC which no longer works after port to audio lib 1.02
- S Meter needs improvement
- UI improvements - configuration settings etc
- clean up display code - remove direct x,y references to allow easier modifications

project blog: rheslip.blogspot.com

----

gmtii notes: 

I've modified the original pinout to get the ILI9341 working with the optimized ILI9341_t3 library and to drive the Softrock RX III ABPF using the SEL0 and SEL1 control lines; remove the ATTiny85 and solder an empty dip socket with wires to the teensy.

- SW1 - relocated to pin D2
- SW2 - relocated to pin D3

SoftRock ABPF control lines:

- SEL0 - Teensy pin D0 -> ATTiny85 pin 3
- SEL1 - Teensy pin D1 -> ATTiny85 pin 1

SoftRock SI570 (Added the si570 library from the RAdiono project to the Library directory):

- Teensy SDA ->  ATTiny85 pin 6 
- Teensy SCL ->  ATTiny85 pin 2 

ILI9341 pinout:

- TFT_DC      D20
- TFT_CS      D21
- TFT_RST     255  // 255 = unused, connect to 3.3V
- TFT_MOSI     D7
- TFT_SCLK    D14
- TFT_MISO    D12

