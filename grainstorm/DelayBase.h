#pragma once
//
// Created by pr on 17.11.21.
//

#ifndef GRAINSTORM_DELAYBASE_H
#define GRAINSTORM_DELAYBASE_H

#include <vector>
#include <cmath>
#include "tools.h"

template<typename T>
class delay {
public:

    delay() {
        feedback = 1.;
        bufsize = bufidx = 0;
    }

    int64_t getsize() {
        return bufsize;
    }

    void setsize(int64_t size)
    {
        if (size <= 0) return;
        std::vector<T> new_buffer;
        new_buffer.resize(size, 0);

        if (bufsize > 0 && bufsize <= size) {
            int64_t prefix_i = size - bufsize;
            for (int64_t i = 0; i < bufsize; i++) new_buffer[prefix_i + i] = this->process(0);
        }
        if (bufsize > 0 && bufsize > size) {
            int64_t cut = bufsize - size;
            for (int64_t i = 0; i < cut; i++) this->process(0);
            for (int64_t i = 0; i < size; i++) new_buffer[i] = this->process(0);
        }

        bufidx = 0;
        bufsize = size;
        buffer = new_buffer;
    }

    void mute() {
        std::fill(buffer.begin(), buffer.end(), 0);
        bufidx = 0;
    }

    void setfeedback(T val) {
        feedback = val;
    }

    T getfeedback() {
        return feedback;
    }


    inline T getlast() {
        if (bufsize == 0) return 0;
        return _getlast();
    }

    inline T _getlast() { return buffer[bufidx]; }

    /**
     * Retrive the signal of the delayline.
     * @param[in] index specifies the index of the delayline.
     * @return The signal value (z^(-index)).
     */
    inline T get_z(int64_t index) {
        if (bufsize == 0) return 0;
        if (index > bufsize) index = bufsize;
        if (index <= 0) index = 1;
        return _get_z(index);
    }

    inline T _get_z(int64_t index) {
        int64_t readpoint = bufidx - index;
        if (readpoint < 0) readpoint += bufsize;
        return buffer[readpoint];
    }

    inline T process(T input) {
        if (bufsize == 0) return input;
        return _process(input);
    }

    inline T operator()(T input) { return process(input); }

    inline T _process(T input) {
        T bufout = buffer[bufidx];
        buffer[bufidx] = input;
        bufidx++;
        if (bufidx >= bufsize) bufidx = 0;
        return bufout;
    }


    inline T process_wf(T input) {
        if (bufsize == 0) return feedback * input;
        return _process_wf(input);
    }

    inline T _process_wf(T input) {
        T bufout = buffer[bufidx];
        buffer[bufidx] = feedback * input;
        bufidx++;
        if (bufidx >= bufsize) bufidx = 0;
        return bufout;
    }

private:
    delay<T>(const delay<T> &x);
    delay<T> &operator=(const delay<T> &x);
    T feedback;
    int64_t bufsize, bufidx;
    std::vector<T> buffer;
};

template<typename T>
class delayline {
public:
    delayline(MYFLOAT sr) {
        currentfs = sr;
        bufsize = baseidx = 0;
    }
    virtual void setSampleRate(T fs) {
        currentfs = fs;
    }


    virtual T getSampleRate() {
        return currentfs;
    }

    void setsize(int64_t size) {
        if (size <= 0) return;
        std::vector<T> new_buffer;
        new_buffer.resize(size, 0);
        if (bufsize > 0 && bufsize <= size) {
            for (int64_t i = 0; i < bufsize; i++) new_buffer[size - bufsize + i] = at(i);
        }
        if (bufsize > 0 && bufsize > size) {
            //int64_t cut = bufsize - size;
            for (int64_t i = 0; i < size; i++) new_buffer[i] = at(i);
        }
        buffer = new_buffer;
        bufsize = buffer.size();
        baseidx = 0;
    }


    int64_t getsize() {
        return bufsize;
    }

    virtual void mute() {
        std::fill(buffer.begin(), buffer.end(), 0);
    }


    virtual T process(T input) {
        // simple delay line example
        baseidx--;
        if (baseidx < 0) baseidx += bufsize;
        T lastOut = (*this)[0];
        (*this)[0] = input;
        return lastOut;
    }

    virtual void setPrimeMode(bool value) { primeMode = value; }

    virtual bool getPrimeMode() { return primeMode; }

    inline T &at(int64_t rindex) {
        int64_t readidx = baseidx + rindex;
        if (readidx >= bufsize) readidx -= bufsize;
        return buffer[readidx];
    }


    inline T &operator[](int64_t rindex) { return at(rindex); }

