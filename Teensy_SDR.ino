/* simple software define radio using the Softrock transceiver
 * the Teensy audio shield is used to capture and generate 16 bit audio
 * audio processing is done by the Teensy 3.1
 * simple UI runs on a 160x120 color TFT display - AdaFruit or Banggood knockoff which has a different LCD controller
 * Copyright (C) 2014, 2015  Rich Heslip rheslip@hotmail.com
 * History:
 * 4/14 initial version by R Heslip VE3MKC
 * 6/14 Loftur E. Jónasson TF3LJ/VE2LJX - filter improvements, inclusion of Metro, software AGC module, optimized audio processing, UI changes
 * 1/15 RH - added encoder and SI5351 tuning library by Jason Milldrum <milldrum@gmail.com>
 *    - added HW AGC option which uses codec AGC module
 *    - added experimental waterfall display for CW
 * 3/15 RH - updated code to Teensyduino 1.21 and audio lib 1.02
 *    - added a lot of #defines to neaten up the code
 *    - added another summer at output - switches audio routing at runtime, provides a nice way to adjust I/Q balance and do AGC/ALC
 *    - added CW I/Q oscillators for CW transmit mode
 *    - added SSB and CW transmit
 *    - restructured the code to facilitate TX/RX switching
 * Todo:
 * clean up some more of the hard coded HW and UI stuff
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//#define SI570           // For Softrock RX III
#define SI5351

static uint8_t abpf = 1;

#include <Metro.h>
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <Encoder.h>
#include <Bounce2.h>
#include "ILI9341_t3.h"
#include <SPI.h>
#include "filters.h"
#include "display.h"
#include "SerialFlash.h"
#include "LiquidCrystal_I2C.h"

#define PCF8575_ADDRESS  0x22
#define LCD_ADDRESS  0x20

LiquidCrystal_I2C lcd(LCD_ADDRESS, 4, 5, 6, 0, 1, 2, 3, 7, NEGATIVE);

#ifdef SI570

#include "Si570.h"
#define SI570_I2C_ADDRESS 0x55
#endif


#ifdef SI5351
#include <si5351.h>
#endif

float s_old = 0;

void setband(unsigned long freq);

extern void agc(void);      // RX agc function
extern void setup_display(void);
extern void show_spectrum(void);  // spectrum display draw
extern void show_waterfall(void); // waterfall display
extern void show_bandwidth(int filterwidth);  // show filter bandwidth
extern void show_radiomode(String mode);  // show filter bandwidth
extern void show_band(String bandname);  // show band
extern void show_frequency(long freq);  // show frequency
extern void show_steps(String name);  // show frequency
extern ILI9341_t3  tft;

//#define  DEBUG
//#define DEBUG_TIMING  // for timing execution - monitor HW pin

// SW configuration defines
// don't use more than one AGC!
#define SW_AGC   // define for Loftur's SW AGC - this has to be tuned carefully for your particular implementation
// codec hardware AGC works but it looks at the entire input bandwidth
// ie codec HW AGC works on the strongest signal, not necessarily what you are listening to
// it should work well for ALC (mic input) though
//#define HW_AGC // define for codec AGC
#define CW_WATERFALL // define for experimental CW waterfall - needs faster update rate
//#define AUDIO_STATS    // shows audio library CPU utilization etc on serial console

// band selection stuff
struct band {
  long freq;
  String name;
};

struct si570_step {
  long size;
  String name;
};



#define SOFTROCK_40M

#ifdef SOFTROCK_40M

#define BAND_80M  0   // these can be in any order but indexes need to be sequential for band switching code
#define BAND_60M  1
#define BAND_49M  2
#define BAND_40M  3
#define BAND_31M  4
#define BAND_30M  5
#define BAND_20M  6
#define BAND_ATC  7
#define BAND_RAF  8
#define BAND_CB   9

#define FIRST_BAND BAND_80M
#define LAST_BAND  BAND_CB
#define NUM_BANDS  LAST_BAND - FIRST_BAND + 1

struct band bands[NUM_BANDS] = {
  3694000,  "80M",
  5000000,  "60M",
  6000000,  "49M",
  7040000,  "40M",
  9500000,  "31M",
  10115000, "30M",
  14115000, "20M",
  8895000,  "ATC",
  11242000, "RAF",
  27544000, "CB "
};

struct si570_step steps[5] = {
  100000,  "100 kHz. ",
  5000,    "  5 kHz. ",
  1000,    "  1 kHz. ",
  100,     "100  Hz. ",
  10,      " 10  Hz. ",
};

#define STARTUP_BAND BAND_40M    // 
#define STARTUP_STEP 2     // 
#endif

// UI switch definitions
// encoder switch
Encoder tune(16, 17);
#define TUNE_STEP       25    // slow tuning rate 25hz steps
#define FAST_TUNE_STEP   1000   // fast tuning rate 1000hz steps

// Switches between pin and ground for USB/LSB/CW modes

const int8_t ModeSW = 0;   // USB/LSB
const int8_t BandSW = 1;   // band selector
const int8_t TuneSW = 6;   // low for fast tune - encoder pushbutton

const int8_t AbpfSW = 2;

// see define above - pin 4 used for SW execution timing & debugging
#ifdef  DEBUG_TIMING
#define DEBUG_PIN   4
#endif
const int8_t PTTSW = 10;    // also used as SDCS on the audio card - can't use an SD card!
const int8_t PTTout = 5;    // PTT signal to softrock

Bounce  PTT_in = Bounce();   // debouncer

// in receive mode we use an audio IF to avoid the noise, offset and hum below ~ 1khz
#define IF_FREQ 11000       // IF Oscillator frequency
#define CW_FREQ 700        // audio tone frequency used for CW

// clock generator

#ifdef SI5351
Si5351 si5351;
#endif

#ifdef SI570
Si570 *vfo;
#endif

#define MASTER_CLK_MULT 2  // softrock requires 4x clock
#define MASTER_CLK_DIV  5  // softrock requires 4x 

// various timers
Metro five_sec = Metro(5000); // Set up a 5 second Metro
Metro ms_100 = Metro(100);  // Set up a 100ms Metro
Metro ms_50 = Metro(50);  // Set up a 50ms Metro for polling switches
Metro lcd_upd = Metro(10); // Set up a Metro for LCD updates
Metro CW_sample = Metro(50); // Set up a Metro for LCD updates

#ifdef CW_WATERFALL
Metro waterfall_upd = Metro(400); // Set up a Metro for waterfall updates
#endif

// radio operation mode defines used for filter selections etc
#define  SSB_USB  0
#define  SSB_LSB  1
#define  CW       2
#define  CWR      3

// audio definitions
//  RX & TX audio input definitions
const int inputTX = AUDIO_INPUT_MIC;
const int inputRX = AUDIO_INPUT_LINEIN;

#define  INITIAL_VOLUME 0.5   // 0-1.0 output volume on startup
#define  CW_SIDETONE_VOLUME    0.25    // 0-1.0 level for CW TX mode
#define  SSB_SIDETONE_VOLUME    0.8   // 0-1.0 adjust to hear SSB TX audio thru the headphones
#define  MIC_GAIN      30      // mic gain in db 
#define  RX_LEVEL_I    1.0    // 0-1.0 adjust for RX I/Q balance
#define  RX_LEVEL_Q    1.0    // 0-1.0 adjust for RX I/Q balance
#define  SSB_TX_LEVEL_I    1.0   // 0-1.0 adjust for SSB TX I/Q balance
#define  SSB_TX_LEVEL_Q    0.956  // 0-1.0 adjust for SSB TX I/Q balance
#define  CW_TX_LEVEL_I    1.0   // 0-1.0 adjust for CW TX I/Q balance
#define  CW_TX_LEVEL_Q    0.956 // 0-1.0 adjust for CW TX I/Q balance

// channel assignments for output Audioselectors
//
#define ROUTE_RX 	        0    // used for all recieve modes
#define ROUTE_SSB_TX 	    1    // SSB modes
#define ROUTE_CW_TX 	    2    // CW modes

// arrays used to set output audio routing and levels
// 3 radio modes x 4 channels for each audioselector
float audiolevels_I[3][4] = {
  RX_LEVEL_I, 0, 0, 0,     // RX mode channel levels
  0, SSB_TX_LEVEL_I, 0, 0, // SSB TX mode channel levels
  0, 0, CW_TX_LEVEL_I, 0 //CW  TX mode channel levels
};

float audiolevels_Q[3][4] = {
  RX_LEVEL_Q, 0, 0, 0,     // RX mode channel levels
  0, SSB_TX_LEVEL_Q, 0, 0, // SSB TX mode channel levels
  0, 0, CW_TX_LEVEL_Q, 0 //CW  TX mode channel levels
};

// Create the Audio components.  These should be created in the
// order data flows, inputs/sources -> processing -> outputs
//
AudioInputI2S       audioinput;           // Audio Shield: mic or line-in

// FIR filters
AudioFilterFIR      Hilbert45_I;
AudioFilterFIR      Hilbert45_Q;
AudioFilterFIR      FIR_BPF;
AudioFilterFIR      postFIR;

AudioMixer4         Summer;        // Summer (add inputs)
AudioAnalyzeFFT256  myFFT;      // Spectrum Display
AudioSynthWaveform  IF_osc;           // Local Oscillator
AudioSynthWaveform  CW_tone_I;           // Oscillator for CW tone I (sine)
AudioSynthWaveform  CW_tone_Q;           // Oscillator for CW tone Q (cosine)
AudioEffectMultiply    Mixer;         // Mixer (multiply inputs)
AudioAnalyzePeak     Smeter;        // Measure Audio Peak for S meter
AudioMixer4         Audioselector_I;   // Summer used for AGC and audio switch
AudioMixer4         Audioselector_Q;   // Summer used as audio selector
AudioAnalyzePeak    AGCpeak;       // Measure Audio Peak for AGC use
AudioOutputI2S      audioOutput;   // Audio Shield: headphones & line-out

AudioControlSGTL5000 audioShield;  // Create an object to control the audio shield.

//---------------------------------------------------------------------------------------------------------
// Create Audio connections to build a software defined Radio Receiver
//
AudioConnection c1(audioinput, 0, Hilbert45_I, 0);// Audio inputs to +/- 45 degree filters
AudioConnection c2(audioinput, 1, Hilbert45_Q, 0);
AudioConnection c3(Hilbert45_I, 0, Summer, 0);    // Sum the shifted filter outputs to supress the image
AudioConnection c4(Hilbert45_Q, 0, Summer, 1);
//
AudioConnection c10(Summer, 0, myFFT, 0);         // FFT for spectrum display
AudioConnection c11(Summer, 0, FIR_BPF, 0);       // 2.4 kHz USB or LSB filter centred at either 12.5 or 9.5 kHz
//                                                // ( local oscillator zero beat is at 11 kHz, see NCO )
AudioConnection c12(FIR_BPF, 0, Mixer, 0);        // IF from BPF to Mixer
AudioConnection c13(IF_osc, 0, Mixer, 1);            // Local Oscillator to Mixer (11 kHz)
//
AudioConnection c20(Mixer, 0, postFIR, 0);        // 2700Hz Low Pass filter or 200 Hz wide CW filter at 700Hz on audio output
AudioConnection c30(postFIR, 0, Smeter, 0);       // RX signal S-Meter measurement point
//
// RX is mono output , but for TX we need I and Q audio channel output
// two summers (I and Q) on the output used to select different audio paths for different RX and TX modes
//
AudioConnection c31(postFIR, 0, Audioselector_I, ROUTE_RX);   // mono RX audio and AGC Gain loop adjust
AudioConnection c32(postFIR, 0, Audioselector_Q, ROUTE_RX);   // mono RX audio to 2nd channel
AudioConnection c33(Hilbert45_I, 0, Audioselector_I, ROUTE_SSB_TX);   // SSB TX I audio and ALC Gain loop adjust
AudioConnection c34(Hilbert45_Q, 0, Audioselector_Q, ROUTE_SSB_TX);   // SSB TX Q audio and ALC Gain loop adjust
AudioConnection c35(CW_tone_I, 0, Audioselector_I, ROUTE_CW_TX);   // CW TX I audio
AudioConnection c36(CW_tone_Q, 0, Audioselector_Q, ROUTE_CW_TX);   // CW TX Q audio
// note that last mixer input is unused - PSK mode ???
//
AudioConnection c40(Audioselector_I, 0, AGCpeak, 0);          // AGC Gain loop measure
AudioConnection c41(Audioselector_I, 0, audioOutput, 0);      // Output the sum on both channels
AudioConnection c42(Audioselector_Q, 0, audioOutput, 1);
//---------------------------------------------------------------------------------------------------------


void setup()
{
  Serial.begin(9600); // debug console
#ifdef DEBUG
  while (!Serial) ; // wait for connection
  Serial.println("initializing");
#endif
  pinMode(ModeSW, INPUT);  // USB/LSB switch
  pinMode(BandSW, INPUT);  // filter width switch
  pinMode(TuneSW, INPUT_PULLUP);  // tuning rate = high
  pinMode(PTTSW, INPUT_PULLUP);  // PTT input
  pinMode(PTTout, OUTPUT);  // PTT output to softrock
  digitalWrite(PTTout, 0); // turn off TX mode
  PTT_in.attach(PTTSW);    // PPT switch debouncer
  PTT_in.interval(5);  // 5ms

  pinMode(AbpfSW, INPUT);  // Auto band pass filter switch

#ifdef  DEBUG_TIMING
  pinMode(DEBUG_PIN, OUTPUT);  // for execution time monitoring with a scope
#endif

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(16);

  // Enable the audio shield and set the output volume.
  Serial.println("initializing audio shield...");
  audioShield.enable();
  audioShield.volume(INITIAL_VOLUME);
  audioShield.unmuteLineout();
#ifdef DEBUG
  Serial.println("audio shield enabled");
#endif

#ifdef HW_AGC
  /* COMMENTS FROM Teensy Audio library:
    Valid values for dap_avc parameters
  maxGain; Maximum gain that can be applied
  0 - 0 dB
  1 - 6.0 dB
  2 - 12 dB
  lbiResponse; Integrator Response
  0 - 0 mS
  1 - 25 mS
  2 - 50 mS
  3 - 100 mS
  hardLimit
  0 - Hard limit disabled. AVC Compressor/Expander enabled.
  1 - Hard limit enabled. The signal is limited to the programmed threshold (signal saturates at the threshold)
  threshold
  floating point in range 0 to -96 dB
  attack
  floating point figure is dB/s rate at which gain is increased
  decay
  floating point figure is dB/s rate at which gain is reduced
  */
  audioShield.autoVolumeControl(2, 1, 0, -30, 3, 20); // see comments above
  audioShield.autoVolumeEnable();

