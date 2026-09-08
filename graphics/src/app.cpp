//
// Created by pr on 16.09.23.
//
#include "app.h"

#include <filesystem>

#include "tools/threadtsl.h"
#ifdef HAS_MIDI
#include "Midi.h"
#endif
#ifdef HAS_GUI
#include "skia.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkStream.h"
#include <roboto-regular-reduced.h>
#include <md_reduced_new.h>
#include <material_icons_subset.h>

#include <utility>
#ifdef NEW_UI
#include "ui/view2.h"
#endif
#ifdef HAS_MIDI
#include "MidiLearning.h"
#endif

#if defined(OS_WIN)
#include <windows.h>   // Must come before SkTypeface_win.h
#include "include/ports/SkTypeface_win.h"
#include <shlobj.h> // SHGetFolderPath
#include <filesystem>
#elif defined(OS_MAC) || defined(__APPLE__)
#include "include/ports/SkFontMgr_mac_ct.h"
#else

#include "include/ports/SkFontMgr_empty.h"
#endif



#endif

#include "logger.h"

LogRingBuffer tsl::logRingBuffer{ 200 };
tsl::AppState::AppState(std::function<void()> onDestroy) : onDestroy_{std::move( onDestroy )}
#ifdef HAS_GUI
	, toast{ this }

#ifdef NEW_UI
	, style{ this }
#endif
#endif
#ifdef LICENSE_CHECK_ENABLED
	, lc{ this, tsl::app::lsName }
#endif
#ifdef GRAINSTORM
	, swapPool{ SwapPagePool::default_path()}
#endif
{

#ifdef HAS_GUI
	graphics._appState = this;
#if defined(OS_WIN)
	sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_DirectWrite();
#elif defined(OS_MAC) || defined(__APPLE__)
	sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_CoreText(nullptr);
#else
	sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_Custom_Empty();
#endif
	sk_sp<SkData> fontDataNormal = SkData::MakeWithoutCopy(roboto_reg_data, roboto_reg_size);
	sk_sp<SkTypeface> tf_normal = fontMgr->makeFromData(fontDataNormal);
	font_normal = SkFont(tf_normal); // Or any desired default size
	// Font: Material Design Reduced
	sk_sp<SkData> fontDataMd = SkData::MakeWithoutCopy(materialicons_subset_data, materialicons_subset_size);
	auto streamMd = std::make_unique<SkMemoryStream>(fontDataMd);
	sk_sp<SkTypeface> tf_md = fontMgr->makeFromStream(std::move(streamMd));
	font_md = SkFont(tf_md); // Match your UI style
	font_normal.setEdging(SkFont::Edging::kSubpixelAntiAlias);
	font_normal.setSubpixel(true);
	font_normal.setHinting(SkFontHinting::kSlight);
	font_md.setEdging(SkFont::Edging::kSubpixelAntiAlias);
	font_md.setSubpixel(true);
	font_md.setHinting(SkFontHinting::kSlight);
	sk_sp<SkData> fontDataMd2 = SkData::MakeWithoutCopy(materialicons_subset_data, materialicons_subset_size);
	auto streamMd2 = std::make_unique<SkMemoryStream>(fontDataMd2);
	sk_sp<SkTypeface> tf_md2 = fontMgr->makeFromStream(std::move(streamMd2));
	font_md_kb = SkFont(tf_md2); // Match your UI style
	font_md_kb.setEdging(SkFont::Edging::kSubpixelAntiAlias);
	font_md_kb.setSubpixel(true);
	font_md_kb.setHinting(SkFontHinting::kSlight);

#endif
#ifdef HAS_AUDIO
	player._appState = this;
#endif
#ifdef HAS_MIDI
	for (int chan = 0; chan < NUM_MIDICHANNELS; chan++) {
		for (int num = 0; num < NUM_MIDI_CONTROL; num++) {
			midicontrolevents[chan][num].eventType = tsl::parameters::Eventtype::NoParam;
			midicontrolevents[chan][num].midiState.type = ParameterType_double;
			midicontrolevents[chan][num].midiState.channel = chan;
			midicontrolevents[chan][num].midiState.num = num;
		}
		for (int num = 0; num < NUM_MIDI_NOTEON; num++) {
			midinoteevents[chan][num].eventType = tsl::parameters::Eventtype::NoParam;
			midinoteevents[chan][num].midiState.type = ParameterType_bool;
			midinoteevents[chan][num].midiState.channel = chan;
			midinoteevents[chan][num].midiState.num = num;
		}
	}
#endif

}

