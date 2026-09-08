#include "api_export.h"
#include "app.h"
#include <Input.h>
#include <View.h>
#include <skia.h>
#include <window.h>
#include <memory>
#include <fstream>
#include <logger.h>

#if defined IGRAPHICS_GL3
#include <glad/glad.h>
#elif defined IGRAPHICS_GL2
#include <glad/glad.h>
#endif

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#include <AppKit/AppKit.h>
#endif

// Static state
static std::unique_ptr<tsl::AppState> _appState;

// Platform-specific OpenGL context storage
#ifdef _WIN32
static HDC m_hdc = nullptr;
static HGLRC m_hglrc = nullptr;
static HDC m_savedHdc = nullptr;
static HGLRC m_savedHglrc = nullptr;
#elif defined(__APPLE__)
static CGLContextObj m_cglContext = nullptr;
static CGLContextObj m_savedCglContext = nullptr;
#endif

void ActivateGLContext()
{
#if defined(IGRAPHICS_GL3) || defined(IGRAPHICS_GL2)
#ifdef _WIN32
    m_savedHdc = wglGetCurrentDC();
    m_savedHglrc = wglGetCurrentContext();
    if (m_hdc && m_hglrc) {
        wglMakeCurrent(m_hdc, m_hglrc);
    }
#elif defined(__APPLE__)
    m_savedCglContext = CGLGetCurrentContext();
    if (m_cglContext) {
        CGLSetCurrentContext(m_cglContext);
    }
#endif
#endif
}

void DeactivateGLContext()
{
#if defined(IGRAPHICS_GL3) || defined(IGRAPHICS_GL2)
#ifdef _WIN32
    if (m_savedHglrc) {
        wglMakeCurrent(m_savedHdc, m_savedHglrc);
    }
#elif defined(__APPLE__)
    if (m_savedCglContext) {
        CGLSetCurrentContext(m_savedCglContext);
    }
#endif
#endif
}

namespace tsl
{
    AppState* getInstance() {
        return _appState.get();
    }
}
static int test = 0;
// C API - flat names, no namespaces
extern "C" {

    TSL_API_EXPORT  void TSL_API_CALL  TSL_Inc(int i) { test += i; }
    TSL_API_EXPORT  int TSL_API_CALL  TSL_Get() { return test; }
    TSL_API_EXPORT void TSL_API_CALL TSL_ProcessAudio(double** inputs, double** outputs, int numSamples, int mask) {
        PlayerBase::synthFunc(_appState.get(), inputs, outputs, numSamples, mask);
    }

    TSL_API_EXPORT void TSL_API_CALL TSL_Setup() {
        _appState = std::unique_ptr<tsl::AppState>(tsl::app::setup());
        tsl::app::setupthr(_appState.get());
    }

    TSL_API_EXPORT void TSL_API_CALL TSL_Cleanup() {
        _appState.reset();
    }

    TSL_API_EXPORT void TSL_API_CALL TSL_OnViewInitialized() {
#if defined(IGRAPHICS_GL3) || defined(IGRAPHICS_GL2)
#ifdef _WIN32
        // Get and save the current context from the host
        m_hglrc = wglGetCurrentContext();
        m_hdc = wglGetCurrentDC();
        LOGE("TSL_OnViewInitialized: Saved DC=0x%p, HGLRC=0x%p", m_hdc, m_hglrc);

        if (!m_hglrc || !m_hdc) {
            LOGE("ERROR: Could not get OpenGL context!");
            return;
        }

        // Make it current for loading GL functions
        if (!wglMakeCurrent(m_hdc, m_hglrc)) {
            DWORD error = GetLastError();
            LOGE("ERROR: wglMakeCurrent failed! Error: %d", error);
            return;
        }

        // Load OpenGL functions
        if (!gladLoadGL()) {
            LOGE("ERROR: gladLoadGL failed!");
            return;
        }
        LOGE("OpenGL initialized successfully");

#elif defined(__APPLE__)
        // Get and save the current context from the host
        m_cglContext = CGLGetCurrentContext();
        LOGE("TSL_OnViewInitialized: Saved CGLContext=0x%p", m_cglContext);

        if (!m_cglContext) {
            LOGE("ERROR: Could not get OpenGL context!");
            return;
        }

        // Make it current for loading GL functions
        CGLError err = CGLSetCurrentContext(m_cglContext);
        if (err != kCGLNoError) {
            LOGE("ERROR: CGLSetCurrentContext failed! Error: %d", err);
            return;
        }

        // Load OpenGL functions
        if (!gladLoadGL()) {
            LOGE("ERROR: gladLoadGL failed!");
            return;
        }
        LOGE("OpenGL initialized successfully");
#endif
#endif

        // Initialize graphics
        _appState->graphics.init();
    }

    TSL_API_EXPORT void TSL_API_CALL TSL_OnViewDestroyed() {
        _appState->graphics.onViewDestroyed();

        // Clear saved contexts
#ifdef _WIN32
        m_hdc = nullptr;
        m_hglrc = nullptr;
#elif defined(__APPLE__)
        m_cglContext = nullptr;
#endif
    }

    TSL_API_EXPORT void TSL_API_CALL TSL_DrawResize(int w, int h) {

#if defined(IGRAPHICS_GL3) || defined(IGRAPHICS_GL2)
        ActivateGLContext();
#endif

        tsl::app::resize(_appState.get(), w, h);
    }

    TSL_API_EXPORT void TSL_API_CALL TSL_BeginFrame(int w, int h) {

        tsl::app::loop(_appState.get(), w, h);
    }

    TSL_API_EXPORT void TSL_API_CALL TSL_EndFrame() {
        _appState->graphics.EndFrame();

#if defined(IGRAPHICS_GL3) || defined(IGRAPHICS_GL2)
        DeactivateGLContext();
#endif
    }

    TSL_API_EXPORT bool TSL_API_CALL TSL_CallbackInput(tsl::graphics::InputEvent& ev) {
        return tsl::graphics::callback_input(_appState.get(), ev);
    }

    TSL_API_EXPORT  Param& TSL_API_CALL  TSL_GetParam(int num) {
        return _appState->parameters[num];
    };
    
    TSL_API_EXPORT  double TSL_API_CALL  TSL_GetSampleRate() {
        return _appState->sr;
    };

#ifdef PLUGIN_MODE
    TSL_API_EXPORT  void TSL_API_CALL  TSL_SetInformHostOfParamChange(std::function<void(int tindex, int idx, double normalizedValue)>func){
        _appState.get()->InformHostOfParamChange = func;
    }
    TSL_API_EXPORT  void TSL_API_CALL  TSL_SetBeginEndInformHostOfParamChange(std::function<void(int tindex, int idx, bool end)> func) {
        _appState.get()->BeginEndInformHostOfParamChange = func;
    }

#endif



} // extern "C"