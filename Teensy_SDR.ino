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


#include <Metro.h>
#include <Audio.h>
#include "i2c_t3.h"
#include <SD.h>
#include <Encoder.h>
#include <si5351.h>
#include "ILI9341_t3.h"
#include "font_Arial.h"
#include <SPI.h>
#include "filters.h"
#include "display.h"
#include "SerialFlash.h"
#include "demod.h"
#include "fft_iq.h"
#include "freq_conv.h"
#include "menu.h"
#include "correct_iq.h"


/////////////////////////////////////////////////////////////////////
// DEFINES
/////////////////////////////////////////////////////////////////////


//#define  DEBUG
//#define FFT_BASE

#define SW_AGC   // define for Loftur's SW AGC - this has to be tuned carefully for your particular implementation
//#define HW_AGC // define for codec AGC
#define CW_WATERFALL // define for experimental CW waterfall - needs faster update rate
#define AUDIO_STATS    // shows audio library CPU utilization etc on serial console

#define PCF8575_ADDRESS  0x22

#define WATERFALL_TUNNING_DELAY 500

#define  USB_WIDE    0  // filter modes
#define  USB_NARROW  1
#define  LSB_WIDE    2
#define  LSB_NARROW  3
#define  DSB         4
#define  USB_3700    5  // filter modes
#define  USB_1700    6
#define  LSB_3700    7
#define  LSB_1700    8
#define  AM_BPF      9


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

#define STARTUP_BAND BAND_40M
#define STARTUP_STEP 2

/////////////////////////////////////////////////////////////////////
//VARIABLES
/////////////////////////////////////////////////////////////////////

static uint8_t abpf = 1;
static bool waterfall_sw = 1;
static bool MuteSW_state = 0;
static bool AgcSW_state = 0;
static int16_t IF_FREQ = 11025;     // IF Oscillator frequency
static int8_t filter = USB_WIDE;
float s_old = 0;
static float32_t Osc_Q_buffer_i_f[AUDIO_BLOCK_SAMPLES];
static float32_t Osc_I_buffer_i_f[AUDIO_BLOCK_SAMPLES];
q15_t Osc_Q_buffer_i[AUDIO_BLOCK_SAMPLES];
q15_t Osc_I_buffer_i[AUDIO_BLOCK_SAMPLES];

static bool flag = 0;

bool freqconv_sw=1;

q15_t last_dc_level = 0;

// band selection stuff
struct band {
  long freq;
  String name;
};

struct si5351_step {
  long size;
  String name;
};


struct band bands[NUM_BANDS] = {
  3694000,  "80M",
  5494000,  "60M",
  6000000,  "49M",
  7134000,  "40M",
  9566000,  "31M",
  10115000, "30M",
  14115000, "20M",
  8895000,  "ATC",
  11242000, "RAF",
  27544000, "CB "
};

struct si5351_step steps[5] = {
  100000,  "100 kHz.",
  10000,   " 10 kHz.",
  1000,    "  1 kHz.",
  100,     "100  Hz.",
  10,      " 10  Hz.",
};

/////////////////////////////////////////////////////////////////////
// FUNCTIONS
/////////////////////////////////////////////////////////////////////

void setband(unsigned long freq);
void sinewave(void);
extern void agc(void);      // RX agc function
extern void setup_display(void);
extern void show_spectrum(void);  // spectrum display draw
extern void show_waterfall(void); // waterfall display
extern void show_spectrum_base(void);  // spectrum display draw
extern void show_waterfall_base(void); // waterfall display
extern void show_bandwidth(int filterwidth, uint16_t IF_FREQ);  // show filter bandwidth
extern void show_radiomode(String mode);  // show filter bandwidth
extern void show_band(String bandname);  // show band
extern void show_frequency(long freq);  // show frequency
extern void show_steps(String name);  // show frequency
extern void box (uint8_t number, String text, bool selected );
extern ILI9341_t3  tft;

/////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////


// UI switch definitions
// encoder switch
Encoder tune(16, 17);


