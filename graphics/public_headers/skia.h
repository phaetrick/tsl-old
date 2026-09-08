#pragma once


#define SK_GL
#include <SkFont.h>
#include <SkSurface.h>
#include <SkPath.h>
#include <SkCanvas.h>
#include <GrDirectContext.h>

#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#define ROOTWIN auto rwin = _STATE->graphics.getRootWin();
#define CANVAS  auto c = _STATE->graphics.getRootCanvas();

#if defined(_WIN32) || defined(_WIN64)
using GRCONTEXT = sk_sp<GrDirectContext>;

//using GRCONTEXT = GrDirectContext*;
#else
using GRCONTEXT = sk_sp<GrDirectContext>;
#endif
struct SafeInsets;

namespace tsl::graphics {
    class View;

    enum AppDimension {
        Not_Specified,
        LandScape,
        Portrait
    };

    struct window {
    public:
        window(tsl::AppState *appState, int indx, float x, float y, int w, int h,
               bool isSystem = true) : _appState(appState), index(indx), isSystemWindow(isSystem) {
            resize(x, y, w, h);
        }

        void resize(float x, float y, int w, int h);

        void setPos(float x, float y) {
            xpos = x;
            ypos = y;
        }

        int width() { return surface.get() == nullptr ? 0 : surface->width(); }
        int heigth() { return surface.get() == nullptr ? 0 : surface->height(); }


        SkCanvas *getCanvas() const;

        sk_sp<SkSurface> getSurface() const;

        tsl::AppState *_appState{};
        int index{};
        sk_sp<SkSurface> surface;
        float xpos{}, ypos{};
        bool isSystemWindow{};

        // The view whose render() created this window, stamped by
        // createWindow() from AppState::renderingView. Valid for exactly as
        // long as the window is in the deque: closing a popup deletes its
        // window, so a dead view cannot leave one behind. Same raw-pointer
        // contract queue_draw already runs on. May be null for windows made
        // outside a render walk.
        View *owner{};
    };

    class Graphics {
    public:
        void onViewDestroyed();

        void BeginFrame();

        void EndFrame();

        void onResized(int w, int h);

        void flush();

        void swapWindows();

        void init();

        bool isInitialized() const { return grContext != nullptr; }

        sk_sp<SkSurface> getBackEndSurface();

        GRCONTEXT getContext() {
            return grContext;
        }


        std::shared_ptr<window> getWindow(int indx);

        SkCanvas *getRootCanvas();

        sk_sp<SkSurface> getSurface(int index);

        void getWindowDimensions(int index, int &w, int &h) const;

        SkCanvas *getCanvas(int &index, float x, float y, int w, int h, bool isSystem = true);


        void deleteWindow(int index);


        // Teardown. Unlike deleteWindow() this releases the surfaces here and
        // now: the render loop has already stopped, so nothing would ever come
        // along to drain the graveyard.
        void reset() {
            std::vector<std::shared_ptr<window> > dead;
            dead.swap(deadWindows);
            for (auto &w: windows) dead.push_back(std::move(w));
            windows.clear();
            dead.clear();
            rootSurface = nullptr;
            backEndSurface = nullptr;
            grContext = nullptr;
        }

        void clear();

        // What a background/foreground round trip actually needs.
        //
        // Only backEndSurface is tied to the platform surface -- it wraps the
        // EGL framebuffer, so it dies with it. rootSurface and every popup's
        // window::surface are plain SkSurfaces::RenderTarget owned by the
        // GrDirectContext, and APP_CMD_TERM_WINDOW destroys gEglSurface while
        // leaving gEglContext alive. They stay valid across the round trip.
        //
        // clear() drops the popup deque as well, which is why open popups
        // vanished on resume: a FloatingView is NOT perm, so it was reaped from
        // queue_draw long ago and stays on screen purely because swapWindows()
        // composites its surface every frame. Destroy the surface and there is
        // nothing left to re-render it.
        //
        // Only popups are kept. The isSystem == false caches go, because
        // keeping them bets that the GL context came back intact and their perm
        // owners rebuild them next frame anyway.
        //
        // contextCurrent == false means the caller has already un-currented the
        // EGL context, so GPU objects must not be destroyed yet; the graveyard
        // waits for a frame that has one.
        void dropFramebufferSurfaces(bool contextCurrent);