#endif

  // initialize the TFT and show signon message etc
  setup_display();

  // set up initial band and frequency
  show_band(bands[STARTUP_BAND].name);


#ifdef SI570

  vfo = new Si570(SI570_I2C_ADDRESS, 56320000);

  if (vfo->status == SI570_ERROR) {
    // The Si570 is unreachable. Show an error for 3 seconds and continue.
    Serial.print("Si570 comm error");
    tft.setCursor(0, 180);
    tft.setTextColor(RED, BLACK);
    tft.setTextSize(2);
    tft.print("SI570 ERROR");
    delay(10000);

  }

  vfo->setFrequency((unsigned long)bands[STARTUP_BAND].freq * MASTER_CLK_MULT);

  delay(3);

#endif


#ifdef SI5351

  // set up clk gen
  si5351.init(SI5351_CRYSTAL_LOAD_8PF);
  si5351.set_correction(+7000);  // I did a by ear correction to WWV
  // Set CLK0 to output 14 MHz with a fixed PLL frequency
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_freq((unsigned long)bands[STARTUP_BAND].freq * MASTER_CLK_MULT / MASTER_CLK_DIV, SI5351_PLL_FIXED, SI5351_CLK0);

#endif

  setup_RX(SSB_USB);  // set up the audio chain for USB reception

