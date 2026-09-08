//
// Created by phaet on 22.05.2024.
//

#ifndef GRAINSTORM_NOISE_H
#define GRAINSTORM_NOISE_H
#include <defines.h>
#include "base.h"

class NoiseBase;

class NoiseEffect : public Effect{
public:
    NoiseEffect(TRACK *t, int32_t chan, int type);
    ~NoiseEffect()override;
    void compute(MYFLOAT *, int)override;
private:
    std::unique_ptr<NoiseBase> noises[3];
    std::atomic<MYFLOAT> *_noisetype;
};


#endif //GRAINSTORM_NOISE_H
