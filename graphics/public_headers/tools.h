#pragma once
#ifndef TOOLS_H
#define TOOLS_H
#include "defines.h"

#include <vector>
#include <cstdint>
#include <cstddef>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <limits>
#include <cmath>
#include <new>
#include <cstring>
#include <algorithm>
#include <semaphore>
#include <string_view>
#if defined(__SSE__) || defined(__AVX__)
#include <xmmintrin.h>
#endif
#include <string>


namespace tsl {
    std::string generateTimestampedName(const std::string& prefix = "Recording");
    std::string trimToValidFilename(std::string name);
    struct DenormalDisabler {
        unsigned int saved_mxcsr = 0;
        uint32_t saved_fpscr = 0;
        uint64_t saved_fpcr = 0;

        DenormalDisabler() {
#if defined(__SSE__) || defined(__AVX__)
            saved_mxcsr = _mm_getcsr();
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);        // bit 15
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON); // bit 6
#elif defined(__arm__)
            asm volatile("VMRS %0, fpscr" : "=r"(saved_fpscr));
        uint32_t fpscr = saved_fpscr | (1 << 24) | (1 << 25);
        asm volatile("VMSR fpscr, %0" :: "r"(fpscr));
#elif defined(__aarch64__)
            asm volatile("mrs %0, fpcr" : "=r"(saved_fpcr));
            uint64_t fpcr = saved_fpcr | (1ULL << 24); // FZ only
            asm volatile("msr fpcr, %0" :: "r"(fpcr));
#endif
        }

        ~DenormalDisabler() {
#if defined(__SSE__) || defined(__AVX__)
            _mm_setcsr(saved_mxcsr);
#elif defined(__arm__)
            asm volatile("VMSR fpscr, %0" :: "r"(saved_fpscr));
#elif defined(__aarch64__)
            asm volatile("msr fpcr, %0" :: "r"(saved_fpcr));
#endif
        }
    };

    int16_t to16Bit(double fval);
    int32_t to24Bit(double fval);


    template<typename T>
    class Balance{
    public:
        Balance(T sr){
            T b = 2.0 - cos(/*p->ihp*/ 10. * TWOPI_P/sr);
            c2 = b - sqrt(b*b - 1.0);
            c1 = 1.0 - c2;

        }
        inline T tickBalance(T in, T comp){
            q = c1 * in * in + c2 * q;
            r = c1 * comp * comp + c2 * r;
            if (q != 0.0)
                return in *sqrt(r/q);
            else
                return in * sqrt(r);
        }
    private:
        T q{}, r{}, c1{}, c2{};
    };

    class BinarySemaphore {
    public:
        void acquire() {
            lock.wait(false, std::memory_order_acquire);
            lock.clear();
        };


        void acquireNoClear() {
            lock.wait(false, std::memory_order_acquire);
        };

        void release() {
            lock.test_and_set(std::memory_order_release);
            lock.notify_one();
        };

    private:
        std::atomic_flag lock{};
    };
    
    // TRUE COUNTING SEMAPHORE:
    class CountingSemaphore {
        std::atomic<int> count;
        std::atomic<int> waiters{ 0 };
    public:
        explicit CountingSemaphore(int initial_count) : count(initial_count) {}

        void acquire() {
            waiters.fetch_add(1, std::memory_order_relaxed);

            while (true) {
                int expected = count.load(std::memory_order_acquire);
                if (expected > 0 && count.compare_exchange_weak(expected, expected - 1,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                    break; // Successfully decremented count
                }
                // Wait for notification
                count.wait(0, std::memory_order_relaxed);
            }

            waiters.fetch_sub(1, std::memory_order_relaxed);
        }

        void release() {
            count.fetch_add(1, std::memory_order_release);
            if (waiters.load(std::memory_order_relaxed) > 0) {
                count.notify_one();
            }
        }

        int available() const {
            return std::max(0, count.load(std::memory_order_acquire));
        }
    };


    template<typename T>
    class SmartMem {
    public:
        SmartMem() = default;

        explicit SmartMem(size_t size = 0) {
            resize(size);
        }

        void operator=(std::vector<T> &src) {
            _buf = src;
        }

        template<typename T2>
        T &operator[](T2 i) {
            return _buf[i];
        }

        void resize(size_t size) {
            _buf.resize(size, 0);
        }

        void push(T val) {
            _buf.push_back(val);
        }

        long size() {
            return _buf.size();
        }

        void clear() {
            _buf.clear();
        }

        void swap(std::vector<T> &src) {
            _buf = std::move(src);
        }

    private:
        std::vector<T> _buf;
    };


    template<typename T, size_t SIZE>
    class array : public std::array<T, SIZE> {
    public:
        using array_t = array<T, SIZE>;

        array_t operator+(const array_t &o) {
            array_t tmp;
            std::transform(this->begin(), this->end(), o.begin(), tmp.begin(), std::plus<T>());
            return tmp;
        }

        array_t operator-(const array_t &o) {
            array_t tmp;
            std::transform(this->begin(), this->end(), o.begin(), tmp.begin(), std::minus<T>());
            return tmp;
        }

        array_t operator*(const T &fact) {
            array_t tmp;
            std::transform(this->begin(), this->end(), tmp.begin(),
                           std::bind(std::multiplies<T>(), std::placeholders::_1, fact));
            return tmp;
        }

        array_t operator*(const array_t &o) {
            array_t tmp;
            std::transform(this->begin(), this->end(), o.begin(), tmp.begin(),
                           std::multiplies<T>());
            return tmp;
        }

        array_t &operator+=(const array_t &o) {
            std::transform(this->begin(), this->end(), o.begin(), this->begin(),
                           std::plus<T>());
            return *this;
        }

        array_t &operator-=(const array_t &o) {
            std::transform(this->begin(), this->end(), o.begin(), this->begin(),
                           std::minus<T>());
            return *this;
        }

        array_t &operator*=(const array_t &o) {
            std::transform(this->begin(), this->end(), o.begin(), this->begin(),
                           std::multiplies<T>());
            return *this;
        }

        array_t &operator*=(const T &fact) {
            std::transform(this->begin(), this->end(), this->begin(),
                           std::bind(std::multiplies<T>(), std::placeholders::_1, fact));
            return *this;
        }

        array_t sqrt() {
            array_t tmp;
            std::transform(this->begin(), this->end(), tmp.begin(), (T(*)(T)) std::sqrt);
            return tmp;
        }

        void min(const array_t &o) {
            std::transform(this->begin(), this->end(), o.begin(), this->begin(),
                           [](auto &a, auto &b) { return std::min(a, b); });
        }

        void max(const array_t &o) {
            std::transform(this->begin(), this->end(), o.begin(), this->begin(),
                           [](auto &a, auto &b) { return std::max(a, b); });
        }
    };

    template<typename T, uint32_t RMSSIZE, typename VALUE_TYPE = float, typename INDEX_TYPE = uint32_t>
    class rms {
    public:
        static constexpr bool isPowerOfTwo(uint32_t n) { return (n & (n - 1)) == 0; }

        static_assert(isPowerOfTwo(RMSSIZE), "Capacity must be a power of 2");
        static_assert(std::is_unsigned<INDEX_TYPE>::value, "Index type must be unsigned");

        inline void push(T &obj) {
            _sum -= _mem[_index];
            auto temp = obj * obj;
            _sum += temp;
            _mem[_index] = temp;
            _index++;
            _index &= (RMSSIZE - 1);
        }

        inline T ms(T &obj) {
            push(obj);
            return _sum * _onedN;
        }
        inline T ms() {
            return _sum * _onedN;
        }

        /*
        inline T rms(T &obj){
            return ms(obj).sqrt();
        }*/

        inline T rms2(T &obj) {
            auto tmp = ms(obj).sqrt();
            VALUE_TYPE val = -1.;
            for (auto &temp : tmp) {
                temp *= val;
                val *= -1.f;
            }
            return tmp;
        }

        inline T rms2() {
            auto tmp = _sum.sqrt();
            VALUE_TYPE val = -1.;
            for (auto &temp : tmp) {
                temp *= val;
                val *= -1.f;
            }
            return tmp;
        }

        inline T rootmeansquare() {
            return _sum.sqrt();
        }

        int size() const { return RMSSIZE; }

    private:
        INDEX_TYPE _index{};
        T _sum{};
        T _mem[RMSSIZE]{};
        const VALUE_TYPE _onedN{1. / (VALUE_TYPE) RMSSIZE};

        INDEX_TYPE mask(INDEX_TYPE n) const {
            return static_cast<INDEX_TYPE>(n & (RMSSIZE - 1));
        }
    };

    class time {
    public:
        double elapsedReplace() {
            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double> res = end - beg;
            beg = end;
            return res.count();
        }

        double elapsed() {
            std::chrono::duration<double> res = std::chrono::system_clock::now() - beg;
            return res.count();
        }

        void reset() { beg = std::chrono::system_clock::now(); }

        static int64_t millisecondsSinceEpoch() {
            return
                    std::chrono::system_clock::now().time_since_epoch() /
                    std::chrono::milliseconds(1);
        }

        static int64_t nanosecondsSinceEpoch() {
            return
                    std::chrono::system_clock::now().time_since_epoch() /
                    std::chrono::nanoseconds(1);
        }

        static constexpr int64_t nanosPerSecond = 1000000000LL;
    private:
        std::chrono::time_point<std::chrono::system_clock> beg{
                std::chrono::system_clock::now()};
    };

    class AtomicTimer {
    public:
        double elapsedReplace() {
            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double> res = end - beg.load();
            beg.store(end);
            return res.count();
        }

        double elapsed() {
            std::chrono::duration<double> res = std::chrono::system_clock::now() - beg.load();
            return res.count();
        }

        void reset() { beg.store(std::chrono::system_clock::now()); }
        void set0() { beg.store(std::chrono::time_point<std::chrono::system_clock>{}); };

    private:
        std::atomic<std::chrono::time_point<std::chrono::system_clock>> beg{
                std::chrono::system_clock::now()};
    };


}

