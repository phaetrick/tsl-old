//
// Created by pr on 16.09.23.
#pragma once
#ifndef POCKET_ANALOG_APP_H
#define POCKET_ANALOG_APP_H
#include "defines.h"
#include "types.h"

#ifdef HAS_SINC
#include "resample.h"
#endif

#include <atomic>
#include <thread>
#include <functional>
#include <string>
#include <deque>
#include <vector>
#include <memory>
#include <cstdint>
#include <cmath>
#include <tools/FunctionQueueWorker.h>

#include <tools/MemPool.h>

#ifdef GRAINSTORM
#include <audio/Recording.h>

#include <tools/SwapRingPool.h>
#include <tools/LockFreeList.h>

struct SyncTarget;

namespace tsl {
    constexpr int displayChannelsSpectrum = 64;
    template<typename T, size_t N>
    class RingBufferMPSCQueue;
}

#include <array>
#include "Follower.h"
#include <map>
#endif
#ifdef HAS_MIDI
#include "Midi.h"
#include "MidiSaver.h"
#endif
#include "tools/queuetsl.h"
#include "params.h"

#ifdef HAS_GUI
#include "skia.h"
#include "Input.h"
#include "view.h"
#include "LogView.h"
#include "Toast.h"
#include "tools/AtomicSharedPtr.h"
#ifdef NEW_UI
#include "ui/style.h"
#endif
struct SafeInsets {
    int left{}, top{}, right{}, bottom{};
};
#ifdef __ANDROID__
#include <android/choreographer.h>
#include <condition_variable>
#include <android/native_window.h>

#endif

#endif

#ifdef HAS_AUDIO
#include "player.h"
#endif

#include "tools/WaitNotify.h"

#ifdef LICENSE_CHECK_ENABLED
#include "lc.h"
#endif

#if defined PLUGIN_MODE || defined STANDALONE_MODE || defined OS_ANDROID
using ToUiThreadQueue = tsl::RingBufferMPSCQueue<stdext::inplace_function<void(), 64>, 128>;
#endif

constexpr int NUM_PARAMS_ALIGNED = (NUM_PARAMS + 7) & ~7;

struct DATA;
constexpr double srStatic = 48000;

namespace tsl {
#ifdef NEW_UI
    namespace ui {
        class View;
        class Layout;
    }
#endif
    namespace graphics {
        class View;
    }

    struct AppState {
        AppState(std::function<void()> onDestroy = nullptr);

        std::function<void()> onDestroy_{};

        ~AppState();

        DATA *data{};
#ifdef IS_MULTITHREADED
        std::atomic<bool> runMultiThreaded{false};
#endif
        alignas(64) std::atomic<MYFLOAT> peak[MAX_CHANNELS]{0.0001, 0.0001};
        //std::array<std::array<std::atomic<MYFLOAT>, NUM_PARAMS>, NUM_TRACKS> params;
        MYFLOAT sr{srStatic}, ksr{srStatic / 64.}, srHost{srStatic};
        MYFLOAT onedsr{1. / srStatic}, onedksr{1. / srStatic * 64.};
        MYFLOAT pidsr{PI_P / srStatic}, twopidsr{TWOPI_P / srStatic};
        int64_t nanospersample{};
        std::atomic<long> buffer_fill{};
        int currentBufSize{1024};
        int prevBufSize{0};
        int maxBufSize{16384};
        std::atomic<int> xruncount{};
        int channels{2};
        int ringSize{};
        int ringPos{};
        int ringMask{};
        std::atomic<int> channelMask{};
        MYFLOAT smoothCoeff{};
        MYFLOAT smoothCoeff2{
            exp(
                DIGITAL_TC / (FXRELEASE * sr * 0.001))
        };
        int32_t bb{};
        int64_t diffprint{};
#if defined(OS_IOS)
        std::function<void(bool, const std::string&)> onPurchaseComplete;
        std::function<void(const char *)> fileBrowserCallback;
        void openFileBrowser();
        void *iosFilePicker = nullptr;
        std::atomic_bool auv3Purchased{};
#if defined(STANDALONE_MODE)
        void purchaseAU();
        void purchasePro();
        void restorePurchases();
        void *iosStoreKitManager = nullptr;
#endif
#endif
#if defined PLUGIN_MODE || defined STANDALONE_MODE || defined OS_ANDROID
        int preDelay{};
#endif
        alignas(64) std::atomic<MYFLOAT> params[NUM_TRACKS][NUM_PARAMS_ALIGNED]{};
        TrackSpecificControl controls[NUM_TRACKS][NUM_PARAMS]{};

