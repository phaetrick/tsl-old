#include "gui_internal.h"


void init_space_env_follower(tsl::AppState* _appState, Layout* root) {

	auto space_envfollow = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, true);
	root->addChild(space_envfollow);

	_DATA->views.active_spaces_array[SPACE_ENVFOLLOWER] = space_envfollow;


	auto placeholder666 = new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	space_envfollow->addChild(placeholder666);

	auto placeholder667 = new View(_appState, 1.25, PERCENTAGE_FROM_MAIN_WINDOW,
		END_ALIGN);
	space_envfollow->addChild(placeholder667);

	auto dummytitlefx = new HorizontalLayout(_appState, 6.75, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN);
	dummytitlefx->paddingtop = 10.f;
	dummytitlefx->paddingbottom = 10.f;
	dummytitlefx->paddingleft = 10.;
	dummytitlefx->paddingright = 2.5f;

	space_envfollow->addChild(dummytitlefx);
	auto envspace = new TitleView(_appState, "ENVELOPE FOLLOWERS", CENTER_ALIGN);
	envspace->textalignhoz = START_ALIGN;
	dummytitlefx->addChild(envspace);

	//  space_envfollow->addChild(new View(_appState,  2.5, PERCENTAGE_FROM_MAIN_WINDOW,
	//                                   START_ALIGN));

	auto dummy6000 = new VerticalLayout(_appState, 15., RATIO_FROM_MAIN_WINDOW, START_ALIGN,
		space_envfollow);
	dummy6000->paddingleft = dummy6000->paddingright = 2.5f;

	auto lfodest = new Selector1<TitleView>(_appState, WRAP, 0, CENTER_ALIGN,
		_STATE->dofastrender ? ARRAY_LEN(env_detectoritems)
		: ARRAY_LEN(env_detectoritems2),
		_STATE->dofastrender ? std::span<const std::string_view>{
		env_detectoritems}
	: std::span<const std::string_view>{
env_detectoritems2 },
_STATE->dofastrender ? std::span<const float>{
		env_detectorvalues} : std::span<const float>{
		env_detectorvalues2 },
		FOLLOWERDEST);

	lfodest->middleclick = true;
	dummy6000->addChild(lfodest);
	lfodest->rv->isActiveFunc = [lfodest, _appState](int32_t index) {
		return _STATE->followerMap[_STATE->active_track.load()].at(
			(int)lfodest->values[index]).envpower.load();
		};
	lfodest->paddingtop = lfodest->paddingbottom = 10.f;
	lfodest->paddingleft = lfodest->paddingright = 1.25f;

	auto lfooffbutton2 = new NormalButton(_appState, SYM, 0, END_ALIGN, ICON_MD_POWER_SETTINGS_NEW,
		ICON_MD_POWER_SETTINGS_NEW, FOLLOWERDESTPOWER);
	lfooffbutton2->padding = 10.f;
	dummy6000->addChild(lfooffbutton2);

	space_envfollow->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));


	auto dummy1env1 = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN);
	dummy1env1->size_reference = &_STATE->knob_height_total;
	//dummy1env->paddingtop = 10.f;
	// dummy1env->paddingbottom = 8.f;
	space_envfollow->addChild(dummy1env1);
	auto a = new Knob(_appState, WRAP, 0, CENTER_ALIGN, FOLLOWERBOUNDA, 0, 1, dummy1env1);
	auto b = new Knob(_appState, WRAP, 0, CENTER_ALIGN, FOLLOWERBOUNDB, 0, 1, dummy1env1);
	space_envfollow->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));


	auto dummy1env = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
		START_ALIGN);
	dummy1env->size_reference = &_STATE->knob_height_total;
	//dummy1env->paddingtop = 10.f;
	// dummy1env->paddingbottom = 8.f;
	space_envfollow->addChild(dummy1env);

	auto att1 = new Knob(_appState, WRAP, 0, CENTER_ALIGN, FOLLOWERATT, 0, 1, dummy1env);
	auto rel1 = new Knob(_appState, WRAP, 0, CENTER_ALIGN, FOLLOWERREL, 0, 1, dummy1env);
	auto gain1 = new Knob(_appState, WRAP, 0, CENTER_ALIGN, FOLLOWERGAIN, 0, 1, dummy1env);

	space_envfollow->addChild(new View(_appState, 2.5, PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN));

	auto env_follower_src = new ButtonView<TextButtonFramed>(_appState, 5.,
		PERCENTAGE_FROM_MAIN_WINDOW,
		START_ALIGN, HORIZONTAL,
		ARRAY_LEN(tracknames),
		std::span<const std::string_view>{
		tracknames},
		std::span<const float>{},
		FOLLOWERSRC);
	env_follower_src->paddingleft = 1.25f;
	env_follower_src->paddingright = 1.25f;
	space_envfollow->addChild(env_follower_src);
	space_envfollow->addChild(new FollowerView(_appState, WRAP, 0, CENTER_ALIGN));
}
