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
	// CLOSE THE DOOR ON THE SNAPSHOT DRAIN THREAD BEFORE ANYTHING ELSE.
	//
	// startSnapshotDrainThread (setup.cpp) opens with
	// `if (snapshotDrainActive.exchange(true)) return;` -- so a flag that is
	// already true turns every later call into a no-op. Setting it here is what
	// stops a start that arrives DURING teardown from creating the thread again
	// behind the join below.
	//
	// AND ONE DOES ARRIVE, THOUGH THE PLAYER HAS ALREADY BEEN STOPPED. That is
	// the part worth writing down, because the sequence reads as if it could not
	// happen: destroyEngine (host.cpp) stops the player, and only then deletes
	// this. But POWER is not a call, it is a QUEUED TASK -- setEnginePower does
	// `_DATA->snapShot.add_taskInt([...]{ ...player.play(); })` so that the UI
	// thread is not held for the length of the fade -- and a power-ON queued a
	// moment before teardown is still in that queue when the player is stopped.
	// The snapshot worker drains it afterwards, on its own thread, and the stack
	// is
	//
	//   MPSCWorker<256>::run -> drain -> setEnginePower's lambda
	//     -> Player::play -> onPlayerStart -> startSnapshotDrainThread
	//
	// measured 1 to 4 times per run of the layout suite. So the player is
	// stopped and then started again, by an instruction that was already in
	// flight. Stopping THAT is destroyEngine's business -- shutting the snapshot
	// worker down before stopping the player would discard the task instead of
	// running it -- and it is not done here. What is here is this destructor
	// refusing to be destroyed with a live thread in it, which is what an
	// AppState owes whatever path deleted it.
	snapshotDrainActive.store(true, std::memory_order_release);
	waitNotify.shutdown();
    if(integrityThread.joinable())
        integrityThread.join();
    if(snapshotDrainThread.joinable())
        snapshotDrainThread.join();
	UiTasksQueue.shutdown();
	WorkerQueue.shutdown();
	RecordingQueue.shutdown();
	// THE SNAPSHOT DRAIN THREAD, AGAIN, and this is the join that matters.
	//
	// Between the join above and here, the queued power-on described at the top
	// of this function can reach Player::play() and raise onPlayerStart, and
	// startSnapshotDrainThread creates the thread a second time. The member is
	// then joinable when it is destroyed, and std::thread's destructor answers
	// that with std::terminate: "terminate called without an active exception",
	// from a destructor, with no exception anywhere in the program.
	//
	// It has to be HERE and not further down, because onDestroy_ deletes
	// AppState::data and the drain loop reads _DATA->snapShot. The abort was the
	// visible half of that race; the use-after-free was the quiet one.
	//
	// Safe by construction: destroyRequested is already set and waitNotify is
	// already shut down, so a thread created this late falls straight out of
	// wait_for_signal (WaitNotify.cpp:59) and returns without touching anything.
	if(snapshotDrainThread.joinable())
	    snapshotDrainThread.join();
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