        MultiChanPolyPhaseResampler<MYFLOAT, MAX_CHANNELS> rsOut{};
#if defined DOES_INPUT_RESAMPLING
        MultiChanPolyPhaseResampler<MYFLOAT, MAX_CHANNELS> rsIn{};
        MYFLOAT *inputBuf[MAX_CHANNELS]{};
        alignas(64) int inputPos{};
        int inputMask{};
        // rsIn and rsOut are independent phase accumulators, so the number of engine
        // frames rsIn produces per host block and the number rsOut consumes to emit one
        // host block differ by +-1. This holds the engine frames rsOut did not take yet.
        // Measured occupancy is 1..3 over every host rate x block size, so 16 is ample.
        static constexpr int rsOutCarryMax = 16;
        static constexpr int rsOutCarryPrime = 2;   // standing reserve, keeps it off empty
        MYFLOAT rsOutCarry[rsOutCarryMax][2]{};
        int rsOutCarryCount{};
#endif
#ifdef HAS_SINC
        WindowedSinc<MYFLOAT, 16, 1024> windowedSinc;
#endif
#ifdef GRAINSTORM
        SwapPagePool swapPool;
        AppendList<tsl::AudioPoolData> recordings{};
        std::vector<SyncTarget *> syncTargets{};
        std::map<int, Follower> followerMap[4];
#endif

        tsl::PoolSet pool;
        tsl::PoolAllocator<MYFLOAT> alloc{&pool};

        Param parameters[NUM_PARAMS]{};

        std::atomic_bool initdone{};
        std::atomic_bool destroyRequested{};

        std::atomic<int> active_track{};
#ifdef HAS_MIDI
        tsl::AtomicSharedPtr<tsl::graphics::MidiLearning> midiLearning{};
        std::atomic_bool midilearning{};
        std::atomic_bool midilearning_waiting{};
        std::recursive_mutex mutex_midi{};
        char activemidicommand[4]{}; // needs 3 but 4 bcoz of alignment
        tsl::parameters::Event midicontrolevents[NUM_MIDICHANNELS][NUM_MIDI_CONTROL]{};
        tsl::parameters::Event midinoteevents[NUM_MIDICHANNELS][NUM_MIDI_NOTEON]{};
        tsl::QueueUnsafe<std::shared_ptr<MIDIHEADER2>, 100> mappings{};
#endif

#ifdef HAS_GUI
        std::atomic_bool uiReady{};
        SafeInsets safeInsets{};
#ifdef __ANDROID__
        std::thread draw_thread;
        AChoreographer *choreographer{};
        std::counting_semaphore<64> sem{0};

        std::atomic<uint64_t> epoch{0};
        std::atomic<uint64_t> completedEpoch{0};

        // The window atomic we discussed
        std::atomic<ANativeWindow *> window{nullptr};
        std::atomic<bool> windowSizeChanged{false};
        std::atomic<bool> isPaused{true};
        long lastFrameTimeNanos{0};
        // At most one token is ever outstanding. kick() used to release
        // unconditionally, and the draw thread only drains at the top of an
        // iteration -- so a frame longer than 64 vsync periods (~1.07s at 60Hz:
        // first frame after attach, shader compiles, a preset load arriving on
        // toUiThreadQueue) pushed the counter past max(), which is UB.
        //
        // The count carried no information anyway: the draw thread reads
        // `epoch` for the frame number. This is a plain "wake up" signal.
        //
        // The draw thread MUST clear this immediately after acquire() and
        // BEFORE reading epoch. Clearing first means any later kick releases a
        // fresh token; reading epoch after means a kick that raced the clear is
        // still picked up by this frame. It must NOT drain with try_acquire():
        // there is nothing to drain, and consuming a token belonging to the
        // next iteration would park the thread with kickPending already true,
        // which no kick would ever clear.
        std::atomic<bool> kickPending{false};

