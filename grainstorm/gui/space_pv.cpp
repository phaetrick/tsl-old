#include "gui_internal.h"


void init_space_pv(tsl::AppState* _appState, Layout* root) {
	auto space_phase_vocoder = new SpacePv(_appState, WRAP, 0, CENTER_ALIGN, root);
	_DATA->views.spaces_fx[SPACE_PV_MAIN] = space_phase_vocoder;

	space_phase_vocoder->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	space_phase_vocoder->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dd = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_phase_vocoder);
	auto dummy0 = new VerticalLayout(_appState, 6, RATIO_FROM_PARENT_View,
		START_ALIGN, dd);
	dummy0->paddingtop = dummy0->paddingbottom = 5.;
	dd->addChild(new View(_appState, 6, RATIO_FROM_PARENT_View, END_ALIGN));

	auto pv_type = new Selector2(_appState, 66.6666, PERCENTAGE_FROM_PARENT_View, START_ALIGN,
		ARRAY_LEN(pv_types), makeStringSpan(pv_types), emptyFloats, ASPV,
		0, 1, "ALGORITHM");
	pv_type->paddingleft = 5.f;
	dummy0->addChild(pv_type);


	auto fftsize = new Selector2(_appState, 33.3333, PERCENTAGE_FROM_PARENT_View, START_ALIGN,
		ARRAY_LEN(fft_sizes), makeStringSpan(fft_sizes), fft_size_values,
		FFT_SIZE, 0, 1,
		"FFT SIZE");
	dummy0->addChild(fftsize);


	auto pvphasecorr = new View(_appState, WRAP, 0,
		CENTER_ALIGN);
	dd->addChild(pvphasecorr);
	pvphasecorr->overlap = OVERLAP;
	_DATA->views.spaces_fx[SPACE_PV_PH1] = pvphasecorr;

	auto pvphasecorr2 = new View(_appState, HORIZONTAL, WRAP, 0,
		CENTER_ALIGN);
	dd->addChild(pvphasecorr2);
	pvphasecorr2->overlap = OVERLAP;
	_DATA->views.spaces_fx[SPACE_PV_PH2] = pvphasecorr2;

	auto pvstretch = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_PV_STRETCH] = pvstretch;
	auto pvstretch2 = knobRow(_appState, pvstretch);
	auto cutoffwarp = new Knob(_appState, WRAP, 0, CENTER_ALIGN, CUTSTRETCH, 0, 1, pvstretch2);
	auto warpcoeff = new Knob(_appState, WRAP, 0, CENTER_ALIGN, STRETCHCOEFF, 0, 1, pvstretch2);

	auto pvrnd = new View(_appState, WRAP, 0,
		CENTER_ALIGN);
	dd->addChild(pvrnd);
	pvrnd->overlap = OVERLAP;
	_DATA->views.spaces_fx[SPACE_PV_RND] = pvrnd;

	auto pvrob = new View(_appState, WRAP, 0, CENTER_ALIGN);
	dd->addChild(pvrob);
	pvrob->overlap = OVERLAP;
	_DATA->views.spaces_fx[SPACE_PV_ROB] = pvrob;

	auto pvosc = new HorizontalLayout(_appState, WRAP, 0,
		CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_PV_OSC] = pvosc;


	/* selector in its own row and the three knobs in one canonical row, the
	   same shape HARM/PERC and SPECTRAL SNAP use */
	auto dummyosc1 = new VerticalLayout(_appState, 4., RATIO_FROM_PARENT_View,
		START_ALIGN, pvosc);
	auto filttype = new Selector2(_appState, WRAP, 0, CENTER_ALIGN, ARRAY_LEN(oscnames),
		makeStringSpan(oscnames),
		emptyFloats, PVAMPS2PHASE, 0, 1, "PHASE");
	filttype->paddingleft = 10.;
	dummyosc1->addChild(filttype);

	auto dummyosc2 = knobRow(_appState, pvosc);
	auto pvamsp2ch = new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVAMPS2RANGE, 0, 1, dummyosc2);
	auto pvamps2mix = new Knob(_appState, WRAP,
		0,
		CENTER_ALIGN, PVAMPS2DRY, 0, 1, dummyosc2);
	auto pvamps2gain = new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVAMPS2WET, 0, 1, dummyosc2);


	auto pvspec = new HorizontalLayout(_appState, WRAP, 0,
		CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_PV_SPECWARP] = pvspec;
	/* no side padding on the sub-view itself any more - the sliders carry
	   SDSIDEPAD now, and padding both nested them in to 79px instead of the
	   61px every other slider in the plugin starts at */
	auto dummy = new HorizontalLayout(_appState, 75.f, PERCENTAGE_FROM_PARENT_View, START_ALIGN);
	pvspec->addChild(dummy);
	/* these had no padding at all and sat flush to the edges, and as WRAP rows
	   sharing 75% of the sub-view they came out 39px - taller than PP DELAY's
	   33, which is the ceiling. Same fixed 6% pitch as SPECTRAL RES, so every
	   slider in this space is the same 30px. */
	auto pvspecslider = [&](int id) {
		auto* sl = new Slider(_appState, 6.f, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN, id, 0, 1, dummy);
		sl->paddingleft = sl->paddingright = SDSIDEPAD;
		sl->paddingtop = sl->paddingbottom = 20.f;
	};
	pvspecslider(PVSPECBOUNDAIN);
	pvspecslider(PVSPECBOUNDBIN);
	pvspecslider(PVSPECBOUNDAOUT);
	pvspecslider(PVSPECBOUNDBOUT);
	auto dummycrevreverse = new VerticalLayout(_appState, 5., PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN,
		pvspec);
	auto dummycrevreverse2 = new HorizontalLayout(_appState, 99.f, PERCENTAGE_FROM_PARENT_View,
		START_ALIGN,
		dummycrevreverse);
	dummycrevreverse2->paddingbottom = 10.f;
	auto chbrev = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, PVSPECINV);
	dummycrevreverse2->addChild(chbrev);

	// HARM/PERC. Reserved slot DUMMYPV2 -- the sub-space is picked by
	// SPACE_PV_PH1 + ASPV, so the view for algorithm 7 has to sit at the eighth
	// entry after SPACE_PV_PH1.
	auto pvhpss = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_DUMMYPV2] = pvhpss;
	auto hp1 = new VerticalLayout(_appState, 4., RATIO_FROM_PARENT_View, START_ALIGN, pvhpss);
	auto hpmask = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(pv_hpss_mask_types), makeStringSpan(pv_hpss_mask_types),
		emptyFloats, PVHPSSMASK);
	hpmask->paddingleft = 10.f;
	hp1->addChild(hpmask);
	auto hp2 = knobRow(_appState, pvhpss);
	hp2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVHPSSMIX));
	hp2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVHPSSWIDTH));

	// CONTRAST. Reserved slot DUMMYPV3.
	auto pvcontrast = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_DUMMYPV3] = pvcontrast;
	auto ct1 = knobRow(_appState, pvcontrast);
	auto ct2 = knobRow(_appState, pvcontrast);
	ct1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVCONTRAST));
	ct1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVCONTRASTBANDS));
	ct2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVCONTRASTFLOOR));

	// SPECTRAL SNAP. Reserved slot DUMMYPV4.
	auto pvsnap = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_DUMMYPV4] = pvsnap;
	auto sn1 = new VerticalLayout(_appState, 4., RATIO_FROM_PARENT_View, START_ALIGN, pvsnap);
	auto snmode = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(pv_snap_modes), makeStringSpan(pv_snap_modes),
		emptyFloats, PVSNAPMODE);
	snmode->paddingleft = 10.f;
	sn1->addChild(snmode);
	auto sn2 = knobRow(_appState, pvsnap);
	sn2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVSNAPROOT));
	sn2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVSNAPAMT));

	// SPECTRAL RES. Reserved slot DUMMYPV5.
	auto pvres = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_DUMMYPV5] = pvres;
	auto rs1 = new VerticalLayout(_appState, 4., RATIO_FROM_PARENT_View, START_ALIGN, pvres);
	auto rsmode = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(pv_res_modes), makeStringSpan(pv_res_modes),
		emptyFloats, PVRESMODE);
	rsmode->paddingleft = 10.f;
	rs1->addChild(rsmode);
	/* SLIDERS, not knobs. This is the one sub-view here that cannot hold its
	   controls at the reference knob height: five params want two knob rows,
	   and a selector row plus two canonical rows is 84 + 300 = 384 in a 336px
	   sub-view. Squeezing them into one row of five would shrink the circles
	   instead (see knobRow). Sliders stack at the GENCREVERB pitch - five rows
	   is 240, so the whole view comes to 324 and fits - and a slider is one
	   size everywhere by construction.
	   (This is also what used to overflow: as three fixed 1/2-of-parent rows
	   under the selector it asked for 1.75 of the sub-view and ran 244px past
	   the bottom, past the space and off the window. Nothing repaints outside
	   a sub-view's rect, so that spill stayed on screen after switching
	   algorithm - the reported phase-vocoder artefact.) */
	auto pvresslider = [&](int id) {
		auto* sl = new Slider(_appState, 6.f, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN, id, 0, 1, pvres);
		sl->paddingleft = sl->paddingright = SDSIDEPAD;
		sl->paddingtop = sl->paddingbottom = 20.f;
		return sl;
	};
	pvresslider(PVRESROOT);
	pvresslider(PVRESDECAY);
	pvresslider(PVRESDAMP);
	pvresslider(PVRESINHARM);
	pvresslider(PVRESMIX);
	pvres->addChild(new View(_appState, WRAP, 0, CENTER_ALIGN));

	// SPECTRAL FREEZE. Reserved slot DUMMYPV6 (algorithm 11).
	auto pvfreeze = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_DUMMYPV6] = pvfreeze;
	auto fz1 = knobRow(_appState, pvfreeze);
	auto fz2 = knobRow(_appState, pvfreeze);
	fz1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVFREEZEAMT));
	fz1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVFREEZEPROB));
	fz2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVFREEZEMIX));

	// PH CORRECTION III is parked (see pv.cpp); its view is compiled out with
	// it. SPECTRAL FREEZE took DUMMYPV6, so on a return it gets DUMMYPV7 (the
	// slot is SPACE_PV_PH1 + algorithm index, and it would re-enter at 12).
#if 0
	// PH CORRECTION III. Reserved slot DUMMYPV7.
	auto pvph3 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dd, true);
	_DATA->views.spaces_fx[SPACE_DUMMYPV7] = pvph3;
	auto p31 = new VerticalLayout(_appState, 2., RATIO_FROM_PARENT_View, START_ALIGN, pvph3);
	p31->paddingtop = p31->paddingbottom = 5.;
	p31->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVPH3LOCK));
	p31->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVPH3TRANS));
	auto p32 = new VerticalLayout(_appState, 2., RATIO_FROM_PARENT_View, START_ALIGN, pvph3);
	p32->paddingtop = p32->paddingbottom = 5.;
	p32->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PVPH3PEAKS));
#endif
}