#define MEASUSEINIT tsl::time tsltimer;

#define MEASURESTOP LOGE(" 1some_long_computation took about %g seconds\n", tsltimer.elapsed());

#define MEASURESTART tsltimer.reset();

struct TIME_P {
    unsigned int h;
    unsigned int m;
    unsigned int s;
    unsigned int ms;
};


//char *printfp(const char *format, ...);

void time_convert_p(TIME_P &t, int sr, double offset, float speed);

void time_convert(TIME_P &time_p, int sr, double offset);

extern int tprio(int prio);

void logString(const char *string);

void killpd();

void showpd(char *string);

//jclass findClass(JNIEnv *env, const char* name);
int machineEndianness();

struct timespec diff(struct timespec start, struct timespec end);

//bool terminate();


constexpr unsigned int next_pow_2(unsigned int n) {
    --n;
    n |= n >> 1;   // Divide by 2^k for consecutive doublings of k up to 32,
    n |= n >> 2;   // and then or the results.
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return ++n;
}

uint32_t largestdivisor(uint32_t number, uint32_t start);

void pinknoise_compute(float *out, int size);
int findIndex(const int *array, size_t size, int target);

int findIndexChar(const std::string_view[], size_t size, const char *target);

int findIndexFloat(const float array[], float target);

int findIndexFloatWithLen(const float array[], float target, int len);

void convertFloatToPcm16(const float* source, int16_t* destination, int32_t numSamples);
void convertFloatToPcm32(const float *in, int32_t *out, int s);
void convertFloatToPcm24(const float *in, int32_t *out, int s);



//given a number n, determine if it is prime
inline bool isPrime(int number);

int findNextPrime(int n);

void printheader(const unsigned char *data, int size, const char *symbol = "test");

#endif
