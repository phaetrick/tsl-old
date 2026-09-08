#include "gui_internal.h"


void init_space_graingen(tsl::AppState* _appState, Layout* root) {
	auto space_phase_vocoder = new SpaceGrainGen(_appState, WRAP, 0, CENTER_ALIGN, root);
	_DATA->views.spaces_fx[SPACE_GRAINGEN] = space_phase_vocoder;

	auto dummy0 = new HorizontalLayout(_appState, WRAP, 0,
		START_ALIGN, space_phase_vocoder);
	dummy0->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN, dummy0);
	dummy1->size_reference = &_STATE->knob_height_total;
	dummy1->paddingleft = 5.f;

	auto dummy7 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy1);

	auto pv_type = new Selector2(_appState, 6.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		ARRAY_LEN(graingen_types), makeStringSpan(graingen_types),
		emptyFloats, ASGRAINGEN,
		0, 1, "ALGORITHM");
	// pv_type->paddingtop = 10.f;
	// pv_type->paddingbottom = 40.f;
	//pv_type->paddingleft = 5.f;
	dummy7->addChild(pv_type);
	dummy7->addChild(new View(_appState, 5, PERCENTAGE_FROM_MAIN_WINDOW, CENTER_ALIGN));
	dummy1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, ARP_SWING));
	dummy7->addChild(new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, GRAINGENMONO));
	auto dummy3 = new HorizontalLayout(_appState, WRAP, 0,
		START_ALIGN, dummy0);

	auto uni = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy3);
	uni->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	uni->overlap = OVERLAP;
	_DATA->views.spaces_fx[SPACE_GRAINGEN1] = _DATA->views.spaces_fx[SPACE_GRAINGEN2] = uni;
	auto uni1 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN,
		uni);
	uni1->size_reference = &_STATE->knob_height_total;
	auto densdev = new Knob(_appState, WRAP, 0, START_ALIGN, DENSDEV);
	uni1->addChild(densdev);
	auto bounce = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy3);
	bounce->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	bounce->overlap = OVERLAP;
	_DATA->views.spaces_fx[SPACE_GRAINGEN3] = bounce;
	auto dd = new VerticalLayout(_appState, WRAP, 0, START_ALIGN, bounce);
	dd->paddingleft = dd->paddingright = 5.f;
	dd->addChild(new CheckBox(_appState, HORIZONTAL, START_ALIGN, GRAINGENLOOP));
	dd->addChild(new CheckBox(_appState, HORIZONTAL, END_ALIGN, GRAINGENREST));

	auto a = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINGENHEIGHTA, 0, 1, bounce);
	a->paddingleft = a->paddingright = 5.f;
	a->paddingtop = a->paddingbottom = 20.f;
	auto b = new Slider(_appState, WRAP, 0, START_ALIGN, GRAINGENHEIGHTB, 0, 1, bounce);
	b->paddingleft = b->paddingright = 5.f;
	b->paddingtop = b->paddingbottom = 20.f;
	auto bounce1 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN, bounce);
	bounce1->size_reference = &_STATE->knob_height_total;
	bounce1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENVEL));

	auto spline = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy3, true);
	spline->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	_DATA->views.spaces_fx[SPACE_GRAINGEN4] = spline;
	auto spline1 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN, spline);
	spline1->size_reference = &_STATE->knob_height_total;
	spline1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENSPLINECPSA));
	spline1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENSPLINECPSB));
	auto spline2 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN, spline);
	spline2->size_reference = &_STATE->knob_height_total;
	spline2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENSPLINEDENSA));
	spline2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENSPLINEDENSB));

	auto env = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy3, true);
	_DATA->views.spaces_fx[SPACE_GRAINGEN5] = env;

	env->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	auto dummy1env = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN);
	dummy1env->size_reference = &_STATE->knob_height_total;
	//dummy1env->paddingtop = 10.f;
	// dummy1env->paddingbottom = 8.f;
	env->addChild(dummy1env);

	auto att1 = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENFOLATTACK, 0, 1, dummy1env);
	auto rel1 = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENFOLDECAY, 0, 1, dummy1env);
	auto gain1 = new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENFOLGAIN, 0, 1, dummy1env);

	auto env1 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN,
		env);
	env1->size_reference = &_STATE->knob_height_total;
	env1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENFOLDENSA));
	env1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENFOLDENSB));

	auto chimes = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy3, true);
	_DATA->views.spaces_fx[SPACE_GRAINGEN6] = chimes;
	chimes->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto chimes1 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN, chimes);
	chimes1->size_reference = &_STATE->knob_height_total;
	chimes1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENCHIMEWIND));
	chimes1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENCHIMEGUST));
	auto chimes2 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN, chimes);
	chimes2->size_reference = &_STATE->knob_height_total;
	chimes2->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENCHIMESPAN));

	// HorizontalLayout is a COLUMN: the selector is its own 6.5% row (same
	// recipe as the ALGORITHM selector above), the three knobs share one
	// knob-height row below it.
	auto fig = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy3, true);
	_DATA->views.spaces_fx[SPACE_GRAINGEN7] = fig;
	fig->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto figshape = new Selector2(_appState, 6.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		ARRAY_LEN(graingen_figure_shapes), makeStringSpan(graingen_figure_shapes),
		emptyFloats, GRAINGENFIGSHAPE);
	figshape->paddingleft = 5.f;
	fig->addChild(figshape);
	fig->addChild(new View(_appState, 5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto fig1 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN, fig);
	fig1->size_reference = &_STATE->knob_height_total;
	fig1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENFIGNOTES));
	fig1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENFIGRANGE));
	fig1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENFIGREST));

	auto gls = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy3, true);
	_DATA->views.spaces_fx[SPACE_GRAINGEN8] = gls;
	gls->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto glsmode = new Selector2(_appState, 6.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		ARRAY_LEN(graingen_gliss_modes), makeStringSpan(graingen_gliss_modes),
		emptyFloats, GRAINGENGLSMODE);
	glsmode->paddingleft = 5.f;
	gls->addChild(glsmode);
	gls->addChild(new View(_appState, 5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto gls1 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN, gls);
	gls1->size_reference = &_STATE->knob_height_total;
	gls1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENGLSRANGE));
	gls1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENGLSTIME));
	gls1->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, GRAINGENGLSREST));

}
