#include "gui_internal.h"


static std::vector<std::string> dbpointstextSpectrum = { "-60", "-45", "-30", "-15", "  0" };
// modalrevmodes (gs_common.h) is shared with ParameterInit so the selector
// and the parameter's enum range cannot drift apart.
//static const float modalvals[] = {128, 256, 512, 1024, 1536, 2048, 4096, 8192};

void init_space_fx_stereo(tsl::AppState* _appState, Layout* root) {
	auto space_reverb = new SpaceStereo(_appState, WRAP, 0,
		CENTER_ALIGN, root);
	_DATA->views.active_spaces_array[SPACE_REVERB] = space_reverb;

	space_reverb->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	space_reverb->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dummytitlereverb = new VerticalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN, space_reverb);
	auto reverbtype = new Selector1<TitleView>(_appState, WRAP, 0, START_ALIGN,
		_STATE->dofastrender.load() ? ARRAY_LEN(reverbtypes)
		: ARRAY_LEN(reverbtypes2),
		_STATE->dofastrender.load() ? makeStringSpan(
			reverbtypes)
		: makeStringSpan(
			reverbtypes2),
		_STATE->dofastrender.load() ? makeFloatSpan(
			reverbtypevalues)
		: makeFloatSpan(
			reverbtypevalues2),
		ASSTFX);
	reverbtype->paddingtop = 10.f;
	reverbtype->paddingbottom = 10.f;
	reverbtype->paddingleft = 2.5;
	dummytitlereverb->addChild(reverbtype);
	reverbtype->titletext = "STEREO EFFECTS";
	reverbtype->middleclick = true;


	auto offbuttonreverb = new BypassOffButton(_appState, SYM, 0, END_ALIGN,
		static_cast<const char*>(ICON_MD_POWER_SETTINGS_NEW),
		static_cast<const char*>(ICON_MD_POWER_SETTINGS_NEW),
		OFFSTEREOFX);
	offbuttonreverb->padding = 10.f;
	dummytitlereverb->addChild(offbuttonreverb);

	auto bypassbuttonreverb = new BypassOffButton(_appState, SYM, 0, END_ALIGN,
		static_cast<const char*>(ICON_MD_RADIO_BUTTON_UNCHECKED),
		static_cast<const char*>(ICON_MD_RADIO_BUTTON_CHECKED),
		BYPASSSTEREOFX);
	bypassbuttonreverb->padding = 10.f;
	dummytitlereverb->addChild(bypassbuttonreverb);

	auto space_clip = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_CLIPPER] = space_clip;
	space_clip->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	auto clip1 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_clip);
	auto clipp = new VerticalLayout(_appState, 3.f, RATIO_FROM_PARENT_View, START_ALIGN, clip1);
	clipp->paddingtop = clipp->paddingbottom = 5.f;
	auto cthr = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CLIPPERTHRS);
	clipp->addChild(cthr);
	auto cst = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CLIPPERSTART);
	clipp->addChild(cst);

	auto space_ms = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_MONOSTEREO] = space_ms;
	space_ms->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto ms1 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_ms);
	auto dms = new VerticalLayout(_appState, 3.f, RATIO_FROM_PARENT_View, START_ALIGN, ms1);
	dms->paddingtop = 5.f;
	dms->paddingbottom = 5.f;
	auto sw = new Knob(_appState, 3.f, RATIO_FROM_PARENT_View, CENTER_ALIGN, STEREOWIDTH);
	dms->addChild(sw);