#ifdef DEBUG
  Serial.println("audio RX path initialized");
#endif


  setband(bands[STARTUP_BAND].freq + IF_FREQ);
  show_frequency(bands[STARTUP_BAND].freq + IF_FREQ);  // frequency we are listening to
  show_steps(steps[STARTUP_STEP].name);

  lcd.begin(20, 2);                      // initialize the lcd

  lcd.setCursor(0, 0);

  lcd.print("Hello!");
}


void loop()
{
  static uint8_t mode = SSB_USB, modesw_state = 0;
  static uint8_t band = STARTUP_BAND, Bandsw_state = 0;
  static uint8_t abpfsw_state = 0;
  static uint8_t num_step = 2, rebote = 0, stepsw_state = 0;
  static long encoder_pos = 0, last_encoder_pos = 999;
  long encoder_change;

  // tune radio using encoder switch

  encoder_pos = tune.read();

  if (encoder_pos != last_encoder_pos) {

    encoder_change = encoder_pos - last_encoder_pos;
    last_encoder_pos = encoder_pos;

    if (rebote > 3) {

      bands[band].freq += encoder_change  * steps[num_step].size; // tune the master vfo - normal steps


#ifdef SI5351
      si5351.set_freq((unsigned long)bands[band].freq * MASTER_CLK_MULT / MASTER_CLK_DIV, SI5351_PLL_FIXED, SI5351_CLK0);
#endif

#ifdef SI570
      vfo->setFrequency((unsigned long)bands[band].freq * MASTER_CLK_MULT);
#endif

      show_frequency(bands[band].freq + IF_FREQ); // frequency we are listening to
      setband(bands[band].freq + IF_FREQ);
      rebote = 0;

    }
    else {
      rebote++;
      delay(1);
    }
  }

  // every 50 ms, adjust the volume and check the switches
  if (ms_50.check() == 1) {
    float vol = analogRead(15);
    vol = vol / 1023.0;
    audioShield.volume(vol);

    // press encoder button for step change

    if (!digitalRead(TuneSW)) {
      if (stepsw_state == 0) {

        if (num_step < 4)
          num_step++;
        else
          num_step = 0;

        show_steps(steps[num_step].name);
        stepsw_state = 1;
      }

    }
    else stepsw_state = 0; // flag switch not pressed

    if (!digitalRead(ModeSW)) {
      if (modesw_state == 0) { // switch was pressed - falling edge
        if (++mode > CWR) mode = SSB_USB; // cycle thru radio modes
        setup_RX(mode);  // set up the audio chain for new mode
        modesw_state = 1; // flag switch is pressed
      }
    }
    else modesw_state = 0; // flag switch not pressed

    if (!digitalRead(AbpfSW)) {
      if (abpfsw_state == 0) { // switch was pressed - falling edge

        if (abpf < 16 )
          abpf <<= 1;
        else
          abpf = 1;

        lpf(abpf);


        abpfsw_state = 1; // flag switch is pressed
      }
    }
    else abpfsw_state = 0; // flag switch not pressed

    if (!digitalRead(BandSW)) {
      if (Bandsw_state == 0) { // switch was pressed - falling edge
        if (++band > LAST_BAND) band = FIRST_BAND; // cycle thru radio bands
        show_band(bands[band].name); // show new band
#ifdef SI5351
        si5351.set_freq((unsigned long)bands[band].freq * MASTER_CLK_MULT / MASTER_CLK_DIV, SI5351_PLL_FIXED, SI5351_CLK0);
#endif

#ifdef SI570
        vfo->setFrequency((unsigned long)bands[band].freq * MASTER_CLK_MULT);
#endif
        setband(bands[band].freq + IF_FREQ);
        show_frequency(bands[band].freq + IF_FREQ);  // frequency we are listening to
        Bandsw_state = 1; // flag switch is pressed
      }
    }
    else Bandsw_state = 0; // flag switch not pressed
  }


  // TX logic
  // looks like the Teensy audio line outs have fixed levels
  // that allows us to set sidetone levels separately which is really nice
  // have to shift freq up by IF_FREQ on TX
  // keyclicks - could ramp waveform in software.

  PTT_in.update();  // check the PTT switch
  if (!PTT_in.read())  // PTT switch is active, go into transmit mode
  {
    tft.setTextColor(RED);
    tft.setCursor(75, 72);
    tft.print("TX");
    setup_TX(mode);  // set up the audio chain for transmit mode
    // in TX mode we don't use an IF so we have to shift the TX frequency up by the IF frequency
    //si5351.set_freq((unsigned long)(bands[band].freq+IF_FREQ)*MASTER_CLK_MULT, SI5351_PLL_FIXED, SI5351_CLK0);
    delay(2); // short delay to allow things to settle
    digitalWrite(PTTout, 1); // transmitter on
    while ( !PTT_in.read()) { // wait for PTT release
      PTT_in.update();  // check the PTT switch
      if ((lcd_upd.check() == 1) && myFFT.available()) show_spectrum(); // only works in SSB mode
    }
    digitalWrite(PTTout, 0); // transmitter off
    // restore the master clock to the RX frequency
    //si5351.set_freq((unsigned long)bands[band].freq*MASTER_CLK_MULT, SI5351_PLL_FIXED, SI5351_CLK0);
    setup_RX(mode);  // set up the audio chain for RX mode
    tft.fillRect(75, 72, 11, 10, BLACK);// erase text
  }

#ifdef SW_AGC
  agc();  // Automatic Gain Control function
#endif

  //
  // Draw Spectrum Display
  //
  //  if ((lcd_upd.check() == 1) && myFFT.available()) show_spectrum();
  if ((lcd_upd.check() == 1)) show_spectrum();

  if ((CW_sample.check() == 1)) {
    //digitalWrite(DEBUG_PIN, 1); // for timing measurements
    //delay(1);
    //digitalWrite(DEBUG_PIN, 0); //
  }

#ifdef CW_WATERFALL
  if ((waterfall_upd.check() == 1) && myFFT.available()) show_waterfall();
#endif

#ifdef AUDIO_STATS
  //
  // DEBUG - Microcontroller Load Check
  //
  // Change this to if(1) to monitor load

  /*
  For PlaySynthMusic this produces:
  Proc = 20 (21),  Mem = 2 (8)
  */
  if (five_sec.check() == 1)
  {
    Serial.print("Proc = ");
    Serial.print(AudioProcessorUsage());
    Serial.print(" (");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("),  Mem = ");
    Serial.print(AudioMemoryUsage());
    Serial.print(" (");
    Serial.print(AudioMemoryUsageMax());
    Serial.println(")");
  }
#endif
}

