#include "logger.h"
#include <android_native_app_glue.h>
#include "Input.h"
#include "view.h"
#include "app.h"
#include "VelocityTracker.h"
#include <android/native_activity.h>
#include <android/window.h>

#include <csignal>

#include <chrono>
#include <thread>
#include <zconf.h>
#if defined RELEASEBUILD1

static std::atomic<int> gSignalStatus{0};

std::thread crashhandlerthread;
std::mutex crashmutex;

void crashHandler() {
    crashmutex.lock();
    bool shown = false;
    /*
    while(gSignalStatus.load() == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
*/

    while (!shown) {
        ATTACH
        jclass clazz = tsl::android::activityclass;
        mid = env->GetStaticMethodID(clazz, "crashHandler", "(Ljava/lang/String;)Z");
        shown = env->CallStaticBooleanMethod(tsl::android::activityclass, mid,
                                             env->NewStringUTF(sys_signame[gSignalStatus]));
        DETACH
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void handler() {
    ATTACH
    jclass clazz = tsl::android::activityclass;
    mid = env->GetStaticMethodID(clazz, "outOfMemory", "()V");
    env->CallStaticVoidMethod(clazz, mid);
    DETACH
}


static void handler2(int sig) {
    gSignalStatus = sig;
    crashmutex.unlock();
    while (true) sleep(1000);
}
#endif

JNIEXPORT jint
JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved) {
    tsl::android::vm = jvm;
#if defined RELEASEBUILD1
    crashmutex.lock();
    crashhandlerthread = std::thread(crashHandler);
    crashhandlerthread.detach();
    std::set_new_handler(handler);

    std::signal(SIGSEGV, handler2);
    std::signal(SIGFPE, handler2);
    std::signal(SIGILL, handler2);
    std::signal(SIGINT, handler2);
    std::signal(SIGABRT, handler2);
    std::signal(SIGTERM, handler2);
#endif

    JNIEnv *env;
    if (jvm->GetEnv((void **) &env, JNI_VERSION_1_6)) {
        LOGE("JNI Version not supported...");
        return JNI_ERR;
    }

    jclass activityClass = env->FindClass(tsl::android::AppClassPath);
    if (!activityClass) {
        LOGE("failed to get %s class reference", tsl::android::AppClassPath);
        return -1;
    }

    env->RegisterNatives(activityClass, tsl::android::methodTable,
                         tsl::android::methodTableSize);
    tsl::android::appclass = (jclass) (env)->NewGlobalRef(activityClass);
    activityClass = env->FindClass(tsl::android::ActivityClassPath);
    if (!activityClass) {
        LOGE("failed to get %s class reference", tsl::android::ActivityClassPath);
        return -1;
    }
    tsl::android::activityclass = (jclass) env->NewGlobalRef(activityClass);
#ifdef HAS_MICREC

    activityClass = env->FindClass(tsl::android::RecorderClassPath);
    if (!activityClass) {
        LOGE("failed to get %s class reference", tsl::android::RecorderClassPath);
        return -1;
    }
    tsl::android::recorderclass = (jclass) env->NewGlobalRef(activityClass);
#endif

    return JNI_VERSION_1_6;
}

static void setup(tsl::AppState *_appState) {
    _STATE->peak[0] = _STATE->peak[1] = .00001;

    ATTACH
    if (env && tsl::android::appclass) {
        jfieldID fid;

        fid = env->GetStaticFieldID(tsl::android::appclass, "dpi", "F");
        if (fid) {
            _STATE->mPpi = env->GetStaticFloatField(tsl::android::appclass, fid) * 160.f;
        } else {
            env->ExceptionClear();
        }

        fid = env->GetStaticFieldID(tsl::android::appclass, "mMinimumFlingVelocity", "F");
        if (fid) {
            _STATE->mMinimumFlingVelocity = env->GetStaticFloatField(tsl::android::appclass, fid);
        } else {
            env->ExceptionClear();
        }

        fid = env->GetStaticFieldID(tsl::android::appclass, "mMaximumFlingVelocity", "F");
        if (fid) {
            _STATE->mMaximumFlingVelocity = env->GetStaticFloatField(tsl::android::appclass, fid);
        } else {
            env->ExceptionClear();
        }

        fid = env->GetStaticFieldID(tsl::android::appclass, "savedChannels", "I");
        if (fid) {
            _STATE->channels = (int) env->GetStaticIntField(tsl::android::appclass, fid);
        } else {
            env->ExceptionClear();
        }

        // The engine's rate is a constant, not the device's -- see
        // tsl::kEngineSampleRate in player.h for why it cannot be otherwise.
        // BuildStream asks the stream for this rate and lets Oboe convert.
        _STATE->sr = tsl::kEngineSampleRate;

        fid = env->GetStaticFieldID(tsl::android::appclass, "useAAudio", "Z");
        if (fid) {
            tsl::android::useAAudio = (bool) env->GetStaticBooleanField(tsl::android::appclass, fid);
        } else {
            env->ExceptionClear();
        }
    }
    DETACH

    tsl::app::setup(_STATE);
}


#define NONE 0
#define DRAG 1
#define ZOOM 2
#define DRAW 3

#include "keyboard.h"
#include <app.h>
#include <MidiLearning.h>
#include <window.h>

using namespace tsl::graphics;

/*
static ImGuiKey ImGui_ImplAndroid_KeyCodeToImGuiKey(int32_t key_code) {
    switch (key_code) {
        case AKEYCODE_TAB:
            return ImGuiKey_Tab;
        case AKEYCODE_DPAD_LEFT:
            return ImGuiKey_LeftArrow;
        case AKEYCODE_DPAD_RIGHT:
            return ImGuiKey_RightArrow;
        case AKEYCODE_DPAD_UP:
            return ImGuiKey_UpArrow;
        case AKEYCODE_DPAD_DOWN:
            return ImGuiKey_DownArrow;
        case AKEYCODE_PAGE_UP:
            return ImGuiKey_PageUp;
        case AKEYCODE_PAGE_DOWN:
            return ImGuiKey_PageDown;
        case AKEYCODE_MOVE_HOME:
            return ImGuiKey_Home;
        case AKEYCODE_MOVE_END:
            return ImGuiKey_End;
        case AKEYCODE_INSERT:
            return ImGuiKey_Insert;
        case AKEYCODE_FORWARD_DEL:
            return ImGuiKey_Delete;
        case AKEYCODE_DEL:
            return ImGuiKey_Backspace;
        case AKEYCODE_SPACE:
            return ImGuiKey_Space;
        case AKEYCODE_ENTER:
            return ImGuiKey_Enter;
        case AKEYCODE_ESCAPE:
            return ImGuiKey_Escape;
        case AKEYCODE_APOSTROPHE:
            return ImGuiKey_Apostrophe;
        case AKEYCODE_COMMA:
            return ImGuiKey_Comma;
        case AKEYCODE_MINUS:
            return ImGuiKey_Minus;
        case AKEYCODE_PERIOD:
            return ImGuiKey_Period;
        case AKEYCODE_SLASH:
            return ImGuiKey_Slash;
        case AKEYCODE_SEMICOLON:
            return ImGuiKey_Semicolon;
        case AKEYCODE_EQUALS:
            return ImGuiKey_Equal;
        case AKEYCODE_LEFT_BRACKET:
            return ImGuiKey_LeftBracket;
        case AKEYCODE_BACKSLASH:
            return ImGuiKey_Backslash;
        case AKEYCODE_RIGHT_BRACKET:
            return ImGuiKey_RightBracket;
        case AKEYCODE_GRAVE:
            return ImGuiKey_GraveAccent;
        case AKEYCODE_CAPS_LOCK:
            return ImGuiKey_CapsLock;
        case AKEYCODE_SCROLL_LOCK:
            return ImGuiKey_ScrollLock;
        case AKEYCODE_NUM_LOCK:
            return ImGuiKey_NumLock;
        case AKEYCODE_SYSRQ:
            return ImGuiKey_PrintScreen;
        case AKEYCODE_BREAK:
            return ImGuiKey_Pause;
        case AKEYCODE_NUMPAD_0:
            return ImGuiKey_Keypad0;
        case AKEYCODE_NUMPAD_1:
            return ImGuiKey_Keypad1;
        case AKEYCODE_NUMPAD_2:
            return ImGuiKey_Keypad2;
        case AKEYCODE_NUMPAD_3:
            return ImGuiKey_Keypad3;
        case AKEYCODE_NUMPAD_4:
            return ImGuiKey_Keypad4;
        case AKEYCODE_NUMPAD_5:
            return ImGuiKey_Keypad5;
        case AKEYCODE_NUMPAD_6:
            return ImGuiKey_Keypad6;
        case AKEYCODE_NUMPAD_7:
            return ImGuiKey_Keypad7;
        case AKEYCODE_NUMPAD_8:
            return ImGuiKey_Keypad8;
        case AKEYCODE_NUMPAD_9:
            return ImGuiKey_Keypad9;
        case AKEYCODE_NUMPAD_DOT:
            return ImGuiKey_KeypadDecimal;
        case AKEYCODE_NUMPAD_DIVIDE:
            return ImGuiKey_KeypadDivide;
        case AKEYCODE_NUMPAD_MULTIPLY:
            return ImGuiKey_KeypadMultiply;
        case AKEYCODE_NUMPAD_SUBTRACT:
            return ImGuiKey_KeypadSubtract;
        case AKEYCODE_NUMPAD_ADD:
            return ImGuiKey_KeypadAdd;
        case AKEYCODE_NUMPAD_ENTER:
            return ImGuiKey_KeypadEnter;
        case AKEYCODE_NUMPAD_EQUALS:
            return ImGuiKey_KeypadEqual;
        case AKEYCODE_CTRL_LEFT:
            return ImGuiKey_LeftCtrl;
        case AKEYCODE_SHIFT_LEFT:
            return ImGuiKey_LeftShift;
        case AKEYCODE_ALT_LEFT:
            return ImGuiKey_LeftAlt;
        case AKEYCODE_META_LEFT:
            return ImGuiKey_LeftSuper;
        case AKEYCODE_CTRL_RIGHT:
            return ImGuiKey_RightCtrl;
        case AKEYCODE_SHIFT_RIGHT:
            return ImGuiKey_RightShift;
        case AKEYCODE_ALT_RIGHT:
            return ImGuiKey_RightAlt;
        case AKEYCODE_META_RIGHT:
            return ImGuiKey_RightSuper;
        case AKEYCODE_MENU:
            return ImGuiKey_Menu;
        case AKEYCODE_0:
            return ImGuiKey_0;
        case AKEYCODE_1:
            return ImGuiKey_1;
        case AKEYCODE_2:
            return ImGuiKey_2;
        case AKEYCODE_3:
            return ImGuiKey_3;
        case AKEYCODE_4:
            return ImGuiKey_4;
        case AKEYCODE_5:
            return ImGuiKey_5;
        case AKEYCODE_6:
            return ImGuiKey_6;
        case AKEYCODE_7:
            return ImGuiKey_7;
        case AKEYCODE_8:
            return ImGuiKey_8;
        case AKEYCODE_9:
            return ImGuiKey_9;
        case AKEYCODE_A:
            return ImGuiKey_A;
        case AKEYCODE_B:
            return ImGuiKey_B;
        case AKEYCODE_C:
            return ImGuiKey_C;
        case AKEYCODE_D:
            return ImGuiKey_D;
        case AKEYCODE_E:
            return ImGuiKey_E;
        case AKEYCODE_F:
            return ImGuiKey_F;
        case AKEYCODE_G:
            return ImGuiKey_G;
        case AKEYCODE_H:
            return ImGuiKey_H;
        case AKEYCODE_I:
            return ImGuiKey_I;
        case AKEYCODE_J:
            return ImGuiKey_J;
        case AKEYCODE_K:
            return ImGuiKey_K;
        case AKEYCODE_L:
            return ImGuiKey_L;
        case AKEYCODE_M:
            return ImGuiKey_M;
        case AKEYCODE_N:
            return ImGuiKey_N;
        case AKEYCODE_O:
            return ImGuiKey_O;
        case AKEYCODE_P:
            return ImGuiKey_P;
        case AKEYCODE_Q:
            return ImGuiKey_Q;
        case AKEYCODE_R:
            return ImGuiKey_R;
        case AKEYCODE_S:
            return ImGuiKey_S;
        case AKEYCODE_T:
            return ImGuiKey_T;
        case AKEYCODE_U:
            return ImGuiKey_U;
        case AKEYCODE_V:
            return ImGuiKey_V;
        case AKEYCODE_W:
            return ImGuiKey_W;
        case AKEYCODE_X:
            return ImGuiKey_X;
        case AKEYCODE_Y:
            return ImGuiKey_Y;
        case AKEYCODE_Z:
            return ImGuiKey_Z;
        case AKEYCODE_F1:
            return ImGuiKey_F1;
        case AKEYCODE_F2:
            return ImGuiKey_F2;
        case AKEYCODE_F3:
            return ImGuiKey_F3;
        case AKEYCODE_F4:
            return ImGuiKey_F4;
        case AKEYCODE_F5:
            return ImGuiKey_F5;
        case AKEYCODE_F6:
            return ImGuiKey_F6;
        case AKEYCODE_F7:
            return ImGuiKey_F7;
        case AKEYCODE_F8:
            return ImGuiKey_F8;
        case AKEYCODE_F9:
            return ImGuiKey_F9;
        case AKEYCODE_F10:
            return ImGuiKey_F10;
        case AKEYCODE_F11:
            return ImGuiKey_F11;
        case AKEYCODE_F12:
            return ImGuiKey_F12;
        default:
            return ImGuiKey_None;
    }
}
*/


static __always_inline int32_t

FindIndex(const AInputEvent *event, int32_t id) {
    for (auto i = 0; i < AMotionEvent_getPointerCount(event); ++i) {
        if (id == AMotionEvent_getPointerId(event, i)) return i;
    }
    return -1;
}

static bool button1Pressed{}, button2Pressed{}, button3Pressed{};

static __always_inline int32_t

FindButton(const AInputEvent *event) {
    int32_t button_state = AMotionEvent_getButtonState(event);
    if ((button_state & AMOTION_EVENT_BUTTON_PRIMARY) != button1Pressed) {
        button1Pressed = !button1Pressed;
        return 0;
    } else if ((button_state & AMOTION_EVENT_BUTTON_SECONDARY) != button2Pressed) {
        button2Pressed = !button2Pressed;
        return 1;
    } else if ((button_state & AMOTION_EVENT_BUTTON_TERTIARY) != button3Pressed) {
        button3Pressed = !button3Pressed;
        return 2;
    } else
        return -1;
}


static void handle_app_command(struct android_app *app, int32_t cmd);

static int32_t callback_input(struct android_app *app, AInputEvent *event);
//static int32_t slidercallback(struct android_app* app, AInputEvent* event);
//static int init_display(DATA *p);



void android_main(struct android_app *app) {
    if (__STATE == nullptr) tsl::app::createInstance();
    app->userData = __STATE;
    if (!__STATE->initdone.load()) {
        LOGI("Fresh start");
        __STATE->setup_thread = std::thread(setup, __STATE);
    }
    app->onAppCmd = handle_app_command;
    app->onInputEvent = callback_input;

    while (!app->destroyRequested) {
        int events;
        struct android_poll_source *source;

        if ((ALooper_pollOnce(-1, nullptr,
                              &events, (void **) &source)) >= 0) {

            if (source != nullptr) {
                source->process(app, source);
            }
        }
    }
}


#include <unordered_map>
#include <vector>
#include <algorithm>
#include <array>
#include <memory>

// Utility functions for better code organization
static int32_t getPointerId(AInputEvent *event, int index) {
    const auto toolType = AMotionEvent_getToolType(event, index);

    if (toolType == AMOTION_EVENT_TOOL_TYPE_FINGER ||
        toolType == AMOTION_EVENT_TOOL_TYPE_UNKNOWN) {
        return AMotionEvent_getPointerId(event, index);
    }

    return FindButton(event); // Your existing function
}

static void initializeInputEvent(tsl::graphics::InputEvent *event,
                                 int action,
                                 int32_t pointerId,
                                 float x,
                                 float y) {
    event->action = action;
    event->pointer_id = pointerId;
    event->x = x;
    event->y = y;
    event->time = tsl::time::nanosecondsSinceEpoch();
}

static bool isViewHit(View *view, const tsl::graphics::InputEvent &event) {
    if (!view->isInside(event)) {
        return false;
    }

    // Check viewport if it has dimensions
    if (view->viewport.width() == 0) {
        return true;
    }

    return (event.x >= view->viewport.x() &&
            event.x < view->viewport.x() + view->viewport.width() &&
            event.y >= view->viewport.y() &&
            event.y < view->viewport.y() + view->viewport.height());
}

static View *findTargetView(tsl::AppState *_STATE, const tsl::graphics::InputEvent &inputEvent) {
    auto node = _STATE->queue_callback._last;

    while (node) {
        View *view = node->data;
        if (view && isViewHit(view, inputEvent)) {
            return view;
        }
        node = node->prev;
    }

    return nullptr;
}

static View *findFocusedView(tsl::AppState *_STATE) {
    auto node = _STATE->queue_callback._last;

    while (node) {
        View *view = node->data;
        if (view && view->hasFocus.load()) {
            return view;
        }
        node = node->prev;
    }

    return nullptr;
}

static void handleViewInteraction(tsl::AppState *_STATE, View *view,
                                  const tsl::graphics::InputEvent *event, bool midilearning) {
#ifdef HAS_MIDI
    if (midilearning && (_STATE->parameters[view->id].flags & Param::MidiParam)) {
        _STATE->UiTasksQueue.add_task([_STATE, id = view->id]{ MidiLearning::wait(_STATE, _STATE->active_track.load(), id); });
    } else if (!midilearning || !(_STATE->parameters[view->id].flags & Param::MidiParam)) {
        view->callback(*event);
    }
#else
    // MidiLearning.cpp is not built without HAS_MIDI, so the learn branch cannot
    // even be referenced here — it would be an undefined symbol at link time.
    (void)midilearning;
    view->callback(*event);
#endif
}

// Optimized pointer down handler with event pooling
static int32_t
handlePointerDown(tsl::AppState *_STATE, AInputEvent *event, int index, bool midilearning) {
    auto &g_event_pool = _STATE->inputEventPool;
    const int32_t pointer_id = getPointerId(event, index);
    if (pointer_id == -1) {
        return 0;
    }

    const float x = AMotionEvent_getX(event, index) - _STATE->graphics.xOffset;
    const float y = AMotionEvent_getY(event, index) - _STATE->graphics.yOffset;

    // Acquire event from pool instead of creating on stack
    tsl::graphics::InputEvent *inputEvent = g_event_pool.acquire();
    if (!inputEvent) {
        // Pool exhausted - fallback or error handling
        return 0;
    }

    initializeInputEvent(inputEvent, tsl::graphics::ACTION_DOWN, pointer_id, x, y);

    /* queue_callback is held by handleMotionEvent for the whole dispatch */

    // Find target view efficiently
    View *targetView = findTargetView(_STATE, *inputEvent);
    if (!targetView) {
        g_event_pool.release(inputEvent);
        return 0;
    }

    inputEvent->v = targetView;
    g_event_pool.addActive(inputEvent);

    // Handle MIDI learning or normal callback
    handleViewInteraction(_STATE, targetView, inputEvent, midilearning);

    return 1;
}

// Optimized pointer up handler with event pooling
static int32_t
handlePointerUp(tsl::AppState *_STATE, AInputEvent *event, int index, bool midilearning) {
    auto &g_event_pool = _STATE->inputEventPool;
    const int32_t released_pointer_id = getPointerId(event, index);
    if (released_pointer_id == -1) {
        return 0;
    }

    const float x = AMotionEvent_getX(event, index) - _STATE->graphics.xOffset;
    const float y = AMotionEvent_getY(event, index) - _STATE->graphics.yOffset;

    // Find the active pointer
    tsl::graphics::InputEvent *pointer = g_event_pool.getActive(released_pointer_id);
    if (!pointer) {
        return 0;
    }

    if (pointer->v) {
        pointer->action = tsl::graphics::ACTION_UP;
        pointer->time = tsl::time::nanosecondsSinceEpoch();
        pointer->x = x;
        pointer->y = y;

        if (!midilearning || !(_STATE->parameters[pointer->v->id].flags & Param::MidiParam)) {
            pointer->v->callback(*pointer);
        }
    }

    // Remove from active list and release to pool
    g_event_pool.removeActive(released_pointer_id);

    return 1;
}

// Handle finger movement (multi-touch) with event pooling
static void handleFingerMove(tsl::AppState *_STATE, AInputEvent *event, bool midilearning) {
    auto &g_event_pool = _STATE->inputEventPool;
    const auto currentTime = tsl::time::nanosecondsSinceEpoch();

    // Pre-build position cache for efficiency
    std::unordered_map<int32_t, std::pair<float, float>> positionCache;
    const int pointerCount = AMotionEvent_getPointerCount(event);

    for (int i = 0; i < pointerCount; ++i) {
        const int32_t pointerId = AMotionEvent_getPointerId(event, i);
        positionCache[pointerId] = {AMotionEvent_getX(event, i) - _STATE->graphics.xOffset,
                                    AMotionEvent_getY(event, i) - _STATE->graphics.yOffset};
    }

    // Update existing active pointers
    const auto &activePointers = g_event_pool.getActivePointers();
    for (size_t i = 0; i < g_event_pool.getActiveCount(); ++i) {
        auto *pointer = activePointers[i];
        if (!pointer) continue;

        auto it = positionCache.find(pointer->pointer_id);
        if (it == positionCache.end()) {
            continue;
        }

        const float newX = it->second.first;
        const float newY = it->second.second;

        if (pointer->x != newX || pointer->y != newY) {
            pointer->x = newX;
            pointer->y = newY;
            pointer->action = tsl::graphics::ACTION_MOVE;
            pointer->time = currentTime;

            if (pointer->v) {
                if (!midilearning || !(_STATE->parameters[pointer->v->id].flags & Param::MidiParam)) {
                    pointer->v->callback(*pointer);
                }
            }
        }
    }
}

// Handle non-finger movement (stylus, etc.) with event pooling
static void
handleNonFingerMove(tsl::AppState *_STATE, AInputEvent *event, int index, bool midilearning) {
    auto &g_event_pool = _STATE->inputEventPool;
    if (g_event_pool.getActiveCount() == 0) {
        return;
    }

    const float x = AMotionEvent_getX(event, index) - _STATE->graphics.xOffset;
    const float y = AMotionEvent_getY(event, index) - _STATE->graphics.yOffset;
    const int32_t pointer_id = getPointerId(event, index);

    // Find the active pointer for this tool
    tsl::graphics::InputEvent *pointer = g_event_pool.getActive(pointer_id);
    if (!pointer) {
        return;
    }

    if (pointer->x != x || pointer->y != y) {
        pointer->x = x;
        pointer->y = y;
        pointer->action = tsl::graphics::ACTION_MOVE;
        pointer->time = tsl::time::nanosecondsSinceEpoch();

        if (pointer->v) {
            if (!midilearning || !(_STATE->parameters[pointer->v->id].flags & Param::MidiParam)) {
                pointer->v->callback(*pointer);
            }
        }
    }
}

// Optimized pointer move handler
static int32_t
handlePointerMove(tsl::AppState *_STATE, AInputEvent *event, int index, bool midilearning) {
    const auto toolType = AMotionEvent_getToolType(event, index);
    const bool isFinger = (toolType == AMOTION_EVENT_TOOL_TYPE_FINGER) ||
                          (toolType == AMOTION_EVENT_TOOL_TYPE_UNKNOWN);

    if (isFinger) {
        handleFingerMove(_STATE, event, midilearning);
    } else {
        handleNonFingerMove(_STATE, event, index, midilearning);
    }

    return 1;
}

// Optimized key event handler with event pooling
static int32_t handleKeyEvent(tsl::AppState *_STATE, AInputEvent *event) {
    const auto event_meta_state = AKeyEvent_getMetaState(event);
    // Legacy globals — key ROUTING uses the per-event mods snapshot below
    _STATE->controlPressed = (event_meta_state & AMETA_CTRL_ON) != 0;
    _STATE->shiftPressed = (event_meta_state & AMETA_SHIFT_ON) != 0;
    _STATE->altPressed = (event_meta_state & AMETA_ALT_ON) != 0;

    const auto action = AKeyEvent_getAction(event);
    const auto keyCode = AKeyEvent_getKeyCode(event);
    const auto key = KeyCodeToVkey(keyCode);

    if (key == VKEY_UNKNOWN ||
        (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP)) {
        return 0;
    }

    std::lock_guard lk(_STATE->queue_callback);

    // Find focused view efficiently
    View *focusedView = findFocusedView(_STATE);
    if (!focusedView) {
        return 0;
    }

    const auto inputAction = (action == AKEY_EVENT_ACTION_UP) ?
                             tsl::graphics::ACTION_KEY_UP : tsl::graphics::ACTION_KEY_DOWN;

    // Create temporary event for key events (these are typically short-lived)
    tsl::graphics::InputEvent keyEvent{
            focusedView, inputAction, key, 0, 0
    };
    // Modifier snapshot travels with the event (the only routing channel)
    if (event_meta_state & AMETA_SHIFT_ON) keyEvent.mods |= tsl::graphics::MOD_SHIFT;
    if (event_meta_state & AMETA_ALT_ON)   keyEvent.mods |= tsl::graphics::MOD_ALT;
    if (event_meta_state & AMETA_CTRL_ON)  keyEvent.mods |= tsl::graphics::MOD_CTRL;

    focusedView->callback(keyEvent);

    return 1;
}

// Separate motion event handler for better organization
static int32_t handleMotionEvent(tsl::AppState *_STATE, AInputEvent *event) {
#ifdef HAS_MIDI
    const bool midilearning = _STATE->midilearning.load() && !_STATE->midilearning_waiting.load();
#else
    // AppState::midilearning only exists behind HAS_MIDI. Everything below
    // takes this as a plain bool parameter, so a constant false leaves the
    // ordinary dispatch path and lets the compiler drop the rest.
    constexpr bool midilearning = false;
#endif

    const auto action = AMotionEvent_getAction(event);
    const auto flags = action & AMOTION_EVENT_ACTION_MASK;
    const int index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    /* Every branch below can reach View::callback, and the ACTION_UP handlers of
       ButtonSpaceSwitch / ButtonEnvelopeSwitch / FloatingView mutate queue_callback
       directly. The draw thread walks and mutates that same intrusive list, so the
       whole dispatch has to be guarded - not just ACTION_DOWN, as it used to be.
       The pool bookkeeping sits inside the guard too: delRecursiveCB() calls
       inputEventPool.invalidateViewEvents(), which raced the unlocked UP path.
       Lock order here is callback -> draw (redraw() takes queue_draw underneath);
       every site that needs both uses std::scoped_lock, so this cannot deadlock. */
    std::lock_guard lk(_STATE->queue_callback);

    switch (flags) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            return handlePointerDown(_STATE, event, index, midilearning);

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            return handlePointerUp(_STATE, event, index, midilearning);

        case AMOTION_EVENT_ACTION_MOVE:
            return handlePointerMove(_STATE, event, index, midilearning);

        case AMOTION_EVENT_ACTION_CANCEL: {
            // Send UP to all active views so they release state (notes off), then
            // drop the pointers -- the gesture is over.
            //
            // This used to keep the pool populated so a late real UP would still
            // route. But the views have already had their UP here, so a second
            // one would be wrong anyway, and the stale entries were the problem:
            // the next DOWN on the same pointer_id took addActive()'s "update
            // existing" path, which retargeted the stale entry (the view that was
            // mid-gesture then never saw ACTION_UP and stayed stuck in its hot
            // state) and leaked the freshly acquired one. A late real UP now
            // finds no active pointer and is ignored, which is what we want.
            auto& g_event_pool = _STATE->inputEventPool;
            const size_t count = g_event_pool.getActiveCount();
            const auto& ptrs = g_event_pool.getActivePointers();
            for (size_t i = 0; i < count; ++i) {
                auto* p = ptrs[i];
                if (p && p->v) {
                    p->action = tsl::graphics::ACTION_UP;
                    p->time = tsl::time::nanosecondsSinceEpoch();
                    if (!midilearning || !(_STATE->parameters[p->v->id].flags & Param::MidiParam))
                        p->v->callback(*p);
                }
            }
            g_event_pool.clearAll();
            return 1;
        }

        case AMOTION_EVENT_ACTION_HOVER_MOVE:
            break;

        default:
            break;
    }

    return 1;
}

