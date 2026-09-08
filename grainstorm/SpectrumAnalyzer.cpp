//
// Created by pr on 08.11.22.
//

#include "SpectrumAnalyzer.h"
#include "track.h"
#include "app.h"
#include "grainstorm.h"
// This is a single frequency DFT.
// This code uses iteration to calculate the Twiddle factors.
// To evaluate the frequency response of an FIR filter at Omega, set
// Samples[] = FirCoeff[]   N = NumTaps  0.0 <= Omega <= 1.0
// 256 pts in 15.6 us
template<typename T>
static T freq_at_x (const T x, const T m0_width) {
    return 20. * pow (1000., x / m0_width);
}

template<typename T>
static T SingleFreqDFT(T *Samples, int32_t N, T Omega)
{
    T TwiddleR =  cos(Omega * PI_P);
    T TwiddleI = -sin(Omega * PI_P);
    T zR = 1.0;    // z, as in e^(j*omega)
    T zI = 0.0;
    T SumR = 0.0;
    T SumI = 0.0;

    for(int32_t k=0; k<N; k++)
    {
        SumR += Samples[k] * zR;
        SumI += Samples[k] * zI;

        // Calculate the complex exponential z by taking it to the kth power.
        T Temp = zR * TwiddleR - zI * TwiddleI;
        zI =   zR * TwiddleI + zI * TwiddleR;
        zR = Temp;
    }

    /*
    // This is the more conventional implementation of the loop above.
    // It is a bit more accurate, but slower.
    for(k=0; k<N; k++)
     {
      SumR += Samples[k] *  cos((double)k * Omega * M_PI);
      SumI += Samples[k] * -sin((double)k * Omega * M_PI);
     }
    */

    return( sqrt(SumR*SumR + SumI*SumI)) ;
    // return( ComplexD(SumR, SumI) );// if phase is needed.
}

template<typename T>
static T SingleFreqDFT2(T *Samples, int32_t N, T Omega)
{
    auto cosine = getcosinewave();
    auto sine = getsinewave();
    T phs = 0;
    T SumR = 0.0;
    T SumI = 0.0;
    T inc = Omega  * (T)WINDOW_SIZE;

    for(int32_t k=0; k<N; k++)
    {
        SumR += Samples[k] * cosine[PHS2INT(phs)];
        SumI += Samples[k] * -sine[PHS2INT(phs)];
        phs += inc;
    }

    return( sqrt(SumR*SumR + SumI*SumI)) ;
    // return( ComplexD(SumR, SumI) );// if phase is needed.
}


void SpectrumAnalyzer::compute3(MYFLOAT *inputL, MYFLOAT *inputR, MYFLOAT *outputL, MYFLOAT *outputR) {
	const auto win = _DATA->hanningwin;
    for (int32_t j = 0; j < _STATE->currentBufSize; j++) {
        buf[count] = (inputL[j] + inputR[j]) * win[(count * windowscale) & 16383u];
        ++count;
        if (count == fftsize) {
            count = 0;
            MYFLOAT* arr = nullptr;
            if(input_.load(std::memory_order_acquire) == nullptr && (arr = _STATE->pool.acquire<double>(tsl::displayChannelsSpectrum)) != nullptr)
            
            for(int32_t i=0;i<tsl::displayChannelsSpectrum;i++){
                auto x = LOG10D20(SingleFreqDFT2(buf, fftsize,freq_at_x((MYFLOAT) i, (MYFLOAT) tsl::displayChannelsSpectrum) * _STATE->onedsr));
                if(x < -60) x = -60;
                else if(x>60)
                    x = 60;
                arr[i] = x;
            }
            input_.store(arr, std::memory_order_release);
        }
    }
};
template <typename T>
T my_log10_20(T val) {
    if (val <= 0.0) return -std::numeric_limits<T>::infinity(); // Or some large negative dB value like -120.0
    return 20.0 * std::log10(val);
}