// Switches between pin and ground for USB/LSB/CW modes

const int8_t ModeSW = 0;   // USB/LSB
const int8_t BandSW = 1;   // band selector
const int8_t TuneSW = 6;   // low for fast tune - encoder pushbutton
const int8_t AbpfSW = 2;
const int8_t pcf8575_int = 3; // pcf8575 interrupt input pin (LOW = gpio state changed).

// clock generator

Si5351 si5351;

#define MASTER_CLK_MULT 2
#define MASTER_CLK_DIV  5

// various timers
Metro five_sec = Metro(5000); // Set up a 5 second Metro
Metro ms_100 = Metro(100);  // Set up a 100ms Metro
Metro ms_50 = Metro(50);  // Set up a 50ms Metro for polling switches
Metro lcd_upd = Metro(10); // Set up a Metro for LCD updates
Metro CW_sample = Metro(50); // Set up a Metro for LCD updates

#ifdef CW_WATERFALL
Metro waterfall_upd = Metro(250); // Set up a Metro for waterfall updates
#endif

// radio operation mode defines used for filter selections etc
#define  SSB_USB    0
#define  SSB_USB_N  1
#define  SSB_USB_W  2
#define  CW         3
#define  SSB_DSB    4
#define  AM         5
#define  CWR        6
#define  SSB_LSB_W  7
#define  SSB_LSB_N  8
#define  SSB_LSB    9

// audio definitions
//  RX audio input definitions
const int inputRX = AUDIO_INPUT_LINEIN;

#define  INITIAL_VOLUME 1   // 0-1.0 output volume on startup
#define  MIC_GAIN      30      // mic gain in db 
#define  RX_LEVEL_I    1.0    // 0-1.0 adjust for RX I/Q balance
#define  RX_LEVEL_Q    1.0    // 0-1.0 adjust for RX I/Q balance

// channel assignments for output Audioselectors
//
#define ROUTE_RX 	        0    // used for all recieve modes
#define ROUTE_AM          1

// arrays used to set output audio routing and levels
// 3 radio modes x 4 channels for each audioselector
float audiolevels_I[4][4] = {
  RX_LEVEL_I, 0, 0, 0,     // RX mode channel levels
  0, RX_LEVEL_I, 0, 0, // SSB TX mode channel levels
  0, 0, RX_LEVEL_I, 0, //CW  TX mode channel levels
  0, 0, 0, RX_LEVEL_I
};

float audiolevels_Q[4][4] = {
  RX_LEVEL_Q, 0, 0, 0,     // RX mode channel levels
  0, RX_LEVEL_Q, 0, 0, // SSB TX mode channel levels
  0, 0, RX_LEVEL_Q, 0, //CW  TX mode channel levels
  0, 0, 0, RX_LEVEL_Q
};

// Create the Audio components.  These should be created in the
// order data flows, inputs/sources -> processing -> outputs
//
AudioInputI2S           audioinput;           // Audio Shield: mic or line-in
AudioAnalyzePeak        Smeter;            // Measure Audio Peak for S meter
AudioAnalyzePeak        AGCpeak;            // Measure Audio Peak for AGC use
AudioOutputI2S          audioOutput;        // Audio Shield: headphones & line-out

AudioEffectCorrectIQ     CorrectIQ;
AudioEffectdemod         demod;
AudioAnalyzeFFT256IQ     FFT256IQ;              // Spectrum Display
AudioEffectFreqConv      FreqConv;

#ifdef FFT_BASE
AudioAnalyzeFFT256       demod_FFT;              // Spectrum Display
#endif

AudioFilterBiquad        biquad;
AudioMixer4              Audioselector_I;    // Summer used for AGC and audio switch
AudioMixer4              Audioselector_Q;    // Summer used as audio selector

AudioConnection          con01(audioinput, 0, CorrectIQ, 0);
AudioConnection          con02(audioinput, 1, CorrectIQ, 1);

