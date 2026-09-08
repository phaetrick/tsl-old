//
// Created by pr on 10.10.23.
//
#include "defines.h"
#include "logger.h"
#include "tools/queuetsl.h"
#include "Input.h"
#include "view.h"
#include "window.h"
#include "skia.h"
#include "app.h"

#if defined USE_IMGUI
#include "imguitsl.h"
#include <imgui.h>
#endif

#include <SkPaint.h>

#include <SkImage.h>
#include <SkData.h>
#include <SkStream.h>
#include <include/codec/SkCodec.h>
#include <include/core/SkImageGenerator.h>
#include <SkBitmap.h>
#include <thread>
#include <functional>
#include <random>
#include <optional>
#if defined OS_IOS || defined OS_MAC
#include "os/log.h"
#endif
sk_sp<SkImage> LoadPNG() {
    sk_sp<SkData> data = SkData::MakeWithoutCopy(tsl::app::icon_data, tsl::app::icon_size);
    auto stream = std::make_unique<SkMemoryStream>(data);

    auto codec = SkCodec::MakeFromStream(std::move(stream));
    if (!codec) {
        LOGE("Failed to create SkCodec");
        return nullptr;
    }

    auto [image, result] = codec->getImage();
    if (result != SkCodec::Result::kSuccess && result != SkCodec::Result::kIncompleteInput) {
        LOGE("Failed to decode image, result=%d", static_cast<int>(result));
        return nullptr;
    }

    return image;
}

using namespace tsl::graphics;

using namespace std::chrono;

static constexpr int64_t fadenanos = 250000000, sleeptime = (tsl::time::nanosPerSecond / 60.);

void tsl::app::setupthr(tsl::AppState *_appState) {
#if defined OS_IOS
    tsl::app::setupPlatformMetrics(_appState);
#endif
    tsl::app::guiSetup(_appState);
    _STATE->initdone = true;
    _STATE->initdone.notify_all();
}

void tsl::app::resize(tsl::AppState *_appState, int w, int h) {
    _STATE->graphics.onResized(w, h);
#if defined OS_IOS
    if (!_STATE->graphics.hasQueriedInsets())
        return;
#endif
    _STATE->rootwin->init();
    tsl::app::setup_main_window(_STATE);
    _STATE->uiReady = true;
    _STATE->uiReady.notify_all();
}


#if defined PLUGIN_MODE || defined STANDALONE_MODE 

void tsl::app::loop(tsl::AppState* _appState, int w, int h) {
    
    _appState->graphics.BeginFrame();
    auto drawCanvas = _STATE->graphics.getRootCanvas();
    
    if (drawCanvas == nullptr) {
        return;
    }

    auto& q = _STATE->queue_draw;
    {
        /* Same split as the Android draw loop. Host and input are the same thread
           here so there is no cycle to break today, but the lambdas drained below
           are the identical ones that take queue_callback. */
        std::scoped_lock lk(q, _STATE->queue_callback);

        while (auto func = _STATE->toUiThreadQueue.try_pop()) {
            (*func)();
        }
    }
    {
        std::lock_guard lk(q);

        if (q._first) {
#if defined USE_IMGUI
            tsl::imgui::BeginFrame(_STATE->windowWidth, _STATE->windowHeight, _STATE->actual_framerate.load());
#endif
            {
                auto node = _STATE->queue_draw._first;
                while (node) {
                    auto* next = node->next;
                    /* Lets createWindow() stamp the owning view on any window
                       this render() asks for. */
                    _STATE->renderingView = node->data;
                    if (node->data && node->data->viewport.width() > 0 && node->data->viewport.height() > 0) {
                        SkPath path;
                        path.addRect(node->data->viewport, SkPathDirection::kCW);
                        drawCanvas->save();
                        drawCanvas->clipPath(path);
                        node->data->render(drawCanvas);
                        drawCanvas->restore();
                    }
                    else
                        node->data->render(drawCanvas);
                    _STATE->renderingView = nullptr;
                    if (!node->data->perm) {
                        q.del(node);
                    }
                    node = next;
                }
            }
            // getRootCanvas() being non-null does not imply BeginFrame() got a backend surface
            if (auto backEnd = _STATE->graphics.getBackEndSurface())
                drawCanvas->getSurface()->draw(backEnd->getCanvas(),
                _STATE->graphics.xOffset, _STATE->graphics.yOffset, nullptr);

#if defined USE_IMGUI
            _STATE->graphics.flush();
            tsl::imgui::EndFrame();
#endif
        }
    }


    {
        std::lock_guard lk(q);
        if (!_STATE->graphics.windows.empty()) {
            _STATE->graphics.swapWindows();
      
        }
        _STATE->graphics.flush();
        /* After the flush: any GPU work still referencing a closed popup's
           surface has been submitted, so this is where it is safe to let the
           surface go -- on this thread, with the context current. */
        _STATE->graphics.releaseDeadWindows();
    }

    const auto s = tsl::time::nanosecondsSinceEpoch();

    int64_t delta = s - _STATE->lastRender;
    _STATE->lastRender = s;
    if (delta > 0) {
        auto framerate = static_cast<double>(tsl::time::nanosPerSecond / delta);
        _STATE->actual_framerate.store(framerate);
        //if (framerate < 50.)
        //    LOGE("Frames dropped. Framerate: %g", framerate);

    }

}



