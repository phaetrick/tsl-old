#include "gui_internal.h"


inline constexpr std::string_view modnames[] = { "MODULATOR: TRACK2", "MODULATOR: TRACK3",
												"MODULATOR: TRACK4",
												"MODULATOR: TRACK1" };

void init_space_fx_grain(tsl::AppState* _appState, Layout* root) {
	Knob* Knob1;
	auto space_granulation = new SpaceGranulation(_appState, WRAP, 0, CENTER_ALIGN, root);
	_DATA->views.active_spaces_array[SPACE_GRANULATION] = space_granulation;

	auto ph0 = new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_granulation->addChild(ph0);


	auto dummytitlespace_granulation = new VerticalLayout(_appState, 6.75,
		PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_granulation->addChild(dummytitlespace_granulation);

	auto grainfxtype = new Selector1<TitleView>(_appState, WRAP, 0, START_ALIGN,
		_STATE->dofastrender ? ARRAY_LEN(grainmods2)
		: ARRAY_LEN(
			grainmods1),
		_STATE->dofastrender ? makeStringSpan(grainmods2)
		: makeStringSpan(grainmods1),
		_STATE->dofastrender ? makeFloatSpan(grainmod2values)
		: makeFloatSpan(
			grainmod1values),
		ASGRAN,
		0, 1);

	grainfxtype->paddingtop = 10.f;
	grainfxtype->paddingbottom = 10.f;
	grainfxtype->paddingleft = 2.5;
	grainfxtype->titletext = "GRAIN EFFECTS";
	grainfxtype->middleclick = true;
	dummytitlespace_granulation->addChild(grainfxtype);

	auto offbuttongrain = new BypassOffButton(_appState, SYM, 0, END_ALIGN,
		static_cast<const char*>(ICON_MD_POWER_SETTINGS_NEW),
		static_cast<const char*>(ICON_MD_POWER_SETTINGS_NEW),
		OFFGRAIN);
	offbuttongrain->padding = 10.f;
	dummytitlespace_granulation->addChild(offbuttongrain);

	auto bypassbuttongrain = new BypassOffButton(_appState, SYM, 0, END_ALIGN,
		static_cast<const char*>(ICON_MD_RADIO_BUTTON_UNCHECKED),
		static_cast<const char*>(ICON_MD_RADIO_BUTTON_CHECKED),
		BYPASSGRAINFX);
	bypassbuttongrain->padding = 10.f;
	dummytitlespace_granulation->addChild(bypassbuttongrain);

	auto space_graingain = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINGAIN] = space_graingain;

	auto divgraingain = new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_graingain->addChild(divgraingain);

	auto phgraingain = new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN);
	space_graingain->addChild(phgraingain);


	auto space_gg_center = new HorizontalLayout(_appState, WRAP, 0,
		CENTER_ALIGN, space_graingain);
	auto dummy1_graingain = new VerticalLayout(_appState, RATIO_FROM_PARENT_View, 3., START_ALIGN,
		space_gg_center);
	dummy1_graingain->paddingtop = dummy1_graingain->paddingbottom = 5.f;

	auto graingainin = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAININPUT_GAIN_DAW, 0, 1,
		dummy1_graingain);
	auto graingainsapl = new Knob(_appState, WRAP, 0, CENTER_ALIGN, INPUT_GAIN_SAMPLER, 0, 1,
		dummy1_graingain);
	space_gg_center->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	if (_DATA->isRunningAsPlugin) {
		auto dummysyncdaw = new HorizontalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN);
		space_gg_center->addChild(dummysyncdaw);
		dummysyncdaw->paddingtop = dummysyncdaw->paddingbottom = 10.f;
		dummysyncdaw->paddingleft = dummysyncdaw->paddingright = 5.f;

		auto transport = new CheckBoxWrapped(_appState, CENTER_ALIGN, LOOPSYNCDAWTRANSPORT);
		dummysyncdaw->addChild(transport);

		space_gg_center->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

		auto dummy2_graingain = new VerticalLayout(_appState, RATIO_FROM_PARENT_View, 3., START_ALIGN,
			space_gg_center);
		dummy2_graingain->paddingtop = dummy2_graingain->paddingbottom = 5.f;

		auto res = new Knob(_appState, WRAP, 0, CENTER_ALIGN, LOOPSYNCDAWBEATS);
		dummy2_graingain->addChild(res);
	}


	init_space_graingen(_STATE, space_granulation);

	init_space_arp(space_granulation);

	auto spacegrainseq2 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINSEQUENCER2] = spacegrainseq2;
	//spacegrainseq2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	auto basscontrol1 = new VerticalLayout(_appState, 6.75,
		PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	spacegrainseq2->addChild(basscontrol1);

	auto input = new NormalButton(_appState, SYM, 0, START_ALIGN, ICON_MD_INPUT, ICON_MD_INPUT,
		GRAINSEQINPUT);
	basscontrol1->addChild(input);
	input->padding = 20.f;

	auto factor = new Selector1<TitleView>(_appState, 6.75, VIEW_COMPUTESIZE, START_ALIGN,
		ARRAY_LEN(syncfactornames),
		makeStringSpan(syncfactornames),
		syncfactors,
		GRAINSEQFACT);
	basscontrol1->addChild(factor);
	factor->paddingleft = 2.5;
	factor->paddingbottom = factor->paddingtop = 10.f;
	auto delay_sync = new NormalButton(_appState, SYM, 0, END_ALIGN, ICON_MD_SYNC, ICON_MD_SYNC,
		GRAINSEQSYNC);
	basscontrol1->addChild(delay_sync);
	delay_sync->padding = 10.f;
	spacegrainseq2->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto basscontrol2 = new VerticalLayout(_appState, 5., PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN);
	spacegrainseq2->addChild(basscontrol2);

	auto delay_backw = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_SKIP_PREVIOUS,
		ICON_MD_SKIP_PREVIOUS, GRAINSEQBACKW);
	basscontrol2->addChild(delay_backw);
	delay_backw->C_active = skcol::fg;

	auto delay_slow = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_SLOW_MOTION_VIDEO,
		ICON_MD_SLOW_MOTION_VIDEO,
		GRAINSEQSLOW);
	basscontrol2->addChild(delay_slow);

	auto delay_fast = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_PLAY_CIRCLE_OUTLINE,
		ICON_MD_PLAY_CIRCLE_OUTLINE,
		GRAINSEQFAST);
	basscontrol2->addChild(delay_fast);

	auto changedir = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_SWAP_HORIZ,
		ICON_MD_SWAP_HORIZ,
		GRAINSEQDIR);
	basscontrol2->addChild(changedir);
	changedir->C_active = skcol::fg;


	auto delay_forw = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_SKIP_NEXT,
		ICON_MD_SKIP_NEXT,
		GRAINSEQFORW);
	basscontrol2->addChild(delay_forw);
	delay_forw->C_active = skcol::fg;


	spacegrainseq2->addChild(new View(_appState, 2.5f, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	auto dummyb1 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN);
	spacegrainseq2->addChild(dummyb1);
	dummyb1->size_reference = &_STATE->knob_height_total;
	dummyb1->size_reference_scale = .75;
	spacegrainseq2->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	auto bsteps = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, GRAINSEQSTEPS, "STEPS");
	dummyb1->addChild(bsteps);
	bsteps->paddingleft = bsteps->paddingright = 5.f;


	auto grains_to_compute = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, GRAINS,
		"GRAINS");
	dummyb1->addChild(grains_to_compute);
	grains_to_compute->paddingleft = grains_to_compute->paddingright = 5.f;

	auto silence_to_compute = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, SILENCE,
		"SILENCE");
	dummyb1->addChild(silence_to_compute);
	silence_to_compute->paddingleft = silence_to_compute->paddingright = 5.f;
	spacegrainseq2->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	auto dummyb2 = new HorizontalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN);
	dummyb2->paddingleft = 5.f;
	/* static const char *smmodenames[] = {"NORMAL", "BOUNCE", "RND DIR", "RND"};
	 dummyb2->addChild(new Selector2(WRAP, 0, CENTER_ALIGN, 4, smmodenames, nullptr, SEQ_MODE));
	 dummyb2->size_reference = &_STATE->textsize1;
	 dummyb2->size_reference_scale = 2.;*/
	spacegrainseq2->addChild(dummyb2);
	spacegrainseq2->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	spacegrainseq2->addChild(new Divider(_appState, START_ALIGN));
	//spacegrainseq2->addChild(new View(_appState, 5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	spacegrainseq2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	spacegrainseq2->addChild(new TitleView(_appState, "STATE", START_ALIGN));

	spacegrainseq2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	auto gsd = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacegrainseq2);
	gsd->paddingleft = gsd->paddingright = 2.5;
	auto gs2d1 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, gsd);
	auto gs2d2 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, gsd);
	auto gs2d3 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, gsd);
	auto gs2d4 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, gsd);

	gs2d1->addChild(new TextButton2(_appState, 5.f, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		LOADSEQUENCE1));
	gs2d1->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	gs2d1->addChild(new TextButton2(_appState, 5.f, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		SAVESEQUENCE1));
	gs2d2->addChild(new TextButton2(_appState, 5.f, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		LOADSEQUENCE2));
	gs2d2->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	gs2d2->addChild(new TextButton2(_appState, 5.f, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		SAVESEQUENCE2));
	gs2d3->addChild(new TextButton2(_appState, 5.f, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		LOADSEQUENCE3));
	gs2d3->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	gs2d3->addChild(new TextButton2(_appState, 5.f, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		SAVESEQUENCE3));
	gs2d4->addChild(new TextButton2(_appState, 5.f, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		LOADSEQUENCE4));
	gs2d4->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	gs2d4->addChild(new TextButton2(_appState, 5.f, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		SAVESEQUENCE4));


	auto spacegrainseq = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAIN_SEQUENCER] = spacegrainseq;

	auto tt = new ScrollView(_appState, WRAP, 0, START_ALIGN, HORIZONTAL, 6);
	spacegrainseq->addChild(tt);

	auto ticker = new SequenceTicker(_appState, START_ALIGN, GRAINSEQACTIVE, tt->window);
	tt->window->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	auto dummy3031 = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	tt->window->addChild(dummy3031);

	for (int32_t i = 0; i < 16; i++) {
		auto dummy3032 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy3031);
		//View *onoff = create_boolean_Button(_appState, ICON_MD_POWER_SETTINGS_NEW,
		//                                  ICON_MD_POWER_SETTINGS_NEW, START_ALIGN, GRAINSEQPOW01 + i, dummy3032);
		auto temp2 = new Knob(_appState, VALUE_FROM_POINTER,
			VALUE_FROM_POINTER, START_ALIGN,
			GRAINSEQGAIN01 + i, 0, 1, dummy3032);
		temp2->size_reference = &_STATE->knob_height_total;
		auto temp = new Knob(_appState, VALUE_FROM_POINTER,
			VALUE_FROM_POINTER, START_ALIGN,
			GRAINSEQPITCH01 + i, 0, 0, dummy3032);
		temp->size_reference = &_STATE->knob_height_total;
		auto ss = new Knob(_appState, VALUE_FROM_POINTER,
			VALUE_FROM_POINTER, START_ALIGN,
			GRAINSEQSIZE01 + i, 0, 0, dummy3032);
		ss->size_reference = &_STATE->knob_height_total;
	}


	init_space_pv(_STATE, space_granulation);

	init_space_cross(_STATE, space_granulation);

	auto space_bpm = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation, true);
	_DATA->views.spaces_fx[SPACE_BPM] = space_bpm;
	auto bpmsync1 = new VerticalLayout(_appState, 6.75,
		PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_bpm->addChild(bpmsync1);

	auto inputsyncbpm = new NormalButton(_appState, SYM, 0, START_ALIGN, ICON_MD_INPUT,
		ICON_MD_INPUT,
		BPMSYNCINPUT);
	bpmsync1->addChild(inputsyncbpm);
	inputsyncbpm->padding = 20.;

	auto bpmsyncfactor = new Selector1<TitleView>(_appState, 6.75, VIEW_COMPUTESIZE, CENTER_ALIGN,
		ARRAY_LEN(syncfactornames),
		makeStringSpan(syncfactornames),
		makeFloatSpan(syncfactors),
		BPMSYNCSYNCFACTOR);
	bpmsync1->addChild(bpmsyncfactor);

	bpmsyncfactor->paddingbottom = bpmsyncfactor->paddingtop = 10.f;
	auto bpm_sync = new NormalButton(_appState, SYM, 0, END_ALIGN, ICON_MD_SYNC, ICON_MD_SYNC,
		BPMSYNCSYNC);
	bpm_sync->padding = 10.;
	bpmsync1->addChild(bpm_sync);
	space_bpm->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto bpmsync2 = new VerticalLayout(_appState, 5., PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN);
	space_bpm->addChild(bpmsync2);

	auto bpm_slow = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_SLOW_MOTION_VIDEO,
		ICON_MD_SLOW_MOTION_VIDEO,
		BPMSYNCSLOW);
	bpmsync2->addChild(bpm_slow);

	auto bpm_fast = new NormalButton(_appState, WRAP, 0, START_ALIGN, ICON_MD_PLAY_CIRCLE_OUTLINE,
		ICON_MD_PLAY_CIRCLE_OUTLINE,
		BPMSYNCFAST);
	bpmsync2->addChild(bpm_fast);


	space_bpm->addChild(new View(_appState, 2.5f, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	auto bpmsync3 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN);
	space_bpm->addChild(bpmsync3);
	bpmsync3->size_reference = &_STATE->knob_height_total;
	//dummyb1->size_reference_scale = .75;

	auto bpmbpm = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, BPMSYNC, "BPM");
	bpmsync3->addChild(bpmbpm);

	auto space_looper = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_LOOPER] = space_looper;

	auto divlooper = new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_looper->addChild(divlooper);

	auto phlooper = new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN);
	space_looper->addChild(phlooper);

	auto dummy1_looper = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN);
	dummy1_looper->paddingtop = 5.f;
	dummy1_looper->paddingbottom = 5.f;
	space_looper->addChild(dummy1_looper);

	auto dummy2_looper = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN);
	dummy2_looper->paddingtop = 5.f;
	dummy2_looper->paddingbottom = 5.f;
	space_looper->addChild(dummy2_looper);

	auto looperfade = new Knob(_appState, WRAP, 0, CENTER_ALIGN, LOOPERFADE, 0, 1, dummy1_looper);
	looperfade->paddingleft = looperfade->paddingright = 25.f;


	auto dummy3_looper = new View(_appState, WRAP,
		0, CENTER_ALIGN);
	space_looper->addChild(dummy3_looper);

	auto space_pdetect = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_PDETECTGRAIN] = space_pdetect;

	space_pdetect->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	space_pdetect->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dummy1_pdetect = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN);
	dummy1_pdetect->paddingtop = 5.f;
	dummy1_pdetect->paddingbottom = 5.f;
	space_pdetect->addChild(dummy1_pdetect);

	auto dummy2_pdetect = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN);
	dummy2_pdetect->paddingtop = 5.f;
	dummy2_pdetect->paddingbottom = 5.f;


	auto dummy3_pdetect = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN);
	dummy3_pdetect->paddingtop = 5.f;
	dummy3_pdetect->paddingbottom = 5.f;
	space_pdetect->addChild(dummy3_pdetect);
	space_pdetect->addChild(dummy2_pdetect);

	auto pdetecttv = new PDetectTVGrain(_appState, CENTER_ALIGN, dummy2_pdetect);
	pdetecttv->paddingtop = pdetecttv->paddingbottom = 10.f;
	pdetecttv->paddingleft = pdetecttv->paddingright = 2.5f;
	pdetecttv->perm = true;

	auto pdetectsmooth = new Knob(_appState, WRAP, 0, CENTER_ALIGN, PDETECTGRAINSMOOTH2, 0, 1,
		dummy1_pdetect);

	auto pdetectprelp = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, PDETECTGRAINPRELP, 0, 1, dummy1_pdetect);

	auto pdetectbounda = new Knob(_appState, WRAP, 0, CENTER_ALIGN, PDETECTGRAINA, 0, 1,
		dummy3_pdetect);

	auto pdetectboundb = new Knob(_appState, WRAP, 0, CENTER_ALIGN, PDETECTGRAINB, 0, 1,
		dummy3_pdetect);

	auto pdetecttransp = new Knob(_appState, WRAP, 0, CENTER_ALIGN, PDETECTGRAINTRANSPOSE, 0, 1,
		dummy3_pdetect);

	auto space_grainvco = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINVCO] = space_grainvco;

	space_grainvco->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dummyvcophreset = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN);
	dummyvcophreset->paddingleft = dummyvcophreset->paddingright = 5;
	dummyvcophreset->paddingtop = dummyvcophreset->paddingbottom = 10.f;
	space_grainvco->addChild(dummyvcophreset);
	auto chbvcophreset = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINVCOPHRESET);
	dummyvcophreset->addChild(chbvcophreset);

	auto dummyvcovcf = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN);
	dummyvcovcf->paddingleft = dummyvcovcf->paddingright = 5;
	dummyvcovcf->paddingtop = dummyvcovcf->paddingbottom = 10.f;
	space_grainvco->addChild(dummyvcovcf);
	auto chbvcovcf = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINVCOVCF);
	dummyvcovcf->addChild(chbvcovcf);

	auto vcograinbase = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCOCPS, 0, 1,
		space_grainvco);
	vcograinbase->paddingleft = vcograinbase->paddingright = 5.f;
	vcograinbase->paddingtop = vcograinbase->paddingbottom = 10.f;

	auto vcograindetlr = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCODETLR, 0, 1,
		space_grainvco);
	vcograindetlr->paddingleft = vcograindetlr->paddingright = 5.f;
	vcograindetlr->paddingtop = vcograindetlr->paddingbottom = 10.f;

	auto dummyvcosettings2 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN);
	dummyvcosettings2->paddingleft = dummyvcosettings2->paddingright = 5.;
	dummyvcosettings2->paddingtop = dummyvcosettings2->paddingbottom = 10.f;
	space_grainvco->addChild(dummyvcosettings2);

	auto dummyfolow = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummyvcosettings2->addChild(dummyfolow);
	auto chb = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINVCOFOLLOW);
	dummyfolow->addChild(chb);

	auto dummyhold = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummyvcosettings2->addChild(dummyhold);
	auto chbh = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINVCOHOLD);
	dummyhold->addChild(chbh);

	auto vcograinmix = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCODRY, 0, 1,
		space_grainvco);
	vcograinmix->paddingleft = vcograinmix->paddingright = 5.f;
	vcograinmix->paddingtop = vcograinmix->paddingbottom = 10.f;

	auto vcograingain = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCOWET, 0, 1,
		space_grainvco);
	vcograingain->paddingleft = vcograingain->paddingright = 5.f;
	vcograingain->paddingtop = vcograingain->paddingbottom = 10.f;

	auto space_grainvco2 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINVCO2] = space_grainvco2;

	space_grainvco2->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dummyvco1 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN);
	dummyvco1->paddingleft = 2.5f;
	dummyvco1->paddingright = 33.3;
	dummyvco1->paddingtop = dummyvco1->paddingbottom = 10.f;
	space_grainvco2->addChild(dummyvco1);

	auto dummyvco1pow2 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummyvco1pow2->paddingleft = dummyvco1pow2->paddingright = 5.f;
	dummyvco1pow2->paddingtop = dummyvco1pow2->paddingbottom = 5.f;
	dummyvco1->addChild(dummyvco1pow2);
	auto chbvco1pow2 = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINVCO1POW);
	dummyvco1pow2->addChild(chbvco1pow2);

	auto vco1type = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(vcoWaveforms),
		vcoWaveforms,
		vcoModes, GRAINVCO0WAVEFORM, 0, 1);
	dummyvco1->addChild(vco1type);

	auto vco1pw = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCO1PW, 0, 1, space_grainvco2);
	vco1pw->paddingleft = vco1pw->paddingright = 5.f;
	vco1pw->paddingtop = vco1pw->paddingbottom = 10.f;

	auto vco1det = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCO1DET, 0, 1, space_grainvco2);
	vco1det->paddingleft = vco1det->paddingright = 5.f;
	vco1det->paddingtop = vco1det->paddingbottom = 10.f;

	auto vco1amp = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCO1AMP, 0, 1, space_grainvco2);
	vco1amp->paddingleft = vco1amp->paddingright = 5.f;
	vco1amp->paddingtop = vco1amp->paddingbottom = 10.f;

	auto dummyvco2 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN);
	dummyvco2->paddingleft = 2.5f;
	dummyvco2->paddingright = 33.3;
	dummyvco2->paddingtop = dummyvco2->paddingbottom = 10.f;
	space_grainvco2->addChild(dummyvco2);

	auto dummyvco2pow2 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummyvco2pow2->paddingleft = dummyvco2pow2->paddingright = 5.f;
	dummyvco2pow2->paddingtop = dummyvco2pow2->paddingbottom = 5.f;
	dummyvco2->addChild(dummyvco2pow2);
	auto chbvco2pow2 = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINVCO2POW);
	dummyvco2pow2->addChild(chbvco2pow2);

	auto vco2type = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(vcoWaveforms),
		vcoWaveforms,
		vcoModes, GRAINVCO1WAVEFORM, 0, 1);
	dummyvco2->addChild(vco2type);

	auto vco2pw = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCO2PW, 0, 1, space_grainvco2);
	vco2pw->paddingleft = vco2pw->paddingright = 5.f;
	vco2pw->paddingtop = vco2pw->paddingbottom = 10.f;

	auto vco2det = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCO2DET, 0, 1, space_grainvco2);
	vco2det->paddingleft = vco2det->paddingright = 5.f;
	vco2det->paddingtop = vco2det->paddingbottom = 10.f;

	auto vco2amp = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCO2AMP, 0, 1, space_grainvco2);
	vco2amp->paddingleft = vco2amp->paddingright = 5.f;
	vco2amp->paddingtop = vco2amp->paddingbottom = 10.f;

	auto dummyvco3 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN);
	dummyvco3->paddingleft = 2.5f;
	dummyvco3->paddingright = 33.3;
	dummyvco3->paddingtop = dummyvco3->paddingbottom = 10.f;
	space_grainvco2->addChild(dummyvco3);

	auto dummyvco3pow2 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummyvco3pow2->paddingleft = dummyvco3pow2->paddingright = 5.f;
	dummyvco3pow2->paddingtop = dummyvco3pow2->paddingbottom = 5.f;
	dummyvco3->addChild(dummyvco3pow2);
	auto chbvco3pow2 = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINVCO3POW);
	dummyvco3pow2->addChild(chbvco3pow2);

	auto vco3type = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(vcoWaveforms),
		vcoWaveforms,
		vcoModes, GRAINVCO2WAVEFORM, 0, 1);
	dummyvco3->addChild(vco3type);

	auto vco3pw = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCO3PW, 0, 1, space_grainvco2);
	vco3pw->paddingleft = vco3pw->paddingright = 5.f;
	vco3pw->paddingtop = vco3pw->paddingbottom = 10.f;

	auto vco3det = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCO3DET, 0, 1, space_grainvco2);
	vco3det->paddingleft = vco3det->paddingright = 5.f;
	vco3det->paddingtop = vco3det->paddingbottom = 10.f;

	auto vco3amp = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINVCO3AMP, 0, 1, space_grainvco2);
	vco3amp->paddingleft = vco3amp->paddingright = 5.f;
	vco3amp->paddingtop = vco3amp->paddingbottom = 10.f;

	auto space_grainsettings2 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN,
		space_granulation, true);
	_DATA->views.spaces_fx[SPACE_GRAINSETTINGS2] = space_grainsettings2;

	auto dddiv1 = new View(_appState, 2.5f, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_grainsettings2->addChild(dddiv1);

	auto dummybandlimitedenvelopes = new HorizontalLayout(_appState, 6.75,
		PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_grainsettings2->addChild(dummybandlimitedenvelopes);
	dummybandlimitedenvelopes->paddingtop = dummybandlimitedenvelopes->paddingbottom = 10.f;
	dummybandlimitedenvelopes->paddingleft = dummybandlimitedenvelopes->paddingright = 5.f;

	auto chbbl = new CheckBoxWrapped(_appState, CENTER_ALIGN, BANDLIMITEDGRAINENV);
	dummybandlimitedenvelopes->addChild(chbbl);
	// AMT(chbvcophreset, GRAINVCOPHRESET, MIDITYPE_NOTEON)

	auto dummynoinput = new HorizontalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_grainsettings2->addChild(dummynoinput);
	dummynoinput->paddingtop = dummynoinput->paddingbottom = 10.f;
	dummynoinput->paddingleft = dummynoinput->paddingright = 5.f;

	auto chbni = new CheckBoxWrapped(_appState, CENTER_ALIGN, NOINPUTFROMTRACK);
	dummynoinput->addChild(chbni);

	auto dummyintegerenv = new HorizontalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_grainsettings2->addChild(dummyintegerenv);
	dummyintegerenv->paddingtop = dummyintegerenv->paddingbottom = 10.f;
	dummyintegerenv->paddingleft = dummyintegerenv->paddingright = 5.f;

	auto chbie = new CheckBoxWrapped(_appState, CENTER_ALIGN, INTEGERENVCYCLES);
	dummyintegerenv->addChild(chbie);

	auto dummywsola = new HorizontalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_grainsettings2->addChild(dummywsola);
	dummywsola->paddingtop = dummywsola->paddingbottom = 10.f;
	dummywsola->paddingleft = dummywsola->paddingright = 5.f;

	auto wsola = new CheckBoxWrapped(_appState, CENTER_ALIGN, WSOLA);
	dummywsola->addChild(wsola);
	auto dummyhq = new HorizontalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN);
	space_grainsettings2->addChild(dummyhq);
	dummyhq->paddingtop = dummyhq->paddingbottom = 10.f;
	dummyhq->paddingleft = dummyhq->paddingright = 5.f;

	auto hq = new CheckBoxWrapped(_appState, CENTER_ALIGN, HQ_RESAMPLING);
	dummyhq->addChild(hq);

	auto space_vcf = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation, true);
	_DATA->views.spaces_fx[SPACE_GRAINFILTER] = space_vcf;

	space_vcf->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto cutfilter = new EnvelopeEditorWindow(_appState, 50.f, PERCENTAGE_FROM_PARENT_View,
		START_ALIGN, GRAINFILTERENVX0, VERTICAL);
	_DATA->views.grainfilterenv = cutfilter;
	space_vcf->addChild(cutfilter);

	auto vcfcont = new EnvelopeWindowControls(_appState, 5., PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN,
		GRAINFILTERENVX0);
	space_vcf->addChild(vcfcont);
	vcfcont->paddingleft = vcfcont->paddingright = 2.5;

	auto dummyvcf1 = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN);
	dummyvcf1->paddingtop = 5.f;
	dummyvcf1->paddingbottom = 5.f;
	space_vcf->addChild(dummyvcf1);


	auto dummyvcf2 = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN);
	dummyvcf1->addChild(dummyvcf2);

	auto filttype = new Selector1<TitleView>(_appState, 5., VIEW_COMPUTESIZE,
		CENTER_ALIGN, ARRAY_LEN(filtertypes),
		makeStringSpan(filtertypes),
		emptyFloats, GRAINFILTERTYPE, 0, 1);
	dummyvcf2->addChild(filttype);
	auto grainfilterres = new Slider(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINFILTERRES, 0, 1, dummyvcf1);
	grainfilterres->paddingleft = grainfilterres->paddingright = 5.f;
	grainfilterres->paddingtop = grainfilterres->paddingbottom = 20.f;

	auto grainfiltergain = new Slider(_appState, WRAP, 0, CENTER_ALIGN, GRAINFILTERGAIN, 0, 1,
		dummyvcf1);
	grainfiltergain->paddingleft = grainfiltergain->paddingright = 5.f;
	grainfiltergain->paddingtop = grainfiltergain->paddingbottom = 20.f;


	auto space_parts = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINPART] = space_parts;

	space_parts->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	space_parts->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dummypartstop = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummypartstop->paddingtop = dummypartstop->paddingbottom = 5.f;
	space_parts->addChild(dummypartstop);

	auto dummypartsmiddle = new VerticalLayout(_appState, WRAP, 0,
		CENTER_ALIGN);
	dummypartsmiddle->paddingtop = dummypartsmiddle->paddingbottom = 5.f;
	space_parts->addChild(dummypartsmiddle);

	auto dummypartsbottom = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummypartsbottom->paddingtop = dummypartsbottom->paddingbottom = 5.f;
	space_parts->addChild(dummypartsbottom);


	auto grainpartsparts = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, GRAINPARTNUMPART,
		"GRAINS");
	dummypartstop->addChild(grainpartsparts);

	auto grainpartsoffset = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, GRAINPARTOFFSET,
		"GRAINS");
	dummypartstop->addChild(grainpartsoffset);

	auto grainpartscps = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINPARTFREQ, 0, 1, dummypartsmiddle);

	auto grainpartsmul = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINPARTMULTI, 0, 1, dummypartsmiddle);

	auto dummypartssettings2 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN);
	dummypartssettings2->paddingleft = dummypartssettings2->paddingright = 5.f;
	dummypartssettings2->paddingtop = dummypartssettings2->paddingbottom = 10.f;
	dummypartsbottom->addChild(dummypartssettings2);

	dummyfolow = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummypartssettings2->addChild(dummyfolow);
	chb = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINPARTFOLLOW);
	dummyfolow->addChild(chb);


	dummyhold = new HorizontalLayout(_appState, HORIZONTAL,
		WRAP, 0);
	dummypartssettings2->addChild(dummyhold);
	chbh = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINPARTHOLD);
	dummyhold->addChild(chbh);


	auto grainpartsdry = new Slider(_appState, WRAP, 0,
		START_ALIGN, GRAINPARTDRY, 0, 1, dummypartsbottom);
	grainpartsdry->paddingleft = grainpartsdry->paddingright = 5.f;
	grainpartsdry->paddingtop = grainpartsdry->paddingbottom = 10.f;

	auto grainpartswet = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINPARTWET, 0, 1,
		dummypartsbottom);
	grainpartswet->paddingleft = grainpartswet->paddingright = 5.f;
	grainpartswet->paddingtop = grainpartswet->paddingbottom = 10.f;


	auto space_modal = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINMODAL] = space_modal;

	space_modal->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	space_modal->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dummymodal = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummymodal->paddingleft = dummymodal->paddingright = 5.f;
	space_modal->addChild(dummymodal);

	auto grainmodalingain = new Slider(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINMODALINGAIN, 0, 1, dummymodal);

	auto grapremodalpregapre = new Slider(_appState, WRAP, 0, CENTER_ALIGN, GRAINMODALPREGAIN, 0, 1,
		dummymodal);

	auto gracpsmodalcpsgacps = new Slider(_appState, WRAP, 0, CENTER_ALIGN, GRAINMODALFREQ, 0, 1,
		dummymodal);

	auto grainmodalq = new Slider(_appState, WRAP, 0, CENTER_ALIGN, GRAINMODALQ, 0, 1, dummymodal);

	auto placeholdermodal3 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummymodal->addChild(placeholdermodal3);

	auto grainmodalmode = new Selector1<TitleView>(_appState, 5., VIEW_COMPUTESIZE,
		CENTER_ALIGN, ARRAY_LEN(modal_names),
		makeStringSpan(modal_names),
		emptyFloats, GRAINMODALMODE, 0, 1);

	placeholdermodal3->addChild(grainmodalmode);

	auto dummymodalsettings2 = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummymodal->addChild(dummymodalsettings2);

	dummyfolow = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummymodalsettings2->addChild(dummyfolow);
	chb = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINMODALFOLLOW);
	dummyfolow->addChild(chb);

	dummyhold = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummymodalsettings2->addChild(dummyhold);
	chbh = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINMODALHOLD);
	dummyhold->addChild(chbh);

	auto grainmodaldry = new Slider(_appState, WRAP, 0, CENTER_ALIGN, GRAINMODALDRY, 0, 1,
		dummymodal);

	auto grainmodalwet = new Slider(_appState, WRAP, 0, CENTER_ALIGN, GRAINMODALWET, 0, 1,
		dummymodal);


	auto space_grain = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINSETTINGS] = space_grain;

	space_grain->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	space_grain->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto knobs1 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN);
	knobs1->paddingtop = 5.f;
	knobs1->paddingbottom = 5.f;
	space_grain->addChild(knobs1);

