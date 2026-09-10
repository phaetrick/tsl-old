#include <cmath>
#include "view.h"
#include "logger.h"
#include "skia.h"
#include "colours.h"
#if defined USE_IMGUI
#include  "imguitsl.h"
#endif
#include "app.h"
#include <SkColorSpace.h>

#ifndef __ANDROID__
#else
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#endif

#if defined OS_MAC || defined OS_IOS
#include <os/log.h>
#include "include/utils/mac/SkCGUtils.h"
#if defined IGRAPHICS_GL2
#error SKIA doesn't work correctly with IGRAPHICS_GL2
#elif defined IGRAPHICS_GL3
#include <OpenGL/gl3.h>
#elif defined IGRAPHICS_METAL
// Metal-specific includes and Graphics::init/BeginFrame/EndFrame live in skia.mm
#elif !defined IGRAPHICS_CPU
#error Define either IGRAPHICS_GL2, IGRAPHICS_GL3, IGRAPHICS_METAL, or IGRAPHICS_CPU for IGRAPHICS_SKIA with OS_MAC
#endif

#include "include/ports/SkFontMgr_mac_ct.h"
#endif

#include <include/gpu/ganesh/SkSurfaceGanesh.h>
#include <include/gpu/ganesh/GrBackendSurface.h>

#ifndef IGRAPHICS_METAL
#include <include/gpu/ganesh/gl/GrGLInterface.h>
#include <include/gpu/ganesh/gl/GrGLDirectContext.h>
#include <include/gpu/ganesh/gl/GrGLBackendSurface.h>
#include "include/gpu/ganesh/gl/GrGLTypes.h"

#if defined OS_MAC
#include "include/gpu/ganesh/gl/mac/GrGLMakeMacInterface.h"
#elif defined OS_WIN
#include <glad/glad.h>
#include <include/gpu/ganesh/gl/win/GrGLMakeWinInterface.h>
#endif
#endif // !IGRAPHICS_METAL

;

void tsl::graphics::window::resize(float x, float y, int w, int h) {
    auto ctx = _appState->graphics.getContext();
    if (ctx) {
        xpos = x;
        ypos = y;
        SkImageInfo info2 = SkImageInfo::MakeN32(w, h,
                                                 SkAlphaType::kPremul_SkAlphaType
#if defined __ANDROID__
                                                 ,SkColorSpace::MakeSRGB()
#endif
        );
        surface = SkSurfaces::RenderTarget(
            ctx.get(),
            skgpu::Budgeted::kNo, info2);
        // Null on a zero dimension or an allocation failure. width()/heigth()
        // report 0 for a null surface, so the next getCanvas() retries the
        // creation; dereferencing here crashed before callers could react.
        if (surface)
            surface->getCanvas()->clear(tsl::sk_colours::bg);
    }
}

sk_sp<SkSurface> tsl::graphics::window::getSurface() const {
    return surface;
};


std::shared_ptr<tsl::graphics::window> tsl::graphics::Graphics::createWindow(
        float x, float y, int width, int height, bool isSystem) {
    auto w = std::make_shared<window>(_appState, count++, x, y, width, height, isSystem);
    // Only ever called from getCanvas(), which every caller reaches from inside
    // its own render(), so this is the view that owns the new window.
    w->owner = _appState->renderingView;
    windows.push_back(w);
    return w;
}

std::vector<tsl::graphics::View *> tsl::graphics::Graphics::windowOwners() const {
    // queue_draw is what guards `windows` everywhere else, and this caller (the
    // Android re-attach path in window.cpp) sits outside the render walk, so it
    // does not already hold it. Recursive, so the v->redraw() the caller runs on
    // each returned owner can take it again.
    std::lock_guard lk(_appState->queue_draw);
    std::vector<View *> out;
    out.reserve(windows.size());
    for (const auto &w: windows) {
        // System windows only -- those are the popups, the ones swapWindows()
        // composites and whose owner may not be perm.
        //
        // An isSystem == false window is a private offscreen cache that a perm
        // view blits itself (Waveform's static peak render, the EQ5 curve).
        // Its owner re-renders every frame and already compares the surface
        // size against its own, so it heals on the next frame without help --
        // and Waveform::init() queues a full peak recomputation on the worker
        // queue, so calling it here would be an expensive no-op.
        if (w->owner && w->isSystemWindow) out.push_back(w->owner);
    }
    return out;
}

