#include <player.h>
#include "gaintask.h"
#include "logger.h"
#include "track.h"
#include "app.h"

tsl::gaintask::gaintask(TRACK* t) : _track{ t } {}

void tsl::gaintask::setTarget(float curDb, float targetDb) {
	state.store({ curDb , targetDb });
}

void tsl::gaintask::setTarget(Task task, bool wait) {
	if (task == GainDown)
		setTarget(0, -120);
	else
		setTarget(-120, 0);
	if (wait){
		_track->_STATE->waitNotify.sleep_for(tsl::gainTaskFadeMs);
	}
}

void tsl::gaintask::setTargetAutoWait(Task task) {
	if (task == GainDown) {
		setTarget(0, -120);
		_track->_STATE->waitNotify.sleep_for(tsl::gainTaskFadeMs);
	}
	else {
		setTarget(-120, 0);
		}
}

void tsl::gaintask::compute(MYFLOAT* inl, MYFLOAT* inr, MYFLOAT* outl, MYFLOAT* outr, int32_t size) {
	auto s = state.load(std::memory_order_acquire);
	MYFLOAT cur = s.current_db;
	MYFLOAT target = s.target_db;
	if (cur == target && target == 0) {
		return;
	}

	MYFLOAT inc = 120.0 / (gainTaskFadeMs * 0.001 * _track->_STATE->sr);
	if (target == -120) inc *= -1.;
	for (int i = 0; i < size; i++) {
		cur += inc;
		cur = std::clamp(cur, -120., 0.);
		MYFLOAT gain = std::pow(10.0, cur / 20.0);
		outl[i] = inl[i] * gain;
		outr[i] = inr[i] * gain;
	}
	// Only write back cur if state hasn't changed under us
	state_t expected = s;
	state_t desired = { (float)cur, s.target_db };
	// If it fails, a setTarget fired mid-buffer — discard our cur update,
	// next compute will pick up the new state
	state.compare_exchange_strong(expected, desired,
		std::memory_order_release,
		std::memory_order_relaxed);
}