#if 0	/* REVERB8/9/10 removed from the project - their params are gone from
	   the enum and the effects from Convolver.h/cpp, so this stays compiled
	   out, not deleted, in case the family ever comes back */
	/* REVERB8 (LONGVERB): IR selector in its own ratio-sized row at the top,
	   everything else as horizontal sliders stacked in one layout */
	auto space_rev8 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_REVERB8] = space_rev8;
	space_rev8->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	auto lvselrow = new VerticalLayout(_appState, 8., RATIO_FROM_PARENT_View,
		START_ALIGN, space_rev8);
	lvselrow->paddingtop = 5.f;
	auto lvalgsel = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(longverbirmodes), makeStringSpan(longverbirmodes),
		emptyFloats, LVALG, 0, 1, "IR");
	lvalgsel->paddingleft = 10.f;
	lvselrow->addChild(lvalgsel);

	auto lvsliders = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_rev8);
	for (uint16_t id : { (uint16_t)LVTIME, (uint16_t)LVRT60, (uint16_t)LVEVOLVE,
			(uint16_t)LVDENSITY, (uint16_t)LVDAMP, (uint16_t)LVMORPHRATE,
			(uint16_t)LVMORPHFRONT, (uint16_t)LVMODDEPTH, (uint16_t)LVMODRATE,
			(uint16_t)LVXFEED, (uint16_t)LVWIDTH, (uint16_t)LVMIX,
			(uint16_t)LVGAIN }) {
		auto sl = new Slider(_appState, WRAP, 0, CENTER_ALIGN, id, 0, 1, lvsliders);
		sl->paddingleft = sl->paddingright = 2.5f;
	}

	/* bottom row: reverse morph + linked draws */
	auto lvbtnrow = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, lvsliders);
	_STATE->parameters[LVMORPHREV].view = new TextButton(_appState, 2.5,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		"REVERSE",
		LVMORPHREV, 0, 1);
	_STATE->parameters[LVMORPHREV].view->paddingleft =
		_STATE->parameters[LVMORPHREV].view->paddingright = 10.;
	lvbtnrow->addChild(_STATE->parameters[LVMORPHREV].view);
	lvbtnrow->addChild(new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, LVLINK));

	/* REVERB9 (4-unit coupled LONGVERB): same layout scheme as REVERB8 */
	auto space_rev9 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_REVERB9] = space_rev9;
	space_rev9->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	auto rv9selrow = new VerticalLayout(_appState, 8., RATIO_FROM_PARENT_View,
		START_ALIGN, space_rev9);
	rv9selrow->paddingtop = 5.f;
	auto rv9algsel = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(longverbirmodes), makeStringSpan(longverbirmodes),
		emptyFloats, RV9ALG, 0, 1, "IR");
	rv9algsel->paddingleft = 10.f;
	rv9selrow->addChild(rv9algsel);

	auto rv9sliders = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_rev9);
	for (uint16_t id : { (uint16_t)RV9SIZE, (uint16_t)RV9RT60, (uint16_t)RV9EVOLVE,
			(uint16_t)RV9DENSITY, (uint16_t)RV9DAMP, (uint16_t)RV9MORPHRATE,
			(uint16_t)RV9MORPHFRONT,
			(uint16_t)RV9XFEED, (uint16_t)RV9WIDTH, (uint16_t)RV9MIX,
			(uint16_t)RV9GAIN }) {
		auto sl = new Slider(_appState, WRAP, 0, CENTER_ALIGN, id, 0, 1, rv9sliders);
		sl->paddingleft = sl->paddingright = 2.5f;
	}

	/* REVERB10 (wet-feedback flat-noise-IR loop): sliders only, no
	   selector - the loop only admits the flat draw */
	auto space_rev10 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_REVERB10] = space_rev10;
	space_rev10->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	auto rv10sliders = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_rev10);
	for (uint16_t id : { (uint16_t)RV10TIME, (uint16_t)RV10RT60,
			(uint16_t)RV10EVOLVE, (uint16_t)RV10DAMP, (uint16_t)RV10MORPHRATE,
			(uint16_t)RV10MORPHFRONT, (uint16_t)RV10XFEED, (uint16_t)RV10WIDTH,
			(uint16_t)RV10MIX, (uint16_t)RV10GAIN }) {
		auto sl = new Slider(_appState, WRAP, 0, CENTER_ALIGN, id, 0, 1, rv10sliders);
		sl->paddingleft = sl->paddingright = 2.5f;
	}
