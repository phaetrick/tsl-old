#pragma once
#ifndef ENVELOPE_H
#define ENVELOPE_H

#include <cmath>
#include <defines.h>
#include <atomic>
#include <logger.h>
#include <tools/aligned_memalloc.h>

namespace tsl {
    namespace envelope {
//a n1 b n2 c n3 d
        template<typename T>
        void
        genSpline(T *fp, int tablesize, T positions[], T values[],
                  int nsegs, bool adjust);

        template<typename T>
        void
        genPoly(T *fp, int tablesize, T positions[], T values[],
                int nsegs, bool limit);

        template<typename T>
        void
        genLinear(T *fp, int tablesize, T positions[], T values[],
                  int nsegs, bool limit);

        template<typename T>
        void
        genRect(T *fp, const int tablesize, T positions[], T values[],
                const int nsegs, const bool limit);

        template<typename T>
        using ComputeFn = void(*)(T*, int, T[], T[], int, bool);

        template<typename T>
        extern ComputeFn<T> compute[4];


        template<typename T>
        T genSplinePhase(
            T phase,
            T positions[],
            T values[],
            int nsegs,
            bool adjust);

        template<typename T>
        T genPolyPhase(
            T phase,          // normalized [0..1]
            T positions[],   // normalized positions [0..1]
            T values[],
            int nsegs,
            bool limit);

        template<typename T>
        T genSplinePhase(
            T phase,
            T positions[],
            T values[],
            int nsegs,
            bool adjust);
        
        template<typename T>
        T genRectPhase(
            T phase,          // normalized [0..1]
            T positions[],   // normalized positions [0..1]
            T values[],
            int nsegs,
            bool limit);

        template<typename T>
        T genLinearPhase(
            T phase,          // normalized [0..1]
            T positions[],   // normalized positions [0..1]
            T values[],
            int nsegs,
            bool limit);


                template<typename T>
        using GenEnvPhaseFn = T(*)(T, T[], T[], int, bool);

        template<typename T>
        extern GenEnvPhaseFn<T> genEnvPhase[4];


        enum WINDOWTYPE {
            Rectangular = 0,
            SINE,
            SINE_LFO,
            SAW,
            TRIANGLE,
            RECTPULS,
            SINE_FULL,
            COSINE_FULL,
            TRIANGLE_FULL,
            FULL_SAW,
            FULL_RECTPULS,
            wtHANNING,
            wtHAMMING,
            wtBLACKMAN,
            wtBLACKMAN_HARRIS,
            wtBLACKMAN_NUTTALL,
            wtNUTTALL,
            wtKAISER,
            wtKAISER_BESSEL,
            wtCOSINE,
            wtSINC,
            wtFLATTOP,
            wtFLATTOP2,
            wtTUKEY,
            wtGAUSSIAN,
            wtWELCH,
            wtBOHMANN,
            wtLOWSIDELOBE,
            wtSINE,
            wtTRAPEZOID,
            wtGAUSS
        };

        struct WindowDescriptor {
            const char* name;
            WINDOWTYPE     type;
            double                        alpha = 0.0;
            double                        beta = 0.0;
            MYFLOAT* win{};
            bool                          isEnvelope = false;  // true = symmetric/oneshot
        };


        template<typename T>
        extern T Sinc2(T x);

//---------------------------------------------------------------------------
        template<typename T>
        extern T Bessel(T x);
        
        template<typename T>
        T WindowFunc(T phase,
            WINDOWTYPE type,
            double alpha = 0.0,
            double beta = 5.);

        template<typename T>
        void GenerateWindow(T* win, int N, tsl::envelope::WINDOWTYPE WindowType,
            MYFLOAT alpha = 0.0, MYFLOAT beta = 5.);
    
        template<typename T>
        class Window : private tsl::AlignedVector<T> {
        public:
            Window(int size, WINDOWTYPE windowtype, MYFLOAT alpha = .5) {gen(size, windowtype, alpha);}

            void gen(int size, WINDOWTYPE windowtype, MYFLOAT alpha = .5)  {
                _size = size;
                _mask = size-1;
                this->resize(size + 4);
                GenerateWindow(this->data(), size, windowtype, alpha);
            }
            T operator[](T phs) {
                // Linearly interpolate between neighboring bins - a plain floor/
                // truncate lookup holds a value constant for a stretch of samples
                // whenever the caller's phase is advancing slowly (which is exactly
                // what happens near a turning point of a smooth curve like this),
                // then jumps to the next bin - audible as a "step" right where a
                // continuous curve should be smoothly reversing direction.
                const T scaled = phs * (T)_size;
                const auto i0 = (uint32_t)scaled & _mask;
                const auto i1 = (i0 + 1) & _mask;
                const T frac = scaled - (T)(uint32_t)scaled;
                return this->at(i0) * (T(1) - frac) + this->at(i1) * frac;
            }
            T operator[](int index) {
                    return this->at(index);
            }
        private:
            int _mask;
            int _size;
        };

        template<typename T>
        class Window2 : public tsl::AlignedVector<T> {
        public:
            Window2(int size, WINDOWTYPE windowtype, MYFLOAT alpha = .5) {gen(size, windowtype, alpha);}

            void gen(int size, WINDOWTYPE windowtype, MYFLOAT alpha = .5)  {
                _size = size;
                _mask = size-1;
                this->resize(size + 4);
                GenerateWindow2(this->data(), size, windowtype, alpha);
            }
        private:
            int _mask;
            int _size;
        };

    };
}

#endif