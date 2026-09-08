#include <logger.h>
#include <SkStream.h>
#include "grainstorm.h"
#include "setup.h"
#include "defines.h"
#include "tools.h"
#include "track.h"
#include "synth.h"
#include "MidiReceiver.h"
#include "app.h"

#include "player.h"
#include "DecoderView.h"
#include "MidiSaver.h"
#include "gui/gui.h"
#include "Editor.h"
#include "pv.h"
#include "ParameterInit.h"
#include "tools/PlatformPaths.h"
#ifdef __ANDROID__
#include "Recorder.h"
#include "DecoderAndroid.h"

jstring java_ofl(JNIEnv* env, jclass obj);

void java_set_pauseplayback(JNIEnv* env, jclass obj, jboolean pauseplayback) {
	tsl::AppState* _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return;
	_DATA->pauseplayback.store((bool)pauseplayback);
}


void java_control(JNIEnv* env, jclass obj, jint _c) {
	tsl::AppState* _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false) {
		return;
	}
	auto c = (int)_c;
	if (c == 0) {
		tsl::parameters::Event e;
		e.setup(_STATE, POWERButton, 0);
		e.applyFromExt(_STATE, tsl::parameters::SenderFlags::FromUi);
	}
	else if (c == 1) {
		_DATA->recorder._isRecording.store(false);
	}
}

void java_set_output_format(JNIEnv* env, jclass obj, jint format) {
	tsl::AppState* _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return;
	_STATE->format = (uint8_t)format;
}


void java_set_micrec_format(JNIEnv* env, jclass obj, jint format) {
	tsl::AppState* _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return;
	_DATA->micrecformat = (uint8_t)format;
}
void java_save_audio(JNIEnv* env, jclass thiz, jboolean saveAudio) {
	tsl::AppState* _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return;
	_DATA->saveAudioWithPreset = (bool)saveAudio;
}


JNINativeMethod tsl::android::methodTable[] = {
		 {"filebrowsercallback",       "(Ljava/lang/String;)I",
																				  (void*)filebrowsercallback},
		{"save_preset_callback",      "(Ljava/lang/String;)I",
																				  (void*)save_preset_callback},
		{"read_header",               "(Ljava/lang/String;Ljava/lang/Object;Z)I", (void*)read_header},
		{"java_recorder_callback",    "(JI)J",                                    (void*)java_recorder_callback},
		{"java_record_loop",          "(IJ)I",                                    (void*)java_record_loop},
		 {"java_save_loop",          "(IJ)I",                                    (void*)java_save_loop},

		 {"java_record_live",          "(I)I",                                     (void*)java_record_live},
		{"java_receive_midievent",    "(BBB)V",                                   (void*)java_receive_midievent},
		{"read_midiheader",           "(Ljava/lang/String;Ljava/lang/Object;)I",  (void*)read_midiheader},
		{"save_midimapping_callback", "(Ljava/lang/String;)I",
																				  (void*)save_midimapping_callback},
		{"java_set_output_format",    "(I)V",                                     (void*)java_set_output_format},
		{"java_set_micrec_format",    "(I)V",                                     (void*)java_set_micrec_format},
		{"java_control",              "(I)V",                                     (void*)java_control},

		{"java_set_pauseplayback",    "(Z)V",                                     (void*)java_set_pauseplayback},
		{"guiSetup",                  "(ZII)V",                                   (void*)guiSetup},
		{"java_save_audio",               "(Z)V", (void*)java_save_audio},
		 {"java_load_preset", "(Ljava/lang/String;)V", (void*)java_load_preset},
		 {"java_ofl",               "()Ljava/lang/String;",  (void*)java_ofl},

		 //  {"java_sig",                         "()[B",                                    (void *) java_sig},

};

int32_t tsl::android::methodTableSize =
sizeof(tsl::android::methodTable) / sizeof(tsl::android::methodTable[0]);
const char* tsl::android::AppClassPath = "me/rocks/grainstorm/MyApplication";
const char* tsl::android::ActivityClassPath = "me/rocks/grainstorm/MainActivity";
const char* tsl::android::RecorderClassPath = "me/rocks/grainstorm/Recorder";
const char* tsl::app::recordingComment = "Made with Grainstorm for Android.";
const char* tsl::app::appName = "Grainstorm";
#else
#include "DecoderWindows.h"
const char* tsl::app::recordingComment = "Made with Grainstorm";

const char* tsl::app::appName = "Grainstorm";
#ifdef LICENSE_CHECK_ENABLED
#if defined(PLUGIN_MODE) || defined(OS_IOS)
const char* tsl::app::lsName = "a3f8c21d9e4b7056";
const int tsl::app::appId = 1;
#else
const char* tsl::app::lsName = "b7e2d94f1c6a8031";
const int tsl::app::appId = 0;
#endif // PLUGIN_MODE


#endif


#include <settings.h>
#endif

// ?????????????????????????????????????????????????????????????????????????????
// Unified GenerateWindow
//
// oneshot = false (default) : PERIODIC — denom = N
//                             win[N] == win[0] via guard point
//                             use for oscillators / LFOs
//
// Envelope = true            : SYMMETRIC — denom = N-1
//                             win[0] == win[N-1] == 0 for zero-ended shapes
//                             use for grain envelopes / one-shot shapes
//
// UnityGain = true          : normalise so mean == 1.0
//                             use for spectral analysis, NOT for FIR design
//
// Beta                      : shape parameter for Kaiser, Sinc, Sine (wtSINE)
//
// Alpha                     : flat-top width [0,1] for Tukey / Trapezoid
//                             shape parameter for Gaussian
// ?????????????????????????????????????????????????????????????????????????????
template<typename T>
T Sinc2(T x) {
	if (x > -1.0E-5 && x < 1.0E-5)
		return (1.0);
	return (sin(x) / x);
}