#endif	/* REVERB8/9/10 */

	auto space_modrev = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_MODALREV] = space_modrev;
	space_modrev->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dmod1 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_modrev);

	auto dummy1modrev = new VerticalLayout(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN,
		dmod1);
	dummy1modrev->paddingtop = 5.f;
	dummy1modrev->paddingbottom = 5.f;

	auto dummy2modrev = new VerticalLayout(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN,
		dmod1);
	dummy2modrev->paddingtop = 5.f;
	dummy2modrev->paddingbottom = 5.f;


	auto dummy3modrev = new VerticalLayout(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN,
		dmod1);
	dummy3modrev->paddingtop = 5.f;
	dummy3modrev->paddingbottom = 5.f;


	auto selcurve = new Selector2(_appState, WRAP, 0, CENTER_ALIGN, (int)std::size(modalrevmodes), makeStringSpan(modalrevmodes),
		emptyFloats, MODALREVMODES, 0,
		1, "MODE");
	dummy1modrev->addChild(selcurve);
	selcurve->paddingleft = 10.f;
	auto dummy1moda = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy1modrev);

	// the button centres its label in its own box, while the selector to the
	// left and the knob to the right put their titles in the top textsize2 of
	// the row (2.5% of window height) - so the box has to be that same height
	// for all three to sit on one line
	_STATE->parameters[MODALREVHOLD].view = new TextButton(_appState, 2.5,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		"FREEZE",
		MODALREVHOLD, 0, 1);
	_STATE->parameters[MODALREVHOLD].view->paddingleft = _STATE->parameters[MODALREVHOLD].view->paddingright = 10.;
	dummy1moda->addChild(_STATE->parameters[MODALREVHOLD].view);
	dummy1modrev->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, MODALREVDUCK));

	dummy2modrev->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, MODALREVDELAY));
	dummy2modrev->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, MODALREVPITCH));
	dummy2modrev->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, MODALREVWIDTH));
#if GS_MODALREV_ADAPTIVE
	dummy3modrev->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, MODALREVPURITY));