        uint64_t kick() {
            uint64_t e = epoch.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (!kickPending.exchange(true, std::memory_order_acq_rel))
                sem.release();
            return e;
        }

        // Guards against stacking parallel choreographer chains: onVSync
        // re-posts itself while the window lives, so a second INIT_WINDOW
        // without an intervening teardown would add a chain permanently.
        //
        // ONLY MEANINGFUL FOR THE THREAD THAT POSTED THE CHAIN. This object
        // outlives native_app_glue's thread, which is restarted whenever the
        // activity relaunches (a rotation, unless the manifest declares
        // configChanges). The chain dies with that thread, but the flag does
        // not -- so INIT_WINDOW compares AChoreographer_getInstance() (a
        // per-thread singleton) against the stored one and clears this when it
        // differs. Without that, rendering stops dead after one rotation while
        // input keeps working, which is a genuinely confusing symptom.
        std::atomic<bool> vsyncChainActive{false};
#endif
#ifdef NEW_UI
        tsl::ui::Style style;
#endif
        std::atomic<float> actual_framerate{60.};
        uint64_t lastRender{};
        std::atomic_bool patched{};
        //MPSCQueueWrapper<tsl::graphics::View*,1024> queue_draw;
        int textsize1{}, textsize2{}, circleradius{}, maxCharWidtht1{}, maxCharWidtht2{};
        int knob_height_total{}, knob_height_title{}, knob_height_buttons{}, knob_height_Knob{},
                knob_height_buttonsandKnob{}, knob_height_titleandKnob{}, knob_default_width{};
        float mPpi{160.f}, mMinimumFlingVelocity{75.f}, mMaximumFlingVelocity{12000.f};
        // Android-standard touch slop (8dp): max displacement that still counts as a tap
        float touchSlop() const { return 8.f * mPpi / 160.f; }
        float startyt2{};
        SkFont font_normal{};
        SkFont font_md{}, font_md_kb{};
        bool shiftPressed{}, altPressed{}, controlPressed{};
        int windowWidth{}, windowHeight{};
        std::deque<tsl::graphics::InputEvent> vec_pointers_;
        tsl::graphics::InputSystem::InputState input_state;
        tsl::graphics::InputEventPool inputEventPool;
        tsl::graphics::Graphics graphics{};
        QueueUnsafe<tsl::graphics::View *, 250> queue_draw{}, queue_callback{};

        // The view the render walk is currently inside. Render thread only, so
        // no atomic. Graphics::createWindow() stamps it onto the window it
        // makes, which is how a popup surface knows who owns it -- needed to
        // re-init and repaint popups after a rotation. Null outside the walk.
        tsl::graphics::View *renderingView{};
        tsl::AtomicSharedPtr<tsl::graphics::EnterValue> enterValue{};
        tsl::graphics::View *mCurrentOver{};
        tsl::AtomicSharedPtr<tsl::graphics::LogView> logView{};
        std::function<void(std::string &)> openURL{};
        tsl::AtomicSharedPtr<void> settingsView{};
#ifdef NEW_UI
        QueueUnsafe<tsl::ui::View *, 250> queue_draw2{}, queue_callback2{};
#endif
        tsl::graphics::Toast toast;
        tsl::MPSCWorker<4> toastQueue;
        std::unique_ptr<::tsl::graphics::Layout> rootwin;
        ToUiThreadQueue toUiThreadQueue{};
        tsl::MPSCWorker<16> WorkerQueue;
#ifdef NEW_UI
        std::unique_ptr<::tsl::ui::Layout> rootwin2;
#endif

#endif
#ifdef HAS_AUDIO
        tsl::Player player;
        int format{6};
        bool askName{};