//---------------------------------------------------------------------------
template<typename T>
T Bessel(T x) {
	T Sum = 0.0, XtoIpower;
	int i, j, Factorial = 1;
	for (i = 1; i < 10; i++) {
		XtoIpower = pow(x / 2.0, (T)i);
		Sum += pow(XtoIpower / (T)Factorial, 2.0);
	}
	for (j = 1; j <= i; j++)
		Factorial *= j;
	return (1.0 + Sum);
}

template<typename T>
void GenerateWindow(T* win,
	int  N,
	tsl::envelope::WINDOWTYPE WindowType,
	MYFLOAT alpha,
	bool    oneshot,
	MYFLOAT beta,
	bool    UnityGain)
{
	if (N <= 0) return;

	T Alpha = static_cast<T>(alpha);
	T Beta = static_cast<T>(beta);

	for (int j = 0; j < N; ++j)
		win[j] = T(0);

	const T denom = oneshot ? T(N - 1) : T(N);
	const T invDenom = T(1) / denom;

	const T center = oneshot ? T(N - 1) * T(0.5)
		: T(N) * T(0.5);

	const T half = center;

	// ----------------------------------------------------------
	// LINEAR
	// ----------------------------------------------------------

	if (WindowType == tsl::envelope::SAW) {
		for (int j = 0; j < N; ++j)
			win[j] = T(j) * invDenom;
		goto unity;
	}

	if (WindowType == tsl::envelope::FULL_SAW) {
		for (int j = 0; j < N; ++j)
			win[j] = T(-1) + T(2) * T(j) * invDenom;
		goto unity;
	}

	// ----------------------------------------------------------
	// RECTANGULAR FAMILY
	// ----------------------------------------------------------

	if (WindowType == tsl::envelope::Rectangular) {
		for (int j = 0; j < N; ++j)
			win[j] = T(1);
		goto unity;
	}

	if (WindowType == tsl::envelope::RECTPULS) {
		for (int j = N / 2; j < N; ++j)
			win[j] = T(1);
		goto unity;
	}

	if (WindowType == tsl::envelope::FULL_RECTPULS) {
		for (int j = 0; j < N / 2; ++j) win[j] = T(-1);
		for (int j = N / 2; j < N; ++j) win[j] = T(1);
		goto unity;
	}

	// ----------------------------------------------------------
	// TRIANGLE (correct symmetric form)
	// ----------------------------------------------------------

	if (WindowType == tsl::envelope::TRIANGLE || WindowType == tsl::envelope::wtTRAPEZOID) {
		for (int j = 0; j < N; ++j) {
			T x = T(j) * invDenom;
			win[j] = T(1) - ABS(T(2) * x - T(1));
		}
		goto unity;
	}
	if (WindowType == tsl::envelope::wtCOSINE) {
		for (int j = 0; j < N; ++j)
			win[j] = sin(PI_P * T(j) * invDenom);
		goto unity;
	}

	if (WindowType == tsl::envelope::wtFLATTOP) {
		for (int j = 0; j < N; ++j) {
			win[j] =
				T(0.28106)
				- T(0.520987) * cos(TWOPI_P * T(j) * invDenom)
				+ T(0.19804) * cos(TWOPI_P * T(2 * j) * invDenom);
		}
		goto unity;
	}

	if (WindowType == tsl::envelope::TRIANGLE_FULL) {
		for (int j = 0; j < N; ++j) {
			T x = T(j) * invDenom;        // 0..1

			if (x < T(0.25))
				win[j] = T(4) * x;
			else if (x < T(0.75))
				win[j] = T(2) - T(4) * x;
			else
				win[j] = -T(4) + T(4) * x;
		}
		goto unity;
	}

	// ----------------------------------------------------------
	// TUKEY
	// ----------------------------------------------------------

	if (WindowType == tsl::envelope::wtTUKEY) {

		if (Alpha <= T(0)) Alpha = T(0.0001);
		if (Alpha >= T(1)) Alpha = T(1);

		for (int j = 0; j < N; ++j) {
			T x = T(j) * invDenom;

			if (x < Alpha / T(2))
				win[j] = T(0.5) *
				(T(1) + cos(PI_P * (T(2) * x / Alpha - T(1))));
			else if (x <= T(1) - Alpha / T(2))
				win[j] = T(1);
			else
				win[j] = T(0.5) *
				(T(1) + cos(PI_P *
					(T(2) * x / Alpha - T(2) / Alpha + T(1))));
		}

		goto unity;
	}

	// ----------------------------------------------------------
	// GAUSSIAN / WELCH / BOHMANN / KAISER
	// ----------------------------------------------------------

	if (WindowType == tsl::envelope::wtGAUSSIAN ||
		WindowType == tsl::envelope::wtGAUSS) {

		for (int j = 0; j < N; ++j) {
			T r = (T(j) - center) / (Alpha * half);
			win[j] = exp(T(-0.5) * r * r);
		}
		goto unity;
	}

	if (WindowType == tsl::envelope::wtWELCH) {
		for (int j = 0; j < N; ++j) {
			T r = (T(j) - center) / half;
			win[j] = T(1) - r * r;
		}
		goto unity;
	}

	if (WindowType == tsl::envelope::wtBOHMANN) {
		for (int j = 0; j < N; ++j) {
			T x = ABS(T(j) - center) / half;
			win[j] = (T(1) - x) * cos(PI_P * x)
				+ (T(1) / T(PI_P)) * sin(PI_P * x);
		}
		goto unity;
	}

	if (WindowType == tsl::envelope::wtKAISER) {
		if (Beta < T(0))  Beta = T(0);
		if (Beta > T(10)) Beta = T(10);

		T denomB = T(Bessel(Beta));

		for (int j = 0; j < N; ++j) {
			T r = (T(j) - center) / half;
			T arg = Beta * sqrt(T(1) - r * r);
			win[j] = T(Bessel(arg)) / denomB;
		}
		goto unity;
	}

	// ----------------------------------------------------------
	// COSINE FAMILY (ALL use denom)
	// ----------------------------------------------------------

	if (WindowType == tsl::envelope::SINE_FULL) {
		for (int j = 0; j < N; ++j)
			win[j] = sin(TWOPI_P * T(j) * invDenom);
		goto unity;
	}

	if (WindowType == tsl::envelope::COSINE_FULL) {
		for (int j = 0; j < N; ++j)
			win[j] = cos(TWOPI_P * T(j) * invDenom);
		goto unity;
	}

	if (WindowType == tsl::envelope::wtHANNING || WindowType == tsl::envelope::SINE) {
		for (int j = 0; j < N; ++j)
			win[j] = T(0.5)
			- T(0.5) *
			cos(TWOPI_P * T(j) * invDenom);
		goto unity;
	}

	if (WindowType == tsl::envelope::wtHAMMING) {
		for (int j = 0; j < N; ++j)
			win[j] = T(0.54)
			- T(0.46) *
			cos(TWOPI_P * T(j) * invDenom);
		goto unity;
	}

	if (WindowType == tsl::envelope::wtBLACKMAN) {
		for (int j = 0; j < N; ++j)
			win[j] =
			T(0.42)
			- T(0.50) * cos(TWOPI_P * T(j) * invDenom)
			+ T(0.08) * cos(TWOPI_P * T(2 * j) * invDenom);
		goto unity;
	}

	if (WindowType == tsl::envelope::wtBLACKMAN_HARRIS) {
		for (int j = 0; j < N; ++j)
			win[j] =
			T(0.35875)
			- T(0.48829) * cos(TWOPI_P * T(j) * invDenom)
			+ T(0.14128) * cos(TWOPI_P * T(2 * j) * invDenom)
			- T(0.01168) * cos(TWOPI_P * T(3 * j) * invDenom);
		goto unity;
	}

	if (WindowType == tsl::envelope::wtFLATTOP2) {
		for (int j = 0; j < N; ++j) {
			win[j] =
				T(1.0)
				- T(1.93) * cos(TWOPI_P * T(j) * invDenom)
				+ T(1.29) * cos(TWOPI_P * T(2 * j) * invDenom)
				- T(0.388) * cos(TWOPI_P * T(3 * j) * invDenom)
				+ T(0.032) * cos(TWOPI_P * T(4 * j) * invDenom);

			win[j] /= T(3.8);
		}
		goto unity;
	}

unity:

	if (UnityGain) {
		double sum = 0.0;
		for (int j = 0; j < N; ++j)
			sum += double(win[j]);

		sum /= double(N);

		if (sum != 0.0)
			for (int j = 0; j < N; ++j)
				win[j] = T(double(win[j]) / sum);
	}
}