AudioConnection          con11(CorrectIQ, 0, FFT256IQ, 0);
AudioConnection          con12(CorrectIQ, 1, FFT256IQ, 1);

AudioConnection          con31(CorrectIQ, 0, FreqConv, 0);
AudioConnection          con32(CorrectIQ, 1, FreqConv, 1);

AudioConnection          con33(FreqConv, 0, demod, 0);
AudioConnection          con34(FreqConv, 1, demod, 1);

#ifdef FFT_BASE
AudioConnection          con38(demod, 0, demod_FFT, 0);
#endif

AudioConnection          con44(demod, 0, biquad, 0);            // IF from BPF to Mixer

AudioConnection          con51(demod, 0, Smeter, 0);            // RX signal S-Meter measurement point

AudioConnection          con61(demod, 0, Audioselector_I, ROUTE_RX);
AudioConnection          con62(demod, 0, Audioselector_Q, ROUTE_RX);

AudioConnection          con63(biquad, 0, Audioselector_I, ROUTE_AM);
AudioConnection          con64(biquad, 0, Audioselector_Q, ROUTE_AM);

AudioConnection          con71(Audioselector_I, 0, AGCpeak, 0);
AudioConnection          con72(Audioselector_I, 0, audioOutput, 0);      // Output the sum on both channels
AudioConnection          con73(Audioselector_Q, 0, audioOutput, 1);

AudioControlSGTL5000      audioShield;  // Create an object to control the audio shield.

//---------------------------------------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////
// SETUP CODE
/////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(9600); // debug console
  Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, I2C_RATE_400);

#ifdef DEBUG
  while (!Serial) ; // wait for connection
  Serial.println("initializing");
#endif
  pinMode(ModeSW, INPUT);  // USB/LSB switch
  pinMode(BandSW, INPUT);  // filter width switch
  pinMode(pcf8575_int, INPUT);
  pinMode(TuneSW, INPUT_PULLUP);  // tuning rate = high
  pinMode(AbpfSW, INPUT);  // Auto band pass filter switch

  // initialize the TFT and show signon message etc
  setup_display();

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(24);

  // Enable the audio shield and set the output volume.
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
  audioShield.f(2, 1, 0, -30, 3, 20); // see comments above
  audioShield.autoVolumeEnable();

#endif

  // set up initial band and frequency
  show_band(bands[STARTUP_BAND].name);

  // set up clk gen
  si5351.init(SI5351_CRYSTAL_LOAD_8PF);
  si5351.set_correction(+7000);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  //si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_4MA);
  si5351.set_freq((unsigned long)bands[STARTUP_BAND].freq * MASTER_CLK_MULT / MASTER_CLK_DIV, SI5351_PLL_FIXED, SI5351_CLK0);

  delay(100);

  setup_RX(USB_WIDE);  // set up the audio chain for USB reception

#ifdef DEBUG
  Serial.println("audio RX path initialized");
#endif


  setband(bands[STARTUP_BAND].freq);
  show_frequency(bands[STARTUP_BAND].freq + IF_FREQ ); // frequency we are listening to
  tft.fillRect(259, 236, 12, 4, RED); // muestra el marcador para el paso de 1 Khz inicial

  sinewave();                   // genera la tabla de SIN/COS para el freqconv
  Options_ResetToDefaults();    // opciones por defecto para las variables de menú de opciones
}

/////////////////////////////////////////////////////////////////////
// LOOP
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// LOOP
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// LOOP
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// LOOP
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// LOOP
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// LOOP
/////////////////////////////////////////////////////////////////////

