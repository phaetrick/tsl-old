#pragma once
//
// Created by pr on 20.11.19.
//

#ifndef GRAINSTORM_PADSYNTH_H
#define GRAINSTORM_PADSYNTH_H



#include <NE10_types.h>
#include <atomic>
#include "types.h"

class PADsynth{
public:
    /*  PADsynth:
            N                - is the samplesize (eg: 262144)
            samplerate       - samplerate (eg. 44100)
            number_harmonics - the number of harmonics that are computed */
    PADsynth(int32_t N_,int samplerate_,int number_harmonics_);

    ~PADsynth();

    /* set the amplitude of the n'th harmonic */
    void setharmonic(int32_t n,float value);
    void computeHarmonics(float f);


    /* get the amplitude of the n'th harmonic */
    float getharmonic(int32_t n);

    /*  synth() generates the wavetable
        f           - the fundamental frequency (eg. 440 Hz)
        bw          - bandwidth in cents of the fundamental frequency (eg. 25 cents)
        bwscale     - how the bandwidth increase on the higher harmonics (recomanded value: 1.0)
        *smp        - a pointer to allocated memory that can hold N samples */
    void synth(float f,float bw,float bwscale,  float *out, int32_t tsl::app::bufsize_init);
    void synthcustom(float f,float bw,float bwscale,  const float *partialfact, const float *partialamps, float *out, int32_t tsl::app::bufsize_init);
protected:
    int32_t N;                  //Size of the sample

    /* IFFT() - inverse fast fourier transform
       YOU MUST IMPLEMENT THIS METHOD!
       *freq_real and *freq_imaginary represents the real and the imaginary part of the spectrum,
       The result should be in *smp array.
       The size of the *smp array is N and the size of the freq_real and freq_imaginary is N/2 */
    //virtual void IFFT(float *freq_real,float *freq_imaginary,float *smp)=0;


    /* relF():
        This method returns the N'th overtone's position relative
        to the fundamental frequency.
        By default it returns N.
        You may override it to make metallic sounds or other
        instruments where the overtones are not harmonic.  */
    virtual float relF(int32_t N);

    /* profile():
        This is the profile of one harmonic
        In this case is a Gaussian distribution (e^(-x^2))
        The amplitude is divided by the bandwidth to ensure that the harmonic
        keeps the same amplitude regardless of the bandwidth */
    virtual float profile(float fi, float bwi);

    /* RND() - a random number generator that
        returns values between 0 and 1
    */
    virtual float RND();

private:
    float *A;            //Amplitude of the harmonics
    float *freq_amp;     //Amplitude spectrum
    int32_t samplerate;
    int32_t number_harmonics;
    ne10_fft_cpx_float32_t *fftbuf;
    FFT *fft;
};

class GrainPADSynth{
public:
    GrainPADSynth(TRACK *track_, int32_t channel);
    ~GrainPADSynth(){
        delete padsynth;
    }

    void compute(float *in, int32_t size);

    static void Compute(GrainPADSynth *grainPadSynth, float *in, int32_t size){
        //if (grainPadSynth->bypass->load())
        //return;
        grainPadSynth->compute(in, size);
    }
private:
    TRACK *track;
    PADsynth *padsynth;
    std::atomic<double> *freq, *mix, *gain;
    std::atomic<bool> *bypass;
int32_t channel;
};


#endif //GRAINSTORM_PADSYNTH_H