static void setup_envelopes(tsl::AppState* _appState) {
	std::vector<tsl::envelope::WindowDescriptor> WindowMap = {
		{ "RECTANGULAR",   tsl::envelope::Rectangular,   0.0, 0.0 }, // 0
		{ "SINE LFO",      tsl::envelope::SINE,          0.0, 0.0 }, // 1
		{ "SAW",           tsl::envelope::SAW,           0.0, 0.0 }, // 2
		{ "FULL SAW",      tsl::envelope::FULL_SAW,      0.0, 0.0 }, // 3
		{ "TRIANGLE",      tsl::envelope::wtTRAPEZOID,   0.0,  0.0 }, // 4
		{ "FULL TRIANGLE", tsl::envelope::TRIANGLE_FULL, 0.0, 0.0 }, // 5
		{ "PULSE",         tsl::envelope::RECTPULS,      0.0, 0.0 }, // 6
		{ "FULL PULSE",    tsl::envelope::FULL_RECTPULS, 0.0, 0.0 }, // 7
		{ "SINE",          tsl::envelope::wtCOSINE,      0.0, 0.0 }, // 8
		{ "HANNING",       tsl::envelope::wtHANNING,     0.0,  0.0 }, // 9
		{ "FULL SINE",     tsl::envelope::SINE_FULL,     0.0, 0.0 }, // 10
		{ "FULL COSINE",   tsl::envelope::COSINE_FULL,   0.0, 0.0 }, // 11
		{ "BLACKMAN",      tsl::envelope::wtBLACKMAN,    0.0,  0.0 }, // 12
		{ "KAISER",        tsl::envelope::wtKAISER,      0.0,  5.0 }, // 13
		{ "TUKEY2",        tsl::envelope::wtTUKEY,       0.25,  0.0 }, // 14
		{ "TUKEY3",        tsl::envelope::wtTUKEY,       0.5,  0.0 }, // 15
		{ "TUKEY4",        tsl::envelope::wtTUKEY,       0.75,  0.0 }, // 16
		{ "FLATTOP",       tsl::envelope::wtFLATTOP,     0.0,  0.0 }, // 17
		{ "FLATTOP2",      tsl::envelope::wtFLATTOP2,    0.0,  0.0 }, // 18
	};

	auto& eq = _DATA->eq;
	int32_t mgs = WINDOW_SIZE;

	for (size_t idx = 0; idx < WindowMap.size(); idx++) {
		auto& wd = WindowMap[idx];
		double* t = wd.win = &_DATA->envelopes[idx][0];

		tsl::envelope::GenerateWindow(t, mgs, wd.type,
			wd.alpha,
			wd.beta);          // UnityGain

		t[WINDOW_SIZE] = t[0];
		eq.insert({ wd.name, wd });

		// Post-processing for specific tables
		if (wd.type == tsl::envelope::TRIANGLE_FULL)                        setfulltri(t);
		if (wd.type == tsl::envelope::SINE_FULL)                            setsinewave(t);
		if (wd.type == tsl::envelope::COSINE_FULL)                          setcosinewave(t);
		if (wd.type == tsl::envelope::wtHANNING && wd.alpha == 0.0)        _DATA->hanningwin = t;
		if (wd.type == tsl::envelope::wtBLACKMAN)                           _DATA->blackmanwin = t;
	}
	for (int i = 0; i < std::size(editorcurvenames);i++)
	    eq.insert({ editorcurvenames[i].data(), eq[editfuncs[i].data()]});
	
	//"POLYNOM", "LINE", "RECT"
	// ── Pan table ─────────────────────────────────────────────────────────────
	for (int i = 0; i < TBLSIZE2; i++) {
		MYFLOAT val = (MYFLOAT)i / (float)TBLMASK2;
		_DATA->pan[0][i] = sin((1. - val) * PI_F_P * .5);
		_DATA->pan[1][i] = sin(val * PI_F_P * .5);
	}

	// ── Build windows array from names ────────────────────────────────────────
	size_t i = 0;
	for (const auto& name : envelopesnames) {
		_DATA->windows[i++] = eq[std::string(name)].win;
	}

}

