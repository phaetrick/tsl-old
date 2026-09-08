//
// Created by pr on 03.10.23.
//

#include <app.h>
#include "grainstorm.h"
#include "sequencer.h"
#include <logger.h>
#include <types.h>
#include <Midi.h>
#include <queue.h>
#include "view.h"
#include "preset.h"
#include "selector.h"
#include "synth.h"

using namespace tsl::midi;

LockFreeQueue<VCOPreEvent, 64> sequencer::preQueue;

static void push(tsl::AppState* _appState, const VCOPreEvent &e) {
    auto c = _DATA->seq_currentnote;
    if (e.type == STATUS_NOTE_ON) {
        if (_DATA->seq_nextcount && (double)_DATA->seq_count / (double)_DATA->seq_nextcount > .25) {
            c -= _DATA->seq_stepforward;
            if (c >= _DATA->seq_stepsold)
                c = 0;
            else if (c < 0)
                c = _DATA->seq_stepsold - 1;
        }
        if (auto* p = _DATA->seq_prequeue.push()) *p = {e, c};
    } else if (e.type == STATUS_NOTE_OFF) {
        auto n = _DATA->seq_prequeue._first;
        const float curAT = _DATA->lastPolyAT.load(std::memory_order_relaxed);
        while (n) {
            if (n->data.event.cps == e.cps) {
                if (_DATA->seq_learnmode == 0) {
                    if (auto* p = _DATA->seq_noteon.push()) {
                        p->init(_appState, n->data.event.cps,
                                std::abs(e.time - n->data.event.time),
                                n->data.event.keyvel);
                        p->_atSnapshot = curAT;
                    }
                } else {
                    if (_DATA->seq_noteon.size() < (int)_appState->params[0][SEQ_STEPS].load()) {
                        if (auto* p = _DATA->seq_noteon.push()) {
                            p->init(_appState, n->data.event.cps,
                                    std::abs(e.time - n->data.event.time),
                                    n->data.event.keyvel);
                            p->_atSnapshot = curAT;
                        }
                    } else {
                        if (auto* p = _DATA->seq_noteon.at(n->data.currentnote)) {
                            p->init(_appState, n->data.event.cps,
                                    std::abs(e.time - n->data.event.time),
                                    n->data.event.keyvel);
                            p->_atSnapshot = curAT;
                        }
                    }
                }
                n = _DATA->seq_prequeue.del(n);
            } else n = n->next;
        }
    } else if (e.type == -1) {
        if (_DATA->seq_learnmode == 0) {
            if (auto* p = _DATA->seq_noteon.push()) p->init(_appState, 0, 0, 0);
        } else {
            if (_DATA->seq_noteon.size() < (int)_appState->params[0][SEQ_STEPS].load()) {
                if (auto* p = _DATA->seq_noteon.push()) p->init(_appState, 0, 0, 0);
            } else {
                auto cc = _DATA->seq_currentnote;
                if (_DATA->seq_nextcount && (double)_DATA->seq_count / (double)_DATA->seq_nextcount > .25) {
                    cc -= _DATA->seq_stepforward;
                    if (cc >= _DATA->seq_stepsold)
                        cc = 0;
                    else if (cc < 0)
                        cc = _DATA->seq_stepsold - 1;
                }
                if (auto* p = _DATA->seq_noteon.at(cc)) p->init(_appState, 0, 0, 0);
            }
        }
    }
}

void sequencer::savePr(tsl::AppState* _appState) {
    std::vector<Preset::PresetParam> vals;
    for (int i = 0; i < NUM_PARAMS; i++) {
        float val = _appState->params[0][i].load();
        if (val != _appState->parameters[i].initvalue && !_appState->parameters[i].getFlag(Param::NoAssignment))
            vals.push_back({i, val});
    }
    std::vector<VcoPreNote> notes;
    for (auto* n = _DATA->seq_noteon._first; n; n = n->next)
        notes.push_back(n->data);
    _appState->WorkerQueue.add_task(
        [_appState, cap = std::move(vals), cap2 = std::move(notes)]() { Preset::savePreset(_appState, cap, cap2); });
}