void loop()
{

  static int8_t mode = USB_WIDE;
  static int8_t modesw_state = 0;
  static uint8_t band = STARTUP_BAND, Bandsw_state = 0;
  static uint8_t pcf8575_int_state = 0;
  static uint8_t abpfsw_state = 0;
  static uint8_t num_step = 2, rebote = 0, stepsw_state = 0;
  static long encoder_pos = 0, last_encoder_pos = 999;
  long encoder_change;
  char string[10];
  static int16_t waterfall_delay = WATERFALL_TUNNING_DELAY;
  static uint8_t menu; // menu position
  static bool in_menu; // we are in menu so encoder readings goes to change selected menu option
  int16_t newValue, currentValue, minValue, maxValue;;

  // tune radio using encoder switch

  if (waterfall_delay > 0) waterfall_delay--;
  else
    waterfall_delay = 0;

  encoder_pos = tune.read();

  /////////////////////////////////////////////////////////////////////
  // ENCODER
  /////////////////////////////////////////////////////////////////////

  if (encoder_pos != last_encoder_pos) {

    encoder_change = encoder_pos - last_encoder_pos;
    last_encoder_pos = encoder_pos;

    waterfall_delay = WATERFALL_TUNNING_DELAY;

    if (rebote > 3) {

      if (in_menu) {
        newValue = Options_GetValue(menu) + encoder_change * Options_GetChangeRate(menu);

        minValue = Options_GetMinimum(menu);
        maxValue = Options_GetMaximum(menu);
        if (newValue <= minValue) {
          newValue = minValue;
        }
        if (newValue >= maxValue) {
          newValue = maxValue;
        }

        Options_SetValue(menu, newValue);

        tft.setTextSize(2);
        tft.setTextColor(YELLOW, BLACK);
        tft.setCursor(0, 220);
        if (menu == OPTION_IQ_COMP || menu == OPTION_DEC_SSB || menu == OPTION_DEC_AM || menu == OPTION_FREQ_CONV)
          sprintf(string, "%s: %s ", Options_GetName(menu), Options_GetValue(menu) ? "ON  " : "OFF ");
        else
          sprintf(string, "%s: %04d ", Options_GetName(menu), Options_GetValue(menu));
        tft.print(string);
      }
      else
      {
        bands[band].freq += encoder_change  * steps[num_step].size; // tune the master vfo - normal steps
        si5351.set_freq((unsigned long)bands[band].freq * MASTER_CLK_MULT / MASTER_CLK_DIV, SI5351_PLL_FIXED, SI5351_CLK0);
        show_bandwidth(filter, IF_FREQ);
        show_frequency(bands[band].freq + IF_FREQ);
      }

      rebote = 0;
    }
    else rebote++;
  }



  /////////////////////////////////////////////////////////////////////
  // 50 ms FUNCTION
  /////////////////////////////////////////////////////////////////////

  // every 50 ms, adjust the volume and check the switches
  if (ms_50.check() == 1) {

    if (!MuteSW_state) {
      float vol = analogRead(15);
      vol = vol / 1023.0;
      audioShield.volume(vol);
    }

    // press encoder button for step change

    if (!digitalRead(TuneSW)) {
      if (stepsw_state == 0) {

        tft.fillRect(200, 236, 120, 4, BLACK);

        if (num_step < 4)
          num_step++;
        else
          num_step = 0;

        if (!num_step) tft.fillRect(235, 236, 12, 4, RED);
        if (num_step == 1) tft.fillRect(247, 236, 12, 4, RED);
        if (num_step == 2) tft.fillRect(259, 236, 12, 4, RED);
        if (num_step == 3) tft.fillRect(283, 236, 12, 4, RED);
        if (num_step == 4) tft.fillRect(295, 236, 12, 4, RED);

        //show_steps(steps[num_step].name);
        stepsw_state = 1;
      }
    }

    else stepsw_state = 0; // flag switch not pressed


    if (!digitalRead(ModeSW)) {
      if (modesw_state == 0) { // switch was pressed - falling edge
        AgcSW_state ^= 1;

        if (AgcSW_state)
        {
          audioShield.autoVolumeDisable();
          box(2, "AGC", 1);
        }
        else {
          audioShield.audioPreProcessorEnable();
          // here are some settings for AVC that have a fairly obvious effect
          audioShield.autoVolumeControl(2, 1, 0, -5, 0.5, 0.5);
          audioShield.autoVolumeEnable();
          box(2, "AGC", 0);
        }
        modesw_state = 1; // flag switch is pressed
      }
    }
    else modesw_state = 0; // flag switch not pressed


    if (!digitalRead(BandSW)) {

      if (abpfsw_state == 0) { // switch was pressed - falling edge

        if (!in_menu) in_menu = 1;

        if (menu++ >= END__OPTIONS - 1) {
          in_menu = 0;
          menu = 0;
          tft.fillRect(0, 191, 191, 50, BLACK);
        }
        else
        {
          tft.setTextSize(2);
          tft.setTextColor(YELLOW, BLACK);
          tft.setCursor(0, 220);
        if (menu == OPTION_IQ_COMP || menu == OPTION_DEC_SSB || menu == OPTION_DEC_AM || menu == OPTION_FREQ_CONV)
            sprintf(string, "%s: %s ", Options_GetName(menu), Options_GetValue(menu) ? "ON  " : "OFF ");
          else
            sprintf(string, "%s: %04d ", Options_GetName(menu), Options_GetValue(menu));
          tft.print(string);
        }

        abpfsw_state = 1; // flag switch is pressed
      }
    }
    else abpfsw_state = 0; // flag switch not pressed


    if (!digitalRead(AbpfSW)) {
      if (Bandsw_state == 0) { // switch was pressed - falling edge
        if (++band > LAST_BAND) band = FIRST_BAND; // cycle thru radio bands
        show_band(bands[band].name); // show new band
        si5351.set_freq((unsigned long)bands[band].freq * MASTER_CLK_MULT / MASTER_CLK_DIV, SI5351_PLL_FIXED, SI5351_CLK0);
        show_frequency(bands[band].freq + IF_FREQ ); // frequency we are listening to
        setband(bands[band].freq + IF_FREQ );

        Bandsw_state = 1;
      }
    }
    else Bandsw_state = 0; // flag switch not pressed


    if (!digitalRead(pcf8575_int)) {

      if (pcf8575_int_state == 0) {

        uint8_t high_byte = read_pcf8575();

        if ( (high_byte == 0b11111011) ) {       // Tecla skip/pausa

          if (MuteSW_state) {
            MuteSW_state = 0;
            float vol = analogRead(15);
            vol = vol / 1023.0;
            audioShield.volume(vol);

            audioShield.volume(INITIAL_VOLUME);
            audioShield.unmuteLineout();
            box(3, "MUTE", 0);
          }
          else
          {
            MuteSW_state = 1;
            audioShield.volume(0); // Tecla skip/pausa

            box(3, "MUTE", 1);
          }

        }

        if (high_byte == 0b11111110 )                     // Tecla canal -
        { if (--mode < SSB_USB) mode = SSB_LSB; // cycle thru radio modes
          setup_RX(mode);  // set up the audio chain for new mode
        }

        if (high_byte == 0b11111101 )                     // Tecla canal +
        { if (++mode > SSB_LSB) mode = SSB_USB; // cycle thru radio modes
          setup_RX(mode);  // set up the audio chain for new mode
        }

        if (high_byte == 0b11110111 ) {
          waterfall_sw ^= 1;                    // Tecla ROJA

          if (waterfall_sw) {
            box(4, "WFAL", 1);
            waterfall_delay = 0;
          }
          else
            box(4, "WFAL", 0);
        }

        pcf8575_int_state = 1;
      }
      else pcf8575_int_state = 0; // flag switch not pressed
    }
  } // end of 50 ms check routine


  /////////////////////////////////////////////////////////////////////
  // VARIOUS FUNCTIONS IN LOOP
  /////////////////////////////////////////////////////////////////////

#ifdef SW_AGC
  if (AgcSW_state) agc();  // Automatic Gain Control function
#endif

  //
  // Draw Spectrum Display
  //

#ifdef FFT_BASE
  if ((lcd_upd.check() == 1)) show_spectrum_base();
  if ((waterfall_upd.check() == 1) && FFT256IQ.available() && waterfall_sw && !waterfall_delay) show_waterfall_base();
#else
  if ((lcd_upd.check() == 1)) show_spectrum();
  if ((waterfall_upd.check() == 1) && FFT256IQ.available() && waterfall_sw && !waterfall_delay) show_waterfall();
#endif


#ifdef AUDIO_STATS
  //
  // DEBUG - Microcontroller Load Check
  //
  // Change this to if(1) to monitor load

  if (five_sec.check() == 1 && !in_menu)
  {
    //    Serial.print("Proc = ");
    //    Serial.print(AudioProcessorUsage());
    //    Serial.print(" (");
    //    Serial.print(AudioProcessorUsageMax());
    //    Serial.print("),  Mem = ");
    //    Serial.print(AudioMemoryUsage());
    //    Serial.print(" (");
    //    Serial.print(AudioMemoryUsageMax());
    //    Serial.println(")");

    tft.setTextSize(1);
    tft.setTextColor(YELLOW, BLACK);
    tft.setCursor(100, 200);
    tft.print(AudioProcessorUsage());

    sprintf(string, " %02d", AudioMemoryUsage() );

    tft.setCursor(135, 200);
    tft.print(string);


    tft.setTextSize(2);
  }
#endif

}


