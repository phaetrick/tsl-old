#include "gui_internal.h"


void init_space_waveform(tsl::AppState * _appState, Layout * root) {
		auto space_waveform = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, root, true);
		_DATA->views.active_spaces_array[SPACE_WAVEFORM] = space_waveform;


		auto placeholder_waveform = new Waveform2(_appState, WRAP, 0, START_ALIGN);
		space_waveform->addChild(placeholder_waveform);


		auto divider_waveform = new Divider(_appState, CENTER_ALIGN);
		space_waveform->addChild(divider_waveform);

		auto dummy_waveform = new HorizontalLayout(_appState, WRAP, 0, END_ALIGN);
		space_waveform->addChild(dummy_waveform);


		auto placeholderzz = new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN);
		dummy_waveform->addChild(placeholderzz);

		auto cpanwaveform = new VerticalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN);
		dummy_waveform->addChild(cpanwaveform);

		auto placeholderzy = new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
			END_ALIGN);
		dummy_waveform->addChild(placeholderzy);


		auto topspacing = new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN);
		dummy_waveform->addChild(topspacing);


#if defined PLUGIN_MODE
#ifdef OS_IOS
	auto ejectbutton = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_EJECT,
				ICON_MD_EJECT,
				EJECTBUTTON);
	cpanwaveform->addChild(ejectbutton);

#endif

		auto micbutton = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_MIC,
			ICON_MD_MIC, MICROPHONEButton);
		micbutton->padding = 10.f;
		cpanwaveform->addChild(micbutton);
		auto recloop = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN,
			ICON_MD_FIBER_MANUAL_RECORD, ICON_MD_FIBER_MANUAL_RECORD,
			RECLOOPButton);
		recloop->padding = 10.f;
		recloop->C_active = skcol::red;
		cpanwaveform->addChild(recloop);

#elif defined STANDALONE_MODE
#ifdef OS_IOS
	auto ejectbutton = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_EJECT,
				ICON_MD_EJECT,
				EJECTBUTTON);
	cpanwaveform->addChild(ejectbutton);

#endif

		auto micbutton = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_MIC,
			ICON_MD_MIC, MICROPHONEButton);
		micbutton->padding = 10.f;
		cpanwaveform->addChild(micbutton);
		if (_STATE->dofastrender) {
			auto recloop = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN,
				ICON_MD_FIBER_MANUAL_RECORD, ICON_MD_FIBER_MANUAL_RECORD,
				RECLOOPButton);
			recloop->padding = 10.f;
			recloop->C_active = skcol::red;
			cpanwaveform->addChild(recloop);
		}

#else


		auto ejectbutton = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_EJECT,
			ICON_MD_EJECT,
			EJECTBUTTON);
		ejectbutton->padding = 10.f;
		cpanwaveform->addChild(ejectbutton);

		if (_DATA->hasmic) {
			auto micbutton = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_MIC,
				ICON_MD_MIC, MICROPHONEButton);
			micbutton->padding = 10.f;
			cpanwaveform->addChild(micbutton);
		}


		if (_STATE->dofastrender) {
			auto recloop = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN,
				ICON_MD_FIBER_MANUAL_RECORD, ICON_MD_FIBER_MANUAL_RECORD,
				RECLOOPButton);
			recloop->padding = 10.f;
			recloop->C_active = skcol::red;
			cpanwaveform->addChild(recloop);
		}

#endif
		auto grainamp = new Knob(_appState, WRAP, 0, CENTER_ALIGN, PREGAIN);
		grainamp->paddingtop = grainamp->paddingbottom = 5.f;
		dummy_waveform->addChild(grainamp);

		auto post_gain = new Knob(_appState, WRAP, 0, CENTER_ALIGN, POSTGAIN);
		post_gain->paddingtop = post_gain->paddingbottom = 5.f;
		dummy_waveform->addChild(post_gain);

		auto speed = new Knob(_appState, WRAP, 0, CENTER_ALIGN, SPEED);
		speed->paddingtop = speed->paddingbottom = 5.f;
		dummy_waveform->addChild(speed);
	}
