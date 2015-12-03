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

#include "demod.h"
#include "utility/sqrt_integer.h"

bool decimate_SSB = 0;
bool decimate_AM = 0;


static void copy_to_AM_buffer(void *destination, const void *sourceI, const void *sourceQ)
{
  const uint16_t *srcI = (const uint16_t *)sourceI;
  const uint16_t *srcQ = (const uint16_t *)sourceQ;
  uint32_t *dst = (uint32_t *)destination;

  int i = AUDIO_BLOCK_SAMPLES / 4;

  while (i > 0) {
    //    *dst++ =  *srcI++ | ((*srcQ++) << 16);
    *dst++ = pack_16b_16b( *srcQ++, *srcI++);      // copy and apply GAIN
    *dst++ = pack_16b_16b( *srcQ++, *srcI++);      // copy and apply GAIN
    *dst++ = pack_16b_16b( *srcQ++, *srcI++);      // copy and apply GAIN
    *dst++ = pack_16b_16b( *srcQ++, *srcI++);      // copy and apply GAIN
    i--;
  }

}

void AudioEffectdemod::update(void)
{
  audio_block_t *blockI, *blockQ;

  q15_t mean;
  uint8_t i;

  blockI = receiveReadOnly(0);
  blockQ = receiveReadOnly(1);

  if (!blockI) {
    if (blockQ) release(blockQ);
    return;
  }
  if (!blockQ) {
    release(blockI);
    return;
  }
  // If there's no coefficient table, give up.
  if (coeff_HILB_I == NULL || coeff_HILB_Q == NULL) {
    release(blockI);
    release(blockQ);
    return;
  }

  // do passthru
  if (coeff_HILB_I == FIR_PASSTHRU || coeff_HILB_Q == FIR_PASSTHRU) {
    // Just passthrough
    transmit(blockI);
    transmit(blockQ);
    release(blockI);
    release(blockQ);
    return;
  }


  /////////////////////////////////////////////////////////////////////
  // SSB mode: apply 0 to I and +90º Q
  /////////////////////////////////////////////////////////////////////

  if (demod_mode == 1 || demod_mode == 2) {

    blockI_HIL = allocate();
    blockQ_HIL = allocate();
    if (blockI_HIL && blockQ_HIL)
    {
      arm_fir_fast_q15(&fir_inst1, (q15_t *)blockI->data, (q15_t *)blockI_HIL->data, AUDIO_BLOCK_SAMPLES);
      arm_fir_fast_q15(&fir_inst2, (q15_t *)blockQ->data, (q15_t *)blockQ_HIL->data, AUDIO_BLOCK_SAMPLES);
    }
    else  // error allocating memory, give up
    {
      release(blockI);
      release(blockQ);
      return;
    }
    release(blockI);
    release(blockQ);

    if (decimate_SSB)

    {
      arm_fir_decimate_fast_q15(&decimate_fir, (q15_t *)blockI_HIL->data, (q15_t *)audio_buffer_in_I, AUDIO_BLOCK_SAMPLES);
      arm_fir_decimate_fast_q15(&decimate_fir, (q15_t *)blockQ_HIL->data, (q15_t *)audio_buffer_in_Q, AUDIO_BLOCK_SAMPLES);

      blockOUT = allocate();
      if (blockOUT) {
        if (demod_mode == 1) // USB
          arm_add_q15( (q15_t *)audio_buffer_in_I, (q15_t *)audio_buffer_in_Q, (q15_t *)audio_buffer_out, RESAMPLED_AUDIO_BLOCK);
        else
          arm_sub_q15( (q15_t *)audio_buffer_in_I, (q15_t *)audio_buffer_in_Q, (q15_t *)audio_buffer_out, RESAMPLED_AUDIO_BLOCK);
      }
      else
      {
        release(blockI_HIL);
        release(blockQ_HIL);
        return;
      }
      release(blockI_HIL);
      release(blockQ_HIL);

      // Just passthrough if coeff_FIR == FIR_PASSTHRU

      if (coeff_FIR == FIR_PASSTHRU ) {

        transmit(blockOUT);
        release(blockOUT);
        return;
      }

      blockFIR = allocate();
      if (blockFIR)
        arm_fir_fast_q15(&fir_inst3, (q15_t *)audio_buffer_out, (q15_t *)audio_buffer_fir, RESAMPLED_AUDIO_BLOCK);
      else                                                                                   // error allocating memory, give up
      {
        transmit(blockOUT);
        release(blockOUT);
        return;
      }

      arm_fir_interpolate_q15(&interpolate_fir, (q15_t *)audio_buffer_fir, (q15_t *)blockFIR->data, RESAMPLED_AUDIO_BLOCK);

      transmit(blockFIR); // transmit demodulated data
      release(blockFIR);
      release(blockOUT);
      return;
    }
    else
    {
      blockOUT = allocate();
      if (blockOUT) {
        if (demod_mode == 1) // USB
          arm_add_q15( (q15_t *)blockI_HIL->data, (q15_t *)blockQ_HIL->data, (q15_t *)blockOUT->data, AUDIO_BLOCK_SAMPLES);
        else
          arm_sub_q15( (q15_t *)blockI_HIL->data, (q15_t *)blockQ_HIL->data, (q15_t *)blockOUT->data, AUDIO_BLOCK_SAMPLES);
      }
      else
      {
        release(blockI_HIL);
        release(blockQ_HIL);
        return;
      }
      release(blockI_HIL);
      release(blockQ_HIL);

      // Just passthrough if coeff_FIR == FIR_PASSTHRU

      if (coeff_FIR == FIR_PASSTHRU ) {

        transmit(blockOUT);
        release(blockOUT);
        return;
      }

      blockFIR = allocate();
      if (blockFIR)
        arm_fir_fast_q15(&fir_inst3, (q15_t *)blockOUT->data, (q15_t *)blockFIR->data, AUDIO_BLOCK_SAMPLES);
      else                                                                                   // error allocating memory, give up
      {
        transmit(blockOUT);
        release(blockOUT);
        return;
      }
      transmit(blockFIR); // transmit demodulated data
      release(blockFIR);
      release(blockOUT);
      return;

    }


  }
  else if (demod_mode == 0)  // AM mode
  {
    if (decimate_AM)
    {
      arm_fir_decimate_fast_q15(&decimate_fir, (q15_t *)blockI->data, (q15_t *)audio_buffer_in_I, AUDIO_BLOCK_SAMPLES);
      arm_fir_decimate_fast_q15(&decimate_fir, (q15_t *)blockQ->data, (q15_t *)audio_buffer_in_Q, AUDIO_BLOCK_SAMPLES);

      blockOUT = allocate();

      if (blockOUT) {

        copy_to_AM_buffer(buffer, audio_buffer_in_I, audio_buffer_in_Q);
        arm_cmplx_mag_squared_q15 ( buffer, audio_buffer_out, RESAMPLED_AUDIO_BLOCK);

        arm_mean_q15((q15_t *)audio_buffer_out, RESAMPLED_AUDIO_BLOCK, &mean);

        float avgdiff = (mean - last_dc_level);

        for (i = 0; i < RESAMPLED_AUDIO_BLOCK; i++) {

          q15_t dc_removal_level = (q15_t) (last_dc_level + avgdiff * ((float)i / RESAMPLED_AUDIO_BLOCK));

          audio_buffer_out[i] -= dc_removal_level;
        }

        last_dc_level = mean;
      }
      else
      {
        release(blockI);
        release(blockQ);
        return;
      }
      arm_fir_interpolate_q15(&interpolate_fir, (q15_t *)audio_buffer_out, (q15_t *)blockOUT->data, RESAMPLED_AUDIO_BLOCK);
      transmit(blockOUT);
      release(blockOUT);
      release(blockI);
      release(blockQ);
    }
    else
    {
      blockOUT = allocate();

      if (blockOUT ) {

        copy_to_AM_buffer(buffer, blockI->data, blockQ->data);
        arm_cmplx_mag_q15( buffer, blockOUT->data, AUDIO_BLOCK_SAMPLES);
        arm_mean_q15((q15_t *)blockOUT->data, AUDIO_BLOCK_SAMPLES, &mean);

        float avgdiff = (mean - last_dc_level);

        for (i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {

          q15_t dc_removal_level = (q15_t) (last_dc_level + avgdiff * ((float)i / AUDIO_BLOCK_SAMPLES));

          blockOUT->data[i] -= dc_removal_level;
        }

        last_dc_level = mean;

      }
      else
      {
        release(blockI);
        release(blockQ);
        return;
      }
      transmit(blockOUT);
      release(blockOUT);
      release(blockI);
      release(blockQ);
    }
  }
}

