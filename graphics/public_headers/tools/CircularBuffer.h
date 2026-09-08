#pragma once
//
// Created by pr on 12.10.22.
//

#ifndef GRAINSTORM_CIRCULARBUFFER_H
#define GRAINSTORM_CIRCULARBUFFER_H

#include <atomic>
#include <vector>
#include <cstring>
#include "aligned_memalloc.h"

namespace tsl {
    template<typename T>
    class CircularBuffer {
    protected:
        CircularBuffer(int size, float overlap):_buffersize(size), _hopsize((int) (size / overlap)) {
            inbuf.resize(_buffersize, 0);
            outbuf.resize(_buffersize, 0);
            computeBuf.resize(_buffersize, 0);
            }
        virtual void onBufferReady(T*, int size) = 0;

        inline T _tick(T in) {
            auto out = outbuf[count];
            inbuf[_buffersize - _hopsize + count] = in;
            if(++count==_hopsize){
                count = 0;
                computeBuf = inbuf;
                onBufferReady(computeBuf.data(),_buffersize);
                for(int i = 0;i<_buffersize - _hopsize;i++){
                    inbuf[i] = inbuf[i+_hopsize];
                    outbuf[i] = outbuf[i+_hopsize];
                    outbuf[i] += computeBuf[i];
                }
                for(int i=_buffersize-_hopsize;i<_buffersize;i++){
                    outbuf[i] = computeBuf[i];
                }
            }
            return out;
        }

        int getDelay() {
            return _buffersize;
        }

    private:
        tsl::AlignedVector<T> inbuf, outbuf, computeBuf;
        int count{};
        const int _buffersize;
        const int _hopsize;
    };

    template<typename T>
    class CircularBufferCross {
    protected:
        CircularBufferCross(int size, float overlap):_buffersize(size), _hopsize((int) (size / overlap)) {
            inbuf1.resize(_buffersize, 0);
            inbuf2.resize(_buffersize, 0);
            outbuf.resize(_buffersize, 0);
            computeBuf1.resize(_buffersize, 0);
            computeBuf2.resize(_buffersize, 0);
        }
        virtual void onBufferReady(T*, T*, int s) = 0;

        inline T _tick(T in1, T in2) {
            auto out = outbuf[count];
            inbuf1[_buffersize - _hopsize + count] = in1;
            inbuf2[_buffersize - _hopsize + count] = in2;
            if(++count==_hopsize){
                count = 0;
                computeBuf1 = inbuf1;
                computeBuf2 = inbuf2;
                onBufferReady(computeBuf1.data(),computeBuf2.data(), _buffersize);
                for(int i = 0;i<_buffersize - _hopsize;i++){
                    inbuf1[i] = inbuf1[i+_hopsize];
                    inbuf2[i] = inbuf2[i+_hopsize];
                    outbuf[i] = outbuf[i+_hopsize];
                    outbuf[i] += computeBuf1[i];
                }
                for(int i=_buffersize-_hopsize;i<_buffersize;i++){
                    outbuf[i] = computeBuf1[i];
                }
            }
            return out;
        }

        int getDelay() {
            return _buffersize;
        }

    private:
        tsl::AlignedVector<T> inbuf1, inbuf2 ,outbuf, computeBuf1, computeBuf2;
        int count{};
        const int _buffersize;
        const int _hopsize;
    };

    class CircularBufferStereo {
    public:
        CircularBufferStereo(int size, float overlap,
                             void (*func)(void *obj, float *bufl, float *bufr, int size),
                             void *obj, int rndoff = 0) {
            _rndoff = rndoff;
            _func = func;
            _obj = obj;
            _buffersize = size;
            _hopsize = (int) (size / overlap);
            sizeringbuffer = 0;
            while (sizeringbuffer < 2 * 16384) {
                sizeringbuffer += size;
            }
            buf.resize(4 * sizeringbuffer + 2 * size, 0);
            ringbufferout[0] = buf.data();
            ringbufferin[0] = buf.data() + sizeringbuffer;
            ringbufferout[1] = buf.data() + 2 * sizeringbuffer;
            ringbufferin[1] = buf.data() + 3 * sizeringbuffer;
            computebuffer[0] = buf.data() + 4 * sizeringbuffer;
            computebuffer[1] = buf.data() + 4 * sizeringbuffer + size;

            framestoprocess = offsetoutread = offsetinread = offsetinwrite = framesready = 0;
        }

        void setSize(int size) {
            _buffersize.store(size);
        }

        void setOverlap(float overlap) {
            _hopsize = (int) (_buffersize / overlap);
        }