void sequencer::loadPreset(tsl::AppState* _appState, int preset) {
    if (_appState->params[0][NONOTES].load() == 0.0) {
        _DATA->seq_noteon.flush();
        Preset::loadPreset(_appState, preset, _DATA->seq_noteon);
    } else {
        tsl::FastQueue<VcoPreNote> dummy(0);
        Preset::loadPreset(_appState, preset, dummy);
    }
    // This runs on the audio thread out of toAudioThreadQueue, which sequencer::check
    // and sequencer::tick drain AFTER synthFunc took the block's synth_params snapshot.
    // Without retaking it, every voice started in the rest of this block - the next
    // sequencer step, or a key pressed at that moment - is still built from the
    // parameters of the preset we just left.
    refreshSynthParams(_appState);
    _DATA->seq_oldarp = false;
    _DATA->seq_currentnote = 0;
    tsl::graphics::Selector::element_callback_new(
        (tsl::graphics::Selector*)_appState->data->views.presetSelector, preset);
    _appState->WorkerQueue.add_task([_appState]() {
        std::lock_guard<std::recursive_mutex> lk(_STATE->queue_draw);
        if (_appState->data->views.presetSelector->visible_) {
            _appState->parameters[POSTGAIN].view->redraw();
            _appState->parameters[JITTERCENTS].view->redraw();
            _appState->data->views.presetSelector->redraw();
        }
        showToast(_STATE, "Preset Imported");
    });
}

static const double timeDivFactors[] = {1./64, 1./32, 1./16, 1./8, 1./4, 1./2, 1., 2., 4.};

static int calcNextCount(tsl::AppState* _appState) {
    const bool syncDaw = _DATA->isRunningAsPlugin && _appState->params[0][SEQ_SYNCDAW].load() == 1.f;
    const double bpm = syncDaw ? std::max(1.0, _DATA->hostTimeSnapshot.bpm)
                               : _appState->params[0][SEQ_BPM].load();
    const int divIdx = std::clamp((int)_appState->params[0][SEQ_TIMEDIV].load(), 0, 8);
    return (int)(_appState->sr / bpm * 60.0 * timeDivFactors[divIdx]);
}

void sequencer::init(tsl::AppState* _appState) {
    _DATA->seq_pitchfacts.resize((int)_appState->parameters[ARP_STEPS].max + 1);
    _DATA->seq_nextcount = calcNextCount(_appState);
}

static void loadParams(tsl::AppState* _appState) {
    auto swing = _appState->params[0][SEQ_SWING].load();
    _DATA->seq_swingval[0] = 1 - (1 - 1 / 1.5) * swing;
    _DATA->seq_swingval[1] = 1 + swing;
    _DATA->seq_arp = _appState->params[0][ARP_MODE].load() != -1.;
    if (_DATA->seq_arp) {
        _DATA->seq_newarpcycles = (int)_appState->params[0][ARP_STEPS].load() - 1;
        MYFLOAT cur = 1;
        for (int i = 0; i < (int)_appState->parameters[ARP_STEPS].max; i++)
            _DATA->seq_pitchfacts[i] = cur *= pow(2., _appState->params[0][ARPSTEP01 + i].load() / 12.);
        _DATA->seq_newarpcyclemode = (int)_appState->params[0][ARP_MODE].load();
        if (!_DATA->seq_oldarp) {
            _DATA->seq_oldarpcycles = _DATA->seq_newarpcycles;
            _DATA->seq_oldarpcyclemode = _DATA->seq_newarpcyclemode;
            if (_DATA->seq_oldarpcyclemode == 1) {
                _DATA->seq_arpstep = _DATA->seq_oldarpcycles;
                _DATA->seq_arpcycledir = -1;
            } else {
                _DATA->seq_arpstep = 0;
                _DATA->seq_arpcycledir = 1;
            }
        }
    }
    _DATA->seq_oldarp = _DATA->seq_arp;
}

