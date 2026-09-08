//
// Created by pr on 19.09.23.
//

#include "MidiReceiver.h"
#include "grainstorm.h"
#include <MidiLearning.h>
#include <app.h>

using namespace tsl::midi;

static constexpr float MIDIINC = 1. / 24.;
static constexpr int32_t middlec = 60;

#define ONEd12          (FL(0.08333333333333333333333))
#define ONEd1200        (FL(0.00083333333333333333333))

#ifdef __ANDROID__
void java_receive_midievent(JNIEnv* env, jclass obj, jbyte _one, jbyte _two, jbyte _three) {
	auto one = (uint8_t)_one;
	auto two = (uint8_t)_two;
	auto three = (uint8_t)_three;
	tsl::AppState* _appState = __STATE;
#else
void onMidiMsg(tsl::AppState * _appState, uint8_t one, uint8_t two, uint8_t three) {
#endif

	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false || _DATA->inputdisabled.load())return;

	int32_t channel;

	if (one == STATUS_REALTIME_EVENT) {
		if (two == STATUS_START) {
			_DATA->midiclockactive = true;
			_DATA->midiclockreset = true;
			return;
		}
		else if (two == STATUS_STOP) {
			_DATA->midiclockactive = false;
			return;
		}
		else if (two == STATUS_CONTINUE) {
			_DATA->midiclockactive = true;
			return;
		}
		else if (!_DATA->midiclockactive || two != STATUS_TIMING_CLOCK)
			return;
		{
			std::lock_guard lk(_DATA->midilfoqueue);
			auto n = _DATA->midilfoqueue._first;

			while (n) {
				auto lfo = n->data;
				if (!_STATE->params[lfo->track->index][LFO1POWER + lfo->index * LFONUMPARAMS].load() ||
					_STATE->params[lfo->track->index][LFO1STOPPED + lfo->index * LFONUMPARAMS].load()) {
					n = n->next;
					continue;
				}
				if (_DATA->midiclockreset) {
					_STATE->params[lfo->track->index][LFO1PHS + lfo->index * LFONUMPARAMS] = 0;
				}
				else {
					auto phase = _STATE->params[lfo->track->index][LFO1PHS + lfo->index * LFONUMPARAMS].load();
					phase += MIDIINC * _STATE->params[lfo->track->index][LFO1DIR + lfo->index * LFONUMPARAMS].load() *
						_STATE->params[lfo->track->index][LFO1CLOCKMULTI + lfo->index * LFONUMPARAMS].load();
					while (phase > 1.0)
						phase -= 1.0;
					while (phase < 0)
						phase += 1.0;
					_STATE->params[lfo->track->index][LFO1PHS + lfo->index * LFONUMPARAMS].store(phase);
				}
				//else LOGE("%02x %02x %02x", (char) one & 0xFF, (char) two & 0xFF, (char) three & 0xFF);
				n = n->next;
			}
		}

		if (_DATA->midiclockreset)
			_DATA->midiclockreset = false;
		return;
	}

	std::lock_guard lk(_STATE->mutex_midi);

	auto command = (uint8_t)(one & STATUS_COMMAND_MASK);
	switch (command) {
	case STATUS_NOTE_ON:
		if (three == 0)
			return;
		if (_STATE->midilearning_waiting) {
			auto ml = _STATE->midiLearning.load();
			if (!ml || (_STATE->parameters[ml->id].type != ParameterType_bool && _STATE->parameters[ml->id].type != ParameterType_enum))
				break;
			_STATE->activemidicommand[0] = one;
			_STATE->activemidicommand[1] = two;
			tsl::graphics::MidiLearning::setEventReceived(_STATE);
		}
		else {
			channel = (one & STATUS_CHANNEL_MASK);
			auto& e = _STATE->midinoteevents[channel][two];
			if (e.eventType != tsl::parameters::Eventtype::NoParam) {
				auto ne = e;
				if (ne.eventType == tsl::parameters::Eventtype::Power) {
					switch (ne.subType) {
					case tsl::parameters::powerFx:
					case tsl::parameters::powerGrainFx:
					case tsl::parameters::powerStereoFx: {
						ne.power.pow = std::bit_cast<tsl::parameters::PowerState>(ne.getCurrentValue(_STATE)).pow == 1 ? 0 : 1;
					}
													   break;
					default:
						ne.value = ne.getCurrentValue(_STATE) == 1.0 ? 0.0 : 1.0;
						break;
					}
				}
				else {

					auto& m = _STATE->parameters[ne.getDisplayParam()];
					if (m.type == ParameterType_bool) {
						if (!m.getFlag(Param::NoValue))
							ne.value = ne.getCurrentValue(_STATE) == 1.0 ? 0.0 : 1.0;
						else
							ne.value = ne.getCurrentValue(_STATE);
						ne.flags |= tsl::parameters::Event::Redraw | tsl::parameters::Event::Info;
					}
					else {
						ne.value = m.toDisplay(_STATE->sr, ne.getCurrentValue(_STATE)) + (e.midiState.inc == 1 ? 1 : -1);
						if (ne.value >= m.getMax(_STATE->sr)) ne.value = m.getMin(_STATE->sr);
						else if (ne.value < m.getMin(_STATE->sr)) ne.value = m.getMax(_STATE->sr) - 1;
						ne.value = m.fromDisplay(_STATE->sr, ne.value);
						ne.flags |= tsl::parameters::Event::Redraw | tsl::parameters::Event::Info;
					}
				}
				ne.applyFromExt(_STATE, tsl::parameters::None);
			}
			else {
				// float fact = log2(powf(2, (two - middlec) * ONEd12));
				int32_t chan = channel % 4;
				auto& m = _STATE->parameters[PITCH];
				auto val = std::clamp((MYFLOAT)((two - middlec) * ONEd12), m.min, m.max);

				// This used to store straight into params[chan][PITCH] and hand-roll
				// a redraw, which left the new value in the engine only: the snapshot
				// never learned about it (so a save did not capture it and a reload
				// silently reverted it) and the host kept reading the old value on an
				// automatable parameter. Going through the Event system fixes all
				// three at once -- it is the same call the UI slider makes, and the
				// same one the MIDI-learned CC branch below already makes.
				tsl::parameters::Event e;
				e.setup(_STATE, chan, PITCH);
				// setup() adds History to any assignable param. A note is performance
				// input, not an edit: keeping it would push an undo entry per note
				// into history[]/redoEvents[], which are unbounded vectors, and undo
				// would step back through individual notes.
				e.setFlag(tsl::parameters::Event::History, false);
				e.flags |= tsl::parameters::Event::Redraw | tsl::parameters::Event::Info;
				e.value = val;
				// apply() exchanges the same atomic the old store wrote and only
				// notifies when the value actually changed, so repeated notes at one
				// pitch cost nothing. redrawEvent covers the UI update, with the
				// null view check the hand-rolled version was missing.
				e.apply(_STATE, tsl::parameters::None);

				//LOGE("%f %f %d", fact, (two - middlec) * ONEd12, _DATA->midiassignmentsnote[channel][two].miditarget);
			}
		}
		break;
	case STATUS_CONTROL_CHANGE:
		if (_STATE->midilearning_waiting) {
			auto ml = _STATE->midiLearning.load();
			if (!ml || _STATE->parameters[ml->id].type != ParameterType_double)
				break;
			_STATE->activemidicommand[0] = one;
			_STATE->activemidicommand[1] = two;
			tsl::graphics::MidiLearning::setEventReceived(_STATE);
		}
		else {
			channel = (one & STATUS_CHANNEL_MASK);
			auto e = _STATE->midicontrolevents[channel][two];
			if (e.eventType == tsl::parameters::Eventtype::NoParam) {
				return;
			}
			MYFLOAT value =
				(e.midiState.min +
				DISTANCE(e.midiState.min, e.midiState.max) * (MIDIDIVISOR * three)) * (1. / (double) UINT16_MAX);
			auto& m = _STATE->parameters[e.getDisplayParam()];
			value = m.fromNormalized(value);
			e.flags |= (tsl::parameters::Event::History | tsl::parameters::Event::Info | tsl::parameters::Event::Redraw);
			e.value = value;
			e.apply(_STATE, tsl::parameters::None);
		}
		break;
		/*
	case STATUS_PROGRAM_CHANGE:
		LOGE("Change %d", two);
		break;
	case STATUS_PITCH_BEND:
		bend = (three << 7) + two;
		LOGE("bend %d %d", channel, bend);
		break;
	case STATUS_NOTE_OFF:
		LOGE("OFF");
		break;
	case STATUS_POLYPHONIC_AFTERTOUCH:
		LOGE("Pressure Change %d %d", two, three);
		break;
	case STATUS_CHANNEL_PRESSURE:
		LOGE("Channel Change %d %d", two, three);
		break;
		 */
	default:
		break;
	}

}
