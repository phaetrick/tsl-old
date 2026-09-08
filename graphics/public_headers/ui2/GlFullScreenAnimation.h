#pragma once
#ifndef __ANDROID__
#include <glad/glad.h>
#else
#include <GLES3/gl3.h>
#endif

#if defined OS_MAC || defined OS_IOS
#include "include/utils/mac/SkCGUtils.h"
#if defined IGRAPHICS_GL2
#error SKIA doesn't work correctly with IGRAPHICS_GL2
#elif defined IGRAPHICS_GL3
#include <OpenGL/gl3.h>
#elif defined IGRAPHICS_METAL
// even though this is a .cpp we are in an objc(pp) compilation unit
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#elif !defined IGRAPHICS_CPU
#error Define either IGRAPHICS_GL2, IGRAPHICS_GL3, IGRAPHICS_METAL, or IGRAPHICS_CPU for IGRAPHICS_SKIA with OS_MAC
#endif

#include "include/ports/SkFontMgr_mac_ct.h"
#endif
#include <include/gpu/ganesh/SkSurfaceGanesh.h>
#include <include/gpu/ganesh/GrBackendSurface.h>
#include <include/gpu/ganesh/gl/GrGLInterface.h>
#include <include/gpu/ganesh/gl/GrGLDirectContext.h>
#include <include/gpu/ganesh/gl/GrGLBackendSurface.h>
#include "include/gpu/ganesh/gl/GrGLTypes.h"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include <array>
#include <chrono>
#include <cmath>

class CubeScreenAnimator {
public:
    CubeScreenAnimator() = default;

    // Call once to initialize
    void init(int w, int h, GrDirectContext* ctx);

    // Call every frame to render
    void render(int w, int h);

    // Fillers: override or customize these to draw your actual app screens
    void drawScreen(SkCanvas* canvas, int index);
    void drawAnimatedScreen(SkCanvas* canvas, int index);
private:
    int getFrontFace() const {
        float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - startTime).count();
        return static_cast<int>(fmodf(elapsed, 20.f) / 5.f) % 4;
    }
    int width, height;
    GrDirectContext* grContext;

    std::array<sk_sp<SkSurface>, 4> surfaces;
    std::array<GLuint, 4> textures;

    GLuint shaderProgram = 0;
    GLuint vao = 0;
    GLuint vbo = 0;

    std::chrono::steady_clock::time_point startTime;

    void setupGL();
    void updateSurfaces();
    void createSurfaceForTexture(int index, int texId);
    GLuint createShaderProgram();
    float getRotationAngle();
};