        std::string getAudioFormat() const {
            switch (format) {
                case 12:
                case 0: return ".wav";
                case 10:
                case 11: return ".flac";
                default: return ".mp3";
            }
        }

        std::vector<int16_t> editorCopyBuf[2];
#endif
        tsl::ThreadSignal waitNotify{50};

        tsl::MPSCWorker<16> UiTasksQueue, RecordingQueue;

        std::thread setup_thread;

        std::thread integrityThread;
        std::thread snapshotDrainThread;
        std::atomic<bool> snapshotDrainActive{false};
        std::atomic<int> snapshotDrainSlot{-1};
        std::function<void()> onPlayerStart{};
        std::function<void()> onPlayerStop{};

#ifdef LICENSE_CHECK_ENABLED
        tsl::Lc lc;
#endif
        std::function<void(tsl::parameters::Event &e, tsl::parameters::SenderFlags from)> onParamChange;

#ifdef PLUGIN_MODE
        std::function<void(tsl::parameters::Event &, tsl::parameters::SenderFlags)> InformHostOfParamChange{};
        std::function<void(tsl::parameters::Event &, bool ending)> BeginEndInformHostOfParamChangePrivate{};
        std::function<void(tsl::parameters::Event &)> InformHostOfParamChangeDirect{};
        std::function<void()> RequestHistory{};
        void StartParamChange(tsl::parameters::Event &);
        void EndParamChange(tsl::parameters::Event &);
#endif
        std::atomic_bool dofastrender{};

#ifdef PLATFORM_MOBILE
        std::binary_semaphore readyForUiSetup{0};
#if defined(OS_IOS)
        void checkAndSetupPurchase();
#endif
#endif
    };


    namespace app {
#ifdef IS_SINGLETON

        extern void setup(tsl::AppState *);

        extern void createInstance();

#else
        extern tsl::AppState *setup(bool isRunningAsPlugin = false);
#endif

#ifdef HAS_GUI

        extern void guiSetup(tsl::AppState *);

        extern void setup_init_state(tsl::AppState *);

        extern void setup_main_window(tsl::AppState *);

        extern const uint32_t icon_size;
        extern const uint32_t icon_data[];
        extern const char *appName;
        extern const char *recordingComment;
        inline constexpr const char *bundleName = "com.thesecretlaboratory";
#ifdef IS_PLUGIN

        extern void setup_main_window2(tsl::AppState *);

        extern void guiSetup2(tsl::AppState *);
#endif
#ifdef OS_IOS
        void setupPlatformMetrics(tsl::AppState *);
        bool isRunningAsAppExtension();
        void onIAPComplete(tsl::AppState *, const std::string& productId);
#endif
#ifdef LICENSE_CHECK_ENABLED
        extern const char *lsName;
        extern const int appId;
#endif


#endif
        // `saver` reports whether the file was actually written: on a replace the
        // old file is only removed once the replacement is safely on disk.
        template<typename T>
        void deleteOverwriteFunc(tsl::AppState *_appState, tsl::QueueUnsafe<T, 100> &q, std::string title,
                                 std::function<std::vector<T>()> getItems,
                                 std::function<bool(tsl::AppState *, std::string &)> saver);
    }
}

#ifdef IS_SINGLETON
extern tsl::AppState *__STATE;
#endif

#ifdef __ANDROID__

#include <jni.h>

namespace tsl {
    namespace android {
        extern const char *AppClassPath;
        extern const char *ActivityClassPath;
#ifdef HAS_MICREC
extern const char *RecorderClassPath;
#endif
extern jclass appclass;
extern jclass activityclass;
extern jclass recorderclass;
extern JavaVM *vm;
extern JNINativeMethod methodTable[];
extern int methodTableSize;
extern bool useAAudio;
    }
}

bool attach(JNIEnv * *env);

#define ATTACH     JNIEnv *env; jclass myClazz; jmethodID mid; bool attached = attach(&env);
#define DETACH     if (attached) {tsl::android::vm->DetachCurrentThread();}

#endif
#ifdef HAS_GUI

extern void showToast(tsl::AppState *, const char *string);

#endif

#endif //POCKET_ANALOG_APP_H
