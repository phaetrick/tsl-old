#include "gui_internal.h"


class VocTV : public TitleView {
public:
	VocTV(tsl::AppState* appState, int32_t al, Layout* par) : TitleView(appState, "z", al, par) {
	};

	void render(void* ctx) override {
		setText(_DATA->tracks[_STATE->active_track.load()]->modulatorName, false);
		TitleView::render(ctx);
	}
};

void init_space_fx(tsl::AppState* _appState, Layout* root) {
	Knob* knob1;
	View* chb, * chbh;
	auto space_fx = new SpaceFX(_appState, WRAP, 0, CENTER_ALIGN, root);

	_DATA->views.active_spaces_array[SPACE_FX] = space_fx;

	space_fx->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	space_fx->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto dummytitlefx = new VerticalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN, space_fx);

	auto fxtype = new Selector1<TitleView>(_appState, WRAP, 0, START_ALIGN,
		_STATE->dofastrender.load() ? ARRAY_LEN(fxtypes2)
		: ARRAY_LEN(fxtypes1),
		_STATE->dofastrender.load()
		? std::span<const std::string_view>(fxtypes2)
		: std::span<const std::string_view>(fxtypes1),
		_STATE->dofastrender.load() ? std::span<const float>(
			fxtypes2values)
		: std::span<const float>(
			fxtypes1values),
		ASFX);
	fxtype->paddingtop = fxtype->paddingbottom = 10.f;
	fxtype->paddingleft = 2.5f;
	dummytitlefx->addChild(fxtype);
	fxtype->middleclick = true;
	fxtype->titletext = "MONO EFFECTS";

	auto offbuttonfx = new BypassOffButton(_appState, SYM, 0, END_ALIGN,
		static_cast<const char*>(ICON_MD_POWER_SETTINGS_NEW),
		static_cast<const char*>(ICON_MD_POWER_SETTINGS_NEW),
		OFFFX);
	offbuttonfx->padding = 10.f;
	dummytitlefx->addChild(offbuttonfx);

	auto bypassbuttonfx = new BypassOffButton(_appState, SYM, 0, END_ALIGN,
		static_cast<const char*>(ICON_MD_RADIO_BUTTON_UNCHECKED),
		static_cast<const char*>(ICON_MD_RADIO_BUTTON_CHECKED),
		BYPASSFX);
	bypassbuttonfx->padding = 10.f;
	dummytitlefx->addChild(bypassbuttonfx);

	auto space_daw = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_INPUTGAIN] = space_daw;
	space_daw->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1daw = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_daw);
	dummy1daw->paddingtop = 5.f;
	dummy1daw->paddingbottom = 5.f;

	dummy1daw->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, INPUT_GAIN_DAW));
	space_daw->addChild(new View(_appState, WRAP, 0, CENTER_ALIGN));
	space_daw->addChild(new View(_appState, WRAP, 0, CENTER_ALIGN));

	auto space_mcmp1 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_MULTICOMP] = space_mcmp1;
	auto space_mcmp = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_mcmp1);
	space_mcmp->paddingleft = 5.f;

	space_mcmp->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	space_mcmp->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN));

	auto s = new Slider(_appState, WRAP, 0, CENTER_ALIGN, MULTICOMPGAIN, 0, 1, space_mcmp);
	s->paddingtop = s->paddingbottom = 10.f;
	s = new Slider(_appState, WRAP, 0, CENTER_ALIGN, MULTICOMPCROSS1, 0, 1, space_mcmp);
	s->paddingtop = s->paddingbottom = 10.f;
	s = new Slider(_appState, WRAP, 0, CENTER_ALIGN, MULTICOMPCROSS2, 0, 1, space_mcmp);
	s->paddingtop = s->paddingbottom = 10.f;


	auto sel1 = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		3,
		std::span<const std::string_view>{comps},
		std::span<const float>{},
		SPACEMULTICOMP);

	sel1->paddingtop = sel1->paddingbottom = 10.f;
	sel1->paddingleft = 2.5f;
	space_mcmp->addChild(sel1);
	s = new Slider(_appState, WRAP, 0, CENTER_ALIGN, MULTICOMPMAKE1, SPACEMULTICOMP, 1, space_mcmp);
	s->paddingtop = s->paddingbottom = 10.f;
	s = new Slider(_appState, WRAP, 0, CENTER_ALIGN, MULTICOMPTHR1, SPACEMULTICOMP, 1, space_mcmp);
	s->paddingtop = s->paddingbottom = 10.f;
	s = new Slider(_appState, WRAP, 0, CENTER_ALIGN, MULTICOMPRATIO1, SPACEMULTICOMP, 1,
		space_mcmp);
	s->paddingtop = s->paddingbottom = 10.f;
	s = new Slider(_appState, WRAP, 0, CENTER_ALIGN, MULTICOMPKNEE1, SPACEMULTICOMP, 1, space_mcmp);
	s->paddingtop = s->paddingbottom = 10.f;
	s = new Slider(_appState, WRAP, 0, CENTER_ALIGN, MULTICOMPATTACK1, SPACEMULTICOMP, 1,
		space_mcmp);
	s->paddingtop = s->paddingbottom = 10.f;
	s = new Slider(_appState, WRAP, 0, CENTER_ALIGN, MULTICOMPRELEASE1, SPACEMULTICOMP, 1,
		space_mcmp);
	s->paddingtop = s->paddingbottom = 10.f;

	auto space_modal = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_MODAL] = space_modal;


	space_modal->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	//space_modal->addChild(new View(_appState,  1.25, PERCENTAGE_FROM_MAIN_WINDOW,
	//                             END_ALIGN));


	auto dummymodalmiddle = new HorizontalLayout(_appState, WRAP, 0,
		CENTER_ALIGN);
	dummymodalmiddle->paddingtop = dummymodalmiddle->paddingbottom = 5.f;
	space_modal->addChild(dummymodalmiddle);

	auto dummymodalbottom = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummymodalbottom->paddingtop = dummymodalbottom->paddingbottom = 5.f;
	space_modal->addChild(dummymodalbottom);

	auto dummymodaltop = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummymodaltop->paddingtop = dummymodaltop->paddingbottom = 5.f;
	space_modal->addChild(dummymodaltop);

	auto gracpsmodalcpsgacps = new Slider(_appState, WRAP, 0, START_ALIGN, MODALFREQ, 0, 1,
		dummymodalmiddle);
	gracpsmodalcpsgacps->paddingleft = gracpsmodalcpsgacps->paddingright = 5.f;
	gracpsmodalcpsgacps->paddingtop = gracpsmodalcpsgacps->paddingbottom = 10.f;

	auto grainmodalq = new Slider(_appState, WRAP, 0, START_ALIGN, MODALQ, 0, 1, dummymodalmiddle);
	grainmodalq->paddingleft = grainmodalq->paddingright = 5.f;
	grainmodalq->paddingtop = grainmodalq->paddingbottom = 10.f;

	auto placeholdermodal3 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN);
	dummymodalmiddle->addChild(placeholdermodal3);

	auto grainmodalmode = new Selector1<TitleView>(_appState, 5., VIEW_COMPUTESIZE,
		CENTER_ALIGN, ARRAY_LEN(modal_names),
		modal_names,
		{}, MODALMODE, 0, 1);

	placeholdermodal3->addChild(grainmodalmode);

	auto dummymodalsettings2 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN);
	dummymodalsettings2->paddingleft = dummymodalsettings2->paddingright = 5.f;
	dummymodalsettings2->paddingtop = dummymodalsettings2->paddingbottom = 10.f;
	dummymodalbottom->addChild(dummymodalsettings2);

	auto dummyfolow = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummymodalsettings2->addChild(dummyfolow);
	chb = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, MODALFOLLOW);
	dummyfolow->addChild(chb);

	auto dummyhold = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	dummymodalsettings2->addChild(dummyhold);
	chbh = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, MODALHOLD);
	dummyhold->addChild(chbh);

	auto grainmodaldry = new Slider(_appState, WRAP, 0, START_ALIGN, MODALDRY, 0, 1,
		dummymodalbottom);
	grainmodaldry->paddingleft = grainmodaldry->paddingright = 5.f;
	grainmodaldry->paddingtop = grainmodaldry->paddingbottom = 10.f;

	auto grainmodalwet = new Slider(_appState, WRAP, 0, START_ALIGN, MODALWET, 0, 1,
		dummymodalbottom);
	grainmodalwet->paddingleft = grainmodalwet->paddingright = 5.f;
	grainmodalwet->paddingtop = grainmodalwet->paddingbottom = 10.f;


	auto space_fm2 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_FM2] = space_fm2;

	auto dummyfm2_0 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN, space_fm2);
	dummyfm2_0->paddingleft = 2.5f;
	dummyfm2_0->paddingright = 20.f;
	dummyfm2_0->paddingtop = dummyfm2_0->paddingbottom = 10.f;

	auto dummyfmunitpow_0 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummyfm2_0);
	auto cbfmpow_0 = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, FMPOW0);
	dummyfmunitpow_0->addChild(cbfmpow_0);

	auto fmtype_0 = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(fminstnames),
		fminstnames,
		fminstvalues, FMTYPE0);
	dummyfm2_0->addChild(fmtype_0);

	auto fmsemis_0 = new Slider(_appState, WRAP, 0, START_ALIGN, FMSEM0, 0, 1, space_fm2);
	fmsemis_0->paddingleft = fmsemis_0->paddingright = 5.f;
	fmsemis_0->paddingtop = fmsemis_0->paddingbottom = 10.f;

	auto fmgains_0 = new Slider(_appState, WRAP, 0, START_ALIGN, FMGAIN0, 0, 1, space_fm2);
	fmgains_0->paddingleft = fmgains_0->paddingright = 5.f;
	fmgains_0->paddingtop = fmgains_0->paddingbottom = 10.f;

	auto dummyfm2_1 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN, space_fm2);
	dummyfm2_1->paddingleft = 2.5f;
	dummyfm2_1->paddingright = 20.f;
	dummyfm2_1->paddingtop = dummyfm2_1->paddingbottom = 10.f;

	auto dummyfmunitpow_1 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummyfm2_1);
	auto cbfmpow_1 = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, FMPOW1);
	dummyfmunitpow_1->addChild(cbfmpow_1);

	auto fmtype_1 = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(fminstnames),
		fminstnames,
		fminstvalues, FMTYPE1);
	dummyfm2_1->addChild(fmtype_1);

	auto fmsemis_1 = new Slider(_appState, WRAP, 0, START_ALIGN, FMSEM1, 0, 1, space_fm2);
	fmsemis_1->paddingleft = fmsemis_1->paddingright = 5.f;
	fmsemis_1->paddingtop = fmsemis_1->paddingbottom = 10.f;

	auto fmgains_1 = new Slider(_appState, WRAP, 0, START_ALIGN, FMGAIN1, 0, 1, space_fm2);
	fmgains_1->paddingleft = fmgains_1->paddingright = 5.f;
	fmgains_1->paddingtop = fmgains_1->paddingbottom = 10.f;

	auto dummyfm2_2 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN, space_fm2);
	dummyfm2_2->paddingleft = 2.5f;
	dummyfm2_2->paddingright = 20.f;
	dummyfm2_2->paddingtop = dummyfm2_2->paddingbottom = 10.f;

	auto dummyfmunitpow_2 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummyfm2_2);
	auto cbfmpow_2 = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, FMPOW2);
	dummyfmunitpow_2->addChild(cbfmpow_2);

	auto fmtype_2 = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(fminstnames),
		fminstnames,
		fminstvalues, FMTYPE2);
	dummyfm2_2->addChild(fmtype_2);

	auto fmsemis_2 = new Slider(_appState, WRAP, 0, START_ALIGN, FMSEM2, 0, 1, space_fm2);
	fmsemis_2->paddingleft = fmsemis_2->paddingright = 5.f;
	fmsemis_2->paddingtop = fmsemis_2->paddingbottom = 10.f;

	auto fmgains_2 = new Slider(_appState, WRAP, 0, START_ALIGN, FMGAIN2, 0, 1, space_fm2);
	fmgains_2->paddingleft = fmgains_2->paddingright = 5.f;
	fmgains_2->paddingtop = fmgains_2->paddingbottom = 10.f;

	auto dummyfm2_3 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN, space_fm2);
	dummyfm2_3->paddingleft = 2.5f;
	dummyfm2_3->paddingright = 20.f;
	dummyfm2_3->paddingtop = dummyfm2_3->paddingbottom = 10.f;

	auto dummyfmunitpow_3 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummyfm2_3);
	auto cbfmpow_3 = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, FMPOW3);
	dummyfmunitpow_3->addChild(cbfmpow_3);

	auto fmtype_3 = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(fminstnames),
		fminstnames,
		fminstvalues, FMTYPE3);
	dummyfm2_3->addChild(fmtype_3);

	auto fmsemis_3 = new Slider(_appState, WRAP, 0, START_ALIGN, FMSEM3, 0, 1, space_fm2);
	fmsemis_3->paddingleft = fmsemis_3->paddingright = 5.f;
	fmsemis_3->paddingtop = fmsemis_3->paddingbottom = 10.f;

	auto fmgains_3 = new Slider(_appState, WRAP, 0, START_ALIGN, FMGAIN3, 0, 1, space_fm2);
	fmgains_3->paddingleft = fmgains_3->paddingright = 5.f;
	fmgains_3->paddingtop = fmgains_3->paddingbottom = 10.f;

	auto dummyfm2_4 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN, space_fm2);
	dummyfm2_4->paddingleft = 2.5f;
	dummyfm2_4->paddingright = 20.f;
	dummyfm2_4->paddingtop = dummyfm2_4->paddingbottom = 10.f;

	auto dummyfmunitpow_4 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummyfm2_4);
	auto cbfmpow_4 = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, FMPOW4);
	dummyfmunitpow_4->addChild(cbfmpow_4);

	auto fmtype_4 = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(fminstnames),
		fminstnames,
		fminstvalues, FMTYPE4);
	dummyfm2_4->addChild(fmtype_4);

	auto fmsemis_4 = new Slider(_appState, WRAP, 0, START_ALIGN, FMSEM4, 0, 1, space_fm2);
	fmsemis_4->paddingleft = fmsemis_4->paddingright = 5.f;
	fmsemis_4->paddingtop = fmsemis_4->paddingbottom = 10.f;

	auto fmgains_4 = new Slider(_appState, WRAP, 0, START_ALIGN, FMGAIN4, 0, 1, space_fm2);
	fmgains_4->paddingleft = fmgains_4->paddingright = 5.f;
	fmgains_4->paddingtop = fmgains_4->paddingbottom = 10.f;

	auto dummyfm2_5 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN, space_fm2);
	dummyfm2_5->paddingleft = 2.5f;
	dummyfm2_5->paddingright = 20.f;
	dummyfm2_5->paddingtop = dummyfm2_5->paddingbottom = 10.f;

	auto dummyfmunitpow_5 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummyfm2_5);
	auto cbfmpow_5 = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, FMPOW5);
	dummyfmunitpow_5->addChild(cbfmpow_5);

	auto fmtype_5 = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(fminstnames),
		fminstnames,
		fminstvalues, FMTYPE5);
	dummyfm2_5->addChild(fmtype_5);

	auto fmsemis_5 = new Slider(_appState, WRAP, 0, START_ALIGN, FMSEM5, 0, 1, space_fm2);
	fmsemis_5->paddingleft = fmsemis_5->paddingright = 5.f;
	fmsemis_5->paddingtop = fmsemis_5->paddingbottom = 10.f;

	auto fmgains_5 = new Slider(_appState, WRAP, 0, START_ALIGN, FMGAIN5, 0, 1, space_fm2);
	fmgains_5->paddingleft = fmgains_5->paddingright = 5.f;
	fmgains_5->paddingtop = fmgains_5->paddingbottom = 10.f;

	auto space_fm = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_FM] = space_fm;

	auto fmcps = new Slider(_appState, WRAP, 0, START_ALIGN, FMCPS, 0, 1, space_fm);
	fmcps->paddingleft = fmcps->paddingright = 5.f;
	fmcps->paddingtop = fmcps->paddingbottom = 10.f;

	View* fmdetune = new Slider(_appState, WRAP, 0, START_ALIGN, FMDETUNE, 0, 1, space_fm);
	fmdetune->paddingleft = fmdetune->paddingright = 5.f;
	fmdetune->paddingtop = fmdetune->paddingbottom = 10.f;

	View* fmI = new Slider(_appState, WRAP, 0, START_ALIGN, FMI, 0, 1, space_fm);
	fmI->paddingleft = fmI->paddingright = 5.f;
	fmI->paddingtop = fmI->paddingbottom = 10.f;

	View* fmR = new Slider(_appState, WRAP, 0, START_ALIGN, FMR, 0, 1, space_fm);
	fmR->paddingleft = fmR->paddingright = 5.f;
	fmR->paddingtop = fmR->paddingbottom = 10.f;


	View* fmr = new Slider(_appState, WRAP, 0, START_ALIGN, FMRATIO, 0, 1, space_fm);
	fmr->paddingleft = fmr->paddingright = 5.f;
	fmr->paddingtop = fmr->paddingbottom = 10.f;

	View* fmvibratea = new Slider(_appState, WRAP, 0, START_ALIGN, FMVIBRATEA, 0, 1, space_fm);
	fmvibratea->paddingleft = fmvibratea->paddingright = 5.f;
	fmvibratea->paddingtop = fmvibratea->paddingbottom = 10.f;

	View* fmvibdeptha = new Slider(_appState, WRAP, 0, START_ALIGN, FMVIBDEPTHA, 0, 1, space_fm);
	fmvibdeptha->paddingleft = fmvibdeptha->paddingright = 5.f;
	fmvibdeptha->paddingtop = fmvibdeptha->paddingbottom = 10.f;

	View* fmdet = new Slider(_appState, WRAP, 0, START_ALIGN, FMDETLR, 0, 1, space_fm);
	fmdet->paddingleft = fmdet->paddingright = 5.f;
	fmdet->paddingtop = fmdet->paddingbottom = 10.f;


	auto dummyfollow_fm = new VerticalLayout(_appState, WRAP,
		0, START_ALIGN, space_fm);
	dummyfollow_fm->paddingleft = dummyfollow_fm->paddingright = 5.f;
	dummyfollow_fm->paddingtop = dummyfollow_fm->paddingbottom = 10.f;

	auto fmdry = new Slider(_appState, WRAP, 0, START_ALIGN, FMDRY, 0, 1, space_fm);
	fmdry->paddingleft = fmdry->paddingright = 5.f;
	fmdry->paddingtop = fmdry->paddingbottom = 10.f;

	View* fmwet = new Slider(_appState, WRAP, 0, START_ALIGN, FMWET, 0, 1, space_fm);
	fmwet->paddingleft = fmwet->paddingright = 5.f;
	fmwet->paddingtop = fmwet->paddingbottom = 10.f;

	auto dummyfolowe = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummyfollow_fm);
	chb = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, FMFOLLOW);
	dummyfolowe->addChild(chb);

	auto dummyholde = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummyfollow_fm);
	chbh = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, FMHOLD);
	dummyholde->addChild(chbh);

	auto space_pdetect = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_PDETECT] = space_pdetect;

	space_pdetect->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1_pdetect = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_pdetect);
	dummy1_pdetect->paddingtop = 5.f;
	dummy1_pdetect->paddingbottom = 5.f;
	auto dummy3_pdetect = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_pdetect);
	dummy3_pdetect->paddingtop = 5.f;
	dummy3_pdetect->paddingbottom = 5.f;
	auto dummy2_pdetect = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_pdetect);
	dummy2_pdetect->paddingtop = 5.f;
	dummy2_pdetect->paddingbottom = 5.f;

	auto pitchsource = new ButtonView<TextButtonFramed>(_appState, 5.f, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN, HORIZONTAL,
		ARRAY_LEN(tracknames),
		makeStringSpan(tracknames),
		emptyFloats,
		PITCHDETECTFXTRACK);
	dummy2_pdetect->addChild(pitchsource);

	auto pdetecttv = new PDectectTV(_appState, CENTER_ALIGN, dummy2_pdetect);
	pdetecttv->paddingtop = pdetecttv->paddingbottom = 10.f;
	pdetecttv->paddingleft = pdetecttv->paddingright = 2.5f;

	dummy1_pdetect->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PDETECTSMOOTH2));
	dummy1_pdetect->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PDETECTPRELP));
	dummy3_pdetect->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PDETECTA));
	dummy3_pdetect->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PDETECTB));
	dummy3_pdetect->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PDETECTTRANSPOSE));

	auto space_bowed = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_BOWED] = space_bowed;
	space_bowed->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto boweddum = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_bowed);
	auto dummy1bowed = new VerticalLayout(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN,
		boweddum);
	dummy1bowed->paddingtop = dummy1bowed->paddingbottom = 5.;
	auto dummy2bowed = new VerticalLayout(_appState, 6.75,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN, boweddum);
	dummy2bowed->padding = 5.f;
	auto dummy3bowed = new VerticalLayout(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN,
		boweddum);
	dummy3bowed->paddingtop = dummy3bowed->paddingbottom = 5.f;
	dummy1bowed->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, BOWEDCPS));
	dummy1bowed->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BOWEDPOS));
	dummy3bowed->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BOWEDDRY));
	dummy3bowed->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BOWEDWET));
	auto dummy2boweda = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy2bowed);
	auto dummy2bowedb = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, dummy2bowed);
	dummy2boweda->addChild(new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, BOWEDFOLLOW));
	dummy2bowedb->addChild(new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, BOWEDHOLD));

	auto space_specdel2 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);

	_DATA->views.spaces_fx[SPACE_SPECDEL2] = space_specdel2;

	auto sdph = new EnvelopeEditorWindow(_appState, 45., PERCENTAGE_FROM_PARENT_View, START_ALIGN,
		SPECDEL2X0, HORIZONTAL);
	space_specdel2->addChild(sdph);
	auto dummyspecdel2 = new HorizontalLayout(_appState, WRAP,
		0, END_ALIGN, space_specdel2);
	auto sdc = new EnvelopeWindowControls(_appState, 5., PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		SPECDEL2X0);
	dummyspecdel2->addChild(sdc);
	sdc->paddingleft = sdc->paddingright = 2.5f;

	auto dummyspecdel1 = new VerticalLayout(_appState, WRAP, 0, START_ALIGN, dummyspecdel2);
	dummyspecdel1->paddingleft = dummyspecdel1->paddingright = 5.f;

	auto dummyje = new HorizontalLayout(_appState, 99., PERCENTAGE_FROM_PARENT_View, START_ALIGN,
		dummyspecdel1);
	dummyje->paddingbottom = dummyje->paddingtop = 10.f;
	auto chbvcovcf = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, SPECDEL2RND);
	dummyje->addChild(chbvcovcf);

	auto sddel = new Slider(_appState, WRAP, 0, START_ALIGN, SPECDEL2DEL, 0, 1, dummyspecdel2);
	sddel->paddingleft = sddel->paddingright = 5.f;
	sddel->paddingtop = sddel->paddingbottom = 10.f;

	View* sdfb = new Slider(_appState, WRAP, 0, START_ALIGN, SPECDEL2FB, 0, 1, dummyspecdel2);
	sdfb->paddingleft = sdfb->paddingright = 5.f;
	sdfb->paddingtop = sdfb->paddingbottom = 10.f;

	View* sdmix = new Slider(_appState, WRAP, 0, START_ALIGN, SPECDEL2MIX, 0, 1, dummyspecdel2);
	sdmix->paddingleft = sdmix->paddingright = 5.f;
	sdmix->paddingtop = sdmix->paddingbottom = 10.f;

	View* sdgain = new Slider(_appState, WRAP, 0, START_ALIGN, SPECDEL2GAIN, 0, 1, dummyspecdel2);
	sdgain->paddingleft = sdgain->paddingright = 5.f;
	sdgain->paddingtop = sdgain->paddingbottom = 10.f;

	//BITCRUSH
	auto space_bc = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_BITCRUSHER] = space_bc;
	space_bc->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1bc = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bc);
	dummy1bc->paddingtop = 5.f;
	dummy1bc->paddingbottom = 5.f;
	auto dummy2bc = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bc);
	dummy2bc->paddingtop = 5.f;
	dummy2bc->paddingbottom = 5.f;
	auto dummy3bc = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bc);
	dummy3bc->paddingtop = 5.f;
	dummy3bc->paddingbottom = 5.f;

	dummy1bc->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, BCSR));
	dummy1bc->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, BCBITS));
	dummy1bc->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, BCTONE));
	dummy2bc->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, BCMIX));
	dummy2bc->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, BCGAIN));


	//BITCRUSH
	auto space_liveconv = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_LIVECONV] = space_liveconv;
	space_liveconv->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1liveconv = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_liveconv);
	dummy1liveconv->paddingtop = 5.f;
	dummy1liveconv->paddingbottom = 5.f;
	auto dummy2liveconv = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_liveconv);
	dummy2liveconv->paddingtop = 5.f;
	dummy2liveconv->paddingbottom = 5.f;
	auto dummy3liveconv = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_liveconv);
	dummy3liveconv->paddingtop = 5.f;
	dummy3liveconv->paddingbottom = 5.f;

	dummy1liveconv->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, LIVECONVSIZE));
	dummy1liveconv->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, LIVECONVDECAY));
	dummy2liveconv->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, LIVECONV1));
	dummy2liveconv->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, LIVECONV2));
	dummy3liveconv->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, LIVECONVDRY));
	dummy3liveconv->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, LIVECONVWET));
	auto ttt = new Selector2(_appState, WRAP, 0,
		CENTER_ALIGN, 4, tracknames, {}, LIVECONVUPDATE, 0, 1, "MOD");
	ttt->paddingleft = 5.f;

	dummy1liveconv->addChild(ttt);
	//PVAMPS
	/* GENCREVERB, BOTH spaces: one horizontal slider per control, one control
	   per row, every row SDROWPITCH tall with the slider itself SDROWBOX of
	   the window inside it. The pitch is fixed rather than WRAP because a WRAP
	   row is the leftover space divided by the number of rows, and the
	   algorithm sub-views hold anywhere from zero to five sliders - the height
	   of a slider would then depend on which algorithm is selected.
	   Why the box is set by padding instead of just letting Slider::init do
	   it: init only centres its own 5%-of-window bar inside the row once the
	   ROW is at least 6.67% tall, and the first space measures 65.25% of the
	   window with ten rows to place, so no pitch that fits can reach that
	   threshold - the sliders would keep the raw row height instead. Padding
	   the row down gets there deterministically at any pitch, in both spaces,
	   and the leftover is the gap between rows.
	   SDROWPAD is the 20% the PING PONG DELAY sliders already use, so a bar is
	   .6 of its row there and here. That also keeps this space's sliders from
	   ever out-sizing that one AT ANY WINDOW SIZE, which is the house rule:
	   PP DELAY splits (65.25 - 12)% over eight rows = 6.66% each, .6 of that
	   is 3.99% of the window, against 3.6% here. Measured at 800px: PP DELAY
	   33px, these 30px, in both spaces. (Whole-pixel truncation of each
	   padding means the arithmetic lands a pixel or two off - harmless, since
	   every row runs the identical computation and so they all agree.) */
	constexpr float SDROWPITCH = 6.f;
	constexpr float SDROWPAD = 20.f;
	auto sdslider = [&](int id, Layout* par) {
		auto sl = new Slider(_appState, SDROWPITCH, PERCENTAGE_FROM_MAIN_WINDOW,
			START_ALIGN, id, 0, 1, par);
		sl->paddingleft = sl->paddingright = SDSIDEPAD;
		sl->paddingtop = sl->paddingbottom = SDROWPAD;
		return sl;
	};

	auto space_specdel = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_SPECDEL] = space_specdel;
	space_specdel->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	/* the algorithm selector lives in the second space (GENCREVERB ALG) now,
	   next to the per-algorithm params */
	sdslider(SPECDELDEL, space_specdel);
	/* global IR shaping - applied to every algorithm after generation */
	sdslider(SPECDELDECAY, space_specdel);
	sdslider(SPECDELDAMPHF, space_specdel);

	sdslider(SPECDELBOUNDA, space_specdel);
	sdslider(SPECDELBOUNDB, space_specdel);

	/* How a new IR replaces the old one: RATE is the speed of the replacement
	   front (1 = one tap per sample, the rate a sound travels along the IR),
	   FRONT is where it stops, REVERSE fills from the tail backwards. */
	sdslider(SPECDELMORPHRATE, space_specdel);
	sdslider(SPECDELMORPHFRONT, space_specdel);

	/* REVERSE is built exactly like PING PONG DELAY's FREEZE/BACKW pair, so it
	   comes out the same size: a WRAP TextButton with padding 10 in a 6%
	   VerticalLayout row. The row has to be a VerticalLayout - that is the one
	   that calls computeWidth on its children - and the empty sibling is what
	   makes the width match: WRAP children split the row evenly, so FREEZE is
	   half of PP DELAY's row, and alone in here REVERSE would be twice that.
	   Measured: 39 x 164, same as FREEZE. */
	auto sdrevrow = new VerticalLayout(_appState, 6.,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN, space_specdel);
	_STATE->parameters[SPECDELMORPHREV].view = new TextButton(_appState, WRAP, 0,
		CENTER_ALIGN, "REVERSE", SPECDELMORPHREV, 0, 1);
	_STATE->parameters[SPECDELMORPHREV].view->padding = 10.f;
	sdrevrow->addChild(_STATE->parameters[SPECDELMORPHREV].view);
	sdrevrow->addChild(new View(_appState, WRAP, 0, CENTER_ALIGN));

	sdslider(SPECDELMIX, space_specdel);
	sdslider(SPECDELGAIN, space_specdel);

	/* fill dummy: soaks up whatever the fixed rows leave over. A plain View
	   paints its rect with the background, so the space stays fully covered
	   and nothing from a taller space can survive underneath it. */
	space_specdel->addChild(new View(_appState, WRAP, 0, CENTER_ALIGN));

	/* SECOND SPECTRAL DELAY SPACE (GENCREVERB ALG): the algorithm selector
	   plus the active algorithm's own params. Same effect instance as
	   GENCREVERB, so it deliberately has NO callbacks_fx_power entry - that
	   is what disables the off/bypass buttons here (SpaceFX::addRecursiveDraw
	   and onChangeSpaceFx both key off the null callback). */
	auto space_sdalg = new SpaceSpecDelAlg(_appState, WRAP, 0, START_ALIGN, space_fx);
	_DATA->views.spaces_fx[SPACE_SPECDELALG] = space_sdalg;
	space_sdalg->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	/* the selector row is FIXED, not WRAP. As a WRAP child it shared the space
	   with the sub-views and each got half, which is less than the five fixed
	   rows LONGVERB needs. Selector2 shrinks itself to title + value (6.25% of
	   the window), so 6.75 is its own box plus a little air. */
	auto sdselcol = new VerticalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN, space_sdalg);
	sdselcol->paddingtop = sdselcol->paddingbottom = 5.f;
	/* the span has to be trimmed too, not just the count: numelements only
	   bounds the arrow and wheel stepping, while the drop-down list is built
	   from the whole names span in Selector's ctor (rv = RecyclerView(names)),
	   so a full span leaves the parked algorithms one click away. */
	auto sdmode = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		specdelmodes_shown, makeStringSpan(specdelmodes).first(specdelmodes_shown),
		{}, SPECDELMODE, 0, 1,
		"ALGORITHM");
	sdmode->paddingleft = sdmode->paddingright = SDSIDEPAD;
	sdselcol->addChild(sdmode);

	/* one row of air between the selector and the algorithm's sliders, so the
	   sub-view reads as its own group rather than a fourth line of the
	   selector block */
	space_sdalg->addChild(new View(_appState, SDROWPITCH,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	/* One overlapping sub-view per algorithm, picked by SpaceSpecDelAlg /
	   onChangeSpecDelMode through the specdel_subspaces map. Same sliders at
	   the same pitch as the first space, top-aligned, so a row sits at the
	   same y whichever algorithm is selected. Each sub-view ends in a fill
	   dummy: the sub-views all get the same rect from the parent, and the
	   dummy keeps that rect fully painted no matter how few sliders the
	   algorithm has, so switching from a five-slider view to a one-slider one
	   cannot leave the extra rows standing. */
	auto sdsub = [&](int spaceId, std::initializer_list<int> sliderIds) {
		auto sub = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN,
			space_sdalg, true);
		_DATA->views.spaces_fx[spaceId] = sub;
		for (auto id : sliderIds)
			sdslider(id, sub);
		sub->addChild(new View(_appState, WRAP, 0, CENTER_ALIGN));
		return sub;
	};
	sdsub(SPACE_SPECDELSUB_RAMP, { SPECDELSHAPE });
	sdsub(SPACE_SPECDELSUB_RAND, { SPECDELRANDEVO });
	sdsub(SPACE_SPECDELSUB_GAUSS, { SPECDELGAUSSEVO });
	sdsub(SPACE_SPECDELSUB_FB, { SPECDELFB, SPECDELDAMP });
	sdsub(SPACE_SPECDELSUB_STEP, { SPECDELSTEPS });
	sdsub(SPACE_SPECDELSUB_RIPPLE, { SPECDELRIPPLES, SPECDELRIPPLEPH });
	sdsub(SPACE_SPECDELSUB_OCT, { SPECDELROOT });
	sdsub(SPACE_SPECDELSUB_DRIFT, { SPECDELDRIFTAMT, SPECDELDRIFTRATE });
	sdsub(SPACE_SPECDELSUB_BARBER, { SPECDELBARBN, SPECDELBARBSPEED });
	sdsub(SPACE_SPECDELSUB_TIDE, { SPECDELTIDESPEED });
	sdsub(SPACE_SPECDELSUB_VERB, { SPECDELVERBRT60, SPECDELVERBSIZE,
		SPECDELVERBDAMP, SPECDELVERBXFEED, SPECDELVERBEVO });
	/* PUREVERB has no controls of its own: RT60 is hardwired to its honest
	   ceiling (8x DELAY), EVOLVE to continuous, DAMP to the family default;
	   BOUND (first space) band-limits it. Empty sub-view. */
	sdsub(SPACE_SPECDELSUB_PUREVERB, {});
	auto sdadapt = sdsub(SPACE_SPECDELSUB_ADAPT, { SPECDELADEPTH, SPECDELARATE });
	/* INVERT is a bool, so it stays a checkbox - in a full-pitch row of its
	   own so it lands on the same rhythm as the two sliders above it. The row
	   is START_ALIGNed like them, so it queues up after them even though it is
	   added after the fill dummy (the dummy is centre-aligned and just takes
	   whatever the fixed rows leave). */
	auto sdadaptrow = new HorizontalLayout(_appState, SDROWPITCH,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN, sdadapt);
	sdadaptrow->paddingleft = 2.5f;
	sdadaptrow->addChild(new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, SPECDELAINV));


	auto space_eq5 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_EQ5] = space_eq5;

	auto eq5frresponse = new EQ5Editor<>(_appState, 75., PERCENTAGE_FROM_PARENT_View, START_ALIGN,
		_DATA->updateRenderThreadeq5, _DATA->inputeq5);
	space_eq5->addChild(eq5frresponse);
	eq5frresponse->paddingleft = 2.5f;
	eq5frresponse->paddingright = 2.5f;
	auto eq5gain = new Slider(_appState, WRAP, 0, END_ALIGN, EQ5GAIN, 0, 1, space_eq5);
	eq5gain->paddingleft = 5.f;
	eq5gain->paddingright = 5.f;

	auto space_dyneq5 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_DYNEQ5] = space_dyneq5;

	auto dyneq5frresponse = new EQ5Editor<EQ5RenderWindow, DYNEQ5LOWCF>(_appState, 50.f,
		PERCENTAGE_FROM_PARENT_View,
		START_ALIGN,
		_DATA->updateRenderThreadeq5dyn,
		_DATA->inputeq5dyn);
	_STATE->parameters[DYNEQ5LOWCF].view = dyneq5frresponse;
	space_dyneq5->addChild(dyneq5frresponse);
	dyneq5frresponse->paddingleft = 2.5f;
	dyneq5frresponse->paddingright = 2.5f;
	auto dyneq5gain = new Slider(_appState, WRAP, 0, CENTER_ALIGN, DYNEQ5GAIN, 0, 1, space_dyneq5);
	dyneq5gain->padding = 5.f;

	auto dd = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_dyneq5);
	auto seldyn = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		5,
		dyns,
		std::span<const float>{},
		SPACEDYNEQ);
	seldyn->paddingtop = sel1->paddingbottom = 10.f;
	seldyn->paddingleft = 2.5f;
	dd->addChild(seldyn);
	auto dev = new DynEqView(_appState, WRAP, 0, CENTER_ALIGN);
	dev->paddingleft = 2.5;
	dev->paddingright = 5.f;
	dev->paddingtop = dev->paddingbottom = 10.f;
	dd->addChild(dev);
	auto vertdyn = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN);
	space_dyneq5->addChild(vertdyn);
	auto cbholder = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, vertdyn);
	auto cbdyn = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, DYNEQFILT0, SPACEDYNEQ, 1);
	cbholder->addChild(cbdyn);

	auto abovebelow = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		2,
		dyns2,
		std::span<const float>{},
		DYNEQBELOW0, SPACEDYNEQ, 1);

	vertdyn->addChild(abovebelow);
	

	auto s1 = new Slider(_appState, WRAP, 0, CENTER_ALIGN, DYNEQ5THR0, SPACEDYNEQ);
	s1->padding = 5.f;
	space_dyneq5->addChild(s1);
	auto s2 = new Slider(_appState, WRAP, 0, CENTER_ALIGN, DYNEQ5ATT0, SPACEDYNEQ);
	s2->padding = 5.f;
	space_dyneq5->addChild(s2);
	auto s3 = new Slider(_appState, WRAP, 0, CENTER_ALIGN, DYNEQ5REL0, SPACEDYNEQ);
	s3->padding = 5.f;
	space_dyneq5->addChild(s3);
	// NOTE: space_dyneq5 is a HorizontalLayout = a COLUMN, so every addChild here
	// costs a row, and the WRAP rows already sit under Slider::init's
	// windowHeight*0.0666 threshold - adding one more shrinks every control in
	// the panel instead of just appending below them.
	space_dyneq5->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, END_ALIGN));


	//PVAMPS
	auto space_pvamps = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_PVAMPS] = space_pvamps;
	space_pvamps->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1pvamps = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_pvamps);
	dummy1pvamps->paddingtop = 5.f;
	dummy1pvamps->paddingbottom = 5.f;
	auto dummy2pvamps = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_pvamps);
	dummy2pvamps->paddingtop = dummy2pvamps->paddingbottom = 5.f;
	auto dummy3pvamps = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_pvamps);
	dummy3pvamps->paddingtop = dummy3pvamps->paddingbottom = 5.f;
	auto selector_pvamps = new Selector2(_appState, WRAP, 0,
		CENTER_ALIGN, 4, vcoWaveforms,
		vcoModes, PVAMPSVCOWAVEFORM2, 0, 1, "WAV");
	selector_pvamps->paddingleft = 10.;
	dummy1pvamps->addChild(selector_pvamps);

	auto selector_pvamps2 = new Selector2(_appState, WRAP, 0, CENTER_ALIGN, ARRAY_LEN(oscnames),
		oscnames, {}, PVAMPSPHASE, 0, 1, "PHS");
	dummy1pvamps->addChild(selector_pvamps2);

	auto selector_pvamps3 = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(pvampsroutes),
		pvampsroutes, {}, PVAMPSROUTE, 0, 1, "ROUTE");
	dummy1pvamps->addChild(selector_pvamps3);

	dummy2pvamps->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, PVAMPSBOUNDA));
	dummy2pvamps->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, PVAMPSBOUNDB));
	dummy2pvamps->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, PVAMPSSMOOTH2));

	auto pvampsrange = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, PVAMPSRANGE);
	dummy3pvamps->addChild(pvampsrange);
	dummy3pvamps->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, PVAMPSDRY));
	dummy3pvamps->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, PVAMPSWET));

	//PITCHMAP
	auto space_pitchmap = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_PITCHMAP] = space_pitchmap;
