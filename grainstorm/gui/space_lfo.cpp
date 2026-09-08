#include "gui_internal.h"


	void initLFOSync(tsl::AppState * _appState, Layout * root) {


		root->addChild(new View(_appState, 1.25f, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN));
		root->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));

		auto center = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
		root->addChild(center);
		//center->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
		auto transport = new CheckBoxWrapped(_appState, CENTER_ALIGN, LFO1SYNCDAWTRANSPORT, ASLFO, 2);

		auto dummysyncdaw = new HorizontalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN);
		center->addChild(dummysyncdaw);
		dummysyncdaw->paddingtop = dummysyncdaw->paddingbottom = 10.f;
		dummysyncdaw->paddingleft = dummysyncdaw->paddingright = 5.f;
		dummysyncdaw->addChild(transport);

		center->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

		auto dummy3 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, center);
		dummy3->size_reference = &_STATE->knob_height_total;


		auto seltiming = new Knob(_appState, WRAP, 0, CENTER_ALIGN, LFO1SYNCDAWTIMING, ASLFO, 2);
		dummy3->addChild(seltiming);

	}


void initLFOEditor(tsl::AppState * _appState, Layout * root) {
		root->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));


		auto enveditor = new EnvEditor<LfoEnvEditor>(_appState, 2.f, RATIO_FROM_PARENT_View,
			START_ALIGN);
		root->addChild(enveditor);

		root->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

		auto test = new ButtonView<EnvelopeButton>(_appState, 6.6666, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN,
			HORIZONTAL, 3, makeStringSpan(editorcurvenames),
			emptyFloats, LFO1EDITFUNC, ASLFO, LFONUMPARAMS);
		test->paddingleft = test->paddingright = 5.f;
		test->paddingtop = test->paddingbottom = 10.f;
		root->addChild(test);

		root->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

		//  space_grainenv2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));


		auto dummycontrolsenveditor = new VerticalLayout(_appState, VALUE_FROM_POINTER,
			VALUE_FROM_POINTER,
			START_ALIGN, root);
		dummycontrolsenveditor->size_reference = &_STATE->knob_height_total;
		dummycontrolsenveditor->paddingbottom = 10.f;
		auto enveditor_nsegments = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN,
			LFO1NSEGS);
		//p->miditargets[GRAINNSEGS].view = enveditor_nsegments;
		dummycontrolsenveditor->addChild(enveditor_nsegments);

		auto dummycontrolsenveditor2 = new HorizontalLayout(_appState, HORIZONTAL, WRAP, CENTER_ALIGN);
		dummycontrolsenveditor->addChild(dummycontrolsenveditor2);

		auto dummycontrolsenveditor3 = new HorizontalLayout(_appState, WRAP, 0,
			START_ALIGN);
		dummycontrolsenveditor2->addChild(dummycontrolsenveditor3);

		auto je = new CheckBox(_appState, VERTICAL, START_ALIGN, LFO1JOIN, ASLFO, LFONUMPARAMS);
		dummycontrolsenveditor3->addChild(je);
		je->paddingbottom = 10.f;

		/*
		auto selector_envedit = new Selector1<WaveformChooser>(_appState, 5.f,
															   PERCENTAGE_FROM_MAIN_WINDOW,
															   END_ALIGN, ARRAY_LEN(editfuncs),
															   editfuncs, nullptr, GRAINCURVE, 0, 1,
															   callback_editfunc);
		dummycontrolsenveditor2->addChild(test);
	*/
		auto envedit_quant = new PlusMinusControlQuant(_appState, WRAP, 0, CENTER_ALIGN, LFO1QUANT);
		dummycontrolsenveditor->addChild(envedit_quant);

	}


