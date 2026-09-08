#include "gui_internal.h"


void init_space_cross(tsl::AppState* _appState, Layout* root) {
	auto space_cross = new SpaceCross(_appState, WRAP, 0, CENTER_ALIGN, root);
	_DATA->views.spaces_fx[SPACE_CROSS_MAIN] = space_cross;

	space_cross->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	space_cross->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));
	auto dd = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_cross);

	auto dummy1 = new VerticalLayout(_appState, 6., RATIO_FROM_PARENT_View,
		START_ALIGN, dd);
	dummy1->paddingtop = dummy1->paddingbottom = 5.;

	auto cross_type = new Selector2(_appState, 66.6666, PERCENTAGE_FROM_PARENT_View, START_ALIGN,
		ARRAY_LEN(cross_types), makeStringSpan(cross_types),
		emptyFloats, ASCROSS,
		0,
		1, "ALGORITHM");
	cross_type->paddingleft = 10. * 50. / 66.6666;
	dummy1->addChild(cross_type);


	auto fftsize = new Selector2(_appState, 33.3333333333, PERCENTAGE_FROM_PARENT_View, END_ALIGN,
		ARRAY_LEN(fft_sizes), fft_sizes, fft_size_values, FFT_SIZE, 0, 1,
		"FFT SIZE");
	dummy1->addChild(fftsize);

	auto dummy2 = new HorizontalLayout(_appState, 6., RATIO_FROM_PARENT_View, END_ALIGN, dd);

	auto tv_cross_dest = new TitleView(_appState, "MODULATOR: TRACK2", CENTER_ALIGN, dummy2);
	_DATA->views.tv_cross_dest = tv_cross_dest;

	auto space_cross_polar = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_CROSS_POLAR] = space_cross_polar;
	auto polard = new VerticalLayout(_appState, 4., RATIO_FROM_PARENT_View, START_ALIGN,
		space_cross_polar);
	auto cross_mag = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(cross_mag_types), makeStringSpan(cross_mag_types),
		emptyFloats, CROSS_MAG);

	cross_mag->paddingleft = 10.f;
	polard->addChild(cross_mag);

	auto cross_phase = new Selector2(_appState, WRAP, 0, CENTER_ALIGN, ARRAY_LEN(cross_phase_types),
		makeStringSpan(cross_phase_types), emptyFloats, CROSS_PHASE);

	//cross_phase->paddingleft = 15.f;
	polard->addChild(cross_phase);


	auto space_cep1 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_CROSS_CEP1] = space_cep1;
	auto cep11 = knobRow(_appState, space_cep1);
	auto cep1cutoffmodulator = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSCEP1CUTMOD, 0, 1,
		cep11);

	auto space_cep2 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_CROSS_CEP2] = space_cep2;
	auto cep21 = knobRow(_appState, space_cep2);

	auto cep2cutoffmodulator = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, CROSSCEP2CUTMOD, 0, 1, cep21);
	auto cep2cutoffsource = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSCEP2CUTSRC, 0, 1,
		cep21);
	auto cep2depth = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSCEP2DEPTH, 0, 1,
		cep21);

	auto space_interp = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_CROSS_INTERP] = space_interp;
	auto i1 = knobRow(_appState, space_interp);
	auto i2 = knobRow(_appState, space_interp);

	auto intcutoffmodulator = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, CROSSINTCUTMOD, 0, 1, i1);

	auto intcutoffsource = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, CROSSINTCUTSRC, 0, 1, i1);

	auto interpolcoef = new Knob(_appState, WRAP,
		0,
		CENTER_ALIGN, IPOL, 0, 1, i2);


	auto space_voc = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_CROSS_VOC] = space_voc;
	auto v1 = new VerticalLayout(_appState, 4., RATIO_FROM_PARENT_View, START_ALIGN, space_voc);
	// v1->paddingbottom = v1->paddingtop = 5.;
	auto v2 = knobRow(_appState, space_voc);

	auto vocoder_noise_mix = new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, VOC2HP, 0, 1, v2);

	auto vocoder_chans = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(vocoder_channels), vocoder_channels,
		vocoder_channels_values, VOC2CHANS);
	vocoder_chans->paddingleft = 5.;
	v1->addChild(vocoder_chans);


	auto space_conv = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dd->addChild(space_conv);
	space_conv->overlap = OVERLAP;
	_DATA->views.spaces_fx[SPACE_CROSS_CONV] = space_conv;
	auto c1 = knobRow(_appState, space_conv);
	c1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSCEP1EMPH));
	auto space_lpc = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dd->addChild(space_lpc);
	space_lpc->overlap = OVERLAP;
	_DATA->views.spaces_fx[SPACE_CROSS_LPC] = space_lpc;
	auto lpc1 = new VerticalLayout(_appState, 2., RATIO_FROM_PARENT_View, START_ALIGN, space_lpc);

	lpc1->paddingtop = lpc1->paddingbottom = 5.;
	lpc1->addChild(new PlusMinusControl(_STATE, WRAP, 0, START_ALIGN, CROSSLPCORDER));
	auto lpc2 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, lpc1);
	auto dummy11lpca = new HorizontalLayout(_appState, WRAP,
		0, START_ALIGN, lpc2);
	auto dummy11lpcb = new HorizontalLayout(_appState, WRAP,
		0, START_ALIGN, lpc2);
	auto lpcwhite = new CheckBox(_appState, VERTICAL, START_ALIGN, CROSSLPCW);
	dummy11lpca->addChild(lpcwhite);
	//auto lpcemph = new CheckBox(_appState, VERTICAL, START_ALIGN, LPCVOCEMPH);
	//dummy11lpcb->addChild(lpcemph);

	// TRANSPORT. Reserved slot DUMMYCROSS2 -- the sub-space is picked by
	// SPACE_CROSS_POLAR + ASCROSS, so the view for algorithm 7 has to sit at the
	// eighth entry after SPACE_CROSS_POLAR.
	auto space_trans = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_DUMMYCROSS2] = space_trans;
	auto tr1 = knobRow(_appState, space_trans);
	auto tr2 = knobRow(_appState, space_trans);
	tr1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSTRANSAMT));
	tr1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSTRANSSMOOTH));
	tr2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSTRANSFLOOR));

	// PARTIAL STACK. Reserved slot DUMMYCROSS3.
	auto space_stack = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_DUMMYCROSS3] = space_stack;
	auto st1 = new VerticalLayout(_appState, 2., RATIO_FROM_PARENT_View, START_ALIGN, space_stack);
	st1->paddingtop = st1->paddingbottom = 5.;
	st1->addChild(new PlusMinusControl(_STATE, WRAP, 0, START_ALIGN, CROSSSTACKN));
	auto st2 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, st1);
	st2->addChild(new CheckBox(_appState, VERTICAL, START_ALIGN, CROSSSTACKFORM));
	auto st3 = knobRow(_appState, space_stack);
	st3->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSSTACKTILT));

	// SPECTRAL DUCK. Reserved slot DUMMYCROSS4 (algorithm 9).
	auto space_duck = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_DUMMYCROSS4] = space_duck;
	auto dk1 = new VerticalLayout(_appState, 4., RATIO_FROM_PARENT_View, START_ALIGN, space_duck);
	auto dkmode = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(cross_duck_modes), makeStringSpan(cross_duck_modes),
		emptyFloats, CROSSDUCKMODE);
	dkmode->paddingleft = 10.f;
	dk1->addChild(dkmode);
	auto dk2 = knobRow(_appState, space_duck);
	auto dk3 = knobRow(_appState, space_duck);
	dk2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSDUCKTHRESH));
	dk2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSDUCKSOFT));
	dk3->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSDUCKSMOOTH));
	dk3->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CROSSDUCKFLOOR));
}