#endif
	dummy3modrev->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, MODALREVMORPH));
	dummy3modrev->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, MODALREVDRY));
	dummy3modrev->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, MODALREVWET));


	auto space_limiter = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_LIMITER] = space_limiter;

	space_limiter->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto d1l = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_limiter);
	auto dummy1spacelimiter = new VerticalLayout(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN,
		d1l);
	dummy1spacelimiter->paddingtop = 5.f;
	dummy1spacelimiter->paddingbottom = 5.f;
	dummy1spacelimiter->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, LIMTHRES));
	dummy1spacelimiter->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, LIMREL));
	/*
		auto limmodedummy = new VerticalLayout(_appState, 6.75f, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN, space_limiter);
		limmodedummy->paddingtop = limmodedummy->paddingbottom = 10.f;
		limmodedummy->paddingleft = limmodedummy->paddingright = 15.f;
		static const char *limmodenames[] = {"HARD", "SOFT"};
		auto limmode = new Selector1<TitleView>(WRAP, 0, CENTER_ALIGN, ARRAY_LEN(limmodenames), limmodenames,
												  shimmermodesvales, LIMMODE);
		limmodedummy->addChild(limmode);
	*/
	auto space_reverbsc = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_REVERB5] = space_reverbsc;

	space_reverbsc->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	auto dummy1spacereverbsc = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverbsc);
	dummy1spacereverbsc->paddingtop = 5.f;
	dummy1spacereverbsc->paddingbottom = 5.f;
	auto dummy2spacereverbsc = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverbsc);
	dummy2spacereverbsc->paddingtop = 5.f;
	dummy2spacereverbsc->paddingbottom = 5.f;

	auto dummy3spacereverbsc = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverbsc);
	dummy3spacereverbsc->paddingtop = 5.f;
	dummy3spacereverbsc->paddingbottom = 5.f;

	dummy1spacereverbsc->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV5PREDELAY));
	dummy1spacereverbsc->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV5T60LOW));
	dummy1spacereverbsc->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV5T60HI));
	dummy2spacereverbsc->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV5XOVER));
	dummy2spacereverbsc->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV5DAMP2));
	dummy3spacereverbsc->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REVERB5MIX));
	dummy3spacereverbsc->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REVERB5GAIN));

	auto space_spec = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	auto spec = new EQ5Editor<EQ5RenderDummy, DUMMYSPECTRUM>(_appState, WRAP, 0, CENTER_ALIGN,
		_DATA->updateRenderThreadSpectrum,
		_DATA->inputspectrum,
		dbpointstextSpectrum);
	spec->paddingleft = spec->paddingright = 5.f;
	space_spec->addChild(spec);
	_DATA->views.spaces_fx[SPACE_SPECTRUM] = space_spec;

	auto space_dcs = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_DCS] = space_dcs;

	auto space_delay = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_PINGPONG] = space_delay;

	auto delaycontrolpanel2 = new VerticalLayout(_appState, 6.,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		space_delay);
	auto delay_input = new NormalButton(_appState, SYM, 0, START_ALIGN, ICON_MD_INPUT,
		ICON_MD_INPUT,
		PP_CONTROLS_ACTIVE, 0,
		1);
	delaycontrolpanel2->addChild(delay_input);
	delay_input->padding = 17.f;


	auto delay_factor = new Selector1<TitleView>(_appState, 5.f, VIEW_COMPUTESIZE, START_ALIGN,
		ARRAY_LEN(syncfactornames), syncfactornames,
		syncfactors, PP_SYNC_FACTOR);
	delaycontrolpanel2->addChild(delay_factor);
	//delay_factor->paddingtop = delay_factor->paddingbottom = 10.f;

	auto delay_slow = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_SLOW_MOTION_VIDEO,
		ICON_MD_SLOW_MOTION_VIDEO, PP_SLOW);
	delay_slow->padding = 10.f;
	delaycontrolpanel2->addChild(delay_slow);

	auto delay_fast = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN,
		ICON_MD_PLAY_CIRCLE_OUTLINE,
		ICON_MD_PLAY_CIRCLE_OUTLINE, PP_FAST);
	delay_fast->padding = 10.f;
	delaycontrolpanel2->addChild(delay_fast);

	auto delay_sync = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_SYNC,
		ICON_MD_SYNC, PP_SYNC);
	delay_sync->padding = 10.f;
	delaycontrolpanel2->addChild(delay_sync);

	auto delay_dum1 = new VerticalLayout(_appState, 6.,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN, space_delay);
	auto delay_hold1 = new TextButton(_appState, WRAP, 0, CENTER_ALIGN, "FREEZE",
		PPHOLD, 0, 1);

	delay_hold1->padding = 10.f;
	delay_dum1->addChild(delay_hold1);
	_STATE->parameters[PPHOLD].view = delay_hold1;


	_STATE->parameters[PPREVERSE].view = new TextButton(_appState, WRAP, 0, CENTER_ALIGN, "BACKW",
		PPREVERSE, 0, 1);

	delay_dum1->addChild(_STATE->parameters[PPREVERSE].view);
	_STATE->parameters[PPREVERSE].view->padding = 10.;
	auto dummy2hordelay = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_delay);
	auto delay_delay = new Slider(_appState, WRAP, 0, START_ALIGN, PPDELAY, 0, 1, dummy2hordelay);
	delay_delay->paddingleft = 5.f;
	delay_delay->paddingright = 5.f;
	delay_delay->paddingtop = 20.f;
	delay_delay->paddingbottom = 20.f;
	auto delay_fb = new Slider(_appState, WRAP, 0, START_ALIGN, PPFB, 0, 1, dummy2hordelay);
	delay_fb->paddingleft = 5.f;
	delay_fb->paddingright = 5.f;
	delay_fb->paddingtop = 20.f;
	delay_fb->paddingbottom = 20.f;
	auto delay_shift = new Slider(_appState, WRAP, 0, START_ALIGN, PPSHIFT, 0, 1, dummy2hordelay);
	delay_shift->paddingleft = 5.f;
	delay_shift->paddingright = 5.f;
	delay_shift->paddingtop = 20.f;
	delay_shift->paddingbottom = 20.f;
	auto delay_shiftmix = new Slider(_appState, WRAP, 0, START_ALIGN, PPSHIFTMIX, 0, 1,
		dummy2hordelay);
	delay_shiftmix->paddingleft = 5.f;
	delay_shiftmix->paddingright = 5.f;
	delay_shiftmix->paddingtop = 20.f;
	delay_shiftmix->paddingbottom = 20.f;
	auto delay_cut_off_h = new Slider(_appState, WRAP, 0, START_ALIGN, PPHP, 0, 1, dummy2hordelay);
	delay_cut_off_h->paddingleft = 5.f;
	delay_cut_off_h->paddingright = 5.f;
	delay_cut_off_h->paddingtop = 20.f;
	delay_cut_off_h->paddingbottom = 20.f;
	auto delay_cut_off_l = new Slider(_appState, WRAP, 0, START_ALIGN, PPLP, 0, 1, dummy2hordelay);
	delay_cut_off_l->paddingleft = 5.f;
	delay_cut_off_l->paddingright = 5.f;
	delay_cut_off_l->paddingtop = 20.f;
	delay_cut_off_l->paddingbottom = 20.f;
	auto delay_dry = new Slider(_appState, WRAP, 0, START_ALIGN, PPDRY, 0, 1, dummy2hordelay);
	delay_dry->paddingleft = 5.f;
	delay_dry->paddingright = 5.f;
	delay_dry->paddingtop = 20.f;
	delay_dry->paddingbottom = 20.f;
	auto delay_wet = new Slider(_appState, WRAP, 0, START_ALIGN, PPWET, 0, 1, dummy2hordelay);
	delay_wet->paddingleft = 5.f;
	delay_wet->paddingright = 5.f;
	delay_wet->paddingtop = 20.f;
	delay_wet->paddingbottom = 20.f;

	auto space_reverb1 = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_REVERB1] = space_reverb1;

	space_reverb1->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1reverbspacereverb1 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb1);
	dummy1reverbspacereverb1->paddingtop = 5.f;
	dummy1reverbspacereverb1->paddingbottom = 5.f;
	auto dummy2reverbspacereverb1 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb1);
	dummy2reverbspacereverb1->paddingtop = 5.f;
	dummy2reverbspacereverb1->paddingbottom = 5.f;
	auto dummy3reverbspacereverb1 = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb1);
	dummy3reverbspacereverb1->paddingtop = 20.f;
	dummy3reverbspacereverb1->paddingbottom = 20.f;

	auto space_reverb2 = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_REVERB2] = space_reverb2;

	space_reverb2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1reverbspacereverb2 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb2);
	dummy1reverbspacereverb2->paddingtop = 5.f;
	dummy1reverbspacereverb2->paddingbottom = 5.f;
	auto dummy2reverbspacereverb2 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb2);
	dummy2reverbspacereverb2->paddingtop = 5.f;
	dummy2reverbspacereverb2->paddingbottom = 5.f;
	auto dummy3reverbspacereverb2 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb2);
	dummy3reverbspacereverb2->paddingtop = 5.f;
	dummy3reverbspacereverb2->paddingbottom = 5.f;

	auto space_reverb3 = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_REVERB3] = space_reverb3;

	space_reverb3->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1reverbspacereverb3 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb3);
	dummy1reverbspacereverb3->paddingtop = 5.f;
	dummy1reverbspacereverb3->paddingbottom = 5.f;
	auto dummy2reverbspacereverb3 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb3);
	dummy2reverbspacereverb3->paddingtop = 5.f;
	dummy2reverbspacereverb3->paddingbottom = 5.f;
	auto dummy3reverbspacereverb3 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb3);
	dummy3reverbspacereverb3->paddingtop = 5.f;
	dummy3reverbspacereverb3->paddingbottom = 5.f;

	auto space_reverb4 = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_REVERB4] = space_reverb4;

	space_reverb4->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1reverbspacereverb4 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb4);
	dummy1reverbspacereverb4->paddingtop = 5.f;
	dummy1reverbspacereverb4->paddingbottom = 5.f;
	auto dummy2reverbspacereverb4 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb4);
	dummy2reverbspacereverb4->paddingtop = 5.f;
	dummy2reverbspacereverb4->paddingbottom = 5.f;
	auto dummy3reverbspacereverb4 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb4);
	dummy3reverbspacereverb4->paddingtop = 5.f;
	dummy3reverbspacereverb4->paddingbottom = 5.f;

	auto space_reverb5 = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_CHORUS] = space_reverb5;

	space_reverb5->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1reverbspacereverb5 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb5);
	dummy1reverbspacereverb5->paddingtop = 5.f;
	dummy1reverbspacereverb5->paddingbottom = 5.f;
	auto dummy2reverbspacereverb5 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb5);
	dummy2reverbspacereverb5->paddingtop = 5.f;
	dummy2reverbspacereverb5->paddingbottom = 5.f;
	auto dummy3reverbspacereverb5 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb5);
	dummy3reverbspacereverb5->paddingtop = 5.f;
	dummy3reverbspacereverb5->paddingbottom = 5.f;

	auto space_reverb6 = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_STEREO_QUAD_PHASER] = space_reverb6;

	space_reverb6->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1reverbspacereverb6 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb6);
	dummy1reverbspacereverb6->paddingtop = 5.f;
	dummy1reverbspacereverb6->paddingbottom = 5.f;
	auto dummy2reverbspacereverb6 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb6);
	dummy2reverbspacereverb6->paddingtop = 5.f;
	dummy2reverbspacereverb6->paddingbottom = 5.f;
	auto dummy3reverbspacereverb6 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb6);
	dummy3reverbspacereverb6->paddingtop = 5.f;
	dummy3reverbspacereverb6->paddingbottom = 5.f;

	auto space_reverb7 = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_STC] = space_reverb7;

	space_reverb7->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1reverbspacereverb7 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb7);
	dummy1reverbspacereverb7->paddingtop = 5.f;
	dummy1reverbspacereverb7->paddingbottom = 5.f;
	auto dummy2reverbspacereverb7 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb7);
	dummy2reverbspacereverb7->paddingtop = 5.f;
	dummy2reverbspacereverb7->paddingbottom = 5.f;
	auto dummy3reverbspacereverb7 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb7);
	dummy3reverbspacereverb7->paddingtop = 5.f;
	dummy3reverbspacereverb7->paddingbottom = 5.f;

	//STEREO COMPRESSOR

	auto render_ir_st = new CompressorView(_appState, WRAP, 0,
		START_ALIGN);
	_DATA->views.IRSTComp = render_ir_st;

	auto stc_thresh = new Knob(_appState, WRAP, 0, START_ALIGN, STCOMPTHR, 0, 1,
		dummy1reverbspacereverb7);

	auto stc_rat = new Knob(_appState, WRAP, 0, START_ALIGN, STCOMPRATIO, 0, 1,
		dummy1reverbspacereverb7);

	auto stc_knee = new Knob(_appState, WRAP, 0, START_ALIGN, STCOMPKNEE, 0, 1,
		dummy1reverbspacereverb7);

	auto stc_att = new Knob(_appState, WRAP, 0, START_ALIGN, STCOMPATT, 0, 1,
		dummy2reverbspacereverb7);

	dummy2reverbspacereverb7->addChild(render_ir_st);

	auto stc_dec = new Knob(_appState, WRAP, 0, START_ALIGN, STCOMPDEC, 0, 1,
		dummy2reverbspacereverb7);

	auto stc_rms = new Knob(_appState, WRAP, 0, START_ALIGN, STCOMPRMS, 0, 1,
		dummy3reverbspacereverb7);

	auto stc_looka = new Knob(_appState, WRAP, 0, START_ALIGN, STCOMPLOOKA, 0, 1,
		dummy3reverbspacereverb7);

	auto stc_make = new Knob(_appState, WRAP, 0, START_ALIGN, STCOMPMAKE, 0, 1,
		dummy3reverbspacereverb7);

	dummy1reverbspacereverb6->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, PHASER2FB));
	dummy1reverbspacereverb6->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, PHASER2PH));
	// dummy2reverbspacereverb6->addChild(new Knob(_appState, WRAP, 0,
	//                                           CENTER_ALIGN, PHASER2HP));
	//dummy2reverbspacereverb6->addChild(new Knob(_appState, WRAP, 0,
	//                                          CENTER_ALIGN, PHASER2DIST));
	auto sqp_mix = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, PHASER2MIX, 0, 1, dummy2reverbspacereverb6);
	sqp_mix->paddingleft = 25.f;
	sqp_mix->paddingright = 25.f;

	auto space_shimmer = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_reverb, true);
	_DATA->views.spaces_fx[SPACE_REVERB6] = space_shimmer;

	auto dummyshimmer = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_shimmer);
	dummyshimmer->paddingleft = dummyshimmer->paddingright = 5.f;

	auto modedummy = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummyshimmer);

	modedummy->paddingleft = modedummy->paddingright = 10.f;
	auto shimmmode = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(shimmerdarkmodenames),
		makeStringSpan(shimmerdarkmodenames),
		emptyFloats, SHIMMERDARKMODE);
	modedummy->addChild(shimmmode);
	auto rev7size = new Slider(_appState, WRAP, 0, CENTER_ALIGN, REV7SIZE, 0, 1, dummyshimmer);
	rev7size->paddingtop = rev7size->paddingbottom = 10.f;
	auto rev7fb = new Slider(_appState, WRAP, 0, CENTER_ALIGN, REV7FB, 0, 1, dummyshimmer);
	rev7fb->paddingtop = rev7fb->paddingbottom = 10.f;
	auto rev7diff = new Slider(_appState, WRAP, 0, CENTER_ALIGN, REV7DIFF, 0, 1, dummyshimmer);
	rev7diff->paddingtop = rev7diff->paddingbottom = 10.f;
	auto rev7hp = new Slider(_appState, WRAP, 0, CENTER_ALIGN, REV7HPCUT, 0, 1, dummyshimmer);
	rev7hp->paddingtop = rev7hp->paddingbottom = 10.f;
	auto rev7lp = new Slider(_appState, WRAP, 0, CENTER_ALIGN, REV7LPCUT, 0, 1, dummyshimmer);
	rev7lp->paddingtop = rev7lp->paddingbottom = 10.f;
	auto rev7rate = new Slider(_appState, WRAP, 0, CENTER_ALIGN, REV7RATE, 0, 1, dummyshimmer);
	rev7rate->paddingtop = rev7rate->paddingbottom = 10.f;
	auto rev7depth = new Slider(_appState, WRAP, 0, CENTER_ALIGN, REV7DEPTH, 0, 1, dummyshimmer);
	rev7depth->paddingtop = rev7depth->paddingbottom = 10.f;
	auto rev7shift = new Slider(_appState, WRAP, 0, CENTER_ALIGN, REV7SHIFT, 0, 1, dummyshimmer);
	rev7shift->paddingtop = rev7shift->paddingbottom = 10.f;
	auto shiftmodedummy = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummyshimmer);
	shiftmodedummy->paddingleft = shiftmodedummy->paddingright = 10.f;
	shiftmodedummy->addChild(
		new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN, ARRAY_LEN(shimmermodesnames),
			makeStringSpan(shimmermodesnames),
			emptyFloats, REV7SHIFTMODE));

	auto rev7mix = new Slider(_appState, WRAP, 0, CENTER_ALIGN, REV7MIX, 0, 1, dummyshimmer);
	rev7mix->paddingtop = rev7mix->paddingbottom = 10.f;
	auto rev7gain = new Slider(_appState, WRAP, 0, CENTER_ALIGN, REV7GAIN, 0, 1, dummyshimmer);
	rev7gain->paddingtop = rev7gain->paddingbottom = 10.f;


	dummy1reverbspacereverb1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV1T60));
	dummy1reverbspacereverb1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV1DAMP));
	dummy2reverbspacereverb1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV1MIX));
	dummy2reverbspacereverb1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV1GAIN));

	auto rev2t60 = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV2T60, 0, 1,
		dummy1reverbspacereverb2);
	rev2t60->paddingleft = rev2t60->paddingright = 25.f;

	// View *rev2damp = create_Knob_new("rev2damp", WRAP, 0, CENTER_ALIGN, REV2DAMP, 0, 0, dummy1reverbspacereverb2);
	auto rev2mix = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV2MIX, 0, 1,
		dummy2reverbspacereverb2);
	auto rev2gain = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV2GAIN, 0, 1,
		dummy2reverbspacereverb2);
	auto rev3t60 = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV3REF, 0, 1,
		dummy1reverbspacereverb3);
	auto rev3hp = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV3HPCUT, 0, 1,
		dummy1reverbspacereverb3);
	auto rev3lp = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV3LPCUT, 0, 1,
		dummy1reverbspacereverb3);
	auto rev3predel = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV3PREDELAY, 0, 1,
		dummy2reverbspacereverb3);
	auto rev3mix = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV3MIX, 0, 1,
		dummy2reverbspacereverb3);
	auto rev3gain = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV3GAIN, 0, 1,
		dummy2reverbspacereverb3);
	auto rev4t60 = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV4REF, 0, 1,
		dummy1reverbspacereverb4);
	auto rev4hp = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV4HPCUT, 0, 1,
		dummy1reverbspacereverb4);
	auto rev4lp = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV4LPCUT, 0, 1,
		dummy1reverbspacereverb4);
	auto rev4pre = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV4PREDELAY, 0, 1,
		dummy2reverbspacereverb4);
	auto rev4mix = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV4MIX, 0, 1,
		dummy2reverbspacereverb4);
	auto rev4gain = new Knob(_appState, WRAP, 0, CENTER_ALIGN, REV4GAIN, 0, 1,
		dummy2reverbspacereverb4);


	auto chorusdepth = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CHORUSINT, 0, 1,
		dummy1reverbspacereverb5);
	auto chorusfreq = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CHORUS2RATE, 0, 1,
		dummy1reverbspacereverb5);


	auto chorus2ntaps = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, CHORUS2TAPS, "TAPS");
	dummy1reverbspacereverb5->addChild(chorus2ntaps);

	auto chwidth = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CHORUS2WIDTH, 0, 1,
		dummy2reverbspacereverb5);
	auto chspread = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CHORUSPHASE, 0, 1,
		dummy2reverbspacereverb5);
	auto selecto10r_pvamps = new ButtonView<TextButtonFramed>(_appState, WRAP, 0,
		CENTER_ALIGN, VERTICAL, 2,
		makeStringSpan(vcoWaveforms),
		emptyFloats, CHORUSMOD);
	dummy2reverbspacereverb5->addChild(selecto10r_pvamps);
	selecto10r_pvamps->padding = 10.f;
	auto chdelay = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CHORUSDELAY, 0, 1,
		dummy3reverbspacereverb5);
	auto chmix = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CHORUSMIX, 0, 1,
		dummy3reverbspacereverb5);
	auto chgain = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CHORUSGAIN, 0, 1,
		dummy3reverbspacereverb5);

}