    inline T at(T rindex, T &ap_save) {
        T floor_mod = std::floor(rindex); // >= 0
        T frac = rindex - floor_mod; // >= 0

        int64_t readidx_a = baseidx + (int64_t) floor_mod;
        if (readidx_a >= bufsize) readidx_a -= bufsize;
        int64_t readidx_b = readidx_a + 1;
        if (readidx_b >= bufsize) readidx_b -= bufsize;

        T temp = buffer[readidx_b] + buffer[readidx_a] * (1 - frac) - (1 - frac) * ap_save;
        UNDENORMAL(temp);
        ap_save = temp;
        return temp;
    }

protected:
    inline void allpass(int64_t rindexbase, int64_t rlength, T feedback) {
        int64_t rindex1 = rindexbase, rindex2 = rindex1 + rlength;
        T r1 = (*this)[rindex1], r2 = (*this)[rindex2];
        r2 -= feedback * r1;
        r1 += feedback * r2;
        (*this)[rindex1] = r1;
        (*this)[rindex2] = r2;
    }

    virtual int64_t p_(T ms) {
        int64_t base = static_cast<int64_t>(currentfs * ms * 0.001);
        if (primeMode) { base = findNextPrime(base); }
        return base;
    }


    delayline(const delayline &x);

    delayline &operator=(const delayline &x);

    T currentfs;
    int64_t bufsize, baseidx;
    bool primeMode;
    std::vector<T> buffer;
};


template<typename T>
class DelayA {
public:
    DelayA() {
    }
    //! Default constructor creates a delay-line with maximum length of 4095 samples and delay = 0.5.
    /*!
      An StkError will be thrown if the delay parameter is less than
      zero, the maximum delay parameter is less than one, or the delay
      parameter is greater than the maxDelay value.
     */
    void init(T delay = 0.5, uint64_t maxDelay = 4095) {

        if (delay > (T) maxDelay) {

        }

        // Writing before reading allows delays from 0 to length-1.
        if (maxDelay + 1 > inputs_.size())
            inputs_.resize(maxDelay + 1, 0.0);

        inPoint_ = 0;
        this->setDelay(delay);
        apInput_ = 0.0;
        doNextOut_ = true;
    }


    //! Class destructor.
    ~DelayA() {
    }

    //! Clears all internal states of the delay line.
    void clear(void) {
        for (uint32_t i = 0; i < inputs_.size(); i++)
            inputs_[i] = 0.0;
        lastFrame_ = 0.0;
        apInput_ = 0.0;
    }


    //! Get the maximum delay-line length.
    uint64_t getMaximumDelay(void) { return inputs_.size() - 1; };

    //! Set the maximum delay-line length.
    /*!
      This method should generally only be used during initial setup
      of the delay line.  If it is used between calls to the tick()
      function, without a call to clear(), a signal discontinuity will
      likely occur.  If the current maximum length is greater than the
      new length, no memory allocation change is made.
    */
    void setMaximumDelay(uint64_t delay) {
        if (delay < inputs_.size()) return;
        inputs_.resize(delay + 1, 0.0);
    }


    //! Set the delay-line length
    /*!
      The valid range for \e delay is from 0.5 to the maximum delay-line length.
    */
    inline void setDelay(T delay) {
        uint64_t length = inputs_.size();
        if (delay + 1 > length) { // The value is too big.

        }

        if (delay < 0.5) {

        }

        T outPointer = inPoint_ - delay + 1.0;     // outPoint chases inpoint
        delay_ = delay;

        while (outPointer < 0)
            outPointer += length;  // modulo maximum length

        outPoint_ = (int64_t) outPointer;         // integer part
        if (outPoint_ == length) outPoint_ = 0;
        alpha_ = 1.0 + outPoint_ - outPointer; // fractional part

        if (alpha_ < 0.5) {
            // The optimal range for alpha is about 0.5 - 1.5 in order to
            // achieve the flattest phase delay response.
            outPoint_ += 1;
            if (outPoint_ >= length) outPoint_ -= length;
            alpha_ += (T) 1.0;
        }

        coeff_ = (1.0 - alpha_) / (1.0 + alpha_);  // coefficient for allpass
    }

    //! Return the current delay-line length.
    T getDelay(void) const { return delay_; };

    //! Return the value at \e tapDelay samples from the delay-line input.
    /*!
      The tap point is determined modulo the delay-line length and is
      relative to the last input value (i.e., a tapDelay of zero returns
      the last input value).
    */
    T tapOut(uint64_t tapDelay) {
        int64_t tap = inPoint_ - tapDelay - 1;
        while (tap < 0) // Check for wraparound.
            tap += inputs_.size();

        return inputs_[tap];
    }