/////////////////////////////////////////////////////////////////////
// Audiochannelsetup
/////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////
// setup_RX
/////////////////////////////////////////////////////////////////////

// set up radio for RX modes - USB, LSB etc
void setup_RX(int mode)
{
  AudioNoInterrupts();   // Disable Audio while reconfiguring filters
  Audiochannelsetup(ROUTE_RX);
  FreqConv.direction(1);
  FreqConv.passthrough(freqconv_sw);
  

  demod.begin_decimate(fir_decimate, fir_interpolate, 4, 16);


  if ( mode == CW || mode == SSB_USB ||  mode == SSB_USB_N ||  mode == SSB_USB_W)
  {
    //demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_37k, postfir_1700hz, HILBERT_COEFFS, COEFF_LPF);
    demod.mode(1);
  }
  if ( mode == CWR || mode == SSB_LSB ||  mode == SSB_LSB_N ||  mode == SSB_LSB_W)
  {
    //demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_37k, postfir_1700hz, HILBERT_COEFFS, COEFF_LPF);
    demod.mode(2);
  }
  if ( mode == AM)
  {
    //demod.begin_demod(RX_hilbert_I_10k, RX_hilbert_Q_10k, FIR_PASSTHRU, HILBERT_COEFFS, 0);
    demod.mode(0);
  }

  switch (mode)	{
    case CWR:
      filter = LSB_NARROW;
      demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_37k, postfir_700, HILBERT_COEFFS, COEFF_700);
      show_radiomode("CWR");
      break;
    case SSB_LSB:
      filter = LSB_WIDE;
      demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_37k, postfir_lpf, HILBERT_COEFFS, COEFF_LPF);
      show_radiomode("LSB");
      break;
    case CW:
      filter = USB_NARROW;
      demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_37k, postfir_700, HILBERT_COEFFS, COEFF_700);
      show_radiomode("CW");
      break;
    case SSB_USB:
      filter = USB_WIDE;
      demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_37k, postfir_lpf, HILBERT_COEFFS, COEFF_LPF);
      show_radiomode("USB");
      break;
    case SSB_DSB:
      filter = DSB;
      demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_10k, postfir_3700hz, HILBERT_COEFFS, COEFF_3700);
      show_radiomode("DSB");
      break;
    case SSB_USB_N:
      filter = USB_1700;
      demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_37k, postfir_1700hz, HILBERT_COEFFS, COEFF_LPF);
      show_radiomode("nUSB");
      break;
    case SSB_LSB_N:
      filter = LSB_1700;
      demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_37k, postfir_1700hz, HILBERT_COEFFS, COEFF_LPF);
      show_radiomode("nLSB");
      break;
    case SSB_USB_W:
      filter = USB_3700;
      demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_37k, postfir_3700hz, HILBERT_COEFFS, COEFF_3700);
      show_radiomode("wUSB");
      break;
    case SSB_LSB_W:
      filter = LSB_3700;
      demod.begin_demod(RX_hilbert_I_37k, RX_hilbert_Q_37k, postfir_3700hz, HILBERT_COEFFS, COEFF_3700);
      show_radiomode("wLSB");
      break;
    case AM:
      filter = AM_BPF;
      demod.begin_demod(RX_hilbert_I_10k, RX_hilbert_Q_10k, FIR_PASSTHRU, HILBERT_COEFFS, 0);
      biquad.setBandpass(0, 400, 0.707);
      show_radiomode("AM");
      Audiochannelsetup(ROUTE_AM);
      break;


  }

  show_bandwidth(filter, IF_FREQ);

  AudioInterrupts();
}

