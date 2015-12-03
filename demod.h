/* Audio Library for Teensy 3.X
   Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com

   Development of this audio library was funded by PJRC.COM, LLC by sales of
   Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
   open source software by purchasing Teensy or other PJRC products.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice, development funding notice, and this permission
   notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#ifndef demod_h_
#define demod_h_
#include "AudioStream.h"
#include "utility/dspinst.h"
#include "arm_math.h"

#define FIR_PASSTHRU ((const short *) 1)

extern q15_t last_dc_level;
extern bool decimate_SSB;
extern bool decimate_AM;

#define FIR_MAX_COEFFS 200
#define DECIMATION_FACTOR_M 2
#define INTERPOLATE_FACTOR_L DECIMATION_FACTOR_M
#define RESAMPLED_AUDIO_BLOCK (AUDIO_BLOCK_SAMPLES / DECIMATION_FACTOR_M )


class AudioEffectdemod: public AudioStream
{
  public:
    AudioEffectdemod() : AudioStream(2, inputQueueArray), coeff_HILB_I(NULL), coeff_HILB_Q(NULL), coeff_FIR(NULL) {
    }
    void begin_demod(const short *cp1, const short *cp2, const short *cp3, int hilberts_coeffs, int fir_bpf_coeffs ) {
      coeff_HILB_I = cp1;
      coeff_HILB_Q = cp2;
      coeff_FIR = cp3;

      // Initialize FIR instance (ARM DSP Math Library)
      if (coeff_HILB_I && (coeff_HILB_I != FIR_PASSTHRU) && hilberts_coeffs <= FIR_MAX_COEFFS) {
        arm_fir_init_q15(&fir_inst1, hilberts_coeffs, (q15_t *)coeff_HILB_I, &HILB_I_State[0], AUDIO_BLOCK_SAMPLES);
      }

      if (coeff_HILB_Q && (coeff_HILB_Q != FIR_PASSTHRU) && hilberts_coeffs <= FIR_MAX_COEFFS) {
        arm_fir_init_q15(&fir_inst2, hilberts_coeffs, (q15_t *)coeff_HILB_Q, &HILB_Q_State[0], AUDIO_BLOCK_SAMPLES);
      }

      if (coeff_FIR && (coeff_FIR != FIR_PASSTHRU) && fir_bpf_coeffs <= FIR_MAX_COEFFS) {
        arm_fir_init_q15(&fir_inst3, fir_bpf_coeffs, (q15_t *)coeff_FIR, &fir_State[0], RESAMPLED_AUDIO_BLOCK);
      }

    }

    void begin_decimate(const short *cp1, const short *cp2, uint16_t decimate_numTaps, uint16_t interpolate_numTaps) {
      coeff_decimate = cp1;
      coeff_interpolate = cp2;

      // Initialize FIR decimate&interpolate instance (ARM DSP Math Library)

      arm_fir_decimate_init_q15   (&decimate_fir,     decimate_numTaps,    DECIMATION_FACTOR_M,   (q15_t *)coeff_decimate,   &dec_state[0],   AUDIO_BLOCK_SAMPLES);
      arm_fir_interpolate_init_q15(&interpolate_fir,  INTERPOLATE_FACTOR_L, interpolate_numTaps,  (q15_t *)coeff_interpolate, &inter_state[0], RESAMPLED_AUDIO_BLOCK);
    }

    void end(void) {
      coeff_HILB_I = NULL;
      coeff_HILB_Q = NULL;
      coeff_FIR = NULL;
    }

    void pass(void)
    {
      //coeff_FIR=FIR_PASSTHRU;
    }

    void mode(uint8_t m) {
      if (m < 0) m = 0;
      demod_mode = m;
    }

    virtual void update(void);

  private:
    audio_block_t *inputQueueArray[2];
    audio_block_t *blockI_HIL, *blockQ_HIL, *blockFIR,  *blockOUT, *blockOUT2;

    // pointer to current coefficients or NULL or FIR_PASSTHRU
    const short *coeff_HILB_I, *coeff_HILB_Q, *coeff_FIR;

    arm_fir_instance_q15 fir_inst1;
    arm_fir_instance_q15 fir_inst2;
    arm_fir_instance_q15 fir_inst3;

    q15_t HILB_I_State[AUDIO_BLOCK_SAMPLES + FIR_MAX_COEFFS];
    q15_t HILB_Q_State[AUDIO_BLOCK_SAMPLES + FIR_MAX_COEFFS];
    q15_t fir_State[RESAMPLED_AUDIO_BLOCK + FIR_MAX_COEFFS];

    uint8_t demod_mode;

    // decimate-interpolate

    const short *coeff_decimate, *coeff_interpolate;

    q15_t audio_buffer_in_I[RESAMPLED_AUDIO_BLOCK];
    q15_t audio_buffer_in_Q[RESAMPLED_AUDIO_BLOCK];

    q15_t audio_buffer_out[RESAMPLED_AUDIO_BLOCK];
    q15_t audio_buffer_fir[RESAMPLED_AUDIO_BLOCK];

    arm_fir_decimate_instance_q15 decimate_fir;
    arm_fir_interpolate_instance_q15 interpolate_fir;

    q15_t dec_state[4 + AUDIO_BLOCK_SAMPLES - 1];
    q15_t inter_state[16 / 4 + AUDIO_BLOCK_SAMPLES - 1];

    int16_t buffer[256] __attribute__ ((aligned (4)));
};

#endif