void startSnapshotDrainThread(tsl::AppState* _appState) {
	if (_STATE->snapshotDrainActive.exchange(true, std::memory_order_acq_rel))
		return;
	if (_STATE->snapshotDrainThread.joinable()) {
		// Thread already exists, parked — wake it.
		if (auto slot = _STATE->snapshotDrainSlot.load(std::memory_order_acquire); slot >= 0)
			_STATE->waitNotify.wake_thread(slot);
		return;
	}
	_STATE->snapshotDrainThread = std::thread([_appState]() {
		_STATE->snapshotDrainSlot.store(_STATE->waitNotify.acquire_slot(), std::memory_order_release);
		for (;;) {
			if (_STATE->snapshotDrainActive.load(std::memory_order_acquire))
				_STATE->waitNotify.wait_for_signal(50);
			else
				_STATE->waitNotify.wait_for_signal(); // parked until start or shutdown
			if (_STATE->destroyRequested.load(std::memory_order_acquire))
				return;
			if (!_STATE->snapshotDrainActive.load(std::memory_order_acquire))
				continue;
			if (_DATA->snapShot.queue.size()) {
				_DATA->snapShot.add_task([_STATE] {
					std::lock_guard lk(_DATA->snapShot);
					auto& s = _DATA->snapShot;
					while (auto e = s.queue.try_pop()) {
						e->flags &= ~tsl::parameters::Event::History;
						s.addEvent(std::move(*e));
					}
				});
			}
		}
	});
}

void stopSnapshotDrainThread(tsl::AppState* _appState) {
	// Pause only — the thread parks itself on its next tick and is
	// reused by the next start. Joined once in ~AppState.
	_STATE->snapshotDrainActive.store(false, std::memory_order_release);
}

void cleanUp(tsl::AppState* _appState) {
	{
		std::scoped_lock lk(_STATE->queue_draw, _STATE->queue_callback);
		_appState->queue_draw.reset();
		_appState->queue_callback.reset();
		_appState->graphics.reset();
	
	}
	delete _appState;
}

void oscbnk_flen_setup(uint64_t n, uint64_t& mask, uint64_t& lobits, MYFLOAT& pfrac) {
	lobits = 0ULL;
	mask = 1ULL;
	pfrac = 0.0;
	while (n < OSCBNK_PHSMAX_64) {
		n <<= 1u;
		mask <<= 1u;
		lobits++;
	}
	pfrac = 1.0 / (double)mask;
	mask--;
}


#include "security/signature.h"
#include "degradation.h"
#include <random>
#include <chrono>

#if defined(_WIN32)
static constexpr auto GOLDEN_CERT = tsl::security::make_secure<32>({ 0x27, 0x9F, 0xEC, 0x7E, 0x29, 0x05, 0xFB, 0x82,
	0xAB, 0x6C, 0x35, 0x4B, 0xC3, 0x19, 0x8E, 0xC4,
	0x1B, 0xD7, 0x2F, 0x43, 0x08, 0xAD, 0x3D, 0x72,
	0x79, 0x37, 0xD8, 0xC4, 0xC7, 0xB8, 0xE4, 0x78 });
#elif defined(__APPLE__)
static constexpr auto GOLDEN_CERT = tsl::security::make_secure<32>({ 0xED, 0x4E, 0xF4, 0x80, 0x6A, 0x30, 0xF1, 0x6B,
	0x3B, 0x23, 0xD2, 0x13, 0x29, 0x6D, 0x62, 0x39,
	0x3C, 0x84, 0xB3, 0x89, 0x86, 0xD7, 0x54, 0xD4,
	0x85, 0xB9, 0x05, 0xD3, 0xDD, 0x1E, 0x51, 0x5F });
#elif defined(__ANDROID__)
static constexpr auto GOLDEN_CERT = tsl::security::make_secure<32>({
																		   0x4f, 0xc6, 0x69, 0xb4, 0xe7, 0xe4, 0x52, 0xb3,
																		   0x8e, 0xb2, 0x10, 0x7f, 0xda, 0x1a, 0x33, 0x1c,
																		   0xf3, 0x79, 0xf0, 0x64, 0x1a, 0x48, 0x06, 0x6a,
																		   0x30, 0x14, 0xbf, 0x66, 0x17, 0x6c, 0x93, 0x93
																   });
#endif




#include <tools/sha256.h>
#include <android/integrity.h>