#else if defined(__ANDROID__)

#include "window_anim.h"
#include "include/effects/SkGradientShader.h"

struct egldata_t {
    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    GLint format;
};

static void setupEGL(egldata_t *egldata) {
    egldata->display = EGL_NO_DISPLAY;
    egldata->context = EGL_NO_CONTEXT;

    const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE,
            EGL_WINDOW_BIT,                          // window surface instead of pixmap or pbuffer surface
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, // configure for opengl es 2
            EGL_STENCIL_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 0,
            EGL_SAMPLES, 4,        // 4x msaa
            EGL_SAMPLE_BUFFERS, 1, // must be 1
            EGL_NONE};

    const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE};

    egldata->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(egldata->display, nullptr, nullptr);

    EGLint numConfigs;
    if (!eglChooseConfig(egldata->display, configAttribs, &egldata->config, 1, &numConfigs)) {
        LOGE("Cannot choose config");
        exit(-1);
    }
    if (numConfigs != 1) {
        LOGE("Did not get exactly one config = %d", numConfigs);
    }

    eglGetConfigAttrib(egldata->display, egldata->config, EGL_NATIVE_VISUAL_ID,
                       &egldata->format);
    egldata->context = eglCreateContext(egldata->display, egldata->config, EGL_NO_CONTEXT,
                                        contextAttribs);
}





static void uiSetupThr() {
    if (__STATE->setup_thread.joinable())
        __STATE->setup_thread.join();

    tsl::app::guiSetup(__STATE);

    __STATE->rootwin->init();

    __STATE->initdone = true;
    __STATE->initdone.notify_all();
}

void signal_ready(struct android_app* app) {
    ANativeActivity* activity = app->activity;
    JavaVM* jvm = activity->vm;
    JNIEnv* env = NULL;

    if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (jvm->AttachCurrentThread(&env, NULL) != 0) return;
    }

    if (env) {
        jclass clazz = env->GetObjectClass(activity->clazz);
        if (clazz) {
            jmethodID methodID = env->GetStaticMethodID(clazz, "onNativeReady", "()V");
            if (methodID) {
                env->CallStaticVoidMethod(clazz, methodID);
            } else {
                env->ExceptionClear();
            }
        }
    }

    jvm->DetachCurrentThread();
}

namespace {
    /* Publishes completedEpoch on every exit from a draw-loop iteration.
       APP_CMD_TERM_WINDOW and APP_CMD_PAUSE block inside handle_app_command
       until this epoch lands, and native_app_glue keeps the Java UI thread
       waiting for that handler to return -- so an iteration that leaves without
       publishing is an ANR, not a dropped frame. Four EGL/Skia failure paths
       used to `continue` without publishing, and by then the vsync chain has
       already stopped (onVSync re-posts only while window != nullptr), so no
       later kick would have released the waiter either.

       The notify matters as much as the store: atomic::wait() is only
       guaranteed to wake on a matching notify. */
    struct EpochPublisher {
        tsl::AppState* st;
        uint64_t       e;
        bool           done{ false };

        ~EpochPublisher() { publish(); }

        void publish() {
            if (done) return;
            done = true;
            st->completedEpoch.store(e, std::memory_order_release);
            st->completedEpoch.notify_all();
        }
    };
} // namespace

