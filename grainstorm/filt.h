#pragma once
#ifndef _FILTER_H
#define _FILTER_H

#include <tools.h>
#include <defines.h>
#include "grainstorm.h"
#include "app.h"

#define NTAPS_FIRSPLIT 127

class StereoMix : public Effect{

public:
    StereoMix(TRACK *t) : Effect(t, SPACE_MONOSTEREO, STEREOEFFECT){
      _bypass = &t->bypass[SPACE_MONOSTEREO];
    }
    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) override {
        const MYFLOAT mix = (_bypass->load() || destroyRequested) ? 0.0 :1.0;
        const MYFLOAT spr = .5 * cos(PI_P * .5 * _STATE->params[_track->index][STEREOWIDTH].load());
        for(int32_t i=0;i<s;i++){
            const MYFLOAT orig = 1. - _spread * _smooth1;
            auto l = inl[i], r = inr[i];
            outl[i] = l * orig + r * _spread;
            outr[i] = r * orig + l * _spread;
            sm1(mix);
            _spread = smoothCoeff * (_spread - spr) + spr;

        }
    }

private:
    MYFLOAT _spread{0};
};


template<typename T>
struct FirDesigner{

// This gets used with the Kaiser window.
	static T Bessel(T x)
	{
		T Sum=0.0, XtoIpower;
		int32_t i, j, Factorial;
		for(i=1; i<10; i++)
		{
			XtoIpower = pow(x/2.0, (T)i);
			Factorial = 1;
			for(j=1; j<=i; j++)Factorial *= j;
			Sum += pow(XtoIpower / (T)Factorial, 2.0);
		}
		return(1.0 + Sum);
	}

//-----------------------------------------------------------------------------

// This gets used with the Sinc window.
	static T Sinc(T x)
	{
		if(x > -1.0E-5 && x < 1.0E-5)return(1.0);
		return(sin(x)/x);
	}