#ifdef LARGE_GRAINS
	auto grainsize = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINSIZE2, 0, 1, knobs1);
#else
	auto grainsize = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINSIZE, 0, 1, knobs1);
#endif
	auto graindens = new Knob(_appState, WRAP, 0, CENTER_ALIGN, DENSITY, 0, 1, knobs1);
	//    auto rndwriteoff = new Knob(_appState, WRAP, 0, CENTER_ALIGN, DENSDEV, 0, 1, knobs1);
	//    rndwriteoff->notify_istouched = true;
	auto rndreadoff = new Knob(_appState, WRAP, 0, CENTER_ALIGN, RNDREAD, 0, 1, knobs1);


	auto knobs2 = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	knobs2->paddingtop = 5.f;
	knobs2->paddingbottom = 5.f;
	space_grain->addChild(knobs2);


	auto pan = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINPAN, 0, 1, knobs2);
	auto ecycles = new Knob(_appState, WRAP, 0, CENTER_ALIGN, AWINCYLCES, 0, 1, knobs2);

	auto ephs = new Knob(_appState, WRAP, 0, CENTER_ALIGN, ENVPHASE, 0, 1, knobs2);
	auto knobs3 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN);
	knobs3->paddingtop = 5.f;
	knobs3->paddingbottom = 5.f;
	space_grain->addChild(knobs3);

	auto pitch = new Knob(_appState, WRAP, 0, CENTER_ALIGN, PITCH, 0, 1, knobs3);
	auto gliss = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGLISS, 0, 1, knobs3);
	auto rev = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REVERSEGRAINS, 0, 1, knobs3);

	/*
		 * Space Pitch
		 */

	auto space_pitch = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_PITCH] = space_pitch;

	space_pitch->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	space_pitch->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));

	auto dummy1pitch = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_pitch);
	dummy1pitch->paddingtop = 5.f;
	dummy1pitch->paddingbottom = 5.f;

	auto dummy2pitch = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_pitch);
	dummy2pitch->paddingtop = 5.f;
	dummy2pitch->paddingbottom = 5.f;

	auto dummy3pitch = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_pitch);
	dummy3pitch->paddingtop = 5.f;
	dummy3pitch->paddingbottom = 5.f;

	auto intervals_min = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, SEMITONES_MIN,
		"MIN");
	dummy2pitch->addChild(intervals_min);

	auto intervals_max = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, SEMITONES_MAX,
		"MAX");
	dummy2pitch->addChild(intervals_max);

	auto interval_size = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, SEMITONES,
		"SEMITONES");
	interval_size->paddingleft = 25.f;
	interval_size->paddingright = 25.f;
	dummy1pitch->addChild(interval_size);


	auto pitch_min = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, PITCHMIN, 0, 1, dummy3pitch);

	auto pitch_max = new Knob(_appState, WRAP, 0, CENTER_ALIGN, PITCHMAX, 0, 1, dummy3pitch);


	auto space_grainreverb = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN,
		space_granulation, true);
	_DATA->views.spaces_fx[SPACE_GRAINREVERB] = space_grainreverb;

	space_grainreverb->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	space_grainreverb->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));


	auto dummy1grainreverb = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_grainreverb);
	dummy1grainreverb->paddingtop = 5.f;
	dummy1grainreverb->paddingbottom = 5.f;

	auto dummy2grainreverb = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_grainreverb);
	dummy2grainreverb->paddingtop = 5.f;
	dummy2grainreverb->paddingbottom = 5.f;

	auto dummy3grainreverb = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_grainreverb);
	dummy3grainreverb->paddingtop = 5.f;
	dummy3grainreverb->paddingbottom = 5.f;

	auto grainreverb_decay = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINREVERBDECAY, 0, 1, dummy1grainreverb);
	grainreverb_decay->paddingleft = grainreverb_decay->paddingright = 25.f;

	auto grainreverbmix = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINREVERBMIX, 0, 1, dummy2grainreverb);

	auto grainreverbgain = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINREVERBGAIN, 0, 1, dummy2grainreverb);


	auto space_rm_grain = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_RM_GRAIN] = space_rm_grain;


	space_rm_grain->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	space_rm_grain->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dummy1rmgrain = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_rm_grain);
	dummy1rmgrain->paddingtop = 5.f;
	dummy1rmgrain->paddingbottom = 5.f;

	auto dummy2rmgrain = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_rm_grain);
	dummy2rmgrain->paddingtop = 5.f;
	dummy2rmgrain->paddingbottom = 5.f;


	auto dummy3rmgrain = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_rm_grain);
	dummy3rmgrain->paddingtop = 5.f;
	dummy3rmgrain->paddingbottom = 5.f;

	auto rm_freq_min = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINRMMIN, 0, 1, dummy1rmgrain);
	auto rm_freq_max = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINRMMAX, 0, 1, dummy1rmgrain);
	auto rm_grain_mix = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINRMMIX, 0, 1, dummy2rmgrain);
	rm_grain_mix->paddingleft = 25.f;
	rm_grain_mix->paddingright = 25.f;
	/*
	   * Space Bandpass
	   */



	auto space_bandpass_grain = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN,
		space_granulation, true);
	_DATA->views.spaces_fx[SPACE_GRAIN_BP] = space_bandpass_grain;

	space_bandpass_grain->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	space_bandpass_grain->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dummy1bandpassgrain = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bandpass_grain);
	dummy1bandpassgrain->paddingtop = 5.f;
	dummy1bandpassgrain->paddingbottom = 5.f;

	auto dummy2bandpassgrain = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bandpass_grain);
	dummy2bandpassgrain->paddingtop = 5.f;
	dummy2bandpassgrain->paddingbottom = 5.f;

	auto dummy3bandpassgrain = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bandpass_grain);
	dummy3bandpassgrain->paddingtop = 5.f;
	dummy3bandpassgrain->paddingbottom = 5.f;

	auto bp_freq_min = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINBPCENTERMIN, 0, 1, dummy1bandpassgrain);
	auto bp_freq_max = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINBPCENTERMAX, 0, 1, dummy1bandpassgrain);
	auto bp_bw_min = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINBPBWMIN, 0, 1, dummy2bandpassgrain);
	auto bp_bw_max = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINBPBWMAX, 0, 1, dummy2bandpassgrain);
	auto reson2mix_grain = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, GRAINBPMIX, 0, 1, dummy3bandpassgrain);
	auto reson2gain_grain = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINBPGAIN, 0, 1,
		dummy3bandpassgrain);

	auto space_noise = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[NOISEGRAINEFFECT] = space_noise;

	space_noise->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	space_noise->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dummy1bandpassnoise = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_noise);
	dummy1bandpassnoise->paddingtop = 5.f;
	dummy1bandpassnoise->paddingbottom = 5.f;

	auto nt = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		3, makeStringSpan(noise_types),
		emptyFloats, NOISEGRAINTYPE);

	nt->paddingleft = 10.;
	dummy1bandpassnoise->addChild(nt);

	auto dummy2bandpassnoise = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_noise);
	dummy2bandpassnoise->paddingtop = 5.f;
	dummy2bandpassnoise->paddingbottom = 5.f;

	space_noise->addChild(new View(_appState, WRAP,
		0, CENTER_ALIGN));

	dummy2bandpassnoise->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, NOISEGRAINMIX));

	dummy2bandpassnoise->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, NOISEGRAINGAIN));

	/*
	 * Grain effects: WAVESET / DRIVE / DISPERSE / VOWEL / PLUCK.
	 * Every MIN/MAX pair is a per-grain random range (the RINGMOD/RESON grain
	 * convention), not a sweep - set both the same for a fixed value.
	 */

	 // Space Waveset
	auto space_waveset = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINWAVESET] = space_waveset;

	space_waveset->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	space_waveset->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));

	auto dummy1waveset = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_waveset);
	dummy1waveset->paddingtop = 5.f;
	dummy1waveset->paddingbottom = 5.f;

	auto wsmode = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(grainwavesetmodes), makeStringSpan(grainwavesetmodes),
		emptyFloats, GRAINWSMODE);
	wsmode->paddingleft = 10.f;
	dummy1waveset->addChild(wsmode);

	auto dummy2waveset = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_waveset);
	dummy2waveset->paddingtop = 5.f;
	dummy2waveset->paddingbottom = 5.f;

	dummy2waveset->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINWSAMT));
	dummy2waveset->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINWSGROUP));

	auto dummy3waveset = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_waveset);
	dummy3waveset->paddingtop = 5.f;
	dummy3waveset->paddingbottom = 5.f;

	dummy3waveset->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINWSMIX));
	dummy3waveset->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINWSGAIN));

	// Space Drive
	auto space_graindrive = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINDRIVE] = space_graindrive;

	space_graindrive->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	space_graindrive->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));

	auto dummy1graindrive = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_graindrive);
	dummy1graindrive->paddingtop = 5.f;
	dummy1graindrive->paddingbottom = 5.f;

	auto drvtype = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(graindrivetypes), makeStringSpan(graindrivetypes),
		emptyFloats, GRAINDRVTYPE);
	drvtype->paddingleft = 10.f;
	dummy1graindrive->addChild(drvtype);

	auto dummy2graindrive = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_graindrive);
	dummy2graindrive->paddingtop = 5.f;
	dummy2graindrive->paddingbottom = 5.f;

	dummy2graindrive->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINDRVMIN));
	dummy2graindrive->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINDRVMAX));

	auto dummy3graindrive = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_graindrive);
	dummy3graindrive->paddingtop = 5.f;
	dummy3graindrive->paddingbottom = 5.f;

	dummy3graindrive->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINDRVMIX));
	dummy3graindrive->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINDRVGAIN));

	// Space Disperse
	auto space_graindisperse = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN,
		space_granulation, true);
	_DATA->views.spaces_fx[SPACE_GRAINDISPERSE] = space_graindisperse;

	space_graindisperse->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	space_graindisperse->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));

	auto dummy1graindisp = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_graindisperse);
	dummy1graindisp->paddingtop = 5.f;
	dummy1graindisp->paddingbottom = 5.f;

	dummy1graindisp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINDISPFMIN));
	dummy1graindisp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINDISPFMAX));

	auto dummy2graindisp = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_graindisperse);
	dummy2graindisp->paddingtop = 5.f;
	dummy2graindisp->paddingbottom = 5.f;

	dummy2graindisp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINDISPSTAGES));
	dummy2graindisp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINDISPDEPTH));

	auto dummy3graindisp = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_graindisperse);
	dummy3graindisp->paddingtop = 5.f;
	dummy3graindisp->paddingbottom = 5.f;

	dummy3graindisp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINDISPMIX));
	dummy3graindisp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINDISPGAIN));

	// Space Vowel
	auto space_grainvowel = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINVOWEL] = space_grainvowel;

	space_grainvowel->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	space_grainvowel->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));

	/* The VOICE selector sits in a fixed row - as a WRAP child it would share
	   the space with the sliders and swallow a slider's worth of height, and
	   Selector2 shrinks itself to title + value (6.25% of the window) anyway,
	   so 6.75 is its own box plus a little air. */
	auto vowselcol = new VerticalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN, space_grainvowel);
	vowselcol->paddingtop = vowselcol->paddingbottom = 5.f;

	auto vowvoice = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(voxvoicenames), makeStringSpan(voxvoicenames),
		emptyFloats, GRAINVOWVOICE);
	vowvoice->paddingleft = vowvoice->paddingright = SDSIDEPAD;
	vowselcol->addChild(vowvoice);

	/* WRAP sliders straight onto the space, the idiom the neighbouring grain
	   spaces use (VCO2, MODAL, BUZZ): they share what is left and Slider::init
	   then settles each on its own 5%-of-window bar, so a VOWEL slider is the
	   same size as a MODAL one when you switch between them. Side padding on
	   the sliders rather than on a container - pad at one level only, or they
	   nest in past the 61px every other slider in the plugin starts at. */
	auto vowslider = [&](int id) {
		auto* sl = new Slider(_appState, WRAP, 0, START_ALIGN, id, 0, 1, space_grainvowel);
		sl->paddingleft = sl->paddingright = SDSIDEPAD;
		return sl;
	};
	vowslider(GRAINVOWMIN);
	vowslider(GRAINVOWMAX);
	vowslider(GRAINVOWBW);
	vowslider(GRAINVOWMIX);
	vowslider(GRAINVOWGAIN);

	// Space Pluck
	auto space_grainpluck = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_granulation,
		true);
	_DATA->views.spaces_fx[SPACE_GRAINPLUCK] = space_grainpluck;

	space_grainpluck->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	space_grainpluck->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));

	auto dummy1grainplk = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_grainpluck);
	dummy1grainplk->paddingtop = 5.f;
	dummy1grainplk->paddingbottom = 5.f;

	dummy1grainplk->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINPLKMIN));
	dummy1grainplk->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINPLKMAX));

	auto dummy2grainplk = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_grainpluck);
	dummy2grainplk->paddingtop = 5.f;
	dummy2grainplk->paddingbottom = 5.f;

	dummy2grainplk->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINPLKDECAY));
	dummy2grainplk->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINPLKDAMP));

	auto dummy3grainplk = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_grainpluck);
	dummy3grainplk->paddingtop = 5.f;
	dummy3grainplk->paddingbottom = 5.f;

	dummy3grainplk->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINPLKMIX));
	dummy3grainplk->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINPLKGAIN));
}