void tsl::graphics::Graphics::dropFramebufferSurfaces(bool contextCurrent) {
    // Leaves windowWidth/windowHeight alone so the re-attach path can still
    // tell a plain resume from a real size change.
    rootSurface = nullptr;
    backEndSurface = nullptr;

    // Drop the isSystem == false windows -- the private offscreen caches
    // (Waveform's peak render, the EQ5 curve). Their owners are perm and
    // rebuild them from scratch on the next frame, so keeping them buys
    // nothing, and it is a bet that the GL context survived backgrounding
    // intact. Popups are kept because nothing would repaint them.
    {
        // Same reasoning as windowOwners(): queue_draw guards `windows`, and
        // this runs outside the walk. Lock order is queue_draw -> the graveyard
        // leaf, matching deleteWindow(); the leaf never calls back into view
        // code, so the pair cannot form a cycle.
        std::lock_guard lk(_appState->queue_draw);
        std::lock_guard dlk(deadWindowsMutex);
        for (auto it = windows.begin(); it != windows.end();) {
            if (!(*it)->isSystemWindow) {
                deadWindows.push_back(std::move(*it));
                it = windows.erase(it);
            }
            else ++it;
        }
    }

    // Releasing a GPU surface needs the context current. When it is not, the
    // graveyard just waits for a frame that has one.
    if (contextCurrent)
        releaseDeadWindows();
}

void tsl::graphics::Graphics::releaseDeadWindows() {
    std::vector<std::shared_ptr<window> > dead;
    {
        std::lock_guard lk(deadWindowsMutex);
        if (deadWindows.empty())
            return;
        dead.swap(deadWindows);
    }
    // Destroyed outside the lock on purpose: ~SkSurface can be slow, and a
    // popup closing on another thread must not block behind GPU teardown.
    dead.clear();
}


std::unique_ptr<tsl::graphics::Graphics> tsl::graphics::create() {
    return std::make_unique<Graphics>();
};

SkCanvas *tsl::graphics::Graphics::getCanvas(int &index, float x, float y, int w, int h, bool isSystem) {
    for (auto &win: windows) {
        if (win->index == index) {
            if (w != win->width() || h != win->heigth()) {
                win->resize(x, y, w, h);
            } else {
                win->xpos = x;
                win->ypos = y;
            }
            // window::resize() leaves surface null when there was no context,
            // and SkSurfaces::RenderTarget also returns null on a zero
            // dimension or an allocation failure. Every caller already tests
            // the result ("Failed to create waveform Skia surface"), so report
            // it instead of dereferencing null.
            return win->surface ? win->surface->getCanvas() : nullptr;
        }
    }
    auto win = createWindow(x, y, w, h, isSystem);
    index = win->index;
    return win->surface ? win->surface->getCanvas() : nullptr;
}

void tsl::graphics::Graphics::deleteWindow(int index) {
    for (auto it = windows.begin(); it != windows.end(); it++) {
        if (it->get()->index == index) {
            // Unlink now -- callers delete a window and re-render in the same
            // frame expecting it gone -- but hand the surface to the render
            // thread to destroy. See deadWindows.
            //
            // The `windows` mutation is already covered by queue_draw (this
            // runs from a render() or a delRecursiveDraw(), both under it);
            // the graveyard push needs its own lock because the render thread
            // drains it from paths that do not hold queue_draw.
            {
                std::lock_guard lk(deadWindowsMutex);
                deadWindows.push_back(std::move(*it));
            }
            windows.erase(it);
            return;
        }
    }
};


