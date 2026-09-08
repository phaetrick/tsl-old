//
// Created by pr on 10.11.23.
//

#include "SyncTargets.h"
#include <app.h>
#include "track.h"
#include "grainstorm.h"

using namespace tsl::parameters;
using namespace tsl::Syncing;

SyncResult DelaySyncTarget::syncBySamples(double samples, std::vector<tsl::parameters::Event>& events) {
	
	SyncResult res{};

	auto _appState = parent->_appState;

	const int32_t min_delay = _DATA->min_delay_samples;
	const int32_t max_delay = _DATA->max_delay_samples;

	//long delay_smpls_const = (long) (pow(10, dest->load() * .05) * _STATE->sr * .001);

	MYFLOAT syncfactor = syncFact();
	while (samples < min_delay)
		samples *= syncfactor;
	while (samples > max_delay)
		samples /= syncfactor;

	auto val = LOG10D20F(samples / _STATE->sr * 1000.);

	auto old = _STATE->params[parent->index][delayid].exchange(val);
	if (old != val) {
		Event ev;
		ev.eventType = Eventtype::paramUpdate;
		ev.trackIndex = parent->index;
		ev.paramIndex = delayid;
		ev.value = val;
		events.push_back(ev);
		_STATE->toUiThreadQueue.try_push([_STATE, this, v = _STATE->parameters[delayid].view] {
			if (_STATE->active_track.load() == parent->index && v->visible_) {
				v->redraw();
			}
		});
		res |= SyncFlags::Ok;
		return res;
	}
	res |= SyncFlags::UnChanged;
	return res;
}

double DelaySyncTarget::getSamples() {
	auto _appState = parent->_appState;
	return LOG2NORMAL(_STATE->params[parent->index][delayid].load()) * _STATE->sr * .001;
}

SyncResult DelaySyncTarget::control(uint16_t todo, std::vector<tsl::parameters::Event>& events) {
	auto _appState = parent->_appState;
	auto ret = SyncResult{};
	switch (todo) {
	case SLOWButton: {

		MYFLOAT delay = LOG2NORMALF(_STATE->params[parent->index][delayid].load()) *
			syncFact();
		
		if (delay < MIN_DELAY_MS) {
			ret |= SyncFlags::MinReached;
			return ret;
		}
		else if (delay > MAX_DELAY_MS) {
			ret |= SyncFlags::MaxReached;
			return ret;
		}
		
		_STATE->params[parent->index][delayid].store(
			LOG10D20F(delay));
		Event ev;
		ev.eventType = Eventtype::paramUpdate;
		ev.trackIndex = parent->index;
		ev.paramIndex = delayid;
		ev.value = LOG10D20F(delay);
		events.push_back(ev);
		_STATE->toUiThreadQueue.try_push([this, _STATE, v = _STATE->parameters[delayid].view] {
			if (_STATE->active_track.load() == parent->index && v->visible_) {
				v->redraw();
			}
			});
		ret |= SyncFlags::Ok;
		return ret;
	}

		
	case FASTButton: {
		MYFLOAT delay = LOG2NORMAL(_STATE->params[parent->index][delayid].load()) /
			syncFact();
		
		if (delay < MIN_DELAY_MS) {
			ret |= SyncFlags::MinReached;
			return ret;
		}
		else if (delay > MAX_DELAY_MS) {
			ret |= SyncFlags::MaxReached;
			return ret;
		}
		Event ev;
		ev.eventType = Eventtype::paramUpdate;
		ev.trackIndex = parent->index;
		ev.paramIndex = delayid;
		ev.value = LOG10D20F(delay);
		events.push_back(ev);
		_STATE->params[parent->index][delayid].store(
			LOG10D20F(delay));
		_STATE->toUiThreadQueue.try_push([this, _STATE, v = _STATE->parameters[delayid].view] {
			if (_STATE->active_track.load() == parent->index && v->visible_) {
				v->redraw();
			}
			});

		ret |= SyncFlags::Ok;
		return ret;


	}
	case STEPBACK:
	case STEPFORW:
	case STOPButton:
	case PLAYButton:
	case DIRButton:
	default:
		ret |= SyncFlags::NotImplemented;
		return ret;
	}
}