void tsl::app::drawThreadGL(struct android_app* app) {
    signal_ready(app);

    const auto _appState = __STATE;
    auto win = _STATE->window.load();
    int w = ANativeWindow_getWidth(win);
    int h = ANativeWindow_getHeight(win);
    ANativeWindow_setBuffersGeometry(win, w, h, WINDOW_FORMAT_RGBX_8888);

    egldata_t egldata{};
    setupEGL(&egldata);

    EGLDisplay& gEglDisplay = egldata.display;
    EGLContext& gEglContext = egldata.context;

    const EGLint surfaceAttribs[] = {
            EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
            EGL_NONE };

    EGLSurface gEglSurface = eglCreateWindowSurface(gEglDisplay, egldata.config, win,
        surfaceAttribs);

    if (!eglMakeCurrent(gEglDisplay, gEglSurface, gEglSurface, gEglContext)) {
        LOGE("Unable to eglMakeCurrent");
        exit(-1);
    }

    // 2. Disable EGL's internal VSync blocking
    // This lets the Choreographer be the only "boss" of your frame rate.
    if (eglSwapInterval(gEglDisplay, 0) == EGL_FALSE) {
        LOGW("Failed to set eglSwapInterval(0). eglSwapBuffers might still block.");
    }


    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);




#if defined USE_IMGUI
    tsl::imgui::OnViewInitialized();
