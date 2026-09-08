#pragma once

/* Shared prologue for the split gui/ translation units.
   `gui.cpp` used to be one ~5300-line file; each space initialiser now lives in
   its own gui/space_*.cpp and includes this header for the common includes and
   for the handful of helpers more than one of them needs. Private to gui/ -
   nothing outside the folder should include it. */

#include "gui.h"
#include "view.h"
#include "logger.h"
#include "defines.h"
#include "grainstorm.h"
#include "waveform.h"
#include "meter.h"
#include "infopanel.h"
#include "button.h"
#include "knob.h"
#include "buttonview.h"
#include "tools.h"
#include "textview.h"
#include "plusminuscontrol.h"
#include "selector.h"
#include "track.h"
#include "colours.h"
#include "compressor.h"
#include "slider.h"
#include <include/core/SkFontMetrics.h>
#include "frequencyresponce.h"
#include "eq.h"
#include "DynEqView.h"
#include "checkbox.h"
#include "envelope_window.h"
#include "lfo.h"
#include "pv.h"
#include "envwin2.h"
#include "scrollview.h"
#include "DecoderView.h"
#include "Layouts.h"

using namespace tsl::graphics;

/* House side padding for horizontal sliders, taken from PING PONG DELAY. It is
   not just a look: at 5 a slider's bar starts at the same x as a half-width
   TextButton with `padding = 10` (both land on 61px of a 410px row), which is
   why FREEZE lines up with the sliders under it there - and why REVERSE lines
   up here. Vertical padding is the matching 20 (see SDROWPAD). */
constexpr float SDSIDEPAD = 5.f;

/* A knob row at the height a knob is drawn at everywhere else in the plugin.
   knob_height_total is 18.769% of the window (root init()); the mono FX panels
   land on it only because their spaces happen to divide that way - measured,
   DISTORT's knobs are 151px at an 800px window. PV and CROSS sub-views are
   smaller (336px against a 502px mono panel) and were dividing up whatever
   they had, so their knobs drifted off that height. Asking for the constant
   makes every knob in both spaces exactly the reference size, whatever the
   sub-view holds. No vertical padding here - the row IS the knob.
   FOUR KNOBS PER ROW IS THE LIMIT: the circle is
   `radius = (MIN(surface.width, surface.height) - lw) * .5` and the knob area
   is ~92px tall, so a 409px row split five ways (82px) starts shrinking the
   circle by width. A sub-view needing more than a row's worth uses sliders. */
inline tsl::graphics::VerticalLayout* knobRow(tsl::AppState* _appState,
	tsl::graphics::Layout* par) {
	auto* r = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN, par);
	r->size_reference = &_STATE->knob_height_total;
	return r;
}


/* Space initialisers - one per gui/space_*.cpp, all called from
   track_gui_init() / init_space_fx_grain() in this folder. */
void init_space_env_follower(tsl::AppState* _appState, Layout* root);
void init_space_fx(tsl::AppState* _appState, Layout* root);
void init_space_fx_stereo(tsl::AppState* _appState, Layout* root);
void init_space_pv(tsl::AppState* _appState, Layout* root);
void init_space_cross(tsl::AppState* _appState, Layout* root);
void init_space_graingen(tsl::AppState* _appState, Layout* root);
void init_space_arp(Layout* root);
void init_space_fx_grain(tsl::AppState* _appState, Layout* root);
void init_space_windows(tsl::AppState* _appState, Layout* root);
void init_space_lfo(tsl::AppState* _appState, Layout* root);
void init_space_waveform(tsl::AppState* _appState, Layout* root);
void track_gui_init(tsl::AppState* _appState, Layout* root);
