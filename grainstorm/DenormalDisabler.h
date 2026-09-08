#pragma once
//
// Created by pr on 20.01.22.
//

#ifndef GRAINSTORM_DENORMALDISABLER_H
#define GRAINSTORM_DENORMALDISABLER_H


#include <cstdint>

class DenormalDisabler2 {
public:
    static bool my_isnan(float value)
    {
        union IEEE754_Single
        {
            float f;
            struct
            {
#if BIG_ENDIAN
                uint32_t sign     : 1;
        uint32_t exponent : 8;
        uint32_t mantissa : 23;
#else
                uint32_t mantissa : 23;
                uint32_t exponent : 8;
                uint32_t sign     : 1;
#endif
            } bits;
        } u = { value };

        // In the IEEE 754 representation, a float is NaN when
        // the mantissa is non-zero, and the exponent is all ones
        // (2^8 - 1 == 255).
        return (u.bits.mantissa != 0) && (u.bits.exponent == 255);
    }
    static bool my_isnan2(float value)
    {
        union IEEE754_Single
        {
            float f;
            struct
            {
#if BIG_ENDIAN
                uint32_t sign     : 1;
        uint32_t exponent : 8;
        uint32_t mantissa : 23;
#else
                uint32_t mantissa : 23;
                uint32_t exponent : 8;
                uint32_t sign     : 1;
#endif
            } bits;
        } u = { value };

        // In the IEEE 754 representation, a float is NaN when
        // the mantissa is non-zero, and the exponent is all ones
        // (2^8 - 1 == 255).
        return (u.bits.mantissa != 0) && (u.bits.exponent == 255);
    }

    DenormalDisabler2()
            : m_savedCSR(0)
    {
        disableDenormals();
    }
    ~DenormalDisabler2()
    {
        restoreState();
    }
    // This is a nop if we can flush denormals to zero in hardware.
    static inline float flushDenormalFloatToZero(float f)
    {
        return f;
    }
private:
    unsigned m_savedCSR;
    static inline void DisableFZ( )
    {
        __asm__ volatile("vmrs r0, fpscr\n"
                         "bic r0, $(1 << 24)\n"
                         "vmsr fpscr, r0" : : : "r0");
    }
    static inline void RestoreFZ( ) {
        __asm__ volatile("vmrs r0, fpscr\n"
                         "orr r0, $(1 << 24)\n"
                         "vmsr fpscr, r0" : : : "r0");
    }
    inline void disableDenormals()
    {   DisableFZ();
        //m_savedCSR = getStatusWord();
        // Bit 24 is the flush-to-zero mode control bit. Setting it to 1 flushes denormals to 0.
       // setStatusWord(m_savedCSR | (1 << 24));
    }
    inline void restoreState()
    {
        RestoreFZ();
        //setStatusWord(m_savedCSR);
    }
    inline int32_t getStatusWord()
    {
        int32_t result;
#if defined(__aarch64__)
        asm volatile("mrs %[result], FPCR" : [result] "=r" (result));
#else
        asm volatile("vmrs %[result], FPSCR" : [result] "=r" (result));
#endif
        return result;
    }
    inline void setStatusWord(int32_t a)
    {
#if defined(__aarch64__)
        asm volatile("msr FPCR, %[src]" : : [src] "r" (a));
#else
        asm volatile("vmsr FPSCR, %[src]" : : [src] "r" (a));
#endif
    }
};

#endif //GRAINSTORM_DENORMALDISABLER_H