	enum TFIRPassTypes{
		firLPF = 0,
		firHPF = 1,
		firBPF = 2,
		firNOTCH = 3,
		firALLPASS =4
	};
	// Rectangular Windowed FIR. The equations used here are developed in numerous textbooks.
	static void RectWinFIR(T *FirCoeff, int32_t NumTaps, TFIRPassTypes PassType, T OmegaC, T BW)
	{
		int32_t j;
		T Arg, OmegaLow, OmegaHigh;

		switch(PassType)
		{
			case firLPF:    // Low Pass
				for(j=0; j<NumTaps; j++)
				{
					Arg = (T)j - (T)(NumTaps-1) / 2.0;
					FirCoeff[j] = OmegaC * Sinc(OmegaC * Arg * PI_P);
				}
				break;

			case firHPF:     // High Pass
				if(NumTaps % 2 == 1) // Odd tap counts
				{
					for(j=0; j<NumTaps; j++)
					{
						Arg = (T)j - (T)(NumTaps-1) / 2.0;
						FirCoeff[j] = Sinc(Arg * PI_P) - OmegaC * Sinc(OmegaC * Arg * PI_P);
					}
				}

				else  // Even tap counts
				{
					for(j=0; j<NumTaps; j++)
					{
						Arg = (T)j - (T)(NumTaps-1) / 2.0;
						if(Arg == 0.0)FirCoeff[j] = 0.0;
						else FirCoeff[j] = cos(OmegaC * Arg * PI_P) / PI_P / Arg  + cos(Arg * PI_P);
					}
				}
				break;

			case firBPF:   // Band Pass
				OmegaLow  = OmegaC - BW/2.0;
				OmegaHigh = OmegaC + BW/2.0;
				for(j=0; j<NumTaps; j++)
				{
					Arg = (T)j - (T)(NumTaps-1) / 2.0;
					if(Arg == 0.0)FirCoeff[j] = 0.0;
					else FirCoeff[j] =  ( cos(OmegaLow * Arg * PI_P) - cos(OmegaHigh * Arg * PI_P) ) / PI_P / Arg ;
				}
				break;

			case firNOTCH:  // Notch,  if NumTaps is even, the response at Pi is attenuated.
				OmegaLow  = OmegaC - BW/2.0;
				OmegaHigh = OmegaC + BW/2.0;
				for(j=0; j<NumTaps; j++)
				{
					Arg = (T)j - (T)(NumTaps-1) / 2.0;
					FirCoeff[j] =  Sinc(Arg * PI_P) - OmegaHigh * Sinc(OmegaHigh * Arg * PI_P) - OmegaLow * Sinc(OmegaLow * Arg * PI_P);
				}
				break;

			case firALLPASS: // All Pass, this is trivial, but it shows how an fir all pass (delay) can be done.
				for(j=0; j<NumTaps; j++)FirCoeff[j] = 0.0;
				FirCoeff[(NumTaps-1) / 2] = 1.0;
				break;
		}
		// Now use the FIRFilterWindow() function to reduce the sinc(x) effects.
	}
    enum TWindowType{
		wtNONE = 0,
		wtKAISER = 1,
		wtSINC = 2,
		wtSINE = 3
	};
// Used to reduce the sinc(x) effects on a set of FIR coefficients. This will, unfortunately,
// widen the filter's transition band, but the stop band attenuation will improve dramatically.
	static void FIRFilterWindow(T *FIRCoeff, int32_t N, TWindowType WindowType, T Beta)
	{
		if(WindowType == wtNONE) return;

		int32_t j;
		T dN, *WinCoeff;

		if(Beta < 0.0)Beta = 0.0;
		if(Beta > 10.0)Beta = 10.0;

		WinCoeff  = new T[N+2];
		if(WinCoeff == NULL)
		{
			// ShowMessage("Failed to allocate memory in WindowData() ");
			return;
		}

		// Calculate the window for N/2 points, then fold the window over (at the bottom).
		dN = N + 1; // a MYFLOAT
		if(WindowType == wtKAISER)
		{
			T Arg;
			for(j=0; j<N; j++)
			{
				Arg = Beta * sqrt(1.0 - pow( ((T)(2*j+2) - dN) / dN, 2.0) );
				WinCoeff[j] = Bessel(Arg) / Bessel(Beta);
			}
		}

		else if(WindowType == wtSINC)  // Lanczos
		{
			for(j=0; j<N; j++)WinCoeff[j] = Sinc((T)(2*j+1-N)/dN * PI_P );
			for(j=0; j<N; j++)WinCoeff[j] = pow(WinCoeff[j], Beta);
		}

		else if(WindowType == wtSINE)  // Hanning if Beta = 2
		{
			for(j=0; j<N/2; j++)WinCoeff[j] = sin((T)(j+1) * PI_P / dN);
			for(j=0; j<N/2; j++)WinCoeff[j] = pow(WinCoeff[j], Beta);
		}

		else // Error.
		{
			// ShowMessage("Incorrect window type in WindowFFTData");
			delete[] WinCoeff;
			return;
		}

		// Fold the coefficients over.
		for(j=0; j<N/2; j++)WinCoeff[N-j-1] = WinCoeff[j];

		// Apply the window to the FIR coefficients.
		for(j=0; j<N; j++)FIRCoeff[j] *= WinCoeff[j];

		delete[] WinCoeff;

	}
};


#include "SpectrumAnalyzer.h"

template<typename T>
class MultiBandCompressor : public Effect{
public:
    static constexpr int32_t Nbands= 3;
    MultiBandCompressor(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_MULTICOMP, MONOEFFECT), splitter(t->_STATE){
        _bypass = &t->bypass[SPACE_MULTICOMP];
        _smooth1 = 0;
        for(int i=0;i<Nbands;i++)env[i] = 1.0;

    }