    //! Set the \e value at \e tapDelay samples from the delay-line input.
    void tapIn(T value, uint64_t tapDelay) {
        int64_t tap = inPoint_ - tapDelay - 1;
        while (tap < 0) // Check for wraparound.
            tap += inputs_.size();

        inputs_[tap] = value;
    }

    //! Return the last computed output value.
    T lastOut(void) const { return lastFrame_; };

    //! Return the value which will be output by the next call to tick().
    /*!
      This method is valid only for delay settings greater than zero!
     */
    T nextOut(void) {
        if (doNextOut_) {
            // Do allpass interpolation delay.
            nextOutput_ = -coeff_ * lastFrame_;
            nextOutput_ += apInput_ + (coeff_ * inputs_[outPoint_]);
            doNextOut_ = false;
        }

        return nextOutput_;
    }


    //! Input one sample to the filter and return one output.
    inline T tick(T input) {
        inputs_[inPoint_++] = input;

        // Increment input pointer modulo length.
        if (inPoint_ == inputs_.size())
            inPoint_ = 0;

        lastFrame_ = nextOut();
        doNextOut_ = true;

        // Save the allpass input and increment modulo length.
        apInput_ = inputs_[outPoint_++];
        if (outPoint_ == inputs_.size())
            outPoint_ = 0;

        return lastFrame_;
    }

    inline T tick(T input, T delay) {
        setDelay(delay);
        inputs_[inPoint_++] = input;

        // Increment input pointer modulo length.
        if (inPoint_ == inputs_.size())
            inPoint_ = 0;

        lastFrame_ = nextOut();
        doNextOut_ = true;

        // Save the allpass input and increment modulo length.
        apInput_ = inputs_[outPoint_++];
        if (outPoint_ == inputs_.size())
            outPoint_ = 0;

        return lastFrame_;
    }

protected:
    std::vector<T> inputs_;
    uint64_t inPoint_{};
    uint64_t outPoint_{};
    T delay_;
    T alpha_;
    T coeff_;
    T apInput_;
    T nextOutput_;
    bool doNextOut_;
    T lastFrame_{};
};


template<typename T>
class SimpleDelay {
public:
    SimpleDelay(int32_t maxdelay) {
        setDelay(maxdelay);
        delay = 0;
    }

    inline void write(T val) {
        delayline[index] = val;
        if (++index >= delay)
            index = 0;
    }

    inline T read() {
        return delayline[index];
    }

    inline T tick(T val) {
        delayline.data()[index] = val;
        if (++index >= delay)
            index = 0;
        return delayline[index];
    }

    inline void reset() {
        std::fill(delayline.begin(), delayline.end(), 0);
    }

    void setDelayms(float ms, int32_t sr) {
        int32_t _delay = (int) (sr  * ms * .001);
        setDelay(_delay);
    }

    void setDelay(int32_t delaysmpls) {
        if (delaysmpls > delayline.size())
            delayline.resize(delaysmpls);
        reset();
        delay = delaysmpls;
        index = 0;
    }

private:
    int32_t delay;
    std::vector<T> delayline;
    int32_t index{};
};

template<typename T>
class SimpleDelay2 {
public:
    void init(int32_t maxdelay, int32_t initdelay = 0) {
        delayline.resize(maxdelay, 0);
        delay = initdelay;
    }

    inline T tick(T val) {
        delayline.data()[index] = val * _fadein;
        _fadein += _fadeincin;
        if(_fadein > 1.0){
            _fadein = 1.0;
            _fadeincin = 0.0;
        }


        _fadeout += _fadeincout;
        if(_fadeout < 0) {
            _fadeout = 1.0;
            _fadeincout = 0.0;
            _fadein = 0.0;
            _fadeincin = _fadeconst;
            setDelay(_nextdelay);
        }
        if (++index >= delay)
            index = 0;
        return delayline[index] * _fadeout;
    }

    inline void reset() {
        std::fill(delayline.begin(), delayline.end(), 0);
    }

    void setDelayMS(float ms, int32_t sr) {
        int32_t _delay = (int) (sr * ms * .001);
        _nextdelay = _delay;
        _fadeincout = -_fadeconst;
    }

private:
    void setDelay(int32_t delaysmpls) {
        if (delaysmpls > delayline.size())
            delayline.resize(delaysmpls);
        reset();
        delay = delaysmpls;
        index = 0;
    }
    int32_t delay, _nextdelay;
    std::vector<T> delayline;
    int32_t index{};
    const T _fadeconst = 2. / 48000.;
    T _fadein{}, _fadeout{1.}, _fadeincout{0}, _fadeincin{_fadeconst};
};


#endif //GRAINSTORM_DELAYBASE_H
