//
// Created by pr on 20.11.19.
//

#include <cstdlib>
#include <cmath>
#include "PADsynth.h"
#include "ffttools.h"
#include "tools.h"
#include "track.h"

PADsynth::PADsynth(int32_t N_, int samplerate_, int number_harmonics_){
    N=next_pow_2(N_);
    fft = new FFT(N);
    samplerate=samplerate_;
    number_harmonics=number_harmonics_;
    A=new float [number_harmonics];
    freq_amp=new float[N/2];

    fftbuf = (ne10_fft_cpx_float32_t*) aligned_calloc((N) * sizeof(float));
};

PADsynth::~PADsynth(){
    delete[] A;
    delete[] freq_amp;
    aligned_free(fftbuf);
    delete fft;
};

float PADsynth::relF(int32_t N){
    return N;
};

void PADsynth::setharmonic(int32_t n,float value){
    if ((n<1)||(n>=number_harmonics)) return;
    A[n]=value;
};

void PADsynth::computeHarmonics(float f1){
    for (int32_t i=0;i<number_harmonics;i++) A[i]=0.0;
    A[1]=1.0;//default, the first harmonic has the amplitude 1.0
    for (int32_t i=1;i<number_harmonics;i++) {
        A[i]=1.0/i;
        float formants=exp(-pow((i*f1-600.0)/150.0,2.0))+exp(-pow((i*f1-900.0)/250.0,2.0))+
                       exp(-pow((i*f1-2200.0)/200.0,2.0))+exp(-pow((i*f1-2600.0)/250.0,2.0))+
                       exp(-pow((i*f1)/3000.0,2.0))*0.1;
        A[i]*=formants;
    };
}

float PADsynth::getharmonic(int32_t n){
    if ((n<1)||(n>=number_harmonics)) return 0.0;
    return A[n];
};

float PADsynth::profile(float fi, float bwi){
    float x=fi/bwi;
    x*=x;
    if (x>14.71280603) return 0.0;//this avoids computing the e^(-x^2) where it's results are very close to zero
    return exp(-x)/bwi;
};

extern "C" {
#include <libavcodec/avfft.h>
}


void PADsynth::synth(float f,float bw,float bwscale,  float *out, int32_t tsl::app::bufsize_init){
    int32_t i,nh;
    int32_t size = next_pow_2(tsl::app::bufsize_init);
    for (i=0;i<size/2;i++) freq_amp[i]=0.0;//default, all the frequency amplitudes are zero
    computeHarmonics(f);
    for (nh=1;nh<number_harmonics;nh++){//for each harmonic
        float bw_Hz;//bandwidth of the current harmonic measured in Hz
        float bwi;
        float fi;
        float rF=f*relF(nh);

        bw_Hz=(powf(2.f,bw/1200.f)-1.f)*f*powf(relF(nh),bwscale);

        bwi=bw_Hz/(2.f*samplerate);
        fi=rF/samplerate;
        for (i=0;i<size/2;i++){//here you can optimize, by avoiding to compute the profile for the full frequency (usually it's zero or very close to zero)
            float hprofile;
            hprofile=profile((i/(float)N)-fi,bwi);
            freq_amp[i]+=hprofile*A[nh];
        };
    };
    //Convert the freq_amp array to complex array (real/imaginary) by making the phases random
    for (i=0;i<size/2;i++){
        float phase=RND()*TWOPI_F_P;
        fftbuf[i].r=freq_amp[i]*cosf(phase);
        fftbuf[i].i=freq_amp[i]*sinf(phase);
    };

    //FFT *fft = track->ffts[channel][(int) (LOG2(size))];
    //RDFTContext *ctx = av_rdft_init ((int) log2(size), IDFT_C2R);
    //av_rdft_calc 	(ctx, (FFTSample*) fftbuf);
    //av_rdft_end 	(ctx);
    fft->backward((float*) fftbuf, (float*) fftbuf);

    memcpy(out, fftbuf, sizeof(float) * tsl::app::bufsize_init);
    //normalize the output
    float max=0.0;
    for (i=0;i<tsl::app::bufsize_init;i++) if (fabs(out[i])>max) max=fabs(out[i]);
    if (max<1e-5) max=1e-5;
    for (i=0;i<size;i++) out[i]/=max*1.4142;

};