void initLFORand(tsl::AppState * _appState, Layout * root) {
		root->addChild(new View(_appState, 1.25f, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN));

		//root->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));
		auto center = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
		root->addChild(center);

		auto dummy2 = new VerticalLayout(_appState, VALUE_FROM_POINTER,
			VALUE_FROM_POINTER, START_ALIGN, center);
		dummy2->size_reference = &_STATE->knob_height_total;

		auto lfo1freq = new Knob(_appState, WRAP, 0, START_ALIGN, LFO1CPSMIN, ASLFO, 1, dummy2);
		auto lfo2freq = new Knob(_appState, WRAP, 0, START_ALIGN, LFO1CPSMAX, ASLFO, 1, dummy2);
		auto lfodpth = new Knob(_appState, WRAP, 0, START_ALIGN, LFO1RNDDEPTH, ASLFO, 1, dummy2);

		center->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

		auto dummy3 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN,
			center);
		dummy3->size_reference = &_STATE->textsize2;
		dummy3->size_reference_scale = 3.f;

		center->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));


		auto selcurve = new Selector2(_appState, WRAP, 0, CENTER_ALIGN, ARRAY_LEN(lforandcurve),
			makeStringSpan(lforandcurve),
			emptyFloats, LFO1RNDCURVE, ASLFO, 1, "CURVE");
		dummy3->addChild(selcurve);
		selcurve->paddingleft = selcurve->paddingright = 10.f;
		
		auto seldistr = new Selector2(_appState, WRAP, 0, CENTER_ALIGN, ARRAY_LEN(lforanddistr),
				makeStringSpan(lforanddistr),
				emptyFloats, LFO1RNDTYPE, ASLFO, 1, "DISTR");
		dummy3->addChild(seldistr);
		seldistr->paddingleft = seldistr->paddingright = 10.f;
	
		auto dummybeta0 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN, center);
		auto vis = new BetaPDFView(_appState, WRAP, 0, START_ALIGN, LFO1RNDALPHA, LFO1RNDBETA, ASLFO);
		dummybeta0->addChild(vis);
		vis->paddingleft = vis->paddingright = 5.f;
		vis->paddingtop = 5.f;
		auto s2 = new SliderVert(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, END_ALIGN,
			LFO1RNDBETA, ASLFO);
		s2->size_reference = &_STATE->textsize2;
		s2->size_reference_scale = 4.f;
		dummybeta0->addChild(s2);
		s2->paddingleft = s2->paddingright = 10.f;
		s2->paddingtop = /* s2->paddingbottom = */ 5.f;
		auto s1 = new SliderVert(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, END_ALIGN,
			LFO1RNDALPHA,
			ASLFO);
		s1->size_reference = &_STATE->textsize2;
		s1->size_reference_scale = 4.f;
		dummybeta0->addChild(s1);
		s1->paddingtop = /*s1->paddingbottom = */5.f;
		s1->paddingleft = s1->paddingright = 10.f;
	}


void init_space_lfo(tsl::AppState * _appState, Layout * root) {
		auto space_lfo = new SpaceLFO(_appState, WRAP, 0, CENTER_ALIGN, root);
		_DATA->views.active_spaces_array[SPACE_LFOS] = space_lfo;

		auto distancelfotop = new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN);
		space_lfo->addChild(distancelfotop);

		auto distancelfobottom = new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
			END_ALIGN);
		space_lfo->addChild(distancelfobottom);


		auto dummy_title_lfo = new VerticalLayout(_appState, 15., RATIO_FROM_MAIN_WINDOW, START_ALIGN);
		space_lfo->addChild(dummy_title_lfo);
		_DATA->views.lfo_title = dummy_title_lfo;
		//dummy_title_lfo->paddingleft = dummy_title_lfo->paddingright = 2.5;

		auto selector_lfo = new Selector1<TitleView>(_appState, WRAP, 0, START_ALIGN,
			ARRAY_LEN(lfonames),
			makeStringSpan(lfonames), emptyFloats,
			ASLFO);

		selector_lfo->paddingtop = selector_lfo->paddingbottom = 10.f;
		selector_lfo->paddingleft = selector_lfo->paddingright = 2.5f;
		dummy_title_lfo->addChild(selector_lfo);

#if defined(OS_IOS) && !defined(PLUGIN_MODE)
		const int lfospaceCount = _DATA->isRunningAsPlugin ? ARRAY_LEN(lfospaces) : ARRAY_LEN(lfospaces) - 1;
#else
		const int lfospaceCount = ARRAY_LEN(lfospaces);
