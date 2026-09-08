#pragma once

#if defined(__ANDROID__) || (defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define PLATFORM_MOBILE 1
#else
#define PLATFORM_DESKTOP 1
#endif


#include <cstddef>
#include <new>     // Declares std::hardware_{con,de}structive_interference_size
#include <version> // Ensure feature test macros are included

#if !defined(_WIN32) && !defined(__APPLE__) && defined(__cpp_lib_hardware_interference_size) && (__cpp_lib_hardware_interference_size >= 201703L)
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
// Fallback values if not available
constexpr std::size_t hardware_constructive_interference_size = 64;
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

#define _DATA _appState->data
#define _STATE _appState

#define MYFLOAT double

inline constexpr MYFLOAT loopPerBarMin = 0.0625;
inline constexpr MYFLOAT loopPerBarMax = 16.;

#define DIGITAL_TC  -2.0 // log(1%)
#define  ANALOG_TC  -0.43533393574791066201247090699309 // (log(36.7%)
#define FXRELEASE  200.

#define J dcomp(0.0,1.0)


#define MYFLT2LRND(x) ((int32_t) lrintf((MYFLOAT) (x)))
#define RANDMAX (2147483648u)
#define TBLMASK 0x3FFFu
#define TBLMAX 0x4000u

#define PHS2INT(x)((uint32_t) /*MYFLT2LRND*/((x) * (MYFLOAT) TBLMAX) & TBLMASK)
#define KSAMPLES 64

#define TBLSIZE2 0x1000u
#define TBLMASK2 0xFFFu
#define PHS2INT2(x)((uint32_t) MYFLT2LRND((x) * (float) TBLSIZE2) & TBLMASK2)


#define TBLSIZE3 0x400u
#define TBLMASK3 0x3FFu
#define PHS2INT3(x)((uint32_t)((x) * (float) TBLSIZE3) & TBLMASK3)

#define CONV16BIT (32767.0)
#define CONVMYFLT (0.000030517578125)
#define  CONV32BIT (32767.0f*65536.0)
#define CONV24BIT (8388608.0)
#define CONV16TO24BIT (CONV24BIT / CONV16BIT)
#define PI_P      (3.141592653589793238462643383279502884197)
#define TWOPI_P   (6.283185307179586476925286766559005768394)
#define TWOPI_F_P (6.283185307179586476925286766559005768394)
#define PI_F_P (3.141592653589793238462643383279502884197)
#define ROOT2 (1.4142135623730950488)
#define ROOT2_F (1.4142135623730950488f)
#define log001 -6.9078    /* log(.001) */
#define LOG2(x) std::log2(x)
#define SHIFT2SEMITONES(x) (12 * log((x)) / log(2))
#define LOG10D20F(x) (20. * log10(x))
#define LOG10D20(x) (20. * log10(x))
#define LIN2SCALED2(x) pow(x, .5f)
#define SCALED2LIN2(x) pow(x, 2.f)
#define LIN2SCALED4(x) pow(x, .25f)
#define SCALED2LIN4(x) pow(x, 4.f)
#define BILLION 1000000000L
#define TENMB 10485760
#define MS2SMPL(x, y) (long) ((y) * 0.001 * (x))
#define LN_2_2 0.34657359027997265470861606072908828403775006718

#define TRACKLOOPSMPLS(x) x->off_stop.load() - x->off_start.load()
#define MSTOSMPLS(x) (x * DATA::sr * .001)
#define SMPLSTOMS(x) (x * 1000. / DATA::sr)

#define ARRAY_LEN(x) (std::size(x))

#define MAXI(x, y) ((x) > (y) ? (x) : (y))
#define MAX(x, y)               (std::max(x, y))
#define MIN(x, y)               (std::min(x, y))
#define ABS(x)                  (std::abs(x))
#define BOTTOM(x)                  (std::max(0., x))
#define TOP(x, y)            (std::min(x, y))
#define ISNEG(x) (((x) < 0) ? 1 : 0)
//#define ISNEG(X) (!((X) > 0) && ((X) != 0) ? -1 : 1)
#define PASS_BOTTOM(x, y)  ((-y) + (x))
#define PASS_TOP(x, y) ((y) - (x))
#define BOTTOMREST(x, y)                  (((x) < 0) ? ((x) + (y)) : 0)
#define TOPREST(x, y)            ((x) > (y) ? ((x) - (y)) : 0)
#define DISTANCE(x, y) (ABS(((MYFLOAT) x) - ((MYFLOAT) y)))
#define DISTANCEF(x, y) (ABS(((MYFLOAT) x) - ((MYFLOAT) y)))
#define CLAMP(x, y, z) ((x) = ((x) < (y) ? (y) : ((x) > (z) ? (z) : (x))))