/////////////////////////////////////////////////////////////////////
// SWITCH FILTER BAND
/////////////////////////////////////////////////////////////////////

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


/////////////////////////////////////////////////////////////////////
// WRITE FILTER TO PCF8575
/////////////////////////////////////////////////////////////////////

void lpf( int output ) {

  byte low = output & 0x00FF;
  byte high = output >> 8;

  char string[10];

  Wire.beginTransmission(PCF8575_ADDRESS);
  Wire.write(low);
  Wire.write(0xff);
  Wire.endTransmission(I2C_STOP, 50);         // blocking Tx with 50us timeout

  sprintf(string, "Filter:%i ", output );

  tft.setCursor(0, 200);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(1);
  tft.print(string);
  tft.setTextSize(2);

  abpf = output;

}

/////////////////////////////////////////////////////////////////////
// READ IO FROM PCF8575
/////////////////////////////////////////////////////////////////////

int read_pcf8575( void ) {

  byte low;
  byte high;

  Wire.beginTransmission(PCF8575_ADDRESS); ////who are you talking to?
  Wire.endTransmission(); //end communication
  Wire.requestFrom(PCF8575_ADDRESS, 2); //request two bytes of data
  if (Wire.available()) {
    low = Wire.receive(); //read byte 1
    high = Wire.receive(); //read byte 2
  }

  return high;
}