// Optimized input callback with better performance and maintainability
static int32_t callback_input(struct android_app *app, AInputEvent *event) {
    const auto _appState = static_cast<tsl::AppState *>(app->userData);
    if (!_appState || !_appState->initdone.load()) {
        return 0;
    }

    const int etype = AInputEvent_getType(event);

    if (etype == AINPUT_EVENT_TYPE_MOTION) {
        return handleMotionEvent(_appState, event);
    }

    if (etype == AINPUT_EVENT_TYPE_KEY) {
        return handleKeyEvent(_appState, event);
    }

    return 0;
}

// Additional optimization: Consider using a spatial data structure for view lookup
// if you have many views and performance is still an issue
class SpatialViewIndex {
private:
    struct ViewBounds {
        View *view;
        float x, y, width, height;
    };

    std::vector<ViewBounds> views_;

public:
    void updateViews(tsl::AppState *_STATE) {
        views_.clear();
        auto node = _STATE->queue_callback._last;

        while (node) {
            if (node->data) {
                View *view = node->data;
                views_.push_back({
                                         view,
                                         view->viewport.x(),
                                         view->viewport.y(),
                                         view->viewport.width(),
                                         view->viewport.height()
                                 });
            }
            node = node->prev;
        }
    }

    View *findHitView(float x, float y) {
        // Could implement quadtree or other spatial indexing here
        // For now, just linear search but with better cache locality
        for (const auto &viewBounds: views_) {
            if (viewBounds.width == 0 ||
                (x >= viewBounds.x && x < viewBounds.x + viewBounds.width &&
                 y >= viewBounds.y && y < viewBounds.y + viewBounds.height)) {
                return viewBounds.view;
            }
        }
        return nullptr;
    }
};