// function to set up audio routes for the four channels on the two output audio summers
// summers are used to change audio routing at runtime
//
void Audiochannelsetup(int route)
{
  for (int i = 0; i < 4 ; ++i) {
    Audioselector_I.gain(i, audiolevels_I[route][i]); // set gains on audioselector channels
    Audioselector_Q.gain(i, audiolevels_Q[route][i]);
  }
}

// set up radio for RX modes - USB, LSB etc
void setup_RX(int mode)
{
  AudioNoInterrupts();   // Disable Audio while reconfiguring filters

  audioShield.inputSelect(inputRX); // RX mode uses line ins
  Audiochannelsetup(ROUTE_RX);   // switch audio path to RX processing chain

  CW_tone_I.amplitude(0);  // turn off cw oscillators to reduce cpu use
  CW_tone_Q.amplitude(0);

  // set IF oscillator to 11 kHz for RX
  IF_osc.begin(1.0, IF_FREQ, TONE_TYPE_SINE);

  // Initialize the wideband +/-45 degree Hilbert filters
  Hilbert45_I.begin(RX_hilbertm45, HILBERT_COEFFS);
  Hilbert45_Q.begin(RX_hilbert45, HILBERT_COEFFS);

  if ((mode == SSB_LSB) || (mode == CWR))             // LSB modes
    FIR_BPF.begin(firbpf_lsb, BPF_COEFFS);      // 2.4kHz LSB filter
  else FIR_BPF.begin(firbpf_usb, BPF_COEFFS);      // 2.4kHz USB filter

  switch (mode)	{
    case CWR:
      postFIR.begin(postfir_700, COEFF_700);    // 700 Hz LSB filter
      show_bandwidth(LSB_NARROW);
      show_radiomode("CWR");
      break;
    case SSB_LSB:
      postFIR.begin(postfir_lpf, COEFF_LPF);    // 2.4kHz LSB filter
      show_bandwidth(LSB_WIDE);
      show_radiomode("LSB");
      break;
    case CW:
      postFIR.begin(postfir_700, COEFF_700);    // 700 Hz LSB filter
      show_bandwidth(USB_NARROW);
      show_radiomode("CW ");
      break;
    case SSB_USB:
      postFIR.begin(postfir_lpf, COEFF_LPF);    // 2.4kHz LSB filter
      show_bandwidth(USB_WIDE);
      show_radiomode("USB");
      break;
  }
  AudioInterrupts();
}

