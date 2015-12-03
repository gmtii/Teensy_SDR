#include "decimate.h"

void AudioEffectDecimate::update(void)
{
  audio_block_t *blockINPUT, *blockOUTPUT;

  blockINPUT = receiveReadOnly(0);

  if (!blockINPUT) {
    release(blockINPUT);
    return;
  }

  if (dec_mode == 0 ) {

    blockOUTPUT = allocate();
    if (blockOUTPUT)
      arm_fir_decimate_q15(&decimate_fir, (q15_t *)blockOUTPUT->data, (q15_t *) blockINPUT->data, AUDIO_BLOCK_SAMPLES);
    else  // error allocating memory, give up
    {
      release(blockINPUT);
      return;
    }
    transmit(blockOUTPUT);

  }
  else
  {
    blockOUTPUT = allocate();
    if (blockOUTPUT)
      arm_fir_interpolate_q15(&interpolate_fir, (q15_t *)blockOUTPUT->data, (q15_t *) blockINPUT->data, AUDIO_BLOCK_SAMPLES);
    else  // error allocating memory, give up
    {
      release(blockINPUT);
      return;
    }
    transmit(blockOUTPUT);


  }
  release(blockOUTPUT);
  release(blockINPUT);

}