struct JniScope {
    JNIEnv* env;
    bool attached;
    JavaVM* vm;

    JniScope(JavaVM* v) : vm(v), env(nullptr), attached(false) {
        if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached = true;
            }
        }
    }
    ~JniScope() {
        if (attached) vm->DetachCurrentThread();
    }
};
SafeInsets get_system_safe_insets(android_app *app) {
    SafeInsets insets = {0, 0, 0, 0};
    JNIEnv* env;
    jint res = app->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    bool shouldDetach = false;

    if (res == JNI_EDETACHED) {
        if (app->activity->vm->AttachCurrentThread(&env, nullptr) != 0) return insets;
        shouldDetach = true;
    }

    if (!env) return insets;

    // 1. Get SDK Version
    jclass versionClass = env->FindClass("android/os/Build$VERSION");
    if (!versionClass) { if (shouldDetach) app->activity->vm->DetachCurrentThread(); return insets; }
    jfieldID sdkIntFieldID = env->GetStaticFieldID(versionClass, "SDK_INT", "I");
    if (!sdkIntFieldID) { env->DeleteLocalRef(versionClass); if (shouldDetach) app->activity->vm->DetachCurrentThread(); return insets; }
    int sdkInt = env->GetStaticIntField(versionClass, sdkIntFieldID);
    env->DeleteLocalRef(versionClass);

    // 2. Access WindowInsets via DecorView
    jclass activityClass = env->GetObjectClass(app->activity->clazz);
    if (!activityClass) { if (shouldDetach) app->activity->vm->DetachCurrentThread(); return insets; }

    jmethodID getWinMid = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
    if (!getWinMid) { env->DeleteLocalRef(activityClass); env->ExceptionClear(); if (shouldDetach) app->activity->vm->DetachCurrentThread(); return insets; }

    jobject window = env->CallObjectMethod(app->activity->clazz, getWinMid);
    if (!window) { env->DeleteLocalRef(activityClass); if (shouldDetach) app->activity->vm->DetachCurrentThread(); return insets; }

    jclass winCls = env->GetObjectClass(window);
    jmethodID getDecorMid = env->GetMethodID(winCls, "getDecorView", "()Landroid/view/View;");
    if (!getDecorMid) { env->DeleteLocalRef(winCls); env->DeleteLocalRef(window); env->DeleteLocalRef(activityClass); env->ExceptionClear(); if (shouldDetach) app->activity->vm->DetachCurrentThread(); return insets; }

    jobject decorView = env->CallObjectMethod(window, getDecorMid);
    env->DeleteLocalRef(winCls);
    if (!decorView) { env->DeleteLocalRef(window); env->DeleteLocalRef(activityClass); if (shouldDetach) app->activity->vm->DetachCurrentThread(); return insets; }

    jclass decorCls = env->GetObjectClass(decorView);
    jmethodID getRootInsMid = env->GetMethodID(decorCls, "getRootWindowInsets", "()Landroid/view/WindowInsets;");
    if (!getRootInsMid) { env->DeleteLocalRef(decorCls); env->DeleteLocalRef(decorView); env->DeleteLocalRef(window); env->DeleteLocalRef(activityClass); env->ExceptionClear(); if (shouldDetach) app->activity->vm->DetachCurrentThread(); return insets; }

    jobject windowInsets = env->CallObjectMethod(decorView, getRootInsMid);
    env->DeleteLocalRef(decorCls);

    if (windowInsets != nullptr) {
        jclass insetsClass = env->GetObjectClass(windowInsets);

        if (sdkInt >= 30) {
            jmethodID getInsetsID = env->GetMethodID(insetsClass, "getInsets", "(I)Landroid/graphics/Insets;");
            if (getInsetsID) {
                jobject insetsObj = env->CallObjectMethod(windowInsets, getInsetsID, 128); // displayCutout()
                if (insetsObj) {
                    jclass rectClass = env->GetObjectClass(insetsObj);
                    insets.left = env->GetIntField(insetsObj, env->GetFieldID(rectClass, "left", "I"));
                    insets.top = env->GetIntField(insetsObj, env->GetFieldID(rectClass, "top", "I"));
                    insets.right = env->GetIntField(insetsObj, env->GetFieldID(rectClass, "right", "I"));
                    insets.bottom = env->GetIntField(insetsObj, env->GetFieldID(rectClass, "bottom", "I"));
                    env->DeleteLocalRef(rectClass);
                    env->DeleteLocalRef(insetsObj);
                }
            } else env->ExceptionClear();
        }
        else if (sdkInt >= 28) {
            jmethodID getCutoutID = env->GetMethodID(insetsClass, "getDisplayCutout", "()Landroid/view/DisplayCutout;");
            if (getCutoutID) {
                jobject cutout = env->CallObjectMethod(windowInsets, getCutoutID);
                if (cutout != nullptr) {
                    jclass cutoutCls = env->GetObjectClass(cutout);
                    jmethodID leftMid = env->GetMethodID(cutoutCls, "getSafeInsetLeft", "()I");
                    jmethodID topMid = env->GetMethodID(cutoutCls, "getSafeInsetTop", "()I");
                    jmethodID rightMid = env->GetMethodID(cutoutCls, "getSafeInsetRight", "()I");
                    jmethodID bottomMid = env->GetMethodID(cutoutCls, "getSafeInsetBottom", "()I");

                    if (leftMid) insets.left = env->CallIntMethod(cutout, leftMid);
                    if (topMid) insets.top = env->CallIntMethod(cutout, topMid);
                    if (rightMid) insets.right = env->CallIntMethod(cutout, rightMid);
                    if (bottomMid) insets.bottom = env->CallIntMethod(cutout, bottomMid);

                    env->DeleteLocalRef(cutoutCls);
                    env->DeleteLocalRef(cutout);
                }
            } else env->ExceptionClear();
        }

        env->DeleteLocalRef(insetsClass);
        env->DeleteLocalRef(windowInsets);
    }

    env->DeleteLocalRef(decorView);
    env->DeleteLocalRef(window);
    env->DeleteLocalRef(activityClass);
    if (shouldDetach) {
        app->activity->vm->DetachCurrentThread();
    }
    return insets;
}