#define BOTTOM_P(x) (((x) < 0) ? ABS(x) : 0 )
#define PASS_TOP_P (x, y) ((x) >= (y) ? 0 : (x))
#define BOTTOMCOMP(x, y)                  ((x) < (y) ? 0 : ((y) - (x)))
//#define TOP(x, y)            ((x) > (y) ? ((x) - (y)) : (x))
#define _SIGN(N) ((N)>>31L)
#define _ABS(N) ((N)^_SIGN(N))
#define _CLIPR(N, R) ((_ABS((N)+(R))-_ABS((N)-((R)+1L)))/2L)

#define LOG2NORMAL(x) (pow(10., (x) * .05))
#define LOG2NORMALF(x) (pow(10., (x) * .05))
#define LOG2NORMAL2(x) (pow(10., (x) * 0.025))
#define LOG2NORMAL2F(x) (pow(10., (x) * 0.025))

#define LOG2LIN60PARAM(x) (pow(10., (x) * .05))

#define NUMBERA 99.948711
#define NUMBERB 0.7284301
#define NUMBERC -0.010247393
#define NUMBERD 0.00129207

#define DBTOPERCENTAGE(x) ((NUMBERA + NUMBERB*(x)) / (1. + NUMBERC * (x) + NUMBERD * (x) * (x)) * .01)

#define NONE 0
#define DRAG 1
#define ZOOM 2
#define DRAW 3

#define NORMAL 0
#define PRESSED 1
#define DISABLED 2
#define HOT 3
#define NOUPDATE 4

#define SOURCE_AUDIO 0
#define SOURCE_RECORD 1

#define CLEAR 0
#define BLACK 1
#define CUSTOM 2

#define PERMANENT true
#define ONLY_ONCE false

#ifdef NDEBUG
#   define ASSERT(condition) if((condition) == false) LOGE("An assertion failed.");

#else
#   define ASSERT(x) assert(x);
#endif


#define DEBUG_ENABLED 0

#define MAX_RANDOM_READ_OFFSET 2000
#define MAX_RANDOM_WRITE_OFFSET 20

#define SHORTCLICKTIME 1000 * 300

#define WINDOW_SIZE 16384u

#define MAX_FFT_SIZE WINDOW_SIZE


#define MAX_CHANNELS 2


#define RECORD_LIVE 0
#define RECORD_LOOP 1
#define SAVE_LOOP 2

#define ARR2VEC(a, b, c) memcpy(&(a)[0], (b), (c) * sizeof(float))
#define VEC2ARR(a, b, c) memcpy((a), &(b)[0], (c) * sizeof(float))

#define NOT_HIDDEN __attribute__((visibility("default"))) Exported()


#if defined(__clang__) || defined(HAVE_GCC3)
#  define LIKELY(x)     __builtin_expect(!!(x),1)
#  define UNLIKELY(x)   __builtin_expect(!!(x),0)
#else
#  define LIKELY(x)     x
#  define UNLIKELY(x)   x
#endif

#define FL(x) ((MYFLOAT) (x))

// Given: MAX = 10.0f
constexpr MYFLOAT SPEED_MAX = 10.0;
constexpr MYFLOAT SPEED_OFFSET = 1.0 / (SPEED_MAX - 2.0); // = 0.125f

#define CUTOFFMIN .01
#define CUTOFFRANGE .84

#define GETView(x) (_STATE->parameters[(x)].view)


#define GASMAIN (int) _STATE->params[tindex][AS].load()
#define GASGRAIN (int) _STATE->params[tindex][ASGRAN].load()
#define GASENV (int) _STATE->params[tindex][ASENV].load()
#define GASLFO (int) _STATE->params[tindex][ASLFO].load()
#define GASFOL (int) _STATE->params[tindex][ASFOL].load()
#define GASFX (int) _STATE->params[tindex][ASFX].load()
#define GASPV (int) _STATE->params[tindex][ASPV].load()
#define GASCROSS (int) _STATE->params[tindex][ASCROSS].load()
#define GASSTFX (int) _STATE->params[tindex][ASSTFX].load()
#define GASMDEL (int) _STATE->params[tindex][ASMDEL].load()
#define NUMFFTS 15


#define MAX_DELAYS 8
#define DELAY_PARALLEL 0
#define DELAY_SERIES 1