// set up radio for TX modes - USB, LSB etc
void setup_TX(int mode)
{
  AudioNoInterrupts();   // Disable Audio while reconfiguring filters

  FIR_BPF.end(); // turn off the BPF - IF filters are not used in TX mode
  postFIR.end();     // turn off 2.4kHz post filter

  switch (mode)	{
    case CW:
      CW_tone_I.begin(1.0, CW_FREQ, TONE_TYPE_SINE);
      CW_tone_Q.begin(1.0, CW_FREQ, TONE_TYPE_SINE);
      CW_tone_Q.phase(90);
      Audiochannelsetup(ROUTE_CW_TX);   // switch audio outs to CW I & Q
      audioShield.volume(CW_SIDETONE_VOLUME);   // fixed level for TX
      break;
    case CWR:
      CW_tone_I.begin(1.0, CW_FREQ, TONE_TYPE_SINE);
      CW_tone_Q.begin(1.0, CW_FREQ, TONE_TYPE_SINE);
      CW_tone_I.phase(90);
      Audiochannelsetup(ROUTE_CW_TX);   // switch audio outs to CW I & Q
      audioShield.volume(CW_SIDETONE_VOLUME);   // fixed level for TX
      break;
    case SSB_USB:
      // Initialize the +/-45 degree Hilbert filters
      Hilbert45_I.begin(TX_hilbert45, HILBERT_COEFFS);
      Hilbert45_Q.begin(TX_hilbertm45, HILBERT_COEFFS);
      Audiochannelsetup(ROUTE_SSB_TX);   // switch audio outs to TX Hilbert filters
      audioShield.inputSelect(inputTX); // SSB TX mode uses mic in
      audioShield.micGain(MIC_GAIN);  // have to adjust mic gain after selecting mic in
      audioShield.volume(SSB_SIDETONE_VOLUME);   // fixed level for TX
      break;
    case SSB_LSB:
      // Initialize the +/-45 degree Hilbert filters
      Hilbert45_I.begin(TX_hilbertm45, HILBERT_COEFFS); // swap filters for LSB mode
      Hilbert45_Q.begin(TX_hilbert45, HILBERT_COEFFS);
      Audiochannelsetup(ROUTE_SSB_TX);   // switch audio outs to TX Hilbert filters
      audioShield.inputSelect(inputTX); // SSB TX mode uses mic in
      audioShield.micGain(MIC_GAIN); // have to adjust mic gain after selecting mic in
      audioShield.volume(SSB_SIDETONE_VOLUME);   // fixed level for TX
      break;
  }
  AudioInterrupts();
}