void sequencer::check(tsl::AppState* _appState) {
    if (_appState->params[0][SAVEPRESETBUTTON].exchange(0.0) == 1.0)
        sequencer::savePr(_appState);
    VCOPreEvent e{};
    while (preQueue.pop(e)) push(_appState, e);
    if (_DATA->seq_backw.exchange(false)) {
        _DATA->seq_currentnote = 0;
        _DATA->seq_arpstep = 0;
        _DATA->seq_arpcycledir = 1;
    }
    _DATA->seq_waitforzero = _appState->params[0][WAITFORZERO].load();
    _DATA->seq_learnmode = _appState->params[0][LEARNMODE].load();
    _DATA->seq_notesettingsfromsynth = _appState->params[0][NOTESETTINGSFROMSYNTH].load() == 1.0;

    if (!_DATA->seq_waitforzero) {
        std::function<void()> func;
        if (_DATA->toAudioThreadQueue.pop(func)) func();
        loadParams(_appState);
        _DATA->seq_nextcount = calcNextCount(_appState);
    }
    if (_appState->params[0][ARPRESET].exchange(0.0) == 1.0) {
        _DATA->seq_noteon.flush();
        _DATA->seq_stepsold = 0;
        _DATA->seq_currentnote = 0;
        _DATA->seq_oldarpcyclemode = _DATA->seq_newarpcyclemode;
        _DATA->seq_oldarpcycles = _DATA->seq_newarpcycles;
        if (_DATA->seq_oldarpcyclemode == 1) {
            _DATA->seq_arpstep = _DATA->seq_oldarpcycles;
            _DATA->seq_arpcycledir = -1;
        } else {
            _DATA->seq_arpstep = 0;
            _DATA->seq_arpcycledir = 1;
        }
        _DATA->seq_swingstep = 0;
    }
}