#define MIN_DELAY_MS 5.
#define MAX_DELAY_MS 1000.

#define FLANGER_MIN_DELAY 0.5


#define ALIGN 64
#define AA(x) (((x) + ALIGN - 1) & ~(ALIGN - 1));

//#define UNDENORMAL(v) if(std::fpclassify(v) != FP_NORMAL&&std::fpclassify(v) != FP_ZERO){v=0;}

#define ADSRSIZE 0x400
#define ADSRANDMASK 0x3FF
#define BPMMIN 1
#define BPMMAX 3000
#define DENSITYMIN 1.
#define DENSITYMAX 500.


#define MAXLEN     0x40000000
#define FMAXLEN    ((MYFLOAT)(MAXLEN))
#define PHMASK     0x3fffffff
#define TABLE_LEN (16384 * 8)

#define OSCBNK_RNDPHS   0               /* 31 bit rand -> phase bit shift */

#define OSCBNK_PHSMAX_64   0x8000000000000000ULL  /* 2^63 */
#define OSCBNK_PHSMSK_64   0x7FFFFFFFFFFFFFFFULL  /* 63-bit mask */
/* Convert floating point phase (0.0 to 1.0) to 64-bit integer phase */
#define OSCBNK_PHS2INT_64(x) (static_cast<uint64_t>((x) * (MYFLOAT) OSCBNK_PHSMAX_64) & OSCBNK_PHSMSK_64)

#define OSCBNK_PHSMAX_32   0x80000000U    /* max. phase   */
#define OSCBNK_PHSMSK_32   0x7FFFFFFFU    /* phase mask   */
#define OSCBNK_RNDPHS_32   0               /* 31 bit rand -> phase bit shift */
#define OSCBNK_PHS2INT_32(x)(static_cast<uint32_t>((x) * (MYFLOAT) OSCBNK_PHSMAX_32) & OSCBNK_PHSMSK_32)

#define VCO2FT_USE_TABLE    1
#define VCO2_MAX_NPART  4096    /* maximum number of harmonic partials */

#define UDD(x)         if (!std::isnormal(x)) {x=0;}//if ( x != 0 && std::fabs( x ) < std::numeric_limits<double>::min() ) {x=0;}
#define UDF(x)         if (!std::isnormal(x)) {x=0;}//if ( (x) != 0 && std::fabsf( (x) ) < std::numeric_limits<float>::min() ) {(x)=0;}
#define UDFD(x,y)      if(y) {UDD(x)} else {UDF(x)}
#define ISDENORMAL(x) (x)!=0 && std::abs(x) < std::numeric_limits<MYFLOAT>::min()





#define DELAYPOS_SHIFT1  28
#define DELAYPOS_SCALE1  0x10000000
#define DELAYPOS_MASK1   0x0FFFFFFF


#define oneUp31Bit      (double) (4.656612875245796924105750827168e-10)


#define randGab   (MYFLOAT) ((double)     \
    (((holdrand = holdrand * 214013 + 2531011) >> 1)  \
     & 0x7fffffff) * oneUp31Bit)
#define BiRandGab (MYFLOAT) ((double)     \
    (holdrand = holdrand * -214013 + 2531011) * oneUp31Bit)

#define RANDOMFLOAT(x, y) (((x) > (y) ? (y) : (x)) + DISTANCEF((x), (y)) * randGab)


#define EXPPARAM(x) (expf(-4.f + 4.f * (x)))

#define SEQUENCER_STEPS 16


#define OFFPOINTY 9
#define OFFNSEGS (9 * 2)
#define OFFCURVE (OFFNSEGS + 1)
#define OFFREDRAW (OFFCURVE + 1)
#define OFFRECOMP (OFFREDRAW + 1)
#define OFFJOINENDS (OFFRECOMP + 2)
#define OFFQUANT (OFFJOINENDS + 1)
#define MAX_SEGS 8
#define MAX_POINTERS MAX_SEGS + 1


// #define kNanosPerSecond 1000000000

#if defined STANDALONE_MODE || defined PLUGIN_MODE
constexpr int OUTPUT_ACTIVE_BIT_L = 8;
constexpr int OUTPUT_ACTIVE_BIT_R = 9;
constexpr int rsOffset = 1;
#endif

#include <cmath>

inline double dbToLinear60(double db) {
    constexpr double kMin = 0.001; // 10^(-60/20)
    return (std::pow(10.0, db * 0.05) - kMin) / (1.0 - kMin);
}