/////////////////////////////////////////////////////////////////////
// Sine/cosine generation function
// Adjust rad_calc *= 32;  p.e. ( 44100 / 128 * 32 = 11025 khz)
/////////////////////////////////////////////////////////////////////

void sinewave()
{
  uint     i;
  float32_t rad_calc;
  char string[10];   // print format stuff

  if (!flag)  {  // have we already calculated the sine wave?
    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {  // No, let's do it!
      rad_calc = (float32_t)i;    // convert to float the current position within the buffer
      rad_calc /= (AUDIO_BLOCK_SAMPLES);     // make this a fraction
      rad_calc *= (PI * 2);     // convert to radians
      rad_calc *= IF_FREQ * 128 / 44100;      // multiply by number of cycles that we want within this block ( 44100 / 128 * 32 = 11025 khz)
      //
      Osc_Q_buffer_i_f[i] = arm_cos_f32(rad_calc);  // get sine and cosine values and store in pre-calculated array
      Osc_I_buffer_i_f[i] = arm_sin_f32(rad_calc);
    }

    arm_float_to_q15(Osc_Q_buffer_i_f, Osc_Q_buffer_i, AUDIO_BLOCK_SAMPLES);
    arm_float_to_q15(Osc_I_buffer_i_f, Osc_I_buffer_i, AUDIO_BLOCK_SAMPLES);

#ifdef DEBUG

    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {

      sprintf(string, "%d ; %d",  Osc_I_buffer_i[i], Osc_Q_buffer_i[i] );
      Serial.println(string);
    }

#endif

    flag = 1; // signal that once we have generated the quadrature sine waves, we shall not do it again
  }


}