// Function to process FFT data into logarithmic bands
void processFFTToLogBands(
        const MYFLOAT* buf,        // Raw interleaved FFT data (real, imag, real, imag...)
        int32_t fftsize,           // Full FFT size (e.g., 2048)
        int32_t displayChannels,   // Number of desired logarithmic display bands (e.g., 64)
        MYFLOAT sampleRate,        // Audio sample rate (e.g., 44100.0f)
        std::array<MYFLOAT, tsl::displayChannelsSpectrum>& arr  // Output vector for logarithmic band magnitudes (dB)
) {

    // FFT bin frequency resolution
    const MYFLOAT freqPerBin = sampleRate / (MYFLOAT)fftsize;

    // Define the desired frequency range for the display
    // Using 20Hz and 20kHz (or 20480Hz as in your original code)
    const MYFLOAT minDisplayFreq = 20.0f;
    const MYFLOAT maxDisplayFreq = 20480.0f; // Using 20kHz, consistent with typical audio range

    // Map display frequencies to fractional FFT bin indices
    const MYFLOAT lowBinFloat = minDisplayFreq / freqPerBin;
    const MYFLOAT highBinFloat = maxDisplayFreq / freqPerBin;

    // Calculate the geometric ratio for logarithmic spacing of display channels
    MYFLOAT coeff = std::pow(highBinFloat / lowBinFloat, 1.0f / (MYFLOAT)displayChannels);

    // Current fractional FFT bin index corresponding to the start of the current display channel
    MYFLOAT currentLogBinStart = lowBinFloat;

    // Loop through each display channel
    for (int chan = 0; chan < displayChannels; ++chan) {
        // Calculate the end (fractional) FFT bin index for the current display channel
        MYFLOAT currentLogBinEnd = currentLogBinStart * coeff;

        // Ensure we don't go beyond Nyquist (fftsize / 2 bins)
        // Note: FFT output usually has N/2 + 1 unique bins from 0 to N/2.
        // So indices go from 0 to fftsize/2.
        currentLogBinEnd = std::min(currentLogBinEnd, (MYFLOAT)(fftsize / 2));

        MYFLOAT channelMagnitudeSum = 0.0f;
        int32_t numBinsInChannel = 0; // To keep track of how many linear bins contributed

        // Iterate through the integer FFT bins that fall within this logarithmic channel
        // Start from the first integer bin that is fully or partially in the channel
        int32_t startFFTBin = static_cast<int32_t>(std::floor(currentLogBinStart));
        int32_t endFFTBin   = static_cast<int32_t>(std::ceil(currentLogBinEnd));

        // Clamp to valid FFT bin range (0 to fftsize/2)
        startFFTBin = std::max(0, startFFTBin);
        endFFTBin   = std::min(fftsize / 2, endFFTBin);

        for (int32_t binIdx = startFFTBin; binIdx < endFFTBin; ++binIdx) {
            // Get magnitude for the current FFT bin
            MYFLOAT real_part = buf[binIdx * 2];
            MYFLOAT imag_part = buf[binIdx * 2 + 1];
            MYFLOAT magnitude = std::sqrt(real_part * real_part + imag_part * imag_part);

            // Contribution for this bin to the current logarithmic channel
            // This is a simple summation. For more precise results,
            // you might want to consider partial contributions for bins that straddle channel boundaries,
            // or sum power instead of magnitude.
            // For now, let's stick to simple summation as in your original code.

            // The original code summed (val1 + (bin - v1) * (val2 - val1)) if interpolation was used.
            // Since it was commented out, and you are just adding val1, let's just add the magnitude.
            channelMagnitudeSum += magnitude;
            numBinsInChannel++;
        }

        // Normalize by the number of bins contributing to this channel
        // This gives an average magnitude for the channel.
        if (numBinsInChannel > 0) {
            arr[chan] = channelMagnitudeSum / MYFLOAT(numBinsInChannel*fftsize);
        } else {
            arr[chan] = 0.0; // No bins contributed, set to 0 magnitude
        }

        // Convert to dB and clamp for display
        arr[chan] = my_log10_20(arr[chan]); // Use the helper to handle <= 0

        arr[chan] = std::clamp(arr[chan], -60.0, 60.0);

        // Prepare for the next channel
        currentLogBinStart = currentLogBinEnd;
    }
}