#ifdef IS_SINGLETON
#ifdef __ANDROID__
static std::string getAndroidCacheDir() {
	ATTACH
		std::string path = "";
	if (env && tsl::android::appclass) {
		mid = env->GetStaticMethodID(tsl::android::appclass, "getInstance2", "()Ljava/lang/Object;");
		if (mid) {
			jobject appinstance = env->CallStaticObjectMethod(tsl::android::appclass, mid);
			if (appinstance) {
				mid = env->GetMethodID(env->GetObjectClass(appinstance), "getCacheDir", "()Ljava/io/File;");
				if (mid) {
					jobject cacheDirFile = env->CallObjectMethod(appinstance, mid);
					if (cacheDirFile) {
						mid = env->GetMethodID(env->GetObjectClass(cacheDirFile), "getAbsolutePath", "()Ljava/lang/String;");
						if (mid) {
							jstring pathString = (jstring)env->CallObjectMethod(cacheDirFile, mid);
							if (pathString) {
								const char* pathChars = env->GetStringUTFChars(pathString, nullptr);
								path = pathChars;
								env->ReleaseStringUTFChars(pathString, pathChars);
							}
						}
					}
				}
			}
		}
		env->ExceptionClear();
	}
	DETACH
		return path;
}
#endif

void tsl::app::createInstance() {
#ifdef __ANDROID__
	auto cacheDir = getAndroidCacheDir();
	if (!cacheDir.empty()) {
		tsl::SwapPagePool::set_android_cache_dir(cacheDir);
	}
#endif
	auto _appState = __STATE = new tsl::AppState;
	_appState->data = new DATA(_appState);
};
void tsl::app::setup(tsl::AppState* _appState) {
/*
	if (!verify_integrity(const_cast<const uint8_t*>(sTextHash))) {LOGE("CHECK PASSED.");}
		else {
			LOGE("CHECK FAILED!!!.");
		}
*/
#else
tsl::AppState* tsl::app::setup(bool isRunningAsPlugin) {
	auto _appState = new tsl::AppState();
	_appState->data = new DATA(_appState);
	_STATE->onDestroy_ = [_appState]() {delete _appState->data; };
	_DATA->isRunningAsPlugin = isRunningAsPlugin;
#endif
	_appState->graphics.appDimension = tsl::graphics::Portrait;

	_DATA->midiclockactive = true;
	setup_fx_callbacks(_DATA->callbacks_fx_power);

	_STATE->peak[0] = _STATE->peak[1] = .00001;
	setup_envelopes(_STATE);

	oscbnk_flen_setup(WINDOW_SIZE, _DATA->mask, _DATA->lobits, _DATA->pfrac);

	TRACK::setupTracks(_STATE);
#ifdef __ANDROID__

    ATTACH

		jfieldID jfield = env->GetStaticFieldID(tsl::android::appclass, "hasMic",
			"Z");
	_DATA->hasmic = (bool)env->GetStaticBooleanField(tsl::android::appclass, jfield);

	jfield = env->GetStaticFieldID(tsl::android::appclass, "outputFormat",
		"I");
	_STATE->format = env->GetStaticIntField(tsl::android::appclass, jfield);

	jfield = env->GetStaticFieldID(tsl::android::appclass, "micRecFormat",
		"I");
	_DATA->micrecformat = env->GetStaticIntField(tsl::android::appclass, jfield);

	jfield = env->GetStaticFieldID(tsl::android::appclass, "pausePlayback",
		"Z"); //getNativeOutputSampleRate
	_DATA->pauseplayback = (bool)env->GetStaticBooleanField(tsl::android::appclass, jfield);

	jfield = env->GetStaticFieldID(tsl::android::appclass, "startPoweredOn",
		"Z"); //getNativeOutputSampleRate
	_DATA->startPoweredOn = (bool)env->GetStaticBooleanField(tsl::android::appclass, jfield);
	// The ceiling every allocation is sized against, not the requested block
	// size. These are two different numbers now that the buffer setting has an
	// Auto entry: audioBufSize is -1 there, and the device picks the block size
	// at open time -- long after this runs. maxBufSize is Java's guarantee that
	// no callback will exceed it, so it is what the engine can safely allocate
	// for. synthFunc still checks numFrames against it and reports
	// "BUFFERSIZE TOO BIG!" rather than overrunning, if that guarantee breaks.
	_STATE->maxBufSize = (int)env->GetStaticIntField(tsl::android::appclass,
		env->GetStaticFieldID(
			tsl::android::appclass,
			"maxBufSize",
			"I"));
	_DATA->saveAudioWithPreset = (bool)env->GetStaticBooleanField(tsl::android::appclass, env->GetStaticFieldID(tsl::android::appclass, "saveAudioWithPreset",
		"Z"));

	DETACH
		_STATE->player.init();
#else
	tsl::settings::SettingsManager mgr{ tsl::app::getStoragePath("Grainstorm") };
	_STATE->format = mgr.Get("audio_format", 0);
	_STATE->askName = mgr.Get("ask_name", false);
	_DATA->saveAudioWithPreset = mgr.Get("preset_save_audio", false);
#ifdef OS_IOS
	_STATE->fileBrowserCallback = [_STATE](const char* path) {
		filebrowsercallback(_STATE, path);
	};
#ifdef STANDALONE_MODE
	_STATE->player.preferredBufferSize = mgr.Get("buffer_size", 256);
#endif
	#endif
#endif
	LOGI("Samplerate = %g, Bufsize = %d MaxBufsize = %d", _STATE->sr,
		_STATE->currentBufSize, _STATE->maxBufSize);

	OnGotSampleRate(_STATE);

	_STATE->onPlayerStart = [_appState]() { startSnapshotDrainThread(_appState); };
	_STATE->onPlayerStop = [_appState]() {
		stopSnapshotDrainThread(_appState);
#if defined IS_MULTITHREADED && defined __ANDROID__
		// Re-arm the priming window for the next start. Safe here and only here:
		// the stream is closed by the time this runs, so no callback can be
		// reading it.
		_DATA->startBlocks.store(0, std::memory_order_relaxed);
#endif
	};

	_STATE->integrityThread = std::thread([_appState]() {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dist(240, 480);
		_STATE->waitNotify.wait_for_signal(dist(gen) * 1000);
		if (_STATE->destroyRequested.load())
			return;
		uint8_t computed[32];
#if defined(_WIN32)
		for (auto& b : computed) b = (uint8_t)(rand() % 256);
#else
		arc4random_buf(computed, 32);
#endif
		tsl::security::getCertHash(computed);
		uint8_t stack_hash[32];
		GOLDEN_CERT.reveal(stack_hash);
		auto deg = tsl::security::getDegradation(computed, stack_hash);
		memset(computed, 0, 32);
		memset(stack_hash, 0, 32);
		if (deg != 0) {
			_DATA->toAudioThreadQueue.try_push([_STATE]() {
				for (auto t : _DATA->tracks) {
					auto newfx = std::make_shared<tsl::degradation>(t);
					newfx->activate();
				}
			});
		}
	});

#if IS_MULTITHREADED
	for (int32_t j = 0; j < 4; j++) {
		for (int32_t i = 0; i < _STATE->channels; i++) {
			_STATE->data->channelThreads[j * MAX_CHANNELS + i].startThread(-19);
		}
	}
#if defined __ANDROID__
	_STATE->data->synthThread.startThread(-19);
#endif
#endif
	_STATE->params[0][POWERTRACK] = true;
   

    LOGI("Ready.");
#ifndef IS_SINGLETON
	return _appState;
#endif
}

#include "pv.h"


static void setupBuffers(tsl::AppState* _appState) {
	const int bufsizetotal = _STATE->ringSize;
	const int bufsize_init = _STATE->maxBufSize;
	const int max_grainsize = _DATA->maxgrainsize;
	const int channels = _STATE->channels;

#if defined DOES_INPUT_RESAMPLING
	auto inputSize = next_pow_2((3 * max_grainsize + max_grainsize * 8) + 64);
	_STATE->inputMask = inputSize - 1;
#endif

	// ── single-pass: dry run (buf=nullptr) computes size,
	//                 wet run (buf=real) assigns pointers ──────────────────
	auto run = [&](uint8_t* buf) -> int64_t {
		int64_t off = 0;

		auto next = [&](size_t bytes) -> uint8_t* {
			// align to CACHELINE
			off = (off + tsl::CACHELINE - 1) & ~(int64_t)(tsl::CACHELINE - 1);
			uint8_t* p = buf ? buf + off : nullptr;
			off += (int64_t)bytes;
			return p;
			};

		for (int32_t j = 0; j < 4; j++) {
			TRACK& t = *_DATA->tracks[j];

			t.ringbuffer[0] = (MYFLOAT*)next(bufsizetotal * sizeof(MYFLOAT));
			t.ringbuffer[1] = (MYFLOAT*)next(bufsizetotal * sizeof(MYFLOAT));
			t.lfobuffer[0] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));
			t.lfobuffer[1] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));
			t.lfobuffer[2] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));
			t.envf_buffer[0] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));
			t.envf_buffer[1] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));
			t.out_buf[0] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));
			t.out_buf[1] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));

			for (int32_t i = 0; i < channels; i++) {
				t.grain_buffer[i] = (MYFLOAT*)next(max_grainsize * sizeof(MYFLOAT));
				t.graingen.buf[i] = (int*)next(bufsize_init * sizeof(int));
				t.graingen.pitchBuf[i] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));
				t.graingen.gainBuf[i] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));
				t.graingen.sizeBuf[i] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));
				t.fft_out[i] = (MYFLOAT*)next(2 * WINDOW_SIZE * sizeof(MYFLOAT));
				// 2 * WINDOW_SIZE, not WINDOW_SIZE: the LPC autocorrelation runs a
				// zero-padded 2N transform through these, so at FFT SIZE 16384 the
				// old WINDOW_SIZE allocation was overrun by a full 128 kB.
				t.fft_help1[i] = (MYFLOAT*)next(2 * WINDOW_SIZE * sizeof(MYFLOAT));
				t.fft_help2[i] = (MYFLOAT*)next(2 * WINDOW_SIZE * sizeof(MYFLOAT));
				t.pitchdetectoutbuf[i] = (MYFLOAT*)next(bufsize_init * sizeof(MYFLOAT));
				t.warp_table[i] = (int*)next(WINDOW_SIZE * sizeof(int));
				t.hpssHist[i] = (MYFLOAT*)next(PV_HPSS_FRAMES * (WINDOW_SIZE / 2) *
					sizeof(MYFLOAT));
				t.freezeMag[i] = (MYFLOAT*)next((WINDOW_SIZE / 2) * sizeof(MYFLOAT));
				t.freezeFreq[i] = (MYFLOAT*)next((WINDOW_SIZE / 2) * sizeof(MYFLOAT));
				t.freezePsi[i] = (MYFLOAT*)next((WINDOW_SIZE / 2) * sizeof(MYFLOAT));
				t.freezePrevPhi[i] = (MYFLOAT*)next((WINDOW_SIZE / 2) * sizeof(MYFLOAT));
				t.duckGain[i] = (MYFLOAT*)next((WINDOW_SIZE / 2) * sizeof(MYFLOAT));
				for (int32_t ind = 0; ind < 2; ind++) {
					t.pv3Peak[i][ind] = (MYFLOAT*)next((WINDOW_SIZE / 2) * sizeof(MYFLOAT));
					t.pv3PeakBin[i][ind] = (int32_t*)next((WINDOW_SIZE / 2) * sizeof(int32_t));
				}

				for (int32_t ind = 0; ind < 2; ind++) {
					for (int32_t bin = 0; bin < 8192 + 2; bin++) {
						t.nodes_pv[i][ind][bin] = (NodePV*)next(sizeof(NodePV));
						if (buf)
							t.nodes_pv[i][ind][bin]->mag = 1.0;
					}
				}
			}
		}