sk_sp<SkSurface> tsl::graphics::Graphics::getBackEndSurface() { return backEndSurface; }

void tsl::graphics::Graphics::swapWindows(
) {
    if (backEndSurface == nullptr)
        return;
    for (auto &w: windows) {
        // window::resize() leaves surface null when there was no context at the time
        if (w->isSystemWindow && w->getSurface() != nullptr)
            w->getSurface()->draw(backEndSurface->getCanvas(), std::round(w->xpos + xOffset), std::round(w->ypos + yOffset));
    }
}


std::shared_ptr<tsl::graphics::window> tsl::graphics::Graphics::getWindow(int indx) {
    for (auto &w: windows)if (w->index == indx)return w;
    return {nullptr};
}

SkCanvas *tsl::graphics::Graphics::getRootCanvas() {
    return rootSurface != nullptr ? rootSurface->getCanvas() : nullptr;
}

sk_sp<SkSurface> tsl::graphics::Graphics::getSurface(int index) {
    for (const auto &win: windows) {
        if (win->index == index) {
            return win->surface;
        }
    }
    return nullptr;
};

void tsl::graphics::Graphics::getWindowDimensions(int index, int &w, int &h) const {
    for (const auto &win: windows) {
        if (win->index == index) {
            w = win->width();
            h = win->heigth();
            return;
        }
    }
    w = h = 0;
};


#ifndef IGRAPHICS_METAL
void tsl::graphics::Graphics::init() {
#if defined OS_MAC
    auto interface = GrGLInterfaces::MakeMac();
#elif defined OS_WIN
    auto interface = GrGLInterfaces::MakeWin();
#elif defined __ANDROID__
    // EGL context must be current on this thread before calling this
    EGLContext ctx = eglGetCurrentContext();
    if (ctx == EGL_NO_CONTEXT) {
        LOGE("Skia init failed: no EGL context current on this thread");
        return;
    }
    auto interface = GrGLMakeNativeInterface();
#endif

    if (interface == nullptr) {
        LOGE("Skia Null Interface");
        return;
    }

    grContext = GrDirectContexts::MakeGL(interface);
    if (grContext == nullptr) {
        LOGE("Skia Null Context");
        return;
    }
}

void tsl::graphics::Graphics::BeginFrame() {
#if defined IGRAPHICS_GL3 || defined IGRAPHICS_GL2
    if (grContext.get()) {
        int fbo = 0, samples = 0, stencilBits = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        glGetIntegerv(GL_SAMPLES, &samples);
#ifdef IGRAPHICS_GL3
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
                                              &stencilBits);
#else
        glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
#endif
#if defined __ANDROID__
        if (fbo == 0) {
            stencilBits = 0;
            samples = 0;
        }
#endif
        GrGLFramebufferInfo fbInfo;
        fbInfo.fFBOID = fbo;
        fbInfo.fFormat = GL_RGBA8;

        auto backendRT = GrBackendRenderTargets::MakeGL(windowWidth, windowHeight, samples, stencilBits, fbInfo);

        backEndSurface = SkSurfaces::WrapBackendRenderTarget(grContext.get(), backendRT, kBottomLeft_GrSurfaceOrigin,
                                                             kRGBA_8888_SkColorType
#if defined __ANDROID__
                ,SkColorSpace::MakeSRGB()
#else
                                                             , nullptr
#endif
                                                             , nullptr);
        // Was an assert, which NDEBUG (forced on for every config) compiled out. The draw path
        // now treats a null surface as a dropped frame rather than crashing, so this only has
        // to be visible, not fatal.
        if (!backEndSurface)
            LOGE("Skia WrapBackendRenderTarget returned null - skipping frame");
    }
#endif
}

void tsl::graphics::Graphics::EndFrame() {
    flush();
    backEndSurface = nullptr;
}
#endif // !IGRAPHICS_METAL


