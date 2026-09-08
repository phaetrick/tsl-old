#pragma once
//
// Created by pr on 23.01.19.
//

#ifndef GRAINSTORM_CALLBACKS_FXPOWER_H
#define GRAINSTORM_CALLBACKS_FXPOWER_H

struct TRACK;
typedef bool (*callback_fx_power) (TRACK *track, bool power);
void setup_fx_callbacks(callback_fx_power *cbs);

#endif //GRAINSTORM_CALLBACKS_FXPOWER_H