#if defined DOES_INPUT_RESAMPLING
		for (int i = 0; i < MAX_CHANNELS; i++)
			_STATE->inputBuf[i] = (MYFLOAT*)next(inputSize * sizeof(MYFLOAT));
#endif
#if defined IS_MULTITHREADED && defined __ANDROID__
		// One block of interleaved stereo for underrun concealment. Sized off
		// maxBufSize like everything else in this arena, because the callback may
		// hand us any block up to that ceiling and the audio thread must never
		// allocate. Null on the dry run, real on the wet one, same as inputBuf.
		_DATA->concealBuf = (sampleTSL*)next(bufsize_init * 2 * sizeof(sampleTSL));
#endif
		return off;
		};

	// dry run — get total size
	int64_t total = run(nullptr);

	_DATA->bufsize_track = total;
	_DATA->buf.resize(total, 0);

	// wet run — assign pointers
	run(_DATA->buf.data());
}

void OnGotSampleRate(tsl::AppState * _appState) {
	//	_STATE->sr = 31352.34375;
	_STATE->pidsr = PI_P / (double)_STATE->sr;
	_STATE->twopidsr = TWOPI_P / (double)_STATE->sr;
	_STATE->onedsr = 1.f / (double)_STATE->sr;
	_STATE->ksr = _STATE->sr / 64.;
	_STATE->onedksr = 1. / _STATE->ksr;
	_STATE->smoothCoeff = (1. / (0.0033 * _STATE->sr + 1));
	_STATE->smoothCoeff2 = exp(DIGITAL_TC / (FXRELEASE * _STATE->sr * 0.001));
	_STATE->nanospersample = tsl::time::nanosPerSecond / _STATE->sr;
	_DATA->maxgrainsize = _STATE->sr;
	_DATA->flanger_min_samples = (int)(_STATE->sr / 4000.);
	_DATA->flanger_max_samples = (int)(_STATE->sr / 1000. * 6.25);
	_DATA->flanger_range_samples = _DATA->flanger_max_samples - _DATA->flanger_min_samples;
	_DATA->min_delay_samples = (int)(MIN_DELAY_MS * (double)_STATE->sr * .001);
	_DATA->max_delay_samples = (int)(MAX_DELAY_MS * (double)_STATE->sr * .001);

	auto tmp = _DATA->maxgrainsize + _DATA->flanger_max_samples + _STATE->maxBufSize + 100;
	auto overshoot = tmp % _STATE->maxBufSize;
	tmp += DISTANCE(overshoot, _STATE->maxBufSize);
	//if (DEBUG)
	_STATE->ringSize = next_pow_2(tmp);
	_STATE->ringMask = _STATE->ringSize - 1;

	setupBuffers(_appState);
	TIME_P temp{};
	time_convert(temp, _STATE->sr, (long)_STATE->ringSize);
	LOGI("ringbuffer: %dm : %ds : %d", temp.m, temp.s, temp.ms);
	tsl::initParams(_appState);

	for (auto t : _DATA->tracks)
		t->OnGotSampleRate();


}