void set_sticky_immersive(android_app* app) {
    JniScope jni(app->activity->vm);
    if (!jni.env) return;

    // 1. Get Window
    jclass actClass = jni.env->GetObjectClass(app->activity->clazz);
    if (!actClass) return;

    jmethodID getWinID = jni.env->GetMethodID(actClass, "getWindow", "()Landroid/view/Window;");
    if (!getWinID) { jni.env->ExceptionClear(); jni.env->DeleteLocalRef(actClass); return; }

    jobject window = jni.env->CallObjectMethod(app->activity->clazz, getWinID);
    jni.env->DeleteLocalRef(actClass);
    if (!window) return;

    // 2. Get DecorView
    jclass winClass = jni.env->GetObjectClass(window);
    jmethodID getDecorID = jni.env->GetMethodID(winClass, "getDecorView", "()Landroid/view/View;");
    if (!getDecorID) { jni.env->ExceptionClear(); jni.env->DeleteLocalRef(winClass); jni.env->DeleteLocalRef(window); return; }

    jobject decorView = jni.env->CallObjectMethod(window, getDecorID);
    jni.env->DeleteLocalRef(winClass);
    jni.env->DeleteLocalRef(window);

    if (decorView) {
        // 3. Set Flags (Sticky Immersive)
        int flags = 0x00000100 | 0x00000200 | 0x00000400 | // Layout Stable/HideNav/Fullscreen
                    0x00000002 | 0x00000004 | 0x00001000;  // HideNav/Fullscreen/ImmersiveSticky

        jclass viewClass = jni.env->GetObjectClass(decorView);
        jmethodID setVisID = jni.env->GetMethodID(viewClass, "setSystemUiVisibility", "(I)V");
        if (setVisID) {
            jni.env->CallVoidMethod(decorView, setVisID, flags);
        } else {
            jni.env->ExceptionClear();
        }

        jni.env->DeleteLocalRef(viewClass);
        jni.env->DeleteLocalRef(decorView);
    }
}