void PADsynth::synthcustom(float f,float bw,float bwscale,  const float *partialfact, const float *partialamps, float *out, int32_t tsl::app::bufsize_init){
    int32_t i,nh;
    int32_t size = next_pow_2(tsl::app::bufsize_init);
    for (i=0;i<size/2;i++) freq_amp[i]=0.0;//default, all the frequency amplitudes are zero
    //computeHarmonics(f);
    for (nh=0;nh<number_harmonics;nh++){//for each harmonic
        float bw_Hz;//bandwidth of the current harmonic measured in Hz
        float bwi;
        float fi;
        float rF=f*partialfact[nh];

        bw_Hz=(powf(2.f,bw/1200.f)-1.f)*f*powf(relF(nh),bwscale);

        bwi=bw_Hz/(2.f*samplerate);
        fi=rF/samplerate;
        for (i=0;i<size/2;i++){//here you can optimize, by avoiding to compute the profile for the full frequency (usually it's zero or very close to zero)
            float hprofile;
            hprofile=profile((i/(float)N)-fi,bwi);
            freq_amp[i]+=hprofile*partialamps[nh];
        };
    };
    //Convert the freq_amp array to complex array (real/imaginary) by making the phases random
    for (i=0;i<size/2;i++){
        float phase=RND()*TWOPI_F_P;
        fftbuf[i].r=freq_amp[i]*cosf(phase);
        fftbuf[i].i=freq_amp[i]*sinf(phase);
    };

    //FFT *fft = track->ffts[channel][(int) (LOG2(size))];
    //RDFTContext *ctx = av_rdft_init ((int) log2(size), IDFT_C2R);
    //av_rdft_calc 	(ctx, (FFTSample*) fftbuf);
    //av_rdft_end 	(ctx);
    fft->backward((float*) fftbuf, (float*) fftbuf);

    memcpy(out, fftbuf, sizeof(float) * tsl::app::bufsize_init);
    //normalize the output
    float max=0.0;
    for (i=0;i<tsl::app::bufsize_init;i++) if (fabs(out[i])>max) max=fabs(out[i]);
    if (max<1e-5) max=1e-5;
    for (i=0;i<size;i++) out[i]/=max*1.4142;

};

float PADsynth::RND(){
    return (rand()/(RAND_MAX+1.0f));
};
#define number_harmonics 64

/*
#define number_harmonics 64
int32_t main(){
    srandom(time(0));
    REALTYPE A[number_harmonics];A[0]=0.0;//A[0] is not used
    for (int32_t note=0;note<=24;note+=4){
        REALTYPE f1=130.81*pow(2,note/12.0);
        printf("Generating frequency: %d Hz\n",(int)f1);
        for (int32_t i=1;i<number_harmonics;i++) {
            A[i]=1.0/i;
            REALTYPE formants=exp(-pow((i*f1-600.0)/150.0,2.0))+exp(-pow((i*f1-900.0)/250.0,2.0))+
                              exp(-pow((i*f1-2200.0)/200.0,2.0))+exp(-pow((i*f1-2600.0)/250.0,2.0))+
                              exp(-pow((i*f1)/3000.0,2.0))*0.1;
            A[i]*=formants;
        };
        padsynth_basic_algorithm(N,44100,f1,60.0,number_harmonics,A,sample);

        short int32_t isample[N];
        for (int32_t i=0;i<N;i++) isample[i]=(int)(sample[i]*32768.0);
        char filename[100];
        sprintf(filename,"note%02d",note);
        FILE *f=fopen(filename,"w");fwrite(isample,N,2,f);fclose(f);
    };
};
*/

GrainPADSynth::GrainPADSynth(TRACK *_track, int32_t _channel){
    track = _track;
    channel = _channel;
    padsynth = new PADsynth(track->p->sr, track->p->sr, 8);
};

void GrainPADSynth::compute(float *in, int32_t size) {
    float fr = LOG2NORMAL(getInstance()->miditargets[GRAINMODALFREQ].reference[0]->load());
    FFT *fft = track->ffts[channel][(int) (LOG2(next_pow_2(size)))];
    padsynth->synth(fr, 60, 1, in, size);
}