#if GS_ENABLE_PITCHMAP2
	// PITCHMAP2 reads the same PMAP* params, so it shares the same page --
	// switching between the two effects keeps every knob in place for A/B.
	_DATA->views.spaces_fx[SPACE_PITCHMAP2] = space_pitchmap;
#endif
	space_pitchmap->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	/* SLIDERS, not knobs: nine params plus a selector and a counter do not fit
	   this page at the reference knob height (nine canonical rows would be
	   1350px in a 522px space), and shrinking the knobs to fit is what put
	   them out of step with every other panel.
	   The header row carries the scale selector and the SOURCES counter side
	   by side. 10% of the window is the smallest it can be: PlusMinusControl
	   stacks a textsize2 title over its value over textsize1 +/- buttons and
	   computes the value box as height - title - buttons, so under 20+20+40
	   the value has nowhere to go. */
	auto pmaphead = new VerticalLayout(_appState, 10., PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN, space_pitchmap);
	/* 2 * SDSIDEPAD, not SDSIDEPAD: padding is a percentage of the view's OWN
	   width and these two split the row, so half the row needs twice the
	   percentage to start at the same x as a full-width slider. This is the
	   same arithmetic that puts PP DELAY's FREEZE (padding 10, half a row) on
	   the same 61px as its sliders (padding 5, full row). */
	auto selector_pmap = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(pitchmap_scales), pitchmap_scales, {}, PMAPSCALE, 0, 1, "SCL");
	selector_pmap->paddingleft = selector_pmap->paddingright = 2.f * SDSIDEPAD;
	pmaphead->addChild(selector_pmap);
	auto pmapsources = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN,
		PMAPSOURCES);
	pmapsources->paddingleft = pmapsources->paddingright = 2.f * SDSIDEPAD;
	pmaphead->addChild(pmapsources);

	/* WRAP sliders sharing what is left, exactly the PING PONG DELAY idiom.
	   This page has no sub-views that come and go, so there is nothing here
	   that needs a fixed pitch - the row count never changes. */
	auto pmapslider = [&](int id) {
		auto* sl = new Slider(_appState, WRAP, 0, START_ALIGN, id, 0, 1,
			space_pitchmap);
		sl->paddingleft = sl->paddingright = SDSIDEPAD;
		sl->paddingtop = sl->paddingbottom = 20.f;
	};
	pmapslider(PMAPCPS);
	pmapslider(PMAPAMT);
	pmapslider(PMAPLO);
	pmapslider(PMAPHI);
	pmapslider(PMAPPURIFY);
	pmapslider(PMAPFORMANT);
	pmapslider(PMAPGLIDE);
	pmapslider(PMAPDRY);
	pmapslider(PMAPWET);
	//SPEC FILT
	auto space_specfilt = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_SPECTRAL_FILTER] = space_specfilt;
	space_specfilt->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummyspecfilt = new HorizontalLayout(_appState, 2.,
		RATIO_FROM_PARENT_View, START_ALIGN, space_specfilt);
	auto flpm = new Slider(_appState, WRAP, 0, START_ALIGN, SPECFILTMAGLPCUT2, 0, 1, dummyspecfilt);
	flpm->paddingleft = flpm->paddingright = 5.f;
	flpm->paddingtop = flpm->paddingbottom = 10.f;

	View* flpf = new Slider(_appState, WRAP, 0, START_ALIGN, SPECFILTFREQLPCUT2, 0, 1,
		dummyspecfilt);
	flpf->paddingleft = flpf->paddingright = 5.f;
	flpf->paddingtop = flpf->paddingbottom = 10.f;

	View* fres = new Slider(_appState, WRAP, 0, START_ALIGN, SPECFILTRES, 0, 1, dummyspecfilt);
	fres->paddingleft = fres->paddingright = 5.f;
	fres->paddingtop = fres->paddingbottom = 10.f;

	View* sfdry = new Slider(_appState, WRAP, 0, START_ALIGN, SPECFILTDRY, 0, 1, dummyspecfilt);
	sfdry->paddingleft = sfdry->paddingright = 5.f;
	sfdry->paddingtop = sfdry->paddingbottom = 10.f;

	View* sfwet = new Slider(_appState, WRAP, 0, START_ALIGN, SPECFILTWET, 0, 1, dummyspecfilt);
	sfwet->paddingleft = sfwet->paddingright = 5.f;
	sfwet->paddingtop = sfwet->paddingbottom = 10.f;

	//LPC VOC
	auto space_lpcvoder = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_LPCVOCODER] = space_lpcvoder;

	space_lpcvoder->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto dummy1lpc = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_lpcvoder);
	dummy1lpc->paddingtop = 5.f;
	dummy1lpc->paddingbottom = 5.f;
	auto dummy2lpc = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_lpcvoder);
	dummy2lpc->paddingtop = 5.f;
	dummy2lpc->paddingbottom = 5.f;
	auto dummy3lpc = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_lpcvoder);
	dummy3lpc->paddingtop = 5.f;
	dummy3lpc->paddingbottom = 5.f;

	auto dummy11lpc = new HorizontalLayout(_appState, WRAP,
		0, END_ALIGN, dummy1lpc);
	auto dummy11lpca = new HorizontalLayout(_appState, WRAP,
		0, START_ALIGN, dummy11lpc);
	auto dummy11lpcb = new HorizontalLayout(_appState, WRAP,
		0, START_ALIGN, dummy11lpc);
	auto lpcorder = new PlusMinusControl(_appState, WRAP, 0, START_ALIGN, LPCVOCORDER);
	lpcorder->paddingleft = lpcorder->paddingright = 5.f;
	dummy1lpc->addChild(lpcorder);
	auto lpcwhite = new CheckBox(_appState, VERTICAL, START_ALIGN, LPCVOCWHITE);
	dummy11lpca->addChild(lpcwhite);
	dummy2lpc->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, LPCVOCMIX));
	dummy2lpc->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, LPCVOCGAIN));
	dummy3lpc->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, LPCVOCEMPH));

	/*
	 * Space SATURATOR
	 */

	auto space_sat = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_SATURATOR] = space_sat;
	space_sat->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1sat = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_sat);
	dummy1sat->paddingtop = 5.f;
	dummy1sat->paddingbottom = 5.f;
	auto dummy2sat = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_sat);
	dummy2sat->paddingtop = 5.f;
	dummy2sat->paddingbottom = 5.f;
	auto dummy3sat = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_sat);
	dummy3sat->paddingtop = 5.f;
	dummy3sat->paddingbottom = 5.f;

	dummy1sat->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, SATDRIVE));
	dummy1sat->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, SATWET));
	dummy1sat->addChild(new Knob(_appState, WRAP, 0,
		CENTER_ALIGN, SATDRY));

	// Inert while the VOX params are parked after NUM_PARAMS.
	if constexpr (VOXFREQ < NUM_PARAMS) {
	/*
	 * Space VOX
	 */

	auto space_vox = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_VOX] = space_vox;
	space_vox->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1vox = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_vox);
	dummy1vox->paddingtop = 5.f;
	dummy1vox->paddingbottom = 5.f;

        dummy1vox->addChild(new Knob(_appState, WRAP, 0,
                                     CENTER_ALIGN, VOXVOWEL));

        auto voxvoice = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
                                      ARRAY_LEN(voxvoicenames),
                                      makeStringSpan(voxvoicenames),
                                      emptyFloats, VOXVOICE, 0, 1, "VOICE");
        dummy1vox->addChild(voxvoice);
        auto dummy1vox2 = new HorizontalLayout(_appState, WRAP, 0 , CENTER_ALIGN);
        dummy1vox->addChild(dummy1vox2);

        auto voxfollow = new TextToggle(_appState, _STATE->parameters[VOXFOLLOW].name, VOXFOLLOW);
        _STATE->parameters[VOXFOLLOW].view = voxfollow;
        dummy1vox2->addChild(voxfollow);
        auto voxhold = new TextToggle(_appState, _STATE->parameters[VOXHOLD].name, VOXHOLD);
        _STATE->parameters[VOXHOLD].view = voxhold;
        dummy1vox2->addChild(voxhold);



	auto dummy2vox = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_vox);
	dummy2vox->paddingtop = 5.f;
	dummy2vox->paddingbottom = 5.f;
        dummy2vox->addChild(new Knob(_appState, WRAP, 0,
                                      CENTER_ALIGN, VOXATT));

        dummy2vox->addChild(new Knob(_appState, WRAP, 0,
                                      CENTER_ALIGN, VOXGLISS));

        dummy2vox->addChild(new Knob(_appState, WRAP, 0,
                                      CENTER_ALIGN, VOXOCT));




	auto dummy3vox = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_vox);
	dummy3vox->paddingtop = 5.f;
	dummy3vox->paddingbottom = 5.f;

        dummy3vox->addChild(new Knob(_appState, WRAP, 0,
                                     CENTER_ALIGN, VOXFREQ));
        dummy3vox->addChild(new Knob(_appState, WRAP, 0,
                                     CENTER_ALIGN, VOXWET));
        dummy3vox->addChild(new Knob(_appState, WRAP, 0,
                                     CENTER_ALIGN, VOXDRY));



    }

	/*
	 * Space VOX2 — formant filter bank (own effect with own power/bypass, cf. Vox2.cpp)
	 */

	auto space_vox2 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_VOX2] = space_vox2;
	space_vox2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1vox2 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_vox2);
	dummy1vox2->paddingtop = 5.f;
	dummy1vox2->paddingbottom = 5.f;
	auto dummy2vox2 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_vox2);
	dummy2vox2->paddingtop = 5.f;
	dummy2vox2->paddingbottom = 5.f;
	auto dummy3vox2 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_vox2);
	dummy3vox2->paddingtop = 5.f;
	dummy3vox2->paddingbottom = 5.f;

	dummy1vox2->addChild(new Knob(_appState, WRAP, 0,
	                              CENTER_ALIGN, VOX2VOWEL));

	auto vox2voice = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
	                               ARRAY_LEN(voxvoicenames),
	                               makeStringSpan(voxvoicenames),
	                               emptyFloats, VOX2VOICE, 0, 1, "VOICE");
	dummy1vox2->addChild(vox2voice);

	dummy2vox2->addChild(new Knob(_appState, WRAP, 0,
	                              CENTER_ALIGN, VOX2BW));

	dummy3vox2->addChild(new Knob(_appState, WRAP, 0,
	                              CENTER_ALIGN, VOX2WET));
	dummy3vox2->addChild(new Knob(_appState, WRAP, 0,
	                              CENTER_ALIGN, VOX2DRY));


	/*
		 * Space DISTORT
		 */
	auto space_dist = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_DISTORT] = space_dist;
	space_dist->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1dist = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_dist);
	dummy1dist->paddingtop = 5.f;
	dummy1dist->paddingbottom = 5.f;
	auto dummy2dist = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_dist);
	dummy2dist->paddingtop = 5.f;
	dummy2dist->paddingbottom = 5.f;
	auto dummy3dist = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_dist);
	dummy3dist->paddingtop = 5.f;
	dummy3dist->paddingbottom = 5.f;

	dummy1dist->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, DISTTONE));
	dummy1dist->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, DISTDRIVE));
	dummy2dist->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, DISTMIX));
	dummy2dist->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, DISTGAIN));


	auto space_flanger = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_FLANGER] = space_flanger;
	space_flanger->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1flanger = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_flanger);
	dummy1flanger->paddingtop = 5.f;
	dummy1flanger->paddingbottom = 5.f;
	auto dummy2flanger = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_flanger);
	dummy2flanger->paddingtop = 5.f;
	dummy2flanger->paddingbottom = 5.f;
	auto dummy3flanger = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_flanger);
	dummy3flanger->paddingtop = 5.f;
	dummy3flanger->paddingbottom = 5.f;

	dummy1flanger->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, FLANGERDELAY));
	dummy1flanger->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, FLANGERFB));
	dummy2flanger->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, FLANGERMIX));
	dummy2flanger->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, FLANGERGAIN));

	/*
		 * Space SSB
		 */
	auto space_ssb = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_SSB] = space_ssb;
	space_ssb->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1ssb = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_ssb);
	dummy1ssb->paddingtop = dummy1ssb->paddingbottom = 5.f;
	auto dummy2ssb = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_ssb);
	dummy2ssb->padding = 5.f;
	dummy2ssb->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, SSBMODMIX));
	space_ssb->addChild(new View(_appState, WRAP, 0, CENTER_ALIGN));
	auto ssbmode = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(mods), mods, {},
		SSBMODTYPE);
	ssbmode->paddingleft = 10.;
	dummy1ssb->addChild(ssbmode);
	dummy1ssb->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, SSBMODRATE));


	auto space_convolver = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_CONVOLVER] = space_convolver;

	auto cenv = new ADSRWindow(_appState, 40., PERCENTAGE_FROM_PARENT_View, START_ALIGN,
		CREVENVX0);
	space_convolver->addChild(cenv);
	auto dummycrevreverse = new VerticalLayout(_appState, 5., PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN,
		space_convolver);
	dummycrevreverse->paddingleft = dummycrevreverse->paddingright = 5.f;
	auto dummycrevreverse2 = new HorizontalLayout(_appState, 99.f, PERCENTAGE_FROM_PARENT_View,
		START_ALIGN,
		dummycrevreverse);
	dummycrevreverse2->paddingbottom = dummyje->paddingtop = 10.f;
	auto chbrev = new CheckBox(_appState, HORIZONTAL, CENTER_ALIGN, CREVBOUNCE);
	dummycrevreverse2->addChild(chbrev);

	auto dummy2_convolver = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, space_convolver);
	dummy2_convolver->paddingtop = 5.f;
	dummy2_convolver->paddingbottom = 5.f;
	auto dummy3_convolver = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_convolver);
	dummy3_convolver->paddingtop = 5.f;
	dummy3_convolver->paddingbottom = 5.f;

	auto conv_load_from = new TitleView(_appState, "LOAD IR", CENTER_ALIGN, dummy2_convolver);

	auto dummy_convolver_info = new VerticalLayout(_appState, 5.f, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN,
		dummy3_convolver);
	dummy_convolver_info->size_reference = &_STATE->textsize2;
	dummy_convolver_info->paddingleft = dummy_convolver_info->paddingright = 2.5f;
	dummy_convolver_info->addChild(new ConvolverButton(_appState, 0));
	dummy_convolver_info->addChild(new ConvolverButton(_appState, 1));
	dummy_convolver_info->addChild(new ConvolverButton(_appState, 2));
	dummy_convolver_info->addChild(new ConvolverButton(_appState, 3));
	auto ccc = new Slider(_appState, WRAP, 0, CENTER_ALIGN, CREVMAXSIZE, 0, 1, space_convolver);
	ccc->paddingleft = ccc->paddingright = 5.f;
	auto convolvermix = new Slider(_appState, WRAP, 0, CENTER_ALIGN, CREVMIX, 0, 1,
		space_convolver);convolvermix->paddingleft = convolvermix->paddingright = 5.f;
	auto convolvergain = new Slider(_appState, WRAP, 0, CENTER_ALIGN, CREVGAIN, 0, 1,
		space_convolver);
	convolvergain->paddingleft = convolvergain->paddingright = 5.f;

	/* SECOND CREVERB SPACE (CREVERB2): the IR morph front, level normalisation
	   and the auto-reload switch. Same effect instance as CREVERB, so it
	   deliberately has NO callbacks_fx_power entry - that is what disables the
	   off/bypass buttons here (SpaceFX::addRecursiveDraw and onChangeSpaceFx
	   both key off the null callback), exactly as in GENCREVERB ALG.
	   The selector and button rows keep the GENCREVERB pitch; the morph
	   controls are a knob row. */
	auto space_creverb2 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_CREVERB2] = space_creverb2;
	space_creverb2->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	/* fixed row, not WRAP: Selector2 shrinks to title + value (6.25% of the
	   window), so 6.75 is its own box plus a little air - same as the
	   GENCREVERB ALG selector */
	auto crnormcol = new VerticalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN, space_creverb2);
	crnormcol->paddingtop = crnormcol->paddingbottom = 5.f;
	auto crnorm = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		ARRAY_LEN(crevnormmodes), crevnormmodes, {}, CREVNORM, 0, 1, "NORM");
	crnorm->paddingleft = crnorm->paddingright = SDSIDEPAD;
	crnormcol->addChild(crnorm);

	/* MORPH RATE + FRONT as a knob row. A VerticalLayout IS a row and its WRAP
	   children split it side by side, so two Knobs sit next to each other.
	   Sized from knob_height_total, NOT from a share of the space: a WRAP row
	   takes whatever the fixed rows leave over (here ~39% of the window, with
	   knobs to match), and any fixed percentage only happens to look right at
	   one panel shape. The constant is what makes a knob the same size in a
	   336px sub-view and a 502px mono panel. START_ALIGN and no VERTICAL
	   padding are part of the idiom - see init_space_graingen /
	   init_space_env_follower.
	   Budget: 2.5 spacer + 6.75 selector + 18.77 knobs + 6 buttons = 34% of
	   the 65.25% an fx space gets, so the fill dummy still has room. */
	auto crmorphrow = new VerticalLayout(_appState, VALUE_FROM_POINTER,
		VALUE_FROM_POINTER, START_ALIGN, space_creverb2);
	crmorphrow->size_reference = &_STATE->knob_height_total;
	crmorphrow->paddingleft = crmorphrow->paddingright = 5.f;
	crmorphrow->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CREVMORPHRATE));
	crmorphrow->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, CREVMORPHFRONT));

	/* REVERSE and AUTO RELOAD share one row, so each WRAP child takes half of
	   it and both come out the size of PING PONG DELAY's FREEZE button */
	auto crbtnrow = new VerticalLayout(_appState, 6.,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN, space_creverb2);
	_STATE->parameters[CREVMORPHREV].view = new TextButton(_appState, WRAP, 0,
		CENTER_ALIGN, "REVERSE", CREVMORPHREV, 0, 1);
	_STATE->parameters[CREVMORPHREV].view->padding = 10.f;
	crbtnrow->addChild(_STATE->parameters[CREVMORPHREV].view);
	_STATE->parameters[CREVAUTO].view = new TextButton(_appState, WRAP, 0,
		CENTER_ALIGN, "AUTO RELOAD", CREVAUTO, 0, 1);
	_STATE->parameters[CREVAUTO].view->padding = 10.f;
	crbtnrow->addChild(_STATE->parameters[CREVAUTO].view);

	/* fill dummy: a plain View paints its rect with the background, so a
	   taller space underneath cannot survive in the leftover area */
	space_creverb2->addChild(new View(_appState, WRAP, 0, CENTER_ALIGN));


	auto space_vocoder = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_VOCODER] = space_vocoder;
	space_vocoder->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1vocoder = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_vocoder);
	dummy1vocoder->paddingtop = 5.f;
	dummy1vocoder->paddingbottom = 5.f;
	auto dummy2vocoder = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_vocoder);
	dummy2vocoder->paddingtop = 5.f;
	dummy2vocoder->paddingbottom = 5.f;
	auto dummy3vocoder = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_vocoder);
	dummy3vocoder->paddingtop = 5.f;
	dummy3vocoder->paddingbottom = 5.f;

	dummy1vocoder->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, VOCBW));
	dummy1vocoder->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, VOCA));
	dummy1vocoder->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, VOCB));

	auto vocoderchans = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, VOCCHANS);
	dummy2vocoder->addChild(vocoderchans);

	dummy2vocoder->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, VOCATT));
	dummy2vocoder->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, VOCREL));

	dummy3vocoder->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, VOCMIX));
	dummy3vocoder->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, VOCGAIN));

	auto dummy_vocoder_info = new HorizontalLayout(_appState, WRAP,
		0, CENTER_ALIGN, dummy3vocoder);
	dummy_vocoder_info->paddingtop = 5.f;
	dummy_vocoder_info->paddingbottom = 5.f;
	auto tv_mod = new TitleView(_appState, "MODULATOR", CENTER_ALIGN, dummy_vocoder_info);
	new VocTV(_appState, CENTER_ALIGN, dummy_vocoder_info);

	/*
	 * Space Env Follower
	 */
	auto space_moog = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_MOOGLADDER] = space_moog;
	space_moog->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1moog = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_moog);
	dummy1moog->paddingtop = 5.f;
	dummy1moog->paddingbottom = 5.f;
	auto dummy2moog = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_moog);
	dummy2moog->paddingtop = 5.f;
	dummy2moog->paddingbottom = 5.f;
	auto dummy3moog = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_moog);
	dummy3moog->paddingtop = 5.f;
	dummy3moog->paddingbottom = 5.f;

	dummy1moog->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, MOOGCUT));
	dummy1moog->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, MOOGRES));
	dummy2moog->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, MOOGMIX));
	dummy2moog->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, MOOGGAIN));

	/*
	 *
	 *  Space Phaser
	 */
	auto space_phaser4 = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_PHASER4] = space_phaser4;
	space_phaser4->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1phaser4 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_phaser4);
	dummy1phaser4->paddingtop = 5.f;
	dummy1phaser4->paddingbottom = 5.f;
	auto dummy2phaser4 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_phaser4);
	dummy2phaser4->paddingtop = 5.f;
	dummy2phaser4->paddingbottom = 5.f;
	auto dummy3phaser4 = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_phaser4);
	dummy3phaser4->paddingtop = 5.f;
	dummy3phaser4->paddingbottom = 5.f;

	dummy1phaser4->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PHASER4BAND));
	auto phaser4_notches = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, PHASER4STAGES);
	dummy1phaser4->addChild(phaser4_notches);
	_STATE->parameters[PHASER4STAGES].view = phaser4_notches;
	phaser4_notches->id = PHASER4STAGES;
	dummy2phaser4->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PHASER4SPACING));
	dummy2phaser4->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PHASER4RADIUS));
	dummy3phaser4->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PHASER4FB));
	dummy3phaser4->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PHASER4MIX));

	auto space_phaser = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_PHASER] = space_phaser;
	space_phaser->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1phaser = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_phaser);
	dummy1phaser->paddingtop = 5.f;
	dummy1phaser->paddingbottom = 5.f;
	auto dummy2phaser = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_phaser);
	dummy2phaser->paddingtop = 5.f;
	dummy2phaser->paddingbottom = 5.f;
	auto dummy3phaser = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_phaser);
	dummy3phaser->paddingtop = 5.f;
	dummy3phaser->paddingbottom = 5.f;

	dummy1phaser->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PHASERFB));
	auto phaser_notches = new PlusMinusControl(_appState, WRAP, 0, CENTER_ALIGN, PHASERNOTCHES);
	dummy1phaser->addChild(phaser_notches);

	dummy2phaser->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PHASERBAND));
	dummy2phaser->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PHASERMIX));

	/*
	 * Space EQ
	 */
	auto space_eq = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_EQ] = space_eq;

	for (int32_t i = 0; i < 12; i++) {
		View* eq100 = new Slider(_appState, WRAP, 0, START_ALIGN, EQ10_0 + i, 0, 1, space_eq);
		eq100->paddingleft = 5.f;
		eq100->paddingright = 5.f;
		eq100->paddingtop = 20.f;
		eq100->paddingbottom = 20.f;
	}

	auto space_mdelay = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_MDELAY] = space_mdelay;


	auto dummysliders = new HorizontalLayout(_appState, 10., PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN,
		space_mdelay);
	auto mdelay_mix = new Slider(_appState, WRAP, 0, START_ALIGN, MDELAYMIX, 0, 1, dummysliders);
	mdelay_mix->paddingleft = 5.f;
	mdelay_mix->paddingright = 5.f;
	mdelay_mix->paddingtop = 20.f;
	mdelay_mix->paddingbottom = 20.f;
	auto mdelay_gain = new Slider(_appState, WRAP, 0, START_ALIGN, MDELAYGAIN, 0, 1, dummysliders);

	mdelay_gain->paddingleft = 5.f;
	mdelay_gain->paddingright = 5.f;
	mdelay_gain->paddingtop = 20.f;
	mdelay_gain->paddingbottom = 20.f;

	space_mdelay->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));
	auto mdelay_mode = new ButtonView<TextButtonFramed>(_appState, 5.,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		HORIZONTAL, 2, mdelaymodes,
		envfollowermodesvalues, MDELAY_MODE);
	mdelay_mode->paddingleft = 5.f;
	mdelay_mode->paddingright = 5.f;
	space_mdelay->addChild(mdelay_mode);

	space_mdelay->addChild(new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));

	auto dummytitlemdelay = new VerticalLayout(_appState, 6.,
		PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN, space_mdelay);

	auto selector_mdelay = new Selector1<TitleView>(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE,
		CENTER_ALIGN,
		ARRAY_LEN(mdelaynames), mdelaynames,
		std::span<const float>{},
		ASMDEL);


	selector_mdelay->paddingtop = 10.f;
	selector_mdelay->paddingbottom = 10.f;
	//selector_mdelay->paddingleft = selector_mdelay->paddingleft = 2.5f;
	dummytitlemdelay->addChild(selector_mdelay);


	auto offbuttonmdelay = new NormalButton(_appState, SYM, 0, END_ALIGN,
		ICON_MD_POWER_SETTINGS_NEW,
		ICON_MD_POWER_SETTINGS_NEW,
		MDELAY1_POW, ASMDEL,
		1);
	offbuttonmdelay->padding = 5.f;
	dummytitlemdelay->addChild(offbuttonmdelay);

	auto mdelaycontrolpanel2 = new VerticalLayout(_appState, 6.,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		space_mdelay);
	_DATA->views.mdelay_controlpanel2 = mdelaycontrolpanel2;

	auto mdelay_input = new NormalButton(_appState, SYM, 0, START_ALIGN, ICON_MD_INPUT,
		ICON_MD_INPUT,
		MDELAY1_CONTROLS_ACTIVE, ASMDEL,
		1);
	mdelay_input->padding = 17.f;
	mdelaycontrolpanel2->addChild(mdelay_input);

	auto mdelay_factor = new Selector1<TitleView>(_appState, 5., VIEW_COMPUTESIZE, START_ALIGN,
		ARRAY_LEN(syncfactornames), syncfactornames,
		syncfactors, MDELAY1_SYNC_FACTOR, ASMDEL);
	mdelaycontrolpanel2->addChild(mdelay_factor);

	auto mdelay_slow = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_SLOW_MOTION_VIDEO,
		ICON_MD_SLOW_MOTION_VIDEO, MDELAY1_SLOW, ASMDEL);
	mdelay_slow->padding = 10.f;
	mdelaycontrolpanel2->addChild(mdelay_slow);

	auto mdelay_fast = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN,
		ICON_MD_PLAY_CIRCLE_OUTLINE,
		ICON_MD_PLAY_CIRCLE_OUTLINE, MDELAY1_FAST, ASMDEL);
	mdelay_fast->padding = 10.f;
	mdelaycontrolpanel2->addChild(mdelay_fast);


	auto mdelay_sync = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_SYNC,
		ICON_MD_SYNC, MDELAY1_SYNC, ASMDEL);
	mdelay_sync->padding = 10.f;
	mdelaycontrolpanel2->addChild(mdelay_sync);

	auto dummymdelay = new HorizontalLayout(_appState, WRAP,
		0, START_ALIGN, space_mdelay);
	_STATE->parameters[MDELAY1DUMMY1].view = dummymdelay;

	auto delay_dum = new VerticalLayout(_appState, 6.,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN, dummymdelay);
	auto delay_hold = new TextButton(_appState, WRAP, 0, CENTER_ALIGN, "FREEZE", MDELAY1HOLD,
		ASMDEL,
		NUM_CONTROLS_MDELAY);

	delay_hold->padding = 10.f;
	delay_dum->addChild(delay_hold);
	_STATE->parameters[MDELAY1HOLD].view = delay_hold;


	_STATE->parameters[MDELAY1BACKW].view = new TextButton(_appState, WRAP, 0, CENTER_ALIGN,
		"BACKW",
		MDELAY1BACKW, ASMDEL, 1);
	_STATE->parameters[MDELAY1BACKW].view->padding = 10.;
	delay_dum->addChild(_STATE->parameters[MDELAY1BACKW].view);
	for (int i = MDELAY2BACKW; i <= MDELAY8BACKW; i++)_STATE->parameters[i].view = _STATE->parameters[MDELAY1BACKW].view;

	auto mdelay_delay = new Slider(_appState, WRAP, 0, START_ALIGN, MDELAY1DEL,
		ASMDEL, NUM_CONTROLS_MDELAY, dummymdelay);
	mdelay_delay->paddingleft = 5.f;
	mdelay_delay->paddingright = 5.f;
	mdelay_delay->paddingtop = 20.f;
	mdelay_delay->paddingbottom = 20.f;
	auto mdelay_feedback = new Slider(_appState, WRAP, 0, START_ALIGN, MDELAY1FB,
		ASMDEL, NUM_CONTROLS_MDELAY, dummymdelay);
	mdelay_feedback->paddingleft = 5.f;
	mdelay_feedback->paddingright = 5.f;
	mdelay_feedback->paddingtop = 20.f;
	mdelay_feedback->paddingbottom = 20.f;

	auto mdelay_cut_off_h = new Slider(_appState, WRAP, 0, START_ALIGN, MDELAY1HP,
		ASMDEL, NUM_CONTROLS_MDELAY, dummymdelay);
	mdelay_cut_off_h->paddingleft = 5.f;
	mdelay_cut_off_h->paddingright = 5.f;
	mdelay_cut_off_h->paddingtop = 20.f;
	mdelay_cut_off_h->paddingbottom = 20.f;
	auto mdelay_cut_off_l = new Slider(_appState, WRAP, 0, START_ALIGN, MDELAY1LP,
		ASMDEL, NUM_CONTROLS_MDELAY, dummymdelay);
	mdelay_cut_off_l->paddingleft = 5.f;
	mdelay_cut_off_l->paddingright = 5.f;
	mdelay_cut_off_l->paddingtop = 20.f;
	mdelay_cut_off_l->paddingbottom = 20.f;
	auto mmdelay_gain = new Slider(_appState, WRAP, 0, START_ALIGN, MDELAY1GAIN,
		ASMDEL, NUM_CONTROLS_MDELAY, dummymdelay);
	mmdelay_gain->paddingleft = 5.f;
	mmdelay_gain->paddingright = 5.f;
	mmdelay_gain->paddingtop = 20.f;
	mmdelay_gain->paddingbottom = 20.f;

	/*
	 * Space Delay
	 */
	auto space_delay = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_DELAY] = space_delay;

	auto delaycontrolpanel2 = new VerticalLayout(_appState, 6.,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN,
		space_delay);
	auto delay_input = new NormalButton(_appState, SYM, 0, START_ALIGN, ICON_MD_INPUT,
		ICON_MD_INPUT,
		DELAY_CONTROLS_ACTIVE, 0,
		1);
	delaycontrolpanel2->addChild(delay_input);
	delay_input->padding = 17.f;


	auto delay_factor = new Selector1<TitleView>(_appState, 5.f, VIEW_COMPUTESIZE, START_ALIGN,
		ARRAY_LEN(syncfactornames), syncfactornames,
		syncfactors, DELAY_SYNC_FACTOR);
	delaycontrolpanel2->addChild(delay_factor);
	//delay_factor->paddingtop = delay_factor->paddingbottom = 10.f;

	auto delay_slow = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_SLOW_MOTION_VIDEO,
		ICON_MD_SLOW_MOTION_VIDEO, DELAY_SLOW);
	delay_slow->padding = 10.f;
	delaycontrolpanel2->addChild(delay_slow);

	auto delay_fast = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN,
		ICON_MD_PLAY_CIRCLE_OUTLINE,
		ICON_MD_PLAY_CIRCLE_OUTLINE, DELAY_FAST);
	delay_fast->padding = 10.f;
	delaycontrolpanel2->addChild(delay_fast);

	auto delay_sync = new NormalButton(_appState, WRAP, 0, CENTER_ALIGN, ICON_MD_SYNC,
		ICON_MD_SYNC, DELAY_SYNC);
	delay_sync->padding = 10.f;
	delaycontrolpanel2->addChild(delay_sync);

	auto delay_dum1 = new VerticalLayout(_appState, 6.,
		PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN, space_delay);
	auto delay_hold1 = new TextButton(_appState, WRAP, 0, CENTER_ALIGN, "FREEZE",
		DELAYHOLD, 0, 1);

	delay_hold1->padding = 10.f;
	delay_dum1->addChild(delay_hold1);
	_STATE->parameters[DELAYHOLD].view = delay_hold1;


	_STATE->parameters[DELAYBACKW].view = new TextButton(_appState, WRAP, 0, CENTER_ALIGN, "BACKW",
		DELAYBACKW, 0, 1);

	delay_dum1->addChild(_STATE->parameters[DELAYBACKW].view);
	_STATE->parameters[DELAYBACKW].view->padding = 10.;
	auto dummy2hordelay = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_delay);
	auto delay_delay = new Slider(_appState, WRAP, 0, START_ALIGN, DELAYDEL, 0, 1, dummy2hordelay);
	delay_delay->paddingleft = 5.f;
	delay_delay->paddingright = 5.f;
	delay_delay->paddingtop = 20.f;
	delay_delay->paddingbottom = 20.f;
	auto delay_fb = new Slider(_appState, WRAP, 0, START_ALIGN, DELAYFB, 0, 1, dummy2hordelay);
	delay_fb->paddingleft = 5.f;
	delay_fb->paddingright = 5.f;
	delay_fb->paddingtop = 20.f;
	delay_fb->paddingbottom = 20.f;
	auto delay_shift = new Slider(_appState, WRAP, 0, START_ALIGN, DELAYSHIFT2, 0, 1,
		dummy2hordelay);
	delay_shift->paddingleft = 5.f;
	delay_shift->paddingright = 5.f;
	delay_shift->paddingtop = 20.f;
	delay_shift->paddingbottom = 20.f;
	auto delay_shiftmix = new Slider(_appState, WRAP, 0, START_ALIGN, DELAYSHIFTMIX, 0, 1,
		dummy2hordelay);
	delay_shiftmix->paddingleft = 5.f;
	delay_shiftmix->paddingright = 5.f;
	delay_shiftmix->paddingtop = 20.f;
	delay_shiftmix->paddingbottom = 20.f;
	auto delay_cut_off_h = new Slider(_appState, WRAP, 0, START_ALIGN, DELAYHP, 0, 1,
		dummy2hordelay);
	delay_cut_off_h->paddingleft = 5.f;
	delay_cut_off_h->paddingright = 5.f;
	delay_cut_off_h->paddingtop = 20.f;
	delay_cut_off_h->paddingbottom = 20.f;
	auto delay_cut_off_l = new Slider(_appState, WRAP, 0, START_ALIGN, DELAYLP, 0, 1,
		dummy2hordelay);
	delay_cut_off_l->paddingleft = 5.f;
	delay_cut_off_l->paddingright = 5.f;
	delay_cut_off_l->paddingtop = 20.f;
	delay_cut_off_l->paddingbottom = 20.f;
	auto delay_dry = new Slider(_appState, WRAP, 0, START_ALIGN, DELAYDRY, 0, 1, dummy2hordelay);
	delay_dry->paddingleft = 5.f;
	delay_dry->paddingright = 5.f;
	delay_dry->paddingtop = 20.f;
	delay_dry->paddingbottom = 20.f;
	auto delay_wet = new Slider(_appState, WRAP, 0, START_ALIGN, DELAYWET, 0, 1, dummy2hordelay);
	delay_wet->paddingleft = 5.f;
	delay_wet->paddingright = 5.f;
	delay_wet->paddingtop = 20.f;
	delay_wet->paddingbottom = 20.f;
	/*
		 * Space Filter
		 */
	auto space_filter = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_HPLP] = space_filter;
	space_filter->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1filter = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_filter);
	dummy1filter->paddingtop = 5.f;
	dummy1filter->paddingbottom = 5.f;
	auto dummy2filter = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_filter);
	dummy2filter->paddingtop = 5.f;
	dummy2filter->paddingbottom = 5.f;
	auto dummy3filter = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_filter);
	dummy3filter->paddingtop = 5.f;
	dummy3filter->paddingbottom = 5.f;

	dummy1filter->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, LPHPHPCUT));
	dummy1filter->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, LPHPLPCUT));
	dummy2filter->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, LPHPMIX));
	dummy2filter->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, LPHPGAIN));


	auto space_bandreject = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_BANDREJECT] = space_bandreject;
	space_bandreject->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1bandreject = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bandreject);
	dummy1bandreject->paddingtop = 5.f;
	dummy1bandreject->paddingbottom = 5.f;
	auto dummy2bandreject = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bandreject);
	dummy2bandreject->paddingtop = 5.f;
	dummy2bandreject->paddingbottom = 5.f;
	auto dummy3bandreject = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bandreject);
	dummy3bandreject->paddingtop = 5.f;
	dummy3bandreject->paddingbottom = 5.f;

	dummy1bandreject->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BRCENTER));
	dummy1bandreject->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BRBW));
	dummy2bandreject->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BRMIX));
	dummy2bandreject->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BRGAIN));

	/*
		 *
		 * BANDPASS
		 *
		 */
	auto space_bandpass = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_BANDPASS] = space_bandpass;
	space_bandpass->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1bandpass = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bandpass);
	dummy1bandpass->paddingtop = 5.f;
	dummy1bandpass->paddingbottom = 5.f;
	auto dummy2bandpass = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bandpass);
	dummy2bandpass->paddingtop = 5.f;
	dummy2bandpass->paddingbottom = 5.f;
	auto dummy3bandpass = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_bandpass);
	dummy3bandpass->paddingtop = 5.f;
	dummy3bandpass->paddingbottom = 5.f;

	dummy1bandpass->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BPCENTER));
	dummy1bandpass->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BPBW));
	dummy2bandpass->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BPMIX));
	dummy2bandpass->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, BPGAIN));

	/*
		 * Space Comp
		 */

	auto space_comp = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_COMPRESSION] = space_comp;
	auto dummy1comp = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_comp);
	dummy1comp->paddingtop = 5.f;
	dummy1comp->paddingbottom = 5.f;
	auto dummy2comp = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_comp);
	dummy2comp->paddingtop = 5.f;
	dummy2comp->paddingbottom = 5.f;
	auto dummy3comp = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_comp);
	dummy3comp->paddingtop = 5.f;
	dummy3comp->paddingbottom = 5.f;

	auto comp_src = new ButtonView<TextButtonFramed>(_appState, 5., PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN, HORIZONTAL, 4,
		makeStringSpan(tracknames),
		emptyFloats, MONOCOMPSRC);
	space_comp->addChild(comp_src);

	auto renderir = new CompressorView(_appState, WRAP, 0, CENTER_ALIGN);
	_DATA->views.IRMonoComp = renderir;

	auto thres = new Knob(_appState, WRAP, 0,
		START_ALIGN, COMPTHR, 0, 1, dummy1comp);

	auto rat = new Knob(_appState, WRAP, 0,
		START_ALIGN, COMPRATIO, 0, 1, dummy1comp);

	auto knee = new Knob(_appState, WRAP, 0,
		START_ALIGN, COMPKNEE, 0, 1, dummy1comp);

	dummy2comp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, COMPATT));
	dummy2comp->addChild(renderir);
	dummy2comp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, COMPREL));

	dummy3comp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, COMPRMS));
	dummy3comp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, COMPLOOKA));
	dummy3comp->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, COMPMAKE));

	auto space_pshift = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[SPACE_SHIFTER] = space_pshift;
	space_pshift->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
	auto dummy1pshift = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_pshift);
	dummy1pshift->paddingtop = dummy1pshift->paddingbottom = 5.f;
	auto dummy2pshift = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_pshift);
	dummy2pshift->paddingtop = dummy2pshift->paddingbottom = 5.f;
	space_pshift->addChild(new View(_appState, WRAP,
		0, CENTER_ALIGN));
	dummy2pshift->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PITCHSHIFERMIX));

	auto shiftmode = new Selector2(_appState, WRAP, 0, CENTER_ALIGN,
		2, makeStringSpan(shimmermodesnames).subspan(1, 2),
		emptyFloats, PITCHSHIFTMODE);

	shiftmode->paddingleft = 10.;
	dummy1pshift->addChild(shiftmode);
	dummy1pshift->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, PITCHSHIFTSHIFT));


	auto space_noise = new HorizontalLayout(_appState, WRAP, 0, START_ALIGN, space_fx, true);
	_DATA->views.spaces_fx[NOISEMONOEFFECT] = space_noise;

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
		emptyFloats, NOISEMONOTYPE);

	nt->paddingleft = 10.;
	dummy1bandpassnoise->addChild(nt);


	auto dummy2bandpassnoise = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_noise);
	dummy2bandpassnoise->paddingtop = 5.f;
	dummy2bandpassnoise->paddingbottom = 5.f;

	auto dummy3bandpassnoise = new VerticalLayout(_appState, WRAP,
		0, CENTER_ALIGN, space_noise);
	dummy3bandpassnoise->paddingtop = 5.f;
	dummy3bandpassnoise->paddingbottom = 5.f;

	dummy2bandpassnoise->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, NOISEMONOMIX));

	dummy2bandpassnoise->addChild(new Knob(_appState, WRAP, 0, CENTER_ALIGN, NOISEMONOGAIN));

}
