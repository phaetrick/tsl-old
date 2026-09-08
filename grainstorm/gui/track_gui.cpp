#include "gui_internal.h"


void track_gui_init(tsl::AppState * _appState, Layout * root) {

		auto toppanel = new VerticalLayout(_appState, 15., RATIO_FROM_MAIN_WINDOW, START_ALIGN);
		root->addChild(toppanel);
		_DATA->views.toppanel_track = toppanel;


		auto trackinput = new NormalButton(_appState, SYM, 0, START_ALIGN, ICON_MD_INPUT, ICON_MD_INPUT,
			TRACK_CONTROLS_ACTIVE);
		toppanel->addChild(trackinput);
		trackinput->padding = 20.f;

		auto track_factor = new Selector1<TitleView>(_appState, 6.75, VIEW_COMPUTESIZE, START_ALIGN,
			ARRAY_LEN(syncfactornames),
			makeStringSpan(syncfactornames),
			makeFloatSpan(syncfactors),
			TRACK_SYNC_FACTOR);
		toppanel->addChild(track_factor);
		track_factor->padding = 10.f;

		auto offbutton = new BypassOffButton(_appState, SYM, 0, END_ALIGN,
			static_cast<const char*>(ICON_MD_POWER_SETTINGS_NEW),
			static_cast<const char*>(ICON_MD_POWER_SETTINGS_NEW),
			POWERTRACK);
		offbutton->padding = 10.f;
		toppanel->addChild(offbutton);

		auto menubutton = new NormalButton(_appState, SYM, 0, END_ALIGN, ICON_MD_MORE_VERT,
			ICON_MD_MORE_VERT,
			MENUBUTTON);
		menubutton->padding = 10.f;
		toppanel->addChild(menubutton);

		auto redo = new NormalButton(_appState, SYM, 0, END_ALIGN, ICON_MD_REDO,
			ICON_MD_REDO,
			REDOBUTTON);
		redo->padding = 10.f;
		toppanel->addChild(redo);
		auto undo = new NormalButton(_appState, SYM, 0, END_ALIGN, ICON_MD_UNDO,
			ICON_MD_UNDO,
			UNDOBUTTON);
		undo->padding = 10.f;
		toppanel->addChild(undo);


		auto cpan = new VerticalLayout(_appState, 15., RATIO_FROM_MAIN_WINDOW, START_ALIGN);
		root->addChild(cpan);
		_DATA->views.controlpanel = cpan;

		auto divider_cpan_bottom = new Divider(_appState, START_ALIGN);
		root->addChild(divider_cpan_bottom);
		_DATA->views.divider_cpan_bottom = divider_cpan_bottom;


		auto stepback = new NormalButton(_appState, WRAP, 0, START_ALIGN,
			ICON_MD_SKIP_PREVIOUS, ICON_MD_SKIP_PREVIOUS, STEPBACK);
		stepback->padding = 10.f;
		cpan->addChild(stepback);

		auto stop = new NormalButton(_appState, WRAP, 0, START_ALIGN,
			ICON_MD_STOP, ICON_MD_STOP, STOPButton);

		stop->padding = 10.f;
		cpan->addChild(stop);

		auto start = new BypassOffButton(_appState, WRAP, 0, START_ALIGN,
			ICON_MD_PLAY_ARROW, ICON_MD_PLAY_ARROW, PLAYButton);

		start->padding = 10.f;
		cpan->addChild(start);

		auto stepforward = new NormalButton(_appState, WRAP, 0, START_ALIGN,
			ICON_MD_SKIP_NEXT, ICON_MD_SKIP_NEXT, STEPFORW);
		stepforward->padding = 10.f;
		cpan->addChild(stepforward);


		auto revert = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_SWAP_HORIZ,
			ICON_MD_SWAP_HORIZ, DIRButton);
		revert->padding = 10.f;
		cpan->addChild(revert);

		auto divtwo = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_SLOW_MOTION_VIDEO,
			ICON_MD_SLOW_MOTION_VIDEO, SLOWButton);
		divtwo->padding = 10.f;
		cpan->addChild(divtwo);

		auto multitwo = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_PLAY_CIRCLE_OUTLINE,
			ICON_MD_PLAY_CIRCLE_OUTLINE, FASTButton);
		multitwo->padding = 10.f;
		cpan->addChild(multitwo);

		auto syncbutton = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_SYNC,
			ICON_MD_SYNC, SYNCButton);
		syncbutton->padding = 10.f;
		cpan->addChild(syncbutton);


		auto trackwindow = new VerticalLayout(_appState, WRAP, 0, START_ALIGN);
		root->addChild(trackwindow);

		auto sidebardummy = new HorizontalLayout(_appState, VALUE_FROM_POINTER,
			VALUE_FROM_POINTER, START_ALIGN);
		trackwindow->addChild(sidebardummy);
		sidebardummy->size_reference = &_STATE->textsize1;
		//sidebardummy->size_reference = &_DATA->track1.button->width;
		//sidebardummy->size_reference_scale = .7f;

		auto midilearnbutton = new MidilearnButton(_appState, SYM, 0, END_ALIGN);
		_DATA->views.midilearnbutton = midilearnbutton;
		sidebardummy->addChild(midilearnbutton);


		auto sidebar = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN);
		sidebardummy->addChild(sidebar);
		_DATA->views.sidebar = sidebar;

		auto divider_left2 = new Divider(_appState, START_ALIGN);
		trackwindow->addChild(divider_left2);
		_DATA->views.divider_left2 = divider_left2;

		init_space_waveform(_STATE, trackwindow);
		init_space_fx_grain(_STATE, trackwindow);
		init_space_windows(_STATE, trackwindow);
		init_space_lfo(_STATE, trackwindow);
		init_space_env_follower(_STATE, trackwindow);
		init_space_fx(_STATE, trackwindow);
		init_space_fx_stereo(_STATE, trackwindow);

		auto button_space_waveform = new ButtonSpaceSwitch(_appState, WRAP, 0, START_ALIGN,
			SPACE_WAVEFORM);
		sidebar->addChild(button_space_waveform);

		auto bgran = new ButtonSpaceSwitch(_appState, WRAP, 0, START_ALIGN, SPACE_GRANULATION);
		sidebar->addChild(bgran);

		auto benv = new ButtonSpaceSwitch(_appState, WRAP, 0, START_ALIGN, SPACE_ENVELOPES);
		sidebar->addChild(benv);

		auto blfo = new ButtonSpaceSwitch(_appState, WRAP, 0, START_ALIGN, SPACE_LFOS);
		sidebar->addChild(blfo);

		auto bfol = new ButtonSpaceSwitch(_appState, WRAP, 0, START_ALIGN, SPACE_ENVFOLLOWER);
		sidebar->addChild(bfol);

		auto bfx = new ButtonSpaceSwitch(_appState, WRAP, 0, START_ALIGN, SPACE_FX);
		sidebar->addChild(bfx);

		auto bstfx = new ButtonSpaceSwitch(_appState, WRAP, 0, START_ALIGN, SPACE_REVERB);
		sidebar->addChild(bstfx);
	}
