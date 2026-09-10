#ifndef GUI_H
#define GUI_H

#include "types.h"
#include <app.h>

bool progress(double progress, const char *message);
void callbackFXSwitch(tsl::AppState* _appState, int id, int space);

namespace tsl { namespace app {
    void guiSetup(tsl::AppState* _appState);
    void setup_main_window(tsl::AppState* _appState);
    // Trial gate verdicts, mirrored from TrialGate.java.
    enum : int { kTrialNone = 0, kTrialActive = 1, kTrialExpired = 2 };
    // Java's answer, which can arrive before the UI exists, during the free
    // session's countdown, or after its wall is already up.
    void setTrialState(tsl::AppState* _appState, int state);
    // The gate (Android; a no-op when dofastrender is set). Called at the end of
    // setup_main_window so any wall lands above a freshly registered root. An
    // active trial runs uncapped; an expired one is walled; anything else -- no
    // token yet, server unreachable -- falls back to the 10-minute session.
    void startSessionCap(tsl::AppState* _appState);
    // Where modulation can take `pid`, in the parameter's own value domain. Installed
    // as Knob::modRangeProvider so knobs can draw their mod range — see modrange.cpp.
    bool modRangeFor2(tsl::AppState* _appState, int pid, float& lo, float& hi);
    // True for any param that defines some knob's mod range; when one changes, the
    // target knobs have to be repainted since their own value did not move.
    bool isModSource(int pid);
    void redrawModTargets(tsl::AppState* _appState);

    // RAII: a "computing" panel sits over the keyboard for as long as one of these
    // is alive. Construct it inside the worker task that runs the build, around the
    // slow call, exactly as lc.cpp does with AnimatedSpinner.
    //
    // MUST NOT be constructed on the audio thread: it pushes a view into
    // queue_draw, and that lock is forbidden there. The UiTasks worker is the
    // intended caller.
    //
    // Nesting is counted, so overlapping builds show one panel and the last one
    // out takes it down.
    class BuildOverlayScope {
    public:
        explicit BuildOverlayScope(tsl::AppState* appState);
        ~BuildOverlayScope();
        BuildOverlayScope(const BuildOverlayScope&) = delete;
        BuildOverlayScope& operator=(const BuildOverlayScope&) = delete;
    private:
        // Whether this scope actually took a reference. The constructor is a no-op
        // before the GUI exists (headless, or a build requested during setup), and
        // the destructor must not decrement a count it never incremented.
        bool _held{false};
    };
}}

#ifdef ANDROID
#include <jni.h>
void guiSetup(JNIEnv *env, jclass obj, jboolean fastrender, jint screenWidth, jint screenHeight);
#endif
#endif