void onVSync(long frameTimeNanos, void* data)
{
    auto* s = static_cast<tsl::AppState*>(data);
    if (s->destroyRequested.load()) {
        /* The chain ends here too, so the flag MUST be cleared -- every exit
           that does not re-post has to, or INIT_WINDOW's exchange() sees a
           chain that no longer exists and never starts a new one. That is a
           dead render thread: it sleeps in sem.acquire() with nothing left to
           kick it, while input keeps working, because input is dispatched on
           the glue thread's ALooper and does not depend on the choreographer.
           Reached whenever a vsync lands between destroyRequested going true
           and the INIT_WINDOW that clears it -- i.e. an ordinary stop/resume. */
        s->vsyncChainActive.store(false, std::memory_order_release);
        return;
    }
    long last = s->lastFrameTimeNanos;

    if (last != 0) {
        long delta = frameTimeNanos - last;

        if (delta > 0) {
            double hz = 1e9 / (double)delta;

            if (hz > 1.0 && hz < 240.0) {
                s->actual_framerate.store(hz, std::memory_order_release);
            }
        }
    }

    s->lastFrameTimeNanos = frameTimeNanos;

    // 3. ONLY wake the render thread if we are active
    // If paused, we don't call releaseVSync(), so the render thread
    // stays in a deep kernel sleep.
    if (!s->isPaused.load(std::memory_order_acquire)) {
        s->kick();
    }

    // 4. THE CHAIN: Keep requesting VSync as long as the window is valid.
    // This allows your FPS counter/math to stay accurate during the
    // transition, but stops the Render Thread from actually working.
    if (s->window.load(std::memory_order_acquire) != nullptr) {
        AChoreographer_postFrameCallback(s->choreographer, onVSync, data);
    }
    else {
        // Chain ends here; INIT_WINDOW is free to start a new one.
        s->vsyncChainActive.store(false, std::memory_order_release);
    }
}

