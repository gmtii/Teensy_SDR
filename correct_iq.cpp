/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Corrige imbalance de I,Q por diferencia de ganancia y fase.

#include "correct_iq.h"

float I_gain=0.5;
float R_xgain;
float Q_gain;
int8_t RF_gain;
bool iq_correction;

void AudioEffectCorrectIQ::update(void)
{
  audio_block_t *blockI, *blockQ;

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


  if (!iq_correction)
  {

    arm_scale_q15  ( (q15_t *)blockI->data, 0x7FFF, RF_gain - 1, (q15_t *)blockI->data, AUDIO_BLOCK_SAMPLES);
    arm_scale_q15  ( (q15_t *)blockQ->data, 0x7FFF, RF_gain - 1, (q15_t *)blockQ->data, AUDIO_BLOCK_SAMPLES);

    transmit(blockI, 0);
    transmit(blockQ, 1);
    release(blockI);
    release(blockQ);
    return;
  }

  blockI_out = allocate();
  blockQ_out = allocate();

  if ( blockI_out && blockQ_out)
  {
    // I * I_Gain * RF_Gain
   
    arm_scale_q15  ( (q15_t *)blockI->data, (I_gain * 0x7FFF), RF_gain, (q15_t *)blockI_out->data, AUDIO_BLOCK_SAMPLES);

    // I * R_xgain
    arm_scale_q15  ( (q15_t *)blockI->data, (R_xgain * 0x7FFF), 0, (q15_t *)phase_adjust, AUDIO_BLOCK_SAMPLES);

    // (I * R_xgain)+Q
    arm_add_q15 ( (q15_t *)phase_adjust, (q15_t *)blockQ->data, (q15_t *)Q_in_temp, AUDIO_BLOCK_SAMPLES);

    // ((I * R_xgain)+Q)*Q_gain
    arm_scale_q15  ( (q15_t *)Q_in_temp, (Q_gain * 0x7FFF), RF_gain, (q15_t *)blockQ_out->data, AUDIO_BLOCK_SAMPLES);

  }
  else
  {
    release(blockI);
    release(blockQ);
    return;
  }
  transmit(blockI_out, 0);
  transmit(blockQ_out, 1);
  release(blockI_out);
  release(blockQ_out);
  release(blockI);
  release(blockQ);
}