    void compute(MYFLOAT *in, int32_t s) override {
/*
            splitter.check(LOG2NORMALF(_STATE->params[_track->index][MULTICOMPCROSS1]), LOG2NORMALF(_STATE->params[_track->index][MULTICOMPCROSS2]));
            int32_t size = 1024;
            std::vector<MYFLOAT> tmpvec;
            tmpvec.resize(size*2);
            T tmp[3];
            splitter.tick(1.0, tmp);
            tmpvec[0] = tmp[0] + tmp[1] +  tmp[2];
            for(int32_t i=1;i<size*2; i++){
                splitter.tick(0.0, tmp);
                tmpvec[i] = tmp[0] + tmp[1] +  tmp[2];
            }
            FFT fft(size*2);
            fft.forward(tmpvec.data(), tmpvec.data());
            for(int32_t i=0;i<size; i++){
                tmpvec[i] = std::abs(std::complex<MYFLOAT>(tmpvec[i*2], tmpvec[i*2+1]));
                tmpvec[i] = tmpvec[i] > 0 ? LOG10D20F(tmpvec[i]) : -60;
                if (tmpvec[i] < -60) tmpvec[i] = -60;
                else if (tmpvec[i] > 60)
                tmpvec[i] = 60;

            }
            tmpvec.resize(size);
            SpectrumAnalyzer::toUI[0].push(std::move(tmpvec));

        return;
*/
        check();
        const MYFLOAT mix = *_bypass || destroyRequested ? 0.0 : 1.0;
        const MYFLOAT gain = dbToLinear60(_STATE->params[_track->index][MULTICOMPGAIN].load());
        for (int32_t i=0;i<s;i++){
            in[i] = (in[i] * (1.f - _smooth1) + tick(in[i])* _smooth1 * _smooth2);
            smmixgain(mix, gain);
        }
    }
private:
    inline T tick(T Signal)
    {
        T split[Nbands];
        splitter.tick(Signal, split);
        T output = 0;
        for(int i=0;i<Nbands;i++){
            auto rms = rMs[i].push(split[i]);
            const T x_G = rms == 0.0 ? -120. : LOG10D20(rms);
            T overshoot = x_G - ThresholddB[i];
            T y_G;
            if (x_G <= lowKnee[i])
                y_G = x_G;
            else if ((x_G > lowKnee[i]) && (x_G < highKnee[i]))
                y_G = x_G + slope[i] * std::pow(overshoot + SoftKnee[i] * .5, 2.) / (2. * SoftKnee[i]);
            else // if (x_G > Threshold + knee / 2)
                y_G = x_G + slope[i] * overshoot;
            T x_T = y_G - x_G;

            if (x_T > env[i])
                env[i] = attackDelta[i] * env[i] + (1. - attackDelta[i]) * x_T;
            else
                env[i] = releaseDelta[i] * env[i] + (1. - releaseDelta[i]) * x_T;

            UDD(env[i]);

            output += split[i] * LOG2NORMAL(MakeUp[i] - env[i]);
        }
        return output;
    }
private:
    T oldcross1{}, oldcross2{};
    ThreeBandSplitter<MYFLOAT> splitter;
    void check(){
        auto p1 = LOG2NORMAL(_STATE->params[_track->index][MULTICOMPCROSS1].load());
        auto p2 = LOG2NORMAL(_STATE->params[_track->index][MULTICOMPCROSS2].load());
        if(p1>p2)
            std::swap(p1,p2);
        if(p1 != oldcross1 || p2 != oldcross2){
            oldcross1 = p1;
            oldcross2 = p2;
            splitter.check(p1, p2);
        }

        // Attack/Release params are in ms, the one-pole coefficient needs samples
        // (as in compfullmono::setAttack). A sample-rate change therefore has to
        // force a recompute even when the params themselves have not moved.
        const auto sr = _STATE->sr;
        const bool srChanged = sr != oldsr;
        if (srChanged)
            oldsr = sr;

        for(int32_t i=0;i<Nbands;i++){
            bool update = false;
            auto att = _STATE->params[_track->index][MULTICOMPATTACK1+i].load();
            if(Attack[i] != att || srChanged){
                Attack[i] = att;
                const auto attSmpl = MS2SMPL(LOG2NORMAL(Attack[i]), sr);
                if (attSmpl > 0)
                    attackDelta[i] = exp(-1.0 / attSmpl);
                else
                    attackDelta[i] = 0;
            }
            auto rel = _STATE->params[_track->index][MULTICOMPRELEASE1+i].load();
            if(rel != Release[i] || srChanged){

                Release[i] = rel;
                const auto relSmpl = MS2SMPL(LOG2NORMAL(Release[i]), sr);
                if (relSmpl > 0)
                    releaseDelta[i] = exp(-1.0 / relSmpl);
                else
                    releaseDelta[i] = 0.0;

            }
            auto thr = _STATE->params[_track->index][MULTICOMPTHR1+i].load();
            if(thr != ThresholddB[i]){
                ThresholddB[i] = thr;
                update = true;
            }
            MakeUp[i] = _STATE->params[_track->index][MULTICOMPMAKE1+i].load();

            auto knee = _STATE->params[_track->index][MULTICOMPKNEE1+i].load();
            if(SoftKnee[i] != knee){
                SoftKnee[i] = knee;
                //log_soft[i] = std::log(LOG2NORMAL(SoftKnee[i]));
                update = true;
            }
            auto rat = _STATE->params[_track->index][MULTICOMPRATIO1+i].load();
            if(oldrat[i] != rat){
                oldrat[i] = rat;
               // auto r = -(1. - 1. / ((.1 + .9 * rat) * 10.));
                slope[i] = 1. / (1. / (1. + rat)) - 1.; //
            }


            if(update)
            {
               // lowClip[i] = Threshold[i] * LOG2NORMALF(-SoftKnee[i]);
               // highClip[i] = Threshold[i] * LOG2NORMALF(SoftKnee[i]);
                lowKnee[i] = ThresholddB[i] - SoftKnee[i] * .5;
                highKnee[i] = ThresholddB[i] + SoftKnee[i] * .5;
            }

//LOGE("%d env %g thresdb %g attdelt %g reldelt %g lowknee %g highknee %g softknee %g slope %g make %g thresh %g attack %g release %g oldrat %g ", i, env[i],
  //      ThresholddB[i], attackDelta[i], releaseDelta[i], lowKnee[i], highKnee[i], SoftKnee[i], slope[i], MakeUp[i],
    //         Threshold[i], Attack[i], Release[i],  oldrat[i]);

        }
    }