void tsl::graphics::Graphics::flush() {
    // BeginFrame() only assigns backEndSurface when grContext is alive, and
    // WrapBackendRenderTarget can return null anyway. Both leave it null here, and
    // EndFrame() calls flush() unconditionally - that was an access violation.
    if (backEndSurface == nullptr)
        return;
    if (auto dContext = GrAsDirectContext(backEndSurface->getCanvas()->recordingContext())) {
        dContext->flushAndSubmit();
    }
}


void tsl::graphics::Graphics::onViewDestroyed() {
    // Final teardown: nothing will drain the graveyard afterwards, so release
    // it here while the context is still whole. Workers (decoder finish,
    // recorder stop) can still close a popup while the desktop editor tears
    // down, so the swap takes the same locks the live paths use; queue_draw is
    // recursive, and the order queue_draw -> deadWindowsMutex matches
    // dropFramebufferSurfaces().
    std::vector<std::shared_ptr<window> > dead;
    {
        std::lock_guard lk(_appState->queue_draw);
        std::lock_guard dlk(deadWindowsMutex);
        dead.swap(deadWindows);
        for (auto &w: windows) dead.push_back(std::move(w));
        windows.clear();
    }
    // Destroyed outside the locks -- ~SkSurface can be slow.
    dead.clear();
    rootSurface = nullptr;
    backEndSurface = nullptr;
    if (grContext) {
        grContext->abandonContext();
    }
    grContext = nullptr;
}

void tsl::graphics::Graphics::onResized(int w, int h) {
#if (defined PLUGIN_MODE || defined STANDALONE_MODE) && !defined(OS_IOS)
    _appState->windowWidth = windowWidth = w;
    _appState->windowHeight = windowHeight = h;
    xOffset = yOffset = 0;
    SkImageInfo info2 = SkImageInfo::MakeN32(w, h, kPremul_SkAlphaType
#else
#if defined(OS_IOS)
   // updateDrawableSizeFromLayer(w, h);
#if defined(STANDALONE_MODE)
  _appState->safeInsets = getSafeInsets();
#endif
#endif
    
    const auto &insets = _appState->safeInsets;
    int usableW = w - (insets.left + insets.right);
    int usableH = h - (insets.top + insets.bottom);

    int targetWidth = 0;
    int targetHeight = 0;

    float currentAspect = (float) usableW / (float) usableH;
    const float targetAspect = 9.0f / 16.0f;

    if (appDimension == Portrait) {
        if (currentAspect > targetAspect) {
                                                     targetWidth = usableH * targetAspect;
                                                     targetHeight = usableH;
                                                     xOffset = insets.left + (usableW - targetWidth) / 2.0f;
                                                     yOffset = insets.top;

                                                 } else {
                                                     targetWidth = usableW;
                                                     targetHeight = usableH;
                                                     xOffset = insets.left;
                                                     yOffset = insets.top;

                                                 }
                                             }
    else if (appDimension == LandScape) {
        float landTarget = 16.0f / 9.0f;
        if (currentAspect < landTarget) {
                                                     targetWidth = usableW;
                                                     targetHeight = usableW / landTarget;
                                                     xOffset = insets.left;
                                                     yOffset = insets.top + (usableH - targetHeight) / 2.0f;

                                                 } else {
                                                     targetWidth = usableW;
                                                     targetHeight = usableH;
                                                     xOffset = insets.left;
                                                     yOffset = insets.top;

                                                 }
                                             }
                                             windowWidth = w;
                                             _appState->windowWidth = targetWidth;
                                             windowHeight = h;
                                             _appState->windowHeight = targetHeight;

                                             SkImageInfo info2 = SkImageInfo::MakeN32(
                                                             targetWidth, targetHeight, kPremul_SkAlphaType
#endif
#if defined __ANDROID__
                                             ,SkColorSpace::MakeSRGB()
#endif
    );

    rootSurface = SkSurfaces::RenderTarget(
        grContext.get(),
        skgpu::Budgeted::kYes, info2);
    if (!rootSurface) {
        LOGE("Failed to create root Skia surface");
        return;
    }
    rootSurface->getCanvas()->clear(tsl::sk_colours::bg);
}