        // The render loop's "anything to composite?" guard. No lock of its own:
        // every mutation of `windows` happens under queue_draw, and each call
        // site here already holds it. If a caller is ever added that does not,
        // this read becomes a data race against deleteWindow()'s erase() -- a
        // real one, not a benign stale read -- so give `windows` a leaf lock
        // then rather than reading it bare.
        [[nodiscard]] bool hasWindows() const {
            return !windows.empty();
        }


        int windowWidth{}, windowHeight{};
        float xOffset{}, yOffset{};
        std::deque<std::shared_ptr<window> > windows{};

        // Windows already unlinked from `windows` whose SkSurface has not been
        // released yet. deleteWindow() runs on whatever thread closed the popup
        // -- a decoder worker, the recorder, the input thread -- and dropping
        // the last reference there would destroy GPU resources with no GL
        // context current. So the deque mutation stays synchronous (every call
        // site depends on that) and only the *release* is handed to the render
        // thread, which drains this once per frame via releaseDeadWindows().
        std::vector<std::shared_ptr<window> > deadWindows{};

        /* A LEAF lock, and the only structure here that needs one. `windows`
           itself is covered incidentally by queue_draw -- every mutation of it
           happens under that lock -- but the graveyard is not: deleteWindow()
           pushes under queue_draw, while two of releaseDeadWindows()' four call
           sites (the Android draw loop's post-swap and paused-frame paths,
           window.cpp) run outside it. Nothing under this lock calls back into
           view code, so it cannot participate in a cycle. */
        std::mutex deadWindowsMutex;

        // Safe to call every frame; does nothing when empty. Must run on the
        // render thread with the GL context current -- that is the whole point.
        void releaseDeadWindows();

        // Snapshot of the views owning the open *system* windows (popups). Used
        // after a rotation to re-init and repaint the ones that would otherwise
        // sit at stale geometry -- most are not perm, so nothing else would
        // redraw them. Deliberately skips isSystem == false windows: those are
        // private offscreen caches belonging to perm views that heal themselves.
        [[nodiscard]] std::vector<View *> windowOwners() const;

        tsl::AppState *_appState;

        float scaleFactor = 1.0f;
        float dpi = 160.0f; // or device DPI

        void updateUIScale(int screenWidthPx, int screenHeightPx, float _dpi) {
            windowWidth = screenWidthPx;
            windowHeight = screenHeightPx;
            dpi = _dpi;

            // Start from DPI-based scale
            float sf = dpi / 160.0f;

            // Clamp to min physical width
            constexpr float baseWidthDp = 360.0f;
            float widthDp = screenWidthPx / sf;
            if (widthDp < baseWidthDp) {
                sf = screenWidthPx / baseWidthDp;
            }

            // Optional: max scale to avoid gigantic UIs on 4K
            constexpr float maxScale = 3.0f;
            sf = std::min(sf, maxScale);

            scaleFactor = sf;
        }

        AppDimension appDimension = Not_Specified;
#if defined OS_IOS || defined OS_MAC
        void setMTLLayer(void *layer) { mMTLLayer = layer; }
        bool hasMTLLayer() const { return mMTLLayer != nullptr; }
        bool mInsetsQueried = false;
        bool hasQueriedInsets() const { return mInsetsQueried; }
#endif
#if defined OS_IOS
        SafeInsets getSafeInsets();
#endif

    private:
        // Out of line: it reads AppState::renderingView, and AppState is still
        // incomplete here (app.h includes this header, not the other way round).
        std::shared_ptr<window> createWindow(float x, float y, int width, int height, bool isSystem = true);

        sk_sp<SkSurface> rootSurface{};
        int count{};
        sk_sp<SkSurface> backEndSurface{};
        GRCONTEXT grContext{};
#if defined OS_IOS || defined OS_MAC
        void *mMTLDevice = nullptr;
        void *mMTLLayer = nullptr;
        void *mMTLCommandQueue = nullptr;
        void *mMTLDrawable = nullptr;
#endif
    };

    extern std::unique_ptr<Graphics> create();
}
