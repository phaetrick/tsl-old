#include "gui_internal.h"


#ifdef __ANDROID__

	void guiSetup(JNIEnv * env, jclass obj, jboolean fastrender, jint screenWidth, jint screenHeight) {
		auto _appState = __STATE;
		_STATE->dofastrender = fastrender;
		_STATE->readyForUiSetup.release();
	}

#endif

class RootLayout : public Layout {
public:
	explicit RootLayout(tsl::AppState* appState) : Layout(appState, WRAP, 0, CENTER_ALIGN) {
			orientation = HORIZONTAL;
		};

	void init() override {

			if (width == _STATE->windowWidth && height == _STATE->windowHeight)return;
			// PORTRAIT MODE: Fill the screen
			startx = 0;
			starty = 0;
			width = _STATE->windowWidth;
			height = _STATE->windowHeight;

			// Update your hitboxes/render bounds
			stopx = startx + width;
			stopy = starty + height;

			_STATE->textsize2 = (int)(height * 0.025);
			_STATE->textsize1 = (int)(height * 0.05);
			SkFont font(_STATE->font_normal);
			font.setSize(_STATE->textsize2 * .9);
			SkFontMetrics metrics{};
			font.getMetrics(&metrics);
			const char* test =
				"POS: 00 : 00 : 000X/ 00 : 00 : 000  LOOP: 00 : 00 : 000XLOAD: 100%";
			float x = 0;
			View::measureTextFixed(100, _STATE->textsize1, font, test, &x, &_STATE->startyt2,
				_STATE->textsize2 * .9);
#ifdef PLATFORM_DESKTOP
			_STATE->maxCharWidtht2 = metrics.fMaxCharWidth;
#else
			_STATE->maxCharWidtht2 = metrics.fAvgCharWidth;
#endif
			font.setSize(_STATE->textsize1 * .9);
			font.getMetrics(&metrics);
			_STATE->maxCharWidtht1 = metrics.fMaxCharWidth;
			_STATE->circleradius = _STATE->textsize2 * .9;


			_STATE->knob_height_Knob =
				(int)(height * 0.18769) - (_STATE->textsize1 + _STATE->textsize2);
			_STATE->knob_height_buttonsandKnob = _STATE->textsize1 + _STATE->knob_height_Knob;
			_STATE->knob_height_titleandKnob = _STATE->textsize2 + _STATE->knob_height_Knob;
			_STATE->knob_height_total =
				_STATE->textsize1 + _STATE->textsize2 + _STATE->knob_height_Knob;
			/*
					_DATA->textsize = (int) (height * 0.0175);
					_DATA->height_toolbars = 2 * _DATA->textsize;
					_DATA->_STATE->circleradius = _DATA->textsize;

					_DATA->_STATE->knob_height_Knob = (int) (_DATA->textsize * 2);
					_DATA->knob_height_buttonsandKnob = _DATA->textsize * 2 + _DATA->_STATE->knob_height_Knob;
					_DATA->knob_height_titleandKnob = _DATA->textsize + _DATA->_STATE->knob_height_Knob;
					_DATA->_STATE->knob_height_total = _DATA->textsize * 3 + _DATA->_STATE->knob_height_Knob;
			*/        //  if(width >height)
			//    Layout::initVertical();
			//else
			Layout::initHorizontal();
			//Layout::init();
#ifdef GRAINSTORM
			_DATA->views.decoderView->init();

			_DATA->views.recorderView->init();
#endif
			auto rec = _DATA->tracks[_STATE->active_track.load()]->filebuffer.load();
			if (rec) {
				rec->dorendering.store(true);
			}
			if (firstrun) {
				firstrun = false;
#ifdef __ANDROID__
				if (_DATA->startPoweredOn)
#endif
					_STATE->player.play();
			}
		}

private:
	bool firstrun = true;
};