void SpectrumAnalyzer::tick(MYFLOAT input) {
	auto win = _DATA->hanningwin;
        buf[count] = input * win[(count * windowscale) & 16383u];
        ++count;
        if (count == fftsize) {
            count = 0;
            MYFLOAT* arr = nullptr;
            if (input_.load(std::memory_order_acquire) == nullptr && (arr = _STATE->pool.acquire<double>(tsl::displayChannelsSpectrum)) != nullptr)

            {
            fft.forward(buf, buf);
            const MYFLOAT freqPerBin = _STATE->sr / (MYFLOAT)fftsize;

            // Define the desired frequency range for the display
            // Using 20Hz and 20kHz (or 20480Hz as in your original code)
            const MYFLOAT minDisplayFreq = 20.0;
            const MYFLOAT maxDisplayFreq = 20000.0; // Using 20kHz, consistent with typical audio range

            // Map display frequencies to fractional FFT bin indices
            const MYFLOAT lowBinFloat = minDisplayFreq / freqPerBin;
            const MYFLOAT highBinFloat = maxDisplayFreq / freqPerBin;

            // Calculate the geometric ratio for logarithmic spacing of display channels
            MYFLOAT coeff = std::pow(highBinFloat / lowBinFloat, 1.0 / (MYFLOAT)tsl::displayChannelsSpectrum);

            // Current fractional FFT bin index corresponding to the start of the current display channel
            MYFLOAT currentLogBinStart = lowBinFloat;

            // Loop through each display channel
            for (int chan = 0; chan < tsl::displayChannelsSpectrum; ++chan) {
                // Calculate the end (fractional) FFT bin index for the current display channel
                MYFLOAT currentLogBinEnd = currentLogBinStart * coeff;

                // Ensure we don't go beyond Nyquist (fftsize / 2 bins)
                // Note: FFT output usually has N/2 + 1 unique bins from 0 to N/2.
                // So indices go from 0 to fftsize/2.
                currentLogBinEnd = std::min(currentLogBinEnd, (MYFLOAT)(fftsized2));

                MYFLOAT channelMagnitudeSum = 0.0f;
                int numBinsInChannel = 0; // To keep track of how many linear bins contributed

                // Iterate through the integer FFT bins that fall within this logarithmic channel
                // Start from the first integer bin that is fully or partially in the channel
                auto startFFTBin = static_cast<int>(std::floor(currentLogBinStart));
                auto endFFTBin   = static_cast<int>(std::ceil(currentLogBinEnd));

                // Clamp to valid FFT bin range (0 to fftsize/2)
                startFFTBin = std::max(0, startFFTBin);
                endFFTBin   = std::min(fftsize / 2, endFFTBin);

                for (int32_t binIdx = startFFTBin; binIdx < endFFTBin; ++binIdx) {
                    // Get magnitude for the current FFT bin
                    MYFLOAT real_part = buf[binIdx * 2];
                    MYFLOAT imag_part = buf[binIdx * 2 + 1];
                    MYFLOAT magnitude = std::sqrt(real_part * real_part + imag_part * imag_part);

                    // Contribution for this bin to the current logarithmic channel
                    // This is a simple summation. For more precise results,
                    // you might want to consider partial contributions for bins that straddle channel boundaries,
                    // or sum power instead of magnitude.
                    // For now, let's stick to simple summation as in your original code.

                    // The original code summed (val1 + (bin - v1) * (val2 - val1)) if interpolation was used.
                    // Since it was commented out, and you are just adding val1, let's just add the magnitude.
                    channelMagnitudeSum += magnitude;
                    numBinsInChannel++;
                }

                // Normalize by the number of bins contributing to this channel
                // This gives an average magnitude for the channel.
                if (numBinsInChannel > 0) {
                    arr[chan] = channelMagnitudeSum / MYFLOAT(numBinsInChannel*fftsize);
                } else {
                    arr[chan] = 0.0; // No bins contributed, set to 0 magnitude
                }

                // Convert to dB and clamp for display
                arr[chan] = my_log10_20(arr[chan]); // Use the helper to handle <= 0

                // Prepare for the next channel
                currentLogBinStart = currentLogBinEnd;
            }
            input_.store(arr, std::memory_order_release);
            }

        }
};


void SpectrumAnalyzer::compute2(MYFLOAT *inputL, MYFLOAT *inputR, MYFLOAT *, MYFLOAT *) {
    for (int32_t j = 0; j < _STATE->currentBufSize; j++) {
        buf[count] = (inputL[j] + inputR[j]) * _DATA->hanningwin[(count * windowscale) & 16383u];
        ++count;
        if (count == fftsize) {
            count = 0;
            MYFLOAT* arr = nullptr;
            if (input_.load(std::memory_order_acquire) == nullptr && (arr = _STATE->pool.acquire<double>(tsl::displayChannelsSpectrum)) != nullptr)

            {

                fft.forward(buf, buf);
                const MYFLOAT freqperbin = (MYFLOAT)(_STATE->sr) / (MYFLOAT)(fftsize);
                MYFLOAT lowbin = 20.f / freqperbin;
                MYFLOAT highbin = 20480.f / freqperbin;
                const MYFLOAT coeff = powf(highbin / lowbin, 1.f / ((MYFLOAT)tsl::displayChannelsSpectrum - 1));
                MYFLOAT next_bin = lowbin;
                for (int32_t chan = 0; chan < tsl::displayChannelsSpectrum; chan++) {
                    //LOGE("inside %f", stop);
                    auto start = next_bin;
                    next_bin *= coeff;
                    while (start < next_bin) {
                        auto v1 = (int)floorf(start);
                        auto v2 = v1 + 1;
                        auto val1 = v1 >= (fftsize >> 1) ? 0 :
                            buf[v1 * 2] * buf[v1 * 2] + buf[v1 * 2 + 1] * buf[v1 * 2 + 1];
                        auto val2 = v2 >= (fftsize >> 1) ? 0 : (buf[v2 * 2] * buf[v2 * 2] +
                            buf[v2 * 2 + 1] * buf[v2 * 2 + 1]);
                        arr[chan] += sqrt(val1 + (start - v1) * (val2 - val1));
                        start += 1;
                    }
                    arr[chan] = arr[chan] / (MYFLOAT)(fftsize >> 1);
                    arr[chan] = arr[chan] > 0 ? LOG10D20F(arr[chan]) : -60;
                    if (arr[chan] < -60) arr[chan] = -60;
                    else if (arr[chan] > 60)
                        arr[chan] = 60;


                }
                input_.store(arr, std::memory_order_release);
            }
        }

    }
}