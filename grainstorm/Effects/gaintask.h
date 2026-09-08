//
// Created by pr on 27.03.26.
//
#pragma once
#include <atomic>
#include <cmath>
#include <algorithm>
#include "defines.h"

struct TRACK;
namespace tsl {
	static constexpr MYFLOAT gainTaskFadeMs = 50; // dB per second
    struct gaintask {
        enum Task : int32_t {
            GainUp,
            GainDown
		};
        struct state_t {
            float current_db;
            float target_db;
		};
        gaintask(TRACK* t);

        void compute(MYFLOAT* inl, MYFLOAT* inr, MYFLOAT* outl, MYFLOAT* outr, int32_t size);
        void setTarget(float curDb, float targetDb);
        void setTarget(Task task, bool wait);
        void setTargetAutoWait(Task task);

        std::atomic<state_t> state{};
        TRACK* _track{ nullptr };
       };
}