void sampleRateFromApp(tsl::AppState * _appState, double sr) {
	_STATE->rsOut.init(_STATE->sr / sr);
	
	_STATE->srHost = sr;
	

#if defined DOES_INPUT_RESAMPLING
	_STATE->rsIn.init(sr / _STATE->sr);
	// Prime the engine->rsOut carry so rsOut can never run dry on the first blocks.
	// Costs rsOutCarryPrime frames of latency at the engine rate (~40us at 48k).
	for (int i = 0; i < tsl::AppState::rsOutCarryPrime; i++)
		_STATE->rsOutCarry[i][0] = _STATE->rsOutCarry[i][1] = 0;
	_STATE->rsOutCarryCount = tsl::AppState::rsOutCarryPrime;
#endif // IS_PLUGIN

	LOGE("Samplerate from app: %g", sr);
}


#ifdef __ANDROID__



/* --------------------------------- ABOUT -------------------------------------
Original Author: Adam Yaxley
Website: https://github.com/adamyaxley
License: See end of file
Obfuscate
Guaranteed compile-time string literal obfuscation library for C++14
Usage:
Pass string literals into the AY_OBFUSCATE macro to obfuscate them at compile
time. AY_OBFUSCATE returns a reference to an ay::obfuscated_data object with the
following traits:
	- Guaranteed obfuscation of string
	The passed string is encrypted with a simple XOR cipher at compile-time to
	prevent it being viewable in the binary image
	- Global lifetime
	The actual instantiation of the ay::obfuscated_data takes place inside a
	lambda as a function level static
	- Implicitly convertable to a char*
	This means that you can pass it directly into functions that would normally
	take a char* or a const char*
Example:
const char* obfuscated_string = AY_OBFUSCATE("Hello World");
std::cout << obfuscated_string << std::endl;
----------------------------------------------------------------------------- */

#ifndef AY_OBFUSCATE_DEFAULT_KEY
// The default 64 bit key to obfuscate strings with.
// This can be user specified by defining AY_OBFUSCATE_DEFAULT_KEY before
// including obfuscate.h
#define AY_OBFUSCATE_DEFAULT_KEY ay::generate_key(__LINE__)
#endif