SyncResult BPMSyncTarget::syncBySamples(double total_loop_length_samples, std::vector<tsl::parameters::Event> &events) {
	auto _appState = parent->_appState;
	auto tindex = _STATE->active_track.load();
	SyncResult res{};

	if (total_loop_length_samples <= 0) {
		// Invalid input
		res |= SyncFlags::LoopTooShort;
		return res;
    }

	// Calculate samples per single beat from the total loop length
	double samples_per_single_beat_derived = total_loop_length_samples;

	
	// Calculate initial BPM from the derived samples_per_single_beat
	double bpm_to_store = (60.0 * _STATE->sr) / samples_per_single_beat_derived;

	// min_samples_per_beat_limit and max_samples_per_beat_limit are based on the BPM limits.
	// These define the valid range for ONE beat's worth of samples.
	const double min_samples_per_beat_limit = (60.0 / maxBPM) * _STATE->sr;
	const double max_samples_per_beat_limit = (60.0 / minBPM) * _STATE->sr;

	MYFLOAT syncfactor = syncFact();
	double adjusted_samples_per_single_beat = samples_per_single_beat_derived;

	// Adjust 'adjusted_samples_per_single_beat' to fit within the limits defined by minBPM/maxBPM
	while (adjusted_samples_per_single_beat < min_samples_per_beat_limit && min_samples_per_beat_limit > 0) {
		adjusted_samples_per_single_beat *= syncfactor;
	}
	while (adjusted_samples_per_single_beat > max_samples_per_beat_limit && max_samples_per_beat_limit > 0) {
		adjusted_samples_per_single_beat /= syncfactor;
	}

	// Convert the *adjusted_samples_per_single_beat* back to the BPM to store
	if (adjusted_samples_per_single_beat <= 0) {
		bpm_to_store = minBPM;
	}
	else {
		bpm_to_store = (60.0 * _STATE->sr) / adjusted_samples_per_single_beat;
	}

	// Final clamp for the calculated BPM
	if (bpm_to_store < minBPM) bpm_to_store = minBPM;
	if (bpm_to_store > maxBPM) bpm_to_store = maxBPM;

	auto old = _STATE->params[parent->index][BPMSYNC].exchange(bpm_to_store);


	if (old != bpm_to_store) {
		Event ev;
		ev.eventType = Eventtype::paramUpdate;
		ev.trackIndex = parent->index;
		ev.paramIndex = BPMSYNC;
		ev.value = bpm_to_store;
		events.push_back(ev);
		_STATE->toUiThreadQueue.try_push([_STATE, this, v = _STATE->parameters[BPMSYNC].view] {
			if (_STATE->active_track.load() == parent->index && v->visible_) {
				v->redraw();
			}
			});
		res |= SyncFlags::Ok;
		return res;
	}

	res |= SyncFlags::UnChanged;
	return res;
}

double BPMSyncTarget::getSamples() { // Returns total loop length in samples
	auto _appState = parent->_appState;
	MYFLOAT current_BPM = _STATE->params[parent->index][BPMSYNC].load();

	// Calculate samples for a single beat at the current BPM
	return (60.0 / current_BPM) * _STATE->sr;
}

SyncResult BPMSyncTarget::control(uint16_t todo, std::vector<tsl::parameters::Event>& events) {
	auto _appState = parent->_appState;
	MYFLOAT current_BPM = _STATE->params[parent->index][BPMSYNC].load();
	MYFLOAT new_BPM = current_BPM;
	auto res = SyncResult{};

	switch (todo) {
	case SLOWButton: // Decrease BPM
		new_BPM /= syncFact();
		if (new_BPM < minBPM) {
			res |= SyncFlags::MinReached;
			return res;
		}
		break;
	case FASTButton: // Increase BPM
		new_BPM *= syncFact();
		if (new_BPM > maxBPM) {
			res |= SyncFlags::MaxReached;
			return res;
		}
		break;
	case STEPBACK:
	case STEPFORW:
	case STOPButton:
	case PLAYButton:
	case DIRButton:
	default:
		res |= SyncFlags::NotImplemented;
		return res; // Unhandled
	}

	if (new_BPM != current_BPM) {
		Event ev;
		ev.eventType = Eventtype::paramUpdate;
		ev.trackIndex = parent->index;
		ev.paramIndex = BPMSYNC;
		ev.value = new_BPM;
		events.push_back(ev);
		_STATE->params[parent->index][BPMSYNC].store(new_BPM);
		_STATE->toUiThreadQueue.try_push([this, _STATE, v = _STATE->parameters[BPMSYNC].view] {
			if (_STATE->active_track.load() == parent->index && v->visible_) {
				v->redraw();
			}
			});
		res |= SyncFlags::Ok;
		return res;
	}
	res |= SyncFlags::UnChanged;
	return res;
}


