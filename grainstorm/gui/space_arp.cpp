#include "gui_internal.h"


void init_space_arp(Layout* root) {
	auto _appState = root->_appState;
	auto space_arp = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, root, true);
	_DATA->views.spaces_fx[SPACE_ARP] = space_arp;
	space_arp->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto space_arp2 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_arp);

	space_arp->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));
	auto arp1 = new VerticalLayout(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN, space_arp2);
	auto kn = new Knob(_appState, WRAP, 0, CENTER_ALIGN, ARP_INTERVAL, 0, 1, arp1);
	kn->padding = 5.f;
	auto pl = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, ARP_CYCLES);
	arp1->addChild(pl);
	pl->padding = 5.f;
	auto sel = new Selector2(_appState, WRAP, 0, CENTER_ALIGN, 4, makeStringSpan(cycle_modes),
		emptyFloats, ARP_CYCLEMODE);
	arp1->addChild(sel);
	sel->padding = 5.f;
#if defined(PLUGIN_MODE) || defined(OS_IOS)
	space_arp2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	space_arp2->addChild(new Divider(_STATE, START_ALIGN));
	space_arp2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	space_arp2->addChild(new TitleView(_appState, "SEQUENCER", START_ALIGN));

	space_arp2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	auto dummysyncdaw = new HorizontalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN);
	space_arp2->addChild(dummysyncdaw);
	dummysyncdaw->paddingtop = dummysyncdaw->paddingbottom = 10.f;
	dummysyncdaw->paddingleft = dummysyncdaw->paddingright = 5.f;

	auto transport = new CheckBoxWrapped(_appState, CENTER_ALIGN, SEQSYNCDAWTRANSPORT);
	dummysyncdaw->addChild(transport);
	auto dummyres = new HorizontalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN);
	space_arp2->addChild(dummyres);
	dummyres->paddingtop = dummyres->paddingbottom = 10.f;
	dummyres->paddingleft = dummyres->paddingright = 5.f;

	auto res = new CheckBoxWrapped(_appState, CENTER_ALIGN, SEQSYNCDAWRESETONSTART);
	dummyres->addChild(res);
#endif
}