/* Every wait below is a wait by the Java UI thread: native_app_glue blocks it
   inside android_app_set_window() / set_activity_state() until this handler
   returns, so an unbounded wait on the draw thread is an ANR with the app's own
   code on the stack.

   Bounded, and the caller decides what a timeout means. Polling rather than
   atomic::wait() because the latter has no timed form; these are rare lifecycle
   events, so 1ms ticks cost nothing. */
static bool waitForEpoch(tsl::AppState *s, uint64_t target,
                         std::chrono::milliseconds budget) {
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (s->completedEpoch.load(std::memory_order_acquire) < target) {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

static void handle_app_command(struct android_app *app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            auto* s = static_cast<tsl::AppState*>(app->userData);
            if (!s) break;
            /* Normally null -- TERM_WINDOW cleared and released it. If it is
               not, the draw thread may be inside eglCreateWindowSurface() or
               ANativeWindow_getWidth() on this very window, so it gets the same
               handshake TERM_WINDOW uses. Releasing it unsynchronised was a
               use-after-free waiting for two INIT_WINDOWs in a row. */
            auto prev = s->window.exchange(nullptr, std::memory_order_acq_rel);
            if (prev != nullptr) {
                if (waitForEpoch(s, s->kick(), std::chrono::milliseconds(1000)))
                    ANativeWindow_release(prev);
                else
                    LOGE("draw thread did not ack; leaking one ANativeWindow ref "
                         "rather than freeing a window still in use");
            }
            if (app->window == nullptr) {
                /* Nothing to attach to. Publishing null and returning leaves the
                   draw thread parked on its win==nullptr branch; the next
                   INIT_WINDOW with a real surface starts it. Acquiring here
                   dereferenced the null we had just diagnosed. */
                LOGW("NDK glue lied! app->window is null during INIT - skipping attach.");
                s->window.store(nullptr, std::memory_order_release);
                break;
            }
            s->window.store(app->window, std::memory_order_release);
            ANativeWindow_acquire(app->window);
// A. Set the Hardware Window Flags (C++ Header constants)
            ANativeActivity_setWindowFlags(app->activity,
                                           AWINDOW_FLAG_FULLSCREEN |
                                           AWINDOW_FLAG_LAYOUT_IN_SCREEN |
                                           AWINDOW_FLAG_LAYOUT_NO_LIMITS,
                                           0);

            // B. Set the UI Behavior Flags (JNI call)
            //set_sticky_immersive(app);

            /* AChoreographer_getInstance() is a PER-THREAD singleton, and a
               rotation without configChanges relaunches the activity, which
               restarts native_app_glue's thread. A new instance here therefore
               means the previous thread -- and every frame callback posted on
               it -- is gone and will never call back.

               vsyncChainActive lives in AppState, which outlives that thread,
               so it would stay true forever and the guard below would never
               post again: no vsync, no kick(), draw thread parked in
               sem.acquire() while input keeps working on the new thread's
               ALooper. Rendering dead after exactly one rotation.

               Only the same-thread case can have a live chain, so that is the
               only case the anti-stacking guard may trust. */
            AChoreographer* ch = AChoreographer_getInstance();
            if (ch != s->choreographer)
                s->vsyncChainActive.store(false, std::memory_order_release);
            s->choreographer = ch;
            s->destroyRequested = false;

            s->safeInsets = get_system_safe_insets(app);
            s->window.store(app->window, std::memory_order_release);
            s->windowSizeChanged = true;

            /* TERM_WINDOW sets isPaused, but only APP_CMD_RESUME clears it --
               and a surface-only rotation delivers TERM_WINDOW -> INIT_WINDOW
               with NO PAUSE/RESUME pair. The flag then stays true forever,
               onVSync never reaches its kick(), and the draw thread sleeps in
               sem.acquire() while input keeps working on the glue thread's
               looper: rendering stops dead after one rotation.

               A window has just arrived, so this thread is by definition not
               paused. The genuine no-surface case is guarded by the draw
               loop's own `win == nullptr` branch, not by this flag. */
            s->isPaused.store(false, std::memory_order_release);

            if (!s->draw_thread.joinable())
                s->draw_thread = std::thread(tsl::app::drawThreadGL, app);
            // One chain only. onVSync re-posts itself for as long as the window
            // lives, so posting unconditionally here stacked a second, parallel
            // chain on every INIT_WINDOW that did not follow a teardown.
            const bool chainWasActive =
                s->vsyncChainActive.exchange(true, std::memory_order_acq_rel);
            if (!chainWasActive)
                AChoreographer_postFrameCallback(s->choreographer, onVSync, s);
            /* Insurance, and cheap: if the chain guard above ever suppresses a
               post that was actually needed, this still wakes the thread for
               the resize frame instead of leaving it parked. */
            s->kick();
            break;
        }

        case APP_CMD_TERM_WINDOW: {
            auto* s = static_cast<tsl::AppState*>(app->userData);
            if (!s) break;
            s->isPaused.store(true, std::memory_order_release);
            auto oldWin = s->window.exchange(nullptr, std::memory_order_acq_rel);

            /* The draw thread must be off this window before it is released --
               it may be mid-eglCreateWindowSurface. On timeout, leak the ref:
               a leaked window beats freeing one still in use, and beats an ANR.
               1s is generous next to Android's own surfaceDestroyed budget. */
            const bool acked = waitForEpoch(s, s->kick(), std::chrono::milliseconds(1000));

            if (oldWin) {
                if (acked)
                    ANativeWindow_release(oldWin);
                else
                    LOGE("draw thread did not ack TERM_WINDOW in 1s; leaking one "
                         "ANativeWindow ref rather than freeing it under the thread");
            }

            break;
            /*
            _STATE->destroyRequested.store(true, std::memory_order_release);
            _STATE->vSyncSem.release();
            if (_STATE->draw_thread.joinable())
                _STATE->draw_thread.join();
            break;*/
        }
        case APP_CMD_CONFIG_CHANGED:
        case APP_CMD_WINDOW_RESIZED:
            if (app->window != nullptr) {
                auto* s = static_cast<tsl::AppState*>(app->userData);
                if (s) {
                    s->safeInsets = get_system_safe_insets(app);
                    s->windowSizeChanged.store(true, std::memory_order_release);
                    s->kick();
                }
            }
            break;
        case APP_CMD_PAUSE: {
            auto* s = static_cast<tsl::AppState*>(app->userData);
            s->isPaused.store(true, std::memory_order_release);

            /* Courtesy only: nothing here frees a resource the draw thread is
               using, so a late ack is harmless and onPause must not be held
               hostage to a long frame (a preset load arriving on
               toUiThreadQueue runs on the draw thread). */
            if (!waitForEpoch(s, s->kick(), std::chrono::milliseconds(200)))
                LOGE("draw thread did not ack PAUSE in 200ms; continuing");

            break;
        }

        case APP_CMD_RESUME: {
            auto* s = static_cast<tsl::AppState*>(app->userData);
            s->isPaused.store(false, std::memory_order_release);
            s->kick();
            break;
        }
        case APP_CMD_DESTROY: {
            auto* s = static_cast<tsl::AppState*>(app->userData);
            if (!s) break;
            s->destroyRequested.store(true, std::memory_order_release);
            s->sem.release();
            if (s->draw_thread.joinable())
                s->draw_thread.join();
            break;
        }
        default:
            break;
    }
}