#endif
		auto selector_lfo2 = new Selector1<TitleView>(_appState, WRAP, 0, START_ALIGN,
			lfospaceCount,
			makeStringSpan(lfospaces), emptyFloats,
			LFO1SPACE);

		dummy_title_lfo->addChild(selector_lfo2);
		selector_lfo2->paddingtop = selector_lfo2->paddingbottom = 10.f;


		auto lfooffbutton = new NormalButton(_appState, SYM, 0, END_ALIGN, ICON_MD_POWER_SETTINGS_NEW,
			ICON_MD_POWER_SETTINGS_NEW,
			LFO1POWER);
		dummy_title_lfo->addChild(lfooffbutton);
		_STATE->parameters[LFO3POWER].view = _STATE->parameters[LFO2POWER].view = _STATE->parameters[LFO1POWER].view = lfooffbutton;
		lfooffbutton->padding = 10.f;

		/*
			auto pLfoEditorButton = new LFOEditorButton(_appState, SYM, 0, END_ALIGN);
			pLfoEditorButton->padding = 5.f;
			dummy_title_lfo->addChild(pLfoEditorButton);
			_DATA->views.lfo_editbutton = pLfoEditorButton;
		*/
		auto lfo_edit_root = new HorizontalLayout(_appState, WRAP,
			0, CENTER_ALIGN, space_lfo, true);
		_DATA->views.lfo_edit_root = lfo_edit_root;
		initLFOEditor(_STATE, lfo_edit_root);

		auto lfo_rand_root = new HorizontalLayout(_appState, WRAP,
			0, CENTER_ALIGN, space_lfo, true);
		_DATA->views.lfo_rand_root = lfo_rand_root;
		initLFORand(_STATE, lfo_rand_root);

		if (_DATA->isRunningAsPlugin) {
			auto lfo_sync = new HorizontalLayout(_appState, WRAP,
				0, CENTER_ALIGN, space_lfo, true);
			_DATA->views.lfo_sync_root = lfo_sync;
			initLFOSync(_STATE, lfo_sync);
		}


		auto space_lfo1 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_lfo, true);
		_DATA->views.space_lfo1 = space_lfo1;
		space_lfo1->paddingleft = space_lfo1->paddingright = 2.5f;
		//button_space_lfo1->userdata = space_lfo1;
		//space_lfo1->userdata = button_space_lfo1;


		auto lfocontrolpanel2 = new VerticalLayout(_appState, 15., RATIO_FROM_MAIN_WINDOW, START_ALIGN);
		space_lfo1->addChild(lfocontrolpanel2);

		auto lfoinput = new NormalButton(_appState, SYM, 0, START_ALIGN, ICON_MD_INPUT, ICON_MD_INPUT,
			LFO1CONTROLSACTIVE, ASLFO, LFONUMPARAMS);
		lfocontrolpanel2->addChild(lfoinput);
		lfoinput->padding = 20.f;
		if (_STATE->dofastrender) {
			auto lfosyncmidi = new SyncMidiButton(_appState, SYM, 0,
				START_ALIGN);
			lfosyncmidi->padding = 17.f;
			lfocontrolpanel2->addChild(lfosyncmidi);
		}

		auto lfo_factor = new Selector1<TitleView>(_appState, 6.75, VIEW_COMPUTESIZE, CENTER_ALIGN,
			4, makeStringSpan(syncfactornames),
			makeFloatSpan(syncfactors),
			LFO1SYNCFACT, ASLFO, LFONUMPARAMS);
		lfocontrolpanel2->addChild(lfo_factor);
		lfo_factor->padding = 10.f;

		auto lfosyncbutton = new NormalButton(_appState, SYM, 0, END_ALIGN, ICON_MD_SYNC,
			ICON_MD_SYNC, LFO1SYNC, ASLFO);
		lfosyncbutton->padding = 10.f;
		lfocontrolpanel2->addChild(lfosyncbutton);

		auto dummy_buttons_lfo = new VerticalLayout(_appState, 6., PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN);
		space_lfo1->addChild(dummy_buttons_lfo);
		_DATA->views.lfocontrolpanel = dummy_buttons_lfo;

		auto lfostepback = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_SKIP_PREVIOUS,
			ICON_MD_SKIP_PREVIOUS, LFO1BACKW, ASLFO);
		lfostepback->padding = 10.f;
		dummy_buttons_lfo->addChild(lfostepback);

		auto stop = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_STOP,
			ICON_MD_STOP, LFO1STOP, ASLFO);
		stop->padding = 10.f;
		dummy_buttons_lfo->addChild(stop);

		auto start = new BypassOffButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_PLAY_ARROW,
			ICON_MD_PLAY_ARROW, LFO1PLAY, ASLFO);
		start->padding = 10.f;
		dummy_buttons_lfo->addChild(start);

		auto lfostepforward = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_SKIP_NEXT,
			ICON_MD_SKIP_NEXT, LFO1FORW, ASLFO);
		lfostepforward->padding = 10.f;
		dummy_buttons_lfo->addChild(lfostepforward);

		auto lforevert = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_SWAP_HORIZ,
			ICON_MD_SWAP_HORIZ, LFO1DIR, ASLFO, LFONUMPARAMS);
		lforevert->padding = 10.f;
		lforevert->C_active = skcol::fg;
		dummy_buttons_lfo->addChild(lforevert);

		auto lfodivtwo = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_SLOW_MOTION_VIDEO,
			ICON_MD_SLOW_MOTION_VIDEO, LFO1SLOW, ASLFO);
		lfodivtwo->padding = 10.f;
		dummy_buttons_lfo->addChild(lfodivtwo);

		auto lfomultitwo = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN,
			ICON_MD_PLAY_CIRCLE_OUTLINE,
			ICON_MD_PLAY_CIRCLE_OUTLINE, LFO1FAST, ASLFO);
		lfomultitwo->padding = 10.f;
		dummy_buttons_lfo->addChild(lfomultitwo);
		space_lfo1->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_PARENT_View, START_ALIGN));
		auto dummy6000 = new VerticalLayout(_appState, 15., RATIO_FROM_MAIN_WINDOW, START_ALIGN,
			space_lfo1);
		space_lfo1->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_PARENT_View, START_ALIGN));
		auto lfodest = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
			_STATE->dofastrender ? ARRAY_LEN(lfoItems) : ARRAY_LEN(
				lfoItems2),
			_STATE->dofastrender ? makeStringSpan(lfoItems)
			: makeStringSpan(lfoItems2),
			_STATE->dofastrender ? makeFloatSpan(lfoValues)
			: makeFloatSpan(lfoValues2),
			LFO1DEST);
		lfodest->middleclick = true;
		dummy6000->addChild(lfodest);
		lfodest->rv->isActiveFunc = [lfodest, _appState](int32_t index) {
			auto track = _DATA->tracks[_STATE->active_track.load()];
			auto tindex = track->index;
			return track->lfo[(int)lfodest->values[index]].load() == track->lfos[GASLFO];
			};
		lfodest->paddingtop = lfodest->paddingbottom = 10.f;
		lfodest->paddingleft = lfodest->paddingright = 1.25f;
		auto lfooffbutton2 = new NormalButton(_appState, SYM, 0, END_ALIGN, ICON_MD_POWER_SETTINGS_NEW,
			ICON_MD_POWER_SETTINGS_NEW,
			LFODESTPOWER);
		lfooffbutton2->padding = 10.f;
		dummy6000->addChild(lfooffbutton2);


		auto dummy1lfo = new VerticalLayout(_appState, VALUE_FROM_POINTER,
			VALUE_FROM_POINTER, START_ALIGN, space_lfo1);
		dummy1lfo->size_reference = &_STATE->knob_height_total;
		space_lfo1->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_PARENT_View, START_ALIGN));
		auto lfo1freq = new Knob(_appState, WRAP, 0, START_ALIGN, LFO1CPS, ASLFO, LFONUMPARAMS,
			dummy1lfo);

		auto lfo1bounda = new Knob(_appState, WRAP, 0,
			START_ALIGN, LFO1BOUNDA, 0, 0,
			dummy1lfo);

		auto lfo1boundb = new Knob(_appState, WRAP, 0,
			START_ALIGN, LFO1BOUNDB, 0, 0,
			dummy1lfo);

		auto dummyenvwindow = new VerticalLayout(_appState, WRAP, 0,
			CENTER_ALIGN);
		space_lfo1->addChild(dummyenvwindow);
		dummyenvwindow->paddingleft = dummyenvwindow->paddingright = 5.f;
		dummyenvwindow->paddingtop = 10.f;

		auto env_window_lfo1 = new LFOWindow(_appState, WRAP,
			0,
			START_ALIGN);
		//env_window_lfo1->paddingtop = 10.f;
		// env_window_lfo1->paddingbottom = 10.f;
		env_window_lfo1->paddingright = 2.5f;
		dummyenvwindow->addChild(env_window_lfo1);
		_DATA->views.env_win_lfo = env_window_lfo1;

		if (_STATE->dofastrender.load()) {
			auto lfomidiclockmulti = new PlusMinusControl(_appState, 3., RATIO_FROM_PARENT_View,
				END_ALIGN, LFO1CLOCKMULTI, ASLFO,
				LFONUMPARAMS,
				"MIDI x");
			dummyenvwindow->addChild(lfomidiclockmulti);
			lfomidiclockmulti->paddingtop = 5.f;
		}

		auto lfo1env = new ButtonView<EnvelopeButton>(_appState, 15.f, RATIO_FROM_MAIN_WINDOW,
			START_ALIGN,
			HORIZONTAL,
			ARRAY_LEN(lfo_envelopes_names),
			makeStringSpan(lfo_envelopes_names), emptyFloats,
			LFO1CURVE,
			ASLFO, LFONUMPARAMS);
		lfo1env->paddingtop = lfo1env->paddingbottom = 10.f;
		lfo1env->paddingleft = 2.5f;
		lfo1env->paddingright = 2.5f;
		space_lfo1->addChild(lfo1env);
		_STATE->parameters[LFO2CURVE].view = _STATE->parameters[LFO2CURVE].view = lfo1env;
	}