        void compute(float *inl, float *inr, float *outl, float *outr, int frames) {
            int sizeold = _buffersize.load();
            int hopsize = _hopsize.load();

            int left = sizeringbuffer - offsetinwrite;
            if (left > frames) {
                std::memcpy(ringbufferin[0] + offsetinwrite, inl, sizeof(float) * frames);
                std::memcpy(ringbufferin[1] + offsetinwrite, inr, sizeof(float) * frames);
            } else {
                std::memcpy(ringbufferin[0] + offsetinwrite, inl, sizeof(float) * left);
                std::memcpy(ringbufferin[1] + offsetinwrite, inr, sizeof(float) * left);

                int off = frames - left;
                std::memcpy(ringbufferin[0], inl + frames - off, sizeof(float) * off);
                std::memcpy(ringbufferin[1], inr + frames - off, sizeof(float) * off);
            }
            offsetinwrite += frames;
            if (offsetinwrite >= sizeringbuffer)
                offsetinwrite -= sizeringbuffer;
            framestoprocess += frames;

            while (framestoprocess > sizeold) {
                left = sizeringbuffer - offsetinread;
                if (left > sizeold) {
                    std::memcpy(computebuffer[0], ringbufferin[0] + offsetinread,
                           sizeof(float) * sizeold);
                    std::memcpy(computebuffer[1], ringbufferin[1] + offsetinread,
                           sizeof(float) * sizeold);

                } else {
                    std::memcpy(computebuffer[0], ringbufferin[0] + offsetinread, sizeof(float) * left);
                    std::memcpy(computebuffer[1], ringbufferin[1] + offsetinread, sizeof(float) * left);

                    int off = sizeold - left;
                    std::memcpy(computebuffer[0] + sizeold - off, ringbufferin[0], sizeof(float) * off);
                    std::memcpy(computebuffer[1] + sizeold - off, ringbufferin[1], sizeof(float) * off);
                }

                _func(_obj, computebuffer[0], computebuffer[1], sizeold);

                int offsetinreadtmp = offsetinread + rand()/RAND_MAX*_rndoff;
                for (int i = 0; i < sizeold; i++) {
                    if (offsetinreadtmp >= sizeringbuffer)
                        offsetinreadtmp -= sizeringbuffer;
                    ringbufferout[0][offsetinreadtmp] += computebuffer[0][i];
                    ringbufferout[1][offsetinreadtmp] += computebuffer[1][i];
                    offsetinreadtmp++;

                }
                offsetinread += hopsize;
                if (offsetinread >= sizeringbuffer)
                    offsetinread -= sizeringbuffer;
                framestoprocess -= hopsize;
                framesready += hopsize;
            }

            if (framesready > frames) {
                left = sizeringbuffer - offsetoutread;
                if (left > frames) {
                    std::memcpy(outl, ringbufferout[0] + offsetoutread, sizeof(float) * frames);
                    std::memcpy(outr, ringbufferout[1] + offsetoutread, sizeof(float) * frames);
                    std::memset(ringbufferout[0] + offsetoutread, 0, sizeof(float) * frames);
                    std::memset(ringbufferout[1] + offsetoutread, 0, sizeof(float) * frames);
                } else {
                    std::memcpy(outl, ringbufferout[0] + offsetoutread, sizeof(float) * left);
                    std::memcpy(outr, ringbufferout[1] + offsetoutread, sizeof(float) * left);
                    std::memset(ringbufferout[0] + offsetoutread, 0, sizeof(float) * left);
                    std::memset(ringbufferout[1] + offsetoutread, 0, sizeof(float) * left);
                    int off = frames - left;
                    std::memcpy(outl + frames - off, ringbufferout[0], sizeof(float) * off);
                    std::memcpy(outr + frames - off, ringbufferout[1], sizeof(float) * off);
                    std::memset(ringbufferout[0], 0, sizeof(float) * off);
                    std::memset(ringbufferout[1], 0, sizeof(float) * off);
                }
                offsetoutread += frames;
                if (offsetoutread >= sizeringbuffer)
                    offsetoutread -= sizeringbuffer;
                framesready -= frames;
            }
        }

    private:
        std::vector<float> buf;
        float *ringbufferout[2], *ringbufferin[2];
        float *computebuffer[2];
        int sizeringbuffer;
        int framestoprocess;
        std::atomic<int> _buffersize;
        std::atomic<int> _hopsize;
        int offsetinread, offsetinwrite, offsetoutread;
        int framesready;

        void (*_func)(void *obj, float *bufl, float *bufr, int size);

        void *_obj;
        int _rndoff{0};
    };
}

#endif //GRAINSTORM_CIRCULARBUFFER_H