tsl::AppState::~AppState() {
	destroyRequested.store(true, std::memory_order_release);
	waitNotify.shutdown();
    if(integrityThread.joinable())
        integrityThread.join();
    if(snapshotDrainThread.joinable())
        snapshotDrainThread.join();
	UiTasksQueue.shutdown();
	WorkerQueue.shutdown();
	RecordingQueue.shutdown();
	if(onDestroy_)
	    onDestroy_();
	pool.printStats();
#if defined(OS_IOS)
	if (iosFilePicker) CFRelease(iosFilePicker);

#if defined(STANDALONE_MODE)
	if (iosStoreKitManager) CFRelease(iosStoreKitManager);

#endif
#endif

	// LOGE("State exit.");
}
#ifdef IS_SINGLETON
tsl::AppState* __STATE{};
#endif

#ifdef PLUGIN_MODE
void tsl::AppState::StartParamChange(tsl::parameters::Event &e)
{

	if (!BeginEndInformHostOfParamChangePrivate) return;
	auto& par = parameters[e.paramIndex];
	if (par.pluginIndex < 0)return;
	int old = par.paramChanging[e.trackIndex].fetch_add(1, std::memory_order_acq_rel);
	if (old == 0)
		BeginEndInformHostOfParamChangePrivate(e, false);
}

void tsl::AppState::EndParamChange(tsl::parameters::Event &e)
{
	if (!BeginEndInformHostOfParamChangePrivate) return;
	auto& par = parameters[e.paramIndex];
	if (par.pluginIndex < 0)return;
	int expected = par.paramChanging[e.trackIndex].load(std::memory_order_acquire);
	while (expected > 0)
	{
		if (par.paramChanging[e.trackIndex].compare_exchange_weak(
			expected, expected - 1,
			std::memory_order_acq_rel,
			std::memory_order_acquire))
		{
			if (expected == 1)
				BeginEndInformHostOfParamChangePrivate(e, true);
			return;
		}
	}
}

#endif





#ifdef __ANDROID__

#include <jni.h>

jclass tsl::android::appclass{};
jclass tsl::android::activityclass{};
jclass tsl::android::recorderclass{};
JavaVM* tsl::android::vm{};
bool tsl::android::useAAudio{};


bool attach(JNIEnv** env) {
	switch (tsl::android::vm->GetEnv((void**)env, JNI_VERSION_1_6)) {
	case JNI_OK:
		return false;
	case JNI_EDETACHED:
		if (tsl::android::vm->AttachCurrentThread(env, nullptr) != 0) {
			LOGE("Could not attach current thread.");
			return false;
		}
		else
			return true;
	case JNI_EVERSION:
		LOGE("Could not attach thread. Invalid Java Version.");
		return false;
	default:
		break;
	}
	return false;
}

bool hasMic() {
	ATTACH
	if (env && tsl::android::appclass) {
		mid = env->GetStaticMethodID(tsl::android::appclass, "getInstance2", "()Ljava/lang/Object;");
		if (mid) {
			jobject appinstance = env->CallStaticObjectMethod(tsl::android::appclass, mid);
			if (appinstance) {
				mid = env->GetMethodID(env->GetObjectClass(appinstance), "hasMic", "()Z");
				if (mid) {
					bool value = (bool)env->CallBooleanMethod(appinstance, mid);
					DETACH
					return value;
				}
			}
		}
		env->ExceptionClear();
	}
	DETACH
	return false;
}

int tprio(int prio) {
	int ret = 0;
	ATTACH
	if (env && tsl::android::appclass) {
		mid = env->GetStaticMethodID(tsl::android::appclass, "setprio", "(II)Z");
		if (mid) {
			pid_t tid = gettid();
			ret = env->CallStaticBooleanMethod(tsl::android::appclass, mid, (jint)tid, (jint)prio);
		} else {
			env->ExceptionClear();
		}
	}
	DETACH
	return ret;
}

#else
int tprio(int prio) {
	tsl::Thread::setPriority(prio);
	return prio;
}
#endif