namespace ay {
	using size_type = unsigned long long;
	using key_type = unsigned long long;

	// Generate a psuedo-random key that spans all 8 bytes
	constexpr key_type generate_key(key_type seed) {
		// Use the MurmurHash3 64-bit finalizer to hash our seed
		key_type key = seed;
		key ^= (key >> 33);
		key *= 0xff51afd7ed558ccd;
		key ^= (key >> 33);
		key *= 0xc4ceb9fe1a85ec53;
		key ^= (key >> 33);

		// Make sure that a bit in each byte is set
		key |= 0x0101010101010101ull;

		return key;
	}

	// Obfuscates or deobfuscates data with key
	constexpr void cipher(char* data, size_type size, key_type key) {
		// Obfuscate with a simple XOR cipher based on key
		for (size_type i = 0; i < size; i++) {
			data[i] ^= char(key >> ((i % 8) * 8));
		}
	}

	// Obfuscates a string at compile time
	template<size_type N, key_type KEY>
	class obfuscator {
	public:
		// Obfuscates the string 'data' on construction
		constexpr obfuscator(const char* data) {
			// Copy data
			for (size_type i = 0; i < N; i++) {
				m_data[i] = data[i];
			}

			// On construction each of the characters in the string is
			// obfuscated with an XOR cipher based on key
			cipher(m_data, N, KEY);
		}

		constexpr const char* data() const {
			return &m_data[0];
		}

		constexpr size_type size() const {
			return N;
		}

		constexpr key_type key() const {
			return KEY;
		}

	private:

		char m_data[N]{};
	};

	// Handles decryption and re-encryption of an encrypted string at runtime
	template<size_type N, key_type KEY>
	class obfuscated_data {
	public:
		obfuscated_data(const obfuscator<N, KEY>& obfuscator) {
			// Copy obfuscated data
			for (size_type i = 0; i < N; i++) {
				m_data[i] = obfuscator.data()[i];
			}
		}

		~obfuscated_data() {
			// Zero m_data to remove it from memory
			for (size_type i = 0; i < N; i++) {
				m_data[i] = 0;
			}
		}

		// Returns a pointer to the plain text string, decrypting it if
		// necessary
		operator char* () {
			decrypt();
			return m_data;
		}

		// Manually decrypt the string
		void decrypt() {
			if (m_encrypted) {
				cipher(m_data, N, KEY);
				m_encrypted = false;
			}
		}

		// Manually re-encrypt the string
		void encrypt() {
			if (!m_encrypted) {
				cipher(m_data, N, KEY);
				m_encrypted = true;
			}
		}

		// Returns true if this string is currently encrypted, false otherwise.
		bool is_encrypted() const {
			return m_encrypted;
		}

	private:

		// Local storage for the string. Call is_encrypted() to check whether or
		// not the string is currently obfuscated.
		char m_data[N];

		// Whether data is currently encrypted
		bool m_encrypted{ true };
	};

	// This function exists purely to extract the number of elements 'N' in the
	// array 'data'
	template<size_type N, key_type KEY = AY_OBFUSCATE_DEFAULT_KEY>
	constexpr auto make_obfuscator(const char(&data)[N]) {
		return obfuscator<N, KEY>(data);
	}
}

// Obfuscates the string 'data' at compile-time and returns a reference to a
// ay::obfuscated_data object with global lifetime that has functions for
// decrypting the string and is also implicitly convertable to a char*
#define AY_OBFUSCATE(data) AY_OBFUSCATE_KEY(data, AY_OBFUSCATE_DEFAULT_KEY)

// Obfuscates the string 'data' with 'key' at compile-time and returns a
// reference to a ay::obfuscated_data object with global lifetime that has
// functions for decrypting the string and is also implicitly convertable to a
// char*
#define AY_OBFUSCATE_KEY(data, key) \
    []() -> ay::obfuscated_data<sizeof(data)/sizeof(data[0]), key>& { \
        static_assert(sizeof(decltype(key)) == sizeof(ay::key_type), "key must be a 64 bit unsigned integer"); \
        static_assert((key) >= (1ull << 56), "key must span all 8 bytes"); \
        constexpr auto n = sizeof(data)/sizeof(data[0]); \
        constexpr auto obfuscator = ay::make_obfuscator<n, key>(data); \
        static auto obfuscated_data = ay::obfuscated_data<n, key>(obfuscator); \
        return obfuscated_data; \
    }()
jstring java_ofl(JNIEnv * env, jclass obj) {
	auto& obfuscated_key = AY_OBFUSCATE("\"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAs2Dy41ouuL6GC+ZtaCMSjixpe/E97tEA3Ndghe75D7XQTU5xIO9akq2ecVdBP8NFI27yf28Og3QQT2hNTelH/NYw8cWYAJFdYw8jkRR1vpB4dgixqjCpVx6bA5WIMdWgrfsxp/zRlzI9WG3stNNXlCPv4aiZZ0aAbLnPxnPavJWvy+feS8PboYxS2HFamu6dKWtcClqlUEi8zGjYcULLmyoI2vYgls/IVibV2I4TfqbFeYfF44J6s+yARGz+QsRbP62BgSYMOr2BdlCws0M7Nwzn63oyIsmpCiBeUReEXISu0wmcSgg0pmVhRicwBQ9bmPoiQ/KLF2JbsaORQjqZHwIDAQAB");

	// 2. Accessing it as a (char*) implicitly calls .decrypt()
	char* plain_text = (char*)obfuscated_key;

	// 3. Create the Java String (The JVM copies the data here)
	jstring result = env->NewStringUTF(plain_text);

	// 4. CRITICAL: Re-encrypt the static buffer immediately
	// This wipes the plain-text key from the C++ static memory segment
	obfuscated_key.encrypt();

	return result;
}
#endif