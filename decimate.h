#ifndef decimate_h_
#define decimate_h__h_
#include "AudioStream.h"
#include "utility/dspinst.h"
#include "arm_math.h"

#define FIR_MAX_COEFFS 200
#define FIR_PASSTHRU ((const short *) 1)

class AudioEffectDecimate : public AudioStream
{
  public:
    AudioEffectDecimate() : AudioStream(1, inputQueueArray), dec_mode(0) { }

    // begin( decimate_coeffs, interpolate_coeffs, interpolate_coeffs, interpolate_coeffs, decimation_factor, interpolate_factor

    void begin(const short *cp1, const short *cp2, int decimate_coeffs, int interpolate_coeffs, uint8_t decimation_factor, uint8_t interpolate_factor ) {
      coeff_decimate = cp1;
      coeff_interpolate = cp2;

      // Initialize FIR decimate&interpolate instance (ARM DSP Math Library)
      
        arm_fir_decimate_init_q15   (&decimate_fir,    decimate_coeffs,    decimation_factor,  (q15_t *)coeff_decimate,    &dec_state[0],   AUDIO_BLOCK_SAMPLES);
        arm_fir_interpolate_init_q15(&interpolate_fir, interpolate_coeffs, interpolate_factor, (q15_t *)coeff_interpolate, &inter_state[0], AUDIO_BLOCK_SAMPLES);


    }

    // Decimate mode=0, interpolate mode=1
    void mode(uint8_t m) {
      if (m < 0) m = 0;
      dec_mode = m;
    }

    virtual void update(void);

  private:
    audio_block_t *inputQueueArray[1];

    // pointer to current coefficients or NULL or FIR_PASSTHRU
    const short *coeff_decimate, *coeff_interpolate;

    arm_fir_decimate_instance_q15 decimate_fir;
    arm_fir_interpolate_instance_q15 interpolate_fir;

    q15_t dec_state[AUDIO_BLOCK_SAMPLES + FIR_MAX_COEFFS];
    q15_t inter_state[AUDIO_BLOCK_SAMPLES + FIR_MAX_COEFFS];

    uint8_t dec_mode;
};


#endif
