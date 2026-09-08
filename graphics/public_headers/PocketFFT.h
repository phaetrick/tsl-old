#pragma once
//
// Created by pr on 07.10.22.
//

#ifndef GRAINSTORM_POCKETFFT_H
#define GRAINSTORM_POCKETFFT_H
#include <stdlib.h>

extern "C" {

struct cfft_plan_i;
typedef struct cfft_plan_i * cfft_plan;
cfft_plan make_cfft_plan (size_t length);
void destroy_cfft_plan (cfft_plan plan);
int cfft_backward(cfft_plan plan, double c[], double fct);
int cfft_forward(cfft_plan plan, double c[], double fct);
size_t cfft_length(cfft_plan plan);

struct rfft_plan_i;
typedef struct rfft_plan_i * rfft_plan;
rfft_plan make_rfft_plan (size_t length);
void destroy_rfft_plan (rfft_plan plan);
int rfft_backward(rfft_plan plan, double c[], double fct);
int rfft_forward(rfft_plan plan, double c[], double fct);
size_t
rfft_length(rfft_plan plan);


}


#endif //GRAINSTORM_POCKETFFT_H