void setband(unsigned long freq)
{
  if ( freq >= 1e6 && freq < 6e6)
  {
    if (abpf != 1) lpf( 1 );
  }
  if ( freq >= 6e6 && freq < 10e6 )
  {
    if (abpf != 2) lpf( 2 );
  }
  if ( freq >= 10e6 && freq < 16e6)
  {
    if (abpf != 8) lpf( 8 );
  }
  if ( freq >= 16e6 && freq < 30e6)
  {
    if (abpf != 16) lpf( 16 );
  }


}

void lpf( int output ) {

  byte low = output & 0x00FF;
  byte high = output >> 8;

  char string[10];

  Wire.beginTransmission(PCF8575_ADDRESS);
  Wire.write(low);
  Wire.write(high);
  Wire.endTransmission();

  sprintf(string, "F:%i ", output );

  tft.setCursor(75, 200);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(1);
  tft.print(string);
  tft.setTextSize(2);

  abpf = output;

}
byte read_pcf8575( void ) {

  byte low;
  byte high;


  byte dataReceived[2]; //a two byte array to hold our data
  Wire.beginTransmission(PCF8575_ADDRESS); ////who are you talking to?
  Wire.endTransmission(); //end communication
  Wire.requestFrom(PCF8575_ADDRESS, 2); //request two bytes of data
  if (Wire.available()) {
    low = Wire.receive(); //read byte 1
    high = Wire.receive(); //read byte 2
  }

}