#endif


    if (!_STATE->initdone.load()) {
        tsl::time time;
        time.reset();
        if (_STATE->patched.load()) {
            ImGuiShapeRenderer ImGuiShapeRenderer;
            ImGuiShapeRenderer.init();
            while (time.elapsed() < 3.0) {
                _STATE->sem.acquire();
                _STATE->kickPending.store(false, std::memory_order_release);
                uint64_t e = _STATE->epoch.load(std::memory_order_acquire);

                auto* win = _STATE->window.load(std::memory_order_acquire);
                bool paused = _STATE->isPaused.load(std::memory_order_acquire);

                if (!win || paused) {
                    exit(0);
                }
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                ImGuiShapeRenderer.render(w, h);
                eglSwapBuffers(gEglDisplay, gEglSurface);
                _STATE->completedEpoch.store(e, std::memory_order_release);
            }
            exit(0);
        }
        std::thread t(uiSetupThr);
        sk_sp<SkImage> icon = LoadPNG();
        // Grainstorm keeps the swirler; VOLTAIC's splash is the lightning strike
        // that leaves its V behind (see LogoLightning in window_anim.h).
#ifdef GRAINSTORM
        const bool useLightning = false;
#else
        const bool useLightning = true;
#endif
        std::optional<LogoSwirler> swirlerGL;
        std::optional<LogoLightning> lightningGL;
        if (useLightning) lightningGL.emplace(icon, w, h);
        else swirlerGL.emplace(icon, w, h);
        double fadeout = 0;
        double elapsed = 0;
        /* destroyRequested is part of the condition because APP_CMD_DESTROY
           joins this thread from handle_app_command: without it a destroy during
           the splash waits out the whole animation and then t.join() on top. */
        while (fadeout < 1.0 &&
               !_STATE->destroyRequested.load(std::memory_order_acquire)) {
            _STATE->sem.acquire();
            /* Clear BEFORE reading epoch -- see AppState::kickPending. */
            _STATE->kickPending.store(false, std::memory_order_release);
            uint64_t e = _STATE->epoch.load(std::memory_order_acquire);
            /* Same contract as the main loop: a PAUSE arriving during the splash
               is waiting on this epoch, and the store alone would not wake it. */
            EpochPublisher pub{ _STATE, e };

            auto* win = _STATE->window.load(std::memory_order_acquire);
            bool paused = _STATE->isPaused.load(std::memory_order_acquire);

            if (!win || paused) {
                if (gEglSurface != EGL_NO_SURFACE) {
                    eglMakeCurrent(gEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    eglDestroySurface(gEglDisplay, gEglSurface);
                    gEglSurface = EGL_NO_SURFACE;
                    LOGE("Surface destroyed (Atomic Null)");
                }
                break;
            }
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (_STATE->patched.load()) exit(0);
            if (_STATE->initdone.load() && elapsed > 1.0) {
                fadeout = time.elapsed() - elapsed;
                if (useLightning) {
                    if (lightningGL->renderExit((float)fadeout)) fadeout = 1.0;
                } else
                    swirlerGL->render(fadeout);

            }
            else {
                elapsed = time.elapsed();
                if (useLightning)
                    lightningGL->renderIntro((float)elapsed);
                else
                    swirlerGL->render(elapsed < 1.0 ? (1.0 - elapsed) : 0.0);
            }
            eglSwapBuffers(gEglDisplay, gEglSurface);
        }
        t.join();
    }

    auto& queue_draw = _STATE->queue_draw;

    SkCanvas* drawCanvas{ nullptr };

    SkCanvas* canvas{ nullptr };

    bool justAttached = true; // Force a resize calculation below

    while (!_STATE->destroyRequested.load(std::memory_order_acquire)) {

        _STATE->sem.acquire();
        /* Clear BEFORE reading epoch -- see AppState::kickPending. Draining with
           try_acquire() here would steal the next iteration's token. */
        _STATE->kickPending.store(false, std::memory_order_release);
        uint64_t e = _STATE->epoch.load(std::memory_order_acquire);
        /* Covers every exit from this iteration, including the EGL/Skia failure
           `continue`s below. Anything waiting in handle_app_command is released
           even when the frame is abandoned. */
        EpochPublisher pub{ _STATE, e };
        ANativeWindow* win = _STATE->window.load(std::memory_order_acquire);
        bool isPaused = _STATE->isPaused.load(std::memory_order_acquire);
        if (win == nullptr) {
            if (gEglSurface != EGL_NO_SURFACE) {
                // flush any pending Skia work while context is still current
                if (auto ctx = _STATE->graphics.getContext())
                    ctx->flushAndSubmit();
                /* Framebuffer-bound surfaces only. The popup deque survives, so
                   an open keyboard / save dialog / TrackSettings comes back
                   exactly as it was -- gEglContext outlives gEglSurface, so
                   their render targets were never invalid. */
                _STATE->graphics.dropFramebufferSurfaces(true);
                eglMakeCurrent(gEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                eglDestroySurface(gEglDisplay, gEglSurface);
                gEglSurface = EGL_NO_SURFACE;
            }
            else {
                /* No EGL surface, so no current context -- GPU teardown has to
                   wait for a frame that has one. */
                _STATE->graphics.dropFramebufferSurfaces(false);
            }
            drawCanvas = nullptr;
            canvas = nullptr;
            continue;
        }

        if (isPaused) {
            /* Paused still means attached: the EGL context is current on this
               thread (only the win == nullptr branch above un-currents it), so
               a popup closed while backgrounded gets its surface released here
               rather than sitting in the graveyard until resume. */
            _STATE->graphics.releaseDeadWindows();
            continue;
        }


        // --- 2. HANDLE ATTACH ---
        if (gEglSurface == EGL_NO_SURFACE) {
            justAttached = true;
        }

        // --- 3. HANDLE RESIZE / INITIAL SETUP ---
        if (_STATE->windowSizeChanged.exchange(false) || justAttached) {
            LOGE("Ws change.");
            // GET THE REAL TRUTH FROM THE WINDOW, NOT EGL
            w = ANativeWindow_getWidth(win);
            h = ANativeWindow_getHeight(win);
            /* Only meaningful because dropFramebufferSurfaces() leaves these
               alone -- clear() used to zero them, which made every resume look
               like a size change. */
            const bool sizeChanged = _STATE->graphics.windowWidth != w ||
                                     _STATE->graphics.windowHeight != h;
            if (sizeChanged || justAttached) {
                LOGE("Window size changed. %dx%d -> %dx%d", _STATE->graphics.windowWidth,
                    _STATE->graphics.windowHeight, w, h);
                // This is the "Safe Reset"
                if (gEglSurface != EGL_NO_SURFACE) {
                    eglMakeCurrent(gEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    eglDestroySurface(gEglDisplay, gEglSurface);
                    gEglSurface = EGL_NO_SURFACE;
                }

                // Rebind to the NEW window size
                ANativeWindow_setBuffersGeometry(win, w, h, WINDOW_FORMAT_RGBX_8888);
                gEglSurface = eglCreateWindowSurface(gEglDisplay, egldata.config, win, nullptr);

                if (gEglSurface == EGL_NO_SURFACE) {
                    LOGE("Failed to create surface. EGL Error: %d", eglGetError());
                    continue;
                }

                if (!eglMakeCurrent(gEglDisplay, gEglSurface, gEglSurface, gEglContext)) {
                    LOGE("MakeCurrent failed");
                    eglDestroySurface(gEglDisplay, gEglSurface);
                    gEglSurface = EGL_NO_SURFACE;
                    continue;
                }
                eglSwapInterval(gEglDisplay, 0); // Force VSync to prevent "No Buffer Available"
                // EGL context is current on this thread — safe to init Skia

                /* Raw GL first, then tell Skia its state cache is void - and do it before
                   onResized()/rootwin->init()/setup_main_window() issue their first draws.
                   A plain rotation keeps the GrDirectContext alive with a warm HW-state
                   cache, so Skia skips the glBlendFunc it believes it already set and keeps
                   the non-premultiplied SRC_ALPHA factor below for its own premultiplied
                   draws - alpha gets applied twice. First launch never showed it because
                   graphics.init() below builds a context with an invalidated cache. */
                glViewport(0, 0, w, h);
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                if (auto ctx = _STATE->graphics.getContext()) {
                    ctx->resetContext(kAll_GrBackendState);
                    /* Keep the popup deque in both cases. On a plain resume
                       nothing more is needed. On a rotation the surfaces are
                       still valid but the geometry is stale, so the owners are
                       re-init()ed and repainted below -- getCanvas() then sees
                       the new size and resizes the surface in place, under the
                       same index. */
                    _STATE->graphics.dropFramebufferSurfaces(true);
                }
                else {
                    _STATE->graphics.init();
                    if (!_STATE->graphics.isInitialized()) {
                        LOGE("Skia init failed after MakeCurrent");
                        continue;
                    }
                }
                _STATE->graphics.onResized(w, h);
                _STATE->graphics.BeginFrame();

                drawCanvas = _STATE->graphics.getRootCanvas();
                auto backEnd = _STATE->graphics.getBackEndSurface();
                canvas = backEnd ? backEnd->getCanvas() : nullptr;
                if (!drawCanvas || !canvas) {
                    LOGE("Skia canvas acquisition failed after resize");
                    drawCanvas = nullptr;
                    canvas = nullptr;
                    continue;
                }
                if (_STATE->rootwin) _STATE->rootwin->init();
                tsl::app::setup_main_window(_STATE);

                if (sizeChanged) {
                    /* rootwin->init() and setup_main_window() cover the root
                       tree only. Open popups are not in it -- and most are not
                       perm either, so they were reaped from queue_draw after
                       their first frame and would keep compositing at the old
                       geometry forever. Re-init them against the new size (run
                       after onResized, so _STATE->windowWidth/Height are the new
                       ones) and put them back in the queue for one frame. */
                    for (auto* v : _STATE->graphics.windowOwners()) {
                        v->init();
                        v->redraw();
                    }
                }
            }
            justAttached = false;
        }

#if defined USE_IMGUI
        tsl::imgui::BeginFrame(w, h, _STATE->actual_framerate.load());
#endif
        if (!drawCanvas || !canvas) {
            continue;
        }
        {
            /* The deferred UI lambdas (onChangeSpaceFx & friends) take
               queue_callback while this thread holds queue_draw, which used to
               close an ABBA cycle against the input thread's callback -> draw
               order. scoped_lock acquires both atomically, so this thread never
               *blocks* on queue_callback while owning queue_draw. The drain still
               runs with queue_draw held because those lambdas call the
               non-locking redrawDirect()/addRecursiveDraw() primitives. */
            std::scoped_lock lck(queue_draw, _STATE->queue_callback);

            while (auto func = _STATE->toUiThreadQueue.try_pop()) {
                (*func)();
            }
        }
        {
            std::lock_guard lck(queue_draw); /* the render walk needs queue_draw only */

            auto node = queue_draw._first;

            while (node) {
                auto* next = node->next;
                /* Lets createWindow() stamp the owning view on any window this
                   render() asks for. */
                _STATE->renderingView = node->data;
                if (node->data && node->data->viewport.width() > 0 &&
                    node->data->viewport.height() > 0) {
                    SkPath path;
                    path.addRect(node->data->viewport,
                        SkPathDirection::kCW);
                    drawCanvas->save();
                    drawCanvas->clipPath(path);
                    node->data->render(drawCanvas);
                    drawCanvas->restore();
                }
                else
                    node->data->render(
                        drawCanvas); /* Pass root canvas so view can render to it;*/
                _STATE->renderingView = nullptr;
                if (!node->data->perm) { /* Remove view from rendering if not permanent */
                    queue_draw.del(node);
                }
                node = next;
            }
        }
        canvas->clear(tsl::sk_colours::bg);
        drawCanvas->getSurface()->draw(canvas,
            _STATE->graphics.xOffset, _STATE->graphics.yOffset, nullptr);

        //_STATE->graphics.getBackEndSurface()->getCanvas()->drawImage(drawCanvas->getSurface()->makeImageSnapshot(), 0, 0);
#if defined USE_IMGUI
        _STATE->graphics.flush();

        tsl::imgui::EndFrame();
        {
            std::lock_guard<std::recursive_mutex> lck(_STATE->queue_draw.mutex);
            if (!_STATE->graphics.windows.empty()) {
                _STATE->graphics.swapWindows();
                _STATE->graphics.flush();
            }
        }
#else
        {
            std::lock_guard lck(_STATE->queue_draw);
            if (!_STATE->graphics.windows.empty()) {
                _STATE->graphics.swapWindows();
            }
        }

        _STATE->graphics.flush();
#endif

        eglSwapBuffers(gEglDisplay, gEglSurface);

        /* Popups are closed from the input thread and from workers (decoder
           finish, recorder stop). Those threads unlink the window but leave the
           SkSurface for this one to destroy, with the GL context current. */
        _STATE->graphics.releaseDeadWindows();

        pub.publish();   // frame is on screen; release any pending PAUSE/TERM waiter
    }

#if defined USE_IMGUI
    tsl::imgui::OnViewDestroyed();
#endif

    _STATE->graphics.onViewDestroyed();
    //  tsl::skia::window::clear();

    if (gEglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(gEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (gEglSurface != EGL_NO_SURFACE)
            eglDestroySurface(gEglDisplay, gEglSurface);
        if (gEglContext != EGL_NO_CONTEXT)
            eglDestroyContext(gEglDisplay, gEglContext);
        eglTerminate(gEglDisplay);
    }

}
#endif

#ifdef NEW_UI

#include "ui/view2.h"
void tsl::app::resize2(tsl::AppState* _appState, int w, int h) {
    _STATE->graphics.onResized(w, h);
    //_STATE->graphics.updateUIScale(w, h, 160);

    _STATE->rootwin2->setBounds(0, 0, w, h);
    _STATE->rootwin2->init();
    tsl::app::setup_main_window2(_STATE);
    _STATE->uiReady = true;
    _STATE->uiReady.notify_all();
}

void tsl::app::setupthr2(tsl::AppState* _appState) {
    //if (_STATE->setup_thread.joinable())
   // _STATE->setup_thread.join();

    tsl::app::guiSetup2(_appState);

    _STATE->initdone = true;
}


void tsl::app::loop2(tsl::AppState* _appState, int w, int h) {
    const auto s = tsl::time::nanosecondsSinceEpoch();
    _appState->graphics.BeginFrame();
    auto drawCanvas = _STATE->graphics.getRootCanvas();
    if (drawCanvas == nullptr) {
        return;
    }
    auto& q = _STATE->queue_draw2;
    _appState->graphics.BeginFrame();


    {
        std::lock_guard lk(q);

        if (q._first) {
#if defined USE_IMGUI
            tsl::imgui::BeginFrame(_STATE->windowWidth, _STATE->windowHeight, _STATE->actual_framerate.load());
#endif
            {
                auto node = q._first;

                while (node) {
                    auto* next = node->next;
                    node->data->render(drawCanvas);
                    if (!node->data->perm) {
                        q.del(node);
                    }
                    node = next;
                }
            }
            drawCanvas->getSurface()->draw(_STATE->graphics.getBackEndSurface()->getCanvas(), 0.0, 0.0, nullptr);


#if defined USE_IMGUI
            _STATE->graphics.flush();
            tsl::imgui::EndFrame();
#endif
        }
    }
    {
        std::lock_guard lk(q);
        if (!_STATE->graphics.windows.empty()) {
            _STATE->graphics.swapWindows();
        }
        _STATE->graphics.flush();
        _STATE->graphics.releaseDeadWindows();
    }

    int64_t delta = tsl::time::nanosecondsSinceEpoch() - s;
    if (delta > 0) {
        auto framerate = static_cast<double>(tsl::time::nanosPerSecond / delta);
        if (framerate < 60.)
            LOGE("Frames dropped. %g", framerate);

    }

}
#endif