    T env[Nbands];
    T ThresholddB[Nbands]{}, attackDelta[Nbands]{}, releaseDelta[Nbands]{}, lowKnee[Nbands]{}, highKnee[Nbands]{}, SoftKnee[Nbands]{}, slope[Nbands]{}, MakeUp[Nbands]{};
    T lowClip[Nbands]{}, highClip[Nbands]{}, Threshold[Nbands]{}, Attack[Nbands]{}, Release[Nbands]{}, log_soft[Nbands],  oldrat[Nbands]{};
    MYFLOAT oldsr{};
    template<uint32_t RMSSIZE, typename INDEX_TYPE = uint32_t>
    class rms {
    public:
        static constexpr bool isPowerOfTwo(uint32_t n) { return (n & (n - 1)) == 0; }

        static_assert(isPowerOfTwo(RMSSIZE), "Capacity must be a power of 2");
        static_assert(std::is_unsigned<INDEX_TYPE>::value, "Index type must be unsigned");

        inline T push(T obj) {
            _sum -= _mem[_index];
            auto temp = obj * obj;
            _sum += temp;
            _mem[_index] = temp;
            _index++;
            _index &= (RMSSIZE - 1);
            return std::sqrt(_sum * _onedN);
        }

        int32_t size() const { return RMSSIZE; }

    private:
        INDEX_TYPE _index{};
        T _sum{};
        T _mem[RMSSIZE]{};
        const T _onedN{1. / (T) RMSSIZE};

        INDEX_TYPE mask(INDEX_TYPE n) const {
            return static_cast<INDEX_TYPE>(n & (RMSSIZE - 1));
        }
    };
    rms<1024> rMs[Nbands];
};