void sequencer::tick(tsl::AppState* _appState, VcoPreNote **note) {
    if (--_DATA->seq_count < 0) {
        if (_DATA->seq_currentnote == 0) {
            _DATA->seq_waitforzero = _appState->params[0][WAITFORZERO].load();
            if ((_DATA->seq_waitforzero && !_DATA->seq_arp) ||
                (_DATA->seq_waitforzero && (_DATA->seq_arpstep == 0 && _DATA->seq_oldarpcyclemode != 1)) ||
                (_DATA->seq_waitforzero && _DATA->seq_oldarpcyclemode == 1 && _DATA->seq_arpstep == _DATA->seq_oldarpcycles)) {
                std::function<void()> func;
                if (_DATA->toAudioThreadQueue.pop(func)) func();
                loadParams(_appState);
            }
            auto s = (int)_appState->params[0][SEQ_STEPS].load();
            while (_DATA->seq_noteon.size() > s)
                _DATA->seq_noteon.pop_front();
            _DATA->seq_stepsold = _DATA->seq_noteon.size();
            _DATA->seq_nextcount = calcNextCount(_appState);
        }
        _DATA->seq_count = _DATA->seq_nextcount;
        if (auto* cur = _DATA->seq_noteon.at(_DATA->seq_currentnote);
            _DATA->seq_currentnote < _DATA->seq_stepsold && cur && cur->_freq > 0) {
            if (_DATA->seq_notesettingsfromsynth) {
                _DATA->seq_dummynote.init(_appState, cur->_freq, cur->_time);
                *note = &_DATA->seq_dummynote;
            } else
                *note = cur;
        }
        _DATA->seq_count *= _DATA->seq_swingval[_DATA->seq_swingstep];
        if (++_DATA->seq_swingstep > 1) _DATA->seq_swingstep = 0;
        if (_DATA->seq_arp) {
            if (_DATA->seq_oldarpcycles == 0) {
                if (_DATA->seq_oldarpcyclemode == 2) {
                    if (_DATA->seq_arpcycledir == -1) {
                        _DATA->seq_oldarpcyclemode = _DATA->seq_newarpcyclemode;
                        _DATA->seq_oldarpcycles = _DATA->seq_newarpcycles;
                        _DATA->seq_stepforward = 1;
                        _DATA->seq_arpcycledir = _DATA->seq_oldarpcyclemode == 1 ? -1 : 1;
                    } else {
                        _DATA->seq_stepforward = 0;
                        _DATA->seq_arpcycledir = -1;
                    }
                } else {
                    _DATA->seq_oldarpcyclemode = _DATA->seq_newarpcyclemode;
                    _DATA->seq_oldarpcycles = _DATA->seq_newarpcycles;
                    _DATA->seq_arpcycledir = _DATA->seq_oldarpcyclemode == 1 ? -1 : 1;
                    _DATA->seq_stepforward = 1;
                }
                _DATA->seq_arpstep = 0;
                if (*note != nullptr)
                    (*note)->_finalfreq = (*note)->_freq * _DATA->seq_pitchfacts[0];
            } else {
                if (*note != nullptr)
                    (*note)->_finalfreq = (*note)->_freq * _DATA->seq_pitchfacts[_DATA->seq_arpstep];
                _DATA->seq_arpstep += _DATA->seq_arpcycledir;
                switch (_DATA->seq_oldarpcyclemode) {
                    case 0:
                        if (_DATA->seq_arpstep > _DATA->seq_oldarpcycles) {
                            _DATA->seq_oldarpcyclemode = _DATA->seq_newarpcyclemode;
                            _DATA->seq_oldarpcycles = _DATA->seq_newarpcycles;
                            if (_DATA->seq_oldarpcyclemode == 1) {
                                _DATA->seq_arpstep = _DATA->seq_oldarpcycles;
                                _DATA->seq_arpcycledir = -1;
                            } else
                                _DATA->seq_arpstep = 0;
                            _DATA->seq_stepforward = 1;
                        } else
                            _DATA->seq_stepforward = 0;
                        break;
                    case 1:
                        if (_DATA->seq_arpstep < 0) {
                            _DATA->seq_oldarpcyclemode = _DATA->seq_newarpcyclemode;
                            _DATA->seq_oldarpcycles = _DATA->seq_newarpcycles;
                            if (_DATA->seq_oldarpcyclemode == 1)
                                _DATA->seq_arpstep = _DATA->seq_oldarpcycles;
                            else {
                                _DATA->seq_arpstep = 0;
                                _DATA->seq_arpcycledir = 1;
                            }
                            _DATA->seq_stepforward = 1;
                        } else
                            _DATA->seq_stepforward = 0;
                        break;
                    case 2:
                        if (_DATA->seq_arpstep > _DATA->seq_oldarpcycles) {
                            _DATA->seq_arpstep = _DATA->seq_oldarpcycles;
                            _DATA->seq_arpcycledir = -1;
                            _DATA->seq_stepforward = 0;
                        } else if (_DATA->seq_arpstep < 0) {
                            _DATA->seq_stepforward = 1;
                            _DATA->seq_oldarpcyclemode = _DATA->seq_newarpcyclemode;
                            _DATA->seq_oldarpcycles = _DATA->seq_newarpcycles;
                            if (_DATA->seq_oldarpcyclemode == 1)
                                _DATA->seq_arpstep = _DATA->seq_oldarpcycles;
                            else {
                                _DATA->seq_arpcycledir = 1;
                                _DATA->seq_arpstep = 0;
                            }
                        } else
                            _DATA->seq_stepforward = 0;
                        break;
                    default:
                        if (_DATA->seq_arpstep >= _DATA->seq_oldarpcycles) {
                            _DATA->seq_arpcycledir = -1;
                            _DATA->seq_stepforward = 0;
                        } else if (_DATA->seq_arpstep == 0 && _DATA->seq_arpcycledir == -1) {
                            _DATA->seq_stepforward = 1;
                            _DATA->seq_oldarpcyclemode = _DATA->seq_newarpcyclemode;
                            _DATA->seq_oldarpcycles = _DATA->seq_newarpcycles;
                            if (_DATA->seq_oldarpcyclemode == 1)
                                _DATA->seq_arpstep = _DATA->seq_oldarpcycles;
                            else
                                _DATA->seq_arpcycledir = 1;
                        } else
                            _DATA->seq_stepforward = 0;
                        break;
                }
            }
        } else
            _DATA->seq_stepforward = 1;
        _DATA->seq_currentnote += _DATA->seq_stepforward;
        if (_DATA->seq_currentnote >= _DATA->seq_stepsold)
            _DATA->seq_currentnote = 0;
        else if (_DATA->seq_currentnote < 0)
            _DATA->seq_currentnote = _DATA->seq_stepsold - 1;
        _appState->params[0][CURRENTNOTE].store(_DATA->seq_currentnote);
        _appState->params[0][ARP_STEP].store(_DATA->seq_arpstep);
    }
}
