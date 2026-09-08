#pragma once
//
// Created by pr on 21.10.20.
//

#ifndef GRAINSTORM_GRAINQueue_H
#define GRAINSTORM_GRAINQueue_H


#include <cstdint>
#include <mutex>
#include <deque>
#include "defines.h"
#include "types.h"
#include "vco.h"
#include "oscil.h"
#include "Reverb.h"
#include "Modal.h"
#include "base.h"
struct TRACK;


class ResonGrain: public GrainEffect{
public:
    MYFLOAT tick(MYFLOAT, int,int) override ;
    void prepare(TRACK *);
private:
    MYFLOAT mix, gain;
    MYFLOAT x1,x2,y1,y2,c1,c2,r;
};

class GrainReverb : public GrainEffect{
public:
    MYFLOAT tick(MYFLOAT, int,int)override ;
    void prepare(TRACK *t, int32_t chan, int size);
private:
    MYFLOAT *window;
    MYFLOAT sp, sl, mix, gain;
    Ap<MYFLOAT> _diff1[4];
    Delay<MYFLOAT> _delay[4];
    MYFLOAT _filtstate[4]{};
    MYFLOAT _feedback[4]{};
    MYFLOAT _c{};
    MYFLOAT _averagedelay{}, _gainfact{.5};
    static MYFLOAT _tdiff1[4];
    static MYFLOAT _tdelay[4];

    void setDamp(MYFLOAT damp) {
        _c = damp * .75f;
    }

    void setT60(MYFLOAT sr, MYFLOAT T60);
    void init(MYFLOAT sr, int32_t chan);
};



struct grain_t {
    grain_t(TRACK *t, int32_t channel) : track(t), chan(channel){
        activeFX.reserve(7);
    }
    void init(std::shared_ptr<std::vector<short>> &filepointer, MYFLOAT gain, int32_t writeoffset, double readoffset, MYFLOAT pitch, int readdirection, int grainsize,
              bool env_only, MYFLOAT *table,uint32_t frq ,uint32_t lobits, uint32_t mask, MYFLOAT pfrac);
    MYFLOAT tick();
    bool _envonly{};
    std::shared_ptr<std::vector<short>>_filepointer{};
    double _readoffset{};
    int32_t _writeoffset;
    MYFLOAT _pitch{};
    MYFLOAT _gain;
    MYFLOAT *_env;
    uint32_t _phs{};
    int32_t _count{};
    int32_t _grainsize{};
    bool _interpol{};
    uint32_t _lobits{}, _mask{};
    MYFLOAT _pfrac{};
    uint32_t _frq{};
    grain_t *prev{};
    grain_t *next{};
    std::vector<GrainEffect*> activeFX;
    GrainFilt grainFilter;
    GrainBuzz grainBuzz;
    ResonGrain resonGrain;
    GrainReverb grainReverb;
    GrainModal grainModal;
    GrainRingMod ringMod;
    TRACK *track;
    int32_t chan;
};

class grainqueue {
public:
    grainqueue(TRACK *track, int32_t channel) : grainqueue(500, track, channel) {}

    grainqueue(uint32_t size, TRACK *track, int32_t channel)  {
        _avail.reserve(size);
        for(int32_t i=0;i<size;i++)
            _avail.emplace_back(track, channel);
        totalsize = size;
        for (int32_t i = 0; i < _avail.size(); i++) {
            _nodes.push_back(&_avail[i]);
        }
    }

    ~grainqueue() {
        flush();
    }

    struct grain_t *_first = nullptr;
    struct grain_t *_last = nullptr;
    std::vector<grain_t> _avail;
    std::deque<grain_t *> _nodes;

    grain_t *get() {
        if (_nodes.empty())
            return nullptr;
        grain_t *ret = _nodes.back();
        ret->next = ret->prev = nullptr;
        _nodes.pop_back();
        push(ret);
        return ret;
    }


    inline grain_t *newNode() {
        if (_nodes.empty())
            return nullptr;
        else {
            grain_t *ret = _nodes.back();
            ret->next = ret->prev = nullptr;
            _nodes.pop_back();
            return ret;
        }
    }

    inline MYFLOAT tick() {
        MYFLOAT ret = 0;
        grain_t *first = _first;
        while (first) {
            ret += first->tick();
            if (first->_count >= first->_grainsize) {
                del(&_first, &_last, first);
            }
            first = first->next;
        }
        return ret;
    }


    inline void push(grain_t *node) {
        if (_first == nullptr) {
            _first = _last = node;
            node->prev = node->next = nullptr;
        } else {
            _last->next = node;
            node->prev = _last;
            _last = node;
            node->next = nullptr;
        }
    }

    void flush() {
        grain_t *first = _first;
        while (first) {
                del(&_first, &_last, first);
            first = first->next;
        }
    }


    inline void del(grain_t **first, grain_t **last, grain_t *node) {
        node->_filepointer = nullptr;
        node->activeFX.clear();
        if (node == *first) {
            if (!node->next) {
                *first = *last = nullptr;
            } else {
                *first = node->next;
                (*first)->prev = nullptr;
            }
        } else {
            if (node == *last) {
                *last = node->prev;
                (*last)->next = nullptr;
            } else {
                (node->next)->prev = node->prev;
                (node->prev)->next = node->next;
            }
        }
        _nodes.push_back(node);
    }

    int32_t size() {
        return totalsize - _avail.size();
    }
    int32_t totalsize;
};

#endif //GRAINSTORM_GRAINQueue_H