/*
enum filterType {LPF, HPF, BPF};

template<typename T>
class Filter{
public:
// Handles LPF and HPF case
	Filter(filterType filt_t, int32_t num_taps, T Fs, T Fx)
	{
		m_filt_t = filt_t;
		m_num_taps = num_taps;
		m_Fs = Fs;
		m_Fx = Fx;
		m_lambda = PI_P * Fx / (Fs/2);

		m_taps.resize(m_num_taps, 0);
		m_STATE->sr.resize(m_num_taps, 0 );


		if( m_filt_t == LPF ) designLPF();
		else if( m_filt_t == HPF ) designHPF();
		return;
	}

// Handles BPF case
	Filter(filterType filt_t, int32_t num_taps, T Fs, T Fl,
				   T Fu)
	{
		m_filt_t = filt_t;
		m_num_taps = num_taps;
		m_Fs = Fs;
		m_Fx = Fl;
		m_Fu = Fu;
		m_lambda = PI_P * Fl / (Fs/2);
		m_phi = PI_P * Fu / (Fs/2);

		m_taps.resize(m_num_taps, 0);
		m_STATE->sr.resize(m_num_taps, 0 );

		designBPF();
	}

	void
	get_taps( T *taps )
	{
		for(int32_t i = 0; i < m_num_taps; i++) taps[i] = m_taps[i];
	}

// Output the magnitude of the frequency response in dB
#define NP 1000
	int
	write_freqres_to_file( char *filename )
	{
		FILE *fd;
		int32_t i, k;
		T w, dw;
		T y_r[NP], y_i[NP], y_mag[NP];
		T mag_max = -1;
		T tmp_d;


		dw = PI_P / (NP - 1.0);
		for(i = 0; i < NP; i++){
			w = i*dw;
			y_r[i] = y_i[i] = 0;
			for(k = 0; k < m_num_taps; k++){
				y_r[i] += m_taps[k] * cos(k * w);
				y_i[i] -= m_taps[k] * sin(k * w);
			}
		}

		for(i = 0; i < NP; i++){
			y_mag[i] = sqrt( y_r[i]*y_r[i] + y_i[i]*y_i[i] );
			if( y_mag[i] > mag_max ) mag_max = y_mag[i];
		}

		if( mag_max <= 0.0 ) return -2;

		fd = fopen(filename, "w");
		if( fd == NULL ) return -3;

		for(i = 0; i < NP; i++){
			w = i*dw;
			if( y_mag[i] == 0 ) tmp_d = -100;
			else{
				tmp_d = 20 * log10( y_mag[i] / mag_max );
				if( tmp_d < -100 ) tmp_d = -100;
			}
			fprintf(fd, "%10.6e %10.6e\n", w * (m_Fs/2)/PI_P, tmp_d);
		}

		fclose(fd);
		return 0;
	}

	T
	tick(T data_sample)
	{
		for(int32_t i = m_num_taps - 1; i >= 1; i--){
			m_STATE->sr[i] = m_STATE->sr[i-1];
		}
		m_STATE->sr[0] = data_sample;
		T result = 0;
		for(int32_t i = 0; i < m_num_taps; i++) result += m_STATE->sr[i] * m_taps[i];
		return result;
	}
	private:
		filterType m_filt_t;
		int32_t m_num_taps;
		T m_Fs;
		T m_Fx;
		T m_lambda;
		tsl::AlignedVector<T> m_taps;
	    tsl::AlignedVector<T> m_STATE->sr;
	void
	designLPF()
	{
		int32_t n;
		T mm;

		for(n = 0; n < m_num_taps; n++){
			mm = n - (m_num_taps - 1.0) / 2.0;
			if( mm == 0.0 ) m_taps[n] = m_lambda / PI_P;
			else m_taps[n] = sin( mm * m_lambda ) / (mm * PI_P);
		}

		return;
	}

	void
	designHPF()
	{
		int32_t n;
		T mm;

		for(n = 0; n < m_num_taps; n++){
			mm = n - (m_num_taps - 1.0) / 2.0;
			if( mm == 0.0 ) m_taps[n] = 1.0 - m_lambda / PI_P;
			else m_taps[n] = -sin( mm * m_lambda ) / (mm * PI_P);
		}

		return;
	}

	void
	designBPF()
	{
		int32_t n;
		T mm;

		for(n = 0; n < m_num_taps; n++){
			mm = n - (m_num_taps - 1.0) / 2.0;
			if( mm == 0.0 ) m_taps[n] = (m_phi - m_lambda) / PI_P;
			else m_taps[n] = (   sin( mm * m_phi ) -
								 sin( mm * m_lambda )   ) / (mm * PI_P);
		}

		return;
	}

	// Only needed for the bandpass filter case
		T m_Fu, m_phi;

	};
*/
#endif
