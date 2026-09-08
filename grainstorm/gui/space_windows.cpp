#include "gui_internal.h"


inline constexpr std::string_view envs[] = { "ENV1", "ENV2" };

void init_space_windows(tsl::AppState* _appState, Layout* root) {
	auto space_envelopes = new SpaceGrainEnv(_appState, WRAP, 0, CENTER_ALIGN, root);
	_DATA->views.active_spaces_array[SPACE_ENVELOPES] = space_envelopes;

	auto dummy_title_windows = new VerticalLayout(_appState, 15., RATIO_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_envelopes->addChild(dummy_title_windows);


	auto button_space_a = new ButtonEnvelopeSwitch(_appState, WRAP, 0, CENTER_ALIGN, "ENV1",
		SPACE_GRAINENV1);
	dummy_title_windows->addChild(button_space_a);
	_DATA->views.button_analysis = button_space_a;


	auto button_space_s = new ButtonEnvelopeSwitch(_appState, WRAP, 0, CENTER_ALIGN, "ENV2",
		SPACE_GRAINENV2);
	dummy_title_windows->addChild(button_space_s);
	_DATA->views.button_synthesis = button_space_s;

	auto divider_envelopes = new Divider(_appState, START_ALIGN);
	space_envelopes->addChild(divider_envelopes);
	_DATA->views.divider_envelopes = divider_envelopes;

	auto space_grainenv2 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_envelopes,
		true);
	_DATA->views.space_grainenv2 = space_grainenv2;

	space_grainenv2->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	space_grainenv2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));


	auto enveditor = new EnvEditor<GrainEnvEditor>(_appState, 2.f, RATIO_FROM_PARENT_View,
		START_ALIGN);
	space_grainenv2->addChild(enveditor);


	auto dd = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_grainenv2);

	auto test = new ButtonView<EnvelopeButton>(_appState, 6.6666, PERCENTAGE_FROM_MAIN_WINDOW,
		CENTER_ALIGN,
		HORIZONTAL, 3, makeStringSpan(editorcurvenames),
		emptyFloats, GRAINCURVE);
	test->paddingleft = test->paddingright = 5.f;
	test->paddingtop = test->paddingbottom = 10.f;
	dd->addChild(test);

	//  space_grainenv2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));


	auto dummycontrolsenveditor = new VerticalLayout(_appState, VALUE_FROM_POINTER,
		VALUE_FROM_POINTER,
		END_ALIGN, space_grainenv2);
	dummycontrolsenveditor->size_reference = &_STATE->knob_height_total;
	dummycontrolsenveditor->paddingbottom = 10.f;
	auto enveditor_nsegments = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN,
		GRAINNSEGS);

	auto dummycontrolsenveditor2 = new HorizontalLayout(_appState, HORIZONTAL, WRAP, CENTER_ALIGN);
	dummycontrolsenveditor->addChild(dummycontrolsenveditor2);

	auto dummycontrolsenveditor3 = new HorizontalLayout(_appState, WRAP, 0,
		START_ALIGN);
	dummycontrolsenveditor2->addChild(dummycontrolsenveditor3);

	auto je = new CheckBox(_appState, VERTICAL, START_ALIGN, GRAINJOIN);
	dummycontrolsenveditor3->addChild(je);
	je->paddingbottom = 10.f;

	/*
	auto selector_envedit = new Selector1<WaveformChooser>(5.f,
														   PERCENTAGE_FROM_MAIN_WINDOW,
														   END_ALIGN, ARRAY_LEN(editfuncs),
														   editfuncs, nullptr, GRAINCURVE, 0, 1,
														   callback_editfunc);
	dummycontrolsenveditor2->addChild(test);
*/
	auto envedit_quant = new PlusMinusControlQuant(_appState, WRAP, 0, CENTER_ALIGN, GRAINQUANT,
		"QUANT");
	dummycontrolsenveditor->addChild(envedit_quant);


	auto space_grainenv1 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_envelopes,
		true);
	_DATA->views.space_grainenv1 = space_grainenv1;

	auto dummydiv1 = new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN);
	space_grainenv1->addChild(dummydiv1);
	auto vertenv1 = new VerticalLayout(_appState, 22.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		space_grainenv1);
	auto hozenv1 = new HorizontalLayout(_appState, 66., PERCENTAGE_FROM_PARENT_View, START_ALIGN,
		vertenv1);
	auto hozenv2 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, vertenv1);

	auto selecto10r_pvamps = new ButtonView<TextButtonFramed>(_appState, WRAP, 0,
		CENTER_ALIGN, VERTICAL, 2,
		makeStringSpan(envs),
		emptyFloats, GRAINENVSPACE);
	auto& buttons = selecto10r_pvamps->getButtons();
	for (auto& b : buttons)b.drawRoundedRect = false;
	hozenv2->addChild(selecto10r_pvamps);
	hozenv2->addChild(new View(_appState, 3., RATIO_FROM_PARENT_View, END_ALIGN));

	auto s_a_inner = new Selector2(_appState, 10., PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN, ARRAY_LEN(envelopesnames),
		makeStringSpan(envelopesnames), emptyFloats, ENVINNER,
		GRAINENVSPACE, ENVINNER2 - ENVINNER,
		"INNER WINDOW");
	s_a_inner->paddingleft = 5.f;
	s_a_inner->paddingright = 5.f;
	hozenv1->addChild(s_a_inner);
	
	auto dummydiv2 = new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN);
	hozenv1->addChild(dummydiv2);

	auto s_a_outer = new Selector2(_appState, WRAP, 0,
		START_ALIGN, ARRAY_LEN(envelopesnames),
		makeStringSpan(envelopesnames), emptyFloats, ENVOUTER,
		GRAINENVSPACE, ENVOUTER2 - ENVOUTER,
		"OUTER WINDOW");
	s_a_outer->paddingleft = 5.f;
	s_a_outer->paddingright = 5.f;
	hozenv1->addChild(s_a_outer);
	

	space_grainenv1->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	auto dummy1analysis = new VerticalLayout(_appState, VALUE_FROM_POINTER,
		VALUE_FROM_POINTER, START_ALIGN);
	dummy1analysis->size_reference = &_STATE->knob_height_total;

	space_grainenv1->addChild(dummy1analysis);
	space_grainenv1->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));


	auto analysis_outer_cycles = new Knob(_appState, WRAP, 0,
		START_ALIGN, AOUTERCYCLES, GRAINENVSPACE,
		AOUTERCYCLES2 - AOUTERCYCLES);
	//analysis_outer_cycles->padding = 3.f;
	dummy1analysis->addChild(analysis_outer_cycles);
	_STATE->parameters[AOUTERCYCLES2].view = analysis_outer_cycles;

	auto analysis_outer_depth = new Knob(_appState, WRAP, 0,
		START_ALIGN, AOUTERDEPTH, GRAINENVSPACE,
		AOUTERDEPTH2 - AOUTERDEPTH);
	//analysis_outer_depth->padding = 3.f;
	dummy1analysis->addChild(analysis_outer_depth);
	_STATE->parameters[AOUTERDEPTH2].view = analysis_outer_depth;

	auto intpol = new Knob(_appState, WRAP, 0,
		START_ALIGN, GRAINENVINTERPOL);
	//analysis_outer_depth->padding = 3.f;
	dummy1analysis->addChild(intpol);


	space_grainenv1->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));


	auto env_window_analysis = new EnvelopeWindow(_appState, WRAP, 0, CENTER_ALIGN);
	env_window_analysis->paddingleft = 5.f;
	env_window_analysis->paddingright = 5.f;
	space_grainenv1->addChild(env_window_analysis);
	_STATE->parameters[GRAINENVELOPEVIEW].view = env_window_analysis;
}