void tsl::app::guiSetup(tsl::AppState * _appState) {

#if defined PLATFORM_MOBILE
#if defined OS_IOS
	_appState->checkAndSetupPurchase();
	_appState->dofastrender = true;

	#else
		bool ok = _STATE->readyForUiSetup.try_acquire_for(std::chrono::seconds(10));
#endif
	#else
		_appState->dofastrender = true;
#endif
		_DATA->views.recorderView = std::make_unique<RecorderView>(_appState);

		_DATA->views.decoderView = std::make_unique<DecoderView>(_appState);
		_STATE->rootwin = std::make_unique<RootLayout>(_appState);
		auto mainview = _STATE->rootwin.get();
		/*
			View *dummy = view_create(p, "dummy", 30, false, VERTICAL, 15.,
										 RATIO_FROM_MAIN_WINDOW, END_ALIGN,
										 view_init_vertical, view_destroy, callback_view,
										 nullptr, nullptr);
			view_addelement(mainview, dummy);
		*/
		auto toppanel = new VerticalLayout(_appState, 15.,
			RATIO_FROM_MAIN_WINDOW, START_ALIGN);
		mainview->addChild(toppanel);
		_DATA->views.toppanel_main = toppanel;

		auto dividertoppanel = new Divider(_appState, START_ALIGN);
		mainview->addChild(dividertoppanel);

		_DATA->views.divider_toppanel = dividertoppanel;

		auto button_track1 = new TrackButton(_appState, SYM, 0,
			START_ALIGN, 0);
		toppanel->addChild(button_track1);
		_DATA->track1.button = button_track1;

		auto button_track2 = new TrackButton(_appState, SYM, 0,
			START_ALIGN, 1);
		toppanel->addChild(button_track2);
		_DATA->track2.button = button_track2;

		auto button_track3 = new TrackButton(_appState, SYM, 0,
			START_ALIGN, 2);
		toppanel->addChild(button_track3);
		_DATA->track3.button = button_track3;

		auto button_track4 = new TrackButton(_appState, SYM, 0,
			START_ALIGN, 3);
		toppanel->addChild(button_track4);
		_DATA->track4.button = button_track4;

		auto offbutton = new BypassOffButton(_appState, SYM, 0, END_ALIGN, ICON_MD_POWER_SETTINGS_NEW,
			ICON_MD_POWER_SETTINGS_NEW, POWERButton);
		offbutton->padding = 10.f;
		toppanel->addChild(offbutton);

		View* settingsbutton = new NormalButton(_appState, SYM, 0, END_ALIGN,
			ICON_MD_MORE_VERT, ICON_MD_MORE_VERT, SETTINGSBUTTON);
		settingsbutton->padding = 10.f;
		toppanel->addChild(settingsbutton);

		//if (_STATE->dofastrender) {
		View* recordbutton = new RecordButton(_appState, SYM, 0, END_ALIGN);
		recordbutton->padding = 10.f;
		toppanel->addChild(recordbutton);
		//}

		auto meter = new Meter(_appState, 50,
			RATIO_FROM_MAIN_WINDOW,
			END_ALIGN);
		mainview->addChild(meter);
		_DATA->views.meter = meter;

		auto info = new InfoPanel(_appState, 30,
			RATIO_FROM_MAIN_WINDOW,
			END_ALIGN);
		mainview->addChild(info);
		_DATA->views.infopanel = info;


		auto divider_bottom = new Divider(_appState, END_ALIGN);
		mainview->addChild(divider_bottom);
		_DATA->views.divider_bottom = divider_bottom;

		auto trackview = new HorizontalLayout(_appState, WRAP, 0,
			CENTER_ALIGN);
		mainview->addChild(trackview);
		track_gui_init(_STATE, trackview);
		_DATA->views.active_spaces_buttons[SPACE_WAVEFORM]->setState(PRESSED);

		if (_STATE->dofastrender) {
			_STATE->params[0][ASFX].store(SPACE_SHIFTER);
			_STATE->params[1][ASFX].store(SPACE_SHIFTER);
			_STATE->params[2][ASFX].store(SPACE_SHIFTER);
			_STATE->params[3][ASFX].store(SPACE_SHIFTER);
		}

		auto usage = tsl::get_memory_stats();
#ifndef RELEASEBUILD
		LOGI("After UI init. Memory Usage %g MB  Phys Avail:  %g MB Total: %g MB", static_cast<double>(usage.process_rss_bytes / (1024. * 1024.)), static_cast<double>(usage.available_phys_bytes / (1024. * 1024.)), static_cast<double>(usage.total_phys_bytes / (1024. * 1024.)));
#endif
	}

void tsl::app::setup_main_window(tsl::AppState * _appState) {
		{
			/* Runs on the Android draw thread on resume (justAttached), concurrently
			   with user taps on the input thread. Nesting draw then callback made
			   this half of an ABBA deadlock; scoped_lock takes both atomically. */
			std::scoped_lock lk(_STATE->queue_draw, _STATE->queue_callback);
			_DATA->views.toppanel_main->addRecursiveCB();
			_DATA->views.toppanel_track->addRecursiveCB();
			_DATA->views.controlpanel->addRecursiveCB();
			_DATA->views.sidebar->addRecursiveCB();
			_DATA->views.midilearnbutton->addRecursiveCB();
			_STATE->rootwin->redrawDirect();
			_DATA->views.midilearnbutton->addRecursiveDraw();
			_DATA->views.infopanel->addRecursiveDraw();
			_DATA->views.meter->addRecursiveDraw();
			// _DATA->views.divider_toppanel->redraw();
			_DATA->views.toppanel_main->addRecursiveDraw();
			_DATA->views.toppanel_track->addRecursiveDraw();
			_DATA->views.sidebar->addRecursiveDraw();
			_DATA->views.controlpanel->addRecursiveDraw();
			_DATA->views.divider_left2->redrawDirect();
			_DATA->views.divider_bottom->redrawDirect();
			_DATA->views.divider_cpan_bottom->redrawDirect();
			tsl::graphics::TrackButton::func(_appState,
				_STATE->active_track.load()); /* Click on trackbutton to so we get back to where we left */
		}
	}

